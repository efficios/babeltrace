/*
 * Copyright 2013, 2014 Jérémie Galarneau <jeremie.galarneau@efficios.com>
 * Copyright 2017-2018 Philippe Proulx <pproulx@efficios.com>
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

#define BT_LOG_TAG "CTF-WRITER-TRACE"
#include <babeltrace/lib-logging-internal.h>

#include <babeltrace/assert-internal.h>
#include <babeltrace/compiler-internal.h>
#include <babeltrace/ctf-writer/attributes-internal.h>
#include <babeltrace/ctf-writer/clock-class-internal.h>
#include <babeltrace/ctf-writer/clock-internal.h>
#include <babeltrace/ctf-writer/event-class-internal.h>
#include <babeltrace/ctf-writer/event.h>
#include <babeltrace/ctf-writer/event-internal.h>
#include <babeltrace/ctf-writer/field-types-internal.h>
#include <babeltrace/ctf-writer/field-wrapper-internal.h>
#include <babeltrace/ctf-writer/functor-internal.h>
#include <babeltrace/ctf-writer/stream-class-internal.h>
#include <babeltrace/ctf-writer/stream-internal.h>
#include <babeltrace/ctf-writer/trace-internal.h>
#include <babeltrace/ctf-writer/utils-internal.h>
#include <babeltrace/ctf-writer/utils.h>
#include <babeltrace/ctf-writer/validation-internal.h>
#include <babeltrace/ctf-writer/visitor-internal.h>
#include <babeltrace/ctf-writer/writer-internal.h>
#include <babeltrace/endian-internal.h>
#include <babeltrace/object.h>
#include <babeltrace/types.h>
#include <babeltrace/values-internal.h>
#include <babeltrace/values.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define DEFAULT_IDENTIFIER_SIZE		128
#define DEFAULT_METADATA_STRING_SIZE	4096

BT_HIDDEN
int bt_ctf_trace_common_initialize(struct bt_ctf_trace_common *trace,
		bt_object_release_func release_func)
{
	int ret = 0;

	BT_LOGD_STR("Initializing common trace object.");
	trace->native_byte_order = BT_CTF_BYTE_ORDER_UNSPECIFIED;
	bt_object_init_shared_with_parent(&trace->base, release_func);
	trace->clock_classes = g_ptr_array_new_with_free_func(
		(GDestroyNotify) bt_object_put_ref);
	if (!trace->clock_classes) {
		BT_LOGE_STR("Failed to allocate one GPtrArray.");
		goto error;
	}

	trace->streams = g_ptr_array_new_with_free_func(
		(GDestroyNotify) bt_object_try_spec_release);
	if (!trace->streams) {
		BT_LOGE_STR("Failed to allocate one GPtrArray.");
		goto error;
	}

	trace->stream_classes = g_ptr_array_new_with_free_func(
		(GDestroyNotify) bt_object_try_spec_release);
	if (!trace->stream_classes) {
		BT_LOGE_STR("Failed to allocate one GPtrArray.");
		goto error;
	}

	/* Create the environment array object */
	trace->environment = bt_ctf_attributes_create();
	if (!trace->environment) {
		BT_LOGE_STR("Cannot create empty attributes object.");
		goto error;
	}

	BT_LOGD("Initialized common trace object: addr=%p", trace);
	goto end;

error:
	ret = -1;

end:
	return ret;
}

BT_HIDDEN
void bt_ctf_trace_common_finalize(struct bt_ctf_trace_common *trace)
{
	BT_LOGD("Finalizing common trace object: addr=%p, name=\"%s\"",
		trace, bt_ctf_trace_common_get_name(trace));

	if (trace->environment) {
		BT_LOGD_STR("Destroying environment attributes.");
		bt_ctf_attributes_destroy(trace->environment);
	}

	if (trace->name) {
		g_string_free(trace->name, TRUE);
	}

	if (trace->clock_classes) {
		BT_LOGD_STR("Putting clock classes.");
		g_ptr_array_free(trace->clock_classes, TRUE);
	}

	if (trace->streams) {
		BT_LOGD_STR("Destroying streams.");
		g_ptr_array_free(trace->streams, TRUE);
	}

	if (trace->stream_classes) {
		BT_LOGD_STR("Destroying stream classes.");
		g_ptr_array_free(trace->stream_classes, TRUE);
	}

	BT_LOGD_STR("Putting packet header field type.");
	bt_object_put_ref(trace->packet_header_field_type);
}

BT_HIDDEN
int bt_ctf_trace_common_set_name(struct bt_ctf_trace_common *trace, const char *name)
{
	int ret = 0;

	if (!trace) {
		BT_LOGW_STR("Invalid parameter: trace is NULL.");
		ret = -1;
		goto end;
	}

	if (!name) {
		BT_LOGW_STR("Invalid parameter: name is NULL.");
		ret = -1;
		goto end;
	}

	if (trace->frozen) {
		BT_LOGW("Invalid parameter: trace is frozen: "
			"addr=%p, name=\"%s\"",
			trace, bt_ctf_trace_common_get_name(trace));
		ret = -1;
		goto end;
	}

	trace->name = trace->name ? g_string_assign(trace->name, name) :
			g_string_new(name);
	if (!trace->name) {
		BT_LOGE_STR("Failed to allocate one GString.");
		ret = -1;
		goto end;
	}

	BT_LOGV("Set trace's name: addr=%p, name=\"%s\"", trace, name);

end:
	return ret;
}

BT_HIDDEN
int bt_ctf_trace_common_set_uuid(struct bt_ctf_trace_common *trace,
		const unsigned char *uuid)
{
	int ret = 0;

	if (!trace) {
		BT_LOGW_STR("Invalid parameter: trace is NULL.");
		ret = -1;
		goto end;
	}

	if (!uuid) {
		BT_LOGW_STR("Invalid parameter: UUID is NULL.");
		ret = -1;
		goto end;
	}

	if (trace->frozen) {
		BT_LOGW("Invalid parameter: trace is frozen: "
			"addr=%p, name=\"%s\"",
			trace, bt_ctf_trace_common_get_name(trace));
		ret = -1;
		goto end;
	}

	memcpy(trace->uuid, uuid, BABELTRACE_UUID_LEN);
	trace->uuid_set = BT_TRUE;
	BT_LOGV("Set trace's UUID: addr=%p, name=\"%s\", "
		"uuid=\"%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x\"",
		trace, bt_ctf_trace_common_get_name(trace),
		(unsigned int) uuid[0],
		(unsigned int) uuid[1],
		(unsigned int) uuid[2],
		(unsigned int) uuid[3],
		(unsigned int) uuid[4],
		(unsigned int) uuid[5],
		(unsigned int) uuid[6],
		(unsigned int) uuid[7],
		(unsigned int) uuid[8],
		(unsigned int) uuid[9],
		(unsigned int) uuid[10],
		(unsigned int) uuid[11],
		(unsigned int) uuid[12],
		(unsigned int) uuid[13],
		(unsigned int) uuid[14],
		(unsigned int) uuid[15]);

end:
	return ret;
}

BT_HIDDEN
int bt_ctf_trace_common_set_environment_field(struct bt_ctf_trace_common *trace,
		const char *name, struct bt_private_value *value)
{
	int ret = 0;

	if (!trace) {
		BT_LOGW_STR("Invalid parameter: trace is NULL.");
		ret = -1;
		goto end;
	}

	if (!name) {
		BT_LOGW_STR("Invalid parameter: name is NULL.");
		ret = -1;
		goto end;
	}

	if (!value) {
		BT_LOGW_STR("Invalid parameter: value is NULL.");
		ret = -1;
		goto end;
	}

	if (!bt_ctf_identifier_is_valid(name)) {
		BT_LOGW("Invalid parameter: environment field's name is not a valid CTF identifier: "
			"trace-addr=%p, trace-name=\"%s\", "
			"env-name=\"%s\"",
			trace, bt_ctf_trace_common_get_name(trace), name);
		ret = -1;
		goto end;
	}

	if (!bt_value_is_integer(bt_private_value_borrow_value(value)) &&
			!bt_value_is_string(bt_private_value_borrow_value(value))) {
		BT_LOGW("Invalid parameter: environment field's value is not an integer or string value: "
			"trace-addr=%p, trace-name=\"%s\", "
			"env-name=\"%s\", env-value-type=%s",
			trace, bt_ctf_trace_common_get_name(trace), name,
			bt_common_value_type_string(
				bt_value_get_type(
					bt_private_value_borrow_value(value))));
		ret = -1;
		goto end;
	}

	if (trace->frozen) {
		/*
		 * New environment fields may be added to a frozen trace,
		 * but existing fields may not be changed.
		 *
		 * The object passed is frozen like all other attributes.
		 */
		struct bt_private_value *attribute =
			bt_ctf_attributes_borrow_field_value_by_name(
				trace->environment, name);

		if (attribute) {
			BT_LOGW("Invalid parameter: trace is frozen and environment field already exists with this name: "
				"trace-addr=%p, trace-name=\"%s\", "
				"env-name=\"%s\"",
				trace, bt_ctf_trace_common_get_name(trace), name);
			ret = -1;
			goto end;
		}

		bt_value_freeze(bt_private_value_borrow_value(value));
	}

	ret = bt_ctf_attributes_set_field_value(trace->environment, name,
		value);
	if (ret) {
		BT_LOGE("Cannot set environment field's value: "
			"trace-addr=%p, trace-name=\"%s\", "
			"env-name=\"%s\"",
			trace, bt_ctf_trace_common_get_name(trace), name);
	} else {
		BT_LOGV("Set environment field's value: "
			"trace-addr=%p, trace-name=\"%s\", "
			"env-name=\"%s\", value-addr=%p",
			trace, bt_ctf_trace_common_get_name(trace), name, value);
	}

end:
	return ret;
}

BT_HIDDEN
int bt_ctf_trace_common_set_environment_field_string(struct bt_ctf_trace_common *trace,
		const char *name, const char *value)
{
	int ret = 0;
	struct bt_private_value *env_value_string_obj = NULL;

	if (!value) {
		BT_LOGW_STR("Invalid parameter: value is NULL.");
		ret = -1;
		goto end;
	}

	env_value_string_obj = bt_private_value_string_create_init(value);
	if (!env_value_string_obj) {
		BT_LOGE_STR("Cannot create string value object.");
		ret = -1;
		goto end;
	}

	/* bt_ctf_trace_common_set_environment_field() logs errors */
	ret = bt_ctf_trace_common_set_environment_field(trace, name,
		env_value_string_obj);

end:
	bt_object_put_ref(env_value_string_obj);
	return ret;
}

BT_HIDDEN
int bt_ctf_trace_common_set_environment_field_integer(
		struct bt_ctf_trace_common *trace, const char *name, int64_t value)
{
	int ret = 0;
	struct bt_private_value *env_value_integer_obj = NULL;

	env_value_integer_obj = bt_private_value_integer_create_init(value);
	if (!env_value_integer_obj) {
		BT_LOGE_STR("Cannot create integer value object.");
		ret = -1;
		goto end;
	}

	/* bt_ctf_trace_common_set_environment_field() logs errors */
	ret = bt_ctf_trace_common_set_environment_field(trace, name,
		env_value_integer_obj);

end:
	bt_object_put_ref(env_value_integer_obj);
	return ret;
}

BT_HIDDEN
int bt_ctf_trace_common_add_clock_class(struct bt_ctf_trace_common *trace,
		struct bt_ctf_clock_class *clock_class)
{
	int ret = 0;

	if (!trace) {
		BT_LOGW_STR("Invalid parameter: trace is NULL.");
		ret = -1;
		goto end;
	}

	if (!bt_ctf_clock_class_is_valid(clock_class)) {
		BT_LOGW("Invalid parameter: clock class is invalid: "
			"trace-addr=%p, trace-name=\"%s\", "
			"clock-class-addr=%p, clock-class-name=\"%s\"",
			trace, bt_ctf_trace_common_get_name(trace),
			clock_class, bt_ctf_clock_class_get_name(clock_class));
		ret = -1;
		goto end;
	}

	/* Check for duplicate clock classes */
	if (bt_ctf_trace_common_has_clock_class(trace, clock_class)) {
		BT_LOGW("Invalid parameter: clock class already exists in trace: "
			"trace-addr=%p, trace-name=\"%s\", "
			"clock-class-addr=%p, clock-class-name=\"%s\"",
			trace, bt_ctf_trace_common_get_name(trace),
			clock_class, bt_ctf_clock_class_get_name(clock_class));
		ret = -1;
		goto end;
	}

	bt_object_get_ref(clock_class);
	g_ptr_array_add(trace->clock_classes, clock_class);

	if (trace->frozen) {
		BT_LOGV_STR("Freezing added clock class because trace is frozen.");
		bt_ctf_clock_class_freeze(clock_class);
	}

	BT_LOGV("Added clock class to trace: "
		"trace-addr=%p, trace-name=\"%s\", "
		"clock-class-addr=%p, clock-class-name=\"%s\"",
		trace, bt_ctf_trace_common_get_name(trace),
		clock_class, bt_ctf_clock_class_get_name(clock_class));

end:
	return ret;
}

static
bool packet_header_field_type_is_valid(struct bt_ctf_trace_common *trace,
		struct bt_ctf_field_type_common *packet_header_type)
{
	int ret;
	bool is_valid = true;
	struct bt_ctf_field_type_common *field_type = NULL;

	if (!packet_header_type) {
		/*
		 * No packet header field type: trace must have only
		 * one stream. At this point the stream class being
		 * added is not part of the trace yet, so we validate
		 * that the trace contains no stream classes yet.
		 */
		if (trace->stream_classes->len >= 1) {
			BT_LOGW_STR("Invalid packet header field type: "
				"packet header field type does not exist but there's more than one stream class in the trace.");
			goto invalid;
		}

		/* No packet header field type: valid at this point */
		goto end;
	}

	/* Packet header field type, if it exists, must be a structure */
	if (packet_header_type->id != BT_CTF_FIELD_TYPE_ID_STRUCT) {
		BT_LOGW("Invalid packet header field type: must be a structure field type if it exists: "
			"ft-addr=%p, ft-id=%s",
			packet_header_type,
			bt_ctf_field_type_id_string(packet_header_type->id));
		goto invalid;
	}

	/*
	 * If there's a `magic` field, it must be a 32-bit unsigned
	 * integer field type. Also it must be the first field of the
	 * packet header field type.
	 */
	field_type = bt_ctf_field_type_common_structure_borrow_field_type_by_name(
		packet_header_type, "magic");
	if (field_type) {
		const char *field_name;

		if (field_type->id != BT_CTF_FIELD_TYPE_ID_INTEGER) {
			BT_LOGW("Invalid packet header field type: `magic` field must be an integer field type: "
				"magic-ft-addr=%p, magic-ft-id=%s",
				field_type,
				bt_ctf_field_type_id_string(field_type->id));
			goto invalid;
		}

		if (bt_ctf_field_type_common_integer_is_signed(field_type)) {
			BT_LOGW("Invalid packet header field type: `magic` field must be an unsigned integer field type: "
				"magic-ft-addr=%p", field_type);
			goto invalid;
		}

		if (bt_ctf_field_type_common_integer_get_size(field_type) != 32) {
			BT_LOGW("Invalid packet header field type: `magic` field must be a 32-bit unsigned integer field type: "
				"magic-ft-addr=%p, magic-ft-size=%u",
				field_type,
				bt_ctf_field_type_common_integer_get_size(field_type));
			goto invalid;
		}

		ret = bt_ctf_field_type_common_structure_borrow_field_by_index(
			packet_header_type, &field_name, NULL, 0);
		BT_ASSERT(ret == 0);

		if (strcmp(field_name, "magic") != 0) {
			BT_LOGW("Invalid packet header field type: `magic` field must be the first field: "
				"magic-ft-addr=%p, first-field-name=\"%s\"",
				field_type, field_name);
			goto invalid;
		}
	}

	/*
	 * If there's a `uuid` field, it must be an array field type of
	 * length 16 with an 8-bit unsigned integer element field type.
	 */
	field_type = bt_ctf_field_type_common_structure_borrow_field_type_by_name(
		packet_header_type, "uuid");
	if (field_type) {
		struct bt_ctf_field_type_common *elem_ft;

		if (field_type->id != BT_CTF_FIELD_TYPE_ID_ARRAY) {
			BT_LOGW("Invalid packet header field type: `uuid` field must be an array field type: "
				"uuid-ft-addr=%p, uuid-ft-id=%s",
				field_type,
				bt_ctf_field_type_id_string(field_type->id));
			goto invalid;
		}

		if (bt_ctf_field_type_common_array_get_length(field_type) != 16) {
			BT_LOGW("Invalid packet header field type: `uuid` array field type's length must be 16: "
				"uuid-ft-addr=%p, uuid-ft-length=%" PRId64,
				field_type,
				bt_ctf_field_type_common_array_get_length(field_type));
			goto invalid;
		}

		elem_ft = bt_ctf_field_type_common_array_borrow_element_field_type(field_type);
		BT_ASSERT(elem_ft);

		if (elem_ft->id != BT_CTF_FIELD_TYPE_ID_INTEGER) {
			BT_LOGW("Invalid packet header field type: `uuid` field's element field type must be an integer field type: "
				"elem-ft-addr=%p, elem-ft-id=%s",
				elem_ft,
				bt_ctf_field_type_id_string(elem_ft->id));
			goto invalid;
		}

		if (bt_ctf_field_type_common_integer_is_signed(elem_ft)) {
			BT_LOGW("Invalid packet header field type: `uuid` field's element field type must be an unsigned integer field type: "
				"elem-ft-addr=%p", elem_ft);
			goto invalid;
		}

		if (bt_ctf_field_type_common_integer_get_size(elem_ft) != 8) {
			BT_LOGW("Invalid packet header field type: `uuid` field's element field type must be an 8-bit unsigned integer field type: "
				"elem-ft-addr=%p, elem-ft-size=%u",
				elem_ft,
				bt_ctf_field_type_common_integer_get_size(elem_ft));
			goto invalid;
		}
	}

	/*
	 * The `stream_id` field must exist if there's more than one
	 * stream classes in the trace.
	 */
	field_type = bt_ctf_field_type_common_structure_borrow_field_type_by_name(
		packet_header_type, "stream_id");

	if (!field_type && trace->stream_classes->len >= 1) {
		BT_LOGW_STR("Invalid packet header field type: "
			"`stream_id` field does not exist but there's more than one stream class in the trace.");
		goto invalid;
	}

	/*
	 * If there's a `stream_id` field, it must be an unsigned
	 * integer field type.
	 */
	if (field_type) {
		if (field_type->id != BT_CTF_FIELD_TYPE_ID_INTEGER) {
			BT_LOGW("Invalid packet header field type: `stream_id` field must be an integer field type: "
				"stream-id-ft-addr=%p, stream-id-ft-id=%s",
				field_type,
				bt_ctf_field_type_id_string(field_type->id));
			goto invalid;
		}

		if (bt_ctf_field_type_common_integer_is_signed(field_type)) {
			BT_LOGW("Invalid packet header field type: `stream_id` field must be an unsigned integer field type: "
				"stream-id-ft-addr=%p", field_type);
			goto invalid;
		}
	}

	/*
	 * If there's a `packet_seq_num` field, it must be an unsigned
	 * integer field type.
	 */
	field_type = bt_ctf_field_type_common_structure_borrow_field_type_by_name(
		packet_header_type, "packet_seq_num");
	if (field_type) {
		if (field_type->id != BT_CTF_FIELD_TYPE_ID_INTEGER) {
			BT_LOGW("Invalid packet header field type: `packet_seq_num` field must be an integer field type: "
				"stream-id-ft-addr=%p, packet-seq-num-ft-id=%s",
				field_type,
				bt_ctf_field_type_id_string(field_type->id));
			goto invalid;
		}

		if (bt_ctf_field_type_common_integer_is_signed(field_type)) {
			BT_LOGW("Invalid packet header field type: `packet_seq_num` field must be an unsigned integer field type: "
				"packet-seq-num-ft-addr=%p", field_type);
			goto invalid;
		}
	}

	goto end;

invalid:
	is_valid = false;

end:
	return is_valid;
}

static
bool packet_context_field_type_is_valid(struct bt_ctf_trace_common *trace,
		struct bt_ctf_stream_class_common *stream_class,
		struct bt_ctf_field_type_common *packet_context_type,
		bool check_ts_begin_end_mapped)
{
	bool is_valid = true;
	struct bt_ctf_field_type_common *field_type = NULL;

	if (!packet_context_type) {
		/* No packet context field type: valid at this point */
		goto end;
	}

	/* Packet context field type, if it exists, must be a structure */
	if (packet_context_type->id != BT_CTF_FIELD_TYPE_ID_STRUCT) {
		BT_LOGW("Invalid packet context field type: must be a structure field type if it exists: "
			"ft-addr=%p, ft-id=%s",
			packet_context_type,
			bt_ctf_field_type_id_string(packet_context_type->id));
		goto invalid;
	}

	/*
	 * If there's a `packet_size` field, it must be an unsigned
	 * integer field type.
	 */
	field_type = bt_ctf_field_type_common_structure_borrow_field_type_by_name(
		packet_context_type, "packet_size");
	if (field_type) {
		if (field_type->id != BT_CTF_FIELD_TYPE_ID_INTEGER) {
			BT_LOGW("Invalid packet context field type: `packet_size` field must be an integer field type: "
				"packet-size-ft-addr=%p, packet-size-ft-id=%s",
				field_type,
				bt_ctf_field_type_id_string(field_type->id));
			goto invalid;
		}

		if (bt_ctf_field_type_common_integer_is_signed(field_type)) {
			BT_LOGW("Invalid packet context field type: `packet_size` field must be an unsigned integer field type: "
				"packet-size-ft-addr=%p", field_type);
			goto invalid;
		}
	}

	/*
	 * If there's a `content_size` field, it must be an unsigned
	 * integer field type.
	 */
	field_type = bt_ctf_field_type_common_structure_borrow_field_type_by_name(
		packet_context_type, "content_size");
	if (field_type) {
		if (field_type->id != BT_CTF_FIELD_TYPE_ID_INTEGER) {
			BT_LOGW("Invalid packet context field type: `content_size` field must be an integer field type: "
				"content-size-ft-addr=%p, content-size-ft-id=%s",
				field_type,
				bt_ctf_field_type_id_string(field_type->id));
			goto invalid;
		}

		if (bt_ctf_field_type_common_integer_is_signed(field_type)) {
			BT_LOGW("Invalid packet context field type: `content_size` field must be an unsigned integer field type: "
				"content-size-ft-addr=%p", field_type);
			goto invalid;
		}
	}

	/*
	 * If there's a `events_discarded` field, it must be an unsigned
	 * integer field type.
	 */
	field_type = bt_ctf_field_type_common_structure_borrow_field_type_by_name(
		packet_context_type, "events_discarded");
	if (field_type) {
		if (field_type->id != BT_CTF_FIELD_TYPE_ID_INTEGER) {
			BT_LOGW("Invalid packet context field type: `events_discarded` field must be an integer field type: "
				"events-discarded-ft-addr=%p, events-discarded-ft-id=%s",
				field_type,
				bt_ctf_field_type_id_string(field_type->id));
			goto invalid;
		}

		if (bt_ctf_field_type_common_integer_is_signed(field_type)) {
			BT_LOGW("Invalid packet context field type: `events_discarded` field must be an unsigned integer field type: "
				"events-discarded-ft-addr=%p", field_type);
			goto invalid;
		}
	}

	/*
	 * If there's a `timestamp_begin` field, it must be an unsigned
	 * integer field type. Also, if the trace is not a CTF writer's
	 * trace, then we cannot automatically set the mapped clock
	 * class of this field, so it must have a mapped clock class.
	 */
	field_type = bt_ctf_field_type_common_structure_borrow_field_type_by_name(
		packet_context_type, "timestamp_begin");
	if (field_type) {
		if (field_type->id != BT_CTF_FIELD_TYPE_ID_INTEGER) {
			BT_LOGW("Invalid packet context field type: `timestamp_begin` field must be an integer field type: "
				"timestamp-begin-ft-addr=%p, timestamp-begin-ft-id=%s",
				field_type,
				bt_ctf_field_type_id_string(field_type->id));
			goto invalid;
		}

		if (bt_ctf_field_type_common_integer_is_signed(field_type)) {
			BT_LOGW("Invalid packet context field type: `timestamp_begin` field must be an unsigned integer field type: "
				"timestamp-begin-ft-addr=%p", field_type);
			goto invalid;
		}

		if (check_ts_begin_end_mapped) {
			struct bt_ctf_clock_class *clock_class =
				bt_ctf_field_type_common_integer_borrow_mapped_clock_class(
					field_type);

			if (!clock_class) {
				BT_LOGW("Invalid packet context field type: `timestamp_begin` field must be mapped to a clock class: "
					"timestamp-begin-ft-addr=%p", field_type);
				goto invalid;
			}
		}
	}

	/*
	 * If there's a `timestamp_end` field, it must be an unsigned
	 * integer field type. Also, if the trace is not a CTF writer's
	 * trace, then we cannot automatically set the mapped clock
	 * class of this field, so it must have a mapped clock class.
	 */
	field_type = bt_ctf_field_type_common_structure_borrow_field_type_by_name(
		packet_context_type, "timestamp_end");
	if (field_type) {
		if (field_type->id != BT_CTF_FIELD_TYPE_ID_INTEGER) {
			BT_LOGW("Invalid packet context field type: `timestamp_end` field must be an integer field type: "
				"timestamp-end-ft-addr=%p, timestamp-end-ft-id=%s",
				field_type,
				bt_ctf_field_type_id_string(field_type->id));
			goto invalid;
		}

		if (bt_ctf_field_type_common_integer_is_signed(field_type)) {
			BT_LOGW("Invalid packet context field type: `timestamp_end` field must be an unsigned integer field type: "
				"timestamp-end-ft-addr=%p", field_type);
			goto invalid;
		}

		if (check_ts_begin_end_mapped) {
			struct bt_ctf_clock_class *clock_class =
				bt_ctf_field_type_common_integer_borrow_mapped_clock_class(
					field_type);

			if (!clock_class) {
				BT_LOGW("Invalid packet context field type: `timestamp_end` field must be mapped to a clock class: "
					"timestamp-end-ft-addr=%p", field_type);
				goto invalid;
			}
		}
	}

	goto end;

invalid:
	is_valid = false;

end:
	return is_valid;
}

static
bool event_header_field_type_is_valid(struct bt_ctf_trace_common *trace,
		struct bt_ctf_stream_class_common *stream_class,
		struct bt_ctf_field_type_common *event_header_type)
{
	bool is_valid = true;
	struct bt_ctf_field_type_common *field_type = NULL;

	/*
	 * We do not validate that the `timestamp` field exists here
	 * because CTF does not require this exact name to be mapped to
	 * a clock class.
	 */

	if (!event_header_type) {
		/*
		 * No event header field type: stream class must have
		 * only one event class.
		 */
		if (bt_ctf_stream_class_common_get_event_class_count(stream_class) > 1) {
			BT_LOGW_STR("Invalid event header field type: "
				"event header field type does not exist but there's more than one event class in the stream class.");
			goto invalid;
		}

		/* No event header field type: valid at this point */
		goto end;
	}

	/* Event header field type, if it exists, must be a structure */
	if (event_header_type->id != BT_CTF_FIELD_TYPE_ID_STRUCT) {
		BT_LOGW("Invalid event header field type: must be a structure field type if it exists: "
			"ft-addr=%p, ft-id=%s",
			event_header_type,
			bt_ctf_field_type_id_string(event_header_type->id));
		goto invalid;
	}

	/*
	 * If there's an `id` field, it must be an unsigned integer
	 * field type or an enumeration field type with an unsigned
	 * integer container field type.
	 */
	field_type = bt_ctf_field_type_common_structure_borrow_field_type_by_name(
		event_header_type, "id");
	if (field_type) {
		struct bt_ctf_field_type_common *int_ft;

		if (field_type->id == BT_CTF_FIELD_TYPE_ID_INTEGER) {
			int_ft = field_type;
		} else if (field_type->id == BT_CTF_FIELD_TYPE_ID_ENUM) {
			int_ft = bt_ctf_field_type_common_enumeration_borrow_container_field_type(
				field_type);
		} else {
			BT_LOGW("Invalid event header field type: `id` field must be an integer or enumeration field type: "
				"id-ft-addr=%p, id-ft-id=%s",
				field_type,
				bt_ctf_field_type_id_string(field_type->id));
			goto invalid;
		}

		BT_ASSERT(int_ft);
		if (bt_ctf_field_type_common_integer_is_signed(int_ft)) {
			BT_LOGW("Invalid event header field type: `id` field must be an unsigned integer or enumeration field type: "
				"id-ft-addr=%p", int_ft);
			goto invalid;
		}
	}

	goto end;

invalid:
	is_valid = false;

end:
	return is_valid;
}

static
int check_packet_header_type_has_no_clock_class(struct bt_ctf_trace_common *trace)
{
	int ret = 0;

	if (trace->packet_header_field_type) {
		struct bt_ctf_clock_class *clock_class = NULL;

		ret = bt_ctf_field_type_common_validate_single_clock_class(
			trace->packet_header_field_type,
			&clock_class);
		bt_object_put_ref(clock_class);
		if (ret || clock_class) {
			BT_LOGW("Trace's packet header field type cannot "
				"contain a field type which is mapped to "
				"a clock class: "
				"trace-addr=%p, trace-name=\"%s\", "
				"clock-class-name=\"%s\"",
				trace, bt_ctf_trace_common_get_name(trace),
				clock_class ?
					bt_ctf_clock_class_get_name(clock_class) :
					NULL);
			ret = -1;
		}
	}

	return ret;
}

BT_HIDDEN
int bt_ctf_trace_common_add_stream_class(struct bt_ctf_trace_common *trace,
		struct bt_ctf_stream_class_common *stream_class,
		bt_ctf_validation_flag_copy_field_type_func copy_field_type_func,
		struct bt_ctf_clock_class *init_expected_clock_class,
		int (*map_clock_classes_func)(struct bt_ctf_stream_class_common *stream_class,
			struct bt_ctf_field_type_common *packet_context_field_type,
			struct bt_ctf_field_type_common *event_header_field_type),
		bool check_ts_begin_end_mapped)
{
	int ret;
	int64_t i;
	int64_t stream_id;
	struct bt_ctf_validation_output trace_sc_validation_output = { 0 };
	struct bt_ctf_validation_output *ec_validation_outputs = NULL;
	const enum bt_ctf_validation_flag trace_sc_validation_flags =
		BT_CTF_VALIDATION_FLAG_TRACE |
		BT_CTF_VALIDATION_FLAG_STREAM;
	const enum bt_ctf_validation_flag ec_validation_flags =
		BT_CTF_VALIDATION_FLAG_EVENT;
	struct bt_ctf_field_type_common *packet_header_type = NULL;
	struct bt_ctf_field_type_common *packet_context_type = NULL;
	struct bt_ctf_field_type_common *event_header_type = NULL;
	struct bt_ctf_field_type_common *stream_event_ctx_type = NULL;
	int64_t event_class_count;
	struct bt_ctf_trace_common *current_parent_trace = NULL;
	struct bt_ctf_clock_class *expected_clock_class =
		bt_object_get_ref(init_expected_clock_class);

	BT_ASSERT(copy_field_type_func);

	if (!trace) {
		BT_LOGW_STR("Invalid parameter: trace is NULL.");
		ret = -1;
		goto end;
	}

	if (!stream_class) {
		BT_LOGW_STR("Invalid parameter: stream class is NULL.");
		ret = -1;
		goto end;
	}

	BT_LOGD("Adding stream class to trace: "
		"trace-addr=%p, trace-name=\"%s\", "
		"stream-class-addr=%p, stream-class-name=\"%s\", "
		"stream-class-id=%" PRId64,
		trace, bt_ctf_trace_common_get_name(trace),
		stream_class, bt_ctf_stream_class_common_get_name(stream_class),
		bt_ctf_stream_class_common_get_id(stream_class));

	current_parent_trace = bt_ctf_stream_class_common_borrow_trace(stream_class);
	if (current_parent_trace) {
		/* Stream class is already associated to a trace, abort. */
		BT_LOGW("Invalid parameter: stream class is already part of a trace: "
			"stream-class-trace-addr=%p, "
			"stream-class-trace-name=\"%s\"",
			current_parent_trace,
			bt_ctf_trace_common_get_name(current_parent_trace));
		ret = -1;
		goto end;
	}

	event_class_count =
		bt_ctf_stream_class_common_get_event_class_count(stream_class);
	BT_ASSERT(event_class_count >= 0);

	if (!stream_class->frozen) {
		/*
		 * Stream class is not frozen yet. Validate that the
		 * stream class contains at most a single clock class
		 * because the previous
		 * bt_ctf_stream_class_common_add_event_class() calls did
		 * not make this validation since the stream class's
		 * direct field types (packet context, event header,
		 * event context) could change afterwards. This stream
		 * class is about to be frozen and those field types
		 * won't be changed if this function succeeds.
		 *
		 * At this point we're also sure that the stream class's
		 * clock, if any, has the same class as the stream
		 * class's expected clock class, if any. This is why, if
		 * bt_ctf_stream_class_common_validate_single_clock_class()
		 * succeeds below, the call to
		 * bt_ctf_stream_class_map_clock_class() at the end of this
		 * function is safe because it maps to the same, single
		 * clock class.
		 */
		ret = bt_ctf_stream_class_common_validate_single_clock_class(
			stream_class, &expected_clock_class);
		if (ret) {
			BT_LOGW("Invalid parameter: stream class or one of its "
				"event classes contains a field type which is "
				"not recursively mapped to the expected "
				"clock class: "
				"stream-class-addr=%p, "
				"stream-class-id=%" PRId64 ", "
				"stream-class-name=\"%s\", "
				"expected-clock-class-addr=%p, "
				"expected-clock-class-name=\"%s\"",
				stream_class, bt_ctf_stream_class_common_get_id(stream_class),
				bt_ctf_stream_class_common_get_name(stream_class),
				expected_clock_class,
				expected_clock_class ?
					bt_ctf_clock_class_get_name(expected_clock_class) :
					NULL);
			goto end;
		}
	}

	ret = check_packet_header_type_has_no_clock_class(trace);
	if (ret) {
		/* check_packet_header_type_has_no_clock_class() logs errors */
		goto end;
	}

	/*
	 * We're about to freeze both the trace and the stream class.
	 * Also, each event class contained in this stream class are
	 * already frozen.
	 *
	 * This trace, this stream class, and all its event classes
	 * should be valid at this point.
	 *
	 * Validate trace and stream class first, then each event
	 * class of this stream class can be validated individually.
	 */
	packet_header_type =
		bt_ctf_trace_common_borrow_packet_header_field_type(trace);
	packet_context_type =
		bt_ctf_stream_class_common_borrow_packet_context_field_type(stream_class);
	event_header_type =
		bt_ctf_stream_class_common_borrow_event_header_field_type(stream_class);
	stream_event_ctx_type =
		bt_ctf_stream_class_common_borrow_event_context_field_type(stream_class);

	BT_LOGD("Validating trace and stream class field types.");
	ret = bt_ctf_validate_class_types(trace->environment,
		packet_header_type, packet_context_type, event_header_type,
		stream_event_ctx_type, NULL, NULL, trace->valid,
		stream_class->valid, 1, &trace_sc_validation_output,
		trace_sc_validation_flags, copy_field_type_func);

	if (ret) {
		/*
		 * This means something went wrong during the validation
		 * process, not that the objects are invalid.
		 */
		BT_LOGE("Failed to validate trace and stream class field types: "
			"ret=%d", ret);
		goto end;
	}

	if ((trace_sc_validation_output.valid_flags &
			trace_sc_validation_flags) !=
			trace_sc_validation_flags) {
		/* Invalid trace/stream class */
		BT_LOGW("Invalid trace or stream class field types: "
			"valid-flags=0x%x",
			trace_sc_validation_output.valid_flags);
		ret = -1;
		goto end;
	}

	if (event_class_count > 0) {
		ec_validation_outputs = g_new0(struct bt_ctf_validation_output,
			event_class_count);
		if (!ec_validation_outputs) {
			BT_LOGE_STR("Failed to allocate one validation output structure.");
			ret = -1;
			goto end;
		}
	}

	/* Validate each event class individually */
	for (i = 0; i < event_class_count; i++) {
		struct bt_ctf_event_class_common *event_class =
			bt_ctf_stream_class_common_borrow_event_class_by_index(
				stream_class, i);
		struct bt_ctf_field_type_common *event_context_type = NULL;
		struct bt_ctf_field_type_common *event_payload_type = NULL;

		event_context_type =
			bt_ctf_event_class_common_borrow_context_field_type(
				event_class);
		event_payload_type =
			bt_ctf_event_class_common_borrow_payload_field_type(
				event_class);

		/*
		 * It is important to use the field types returned by
		 * the previous trace and stream class validation here
		 * because copies could have been made.
		 */
		BT_LOGD("Validating event class's field types: "
			"addr=%p, name=\"%s\", id=%" PRId64,
			event_class, bt_ctf_event_class_common_get_name(event_class),
			bt_ctf_event_class_common_get_id(event_class));
		ret = bt_ctf_validate_class_types(trace->environment,
			trace_sc_validation_output.packet_header_type,
			trace_sc_validation_output.packet_context_type,
			trace_sc_validation_output.event_header_type,
			trace_sc_validation_output.stream_event_ctx_type,
			event_context_type, event_payload_type,
			1, 1, event_class->valid, &ec_validation_outputs[i],
			ec_validation_flags, copy_field_type_func);

		if (ret) {
			BT_LOGE("Failed to validate event class field types: "
				"ret=%d", ret);
			goto end;
		}

		if ((ec_validation_outputs[i].valid_flags &
				ec_validation_flags) != ec_validation_flags) {
			/* Invalid event class */
			BT_LOGW("Invalid event class field types: "
				"valid-flags=0x%x",
				ec_validation_outputs[i].valid_flags);
			ret = -1;
			goto end;
		}
	}

	stream_id = bt_ctf_stream_class_common_get_id(stream_class);
	if (stream_id < 0) {
		stream_id = trace->next_stream_id++;
		if (stream_id < 0) {
			BT_LOGE_STR("No more stream class IDs available.");
			ret = -1;
			goto end;
		}

		/* Try to assign a new stream id */
		for (i = 0; i < trace->stream_classes->len; i++) {
			if (stream_id == bt_ctf_stream_class_common_get_id(
				trace->stream_classes->pdata[i])) {
				/* Duplicate stream id found */
				BT_LOGW("Duplicate stream class ID: "
					"id=%" PRId64, (int64_t) stream_id);
				ret = -1;
				goto end;
			}
		}

		if (bt_ctf_stream_class_common_set_id_no_check(stream_class,
				stream_id)) {
			/* TODO Should retry with a different stream id */
			BT_LOGE("Cannot set stream class's ID: "
				"id=%" PRId64, (int64_t) stream_id);
			ret = -1;
			goto end;
		}
	}

	/*
	 * At this point all the field types in the validation output
	 * are valid. Validate the semantics of some scopes according to
	 * the CTF specification.
	 */
	if (!packet_header_field_type_is_valid(trace,
			trace_sc_validation_output.packet_header_type)) {
		BT_LOGW_STR("Invalid trace's packet header field type.");
		ret = -1;
		goto end;
	}

	if (!packet_context_field_type_is_valid(trace,
			stream_class,
			trace_sc_validation_output.packet_context_type,
			check_ts_begin_end_mapped)) {
		BT_LOGW_STR("Invalid stream class's packet context field type.");
		ret = -1;
		goto end;
	}

	if (!event_header_field_type_is_valid(trace,
			stream_class,
			trace_sc_validation_output.event_header_type)) {
		BT_LOGW_STR("Invalid steam class's event header field type.");
		ret = -1;
		goto end;
	}

	/*
	 * Now is the time to automatically map specific field types of
	 * the stream class's packet context and event header field
	 * types to the stream class's clock's class if they are not
	 * mapped to a clock class yet. We do it here because we know
	 * that after this point, everything is frozen so it won't be
	 * possible for the user to modify the stream class's clock, or
	 * to map those field types to other clock classes.
	 */
	if (map_clock_classes_func) {
		if (map_clock_classes_func(stream_class,
				trace_sc_validation_output.packet_context_type,
				trace_sc_validation_output.event_header_type)) {
			/* map_clock_classes_func() logs errors */
			ret = -1;
			goto end;
		}
	}

	bt_object_set_parent(&stream_class->base, &trace->base);
	g_ptr_array_add(trace->stream_classes, stream_class);

	/*
	 * At this point we know that the function will be successful.
	 * Therefore we can replace the trace and stream class field
	 * types with what's in their validation output structure and
	 * mark them as valid. We can also replace the field types of
	 * all the event classes of the stream class and mark them as
	 * valid.
	 */
	bt_ctf_validation_replace_types(trace, stream_class, NULL,
		&trace_sc_validation_output, trace_sc_validation_flags);
	trace->valid = 1;
	stream_class->valid = 1;

	/*
	 * Put what was not moved in bt_ctf_validation_replace_types().
	 */
	bt_ctf_validation_output_put_types(&trace_sc_validation_output);

	for (i = 0; i < event_class_count; i++) {
		struct bt_ctf_event_class_common *event_class =
			bt_ctf_stream_class_common_borrow_event_class_by_index(
				stream_class, i);

		bt_ctf_validation_replace_types(NULL, NULL, event_class,
			&ec_validation_outputs[i], ec_validation_flags);
		event_class->valid = 1;

		/*
		 * Put what was not moved in
		 * bt_ctf_validation_replace_types().
		 */
		bt_ctf_validation_output_put_types(&ec_validation_outputs[i]);
	}

	/*
	 * Freeze the trace and the stream class.
	 */
	bt_ctf_stream_class_common_freeze(stream_class);
	bt_ctf_trace_common_freeze(trace);

	/*
	 * It is safe to set the stream class's unique clock class
	 * now because the stream class is frozen.
	 */
	if (expected_clock_class) {
		BT_OBJECT_MOVE_REF(stream_class->clock_class, expected_clock_class);
	}

	BT_LOGD("Added stream class to trace: "
		"trace-addr=%p, trace-name=\"%s\", "
		"stream-class-addr=%p, stream-class-name=\"%s\", "
		"stream-class-id=%" PRId64,
		trace, bt_ctf_trace_common_get_name(trace),
		stream_class, bt_ctf_stream_class_common_get_name(stream_class),
		bt_ctf_stream_class_common_get_id(stream_class));

end:
	if (ret) {
		bt_object_set_parent(&stream_class->base, NULL);

		if (ec_validation_outputs) {
			for (i = 0; i < event_class_count; i++) {
				bt_ctf_validation_output_put_types(
					&ec_validation_outputs[i]);
			}
		}
	}

	g_free(ec_validation_outputs);
	bt_ctf_validation_output_put_types(&trace_sc_validation_output);
	bt_object_put_ref(expected_clock_class);
	return ret;
}

BT_HIDDEN
bt_bool bt_ctf_trace_common_has_clock_class(struct bt_ctf_trace_common *trace,
		struct bt_ctf_clock_class *clock_class)
{
	struct bt_ctf_search_query query = { .value = clock_class, .found = 0 };

	BT_ASSERT(trace);
	BT_ASSERT(clock_class);

	g_ptr_array_foreach(trace->clock_classes, value_exists, &query);
	return query.found;
}

BT_HIDDEN
int bt_ctf_trace_common_set_native_byte_order(struct bt_ctf_trace_common *trace,
		enum bt_ctf_byte_order byte_order, bool allow_unspecified)
{
	int ret = 0;

	if (!trace) {
		BT_LOGW_STR("Invalid parameter: trace is NULL.");
		ret = -1;
		goto end;
	}

	if (trace->frozen) {
		BT_LOGW("Invalid parameter: trace is frozen: "
			"addr=%p, name=\"%s\"",
			trace, bt_ctf_trace_common_get_name(trace));
		ret = -1;
		goto end;
	}

	if (byte_order == BT_CTF_BYTE_ORDER_UNSPECIFIED && !allow_unspecified) {
		BT_LOGW("Invalid parameter: BT_CTF_BYTE_ORDER_UNSPECIFIED byte order is not allowed: "
			"addr=%p, name=\"%s\"",
			trace, bt_ctf_trace_common_get_name(trace));
		ret = -1;
		goto end;
	}

	if (byte_order != BT_CTF_BYTE_ORDER_LITTLE_ENDIAN &&
			byte_order != BT_CTF_BYTE_ORDER_BIG_ENDIAN &&
			byte_order != BT_CTF_BYTE_ORDER_NETWORK) {
		BT_LOGW("Invalid parameter: invalid byte order: "
			"addr=%p, name=\"%s\", bo=%s",
			trace, bt_ctf_trace_common_get_name(trace),
			bt_ctf_byte_order_string(byte_order));
		ret = -1;
		goto end;
	}

	trace->native_byte_order = byte_order;
	BT_LOGV("Set trace's native byte order: "
		"addr=%p, name=\"%s\", bo=%s",
		trace, bt_ctf_trace_common_get_name(trace),
		bt_ctf_byte_order_string(byte_order));

end:
	return ret;
}

BT_HIDDEN
int bt_ctf_trace_common_set_packet_header_field_type(struct bt_ctf_trace_common *trace,
		struct bt_ctf_field_type_common *packet_header_type)
{
	int ret = 0;

	if (!trace) {
		BT_LOGW_STR("Invalid parameter: trace is NULL.");
		ret = -1;
		goto end;
	}

	if (trace->frozen) {
		BT_LOGW("Invalid parameter: trace is frozen: "
			"addr=%p, name=\"%s\"",
			trace, bt_ctf_trace_common_get_name(trace));
		ret = -1;
		goto end;
	}

	/* packet_header_type must be a structure. */
	if (packet_header_type &&
			packet_header_type->id != BT_CTF_FIELD_TYPE_ID_STRUCT) {
		BT_LOGW("Invalid parameter: packet header field type must be a structure field type if it exists: "
			"addr=%p, name=\"%s\", ft-addr=%p, ft-id=%s",
			trace, bt_ctf_trace_common_get_name(trace),
			packet_header_type,
			bt_ctf_field_type_id_string(packet_header_type->id));
		ret = -1;
		goto end;
	}

	bt_object_put_ref(trace->packet_header_field_type);
	trace->packet_header_field_type = bt_object_get_ref(packet_header_type);
	BT_LOGV("Set trace's packet header field type: "
		"addr=%p, name=\"%s\", packet-context-ft-addr=%p",
		trace, bt_ctf_trace_common_get_name(trace), packet_header_type);
end:
	return ret;
}

static
int64_t get_stream_class_count(void *element)
{
	return bt_ctf_trace_get_stream_class_count(
			(struct bt_ctf_trace *) element);
}

static
void *get_stream_class(void *element, int i)
{
	return bt_ctf_trace_get_stream_class_by_index(
			(struct bt_ctf_trace *) element, i);
}

static
int visit_stream_class(void *object, bt_ctf_visitor visitor,void *data)
{
	return bt_ctf_stream_class_visit(object, visitor, data);
}

int bt_ctf_trace_visit(struct bt_ctf_trace *trace,
		bt_ctf_visitor visitor, void *data)
{
	int ret;
	struct bt_ctf_visitor_object obj = {
		.object = trace,
		.type = BT_CTF_VISITOR_OBJECT_TYPE_TRACE
	};

	if (!trace) {
		BT_LOGW_STR("Invalid parameter: trace is NULL.");
		ret = -1;
		goto end;
	}

	if (!visitor) {
		BT_LOGW_STR("Invalid parameter: visitor is NULL.");
		ret = -1;
		goto end;
	}

	BT_LOGV("Visiting trace: addr=%p, name=\"%s\"",
		trace, bt_ctf_trace_get_name(trace));
	ret = bt_ctf_visitor_helper(&obj, get_stream_class_count,
			get_stream_class, visit_stream_class, visitor, data);
end:
	return ret;
}

static
void bt_ctf_trace_destroy(struct bt_object *obj)
{
	struct bt_ctf_trace *trace = (void *) obj;

	BT_LOGD("Destroying CTF writer trace object: addr=%p, name=\"%s\"",
		trace, bt_ctf_trace_get_name(trace));
	bt_ctf_trace_common_finalize(BT_CTF_TO_COMMON(trace));
	g_free(trace);
}

BT_HIDDEN
struct bt_ctf_trace *bt_ctf_trace_create(void)
{
	struct bt_ctf_trace *trace = NULL;
	int ret;

	BT_LOGD_STR("Creating CTF writer trace object.");
	trace = g_new0(struct bt_ctf_trace, 1);
	if (!trace) {
		BT_LOGE_STR("Failed to allocate one CTF writer trace.");
		goto error;
	}

	ret = bt_ctf_trace_common_initialize(BT_CTF_TO_COMMON(trace),
		bt_ctf_trace_destroy);
	if (ret) {
		/* bt_ctf_trace_common_initialize() logs errors */
		goto error;
	}

	BT_LOGD("Created CTF writer trace object: addr=%p", trace);
	return trace;

error:
	BT_OBJECT_PUT_REF_AND_RESET(trace);
	return trace;
}

const unsigned char *bt_ctf_trace_get_uuid(struct bt_ctf_trace *trace)
{
	return bt_ctf_trace_common_get_uuid(BT_CTF_TO_COMMON(trace));
}

int bt_ctf_trace_set_uuid(struct bt_ctf_trace *trace,
		const unsigned char *uuid)
{
	return bt_ctf_trace_common_set_uuid(BT_CTF_TO_COMMON(trace), uuid);
}

int bt_ctf_trace_set_environment_field_string(struct bt_ctf_trace *trace,
		const char *name, const char *value)
{
	return bt_ctf_trace_common_set_environment_field_string(BT_CTF_TO_COMMON(trace),
		name, value);
}

int bt_ctf_trace_set_environment_field_integer(
		struct bt_ctf_trace *trace, const char *name, int64_t value)
{
	return bt_ctf_trace_common_set_environment_field_integer(
		BT_CTF_TO_COMMON(trace), name, value);
}

int64_t bt_ctf_trace_get_environment_field_count(struct bt_ctf_trace *trace)
{
	return bt_ctf_trace_common_get_environment_field_count(BT_CTF_TO_COMMON(trace));
}

const char *
bt_ctf_trace_get_environment_field_name_by_index(struct bt_ctf_trace *trace,
		uint64_t index)
{
	return bt_ctf_trace_common_get_environment_field_name_by_index(
		BT_CTF_TO_COMMON(trace), index);
}

struct bt_value *bt_ctf_trace_get_environment_field_value_by_index(
		struct bt_ctf_trace *trace, uint64_t index)
{
	return bt_object_get_ref(bt_ctf_trace_common_borrow_environment_field_value_by_index(
		BT_CTF_TO_COMMON(trace), index));
}

struct bt_value *bt_ctf_trace_get_environment_field_value_by_name(
		struct bt_ctf_trace *trace, const char *name)
{
	return bt_object_get_ref(bt_ctf_trace_common_borrow_environment_field_value_by_name(
		BT_CTF_TO_COMMON(trace), name));
}

BT_HIDDEN
int bt_ctf_trace_add_clock_class(struct bt_ctf_trace *trace,
		struct bt_ctf_clock_class *clock_class)
{
	return bt_ctf_trace_common_add_clock_class(BT_CTF_TO_COMMON(trace),
		(void *) clock_class);
}

BT_HIDDEN
int64_t bt_ctf_trace_get_clock_class_count(struct bt_ctf_trace *trace)
{
	return bt_ctf_trace_common_get_clock_class_count(BT_CTF_TO_COMMON(trace));
}

BT_HIDDEN
struct bt_ctf_clock_class *bt_ctf_trace_get_clock_class_by_index(
		struct bt_ctf_trace *trace, uint64_t index)
{
	return bt_object_get_ref(bt_ctf_trace_common_borrow_clock_class_by_index(
		BT_CTF_TO_COMMON(trace), index));
}

static
int map_clock_classes_func(struct bt_ctf_stream_class_common *stream_class,
		struct bt_ctf_field_type_common *packet_context_type,
		struct bt_ctf_field_type_common *event_header_type)
{
	int ret = bt_ctf_stream_class_map_clock_class(
		BT_CTF_FROM_COMMON(stream_class),
		BT_CTF_FROM_COMMON(packet_context_type),
		BT_CTF_FROM_COMMON(event_header_type));

	if (ret) {
		BT_LOGW_STR("Cannot automatically map selected stream class's field types to stream class's clock's class.");
	}

	return ret;
}

int bt_ctf_trace_add_stream_class(struct bt_ctf_trace *trace,
		struct bt_ctf_stream_class *stream_class)
{
	int ret = 0;
	struct bt_ctf_clock_class *expected_clock_class = NULL;

	if (!trace) {
		BT_LOGW_STR("Invalid parameter: trace is NULL.");
		ret = -1;
		goto end;
	}

	if (!stream_class) {
		BT_LOGW_STR("Invalid parameter: stream class is NULL.");
		ret = -1;
		goto end;
	}

	if (stream_class->clock) {
		struct bt_ctf_clock_class *stream_clock_class =
			stream_class->clock->clock_class;

		/*
		 * Make sure this clock was also added to the
		 * trace (potentially through its CTF writer
		 * owner).
		 */
		size_t i;

		for (i = 0; i < trace->common.clock_classes->len; i++) {
			if (trace->common.clock_classes->pdata[i] ==
					stream_clock_class) {
				/* Found! */
				break;
			}
		}

		if (i == trace->common.clock_classes->len) {
			/* Not found */
			BT_LOGW("Stream class's clock's class is not part of the trace: "
				"clock-class-addr=%p, clock-class-name=\"%s\"",
				stream_clock_class,
				bt_ctf_clock_class_get_name(stream_clock_class));
			ret = -1;
			goto end;
		}

		if (stream_class->common.clock_class &&
				stream_class->common.clock_class !=
				stream_class->clock->clock_class) {
			/*
			 * Stream class already has an expected clock
			 * class, but it does not match its clock's
			 * class.
			 */
			BT_LOGW("Invalid parameter: stream class's clock's "
				"class does not match stream class's "
				"expected clock class: "
				"stream-class-addr=%p, "
				"stream-class-id=%" PRId64 ", "
				"stream-class-name=\"%s\", "
				"expected-clock-class-addr=%p, "
				"expected-clock-class-name=\"%s\"",
				stream_class,
				bt_ctf_stream_class_get_id(stream_class),
				bt_ctf_stream_class_get_name(stream_class),
				stream_class->common.clock_class,
				bt_ctf_clock_class_get_name(stream_class->common.clock_class));
		} else if (!stream_class->common.clock_class) {
			/*
			 * Set expected clock class to stream class's
			 * clock's class.
			 */
			expected_clock_class = stream_class->clock->clock_class;
		}
	}


	ret = bt_ctf_trace_common_add_stream_class(BT_CTF_TO_COMMON(trace),
		BT_CTF_TO_COMMON(stream_class),
		(bt_ctf_validation_flag_copy_field_type_func) bt_ctf_field_type_copy,
		expected_clock_class, map_clock_classes_func,
		false);

end:
	return ret;
}

int64_t bt_ctf_trace_get_stream_count(struct bt_ctf_trace *trace)
{
	return bt_ctf_trace_common_get_stream_count(BT_CTF_TO_COMMON(trace));
}

struct bt_ctf_stream *bt_ctf_trace_get_stream_by_index(
		struct bt_ctf_trace *trace, uint64_t index)
{
	return bt_object_get_ref(bt_ctf_trace_common_borrow_stream_by_index(
		BT_CTF_TO_COMMON(trace), index));
}

int64_t bt_ctf_trace_get_stream_class_count(struct bt_ctf_trace *trace)
{
	return bt_ctf_trace_common_get_stream_class_count(BT_CTF_TO_COMMON(trace));
}

struct bt_ctf_stream_class *bt_ctf_trace_get_stream_class_by_index(
		struct bt_ctf_trace *trace, uint64_t index)
{
	return bt_object_get_ref(bt_ctf_trace_common_borrow_stream_class_by_index(
		BT_CTF_TO_COMMON(trace), index));
}

struct bt_ctf_stream_class *bt_ctf_trace_get_stream_class_by_id(
		struct bt_ctf_trace *trace, uint64_t id)
{
	return bt_object_get_ref(bt_ctf_trace_common_borrow_stream_class_by_id(
		BT_CTF_TO_COMMON(trace), id));
}

BT_HIDDEN
struct bt_ctf_clock_class *bt_ctf_trace_get_clock_class_by_name(
		struct bt_ctf_trace *trace, const char *name)
{
	return bt_object_get_ref(
		bt_ctf_trace_common_borrow_clock_class_by_name(BT_CTF_TO_COMMON(trace),
			name));
}

static
int append_trace_metadata(struct bt_ctf_trace *trace,
		struct metadata_context *context)
{
	unsigned char *uuid = trace->common.uuid;
	int ret = 0;

	if (trace->common.native_byte_order == BT_CTF_BYTE_ORDER_NATIVE ||
			trace->common.native_byte_order == BT_CTF_BYTE_ORDER_UNSPECIFIED) {
		BT_LOGW("Invalid parameter: trace's byte order cannot be BT_CTF_BYTE_ORDER_NATIVE or BT_CTF_BYTE_ORDER_UNSPECIFIED at this point; "
			"set it with bt_ctf_trace_set_native_byte_order(): "
			"addr=%p, name=\"%s\"",
			trace, bt_ctf_trace_get_name(trace));
		ret = -1;
		goto end;
	}

	g_string_append(context->string, "trace {\n");
	g_string_append(context->string, "\tmajor = 1;\n");
	g_string_append(context->string, "\tminor = 8;\n");
	BT_ASSERT(trace->common.native_byte_order == BT_CTF_BYTE_ORDER_LITTLE_ENDIAN ||
		trace->common.native_byte_order == BT_CTF_BYTE_ORDER_BIG_ENDIAN ||
		trace->common.native_byte_order == BT_CTF_BYTE_ORDER_NETWORK);

	if (trace->common.uuid_set) {
		g_string_append_printf(context->string,
			"\tuuid = \"%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x\";\n",
			uuid[0], uuid[1], uuid[2], uuid[3],
			uuid[4], uuid[5], uuid[6], uuid[7],
			uuid[8], uuid[9], uuid[10], uuid[11],
			uuid[12], uuid[13], uuid[14], uuid[15]);
	}

	g_string_append_printf(context->string, "\tbyte_order = %s;\n",
		bt_ctf_get_byte_order_string(trace->common.native_byte_order));

	if (trace->common.packet_header_field_type) {
		g_string_append(context->string, "\tpacket.header := ");
		context->current_indentation_level++;
		g_string_assign(context->field_name, "");
		BT_LOGD_STR("Serializing trace's packet header field type's metadata.");
		ret = bt_ctf_field_type_serialize_recursive(
			(void *) trace->common.packet_header_field_type,
			context);
		if (ret) {
			goto end;
		}
		context->current_indentation_level--;
	}

	g_string_append(context->string, ";\n};\n\n");
end:
	return ret;
}

static
void append_env_metadata(struct bt_ctf_trace *trace,
		struct metadata_context *context)
{
	int64_t i;
	int64_t env_size;

	env_size = bt_ctf_attributes_get_count(trace->common.environment);
	if (env_size <= 0) {
		return;
	}

	g_string_append(context->string, "env {\n");

	for (i = 0; i < env_size; i++) {
		struct bt_private_value *env_field_value_obj = NULL;
		const char *entry_name;

		entry_name = bt_ctf_attributes_get_field_name(
			trace->common.environment, i);
		env_field_value_obj = bt_ctf_attributes_borrow_field_value(
			trace->common.environment, i);

		BT_ASSERT(entry_name);
		BT_ASSERT(env_field_value_obj);

		switch (bt_value_get_type(
			bt_private_value_borrow_value(env_field_value_obj))) {
		case BT_VALUE_TYPE_INTEGER:
		{
			int64_t int_value;

			int_value = bt_value_integer_get(
				bt_private_value_borrow_value(
					env_field_value_obj));
			g_string_append_printf(context->string,
				"\t%s = %" PRId64 ";\n", entry_name,
				int_value);
			break;
		}
		case BT_VALUE_TYPE_STRING:
		{
			const char *str_value;
			char *escaped_str = NULL;

			str_value = bt_value_string_get(
				bt_private_value_borrow_value(
					env_field_value_obj));
			escaped_str = g_strescape(str_value, NULL);
			if (!escaped_str) {
				BT_LOGE("Cannot escape string: string=\"%s\"",
					str_value);
				continue;
			}

			g_string_append_printf(context->string,
				"\t%s = \"%s\";\n", entry_name, escaped_str);
			free(escaped_str);
			break;
		}
		default:
			continue;
		}
	}

	g_string_append(context->string, "};\n\n");
}

char *bt_ctf_trace_get_metadata_string(struct bt_ctf_trace *trace)
{
	char *metadata = NULL;
	struct metadata_context *context = NULL;
	int err = 0;
	size_t i;

	if (!trace) {
		BT_LOGW_STR("Invalid parameter: trace is NULL.");
		goto end;
	}

	context = g_new0(struct metadata_context, 1);
	if (!context) {
		BT_LOGE_STR("Failed to allocate one metadata context.");
		goto end;
	}

	context->field_name = g_string_sized_new(DEFAULT_IDENTIFIER_SIZE);
	context->string = g_string_sized_new(DEFAULT_METADATA_STRING_SIZE);
	g_string_append(context->string, "/* CTF 1.8 */\n\n");
	if (append_trace_metadata(trace, context)) {
		/* append_trace_metadata() logs errors */
		goto error;
	}
	append_env_metadata(trace, context);
	g_ptr_array_foreach(trace->common.clock_classes,
		(GFunc) bt_ctf_clock_class_serialize, context);

	for (i = 0; i < trace->common.stream_classes->len; i++) {
		/* bt_ctf_stream_class_serialize() logs details */
		err = bt_ctf_stream_class_serialize(
			trace->common.stream_classes->pdata[i], context);
		if (err) {
			/* bt_ctf_stream_class_serialize() logs errors */
			goto error;
		}
	}

	metadata = context->string->str;

error:
	g_string_free(context->string, err ? TRUE : FALSE);
	g_string_free(context->field_name, TRUE);
	g_free(context);

end:
	return metadata;
}

enum bt_ctf_byte_order bt_ctf_trace_get_native_byte_order(
		struct bt_ctf_trace *trace)
{
	return (int) bt_ctf_trace_common_get_native_byte_order(BT_CTF_TO_COMMON(trace));
}

int bt_ctf_trace_set_native_byte_order(struct bt_ctf_trace *trace,
		enum bt_ctf_byte_order byte_order)
{
	return bt_ctf_trace_common_set_native_byte_order(BT_CTF_TO_COMMON(trace),
		(int) byte_order, false);
}

struct bt_ctf_field_type *bt_ctf_trace_get_packet_header_field_type(
		struct bt_ctf_trace *trace)
{
	return bt_object_get_ref(bt_ctf_trace_common_borrow_packet_header_field_type(
		BT_CTF_TO_COMMON(trace)));
}

int bt_ctf_trace_set_packet_header_field_type(struct bt_ctf_trace *trace,
		struct bt_ctf_field_type *packet_header_type)
{
	return bt_ctf_trace_common_set_packet_header_field_type(BT_CTF_TO_COMMON(trace),
		(void *) packet_header_type);
}

const char *bt_ctf_trace_get_name(struct bt_ctf_trace *trace)
{
	return bt_ctf_trace_common_get_name(BT_CTF_TO_COMMON(trace));
}
