/*
 * stream.c
 *
 * Babeltrace CTF IR - Stream
 *
 * Copyright 2013, 2014 Jérémie Galarneau <jeremie.galarneau@efficios.com>
 *
 * Author: Jérémie Galarneau <jeremie.galarneau@efficios.com>
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

#define BT_LOG_TAG "STREAM"
#include <babeltrace/lib-logging-internal.h>

#include <babeltrace/ctf-ir/clock-class.h>
#include <babeltrace/ctf-writer/clock.h>
#include <babeltrace/ctf-writer/clock-internal.h>
#include <babeltrace/ctf-writer/event.h>
#include <babeltrace/ctf-ir/event-internal.h>
#include <babeltrace/ctf-ir/field-types-internal.h>
#include <babeltrace/ctf-ir/fields-internal.h>
#include <babeltrace/ctf-ir/stream.h>
#include <babeltrace/ctf-ir/stream-internal.h>
#include <babeltrace/ctf-ir/stream-class-internal.h>
#include <babeltrace/ctf-ir/trace.h>
#include <babeltrace/ctf-ir/trace-internal.h>
#include <babeltrace/ctf-writer/writer-internal.h>
#include <babeltrace/graph/component-internal.h>
#include <babeltrace/ref.h>
#include <babeltrace/ctf-writer/functor-internal.h>
#include <babeltrace/compiler-internal.h>
#include <babeltrace/align-internal.h>
#include <inttypes.h>
#include <unistd.h>

static
void bt_stream_destroy(struct bt_object *obj);
static
int try_set_structure_field_integer(struct bt_field *, char *, uint64_t);

static
int set_integer_field_value(struct bt_field* field, uint64_t value)
{
	int ret = 0;
	struct bt_field_type *field_type = NULL;

	if (!field) {
		BT_LOGW_STR("Invalid parameter: field is NULL.");
		ret = -1;
		goto end;
	}

	field_type = bt_field_get_type(field);
	assert(field_type);

	if (bt_field_type_get_type_id(field_type) !=
			BT_FIELD_TYPE_ID_INTEGER) {
		/* Not an integer and the value is unset, error. */
		BT_LOGW("Invalid parameter: field's type is not an integer field type: "
			"field-addr=%p, ft-addr=%p, ft-id=%s",
			field, field_type,
			bt_field_type_id_string(field_type->id));
		ret = -1;
		goto end;
	}

	if (bt_field_type_integer_is_signed(field_type)) {
		ret = bt_field_signed_integer_set_value(field, (int64_t) value);
		if (ret) {
			/* Value is out of range, error. */
			BT_LOGW("Cannot set signed integer field's value: "
				"addr=%p, value=%" PRId64,
				field, (int64_t) value);
			goto end;
		}
	} else {
		ret = bt_field_unsigned_integer_set_value(field, value);
		if (ret) {
			/* Value is out of range, error. */
			BT_LOGW("Cannot set unsigned integer field's value: "
				"addr=%p, value=%" PRIu64,
				field, value);
			goto end;
		}
	}
end:
	bt_put(field_type);
	return ret;
}

static
int set_packet_header_magic(struct bt_stream *stream)
{
	int ret = 0;
	struct bt_field *magic_field = bt_field_structure_get_field_by_name(
		stream->packet_header, "magic");
	const uint32_t magic_value = 0xc1fc1fc1;

	assert(stream);

	if (!magic_field) {
		/* No magic field found. Not an error, skip. */
		BT_LOGV("No field named `magic` in packet header: skipping: "
			"stream-addr=%p, stream-name=\"%s\"",
			stream, bt_stream_get_name(stream));
		goto end;
	}

	ret = bt_field_unsigned_integer_set_value(magic_field,
		(uint64_t) magic_value);

	if (ret) {
		BT_LOGW("Cannot set packet header field's `magic` integer field's value: "
			"stream-addr=%p, stream-name=\"%s\", field-addr=%p, value=%" PRIu64,
			stream, bt_stream_get_name(stream),
			magic_field, (uint64_t) magic_value);
	} else {
		BT_LOGV("Set packet header field's `magic` field's value: "
			"stream-addr=%p, stream-name=\"%s\", field-addr=%p, value=%" PRIu64,
			stream, bt_stream_get_name(stream),
			magic_field, (uint64_t) magic_value);
	}
end:
	bt_put(magic_field);
	return ret;
}

static
int set_packet_header_uuid(struct bt_stream *stream)
{
	int ret = 0;
	int64_t i;
	struct bt_trace *trace = NULL;
	struct bt_field *uuid_field = bt_field_structure_get_field_by_name(
		stream->packet_header, "uuid");

	assert(stream);

	if (!uuid_field) {
		/* No uuid field found. Not an error, skip. */
		BT_LOGV("No field named `uuid` in packet header: skipping: "
			"stream-addr=%p, stream-name=\"%s\"",
			stream, bt_stream_get_name(stream));
		goto end;
	}

	trace = (struct bt_trace *) bt_object_get_parent(stream);
	for (i = 0; i < 16; i++) {
		struct bt_field *uuid_element =
			bt_field_array_get_field(uuid_field, i);

		ret = bt_field_unsigned_integer_set_value(
			uuid_element, (uint64_t) trace->uuid[i]);
		bt_put(uuid_element);
		if (ret) {
			BT_LOGW("Cannot set integer field's value (for `uuid` packet header field): "
				"stream-addr=%p, stream-name=\"%s\", field-addr=%p, "
				"value=%" PRIu64 ", index=%" PRId64,
				stream, bt_stream_get_name(stream),
				uuid_element, (uint64_t) trace->uuid[i], i);
			goto end;
		}
	}

	BT_LOGV("Set packet header field's `uuid` field's value: "
		"stream-addr=%p, stream-name=\"%s\", field-addr=%p",
		stream, bt_stream_get_name(stream), uuid_field);

end:
	bt_put(uuid_field);
	BT_PUT(trace);
	return ret;
}
static
int set_packet_header_stream_id(struct bt_stream *stream)
{
	int ret = 0;
	uint32_t stream_id;
	struct bt_field *stream_id_field = bt_field_structure_get_field_by_name(
		stream->packet_header, "stream_id");

	if (!stream_id_field) {
		/* No stream_id field found. Not an error, skip. */
		BT_LOGV("No field named `stream_id` in packet header: skipping: "
			"stream-addr=%p, stream-name=\"%s\"",
			stream, bt_stream_get_name(stream));
		goto end;
	}

	stream_id = stream->stream_class->id;
	ret = bt_field_unsigned_integer_set_value(stream_id_field,
		(uint64_t) stream_id);
	if (ret) {
		BT_LOGW("Cannot set packet header field's `stream_id` integer field's value: "
			"stream-addr=%p, stream-name=\"%s\", field-addr=%p, value=%" PRIu64,
			stream, bt_stream_get_name(stream),
			stream_id_field, (uint64_t) stream_id);
	} else {
		BT_LOGV("Set packet header field's `stream_id` field's value: "
			"stream-addr=%p, stream-name=\"%s\", field-addr=%p, value=%" PRIu64,
			stream, bt_stream_get_name(stream),
			stream_id_field, (uint64_t) stream_id);
	}

end:
	bt_put(stream_id_field);
	return ret;
}

static
int auto_populate_packet_header(struct bt_stream *stream)
{
	int ret = 0;

	if (!stream->packet_header) {
		goto end;
	}

	ret = set_packet_header_magic(stream);
	if (ret) {
		BT_LOGW("Cannot set packet header's magic number field: "
			"stream-addr=%p, stream-name=\"%s\"",
			stream, bt_stream_get_name(stream));
		goto end;
	}

	ret = set_packet_header_uuid(stream);
	if (ret) {
		BT_LOGW("Cannot set packet header's UUID field: "
			"stream-addr=%p, stream-name=\"%s\"",
			stream, bt_stream_get_name(stream));
		goto end;
	}

	ret = set_packet_header_stream_id(stream);
	if (ret) {
		BT_LOGW("Cannot set packet header's stream class ID field: "
			"stream-addr=%p, stream-name=\"%s\"",
			stream, bt_stream_get_name(stream));
		goto end;
	}

	BT_LOGV("Automatically populated stream's packet header's known fields: "
		"stream-addr=%p, stream-name=\"%s\"",
		stream, bt_stream_get_name(stream));

end:
	return ret;
}

static
int set_packet_context_packet_size(struct bt_stream *stream)
{
	int ret = 0;
	struct bt_field *field = bt_field_structure_get_field_by_name(
		stream->packet_context, "packet_size");

	assert(stream);

	if (!field) {
		/* No packet size field found. Not an error, skip. */
		BT_LOGV("No field named `packet_size` in packet context: skipping: "
			"stream-addr=%p, stream-name=\"%s\"",
			stream, bt_stream_get_name(stream));
		goto end;
	}

	ret = bt_field_unsigned_integer_set_value(field,
		stream->pos.packet_size);
	if (ret) {
		BT_LOGW("Cannot set packet context field's `packet_size` integer field's value: "
			"stream-addr=%p, stream-name=\"%s\", field-addr=%p, value=%" PRIu64,
			stream, bt_stream_get_name(stream),
			field, stream->pos.packet_size);
	} else {
		BT_LOGV("Set packet context field's `packet_size` field's value: "
			"stream-addr=%p, stream-name=\"%s\", field-addr=%p, value=%" PRIu64,
			stream, bt_stream_get_name(stream),
			field, stream->pos.packet_size);
	}

end:
	bt_put(field);
	return ret;
}

static
int set_packet_context_content_size(struct bt_stream *stream)
{
	int ret = 0;
	struct bt_field *field = bt_field_structure_get_field_by_name(
		stream->packet_context, "content_size");

	assert(stream);

	if (!field) {
		/* No content size field found. Not an error, skip. */
		BT_LOGV("No field named `content_size` in packet context: skipping: "
			"stream-addr=%p, stream-name=\"%s\"",
			stream, bt_stream_get_name(stream));
		goto end;
	}

	ret = bt_field_unsigned_integer_set_value(field,
		stream->pos.offset);
	if (ret) {
		BT_LOGW("Cannot set packet context field's `content_size` integer field's value: "
			"stream-addr=%p, stream-name=\"%s\", field-addr=%p, value=%" PRId64,
			stream, bt_stream_get_name(stream),
			field, stream->pos.offset);
	} else {
		BT_LOGV("Set packet context field's `content_size` field's value: "
			"stream-addr=%p, stream-name=\"%s\", field-addr=%p, value=%" PRId64,
			stream, bt_stream_get_name(stream),
			field, stream->pos.offset);
	}

end:
	bt_put(field);
	return ret;
}

static
int set_packet_context_events_discarded(struct bt_stream *stream)
{
	int ret = 0;
	struct bt_field *field = bt_field_structure_get_field_by_name(
		stream->packet_context, "events_discarded");

	assert(stream);

	if (!field) {
		/* No discarded events count field found. Not an error, skip. */
		BT_LOGV("No field named `events_discarded` in packet context: skipping: "
			"stream-addr=%p, stream-name=\"%s\"",
			stream, bt_stream_get_name(stream));
		goto end;
	}

	/*
	 * If the field is set by the user, make sure that the value is
	 * greater than or equal to the stream's current count of
	 * discarded events. We do not allow wrapping here. If it's
	 * valid, update the stream's current count.
	 */
	if (bt_field_is_set(field)) {
		uint64_t user_val;

		ret = bt_field_unsigned_integer_get_value(field,
			&user_val);
		if (ret) {
			BT_LOGW("Cannot get packet context `events_discarded` field's unsigned value: "
				"stream-addr=%p, stream-name=\"%s\", field-addr=%p",
				stream, bt_stream_get_name(stream), field);
			goto end;
		}

		if (user_val < stream->discarded_events) {
			BT_LOGW("Invalid packet context `events_discarded` field's unsigned value: "
				"value is lesser than the stream's current discarded events count: "
				"stream-addr=%p, stream-name=\"%s\", field-addr=%p, "
				"value=%" PRIu64 ", "
				"stream-discarded-events-count=%" PRIu64,
				stream, bt_stream_get_name(stream), field,
				user_val, stream->discarded_events);
			goto end;
		}

		stream->discarded_events = user_val;
	} else {
		ret = bt_field_unsigned_integer_set_value(field,
			stream->discarded_events);
		if (ret) {
			BT_LOGW("Cannot set packet context field's `events_discarded` integer field's value: "
				"stream-addr=%p, stream-name=\"%s\", field-addr=%p, value=%" PRIu64,
				stream, bt_stream_get_name(stream),
				field, stream->discarded_events);
		} else {
			BT_LOGV("Set packet context field's `events_discarded` field's value: "
				"stream-addr=%p, stream-name=\"%s\", field-addr=%p, value=%" PRIu64,
				stream, bt_stream_get_name(stream),
				field, stream->discarded_events);
		}
	}

end:
	bt_put(field);
	return ret;
}

static
void update_clock_value(uint64_t *val, uint64_t new_val,
		unsigned int new_val_size)
{
	const uint64_t pow2 = 1ULL << new_val_size;
	const uint64_t mask = pow2 - 1;
	uint64_t val_masked;

#ifdef BT_LOG_ENABLED_VERBOSE
	uint64_t old_val = *val;
#endif

	if (new_val_size == 64) {
		*val = new_val;
		goto end;
	}

	val_masked = *val & mask;

	if (new_val < val_masked) {
		/* Wrapped once */
		new_val |= pow2;
	}

	*val &= ~mask;
	*val |= new_val;

end:
	BT_LOGV("Updated clock value: old-val=%" PRIu64 ", new-val=%" PRIu64,
		old_val, *val);
	return;
}

static
int visit_field_update_clock_value(struct bt_field *field, uint64_t *val)
{
	int ret = 0;

	if (!field) {
		goto end;
	}

	switch (bt_field_get_type_id(field)) {
	case BT_FIELD_TYPE_ID_INTEGER:
	{
		struct bt_clock_class *cc =
			bt_field_type_integer_get_mapped_clock_class(
				field->type);
		int val_size;
		uint64_t uval;

		if (!cc) {
			goto end;
		}

		bt_put(cc);
		val_size = bt_field_type_integer_get_size(field->type);
		assert(val_size >= 1);

		if (bt_field_type_integer_is_signed(field->type)) {
			int64_t ival;

			ret = bt_field_signed_integer_get_value(field, &ival);
			uval = (uint64_t) ival;
		} else {
			ret = bt_field_unsigned_integer_get_value(field, &uval);
		}

		if (ret) {
			/* Not set */
			goto end;
		}

		update_clock_value(val, uval, val_size);
		break;
	}
	case BT_FIELD_TYPE_ID_ENUM:
	{
		struct bt_field *int_field =
			bt_field_enumeration_get_container(field);

		assert(int_field);
		ret = visit_field_update_clock_value(int_field, val);
		bt_put(int_field);
		break;
	}
	case BT_FIELD_TYPE_ID_ARRAY:
	{
		uint64_t i;
		int64_t len = bt_field_type_array_get_length(field->type);

		assert(len >= 0);

		for (i = 0; i < len; i++) {
			struct bt_field *elem_field =
				bt_field_array_get_field(field, i);

			assert(elem_field);
			ret = visit_field_update_clock_value(elem_field, val);
			bt_put(elem_field);
			if (ret) {
				goto end;
			}
		}
		break;
	}
	case BT_FIELD_TYPE_ID_SEQUENCE:
	{
		uint64_t i;
		int64_t len = bt_field_sequence_get_int_length(field);

		if (len < 0) {
			ret = -1;
			goto end;
		}

		for (i = 0; i < len; i++) {
			struct bt_field *elem_field =
				bt_field_sequence_get_field(field, i);

			assert(elem_field);
			ret = visit_field_update_clock_value(elem_field, val);
			bt_put(elem_field);
			if (ret) {
				goto end;
			}
		}
		break;
	}
	case BT_FIELD_TYPE_ID_STRUCT:
	{
		uint64_t i;
		int64_t len = bt_field_type_structure_get_field_count(
			field->type);

		assert(len >= 0);

		for (i = 0; i < len; i++) {
			struct bt_field *member_field =
				bt_field_structure_get_field_by_index(field, i);

			assert(member_field);
			ret = visit_field_update_clock_value(member_field, val);
			bt_put(member_field);
			if (ret) {
				goto end;
			}
		}
		break;
	}
	case BT_FIELD_TYPE_ID_VARIANT:
	{
		struct bt_field *cur_field =
			bt_field_variant_get_current_field(field);

		if (!cur_field) {
			ret = -1;
			goto end;
		}

		ret = visit_field_update_clock_value(cur_field, val);
		bt_put(cur_field);
		break;
	}
	default:
		break;
	}

end:
	return ret;
}

int visit_event_update_clock_value(struct bt_event *event, uint64_t *val)
{
	int ret = 0;
	struct bt_field *field;

	field = bt_event_get_header(event);
	ret = visit_field_update_clock_value(field, val);
	bt_put(field);
	if (ret) {
		BT_LOGW_STR("Cannot automatically update clock value in "
			"event's header.");
		goto end;
	}

	field = bt_event_get_stream_event_context(event);
	ret = visit_field_update_clock_value(field, val);
	bt_put(field);
	if (ret) {
		BT_LOGW_STR("Cannot automatically update clock value in "
			"event's stream event context.");
		goto end;
	}

	field = bt_event_get_event_context(event);
	ret = visit_field_update_clock_value(field, val);
	bt_put(field);
	if (ret) {
		BT_LOGW_STR("Cannot automatically update clock value in "
			"event's context.");
		goto end;
	}

	field = bt_event_get_event_payload(event);
	ret = visit_field_update_clock_value(field, val);
	bt_put(field);
	if (ret) {
		BT_LOGW_STR("Cannot automatically update clock value in "
			"event's payload.");
		goto end;
	}

end:
	return ret;
}

static
int set_packet_context_timestamps(struct bt_stream *stream)
{
	int ret = 0;
	uint64_t val;
	uint64_t cur_clock_value;
	uint64_t init_clock_value = 0;
	struct bt_field *ts_begin_field = bt_field_structure_get_field_by_name(
		stream->packet_context, "timestamp_begin");
	struct bt_field *ts_end_field = bt_field_structure_get_field_by_name(
		stream->packet_context, "timestamp_end");
	uint64_t i;
	int64_t len;

	if (ts_begin_field && bt_field_is_set(ts_begin_field)) {
		/* Use provided `timestamp_begin` value as starting value */
		ret = bt_field_unsigned_integer_get_value(ts_begin_field, &val);
		assert(ret == 0);
		init_clock_value = val;
	} else if (stream->last_ts_end != -1ULL) {
		/* Use last packet's ending timestamp as starting value */
		init_clock_value = stream->last_ts_end;
	}

	cur_clock_value = init_clock_value;

	if (stream->last_ts_end != -1ULL &&
			cur_clock_value < stream->last_ts_end) {
		BT_LOGW("Packet's initial timestamp is less than previous "
			"packet's final timestamp: "
			"stream-addr=%p, stream-name=\"%s\", "
			"cur-packet-ts-begin=%" PRIu64 ", "
			"prev-packet-ts-end=%" PRIu64,
			stream, bt_stream_get_name(stream),
			cur_clock_value, stream->last_ts_end);
		ret = -1;
		goto end;
	}

	/*
	 * Visit all the packet context fields, followed by all the
	 * fields of all the events, in order, updating our current
	 * clock value as we visit.
	 *
	 * While visiting the packet context fields, do not consider
	 * `timestamp_begin` and `timestamp_end` because this function's
	 * purpose is to set them anyway. Also do not consider
	 * `packet_size`, `content_size`, `events_discarded`, and
	 * `packet_seq_num` if they are not set because those are
	 * autopopulating fields.
	 */
	len = bt_field_type_structure_get_field_count(
		stream->packet_context->type);
	assert(len >= 0);

	for (i = 0; i < len; i++) {
		const char *member_name;
		struct bt_field *member_field;

		ret = bt_field_type_structure_get_field_by_index(
			stream->packet_context->type, &member_name, NULL, i);
		assert(ret == 0);

		if (strcmp(member_name, "timestamp_begin") == 0 ||
				strcmp(member_name, "timestamp_end") == 0) {
			continue;
		}

		member_field = bt_field_structure_get_field_by_index(
			stream->packet_context, i);
		assert(member_field);

		if (strcmp(member_name, "packet_size") == 0 &&
				!bt_field_is_set(member_field)) {
			bt_put(member_field);
			continue;
		}

		if (strcmp(member_name, "content_size") == 0 &&
				!bt_field_is_set(member_field)) {
			bt_put(member_field);
			continue;
		}

		if (strcmp(member_name, "events_discarded") == 0 &&
				!bt_field_is_set(member_field)) {
			bt_put(member_field);
			continue;
		}

		if (strcmp(member_name, "packet_seq_num") == 0 &&
				!bt_field_is_set(member_field)) {
			bt_put(member_field);
			continue;
		}

		ret = visit_field_update_clock_value(member_field,
			&cur_clock_value);
		bt_put(member_field);
		if (ret) {
			BT_LOGW("Cannot automatically update clock value "
				"in stream's packet context: "
				"stream-addr=%p, stream-name=\"%s\", "
				"field-name=\"%s\"",
				stream, bt_stream_get_name(stream),
				member_name);
			goto end;
		}
	}

	for (i = 0; i < stream->events->len; i++) {
		struct bt_event *event = g_ptr_array_index(stream->events, i);

		assert(event);
		ret = visit_event_update_clock_value(event, &cur_clock_value);
		if (ret) {
			BT_LOGW("Cannot automatically update clock value "
				"in stream's packet context: "
				"stream-addr=%p, stream-name=\"%s\", "
				"index=%" PRIu64 ", event-addr=%p, "
				"event-class-id=%" PRId64 ", "
				"event-class-name=\"%s\"",
				stream, bt_stream_get_name(stream),
				i, event,
				bt_event_class_get_id(event->event_class),
				bt_event_class_get_name(event->event_class));
			goto end;
		}
	}

	/*
	 * Everything is visited, thus the current clock value
	 * corresponds to the ending timestamp. Validate this value
	 * against the provided value of `timestamp_end`, if any,
	 * otherwise set it.
	 */
	if (ts_end_field && bt_field_is_set(ts_end_field)) {
		ret = bt_field_unsigned_integer_get_value(ts_end_field, &val);
		assert(ret == 0);

		if (val < cur_clock_value) {
			BT_LOGW("Packet's final timestamp is less than "
				"computed packet's final timestamp: "
				"stream-addr=%p, stream-name=\"%s\", "
				"cur-packet-ts-end=%" PRIu64 ", "
				"computed-packet-ts-end=%" PRIu64,
				stream, bt_stream_get_name(stream),
				val, cur_clock_value);
			ret = -1;
			goto end;
		}

		stream->last_ts_end = val;
	}

	if (ts_end_field && !bt_field_is_set(ts_end_field)) {
		ret = set_integer_field_value(ts_end_field, cur_clock_value);
		assert(ret == 0);
		stream->last_ts_end = cur_clock_value;
	}

	if (!ts_end_field) {
		stream->last_ts_end = cur_clock_value;
	}

	/* Set `timestamp_begin` field to initial clock value */
	if (ts_begin_field && !bt_field_is_set(ts_begin_field)) {
		ret = set_integer_field_value(ts_begin_field, init_clock_value);
		assert(ret == 0);
	}

end:
	bt_put(ts_begin_field);
	bt_put(ts_end_field);
	return ret;
}

static
int auto_populate_packet_context(struct bt_stream *stream, bool set_ts)
{
	int ret = 0;

	if (!stream->packet_context) {
		goto end;
	}

	ret = set_packet_context_packet_size(stream);
	if (ret) {
		BT_LOGW("Cannot set packet context's packet size field: "
			"stream-addr=%p, stream-name=\"%s\"",
			stream, bt_stream_get_name(stream));
		goto end;
	}

	ret = set_packet_context_content_size(stream);
	if (ret) {
		BT_LOGW("Cannot set packet context's content size field: "
			"stream-addr=%p, stream-name=\"%s\"",
			stream, bt_stream_get_name(stream));
		goto end;
	}

	if (set_ts) {
		ret = set_packet_context_timestamps(stream);
		if (ret) {
			BT_LOGW("Cannot set packet context's timestamp fields: "
				"stream-addr=%p, stream-name=\"%s\"",
				stream, bt_stream_get_name(stream));
			goto end;
		}
	}

	ret = set_packet_context_events_discarded(stream);
	if (ret) {
		BT_LOGW("Cannot set packet context's discarded events count field: "
			"stream-addr=%p, stream-name=\"%s\"",
			stream, bt_stream_get_name(stream));
		goto end;
	}

	BT_LOGV("Automatically populated stream's packet context's known fields: "
		"stream-addr=%p, stream-name=\"%s\"",
		stream, bt_stream_get_name(stream));

end:
	return ret;
}

static
void release_event(struct bt_event *event)
{
	if (bt_object_get_ref_count(event)) {
		/*
		 * The event is being orphaned, but it must guarantee the
		 * existence of its event class for the duration of its
		 * lifetime.
		 */
		bt_get(event->event_class);
		BT_PUT(event->base.parent);
	} else {
		bt_object_release(event);
	}
}

static
int create_stream_file(struct bt_ctf_writer *writer,
		struct bt_stream *stream)
{
	int fd;
	GString *filename = g_string_new(NULL);
	int64_t stream_class_id;
	char *file_path = NULL;

	BT_LOGD("Creating stream file: writer-addr=%p, stream-addr=%p, "
		"stream-name=\"%s\", stream-class-addr=%p, stream-class-name=\"%s\"",
		writer, stream, bt_stream_get_name(stream),
		stream->stream_class, stream->stream_class->name->str);

	if (stream->name && stream->name->len > 0) {
		/* Use stream name's base name as prefix */
		gchar *basename = g_path_get_basename(stream->name->str);

		assert(basename);

		if (strcmp(basename, G_DIR_SEPARATOR_S) == 0) {
			g_string_assign(filename, "stream");
		} else {
			g_string_assign(filename, basename);
		}

		g_free(basename);
		goto append_ids;
	}

	if (stream->stream_class->name &&
			stream->stream_class->name->len > 0) {
		/* Use stream class name's base name as prefix */
		gchar *basename =
			g_path_get_basename(stream->stream_class->name->str);

		assert(basename);

		if (strcmp(basename, G_DIR_SEPARATOR_S) == 0) {
			g_string_assign(filename, "stream");
		} else {
			g_string_assign(filename, basename);
		}

		g_free(basename);
		goto append_ids;
	}

	/* Default to using `stream-` as prefix */
	g_string_assign(filename, "stream");

append_ids:
	stream_class_id = bt_stream_class_get_id(stream->stream_class);
	assert(stream_class_id >= 0);
	assert(stream->id >= 0);
	g_string_append_printf(filename, "-%" PRId64 "-%" PRId64,
		stream_class_id, stream->id);

	file_path = g_build_filename(writer->path->str, filename->str, NULL);
	if (file_path == NULL) {
		fd = -1;
		goto end;
	}

	fd = open(file_path,
		O_RDWR | O_CREAT | O_TRUNC,
		S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);
	g_free(file_path);
	if (fd < 0) {
		BT_LOGW_ERRNO("Failed to open stream file for writing",
			": file_path=\"%s\", filename=\"%s\", ret=%d",
			file_path, filename->str, fd);
		goto end;
	}

	BT_LOGD("Created stream file for writing: "
		"stream-addr=%p, stream-name=\"%s\", "
		"filename=\"%s\", fd=%d", stream, bt_stream_get_name(stream),
		filename->str, fd);

end:
	g_string_free(filename, TRUE);
	return fd;
}

static
void set_stream_fd(struct bt_stream *stream, int fd)
{
	(void) bt_stream_pos_init(&stream->pos, fd, O_RDWR);
	stream->pos.fd = fd;
}

static
void component_destroy_listener(struct bt_component *component, void *data)
{
	struct bt_stream *stream = data;

	BT_LOGD("Component is being destroyed, stream is notified: "
		"comp-addr=%p, stream-addr=%p", component, stream);
	g_hash_table_remove(stream->comp_cur_port, component);
}

static
struct bt_stream *bt_stream_create_with_id_no_check(
		struct bt_stream_class *stream_class,
		const char *name, uint64_t id)
{
	int ret;
	struct bt_stream *stream = NULL;
	struct bt_trace *trace = NULL;
	struct bt_ctf_writer *writer = NULL;

	if (!stream_class) {
		BT_LOGW_STR("Invalid parameter: stream class is NULL.");
		goto error;
	}

	BT_LOGD("Creating stream object: stream-class-addr=%p, "
		"stream-class-name=\"%s\", stream-name=\"%s\", "
		"stream-id=%" PRIu64,
		stream_class, bt_stream_class_get_name(stream_class),
		name, id);
	trace = bt_stream_class_borrow_trace(stream_class);
	if (!trace) {
		BT_LOGW("Invalid parameter: cannot create stream from a stream class which is not part of trace: "
			"stream-class-addr=%p, stream-class-name=\"%s\", "
			"stream-name=\"%s\"",
			stream_class, bt_stream_class_get_name(stream_class),
			name);
		goto error;
	}

	if (bt_trace_is_static(trace)) {
		/*
		 * A static trace has the property that all its stream
		 * classes, clock classes, and streams are definitive:
		 * no more can be added, and each object is also frozen.
		 */
		BT_LOGW("Invalid parameter: cannot create stream from a stream class which is part of a static trace: "
			"stream-class-addr=%p, stream-class-name=\"%s\", "
			"stream-name=\"%s\", trace-addr=%p",
			stream_class, bt_stream_class_get_name(stream_class),
			name, trace);
		goto error;
	}

	if (id != -1ULL) {
		/*
		 * Validate that the given ID is unique amongst all the
		 * existing trace's streams created from the same stream
		 * class.
		 */
		size_t i;

		for (i = 0; i < trace->streams->len; i++) {
			struct bt_stream *trace_stream =
				g_ptr_array_index(trace->streams, i);

			if (trace_stream->stream_class != stream_class) {
				continue;
			}

			if (trace_stream->id == id) {
				BT_LOGW_STR("Invalid parameter: another stream in the same trace already has this ID.");
				goto error;
			}
		}
	}

	stream = g_new0(struct bt_stream, 1);
	if (!stream) {
		BT_LOGE_STR("Failed to allocate one stream.");
		goto error;
	}

	bt_object_init(stream, bt_stream_destroy);
	/*
	 * Acquire reference to parent since stream will become publicly
	 * reachable; it needs its parent to remain valid.
	 */
	bt_object_set_parent(stream, trace);
	stream->stream_class = stream_class;
	stream->pos.fd = -1;
	stream->id = (int64_t) id;

	stream->destroy_listeners = g_array_new(FALSE, TRUE,
		sizeof(struct bt_stream_destroy_listener));
	if (!stream->destroy_listeners) {
		BT_LOGE_STR("Failed to allocate a GArray.");
		goto error;
	}

	if (name) {
		stream->name = g_string_new(name);
		if (!stream->name) {
			BT_LOGE_STR("Failed to allocate a GString.");
			goto error;
		}
	}

	BT_LOGD("Set stream's trace parent: trace-addr=%p", trace);

	if (trace->is_created_by_writer) {
		int fd;

		writer = (struct bt_ctf_writer *) bt_object_get_parent(trace);
		stream->id = (int64_t) stream_class->next_stream_id++;
		stream->last_ts_end = -1ULL;

		BT_LOGD("Stream object belongs to a writer's trace: "
			"writer-addr=%p", writer);
		assert(writer);

		if (stream_class->packet_context_type) {
			BT_LOGD("Creating stream's packet context field: "
				"ft-addr=%p", stream_class->packet_context_type);
			stream->packet_context = bt_field_create(
				stream_class->packet_context_type);
			if (!stream->packet_context) {
				BT_LOGW_STR("Cannot create stream's packet context field.");
				goto error;
			}

			/* Initialize events_discarded */
			ret = try_set_structure_field_integer(
				stream->packet_context, "events_discarded", 0);
			if (ret < 0) {
				BT_LOGW("Cannot set `events_discarded` field in packet context: "
					"ret=%d, packet-context-field-addr=%p",
					ret, stream->packet_context);
				goto error;
			}
		}

		stream->events = g_ptr_array_new_with_free_func(
			(GDestroyNotify) release_event);
		if (!stream->events) {
			BT_LOGE_STR("Failed to allocate a GPtrArray.");
			goto error;
		}

		if (trace->packet_header_type) {
			BT_LOGD("Creating stream's packet header field: "
				"ft-addr=%p", trace->packet_header_type);
			stream->packet_header =
				bt_field_create(trace->packet_header_type);
			if (!stream->packet_header) {
				BT_LOGW_STR("Cannot create stream's packet header field.");
				goto error;
			}
		}

		/*
		 * Attempt to populate the default trace packet header fields
		 * (magic, uuid and stream_id). This will _not_ fail shall the
		 * fields not be found or be of an incompatible type; they will
		 * simply not be populated automatically. The user will have to
		 * make sure to set the trace packet header fields himself
		 * before flushing.
		 */
		ret = auto_populate_packet_header(stream);
		if (ret) {
			BT_LOGW_STR("Cannot automatically populate the stream's packet header.");
			goto error;
		}

		/* Create file associated with this stream */
		fd = create_stream_file(writer, stream);
		if (fd < 0) {
			BT_LOGW_STR("Cannot create stream file.");
			goto error;
		}

		set_stream_fd(stream, fd);

		/* Freeze the writer */
		BT_LOGD_STR("Freezing stream's CTF writer.");
		bt_ctf_writer_freeze(writer);
	} else {
		/* Non-writer stream indicated by a negative FD */
		set_stream_fd(stream, -1);
		stream->comp_cur_port = g_hash_table_new(g_direct_hash,
			g_direct_equal);
		if (!stream->comp_cur_port) {
			BT_LOGE_STR("Failed to allocate a GHashTable.");
			goto error;
		}
	}

	/* Add this stream to the trace's streams */
	g_ptr_array_add(trace->streams, stream);
	BT_LOGD("Created stream object: addr=%p", stream);
	goto end;

error:
	BT_PUT(stream);

end:
	bt_put(writer);
	return stream;
}

struct bt_stream *bt_stream_create_with_id(
		struct bt_stream_class *stream_class,
		const char *name, uint64_t id_param)
{
	struct bt_trace *trace;
	struct bt_stream *stream = NULL;
	int64_t id = (int64_t) id_param;

	if (!stream_class) {
		BT_LOGW_STR("Invalid parameter: stream class is NULL.");
		goto end;
	}

	if (id < 0) {
		BT_LOGW("Invalid parameter: invalid stream's ID: "
			"name=\"%s\", id=%" PRIu64,
			name, id_param);
		goto end;
	}

	trace = bt_stream_class_borrow_trace(stream_class);
	if (!trace) {
		BT_LOGW("Invalid parameter: cannot create stream from a stream class which is not part of trace: "
			"stream-class-addr=%p, stream-class-name=\"%s\", "
			"stream-name=\"%s\", stream-id=%" PRIu64,
			stream_class, bt_stream_class_get_name(stream_class),
			name, id_param);
		goto end;
	}

	if (trace->is_created_by_writer) {
		BT_LOGW("Invalid parameter: cannot create a CTF writer stream with this function; use bt_stream_create(): "
			"stream-class-addr=%p, stream-class-name=\"%s\", "
			"stream-name=\"%s\", stream-id=%" PRIu64,
			stream_class, bt_stream_class_get_name(stream_class),
			name, id_param);
		goto end;
	}

	stream = bt_stream_create_with_id_no_check(stream_class,
		name, id_param);

end:
	return stream;
}

struct bt_stream *bt_stream_create(
		struct bt_stream_class *stream_class,
		const char *name)
{
	return bt_stream_create_with_id_no_check(stream_class,
		name, -1ULL);
}

struct bt_stream_class *bt_stream_get_class(
		struct bt_stream *stream)
{
	struct bt_stream_class *stream_class = NULL;

	if (!stream) {
		BT_LOGW_STR("Invalid parameter: stream is NULL.");
		goto end;
	}

	stream_class = stream->stream_class;
	bt_get(stream_class);
end:
	return stream_class;
}

int bt_stream_get_discarded_events_count(
		struct bt_stream *stream, uint64_t *count)
{
	int ret = 0;

	if (!stream) {
		BT_LOGW_STR("Invalid parameter: stream is NULL.");
		ret = -1;
		goto end;
	}

	if (!count) {
		BT_LOGW_STR("Invalid parameter: count is NULL.");
		ret = -1;
		goto end;
	}

	if (stream->pos.fd < 0) {
		BT_LOGW("Invalid parameter: stream is not a CTF writer stream: "
			"stream-addr=%p, stream-name=\"%s\"",
			stream, bt_stream_get_name(stream));
		ret = -1;
		goto end;
	}

	*count = (uint64_t) stream->discarded_events;

end:
	return ret;
}

static
int set_packet_context_events_discarded_field(struct bt_stream *stream,
	uint64_t count)
{
	int ret = 0;
	struct bt_field *events_discarded_field = NULL;

	if (!stream->packet_context) {
		goto end;
	}

	events_discarded_field = bt_field_structure_get_field_by_name(
		stream->packet_context, "events_discarded");
	if (!events_discarded_field) {
		goto end;
	}

	ret = bt_field_unsigned_integer_set_value(
		events_discarded_field, count);
	if (ret) {
		BT_LOGW("Cannot set packet context's `events_discarded` field: "
			"field-addr=%p, value=%" PRIu64,
			events_discarded_field, count);
		goto end;
	}

end:
	bt_put(events_discarded_field);
	return ret;
}

void bt_stream_append_discarded_events(struct bt_stream *stream,
		uint64_t event_count)
{
	int ret;
	uint64_t new_count;
	struct bt_field *events_discarded_field = NULL;

	if (!stream) {
		BT_LOGW_STR("Invalid parameter: stream is NULL.");
		goto end;
	}

	BT_LOGV("Appending discarded events to stream: "
		"stream-addr=%p, stream-name=\"%s\", append-count=%" PRIu64,
		stream, bt_stream_get_name(stream), event_count);

	if (!stream->packet_context) {
		BT_LOGW_STR("Invalid parameter: stream has no packet context field.");
		goto end;
	}

	if (stream->pos.fd < 0) {
		BT_LOGW_STR("Invalid parameter: stream is not a CTF writer stream.");
		goto end;
	}

	events_discarded_field = bt_field_structure_get_field_by_name(
		stream->packet_context, "events_discarded");
	if (!events_discarded_field) {
		BT_LOGW_STR("No field named `events_discarded` in stream's packet context.");
		goto end;
	}

	new_count = stream->discarded_events + event_count;
	if (new_count < stream->discarded_events) {
		BT_LOGW("New discarded events count is less than the stream's current discarded events count: "
			"cur-count=%" PRIu64 ", new-count=%" PRIu64,
			stream->discarded_events, new_count);
		goto end;
	}

	ret = set_packet_context_events_discarded_field(stream, new_count);
	if (ret) {
		/* set_packet_context_events_discarded_field() logs errors */
		goto end;
	}

	stream->discarded_events = new_count;
	BT_LOGV("Appended discarded events to stream: "
		"stream-addr=%p, stream-name=\"%s\", append-count=%" PRIu64,
		stream, bt_stream_get_name(stream), event_count);

end:
	bt_put(events_discarded_field);
}

static int auto_populate_event_header(struct bt_stream *stream,
		struct bt_event *event)
{
	int ret = 0;
	struct bt_field *id_field = NULL, *timestamp_field = NULL;
	struct bt_clock_class *mapped_clock_class = NULL;
	int64_t event_class_id;

	assert(event);

	if (!event->event_header) {
		goto end;
	}

	if (event->frozen) {
		BT_LOGW_STR("Cannot populate event header field: event is frozen.");
		ret = -1;
		goto end;
	}

	BT_LOGV("Automatically populating event's header field: "
		"stream-addr=%p, stream-name=\"%s\", event-addr=%p",
		stream, bt_stream_get_name(stream), event);

	id_field = bt_field_structure_get_field_by_name(event->event_header, "id");
	event_class_id = bt_event_class_get_id(event->event_class);
	assert(event_class_id >= 0);

	if (id_field && bt_field_type_is_integer(id_field->type)) {
		ret = set_integer_field_value(id_field, event_class_id);
		if (ret) {
			BT_LOGW("Cannot set event header's `id` field's value: "
				"addr=%p, value=%" PRIu64, id_field,
				event_class_id);
			goto end;
		}
	}

	/*
	 * The conditions to automatically set the timestamp are:
	 *
	 * 1. The event header field "timestamp" exists and is an
	 *    integer field.
	 * 2. This stream's class has a registered clock (set with
	 *    bt_stream_class_set_clock()).
	 * 3. The "timestamp" field is not set.
	 */
	timestamp_field = bt_field_structure_get_field_by_name(
			event->event_header, "timestamp");
	if (timestamp_field && stream->stream_class->clock &&
			bt_field_type_is_integer(timestamp_field->type) &&
			!bt_field_is_set(timestamp_field)) {
		struct bt_clock_class *stream_class_clock_class =
			stream->stream_class->clock->clock_class;

		mapped_clock_class =
			bt_field_type_integer_get_mapped_clock_class(
				timestamp_field->type);
		if (mapped_clock_class) {
			uint64_t timestamp;

			assert(mapped_clock_class == stream_class_clock_class);
			ret = bt_ctf_clock_get_value(
				stream->stream_class->clock,
				&timestamp);
			assert(ret == 0);
			ret = set_integer_field_value(timestamp_field,
					timestamp);
			if (ret) {
				BT_LOGW("Cannot set event header's `timestamp` field's value: "
					"addr=%p, value=%" PRIu64,
					timestamp_field, timestamp);
				goto end;
			}
		}
	}

	BT_LOGV("Automatically populated event's header field: "
		"stream-addr=%p, stream-name=\"%s\", event-addr=%p",
		stream, bt_stream_get_name(stream), event);

end:
	bt_put(id_field);
	bt_put(timestamp_field);
	bt_put(mapped_clock_class);
	return ret;
}

int bt_stream_append_event(struct bt_stream *stream,
		struct bt_event *event)
{
	int ret = 0;

	if (!stream) {
		BT_LOGW_STR("Invalid parameter: stream is NULL.");
		ret = -1;
		goto end;
	}

	if (!event) {
		BT_LOGW_STR("Invalid parameter: event is NULL.");
		ret = -1;
		goto end;
	}

	if (stream->pos.fd < 0) {
		BT_LOGW_STR("Invalid parameter: stream is not a CTF writer stream.");
		ret = -1;
		goto end;
	}

	BT_LOGV("Appending event to stream: "
		"stream-addr=%p, stream-name=\"%s\", event-addr=%p, "
		"event-class-name=\"%s\", event-class-id=%" PRId64,
		stream, bt_stream_get_name(stream), event,
		bt_event_class_get_name(bt_event_borrow_event_class(event)),
		bt_event_class_get_id(bt_event_borrow_event_class(event)));

	/*
	 * The event is not supposed to have a parent stream at this
	 * point. The only other way an event can have a parent stream
	 * is if it was assigned when setting a packet to the event,
	 * in which case the packet's stream is not a writer stream,
	 * and thus the user is trying to append an event which belongs
	 * to another stream.
	 */
	if (event->base.parent) {
		ret = -1;
		goto end;
	}

	bt_object_set_parent(event, stream);
	BT_LOGV_STR("Automatically populating the header of the event to append.");
	ret = auto_populate_event_header(stream, event);
	if (ret) {
		/* auto_populate_event_header() reports errors */
		goto error;
	}

	/* Make sure the various scopes of the event are set */
	BT_LOGV_STR("Validating event to append.");
	ret = bt_event_validate(event);
	if (ret) {
		goto error;
	}

	/* Save the new event and freeze it */
	BT_LOGV_STR("Freezing the event to append.");
	bt_event_freeze(event);
	g_ptr_array_add(stream->events, event);

	/*
	 * Event had to hold a reference to its event class as long as it wasn't
	 * part of the same trace hierarchy. From now on, the event and its
	 * class share the same lifetime guarantees and the reference is no
	 * longer needed.
	 */
	BT_LOGV_STR("Putting the event's class.");
	bt_put(event->event_class);
	BT_LOGV("Appended event to stream: "
		"stream-addr=%p, stream-name=\"%s\", event-addr=%p, "
		"event-class-name=\"%s\", event-class-id=%" PRId64,
		stream, bt_stream_get_name(stream), event,
		bt_event_class_get_name(bt_event_borrow_event_class(event)),
		bt_event_class_get_id(bt_event_borrow_event_class(event)));

end:
	return ret;

error:
	/*
	 * Orphan the event; we were not successful in associating it to
	 * a stream.
	 */
	bt_object_set_parent(event, NULL);

	return ret;
}

struct bt_field *bt_stream_get_packet_context(struct bt_stream *stream)
{
	struct bt_field *packet_context = NULL;

	if (!stream) {
		BT_LOGW_STR("Invalid parameter: stream is NULL.");
		goto end;
	}

	if (stream->pos.fd < 0) {
		BT_LOGW("Invalid parameter: stream is not a CTF writer stream: "
			"stream-addr=%p, stream-name=\"%s\"", stream,
			bt_stream_get_name(stream));
		goto end;
	}

	packet_context = stream->packet_context;
	if (packet_context) {
		bt_get(packet_context);
	}
end:
	return packet_context;
}

int bt_stream_set_packet_context(struct bt_stream *stream,
		struct bt_field *field)
{
	int ret = 0;
	struct bt_field_type *field_type;

	if (!stream) {
		BT_LOGW_STR("Invalid parameter: stream is NULL.");
		ret = -1;
		goto end;
	}

	if (stream->pos.fd < 0) {
		BT_LOGW_STR("Invalid parameter: stream is not a CTF writer stream.");
		ret = -1;
		goto end;
	}

	field_type = bt_field_get_type(field);
	if (bt_field_type_compare(field_type,
			stream->stream_class->packet_context_type)) {
		BT_LOGW("Invalid parameter: packet context's field type is different from the stream's packet context field type: "
			"stream-addr=%p, stream-name=\"%s\", "
			"packet-context-field-addr=%p, "
			"packet-context-ft-addr=%p",
			stream, bt_stream_get_name(stream),
			field, field_type);
		ret = -1;
		goto end;
	}

	bt_put(field_type);
	bt_put(stream->packet_context);
	stream->packet_context = bt_get(field);
	BT_LOGV("Set stream's packet context field: "
		"stream-addr=%p, stream-name=\"%s\", "
		"packet-context-field-addr=%p",
		stream, bt_stream_get_name(stream), field);
end:
	return ret;
}

struct bt_field *bt_stream_get_packet_header(struct bt_stream *stream)
{
	struct bt_field *packet_header = NULL;

	if (!stream) {
		BT_LOGW_STR("Invalid parameter: stream is NULL.");
		goto end;
	}

	if (stream->pos.fd < 0) {
		BT_LOGW("Invalid parameter: stream is not a CTF writer stream: "
			"stream-addr=%p, stream-name=\"%s\"", stream,
			bt_stream_get_name(stream));
		goto end;
	}

	packet_header = stream->packet_header;
	if (packet_header) {
		bt_get(packet_header);
	}
end:
	return packet_header;
}

int bt_stream_set_packet_header(struct bt_stream *stream,
		struct bt_field *field)
{
	int ret = 0;
	struct bt_trace *trace = NULL;
	struct bt_field_type *field_type = NULL;

	if (!stream) {
		BT_LOGW_STR("Invalid parameter: stream is NULL.");
		ret = -1;
		goto end;
	}

	if (stream->pos.fd < 0) {
		BT_LOGW_STR("Invalid parameter: stream is not a CTF writer stream.");
		ret = -1;
		goto end;
	}

	trace = (struct bt_trace *) bt_object_get_parent(stream);

	if (!field) {
		if (trace->packet_header_type) {
			BT_LOGW("Invalid parameter: setting no packet header but packet header field type is not NULL: "
				"stream-addr=%p, stream-name=\"%s\", "
				"packet-header-field-addr=%p, "
				"expected-ft-addr=%p",
				stream, bt_stream_get_name(stream),
				field, trace->packet_header_type);
			ret = -1;
			goto end;
		}

		goto skip_validation;
	}

	field_type = bt_field_get_type(field);
	assert(field_type);

	if (bt_field_type_compare(field_type, trace->packet_header_type)) {
		BT_LOGW("Invalid parameter: packet header's field type is different from the stream's packet header field type: "
			"stream-addr=%p, stream-name=\"%s\", "
			"packet-header-field-addr=%p, "
			"packet-header-ft-addr=%p",
			stream, bt_stream_get_name(stream),
			field, field_type);
		ret = -1;
		goto end;
	}

skip_validation:
	bt_put(stream->packet_header);
	stream->packet_header = bt_get(field);
	BT_LOGV("Set stream's packet header field: "
		"stream-addr=%p, stream-name=\"%s\", "
		"packet-header-field-addr=%p",
		stream, bt_stream_get_name(stream), field);
end:
	BT_PUT(trace);
	bt_put(field_type);
	return ret;
}

static
void reset_structure_field(struct bt_field *structure, const char *name)
{
	struct bt_field *member;

	member = bt_field_structure_get_field_by_name(structure, name);
	if (member) {
		(void) bt_field_reset(member);
		bt_put(member);
	}
}

int bt_stream_flush(struct bt_stream *stream)
{
	int ret = 0;
	size_t i;
	struct bt_stream_pos packet_context_pos;
	struct bt_trace *trace;
	enum bt_byte_order native_byte_order;
	bool has_packet_size = false;

	if (!stream) {
		BT_LOGW_STR("Invalid parameter: stream is NULL.");
		ret = -1;
		goto end_no_stream;
	}

	if (stream->pos.fd < 0) {
		BT_LOGW_STR("Invalid parameter: stream is not a CTF writer stream.");
		ret = -1;
		goto end;
	}

	if (stream->packet_context) {
		struct bt_field *packet_size_field;

		packet_size_field = bt_field_structure_get_field_by_name(
				stream->packet_context, "packet_size");
		has_packet_size = (packet_size_field != NULL);
		bt_put(packet_size_field);
	}

	if (stream->flushed_packet_count == 1) {
		if (!stream->packet_context) {
			BT_LOGW_STR("Cannot flush a stream which has no packet context field more than once.");
			ret = -1;
			goto end;
		}

		if (!has_packet_size) {
			BT_LOGW_STR("Cannot flush a stream which has no packet context's `packet_size` field more than once.");
			ret = -1;
			goto end;
		}
	}

	BT_LOGV("Flushing stream's current packet: stream-addr=%p, "
		"stream-name=\"%s\", packet-index=%u", stream,
		bt_stream_get_name(stream), stream->flushed_packet_count);
	trace = bt_stream_class_borrow_trace(stream->stream_class);
	assert(trace);
	native_byte_order = bt_trace_get_native_byte_order(trace);

	ret = auto_populate_packet_header(stream);
	if (ret) {
		BT_LOGW_STR("Cannot automatically populate the stream's packet header field.");
		ret = -1;
		goto end;
	}

	ret = auto_populate_packet_context(stream, true);
	if (ret) {
		BT_LOGW_STR("Cannot automatically populate the stream's packet context field.");
		ret = -1;
		goto end;
	}

	/* mmap the next packet */
	BT_LOGV("Seeking to the next packet: pos-offset=%" PRId64,
		stream->pos.offset);
	bt_stream_pos_packet_seek(&stream->pos, 0, SEEK_CUR);
	assert(stream->pos.packet_size % 8 == 0);

	if (stream->packet_header) {
		BT_LOGV_STR("Serializing packet header field.");
		ret = bt_field_serialize(stream->packet_header, &stream->pos,
			native_byte_order);
		if (ret) {
			BT_LOGW("Cannot serialize stream's packet header field: "
				"field-addr=%p", stream->packet_header);
			goto end;
		}
	}

	if (stream->packet_context) {
		/* Write packet context */
		memcpy(&packet_context_pos, &stream->pos,
			sizeof(packet_context_pos));
		BT_LOGV_STR("Serializing packet context field.");
		ret = bt_field_serialize(stream->packet_context,
			&stream->pos, native_byte_order);
		if (ret) {
			BT_LOGW("Cannot serialize stream's packet context field: "
				"field-addr=%p", stream->packet_context);
			goto end;
		}
	}

	BT_LOGV("Serializing events: count=%u", stream->events->len);

	for (i = 0; i < stream->events->len; i++) {
		struct bt_event *event = g_ptr_array_index(
			stream->events, i);
		struct bt_event_class *event_class =
			bt_event_borrow_event_class(event);

		BT_LOGV("Serializing event: index=%zu, event-addr=%p, "
			"event-class-name=\"%s\", event-class-id=%" PRId64 ", "
			"pos-offset=%" PRId64 ", packet-size=%" PRIu64,
			i, event, bt_event_class_get_name(event_class),
			bt_event_class_get_id(event_class),
			stream->pos.offset, stream->pos.packet_size);

		/* Write event header */
		if (event->event_header) {
			BT_LOGV_STR("Serializing event's header field.");
			ret = bt_field_serialize(event->event_header,
					&stream->pos, native_byte_order);
			if (ret) {
				BT_LOGW("Cannot serialize event's header field: "
						"field-addr=%p", event->event_header);
				goto end;
			}
		}

		/* Write stream event context */
		if (event->stream_event_context) {
			BT_LOGV_STR("Serializing event's stream event context field.");
			ret = bt_field_serialize(
				event->stream_event_context, &stream->pos,
				native_byte_order);
			if (ret) {
				BT_LOGW("Cannot serialize event's stream event context field: "
					"field-addr=%p", event->stream_event_context);
				goto end;
			}
		}

		/* Write event content */
		ret = bt_event_serialize(event, &stream->pos,
			native_byte_order);
		if (ret) {
			/* bt_event_serialize() logs errors */
			goto end;
		}
	}

	if (!has_packet_size && stream->pos.offset % 8 != 0) {
		BT_LOGW("Stream's packet context field type has no `packet_size` field, "
			"but current content size is not a multiple of 8 bits: "
			"content-size=%" PRId64 ", "
			"packet-size=%" PRIu64,
			stream->pos.offset,
			stream->pos.packet_size);
		ret = -1;
		goto end;
	}

	assert(stream->pos.packet_size % 8 == 0);

	/*
	 * Remove extra padding bytes.
	 */
	stream->pos.packet_size = (stream->pos.offset + 7) & ~7;

	if (stream->packet_context) {
		/*
		 * The whole packet is serialized at this point. Make sure that,
		 * if `packet_size` is missing, the current content size is
		 * equal to the current packet size.
		 */
		struct bt_field *field = bt_field_structure_get_field_by_name(
			stream->packet_context, "content_size");

		bt_put(field);
		if (!field) {
			if (stream->pos.offset != stream->pos.packet_size) {
				BT_LOGW("Stream's packet context's `content_size` field is missing, "
					"but current packet's content size is not equal to its packet size: "
					"content-size=%" PRId64 ", "
					"packet-size=%" PRIu64,
					stream->pos.offset,
					stream->pos.packet_size);
				ret = -1;
				goto end;
			}
		}

		/*
		 * Overwrite the packet context now that the stream
		 * position's packet and content sizes have the correct
		 * values.
		 *
		 * Copy base_mma as the packet may have been remapped
		 * (e.g. when a packet is resized).
		 */
		packet_context_pos.base_mma = stream->pos.base_mma;
		ret = auto_populate_packet_context(stream, false);
		if (ret) {
			BT_LOGW_STR("Cannot automatically populate the stream's packet context field.");
			ret = -1;
			goto end;
		}

		BT_LOGV("Rewriting (serializing) packet context field.");
		ret = bt_field_serialize(stream->packet_context,
			&packet_context_pos, native_byte_order);
		if (ret) {
			BT_LOGW("Cannot serialize stream's packet context field: "
				"field-addr=%p", stream->packet_context);
			goto end;
		}
	}

	g_ptr_array_set_size(stream->events, 0);
	stream->flushed_packet_count++;
	stream->size += stream->pos.packet_size / CHAR_BIT;

end:
	/* Reset automatically-set fields. */
	if (stream->packet_context) {
		reset_structure_field(stream->packet_context, "timestamp_begin");
		reset_structure_field(stream->packet_context, "timestamp_end");
		reset_structure_field(stream->packet_context, "packet_size");
		reset_structure_field(stream->packet_context, "content_size");
		reset_structure_field(stream->packet_context, "events_discarded");
	}

	if (ret < 0) {
		/*
		 * We failed to write the packet. Its size is therefore set to 0
		 * to ensure the next mapping is done in the same place rather
		 * than advancing by "stream->pos.packet_size", which would
		 * leave a corrupted packet in the trace.
		 */
		stream->pos.packet_size = 0;
	} else {
		BT_LOGV("Flushed stream's current packet: content-size=%" PRId64 ", "
			"packet-size=%" PRIu64,
			stream->pos.offset, stream->pos.packet_size);
	}

end_no_stream:
	return ret;
}

/* Pre-2.0 CTF writer backward compatibility */
void bt_ctf_stream_get(struct bt_stream *stream)
{
	bt_get(stream);
}

/* Pre-2.0 CTF writer backward compatibility */
void bt_ctf_stream_put(struct bt_stream *stream)
{
	bt_put(stream);
}

static
void bt_stream_destroy(struct bt_object *obj)
{
	struct bt_stream *stream;
	int i;

	stream = container_of(obj, struct bt_stream, base);
	BT_LOGD("Destroying stream object: addr=%p, name=\"%s\"",
		stream, bt_stream_get_name(stream));

	/* Call destroy listeners in reverse registration order */
	for (i = stream->destroy_listeners->len - 1; i >= 0; i--) {
		struct bt_stream_destroy_listener *listener =
			&g_array_index(stream->destroy_listeners,
				struct bt_stream_destroy_listener, i);

		BT_LOGD("Calling destroy listener: func=%p, data=%p, index=%d",
			listener->func, listener->data, i);
		listener->func(stream, listener->data);
	}

	(void) bt_stream_pos_fini(&stream->pos);
	if (stream->pos.fd >= 0) {
		int ret;

		/*
		 * Truncate the file's size to the minimum required to fit the
		 * last packet as we might have grown it too much on the last
		 * mmap.
		 */
		do {
			ret = ftruncate(stream->pos.fd, stream->size);
		} while (ret == -1 && errno == EINTR);
		if (ret) {
			BT_LOGE_ERRNO("Failed to truncate stream file",
				": ret=%d, size=%" PRIu64,
				ret, (uint64_t) stream->size);
		}

		if (close(stream->pos.fd)) {
			BT_LOGE_ERRNO("Failed to close stream file",
				": ret=%d", ret);
		}
	}

	if (stream->events) {
		BT_LOGD_STR("Putting events.");
		g_ptr_array_free(stream->events, TRUE);
	}

	if (stream->name) {
		g_string_free(stream->name, TRUE);
	}

	if (stream->comp_cur_port) {
		GHashTableIter ht_iter;
		gpointer comp_gptr, port_gptr;

		/*
		 * Since we're destroying the stream, remove the destroy
		 * listeners that it registered for each component in
		 * its component-port mapping hash table. Otherwise they
		 * would be called and the stream would be accessed once
		 * it's freed or another stream would be accessed.
		 */
		g_hash_table_iter_init(&ht_iter, stream->comp_cur_port);

		while (g_hash_table_iter_next(&ht_iter, &comp_gptr, &port_gptr)) {
			assert(comp_gptr);
			bt_component_remove_destroy_listener((void *) comp_gptr,
				component_destroy_listener, stream);
		}

		g_hash_table_destroy(stream->comp_cur_port);
	}

	if (stream->destroy_listeners) {
		g_array_free(stream->destroy_listeners, TRUE);
	}

	BT_LOGD_STR("Putting packet header field.");
	bt_put(stream->packet_header);
	BT_LOGD_STR("Putting packet context field.");
	bt_put(stream->packet_context);
	g_free(stream);
}

static
int _set_structure_field_integer(struct bt_field *structure, char *name,
		uint64_t value, bt_bool force)
{
	int ret = 0;
	struct bt_field_type *field_type = NULL;
	struct bt_field *integer;

	assert(structure);
	assert(name);

	integer = bt_field_structure_get_field_by_name(structure, name);
	if (!integer) {
		/* Field not found, not an error. */
		BT_LOGV("Field not found: struct-field-addr=%p, "
			"name=\"%s\", force=%d", structure, name, force);
		goto end;
	}

	/* Make sure the payload has not already been set. */
	if (!force && bt_field_is_set(integer)) {
		/* Payload already set, not an error */
		BT_LOGV("Field's payload is already set: struct-field-addr=%p, "
			"name=\"%s\", force=%d", structure, name, force);
		goto end;
	}

	field_type = bt_field_get_type(integer);
	assert(field_type);
	if (bt_field_type_get_type_id(field_type) != BT_FIELD_TYPE_ID_INTEGER) {
		/*
		 * The user most likely meant for us to populate this field
		 * automatically. However, we can only do this if the field
		 * is an integer. Return an error.
		 */
		BT_LOGW("Invalid parameter: field's type is not an integer field type: "
			"field-addr=%p, ft-addr=%p, ft-id=%s",
			integer, field_type,
			bt_field_type_id_string(field_type->id));
		ret = -1;
		goto end;
	}

	if (bt_field_type_integer_is_signed(field_type)) {
		ret = bt_field_signed_integer_set_value(integer,
			(int64_t) value);
	} else {
		ret = bt_field_unsigned_integer_set_value(integer, value);
	}
	ret = !ret ? 1 : ret;
end:
	bt_put(integer);
	bt_put(field_type);
	return ret;
}

/*
 * Returns the following codes:
 * 1 if the field was found and set,
 * 0 if nothing was done (field not found, or was already set),
 * <0 if an error was encoutered
 */
static
int try_set_structure_field_integer(struct bt_field *structure, char *name,
		uint64_t value)
{
	return _set_structure_field_integer(structure, name, value, BT_FALSE);
}

const char *bt_stream_get_name(struct bt_stream *stream)
{
	const char *name = NULL;

	if (!stream) {
		BT_LOGW_STR("Invalid parameter: stream is NULL.");
		goto end;
	}

	name = stream->name ? stream->name->str : NULL;

end:
	return name;
}

int bt_stream_is_writer(struct bt_stream *stream)
{
	int ret = -1;

	if (!stream) {
		BT_LOGW_STR("Invalid parameter: stream is NULL.");
		goto end;
	}

	ret = (stream->pos.fd >= 0);

end:
	return ret;
}

BT_HIDDEN
void bt_stream_map_component_to_port(struct bt_stream *stream,
		struct bt_component *comp,
		struct bt_port *port)
{
	assert(stream);
	assert(comp);
	assert(port);
	assert(stream->comp_cur_port);

	/*
	 * Do not take a reference to the component here because we
	 * don't want the component to exist as long as this stream
	 * exists. Instead, keep a weak reference, but add a destroy
	 * listener so that we remove this hash table entry when we know
	 * the component is destroyed.
	 */
	BT_LOGV("Adding component's destroy listener for stream: "
		"stream-addr=%p, stream-name=\"%s\", comp-addr=%p, "
		"comp-name=\"%s\", port-addr=%p, port-name=\"%s\"",
		stream, bt_stream_get_name(stream),
		comp, bt_component_get_name(comp), port,
		bt_port_get_name(port));
	bt_component_add_destroy_listener(comp, component_destroy_listener,
		stream);
	g_hash_table_insert(stream->comp_cur_port, comp, port);
	BT_LOGV_STR("Mapped component to port for stream.");
}

BT_HIDDEN
struct bt_port *bt_stream_port_for_component(struct bt_stream *stream,
		struct bt_component *comp)
{
	assert(stream);
	assert(comp);
	assert(stream->comp_cur_port);
	return g_hash_table_lookup(stream->comp_cur_port, comp);
}

BT_HIDDEN
void bt_stream_add_destroy_listener(struct bt_stream *stream,
		bt_stream_destroy_listener_func func, void *data)
{
	struct bt_stream_destroy_listener listener;

	assert(stream);
	assert(func);
	listener.func = func;
	listener.data = data;
	g_array_append_val(stream->destroy_listeners, listener);
	BT_LOGV("Added stream destroy listener: stream-addr=%p, "
		"stream-name=\"%s\", func=%p, data=%p",
		stream, bt_stream_get_name(stream), func, data);
}

BT_HIDDEN
void bt_stream_remove_destroy_listener(struct bt_stream *stream,
		bt_stream_destroy_listener_func func, void *data)
{
	size_t i;

	assert(stream);
	assert(func);

	for (i = 0; i < stream->destroy_listeners->len; i++) {
		struct bt_stream_destroy_listener *listener =
			&g_array_index(stream->destroy_listeners,
				struct bt_stream_destroy_listener, i);

		if (listener->func == func && listener->data == data) {
			g_array_remove_index(stream->destroy_listeners, i);
			i--;
			BT_LOGV("Removed stream destroy listener: stream-addr=%p, "
				"stream-name=\"%s\", func=%p, data=%p",
				stream, bt_stream_get_name(stream),
				func, data);
		}
	}
}

int64_t bt_stream_get_id(struct bt_stream *stream)
{
	int64_t ret;

	if (!stream) {
		BT_LOGW_STR("Invalid parameter: stream is NULL.");
		ret = (int64_t) -1;
		goto end;
	}

	ret = stream->id;
	if (ret < 0) {
		BT_LOGV("Stream's ID is not set: addr=%p, name=\"%s\"",
			stream, bt_stream_get_name(stream));
	}

end:
	return ret;
}
