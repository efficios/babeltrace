/*
 * Babeltrace - Trace IR data object copy
 *
 * Copyright (c) 2015-2019 EfficiOS Inc. and Linux Foundation
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

#define BT_COMP_LOG_SELF_COMP self_comp
#define BT_LOG_OUTPUT_LEVEL log_level
#define BT_LOG_TAG "PLUGIN/FLT.LTTNG-UTILS.DEBUG-INFO/TRACE-IR-DATA-COPY"
#include "logging/comp-logging.h"

#include <inttypes.h>
#include <stdint.h>

#include "common/assert.h"

#include "trace-ir-data-copy.h"

BT_HIDDEN
void copy_trace_content(const bt_trace *in_trace, bt_trace *out_trace,
		bt_logging_level log_level, bt_self_component *self_comp)
{
	bt_trace_set_name_status status;
	const char *trace_name;
	uint64_t i, env_field_count;

	BT_COMP_LOGD("Copying content of trace: in-t-addr=%p, out-t-addr=%p",
			in_trace, out_trace);
	trace_name = bt_trace_get_name(in_trace);
	/* Copy the trace name. */
	if (trace_name) {
		status = bt_trace_set_name(out_trace, trace_name);
		if (status != BT_TRACE_SET_NAME_STATUS_OK) {
			BT_COMP_LOGE("Cannot set trace's name: trace-addr=%p, name=\"%s\"",
				out_trace, trace_name);
			bt_current_thread_clear_error();
			goto end;
		}
	}

	/*
	 * Do not copy the trace UUID as it may be modified and should
	 * no longer have the same UUID.
	 */

	/*
	 * Go over all the entries in the environment section of the
	 * trace and copy the content to the new trace.
	 */
	env_field_count = bt_trace_get_environment_entry_count(in_trace);
	for (i = 0; i < env_field_count; i++) {
		const char *value_name;
		const bt_value *value = NULL;
		bt_trace_set_environment_entry_status set_env_status;

		bt_trace_borrow_environment_entry_by_index_const(
			in_trace, i, &value_name, &value);

		BT_COMP_LOGD("Copying trace environnement entry: "
			"index=%" PRId64 ", value-addr=%p, value-name=%s",
			i, value, value_name);

		BT_ASSERT(value_name);
		BT_ASSERT(value);

		if (bt_value_is_signed_integer(value)) {
			set_env_status =
				bt_trace_set_environment_entry_integer(
						out_trace, value_name,
						bt_value_integer_signed_get(
							value));
		} else if (bt_value_is_string(value)) {
			set_env_status =
				bt_trace_set_environment_entry_string(
					out_trace, value_name,
					bt_value_string_get(value));
		} else {
			abort();
		}

		if (set_env_status !=
				BT_TRACE_SET_ENVIRONMENT_ENTRY_STATUS_OK) {
			BT_COMP_LOGE("Cannot copy trace's environment: "
				"trace-addr=%p, name=\"%s\"",
				out_trace, trace_name);
			bt_current_thread_clear_error();
			goto end;
		}
	}

	BT_COMP_LOGD("Copied content of trace: in-t-addr=%p, out-t-addr=%p",
			in_trace, out_trace);
end:
	return;
}

BT_HIDDEN
void copy_stream_content(const bt_stream *in_stream, bt_stream *out_stream,
		bt_logging_level log_level, bt_self_component *self_comp)
{
	const char *stream_name;
	bt_stream_set_name_status status;

	BT_COMP_LOGD("Copying content of stream: in-s-addr=%p, out-s-addr=%p",
			in_stream, out_stream);

	stream_name = bt_stream_get_name(in_stream);
	if (stream_name) {
		status = bt_stream_set_name(out_stream, stream_name);
		if (status != BT_STREAM_SET_NAME_STATUS_OK) {
			BT_COMP_LOGE("Cannot set stream's name: stream-addr=%p, "
				"name=%s", out_stream, stream_name);
			goto end;
		}
	}

	BT_COMP_LOGD("Copied content of stream: in-s-addr=%p, out-s-addr=%p",
			in_stream, out_stream);
end:
	return;
}

BT_HIDDEN
void copy_packet_content(const bt_packet *in_packet, bt_packet *out_packet,
		bt_logging_level log_level, bt_self_component *self_comp)
{
	const bt_field *in_context_field;
	bt_field *out_context_field;

	BT_COMP_LOGD("Copying content of packet: in-p-addr=%p, out-p-addr=%p",
			in_packet, out_packet);

	/* Copy context field. */
	in_context_field = bt_packet_borrow_context_field_const(in_packet);
	if (in_context_field) {
		out_context_field = bt_packet_borrow_context_field(out_packet);
		BT_ASSERT(out_context_field);
		copy_field_content(in_context_field, out_context_field,
			log_level, self_comp);
	}

	BT_COMP_LOGD("Copied content of packet: in-p-addr=%p, out-p-addr=%p",
			in_packet, out_packet);
	return;
}

BT_HIDDEN
void copy_event_content(const bt_event *in_event, bt_event *out_event,
		bt_logging_level log_level, bt_self_component *self_comp)
{
	const bt_field *in_common_ctx_field, *in_specific_ctx_field,
	      *in_payload_field;
	bt_field *out_common_ctx_field, *out_specific_ctx_field,
		 *out_payload_field;

	BT_COMP_LOGD("Copying content of event: in-e-addr=%p, out-e-addr=%p",
			in_event, out_event);
	in_common_ctx_field =
		bt_event_borrow_common_context_field_const(in_event);
	if (in_common_ctx_field) {
		out_common_ctx_field =
			bt_event_borrow_common_context_field(out_event);
		BT_ASSERT(out_common_ctx_field);
		copy_field_content(in_common_ctx_field,
			out_common_ctx_field, log_level, self_comp);
	}

	in_specific_ctx_field =
		bt_event_borrow_specific_context_field_const(in_event);
	if (in_specific_ctx_field) {
		out_specific_ctx_field =
			bt_event_borrow_specific_context_field(out_event);
		BT_ASSERT(out_specific_ctx_field);
		copy_field_content(in_specific_ctx_field,
			out_specific_ctx_field, log_level, self_comp);
	}

	in_payload_field = bt_event_borrow_payload_field_const(in_event);
	if (in_payload_field) {
		out_payload_field = bt_event_borrow_payload_field(out_event);
		BT_ASSERT(out_payload_field);
		copy_field_content(in_payload_field,
			out_payload_field, log_level, self_comp);
	}

	BT_COMP_LOGD("Copied content of event: in-e-addr=%p, out-e-addr=%p",
			in_event, out_event);
}

BT_HIDDEN
void copy_field_content(const bt_field *in_field, bt_field *out_field,
		bt_logging_level log_level, bt_self_component *self_comp)
{
	bt_field_class_type in_fc_type, out_fc_type;

	in_fc_type = bt_field_get_class_type(in_field);
	out_fc_type = bt_field_get_class_type(out_field);
	BT_ASSERT(in_fc_type == out_fc_type);

	BT_COMP_LOGT("Copying content of field: in-f-addr=%p, out-f-addr=%p",
			in_field, out_field);
	switch (in_fc_type) {
	case BT_FIELD_CLASS_TYPE_BOOL:
		bt_field_bool_set_value(out_field,
			bt_field_bool_get_value(in_field));
		break;
	case BT_FIELD_CLASS_TYPE_UNSIGNED_INTEGER:
	case BT_FIELD_CLASS_TYPE_UNSIGNED_ENUMERATION:
		bt_field_integer_unsigned_set_value(out_field,
				bt_field_integer_unsigned_get_value(in_field));
		break;
	case BT_FIELD_CLASS_TYPE_SIGNED_INTEGER:
	case BT_FIELD_CLASS_TYPE_SIGNED_ENUMERATION:
		bt_field_integer_signed_set_value(out_field,
				bt_field_integer_signed_get_value(in_field));
		break;
	case BT_FIELD_CLASS_TYPE_REAL:
		bt_field_real_set_value(out_field,
				bt_field_real_get_value(in_field));
		break;
	case BT_FIELD_CLASS_TYPE_STRING:
	{
		const char *str = bt_field_string_get_value(in_field);
		bt_field_string_set_value_status status =
			bt_field_string_set_value(out_field, str);
		if (status != BT_FIELD_STRING_SET_VALUE_STATUS_OK) {
			BT_COMP_LOGE("Cannot set string field's value: "
				"str-field-addr=%p, str=%s" PRId64,
				out_field, str);
			bt_current_thread_clear_error();

		}
		break;
	}
	case BT_FIELD_CLASS_TYPE_STRUCTURE:
	{
		uint64_t i, nb_member_struct;
		const bt_field *in_member_field;
		bt_field *out_member_field;
		const bt_field_class *in_field_class;
		const char *in_member_name;

		in_field_class = bt_field_borrow_class_const(in_field);
		nb_member_struct = bt_field_class_structure_get_member_count(
				in_field_class);

		/*
		 * Iterate over the fields by names in the input field to avoid
		 * problem if the struct fields are not in the same order after
		 * the debug-info was added.
		 */
		for (i = 0; i < nb_member_struct; i++) {
			const bt_field_class_structure_member *member =
				bt_field_class_structure_borrow_member_by_index_const(
					in_field_class, i);

			in_member_name =
				bt_field_class_structure_member_get_name(
					member);
			in_member_field =
				bt_field_structure_borrow_member_field_by_name_const(
						in_field, in_member_name);
			out_member_field =
				bt_field_structure_borrow_member_field_by_name(
						out_field, in_member_name);

			copy_field_content(in_member_field,
				out_member_field, log_level, self_comp);
		}
		break;
	}
	case BT_FIELD_CLASS_TYPE_DYNAMIC_ARRAY:
		/* fall through */
	case BT_FIELD_CLASS_TYPE_STATIC_ARRAY:
	{
		const bt_field *in_element_field;
		bt_field *out_element_field;
		uint64_t i, array_len;
		bt_field_array_dynamic_set_length_status set_len_status;

		array_len = bt_field_array_get_length(in_field);

		if (in_fc_type == BT_FIELD_CLASS_TYPE_DYNAMIC_ARRAY) {
			set_len_status = bt_field_array_dynamic_set_length(
				out_field, array_len);
			if (set_len_status !=
					BT_FIELD_DYNAMIC_ARRAY_SET_LENGTH_STATUS_OK) {
				BT_COMP_LOGE("Cannot set dynamic array field's "
					"length field: field-addr=%p, "
					"length=%" PRIu64, out_field, array_len);
				bt_current_thread_clear_error();
			}
		}

		for (i = 0; i < array_len; i++) {
			in_element_field =
				bt_field_array_borrow_element_field_by_index_const(
					in_field, i);
			out_element_field =
				bt_field_array_borrow_element_field_by_index(
					out_field, i);
			copy_field_content(in_element_field, out_element_field,
				log_level, self_comp);
		}
		break;
	}
	case BT_FIELD_CLASS_TYPE_OPTION:
	{
		const bt_field *in_option_field;
		bt_field *out_option_field;

		in_option_field = bt_field_option_borrow_field_const(in_field);

		if (in_option_field) {
			bt_field_option_set_has_field(out_field, BT_TRUE);
			out_option_field = bt_field_option_borrow_field(
				out_field);
			BT_ASSERT(out_option_field);
			copy_field_content(in_option_field, out_option_field,
				log_level, self_comp);
		} else {
			bt_field_option_set_has_field(out_field, BT_FALSE);
		}

		break;
	}
	case BT_FIELD_CLASS_TYPE_VARIANT_WITHOUT_SELECTOR:
	case BT_FIELD_CLASS_TYPE_VARIANT_WITH_UNSIGNED_SELECTOR:
	case BT_FIELD_CLASS_TYPE_VARIANT_WITH_SIGNED_SELECTOR:
	{
		bt_field_variant_select_option_field_by_index_status sel_opt_status;
		uint64_t in_selected_option_idx;
		const bt_field *in_option_field;
		bt_field *out_option_field;

		in_selected_option_idx =
			bt_field_variant_get_selected_option_field_index(
					in_field);
		sel_opt_status = bt_field_variant_select_option_field_by_index(out_field,
				in_selected_option_idx);
		if (sel_opt_status !=
				BT_FIELD_VARIANT_SELECT_OPTION_FIELD_STATUS_OK) {
			BT_COMP_LOGE("Cannot select variant field's option field: "
				"var-field-addr=%p, opt-index=%" PRId64,
				out_field, in_selected_option_idx);
			bt_current_thread_clear_error();
		}

		in_option_field = bt_field_variant_borrow_selected_option_field_const(in_field);
		out_option_field = bt_field_variant_borrow_selected_option_field(out_field);

		copy_field_content(in_option_field, out_option_field,
			log_level, self_comp);

		break;
	}
	default:
		abort();
	}
	BT_COMP_LOGT("Copied content of field: in-f-addr=%p, out-f-addr=%p",
			in_field, out_field);
}
