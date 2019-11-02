/*
 * Babeltrace - Trace IR field copy
 *
 * Copyright (c) 2015-2019 EfficiOS Inc. and Linux Foundation
 * Copyright (c) 2018 Philippe Proulx <pproulx@efficios.com>
 * Copyright (c) 2019 Francis Deslauriers <francis.deslauriers@efficios.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#define BT_COMP_LOG_SELF_COMP (md_maps->self_comp)
#define BT_LOG_OUTPUT_LEVEL (md_maps->log_level)
#define BT_LOG_TAG "PLUGIN/FLT.LTTNG-UTILS.DEBUG-INFO/TRACE-IR-META-FC-COPY"
#include "logging/comp-logging.h"

#include "common/assert.h"
#include "common/common.h"
#include "compat/compiler.h"
#include <babeltrace2/babeltrace.h>

#include "trace-ir-metadata-copy.h"
#include "trace-ir-metadata-field-class-copy.h"

/*
 * This fonction walks througth the nested structures field class to resolve a
 * field path object. A field path is made of indexes inside possibly nested
 * structures ultimately leading to a field class.
 */
static
const bt_field_class *walk_field_path(struct trace_ir_metadata_maps *md_maps,
		const bt_field_path *fp, const bt_field_class *fc)
{
	uint64_t i, fp_item_count;
	const bt_field_class *curr_fc;

	BT_ASSERT(bt_field_class_get_type(fc) == BT_FIELD_CLASS_TYPE_STRUCTURE);
	BT_COMP_LOGD("Walking field path on field class: fp-addr=%p, fc-addr=%p",
		fp, fc);

	fp_item_count = bt_field_path_get_item_count(fp);
	curr_fc = fc;
	for (i = 0; i < fp_item_count; i++) {
		bt_field_class_type fc_type = bt_field_class_get_type(curr_fc);
		const bt_field_path_item *fp_item =
			bt_field_path_borrow_item_by_index_const(fp, i);

		if (fc_type == BT_FIELD_CLASS_TYPE_STRUCTURE) {
			const bt_field_class_structure_member *member;

			BT_ASSERT(bt_field_path_item_get_type(fp_item) ==
				BT_FIELD_PATH_ITEM_TYPE_INDEX);
			member = bt_field_class_structure_borrow_member_by_index_const(
				curr_fc,
				bt_field_path_item_index_get_index(fp_item));
			curr_fc = bt_field_class_structure_member_borrow_field_class_const(
				member);
		} else if (bt_field_class_type_is(fc_type, BT_FIELD_CLASS_TYPE_OPTION)) {
			BT_ASSERT(bt_field_path_item_get_type(fp_item) ==
				BT_FIELD_PATH_ITEM_TYPE_CURRENT_OPTION_CONTENT);
			curr_fc = bt_field_class_option_borrow_field_class_const(
				curr_fc);

		} else if (bt_field_class_type_is(fc_type, BT_FIELD_CLASS_TYPE_VARIANT)) {
			const bt_field_class_variant_option *option;

			BT_ASSERT(bt_field_path_item_get_type(fp_item) ==
				BT_FIELD_PATH_ITEM_TYPE_INDEX);
			option = bt_field_class_variant_borrow_option_by_index_const(
				curr_fc,
				bt_field_path_item_index_get_index(fp_item));
			curr_fc = bt_field_class_variant_option_borrow_field_class_const(
				option);
			break;
		} else if (bt_field_class_type_is(fc_type, BT_FIELD_CLASS_TYPE_ARRAY)) {
			BT_ASSERT(bt_field_path_item_get_type(fp_item) ==
				BT_FIELD_PATH_ITEM_TYPE_CURRENT_ARRAY_ELEMENT);
			curr_fc = bt_field_class_array_borrow_element_field_class_const(
				curr_fc);
			break;
		} else {
			bt_common_abort();
		}
	}

	return curr_fc;
}

static
const bt_field_class *resolve_field_path_to_field_class(const bt_field_path *fp,
		struct trace_ir_metadata_maps *md_maps)
{
	struct field_class_resolving_context *fc_resolving_ctx;
	const bt_field_class *fc;
	bt_field_path_scope fp_scope;

	BT_COMP_LOGD("Resolving field path: fp-addr=%p", fp);

	fc_resolving_ctx = md_maps->fc_resolving_ctx;
	fp_scope = bt_field_path_get_root_scope(fp);

	switch (fp_scope) {
	case BT_FIELD_PATH_SCOPE_PACKET_CONTEXT:
		fc = walk_field_path(md_maps, fp,
			fc_resolving_ctx->packet_context);
		break;
	case BT_FIELD_PATH_SCOPE_EVENT_COMMON_CONTEXT:
		fc = walk_field_path(md_maps, fp,
			fc_resolving_ctx->event_common_context);
		break;
	case BT_FIELD_PATH_SCOPE_EVENT_SPECIFIC_CONTEXT:
		fc = walk_field_path(md_maps, fp,
			fc_resolving_ctx->event_specific_context);
		break;
	case BT_FIELD_PATH_SCOPE_EVENT_PAYLOAD:
		fc = walk_field_path(md_maps, fp,
			fc_resolving_ctx->event_payload);
		break;
	default:
		bt_common_abort();
	}

	return fc;
}

static inline
void field_class_integer_set_props(const bt_field_class *input_fc,
		bt_field_class *output_fc)
{
	bt_field_class_integer_set_preferred_display_base(output_fc,
		bt_field_class_integer_get_preferred_display_base(input_fc));
	bt_field_class_integer_set_field_value_range(output_fc,
		bt_field_class_integer_get_field_value_range(input_fc));
}

static inline
enum debug_info_trace_ir_mapping_status field_class_bool_copy(
		struct trace_ir_metadata_maps *md_maps,
		const bt_field_class *in_field_class,
		bt_field_class *out_field_class)
{
	BT_COMP_LOGD("Copying content of boolean field class: "
		"in-fc-addr=%p, out-fc-addr=%p", in_field_class, out_field_class);
	/*
	 * There is no content to copy. Keep this function call anyway for
	 * logging purposes.
	 */
	BT_COMP_LOGD("Copied content of boolean field class: "
		"in-fc-addr=%p, out-fc-addr=%p", in_field_class, out_field_class);
	return DEBUG_INFO_TRACE_IR_MAPPING_STATUS_OK;
}

static inline
enum debug_info_trace_ir_mapping_status field_class_bit_array_copy(
		struct trace_ir_metadata_maps *md_maps,
		const bt_field_class *in_field_class,
		bt_field_class *out_field_class)
{
	BT_COMP_LOGD("Copying content of bit array field class: "
		"in-fc-addr=%p, out-fc-addr=%p", in_field_class, out_field_class);
	/*
	 * There is no content to copy. Keep this function call anyway for
	 * logging purposes.
	 */
	BT_COMP_LOGD("Copied content of bit array field class: "
		"in-fc-addr=%p, out-fc-addr=%p", in_field_class, out_field_class);
	return DEBUG_INFO_TRACE_IR_MAPPING_STATUS_OK;
}

static inline
enum debug_info_trace_ir_mapping_status field_class_unsigned_integer_copy(
		struct trace_ir_metadata_maps *md_maps,
		const bt_field_class *in_field_class,
		bt_field_class *out_field_class)
{
	BT_COMP_LOGD("Copying content of unsigned integer field class: "
		"in-fc-addr=%p, out-fc-addr=%p", in_field_class, out_field_class);

	field_class_integer_set_props(in_field_class, out_field_class);

	BT_COMP_LOGD("Copied content of unsigned integer field class: "
		"in-fc-addr=%p, out-fc-addr=%p", in_field_class, out_field_class);
	return DEBUG_INFO_TRACE_IR_MAPPING_STATUS_OK;
}

static inline
enum debug_info_trace_ir_mapping_status field_class_signed_integer_copy(
		struct trace_ir_metadata_maps *md_maps,
		const bt_field_class *in_field_class,
		bt_field_class *out_field_class)
{
	BT_COMP_LOGD("Copying content of signed integer field class: "
		"in-fc-addr=%p, out-fc-addr=%p", in_field_class, out_field_class);

	field_class_integer_set_props(in_field_class, out_field_class);

	BT_COMP_LOGD("Copied content of signed integer field class: "
		"in-fc-addr=%p, out-fc-addr=%p", in_field_class, out_field_class);
	return DEBUG_INFO_TRACE_IR_MAPPING_STATUS_OK;
}

static
enum debug_info_trace_ir_mapping_status field_class_unsigned_enumeration_copy(
		struct trace_ir_metadata_maps *md_maps,
		const bt_field_class *in_field_class,
		bt_field_class *out_field_class)
{
	enum debug_info_trace_ir_mapping_status status;
	uint64_t i, enum_mapping_count;

	BT_COMP_LOGD("Copying content of unsigned enumeration field class: "
		"in-fc-addr=%p, out-fc-addr=%p", in_field_class, out_field_class);

	/* Copy properties of the inner integer. */
	field_class_integer_set_props(in_field_class, out_field_class);

	/* Copy all enumeration entries. */
	enum_mapping_count = bt_field_class_enumeration_get_mapping_count(
		in_field_class);
	for (i = 0; i < enum_mapping_count; i++) {
		const char *label;
		const bt_integer_range_set_unsigned *range_set;
		const bt_field_class_enumeration_unsigned_mapping *u_mapping;
		const bt_field_class_enumeration_mapping *mapping;
		enum bt_field_class_enumeration_add_mapping_status add_mapping_status;

		u_mapping = bt_field_class_enumeration_unsigned_borrow_mapping_by_index_const(
			in_field_class, i);
		mapping = bt_field_class_enumeration_unsigned_mapping_as_mapping_const(
			u_mapping);
		label = bt_field_class_enumeration_mapping_get_label(mapping);
		range_set = bt_field_class_enumeration_unsigned_mapping_borrow_ranges_const(
			u_mapping);
		add_mapping_status = bt_field_class_enumeration_unsigned_add_mapping(
			out_field_class, label, range_set);
		if (add_mapping_status != BT_FIELD_CLASS_ENUMERATION_ADD_MAPPING_STATUS_OK) {
			status = (int) add_mapping_status;
			goto end;
		}
	}

	BT_COMP_LOGD("Copied content of unsigned enumeration field class: "
		"in-fc-addr=%p, out-fc-addr=%p", in_field_class, out_field_class);

	status = DEBUG_INFO_TRACE_IR_MAPPING_STATUS_OK;
end:
	return status;
}

static inline
enum debug_info_trace_ir_mapping_status field_class_signed_enumeration_copy(
		struct trace_ir_metadata_maps *md_maps,
		const bt_field_class *in_field_class,
		bt_field_class *out_field_class)
{
	enum debug_info_trace_ir_mapping_status status;
	uint64_t i, enum_mapping_count;

	BT_COMP_LOGD("Copying content of signed enumeration field class: "
		"in-fc-addr=%p, out-fc-addr=%p", in_field_class, out_field_class);

	/* Copy properties of the inner integer. */
	field_class_integer_set_props(in_field_class, out_field_class);

	/* Copy all enumeration entries. */
	enum_mapping_count =
		bt_field_class_enumeration_get_mapping_count(in_field_class);
	for (i = 0; i < enum_mapping_count; i++) {
		const char *label;
		const bt_integer_range_set_signed *range_set;
		const bt_field_class_enumeration_signed_mapping *s_mapping;
		const bt_field_class_enumeration_mapping *mapping;
		enum bt_field_class_enumeration_add_mapping_status add_mapping_status;

		s_mapping = bt_field_class_enumeration_signed_borrow_mapping_by_index_const(
			in_field_class, i);
		mapping = bt_field_class_enumeration_signed_mapping_as_mapping_const(
			s_mapping);
		label = bt_field_class_enumeration_mapping_get_label(mapping);
		range_set = bt_field_class_enumeration_signed_mapping_borrow_ranges_const(
			s_mapping);
		add_mapping_status = bt_field_class_enumeration_signed_add_mapping(
			out_field_class, label, range_set);
		if (add_mapping_status != BT_FIELD_CLASS_ENUMERATION_ADD_MAPPING_STATUS_OK) {
			status = (int) add_mapping_status;
			goto end;
		}
	}

	BT_COMP_LOGD("Copied content of signed enumeration field class: "
		"in-fc-addr=%p, out-fc-addr=%p", in_field_class, out_field_class);

	status = DEBUG_INFO_TRACE_IR_MAPPING_STATUS_OK;
end:
	return status;
}

static inline
enum debug_info_trace_ir_mapping_status field_class_single_precision_real_copy(
		struct trace_ir_metadata_maps *md_maps,
		const bt_field_class *in_field_class,
		bt_field_class *out_field_class)
{
	BT_COMP_LOGD("Copying content of single-precision real field class: "
		"in-fc-addr=%p, out-fc-addr=%p", in_field_class, out_field_class);

	BT_COMP_LOGD("Copied content single-precision real field class:"
		"in-fc-addr=%p, out-fc-addr=%p", in_field_class, out_field_class);

	return DEBUG_INFO_TRACE_IR_MAPPING_STATUS_OK;
}

static inline
enum debug_info_trace_ir_mapping_status field_class_double_precision_real_copy(
		struct trace_ir_metadata_maps *md_maps,
		const bt_field_class *in_field_class,
		bt_field_class *out_field_class)
{
	BT_COMP_LOGD("Copying content of double-precision real field class: "
		"in-fc-addr=%p, out-fc-addr=%p", in_field_class, out_field_class);

	BT_COMP_LOGD("Copied content double-precision real field class:"
		"in-fc-addr=%p, out-fc-addr=%p", in_field_class, out_field_class);

	return DEBUG_INFO_TRACE_IR_MAPPING_STATUS_OK;
}

static inline
enum debug_info_trace_ir_mapping_status field_class_structure_copy(
		struct trace_ir_metadata_maps *md_maps,
		const bt_field_class *in_field_class,
		bt_field_class *out_field_class)
{
	bt_self_component *self_comp = md_maps->self_comp;
	uint64_t i, struct_member_count;
	enum debug_info_trace_ir_mapping_status status;

	BT_COMP_LOGD("Copying content of structure field class: "
		"in-fc-addr=%p, out-fc-addr=%p", in_field_class, out_field_class);
	/* Get the number of member in that struct. */
	struct_member_count =
		bt_field_class_structure_get_member_count(in_field_class);

	/* Iterate over all the members of the struct. */
	for (i = 0; i < struct_member_count; i++) {
		enum bt_field_class_structure_append_member_status append_member_status;
		const bt_field_class_structure_member *in_member;
		bt_field_class_structure_member *out_member;
		const char *member_name;
		const bt_field_class *in_member_fc;
		bt_field_class *out_member_fc;

		in_member = bt_field_class_structure_borrow_member_by_index_const(
			in_field_class, i);
		in_member_fc = bt_field_class_structure_member_borrow_field_class_const(
			in_member);
		member_name = bt_field_class_structure_member_get_name(in_member);
		BT_COMP_LOGD("Copying structure field class's member: "
			"index=%" PRId64 ", member-fc-addr=%p, member-name=\"%s\"",
			i, in_member_fc, member_name);

		out_member_fc = create_field_class_copy(md_maps, in_member_fc);
		if (!out_member_fc) {
			BT_COMP_LOGE_APPEND_CAUSE(self_comp, "Cannot copy structure field class's member: "
				"index=%" PRId64 ", in-member-fc-addr=%p, "
				"member-name=\"%s\"", i, in_member_fc,
				member_name);
			status = DEBUG_INFO_TRACE_IR_MAPPING_STATUS_MEMORY_ERROR;
			goto end;
		}

		status = copy_field_class_content(md_maps, in_member_fc,
			out_member_fc);
		if (status != DEBUG_INFO_TRACE_IR_MAPPING_STATUS_OK) {
			BT_COMP_LOGE_APPEND_CAUSE(self_comp, "Cannot copy content of structure field class's member: "
				"index=%" PRId64 ", in-member-fc-addr=%p, "
				"member-name=\"%s\"", i, in_member_fc,
				member_name);
			BT_FIELD_CLASS_PUT_REF_AND_RESET(out_member_fc);
			goto end;
		}

		append_member_status = bt_field_class_structure_append_member(out_field_class,
				member_name, out_member_fc);
		if (append_member_status !=
				BT_FIELD_CLASS_STRUCTURE_APPEND_MEMBER_STATUS_OK) {
			BT_COMP_LOGE_APPEND_CAUSE(self_comp, "Cannot append structure field class's field: "
				"index=%" PRId64 ", field-fc-addr=%p, "
				"field-name=\"%s\"", i, in_member_fc,
				member_name);
			BT_FIELD_CLASS_PUT_REF_AND_RESET(out_member_fc);
			status = (int) append_member_status;
			goto end;
		}

		out_member = bt_field_class_structure_borrow_member_by_index(
			out_field_class, i);
		BT_ASSERT(out_member);

		/*
		 * Safe to use the same value object because it's frozen
		 * at this point.
		 */
		bt_field_class_structure_member_set_user_attributes(
			out_member,
			bt_field_class_structure_member_borrow_user_attributes_const(
				in_member));
	}

	BT_COMP_LOGD("Copied structure field class: original-fc-addr=%p, copy-fc-addr=%p",
		in_field_class, out_field_class);

	status = DEBUG_INFO_TRACE_IR_MAPPING_STATUS_OK;
end:
	return status;
}

static inline
enum debug_info_trace_ir_mapping_status field_class_variant_copy(
		struct trace_ir_metadata_maps *md_maps,
		const bt_field_class *in_field_class,
		bt_field_class *out_field_class)
{
	bt_self_component *self_comp = md_maps->self_comp;
	enum debug_info_trace_ir_mapping_status status;
	bt_field_class *out_tag_field_class = NULL;
	uint64_t i, variant_option_count;
	bt_field_class_type fc_type = bt_field_class_get_type(in_field_class);

	BT_COMP_LOGD("Copying content of variant field class: "
		"in-fc-addr=%p, out-fc-addr=%p", in_field_class, out_field_class);
	variant_option_count =
		bt_field_class_variant_get_option_count(in_field_class);
	for (i = 0; i < variant_option_count; i++) {
		const bt_field_class *in_option_fc;
		const char *option_name;
		bt_field_class *out_option_fc;
		const bt_field_class_variant_option *in_option;
		bt_field_class_variant_option *out_option;

		in_option = bt_field_class_variant_borrow_option_by_index_const(
			in_field_class, i);
		in_option_fc = bt_field_class_variant_option_borrow_field_class_const(
			in_option);
		option_name = bt_field_class_variant_option_get_name(in_option);

		out_option_fc = create_field_class_copy_internal(
			md_maps, in_option_fc);
		if (!out_option_fc) {
			BT_COMP_LOGE_APPEND_CAUSE(self_comp, "Cannot copy variant option field class: "
				"in-option-fc=%p, in-option-name=\"%s\"",
				in_option_fc, option_name);
			status = DEBUG_INFO_TRACE_IR_MAPPING_STATUS_MEMORY_ERROR;
			goto end;
		}

		status = copy_field_class_content_internal(md_maps, in_option_fc,
			out_option_fc);
		if (status != DEBUG_INFO_TRACE_IR_MAPPING_STATUS_OK) {
			BT_COMP_LOGE_APPEND_CAUSE(self_comp, "Error copying content of variant option field class: "
				"in-option-fc=%p, in-option-name=\"%s\"",
				in_option_fc, option_name);
			BT_FIELD_CLASS_PUT_REF_AND_RESET(out_option_fc);
			goto end;
		}

		if (fc_type == BT_FIELD_CLASS_TYPE_VARIANT_WITH_UNSIGNED_INTEGER_SELECTOR_FIELD) {
			const bt_field_class_variant_with_selector_field_integer_unsigned_option *spec_opt =
				bt_field_class_variant_with_selector_field_integer_unsigned_borrow_option_by_index_const(
					in_field_class, i);
			const bt_integer_range_set_unsigned *ranges =
				bt_field_class_variant_with_selector_field_integer_unsigned_option_borrow_ranges_const(
					spec_opt);
			enum bt_field_class_variant_with_selector_field_integer_append_option_status append_opt_status =
				bt_field_class_variant_with_selector_field_integer_unsigned_append_option(
					out_field_class, option_name,
					out_option_fc, ranges);

			if (append_opt_status !=
					BT_FIELD_CLASS_VARIANT_WITH_SELECTOR_FIELD_APPEND_OPTION_STATUS_OK) {
				BT_COMP_LOGE_APPEND_CAUSE(self_comp, "Cannot append option to variant field class with unsigned integer selector"
					"out-fc-addr=%p, out-option-fc-addr=%p, "
					"out-option-name=\"%s\"", out_field_class,
					out_option_fc, option_name);
				BT_FIELD_CLASS_PUT_REF_AND_RESET(out_tag_field_class);
				status = (int) append_opt_status;
				goto end;
			}
		} else if (fc_type == BT_FIELD_CLASS_TYPE_VARIANT_WITH_SIGNED_INTEGER_SELECTOR_FIELD) {
			const bt_field_class_variant_with_selector_field_integer_signed_option *spec_opt =
				bt_field_class_variant_with_selector_field_integer_signed_borrow_option_by_index_const(
					in_field_class, i);
			const bt_integer_range_set_signed *ranges =
				bt_field_class_variant_with_selector_field_integer_signed_option_borrow_ranges_const(
					spec_opt);

			enum bt_field_class_variant_with_selector_field_integer_append_option_status append_opt_status =
				bt_field_class_variant_with_selector_field_integer_signed_append_option(
					out_field_class, option_name,
					out_option_fc, ranges);
			if (append_opt_status !=
					BT_FIELD_CLASS_VARIANT_WITH_SELECTOR_FIELD_APPEND_OPTION_STATUS_OK) {
				BT_COMP_LOGE_APPEND_CAUSE(self_comp, "Cannot append option to variant field class with signed integer selector"
					"out-fc-addr=%p, out-option-fc-addr=%p, "
					"out-option-name=\"%s\"", out_field_class,
					out_option_fc, option_name);
				BT_FIELD_CLASS_PUT_REF_AND_RESET(out_tag_field_class);
				status = (int) append_opt_status;
				goto end;
			}
		} else {
			BT_ASSERT(fc_type == BT_FIELD_CLASS_TYPE_VARIANT_WITHOUT_SELECTOR_FIELD);

			enum bt_field_class_variant_without_selector_append_option_status append_opt_status =
				bt_field_class_variant_without_selector_append_option(
					out_field_class, option_name,
					out_option_fc);
			if (append_opt_status !=
					BT_FIELD_CLASS_VARIANT_WITHOUT_SELECTOR_FIELD_APPEND_OPTION_STATUS_OK) {
				BT_COMP_LOGE_APPEND_CAUSE(self_comp, "Cannot append option to variant field class"
					"out-fc-addr=%p, out-option-fc-addr=%p, "
					"out-option-name=\"%s\"", out_field_class,
					out_option_fc, option_name);
				BT_FIELD_CLASS_PUT_REF_AND_RESET(out_tag_field_class);
				status = (int) append_opt_status;
				goto end;
			}
		}

		out_option = bt_field_class_variant_borrow_option_by_index(
			out_field_class, i);
		BT_ASSERT(out_option);

		/*
		 * Safe to use the same value object because it's frozen
		 * at this point.
		 */
		bt_field_class_variant_option_set_user_attributes(
			out_option,
			bt_field_class_variant_option_borrow_user_attributes_const(
				in_option));
	}

	BT_COMP_LOGD("Copied content of variant field class: in-fc-addr=%p, "
		"out-fc-addr=%p", in_field_class, out_field_class);
	status = DEBUG_INFO_TRACE_IR_MAPPING_STATUS_OK;
end:
	return status;
}

static inline
enum debug_info_trace_ir_mapping_status field_class_static_array_copy(
		struct trace_ir_metadata_maps *md_maps,
		const bt_field_class *in_field_class,
		bt_field_class *out_field_class)
{
	BT_COMP_LOGD("Copying content of static array field class: in-fc-addr=%p, "
		"out-fc-addr=%p", in_field_class, out_field_class);
	/*
	 * There is no content to copy. Keep this function call anyway for
	 * logging purposes.
	 */
	BT_COMP_LOGD("Copied content of static array field class: in-fc-addr=%p, "
		"out-fc-addr=%p", in_field_class, out_field_class);

	return DEBUG_INFO_TRACE_IR_MAPPING_STATUS_OK;
}

static inline
enum debug_info_trace_ir_mapping_status field_class_dynamic_array_copy(
		struct trace_ir_metadata_maps *md_maps,
		const bt_field_class *in_field_class,
		bt_field_class *out_field_class)
{
	BT_COMP_LOGD("Copying content of dynamic array field class: "
		"in-fc-addr=%p, out-fc-addr=%p", in_field_class, out_field_class);
	/*
	 * There is no content to copy. Keep this function call anyway for
	 * logging purposes.
	 */
	BT_COMP_LOGD("Copied content of dynamic array field class: "
		"in-fc-addr=%p, out-fc-addr=%p", in_field_class, out_field_class);

	return DEBUG_INFO_TRACE_IR_MAPPING_STATUS_OK;
}

static inline
enum debug_info_trace_ir_mapping_status field_class_option_copy(
		struct trace_ir_metadata_maps *md_maps,
		const bt_field_class *in_field_class,
		bt_field_class *out_field_class)
{
	BT_COMP_LOGD("Copying content of option field class: "
		"in-fc-addr=%p, out-fc-addr=%p", in_field_class, out_field_class);

	if (bt_field_class_get_type(out_field_class) ==
			BT_FIELD_CLASS_TYPE_OPTION_WITH_BOOL_SELECTOR_FIELD) {
		bt_field_class_option_with_selector_field_bool_set_selector_is_reversed(
			out_field_class,
			bt_field_class_option_with_selector_field_bool_selector_is_reversed(
				in_field_class));
	}

	BT_COMP_LOGD("Copied content of option field class: "
		"in-fc-addr=%p, out-fc-addr=%p", in_field_class, out_field_class);

	return DEBUG_INFO_TRACE_IR_MAPPING_STATUS_OK;
}

static inline
enum debug_info_trace_ir_mapping_status field_class_string_copy(
		struct trace_ir_metadata_maps *md_maps,
		const bt_field_class *in_field_class,
		bt_field_class *out_field_class)
{
	BT_COMP_LOGD("Copying content of string field class: in-fc-addr=%p, "
		"out-fc-addr=%p", in_field_class, out_field_class);
	/*
	 * There is no content to copy. Keep this function call anyway for
	 * logging purposes.
	 */
	BT_COMP_LOGD("Copied content of string field class: in-fc-addr=%p, "
		"out-fc-addr=%p", in_field_class, out_field_class);

	return DEBUG_INFO_TRACE_IR_MAPPING_STATUS_OK;
}

static
bt_field_class *copy_field_class_array_element(
		struct trace_ir_metadata_maps *md_maps,
		const bt_field_class *in_elem_fc)
{
	bt_self_component *self_comp = md_maps->self_comp;
	bt_field_class *out_elem_fc =
		create_field_class_copy_internal(md_maps, in_elem_fc);
	if (!out_elem_fc) {
		BT_COMP_LOGE_APPEND_CAUSE(self_comp,
			"Error creating output elem field class from input elem field class for static array: "
			"in-fc-addr=%p", in_elem_fc);
		goto end;
	}

	if (copy_field_class_content_internal(md_maps, in_elem_fc, out_elem_fc) !=
			DEBUG_INFO_TRACE_IR_MAPPING_STATUS_OK) {
		BT_COMP_LOGE_APPEND_CAUSE(self_comp,
			"Error creating output elem field class from input elem field class for static array: "
			"in-fc-addr=%p", in_elem_fc);
		BT_FIELD_CLASS_PUT_REF_AND_RESET(out_elem_fc);
		goto end;
	}

end:
	return out_elem_fc;
}

BT_HIDDEN
bt_field_class *create_field_class_copy_internal(
		struct trace_ir_metadata_maps *md_maps,
		const bt_field_class *in_field_class)
{
	bt_self_component *self_comp = md_maps->self_comp;
	enum debug_info_trace_ir_mapping_status status;
	bt_field_class *out_field_class = NULL;
	bt_field_class_type fc_type = bt_field_class_get_type(in_field_class);

	BT_COMP_LOGD("Creating bare field class based on field class: in-fc-addr=%p",
		in_field_class);

	switch (fc_type) {
	case BT_FIELD_CLASS_TYPE_BOOL:
		out_field_class = bt_field_class_bool_create(
			md_maps->output_trace_class);
		break;
	case BT_FIELD_CLASS_TYPE_BIT_ARRAY:
		out_field_class = bt_field_class_bit_array_create(
			md_maps->output_trace_class,
			bt_field_class_bit_array_get_length(
				in_field_class));
		break;
	case BT_FIELD_CLASS_TYPE_UNSIGNED_INTEGER:
		out_field_class = bt_field_class_integer_unsigned_create(
			md_maps->output_trace_class);
		break;
	case BT_FIELD_CLASS_TYPE_SIGNED_INTEGER:
		out_field_class = bt_field_class_integer_signed_create(
			md_maps->output_trace_class);
		break;
	case BT_FIELD_CLASS_TYPE_UNSIGNED_ENUMERATION:
		out_field_class = bt_field_class_enumeration_unsigned_create(
			md_maps->output_trace_class);
		break;
	case BT_FIELD_CLASS_TYPE_SIGNED_ENUMERATION:
		out_field_class = bt_field_class_enumeration_signed_create(
			md_maps->output_trace_class);
		break;
	case BT_FIELD_CLASS_TYPE_SINGLE_PRECISION_REAL:
		out_field_class = bt_field_class_real_single_precision_create(
			md_maps->output_trace_class);
		break;
	case BT_FIELD_CLASS_TYPE_DOUBLE_PRECISION_REAL:
		out_field_class = bt_field_class_real_double_precision_create(
			md_maps->output_trace_class);
		break;
	case BT_FIELD_CLASS_TYPE_STRING:
		out_field_class = bt_field_class_string_create(
			md_maps->output_trace_class);
		break;
	case BT_FIELD_CLASS_TYPE_STRUCTURE:
		out_field_class = bt_field_class_structure_create(
			md_maps->output_trace_class);
		break;
	case BT_FIELD_CLASS_TYPE_STATIC_ARRAY:
	{
		const bt_field_class *in_elem_fc =
			bt_field_class_array_borrow_element_field_class_const(
					in_field_class);
		uint64_t array_len = bt_field_class_array_static_get_length(
			in_field_class);

		bt_field_class *out_elem_fc = copy_field_class_array_element(
				md_maps, in_elem_fc);
		if (!out_elem_fc) {
			out_field_class = NULL;
			goto error;
		}

		out_field_class = bt_field_class_array_static_create(
			md_maps->output_trace_class,
			out_elem_fc, array_len);
		break;
	}
	default:
		break;
	}

	if (bt_field_class_type_is(fc_type,
			BT_FIELD_CLASS_TYPE_DYNAMIC_ARRAY)) {
		const bt_field_class *in_elem_fc =
			bt_field_class_array_borrow_element_field_class_const(
					in_field_class);
		bt_field_class *out_length_fc = NULL;
		bt_field_class *out_elem_fc = copy_field_class_array_element(
			md_maps, in_elem_fc);

		if (!out_elem_fc) {
			out_field_class = NULL;
			goto error;
		}

		if (fc_type == BT_FIELD_CLASS_TYPE_DYNAMIC_ARRAY_WITH_LENGTH_FIELD) {
			const bt_field_path *length_fp =
				bt_field_class_array_dynamic_with_length_field_borrow_length_field_path_const(
					in_field_class);
			const bt_field_class *in_length_fc =
				resolve_field_path_to_field_class(length_fp,
					md_maps);

			BT_ASSERT(in_length_fc);
			out_length_fc = g_hash_table_lookup(md_maps->field_class_map,
				in_length_fc);
			BT_ASSERT(out_length_fc);
		}

		out_field_class = bt_field_class_array_dynamic_create(
			md_maps->output_trace_class, out_elem_fc, out_length_fc);
	} else if (bt_field_class_type_is(fc_type, BT_FIELD_CLASS_TYPE_OPTION)) {
		const bt_field_class *in_content_fc =
			bt_field_class_option_borrow_field_class_const(
				in_field_class);
		bt_field_class *out_selector_fc = NULL;
		bt_field_class *out_content_fc;

		out_content_fc = create_field_class_copy_internal(
			md_maps, in_content_fc);
		if (!out_content_fc) {
			BT_COMP_LOGE_APPEND_CAUSE(self_comp,
				"Cannot copy option's content field class: "
				"in-content-fc-addr=%p", in_content_fc);
			goto error;
		}

		status = copy_field_class_content_internal(md_maps,
			in_content_fc, out_content_fc);
		if (status != DEBUG_INFO_TRACE_IR_MAPPING_STATUS_OK) {
			BT_COMP_LOGE_APPEND_CAUSE(self_comp,
				"Error copying content of option's content field class: "
				"in-content-fc-addr=%p, out-content-fc-addr=%p",
				in_content_fc, out_content_fc);
			BT_FIELD_CLASS_PUT_REF_AND_RESET(out_content_fc);
			goto error;
		}

		if (fc_type == BT_FIELD_CLASS_TYPE_OPTION_WITHOUT_SELECTOR_FIELD) {
			out_field_class =
				bt_field_class_option_without_selector_create(
					md_maps->output_trace_class,
					out_content_fc);
		} else {
			const bt_field_path *in_selector_fp =
				bt_field_class_option_with_selector_field_borrow_selector_field_path_const(
					in_field_class);
			const bt_field_class *in_selector_fc;

			BT_ASSERT(in_selector_fp);
			in_selector_fc = resolve_field_path_to_field_class(
				in_selector_fp, md_maps);
			BT_ASSERT(in_selector_fc);
			out_selector_fc = g_hash_table_lookup(
				md_maps->field_class_map, in_selector_fc);
			BT_ASSERT(out_selector_fc);

			if (fc_type == BT_FIELD_CLASS_TYPE_OPTION_WITH_BOOL_SELECTOR_FIELD) {
				out_field_class =
					bt_field_class_option_with_selector_field_bool_create(
						md_maps->output_trace_class,
						out_content_fc, out_selector_fc);
			} else if (fc_type == BT_FIELD_CLASS_TYPE_OPTION_WITH_UNSIGNED_INTEGER_SELECTOR_FIELD) {
				const bt_integer_range_set_unsigned *ranges =
					bt_field_class_option_with_selector_field_integer_unsigned_borrow_selector_ranges_const(
						in_field_class);

				BT_ASSERT(ranges);
				out_field_class =
					bt_field_class_option_with_selector_field_integer_unsigned_create(
						md_maps->output_trace_class,
						out_content_fc, out_selector_fc,
						ranges);
			} else if (fc_type == BT_FIELD_CLASS_TYPE_OPTION_WITH_SIGNED_INTEGER_SELECTOR_FIELD) {
				const bt_integer_range_set_signed *ranges =
					bt_field_class_option_with_selector_field_integer_signed_borrow_selector_ranges_const(
						in_field_class);

				BT_ASSERT(ranges);
				out_field_class =
					bt_field_class_option_with_selector_field_integer_signed_create(
						md_maps->output_trace_class,
						out_content_fc, out_selector_fc,
						ranges);
			}
		}
	} else if (bt_field_class_type_is(fc_type,
			BT_FIELD_CLASS_TYPE_VARIANT)) {
		bt_field_class *out_sel_fc = NULL;

		if (bt_field_class_type_is(fc_type,
				BT_FIELD_CLASS_TYPE_VARIANT_WITH_SELECTOR_FIELD)) {
			const bt_field_class *in_sel_fc;
			const bt_field_path *sel_fp =
				bt_field_class_variant_with_selector_field_borrow_selector_field_path_const(
					in_field_class);

			BT_ASSERT(sel_fp);
			in_sel_fc = resolve_field_path_to_field_class(sel_fp,
				md_maps);
			BT_ASSERT(in_sel_fc);
			out_sel_fc = g_hash_table_lookup(
				md_maps->field_class_map, in_sel_fc);
			BT_ASSERT(out_sel_fc);
		}

		out_field_class = bt_field_class_variant_create(
			md_maps->output_trace_class, out_sel_fc);
	}

	/*
	 * Add mapping from in_field_class to out_field_class. This simplifies
	 * the resolution of field paths in variant and dynamic array field
	 * classes.
	 */
	BT_ASSERT(out_field_class);
	g_hash_table_insert(md_maps->field_class_map,
		(gpointer) in_field_class, out_field_class);

error:
	if(out_field_class){
		BT_COMP_LOGD("Created bare field class based on field class: in-fc-addr=%p, "
				"out-fc-addr=%p", in_field_class, out_field_class);
	} else {
		BT_COMP_LOGE_APPEND_CAUSE(self_comp,
			"Error creating output field class from input field class: "
			"in-fc-addr=%p", in_field_class);
	}

	return out_field_class;
}

BT_HIDDEN
enum debug_info_trace_ir_mapping_status copy_field_class_content_internal(
		struct trace_ir_metadata_maps *md_maps,
		const bt_field_class *in_field_class,
		bt_field_class *out_field_class)
{
	enum debug_info_trace_ir_mapping_status status;
	bt_field_class_type in_fc_type =
		bt_field_class_get_type(in_field_class);

	/*
	 * Safe to use the same value object because it's frozen at this
	 * point.
	 */
	bt_field_class_set_user_attributes(out_field_class,
		bt_field_class_borrow_user_attributes_const(in_field_class));

	if (in_fc_type == BT_FIELD_CLASS_TYPE_BOOL) {
		status = field_class_bool_copy(md_maps,
			in_field_class, out_field_class);
	} else if (in_fc_type == BT_FIELD_CLASS_TYPE_BIT_ARRAY) {
		status = field_class_bit_array_copy(md_maps,
			in_field_class, out_field_class);
	} else if (in_fc_type == BT_FIELD_CLASS_TYPE_UNSIGNED_INTEGER) {
		status = field_class_unsigned_integer_copy(md_maps,
			in_field_class, out_field_class);
	} else if (in_fc_type == BT_FIELD_CLASS_TYPE_SIGNED_INTEGER) {
		status = field_class_signed_integer_copy(md_maps,
			in_field_class, out_field_class);
	} else if (in_fc_type == BT_FIELD_CLASS_TYPE_UNSIGNED_ENUMERATION) {
		status = field_class_unsigned_enumeration_copy(md_maps,
			in_field_class, out_field_class);
	} else if (in_fc_type == BT_FIELD_CLASS_TYPE_SIGNED_ENUMERATION) {
		status = field_class_signed_enumeration_copy(md_maps,
			in_field_class, out_field_class);
	} else if (in_fc_type == BT_FIELD_CLASS_TYPE_SINGLE_PRECISION_REAL) {
		status = field_class_single_precision_real_copy(md_maps,
			in_field_class, out_field_class);
	} else if (in_fc_type == BT_FIELD_CLASS_TYPE_DOUBLE_PRECISION_REAL) {
		status = field_class_double_precision_real_copy(md_maps,
			in_field_class, out_field_class);
	} else if (in_fc_type == BT_FIELD_CLASS_TYPE_STRING) {
		status = field_class_string_copy(md_maps,
			in_field_class, out_field_class);
	} else if (in_fc_type == BT_FIELD_CLASS_TYPE_STRUCTURE) {
		status = field_class_structure_copy(md_maps,
			in_field_class, out_field_class);
	} else if (in_fc_type == BT_FIELD_CLASS_TYPE_STATIC_ARRAY) {
		status = field_class_static_array_copy(md_maps,
			in_field_class, out_field_class);
	} else if (bt_field_class_type_is(in_fc_type,
			BT_FIELD_CLASS_TYPE_DYNAMIC_ARRAY)) {
		status = field_class_dynamic_array_copy(md_maps,
			in_field_class, out_field_class);
	} else if (bt_field_class_type_is(in_fc_type,
			BT_FIELD_CLASS_TYPE_OPTION)) {
		status = field_class_option_copy(md_maps,
			in_field_class, out_field_class);
	} else if (bt_field_class_type_is(in_fc_type,
			BT_FIELD_CLASS_TYPE_VARIANT)) {
		status = field_class_variant_copy(md_maps,
			in_field_class, out_field_class);
	} else {
		bt_common_abort();
	}

	return status;
}
