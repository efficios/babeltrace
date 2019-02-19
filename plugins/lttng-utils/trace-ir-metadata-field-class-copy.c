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

#define BT_LOG_TAG "PLUGIN-LTTNG-UTILS-DEBUG-INFO-TRACE-IR-METADATA-FC-COPY"
#include "logging.h"

#include <babeltrace/assert-internal.h>
#include <babeltrace/common-internal.h>
#include <babeltrace/compiler-internal.h>
#include <babeltrace/babeltrace.h>

#include "trace-ir-metadata-copy.h"
#include "trace-ir-metadata-field-class-copy.h"

/*
 * This fonction walks througth the nested structures field class to resolve a
 * field path object. A field path is made of indexes inside possibly nested
 * structures ultimately leading to a field class.
 */
static
const bt_field_class *walk_field_path(const bt_field_path *fp,
		const bt_field_class *fc)
{
	uint64_t i, fp_index_count;
	const bt_field_class *curr_fc;

	BT_ASSERT(bt_field_class_get_type(fc) == BT_FIELD_CLASS_TYPE_STRUCTURE);
	BT_LOGD("Walking field path on field class: fp-addr=%p, fc-addr=%p",
			fp, fc);

	fp_index_count = bt_field_path_get_index_count(fp);
	curr_fc = fc;
	for (i = 0; i < fp_index_count; i++) {
		const char *fc_name;
		bt_field_class_type fc_type = bt_field_class_get_type(curr_fc);
		uint64_t curr_index = bt_field_path_get_index_by_index(fp, i);

		switch (fc_type) {
		case BT_FIELD_CLASS_TYPE_STRUCTURE:
			bt_field_class_structure_borrow_member_by_index_const(
					curr_fc, curr_index, &fc_name, &curr_fc);
			break;
		case BT_FIELD_CLASS_TYPE_VARIANT:
			bt_field_class_variant_borrow_option_by_index_const(
					curr_fc, curr_index, &fc_name, &curr_fc);
			break;
		default:
			abort();
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
	bt_scope fp_scope;

	BT_LOGD("Resolving field path: fp-addr=%p", fp);

	fc_resolving_ctx = md_maps->fc_resolving_ctx;
	fp_scope = bt_field_path_get_root_scope(fp);

	switch (fp_scope) {
	case BT_SCOPE_PACKET_CONTEXT:
		fc = walk_field_path(fp, fc_resolving_ctx->packet_context);
		break;
	case BT_SCOPE_EVENT_COMMON_CONTEXT:
		fc = walk_field_path(fp, fc_resolving_ctx->event_common_context);
		break;
	case BT_SCOPE_EVENT_SPECIFIC_CONTEXT:
		fc = walk_field_path(fp, fc_resolving_ctx->event_specific_context);
		break;
	case BT_SCOPE_EVENT_PAYLOAD:
		fc = walk_field_path(fp, fc_resolving_ctx->event_payload);
		break;
	default:
		abort();
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
int field_class_unsigned_integer_copy(
		struct trace_ir_metadata_maps *md_maps,
		const bt_field_class *in_field_class,
		bt_field_class *out_field_class)
{
	BT_LOGD("Copying content of unsigned integer field class: "
			"in-fc-addr=%p, out-fc-addr=%p",
			in_field_class, out_field_class);

	field_class_integer_set_props(in_field_class, out_field_class);

	BT_LOGD("Copied content of unsigned integer field class: "
			"in-fc-addr=%p, out-fc-addr=%p",
			in_field_class, out_field_class);
	return 0;
}

static inline
int field_class_signed_integer_copy(
		struct trace_ir_metadata_maps *md_maps,
		const bt_field_class *in_field_class,
		bt_field_class *out_field_class)
{
	BT_LOGD("Copying content of signed integer field class: "
			"in-fc-addr=%p, out-fc-addr=%p",
			in_field_class, out_field_class);

	field_class_integer_set_props(in_field_class, out_field_class);

	BT_LOGD("Copied content of signed integer field class: "
			"in-fc-addr=%p, out-fc-addr=%p",
			in_field_class, out_field_class);
	return 0;
}

BT_HIDDEN
int field_class_unsigned_enumeration_copy(
		struct trace_ir_metadata_maps *md_maps,
		const bt_field_class *in_field_class,
		bt_field_class *out_field_class)
{
	uint64_t i, enum_mapping_count;
	int ret = 0;

	BT_LOGD("Copying content of unsigned enumeration field class: "
			"in-fc-addr=%p, out-fc-addr=%p",
			in_field_class, out_field_class);

	/* Copy properties of the inner integer. */
	field_class_integer_set_props(in_field_class, out_field_class);

	/* Copy all enumeration entries. */
	enum_mapping_count = bt_field_class_enumeration_get_mapping_count(in_field_class);
	for (i = 0; i < enum_mapping_count; i++) {
		const char *label;
		const bt_field_class_unsigned_enumeration_mapping_ranges *ranges;
		uint64_t range_index, range_count;

		/* Get the ranges and the range count. */
		bt_field_class_unsigned_enumeration_borrow_mapping_by_index_const(
				in_field_class, i, &label, &ranges);
		range_count =
			bt_field_class_unsigned_enumeration_mapping_ranges_get_range_count(
					ranges);
		/*
		 * Iterate over all the ranges to add them to copied field
		 * class.
		 */
		for (range_index = 0; range_index < range_count; range_index++) {
			uint64_t lower, upper;
			bt_field_class_status status;
			bt_field_class_unsigned_enumeration_mapping_ranges_get_range_by_index(
					ranges, range_index, &lower, &upper);

			BT_LOGD("Copying range in enumeration field class: "
					"label=%s, lower=%"PRId64", upper=%"PRId64,
					label, lower, upper);

			/* Add the label and its range to the copy field class. */
			status = bt_field_class_unsigned_enumeration_map_range(
					out_field_class, label, lower, upper);

			if (status != BT_FIELD_CLASS_STATUS_OK) {
				BT_LOGE_STR("Failed to add range to unsigned "
						"enumeration.");
				BT_FIELD_CLASS_PUT_REF_AND_RESET(out_field_class);
				ret = -1;
				goto error;
			}
		}
	}

	BT_LOGD("Copied content of unsigned enumeration field class: "
			"in-fc-addr=%p, out-fc-addr=%p",
			in_field_class, out_field_class);

error:
	return ret;
}

static inline
int field_class_signed_enumeration_copy(
		struct trace_ir_metadata_maps *md_maps,
		const bt_field_class *in_field_class,
		bt_field_class *out_field_class)
{
	uint64_t i, enum_mapping_count;
	int ret = 0;

	BT_LOGD("Copying content of signed enumeration field class: "
			"in-fc-addr=%p, out-fc-addr=%p",
			in_field_class, out_field_class);

	/* Copy properties of the inner integer. */
	field_class_integer_set_props(in_field_class, out_field_class);

	/* Copy all enumeration entries. */
	enum_mapping_count =
		bt_field_class_enumeration_get_mapping_count(in_field_class);
	for (i = 0; i < enum_mapping_count; i++) {
		const char *label;
		const bt_field_class_signed_enumeration_mapping_ranges *ranges;
		uint64_t range_index, range_count;

		/* Get the ranges and the range count. */
		bt_field_class_signed_enumeration_borrow_mapping_by_index_const(
				in_field_class, i, &label, &ranges);
		range_count =
			bt_field_class_signed_enumeration_mapping_ranges_get_range_count(
					ranges);
		/*
		 * Iterate over all the ranges to add them to copied field
		 * class.
		 */
		for (range_index = 0; range_index < range_count; range_index++) {
			int64_t lower, upper;
			bt_field_class_status status;
			bt_field_class_signed_enumeration_mapping_ranges_get_range_by_index(
					ranges, range_index, &lower, &upper);

			BT_LOGD("Copying range in enumeration field class: "
					"label=%s, lower=%ld, upper=%ld",
					label, lower, upper);

			/* Add the label and its range to the copy field class. */
			status = bt_field_class_signed_enumeration_map_range(
					out_field_class, label, lower, upper);
			if (status != BT_FIELD_CLASS_STATUS_OK) {
				BT_LOGE_STR("Failed to add range to signed "
						"enumeration.");
				BT_FIELD_CLASS_PUT_REF_AND_RESET(out_field_class);
				ret = -1;
				goto error;
			}
		}
	}

	BT_LOGD("Copied content of signed enumeration field class: "
			"in-fc-addr=%p, out-fc-addr=%p",
			in_field_class, out_field_class);

error:
	return ret;
}

static inline
int field_class_real_copy(
		struct trace_ir_metadata_maps *md_maps,
		const bt_field_class *in_field_class,
		bt_field_class *out_field_class)
{
	BT_LOGD("Copying content of real field class: "
			"in-fc-addr=%p, out-fc-addr=%p",
			in_field_class, out_field_class);

	bt_field_class_real_set_is_single_precision(out_field_class,
			bt_field_class_real_is_single_precision(in_field_class));

	BT_LOGD("Copied content real field class: in-fc-addr=%p, "
			"out-fc-addr=%p", in_field_class, out_field_class);

	return 0;
}

static inline
int field_class_structure_copy(
		struct trace_ir_metadata_maps *md_maps,
		const bt_field_class *in_field_class,
		bt_field_class *out_field_class)
{
	uint64_t i, struct_member_count;
	bt_field_class_status status;
	int ret = 0;

	BT_LOGD("Copying content of structure field class: "
			"in-fc-addr=%p, out-fc-addr=%p",
			in_field_class, out_field_class);
	/* Get the number of member in that struct. */
	struct_member_count =
		bt_field_class_structure_get_member_count(in_field_class);

	/* Iterate over all the members of the struct. */
	for (i = 0; i < struct_member_count; i++) {
		const char *member_name;
		const bt_field_class *member_fc;
		bt_field_class *out_member_field_class;

		bt_field_class_structure_borrow_member_by_index_const(
				in_field_class, i, &member_name, &member_fc);
		BT_LOGD("Copying structure field class's field: "
			"index=%" PRId64 ", "
			"member-fc-addr=%p, field-name=\"%s\"",
			i, member_fc, member_name);

		out_member_field_class = create_field_class_copy(md_maps,
				member_fc);
		if (!out_member_field_class) {
			BT_LOGE("Cannot copy structure field class's field: "
				"index=%" PRId64 ", "
				"field-fc-addr=%p, field-name=\"%s\"",
				i, member_fc, member_name);
			ret = -1;
			goto error;
		}
		ret = copy_field_class_content(md_maps, member_fc,
				out_member_field_class);
		if (ret) {
			goto error;
		}

		status = bt_field_class_structure_append_member(out_field_class,
				member_name, out_member_field_class);
		if (status != BT_FIELD_CLASS_STATUS_OK) {
			BT_LOGE("Cannot append structure field class's field: "
				"index=%" PRId64 ", "
				"field-fc-addr=%p, field-name=\"%s\"",
				i, member_fc, member_name);
			BT_FIELD_CLASS_PUT_REF_AND_RESET(out_member_field_class);
			ret = -1;
			goto error;
		}
	}

	BT_LOGD("Copied structure field class: original-fc-addr=%p, copy-fc-addr=%p",
		in_field_class, out_field_class);

error:
	return ret;
}

static inline
int field_class_variant_copy(
		struct trace_ir_metadata_maps *md_maps,
		const bt_field_class *in_field_class,
		bt_field_class *out_field_class)
{
	bt_field_class *out_tag_field_class;
	uint64_t i, variant_option_count;
	const bt_field_path *tag_fp;
	const bt_field_class *tag_fc;
	int ret = 0;

	BT_LOGD("Copying content of variant field class: "
			"in-fc-addr=%p, out-fc-addr=%p",
			in_field_class, out_field_class);

	tag_fp = bt_field_class_variant_borrow_selector_field_path_const(
			in_field_class);
	if (tag_fp) {
		tag_fc = resolve_field_path_to_field_class(tag_fp,
				md_maps);

		out_tag_field_class = g_hash_table_lookup(
				md_maps->field_class_map, tag_fc);
		if (!out_tag_field_class) {
			BT_LOGE_STR("Cannot find the tag field class.");
			ret = -1;
			goto error;
		}
		bt_field_class_variant_set_selector_field_class(out_field_class,
				out_tag_field_class);
	}

	variant_option_count =
		bt_field_class_variant_get_option_count(in_field_class);
	for (i = 0; i < variant_option_count; i++) {
		const bt_field_class *option;
		const char *option_name;
		bt_field_class *out_option_field_class;
		bt_field_class_status status;

		bt_field_class_variant_borrow_option_by_index_const(in_field_class,
				i, &option_name, &option);

		out_option_field_class = create_field_class_copy_internal(
				md_maps, option);
		if (!out_option_field_class) {
			BT_LOGE_STR("Cannot copy field class.");
			ret = -1;
			goto error;
		}
		ret = copy_field_class_content_internal(md_maps, option,
				out_option_field_class);
		if (ret) {
			BT_LOGE_STR("Error copying content of option variant "
					"field class'");
			goto error;
		}

		status = bt_field_class_variant_append_option(
				out_field_class, option_name,
				out_option_field_class);
		if (status != BT_FIELD_CLASS_STATUS_OK) {
			BT_LOGE_STR("Cannot append option to variant field class'");
			BT_FIELD_CLASS_PUT_REF_AND_RESET(out_tag_field_class);
			ret = -1;
			goto error;
		}
	}

	BT_LOGD("Copied content of variant field class: in-fc-addr=%p, "
		"out-fc-addr=%p", in_field_class, out_field_class);

error:
	return ret;
}

static inline
int field_class_static_array_copy(
		struct trace_ir_metadata_maps *md_maps,
		const bt_field_class *in_field_class,
		bt_field_class *out_field_class)
{
	BT_LOGD("Copying content of static array field class: in-fc-addr=%p, "
			"out-fc-addr=%p", in_field_class, out_field_class);
	/*
	 * There is no content to copy. Keep this function call anyway for
	 * logging purposes.
	 */
	BT_LOGD("Copied content of static array field class: in-fc-addr=%p, "
			"out-fc-addr=%p", in_field_class, out_field_class);

	return 0;
}

static inline
int field_class_dynamic_array_copy(
		struct trace_ir_metadata_maps *md_maps,
		const bt_field_class *in_field_class,
		bt_field_class *out_field_class)
{
	const bt_field_class *element_fc, *len_fc;
	const bt_field_path *len_fp;
	bt_field_class_status status;
	bt_field_class *out_len_field_class;
	int ret = 0;

	BT_LOGD("Copying content of dynamic array field class: "
			"in-fc-addr=%p, out-fc-addr=%p",
			in_field_class, out_field_class);

	element_fc = bt_field_class_array_borrow_element_field_class_const(
			in_field_class);
	len_fp = bt_field_class_dynamic_array_borrow_length_field_path_const(
			in_field_class);

	if (len_fp) {
		BT_LOGD("Copying dynamic array length field class using "
				"field path: in-len-fp=%p", len_fp);
		len_fc = resolve_field_path_to_field_class(
				len_fp, md_maps);
		out_len_field_class = g_hash_table_lookup(
				md_maps->field_class_map, len_fc);
		if (!out_len_field_class) {
			BT_LOGE_STR("Cannot find the output matching length"
					"field class.");
			ret = -1;
			goto error;
		}

		status = bt_field_class_dynamic_array_set_length_field_class(
				out_field_class, out_len_field_class);
		if (status != BT_FIELD_CLASS_STATUS_OK) {
			BT_LOGE_STR("Cannot set dynamic array field class' "
					"length field class.");
			BT_FIELD_CLASS_PUT_REF_AND_RESET(out_len_field_class);
			ret = -1;
			goto error;
		}
	}

	BT_LOGD("Copied dynamic array field class: in-fc-addr=%p, "
			"out-fc-addr=%p", in_field_class, out_field_class);

error:
	return ret;
}

static inline
int field_class_string_copy(struct trace_ir_metadata_maps *md_maps,
		const bt_field_class *in_field_class,
		bt_field_class *out_field_class)
{
	BT_LOGD("Copying content of string field class: in-fc-addr=%p, "
			"out-fc-addr=%p", in_field_class, out_field_class);
	/*
	 * There is no content to copy. Keep this function call anyway for
	 * logging purposes.
	 */
	BT_LOGD("Copied content of string field class: in-fc-addr=%p, "
			"out-fc-addr=%p", in_field_class, out_field_class);

	return 0;
}

static
bt_field_class *copy_field_class_array_element(struct trace_ir_metadata_maps *md_maps,
		const bt_field_class *in_elem_fc)
{
	int ret;
	bt_field_class *out_elem_fc =
		create_field_class_copy_internal(md_maps, in_elem_fc);
	if (!out_elem_fc) {
		BT_LOGE("Error creating output elem field class "
				"from input elem field class for static array: "
				"in-fc-addr=%p", in_elem_fc);
		goto error;
	}

	ret = copy_field_class_content_internal(md_maps, in_elem_fc, out_elem_fc);
	if (ret) {
		BT_LOGE("Error creating output elem field class "
				"from input elem field class for static array: "
				"in-fc-addr=%p", in_elem_fc);
		BT_FIELD_CLASS_PUT_REF_AND_RESET(out_elem_fc);
		goto error;
	}

error:
	return out_elem_fc;
}

BT_HIDDEN
bt_field_class *create_field_class_copy_internal(struct trace_ir_metadata_maps *md_maps,
		const bt_field_class *in_field_class)
{
	bt_field_class *out_field_class = NULL;

	BT_LOGD("Creating bare field class based on field class: in-fc-addr=%p",
			in_field_class);

	switch(bt_field_class_get_type(in_field_class)) {
	case BT_FIELD_CLASS_TYPE_UNSIGNED_INTEGER:
		out_field_class = bt_field_class_unsigned_integer_create(
				md_maps->output_trace_class);
		break;
	case BT_FIELD_CLASS_TYPE_SIGNED_INTEGER:
		out_field_class = bt_field_class_signed_integer_create(
				md_maps->output_trace_class);
		break;
	case BT_FIELD_CLASS_TYPE_UNSIGNED_ENUMERATION:
		out_field_class = bt_field_class_unsigned_enumeration_create(
				md_maps->output_trace_class);
		break;
	case BT_FIELD_CLASS_TYPE_SIGNED_ENUMERATION:
		out_field_class = bt_field_class_signed_enumeration_create(
				md_maps->output_trace_class);
		break;
	case BT_FIELD_CLASS_TYPE_REAL:
		out_field_class = bt_field_class_real_create(
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
		uint64_t array_len =
			bt_field_class_static_array_get_length(in_field_class);

		bt_field_class *out_elem_fc = copy_field_class_array_element(
				md_maps, in_elem_fc);
		if (!out_elem_fc) {
			out_field_class = NULL;
			goto error;
		}

		out_field_class = bt_field_class_static_array_create(
				md_maps->output_trace_class,
				out_elem_fc, array_len);
		break;
	}
	case BT_FIELD_CLASS_TYPE_DYNAMIC_ARRAY:
	{
		const bt_field_class *in_elem_fc =
			bt_field_class_array_borrow_element_field_class_const(
					in_field_class);

		bt_field_class *out_elem_fc = copy_field_class_array_element(
				md_maps, in_elem_fc);
		if (!out_elem_fc) {
			out_field_class = NULL;
			goto error;
		}

		out_field_class = bt_field_class_dynamic_array_create(
				md_maps->output_trace_class,
				out_elem_fc);
		break;
	}
	case BT_FIELD_CLASS_TYPE_VARIANT:
		out_field_class = bt_field_class_variant_create(
				md_maps->output_trace_class);
		break;
	default:
		abort();
	}

	/*
	 * Add mapping from in_field_class to out_field_class. This simplifies
	 * the resolution of field paths in variant and dynamic array field
	 * classes.
	 */
	g_hash_table_insert(md_maps->field_class_map,
			(gpointer) in_field_class, out_field_class);

error:
	if(out_field_class){
		BT_LOGD("Created bare field class based on field class: in-fc-addr=%p, "
				"out-fc-addr=%p", in_field_class, out_field_class);
	} else {
		BT_LOGE("Error creating output field class from input field "
			"class: in-fc-addr=%p", in_field_class);
	}

	return out_field_class;
}

BT_HIDDEN
int copy_field_class_content_internal(
		struct trace_ir_metadata_maps *md_maps,
		const bt_field_class *in_field_class,
		bt_field_class *out_field_class)
{
	int ret = 0;
	switch(bt_field_class_get_type(in_field_class)) {
	case BT_FIELD_CLASS_TYPE_UNSIGNED_INTEGER:
		ret = field_class_unsigned_integer_copy(md_maps,
				in_field_class, out_field_class);
		break;
	case BT_FIELD_CLASS_TYPE_SIGNED_INTEGER:
		ret = field_class_signed_integer_copy(md_maps,
				in_field_class, out_field_class);
		break;
	case BT_FIELD_CLASS_TYPE_UNSIGNED_ENUMERATION:
		ret = field_class_unsigned_enumeration_copy(md_maps,
				in_field_class, out_field_class);
		break;
	case BT_FIELD_CLASS_TYPE_SIGNED_ENUMERATION:
		ret = field_class_signed_enumeration_copy(md_maps,
				in_field_class, out_field_class);
		break;
	case BT_FIELD_CLASS_TYPE_REAL:
		ret = field_class_real_copy(md_maps,
				in_field_class, out_field_class);
		break;
	case BT_FIELD_CLASS_TYPE_STRING:
		ret = field_class_string_copy(md_maps,
				in_field_class, out_field_class);
		break;
	case BT_FIELD_CLASS_TYPE_STRUCTURE:
		ret = field_class_structure_copy(md_maps,
				in_field_class, out_field_class);
		break;
	case BT_FIELD_CLASS_TYPE_STATIC_ARRAY:
		ret = field_class_static_array_copy(md_maps,
				in_field_class, out_field_class);
		break;
	case BT_FIELD_CLASS_TYPE_DYNAMIC_ARRAY:
		ret = field_class_dynamic_array_copy(md_maps,
				in_field_class, out_field_class);
		break;
	case BT_FIELD_CLASS_TYPE_VARIANT:
		ret = field_class_variant_copy(md_maps,
				in_field_class, out_field_class);
		break;
	default:
		abort();
	}

	return ret;
}
