/*
 * event.c
 *
 * Babeltrace CTF IR - Event
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

#include <babeltrace/ctf-ir/fields-internal.h>
#include <babeltrace/ctf-ir/field-types-internal.h>
#include <babeltrace/ctf-ir/event-internal.h>
#include <babeltrace/ctf-ir/event-class.h>
#include <babeltrace/ctf-ir/event-class-internal.h>
#include <babeltrace/ctf-ir/stream-class.h>
#include <babeltrace/ctf-ir/stream-class-internal.h>
#include <babeltrace/ctf-ir/stream-internal.h>
#include <babeltrace/ctf-ir/packet.h>
#include <babeltrace/ctf-ir/packet-internal.h>
#include <babeltrace/ctf-ir/trace-internal.h>
#include <babeltrace/ctf-ir/validation-internal.h>
#include <babeltrace/ctf-ir/packet-internal.h>
#include <babeltrace/ctf-ir/clock-internal.h>
#include <babeltrace/ctf-ir/utils.h>
#include <babeltrace/ref.h>
#include <babeltrace/ctf-ir/attributes-internal.h>
#include <babeltrace/compiler.h>

static
void bt_ctf_event_destroy(struct bt_object *obj);
static
int set_integer_field_value(struct bt_ctf_field *field, uint64_t value);

struct bt_ctf_event *bt_ctf_event_create(struct bt_ctf_event_class *event_class)
{
	int ret;
	enum bt_ctf_validation_flag validation_flags =
		BT_CTF_VALIDATION_FLAG_STREAM |
		BT_CTF_VALIDATION_FLAG_EVENT;
	struct bt_ctf_event *event = NULL;
	struct bt_ctf_trace *trace = NULL;
	struct bt_ctf_stream_class *stream_class = NULL;
	struct bt_ctf_field_type *packet_header_type = NULL;
	struct bt_ctf_field_type *packet_context_type = NULL;
	struct bt_ctf_field_type *event_header_type = NULL;
	struct bt_ctf_field_type *stream_event_ctx_type = NULL;
	struct bt_ctf_field_type *event_context_type = NULL;
	struct bt_ctf_field_type *event_payload_type = NULL;
	struct bt_ctf_field *event_header = NULL;
	struct bt_ctf_field *stream_event_context = NULL;
	struct bt_ctf_field *event_context = NULL;
	struct bt_ctf_field *event_payload = NULL;
	struct bt_value *environment = NULL;
	struct bt_ctf_validation_output validation_output = { 0 };
	int trace_valid = 0;

	if (!event_class) {
		goto error;
	}

	stream_class = bt_ctf_event_class_get_stream_class(event_class);

	/*
	 * We disallow the creation of an event if its event class has not been
	 * associated to a stream class.
	 */
	if (!stream_class) {
		goto error;
	}

	/* A stream class should always have an existing event header type */
	assert(stream_class->event_header_type);

	/* The event class was frozen when added to its stream class */
	assert(event_class->frozen);

	/* Validate the trace (if any), the stream class, and the event class */
	trace = bt_ctf_stream_class_get_trace(stream_class);
	if (trace) {
		packet_header_type = bt_ctf_trace_get_packet_header_type(trace);
		trace_valid = trace->valid;
		assert(trace_valid);
		environment = trace->environment;
	}

	packet_context_type = bt_ctf_stream_class_get_packet_context_type(
		stream_class);
	event_header_type = bt_ctf_stream_class_get_event_header_type(
		stream_class);
	stream_event_ctx_type = bt_ctf_stream_class_get_event_context_type(
		stream_class);
	event_context_type = bt_ctf_event_class_get_context_type(event_class);
	event_payload_type = bt_ctf_event_class_get_payload_type(event_class);
	ret = bt_ctf_validate_class_types(environment, packet_header_type,
		packet_context_type, event_header_type, stream_event_ctx_type,
		event_context_type, event_payload_type, trace_valid,
		stream_class->valid, event_class->valid,
		&validation_output, validation_flags);
	BT_PUT(packet_header_type);
	BT_PUT(packet_context_type);
	BT_PUT(event_header_type);
	BT_PUT(stream_event_ctx_type);
	BT_PUT(event_context_type);
	BT_PUT(event_payload_type);
	if (ret) {
		/*
		 * This means something went wrong during the validation
		 * process, not that the objects are invalid.
		 */
		goto error;
	}

	if ((validation_output.valid_flags & validation_flags) !=
			validation_flags) {
		/* Invalid trace/stream class/event class */
		goto error;
	}

	/*
	 * At this point we know the trace (if associated to the stream
	 * class), the stream class, and the event class, with their
	 * current types, are valid. We may proceed with creating
	 * the event.
	 */
	event = g_new0(struct bt_ctf_event, 1);
	if (!event) {
		goto error;
	}

	bt_object_init(event, bt_ctf_event_destroy);

	/*
	 * event does not share a common ancestor with the event class; it has
	 * to guarantee its existence by holding a reference. This reference
	 * shall be released once the event is associated to a stream since,
	 * from that point, the event and its class will share the same
	 * lifetime.
	 */
	event->event_class = bt_get(event_class);
	event->clock_values = g_hash_table_new_full(g_direct_hash,
		g_direct_equal, NULL, g_free);
	event_header =
		bt_ctf_field_create(validation_output.event_header_type);
	if (!event_header) {
		goto error;
	}

	if (validation_output.stream_event_ctx_type) {
		stream_event_context = bt_ctf_field_create(
			validation_output.stream_event_ctx_type);
		if (!stream_event_context) {
			goto error;
		}
	}

	if (validation_output.event_context_type) {
		event_context = bt_ctf_field_create(
			validation_output.event_context_type);
		if (!event_context) {
			goto error;
		}
	}

	if (validation_output.event_payload_type) {
		event_payload = bt_ctf_field_create(
			validation_output.event_payload_type);
		if (!event_payload) {
			goto error;
		}
	}

	/*
	 * At this point all the fields are created, potentially from
	 * validated copies of field types, so that the field types and
	 * fields can be replaced in the trace, stream class,
	 * event class, and created event.
	 */
	bt_ctf_validation_replace_types(trace, stream_class,
		event_class, &validation_output, validation_flags);
	BT_MOVE(event->event_header, event_header);
	BT_MOVE(event->stream_event_context, stream_event_context);
	BT_MOVE(event->context_payload, event_context);
	BT_MOVE(event->fields_payload, event_payload);

	/*
	 * Put what was not moved in bt_ctf_validation_replace_types().
	 */
	bt_ctf_validation_output_put_types(&validation_output);

	/*
	 * Freeze the stream class since the event header must not be changed
	 * anymore.
	 */
	bt_ctf_stream_class_freeze(stream_class);

	/*
	 * Mark stream class, and event class as valid since
	 * they're all frozen now.
	 */
	stream_class->valid = 1;
	event_class->valid = 1;

	/* Put stuff we borrowed from the event class */
	BT_PUT(stream_class);
	BT_PUT(trace);

	return event;

error:
	bt_ctf_validation_output_put_types(&validation_output);
	BT_PUT(event);
	BT_PUT(stream_class);
	BT_PUT(trace);
	BT_PUT(event_header);
	BT_PUT(stream_event_context);
	BT_PUT(event_context);
	BT_PUT(event_payload);
	assert(!packet_header_type);
	assert(!packet_context_type);
	assert(!event_header_type);
	assert(!stream_event_ctx_type);
	assert(!event_context_type);
	assert(!event_payload_type);

	return event;
}

BT_HIDDEN
struct bt_ctf_event_class *bt_ctf_event_get_class(struct bt_ctf_event *event)
{
	struct bt_ctf_event_class *event_class = NULL;

	if (!event) {
		goto end;
	}

	event_class = event->event_class;
	bt_get(event_class);
end:
	return event_class;
}

BT_HIDDEN
struct bt_ctf_stream *bt_ctf_event_get_stream(struct bt_ctf_event *event)
{
	struct bt_ctf_stream *stream = NULL;

	if (!event) {
		goto end;
	}

	/*
	 * If the event has a parent, then this is its (writer) stream.
	 * If the event has no parent, then if it has a packet, this
	 * is its (non-writer) stream.
	 */
	if (event->base.parent) {
		stream = (struct bt_ctf_stream *) bt_object_get_parent(event);
	} else {
		if (event->packet) {
			stream = bt_get(event->packet->stream);
		}
	}

end:
	return stream;
}

BT_HIDDEN
struct bt_ctf_clock *bt_ctf_event_get_clock(struct bt_ctf_event *event)
{
	struct bt_ctf_clock *clock = NULL;
	struct bt_ctf_event_class *event_class;
	struct bt_ctf_stream_class *stream_class;

	if (!event) {
		goto end;
	}

	event_class = bt_ctf_event_get_class(event);
	if (!event_class) {
		goto end;
	}

	stream_class = bt_ctf_event_class_get_stream_class(event_class);
	if (!stream_class) {
		goto error_put_event_class;
	}

	clock = bt_ctf_stream_class_get_clock(stream_class);
	if (!clock) {
		goto error_put_stream_class;
	}

error_put_stream_class:
	bt_put(stream_class);
error_put_event_class:
	bt_put(event_class);
end:
	return clock;
}

int bt_ctf_event_set_payload(struct bt_ctf_event *event,
		const char *name,
		struct bt_ctf_field *payload)
{
	int ret = 0;

	if (!event || !payload || event->frozen) {
		ret = -1;
		goto end;
	}

	if (name) {
		ret = bt_ctf_field_structure_set_field(event->fields_payload,
			name, payload);
	} else {
		struct bt_ctf_field_type *payload_type;

		payload_type = bt_ctf_field_get_type(payload);

		if (bt_ctf_field_type_compare(payload_type,
				event->event_class->fields) == 0) {
			bt_put(event->fields_payload);
			bt_get(payload);
			event->fields_payload = payload;
		} else {
			ret = -1;
		}

		bt_put(payload_type);
	}
end:
	return ret;
}

BT_HIDDEN
struct bt_ctf_field *bt_ctf_event_get_payload_field(struct bt_ctf_event *event)
{
	struct bt_ctf_field *payload = NULL;

	if (!event || !event->fields_payload) {
		goto end;
	}

	payload = event->fields_payload;
	bt_get(payload);
end:
	return payload;
}

BT_HIDDEN
int bt_ctf_event_set_payload_field(struct bt_ctf_event *event,
		struct bt_ctf_field *payload)
{
	int ret = 0;
	struct bt_ctf_field_type *payload_type = NULL;

	if (!event || !payload || event->frozen) {
		ret = -1;
		goto end;
	}

	payload_type = bt_ctf_field_get_type(payload);
	if (!payload_type) {
		ret = -1;
		goto end;
	}

	if (bt_ctf_field_type_get_type_id(payload_type) !=
			CTF_TYPE_STRUCT) {
		ret = -1;
		goto end;
	}

	bt_get(payload);
	bt_put(event->fields_payload);
	event->fields_payload = payload;

end:
	bt_put(payload_type);
	return ret;
}

struct bt_ctf_field *bt_ctf_event_get_payload(struct bt_ctf_event *event,
		const char *name)
{
	struct bt_ctf_field *field = NULL;

	if (!event) {
		goto end;
	}

	if (name) {
		field = bt_ctf_field_structure_get_field(event->fields_payload,
			name);
	} else {
		field = event->fields_payload;
		bt_get(field);
	}
end:
	return field;
}

BT_HIDDEN
struct bt_ctf_field *bt_ctf_event_get_payload_by_index(
		struct bt_ctf_event *event, int index)
{
	struct bt_ctf_field *field = NULL;

	if (!event || index < 0) {
		goto end;
	}

	field = bt_ctf_field_structure_get_field_by_index(event->fields_payload,
		index);
end:
	return field;
}

BT_HIDDEN
struct bt_ctf_field *bt_ctf_event_get_header(
		struct bt_ctf_event *event)
{
	struct bt_ctf_field *header = NULL;

	if (!event || !event->event_header) {
		goto end;
	}

	header = event->event_header;
	bt_get(header);
end:
	return header;
}

BT_HIDDEN
int bt_ctf_event_set_header(struct bt_ctf_event *event,
		struct bt_ctf_field *header)
{
	int ret = 0;
	struct bt_ctf_field_type *field_type = NULL;
	struct bt_ctf_stream_class *stream_class = NULL;

	if (!event || !header || event->frozen) {
		ret = -1;
		goto end;
	}

	stream_class = (struct bt_ctf_stream_class *) bt_object_get_parent(
		event->event_class);
	/*
	 * Ensure the provided header's type matches the one registered to the
	 * stream class.
	 */
	field_type = bt_ctf_field_get_type(header);
	if (bt_ctf_field_type_compare(field_type,
			stream_class->event_header_type)) {
		ret = -1;
		goto end;
	}

	bt_get(header);
	bt_put(event->event_header);
	event->event_header = header;
end:
	bt_put(stream_class);
	bt_put(field_type);
	return ret;
}

BT_HIDDEN
struct bt_ctf_field *bt_ctf_event_get_event_context(
		struct bt_ctf_event *event)
{
	struct bt_ctf_field *context = NULL;

	if (!event || !event->context_payload) {
		goto end;
	}

	context = event->context_payload;
	bt_get(context);
end:
	return context;
}

BT_HIDDEN
int bt_ctf_event_set_event_context(struct bt_ctf_event *event,
		struct bt_ctf_field *context)
{
	int ret = 0;
	struct bt_ctf_field_type *field_type = NULL;

	if (!event || !context || event->frozen) {
		ret = -1;
		goto end;
	}

	field_type = bt_ctf_field_get_type(context);
	if (bt_ctf_field_type_compare(field_type,
			event->event_class->context)) {
		ret = -1;
		goto end;
	}

	bt_get(context);
	bt_put(event->context_payload);
	event->context_payload = context;
end:
	bt_put(field_type);
	return ret;
}

BT_HIDDEN
struct bt_ctf_field *bt_ctf_event_get_stream_event_context(
		struct bt_ctf_event *event)
{
	struct bt_ctf_field *stream_event_context = NULL;

	if (!event || !event->stream_event_context) {
		goto end;
	}

	stream_event_context = event->stream_event_context;
end:
	return bt_get(stream_event_context);
}

BT_HIDDEN
int bt_ctf_event_set_stream_event_context(struct bt_ctf_event *event,
		struct bt_ctf_field *stream_event_context)
{
	int ret = 0;
	struct bt_ctf_field_type *field_type = NULL;
	struct bt_ctf_stream_class *stream_class = NULL;

	if (!event || !stream_event_context || event->frozen) {
		ret = -1;
		goto end;
	}

	stream_class = bt_ctf_event_class_get_stream_class(event->event_class);
	/*
	 * We should not have been able to create the event without associating
	 * the event class to a stream class.
	 */
	assert(stream_class);

	field_type = bt_ctf_field_get_type(stream_event_context);
	if (bt_ctf_field_type_compare(field_type,
			stream_class->event_context_type)) {
		ret = -1;
		goto end;
	}

	bt_get(stream_event_context);
	BT_MOVE(event->stream_event_context, stream_event_context);
end:
	BT_PUT(stream_class);
	bt_put(field_type);
	return ret;
}

void bt_ctf_event_get(struct bt_ctf_event *event)
{
	bt_get(event);
}

void bt_ctf_event_put(struct bt_ctf_event *event)
{
	bt_put(event);
}

void bt_ctf_event_destroy(struct bt_object *obj)
{
	struct bt_ctf_event *event;

	event = container_of(obj, struct bt_ctf_event, base);
	if (!event->base.parent) {
		/*
		 * Event was keeping a reference to its class since it shared no
		 * common ancestor with it to guarantee they would both have the
		 * same lifetime.
		 */
		bt_put(event->event_class);
	}
	g_hash_table_destroy(event->clock_values);
	bt_put(event->event_header);
	bt_put(event->stream_event_context);
	bt_put(event->context_payload);
	bt_put(event->fields_payload);
	bt_put(event->packet);
	g_free(event);
}

static
int set_integer_field_value(struct bt_ctf_field* field, uint64_t value)
{
	int ret = 0;
	struct bt_ctf_field_type *field_type = NULL;

	if (!field) {
		ret = -1;
		goto end;
	}

	if (!bt_ctf_field_validate(field)) {
		/* Payload already set, skip! (not an error) */
		goto end;
	}

	field_type = bt_ctf_field_get_type(field);
	assert(field_type);

	if (bt_ctf_field_type_get_type_id(field_type) !=
			CTF_TYPE_INTEGER) {
		/* Not an integer and the value is unset, error. */
		ret = -1;
		goto end;
	}

	if (bt_ctf_field_type_integer_get_signed(field_type)) {
		ret = bt_ctf_field_signed_integer_set_value(field, (int64_t) value);
		if (ret) {
			/* Value is out of range, error. */
			goto end;
		}
	} else {
		ret = bt_ctf_field_unsigned_integer_set_value(field, value);
		if (ret) {
			/* Value is out of range, error. */
			goto end;
		}
	}
end:
	bt_put(field_type);
	return ret;
}

BT_HIDDEN
int bt_ctf_event_validate(struct bt_ctf_event *event)
{
	/* Make sure each field's payload has been set */
	int ret;
	struct bt_ctf_stream_class *stream_class = NULL;

	assert(event);
	ret = bt_ctf_field_validate(event->event_header);
	if (ret) {
		goto end;
	}

	stream_class = bt_ctf_event_class_get_stream_class(event->event_class);
	/*
	 * We should not have been able to create the event without associating
	 * the event class to a stream class.
	 */
	assert(stream_class);
	if (stream_class->event_context_type) {
		ret = bt_ctf_field_validate(event->stream_event_context);
		if (ret) {
			goto end;
		}
	}

	ret = bt_ctf_field_validate(event->fields_payload);
	if (ret) {
		goto end;
	}

	if (event->event_class->context) {
		ret = bt_ctf_field_validate(event->context_payload);
	}
end:
	bt_put(stream_class);
	return ret;
}

BT_HIDDEN
int bt_ctf_event_serialize(struct bt_ctf_event *event,
		struct ctf_stream_pos *pos)
{
	int ret = 0;

	assert(event);
	assert(pos);
	if (event->context_payload) {
		ret = bt_ctf_field_serialize(event->context_payload, pos);
		if (ret) {
			goto end;
		}
	}

	if (event->fields_payload) {
		ret = bt_ctf_field_serialize(event->fields_payload, pos);
		if (ret) {
			goto end;
		}
	}
end:
	return ret;
}

BT_HIDDEN
int bt_ctf_event_populate_event_header(struct bt_ctf_event *event)
{
	int ret = 0;
	struct bt_ctf_field *id_field = NULL, *timestamp_field = NULL;

	if (!event || event->frozen) {
		ret = -1;
		goto end;
	}

	id_field = bt_ctf_field_structure_get_field(event->event_header, "id");
	if (id_field) {
		ret = set_integer_field_value(id_field,
			(uint64_t) bt_ctf_event_class_get_id(
				event->event_class));
		if (ret) {
			goto end;
		}
	}

	timestamp_field = bt_ctf_field_structure_get_field(event->event_header,
		"timestamp");
	if (timestamp_field) {
		struct bt_ctf_field_type *timestamp_field_type =
			bt_ctf_field_get_type(timestamp_field);
		struct bt_ctf_clock *mapped_clock;

		assert(timestamp_field_type);
		mapped_clock = bt_ctf_field_type_integer_get_mapped_clock(
			timestamp_field_type);
		bt_put(timestamp_field_type);
		if (mapped_clock) {
			int64_t timestamp;

			ret = bt_ctf_clock_get_time(mapped_clock, &timestamp);
			bt_put(mapped_clock);
			if (ret) {
				goto end;
			}

			ret = set_integer_field_value(timestamp_field,
				timestamp);
			if (ret) {
				goto end;
			}
		}
	}
end:
	bt_put(id_field);
	bt_put(timestamp_field);
	return ret;
}

BT_HIDDEN
int bt_ctf_event_set_packet(struct bt_ctf_event *event,
		struct bt_ctf_packet *packet)
{
	struct bt_ctf_stream_class *event_stream_class = NULL;
	struct bt_ctf_stream_class *packet_stream_class = NULL;
	struct bt_ctf_stream *stream = NULL;
	int ret = 0;

	if (!event || !packet || event->frozen) {
		ret = -1;
		goto end;
	}

	/*
	 * Make sure the new packet was created by this event's
	 * stream, if it is set.
	 */
	stream = bt_ctf_event_get_stream(event);
	if (stream) {
		if (packet->stream != stream) {
			ret = -1;
			goto end;
		}
	} else {
		event_stream_class =
			bt_ctf_event_class_get_stream_class(event->event_class);
		packet_stream_class =
			bt_ctf_stream_get_class(packet->stream);

		assert(event_stream_class);
		assert(packet_stream_class);

		if (event_stream_class != packet_stream_class) {
			ret = -1;
			goto end;
		}
	}

	bt_get(packet);
	BT_MOVE(event->packet, packet);

end:
	BT_PUT(stream);
	BT_PUT(event_stream_class);
	BT_PUT(packet_stream_class);

	return ret;
}

BT_HIDDEN
void bt_ctf_event_freeze(struct bt_ctf_event *event)
{
	assert(event);
	bt_ctf_packet_freeze(event->packet);
	bt_ctf_field_freeze(event->event_header);
	bt_ctf_field_freeze(event->stream_event_context);
	bt_ctf_field_freeze(event->context_payload);
	bt_ctf_field_freeze(event->fields_payload);
	event->frozen = 1;
}

static
void insert_stream_clock_value_into_event_clock_values(gpointer key,
		gpointer value,
		gpointer data)
{
	struct bt_ctf_event *event = data;
	uint64_t *clock_value;

	assert(event);

	/* Copy clock value because it belongs to the hash table */
	clock_value = g_new0(uint64_t, 1);
	*clock_value = *((uint64_t *) value);

	/* Insert copy into event clock values */
	g_hash_table_insert(event->clock_values, key, clock_value);
}

BT_HIDDEN
int bt_ctf_event_register_stream_clock_values(struct bt_ctf_event *event)
{
	int ret = 0;
	struct bt_ctf_stream *stream;

	stream = bt_ctf_event_get_stream(event);
	assert(stream);
	g_hash_table_remove_all(event->clock_values);
	g_hash_table_foreach(stream->clock_values,
		insert_stream_clock_value_into_event_clock_values, event);
	BT_PUT(stream);

	return ret;
}

BT_HIDDEN
uint64_t bt_ctf_event_get_clock_value(struct bt_ctf_event *event,
		struct bt_ctf_clock *clock)
{
	uint64_t ret = -1ULL;
	uint64_t *clock_value;

	if (!event || !clock) {
		goto end;
	}

	clock_value = g_hash_table_lookup(event->clock_values, clock);
	if (!clock_value) {
		goto end;
	}

	ret = *clock_value;

end:
	return ret;
}
