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

#define BT_LOG_TAG "PLUGIN-LTTNG-UTILS-DEBUG-INFO-TRACE-IR-DATA-COPY"
#include "logging.h"

#include <inttypes.h>
#include <stdint.h>

#include <babeltrace/assert-internal.h>

#include "trace-ir-data-copy.h"

BT_HIDDEN
void copy_trace_content(const bt_trace *in_trace, bt_trace *out_trace)
{
	bt_trace_status status;
	const char *trace_name;

	BT_LOGD("Copying content of trace: in-t-addr=%p, out-t-addr=%p",
			in_trace, out_trace);

	trace_name = bt_trace_get_name(in_trace);
	/* Copy the trace name. */
	if (trace_name) {
		status = bt_trace_set_name(out_trace, trace_name);
		if (status != BT_TRACE_STATUS_OK) {
			BT_LOGE("Cannot set trace's name: trace-addr=%p, name=\"%s\"",
					out_trace, trace_name);
			goto end;
		}
	}

	BT_LOGD("Copied content of trace: in-t-addr=%p, out-t-addr=%p",
			in_trace, out_trace);
end:
	return;
}

BT_HIDDEN
void copy_stream_content(const bt_stream *in_stream, bt_stream *out_stream)
{
	const char *stream_name;
	bt_stream_status status;

	BT_LOGD("Copying content of stream: in-s-addr=%p, out-s-addr=%p",
			in_stream, out_stream);

	stream_name = bt_stream_get_name(in_stream);
	if (stream_name) {
		status = bt_stream_set_name(out_stream, stream_name);
		if (status != BT_STREAM_STATUS_OK) {
			BT_LOGE("Cannot set stream's name: stream-addr=%p, "
				"name=%s", out_stream, stream_name);
			goto end;
		}
	}

	BT_LOGD("Copied content of stream: in-s-addr=%p, out-s-addr=%p",
			in_stream, out_stream);
end:
	return;
}

BT_HIDDEN
void copy_packet_content(const bt_packet *in_packet, bt_packet *out_packet)
{
	const bt_field *in_context_field;
	bt_field *out_context_field;

	BT_LOGD("Copying content of packet: in-p-addr=%p, out-p-addr=%p",
			in_packet, out_packet);

	/* Copy context field. */
	in_context_field = bt_packet_borrow_context_field_const(in_packet);
	if (in_context_field) {
		out_context_field = bt_packet_borrow_context_field(out_packet);
		BT_ASSERT(out_context_field);
		copy_field_content(in_context_field, out_context_field);
	}

	BT_LOGD("Copied content of packet: in-p-addr=%p, out-p-addr=%p",
			in_packet, out_packet);
	return;
}

BT_HIDDEN
void copy_event_content(const bt_event *in_event, bt_event *out_event)
{
	const bt_field *in_common_ctx_field, *in_specific_ctx_field,
	      *in_payload_field;
	bt_field *out_common_ctx_field, *out_specific_ctx_field,
		 *out_payload_field;

	BT_LOGD("Copying content of event: in-e-addr=%p, out-e-addr=%p",
			in_event, out_event);
	in_common_ctx_field =
		bt_event_borrow_common_context_field_const(in_event);
	if (in_common_ctx_field) {
		out_common_ctx_field =
			bt_event_borrow_common_context_field(out_event);
		BT_ASSERT(out_common_ctx_field);
		copy_field_content(in_common_ctx_field,
				out_common_ctx_field);
	}

	in_specific_ctx_field =
		bt_event_borrow_specific_context_field_const(in_event);
	if (in_specific_ctx_field) {
		out_specific_ctx_field =
			bt_event_borrow_specific_context_field(out_event);
		BT_ASSERT(out_specific_ctx_field);
		copy_field_content(in_specific_ctx_field,
				out_specific_ctx_field);
	}

	in_payload_field = bt_event_borrow_payload_field_const(in_event);
	if (in_payload_field) {
		out_payload_field = bt_event_borrow_payload_field(out_event);
		BT_ASSERT(out_payload_field);
		copy_field_content(in_payload_field,
				out_payload_field);
	}

	BT_LOGD("Copied content of event: in-e-addr=%p, out-e-addr=%p",
			in_event, out_event);
}

BT_HIDDEN
void copy_field_content(const bt_field *in_field, bt_field *out_field)
{
	bt_field_class_type in_fc_type, out_fc_type;

	in_fc_type = bt_field_get_class_type(in_field);
	out_fc_type = bt_field_get_class_type(out_field);
	BT_ASSERT(in_fc_type == out_fc_type);

	BT_LOGD("Copying content of field: in-f-addr=%p, out-f-addr=%p",
			in_field, out_field);
	switch (in_fc_type) {
	case BT_FIELD_CLASS_TYPE_UNSIGNED_INTEGER:
	case BT_FIELD_CLASS_TYPE_UNSIGNED_ENUMERATION:
		bt_field_unsigned_integer_set_value(out_field,
				bt_field_unsigned_integer_get_value(in_field));
		break;
	case BT_FIELD_CLASS_TYPE_SIGNED_INTEGER:
	case BT_FIELD_CLASS_TYPE_SIGNED_ENUMERATION:
		bt_field_signed_integer_set_value(out_field,
				bt_field_signed_integer_get_value(in_field));
		break;
	case BT_FIELD_CLASS_TYPE_REAL:
		bt_field_real_set_value(out_field,
				bt_field_real_get_value(in_field));
		break;
	case BT_FIELD_CLASS_TYPE_STRING:
	{
		const char *str = bt_field_string_get_value(in_field);
		bt_field_status status = bt_field_string_set_value(out_field, str);
		if (status != BT_FIELD_STATUS_OK) {
			BT_LOGE("Cannot set string field's value: "
				"str-field-addr=%p, str=%s" PRId64,
				out_field, str);
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
					out_member_field);
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
		bt_field_status status;

		array_len = bt_field_array_get_length(in_field);

		if (in_fc_type == BT_FIELD_CLASS_TYPE_DYNAMIC_ARRAY) {
			status = bt_field_dynamic_array_set_length(out_field,
					array_len);
			if (status != BT_FIELD_STATUS_OK) {
				BT_LOGE("Cannot set dynamic array field's "
					"length field: field-addr=%p, "
					"length=%" PRIu64, out_field, array_len);
			}
		}

		for (i = 0; i < array_len; i++) {
			in_element_field =
				bt_field_array_borrow_element_field_by_index_const(
						in_field, i);
			out_element_field =
				bt_field_array_borrow_element_field_by_index(
						out_field, i);
			copy_field_content(in_element_field, out_element_field);
		}
		break;
	}
	case BT_FIELD_CLASS_TYPE_VARIANT:
	{
		bt_field_status status;
		uint64_t in_selected_option_idx;
		const bt_field *in_option_field;
		bt_field *out_option_field;

		in_selected_option_idx =
			bt_field_variant_get_selected_option_field_index(
					in_field);
		status = bt_field_variant_select_option_field(out_field,
				in_selected_option_idx);
		if (status != BT_FIELD_STATUS_OK) {
			BT_LOGE("Cannot select variant field's option field: "
				"var-field-addr=%p, opt-index=%" PRId64,
				out_field, in_selected_option_idx);
		}

		in_option_field = bt_field_variant_borrow_selected_option_field_const(in_field);
		out_option_field = bt_field_variant_borrow_selected_option_field(out_field);

		copy_field_content(in_option_field, out_option_field);

		break;
	}
	default:
		abort();
	}
	BT_LOGD("Copied content of field: in-f-addr=%p, out-f-addr=%p",
			in_field, out_field);
}
