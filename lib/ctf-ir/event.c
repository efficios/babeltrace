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

#define BT_LOG_TAG "EVENT"
#include <babeltrace/lib-logging-internal.h>

#include <babeltrace/ctf-ir/fields-internal.h>
#include <babeltrace/ctf-ir/field-types-internal.h>
#include <babeltrace/ctf-ir/clock-class.h>
#include <babeltrace/ctf-ir/clock-value.h>
#include <babeltrace/ctf-ir/clock-value-internal.h>
#include <babeltrace/ctf-ir/clock-class-internal.h>
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
#include <babeltrace/ctf-ir/utils.h>
#include <babeltrace/ctf-writer/serialize-internal.h>
#include <babeltrace/ctf-writer/clock-internal.h>
#include <babeltrace/ref.h>
#include <babeltrace/ctf-ir/attributes-internal.h>
#include <babeltrace/compiler-internal.h>
#include <inttypes.h>

static
void bt_event_destroy(struct bt_object *obj);

struct bt_event *bt_event_create(struct bt_event_class *event_class)
{
	int ret;
	enum bt_validation_flag validation_flags =
		BT_VALIDATION_FLAG_STREAM |
		BT_VALIDATION_FLAG_EVENT;
	struct bt_event *event = NULL;
	struct bt_trace *trace = NULL;
	struct bt_stream_class *stream_class = NULL;
	struct bt_field_type *packet_header_type = NULL;
	struct bt_field_type *packet_context_type = NULL;
	struct bt_field_type *event_header_type = NULL;
	struct bt_field_type *stream_event_ctx_type = NULL;
	struct bt_field_type *event_context_type = NULL;
	struct bt_field_type *event_payload_type = NULL;
	struct bt_field *event_header = NULL;
	struct bt_field *stream_event_context = NULL;
	struct bt_field *event_context = NULL;
	struct bt_field *event_payload = NULL;
	struct bt_value *environment = NULL;
	struct bt_validation_output validation_output = { 0 };
	int trace_valid = 0;
	struct bt_clock_class *expected_clock_class = NULL;

	BT_LOGD("Creating event object: event-class-addr=%p, "
		"event-class-name=\"%s\", event-class-id=%" PRId64,
		event_class, bt_event_class_get_name(event_class),
		bt_event_class_get_id(event_class));

	if (!event_class) {
		BT_LOGW_STR("Invalid parameter: event class is NULL.");
		goto error;
	}

	stream_class = bt_event_class_get_stream_class(event_class);

	/*
	 * We disallow the creation of an event if its event class has not been
	 * associated to a stream class.
	 */
	if (!stream_class) {
		BT_LOGW_STR("Event class is not part of a stream class.");
		goto error;
	}

	/* The event class was frozen when added to its stream class */
	assert(event_class->frozen);

	if (!stream_class->frozen) {
		if (stream_class->clock) {
			expected_clock_class =
				bt_get(stream_class->clock->clock_class);
		}

		/*
		 * Because this function freezes the stream class,
		 * validate that this stream class contains at most a
		 * single clock class so that we set its expected clock
		 * class for future checks.
		 */
		ret = bt_stream_class_validate_single_clock_class(
			stream_class, &expected_clock_class);
		if (ret) {
			BT_LOGW("Event class's stream class or one of its event "
				"classes contains a field type which is not "
				"recursively mapped to the expected "
				"clock class: "
				"stream-class-addr=%p, "
				"stream-class-id=%" PRId64 ", "
				"stream-class-name=\"%s\", "
				"expected-clock-class-addr=%p, "
				"expected-clock-class-name=\"%s\"",
				stream_class, bt_stream_class_get_id(stream_class),
				bt_stream_class_get_name(stream_class),
				expected_clock_class,
				expected_clock_class ?
					bt_clock_class_get_name(expected_clock_class) :
					NULL);
			goto error;
		}
	}

	/* Validate the trace (if any), the stream class, and the event class */
	trace = bt_stream_class_get_trace(stream_class);
	if (trace) {
		BT_LOGD_STR("Event's class is part of a trace.");
		packet_header_type = bt_trace_get_packet_header_type(trace);
		trace_valid = trace->valid;
		assert(trace_valid);
		environment = trace->environment;
	}

	packet_context_type = bt_stream_class_get_packet_context_type(
		stream_class);
	event_header_type = bt_stream_class_get_event_header_type(
		stream_class);
	stream_event_ctx_type = bt_stream_class_get_event_context_type(
		stream_class);
	event_context_type = bt_event_class_get_context_type(event_class);
	event_payload_type = bt_event_class_get_payload_type(event_class);
	ret = bt_validate_class_types(environment, packet_header_type,
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
		BT_LOGE("Failed to validate event and parents: ret=%d", ret);
		goto error;
	}

	if ((validation_output.valid_flags & validation_flags) !=
			validation_flags) {
		/* Invalid trace/stream class/event class */
		BT_LOGW("Invalid trace, stream class, or event class: "
			"valid-flags=0x%x", validation_output.valid_flags);
		goto error;
	}

	/*
	 * Safe to automatically map selected fields to the stream's
	 * clock's class here because the stream class is about to be
	 * frozen.
	 */
	if (bt_stream_class_map_clock_class(stream_class,
			validation_output.packet_context_type,
			validation_output.event_header_type)) {
		BT_LOGW_STR("Cannot automatically map selected stream class's "
			"field types to stream class's clock's class.");
		goto error;
	}

	/*
	 * At this point we know the trace (if associated to the stream
	 * class), the stream class, and the event class, with their
	 * current types, are valid. We may proceed with creating
	 * the event.
	 */
	event = g_new0(struct bt_event, 1);
	if (!event) {
		BT_LOGE_STR("Failed to allocate one event.");
		goto error;
	}

	bt_object_init(event, bt_event_destroy);

	/*
	 * event does not share a common ancestor with the event class; it has
	 * to guarantee its existence by holding a reference. This reference
	 * shall be released once the event is associated to a stream since,
	 * from that point, the event and its class will share the same
	 * lifetime.
	 */
	event->event_class = bt_get(event_class);
	event->clock_values = g_hash_table_new_full(g_direct_hash,
			g_direct_equal, bt_put, bt_put);

	if (validation_output.event_header_type) {
		BT_LOGD("Creating initial event header field: ft-addr=%p",
			validation_output.event_header_type);
		event_header =
			bt_field_create(validation_output.event_header_type);
		if (!event_header) {
			BT_LOGE_STR("Cannot create initial event header field object.");
			goto error;
		}
	}

	if (validation_output.stream_event_ctx_type) {
		BT_LOGD("Creating initial stream event context field: ft-addr=%p",
			validation_output.stream_event_ctx_type);
		stream_event_context = bt_field_create(
			validation_output.stream_event_ctx_type);
		if (!stream_event_context) {
			BT_LOGE_STR("Cannot create initial stream event context field object.");
			goto error;
		}
	}

	if (validation_output.event_context_type) {
		BT_LOGD("Creating initial event context field: ft-addr=%p",
			validation_output.event_context_type);
		event_context = bt_field_create(
			validation_output.event_context_type);
		if (!event_context) {
			BT_LOGE_STR("Cannot create initial event context field object.");
			goto error;
		}
	}

	if (validation_output.event_payload_type) {
		BT_LOGD("Creating initial event payload field: ft-addr=%p",
			validation_output.event_payload_type);
		event_payload = bt_field_create(
			validation_output.event_payload_type);
		if (!event_payload) {
			BT_LOGE_STR("Cannot create initial event payload field object.");
			goto error;
		}
	}

	/*
	 * At this point all the fields are created, potentially from
	 * validated copies of field types, so that the field types and
	 * fields can be replaced in the trace, stream class,
	 * event class, and created event.
	 */
	bt_validation_replace_types(trace, stream_class,
		event_class, &validation_output, validation_flags);
	BT_MOVE(event->event_header, event_header);
	BT_MOVE(event->stream_event_context, stream_event_context);
	BT_MOVE(event->context_payload, event_context);
	BT_MOVE(event->fields_payload, event_payload);

	/*
	 * Put what was not moved in bt_validation_replace_types().
	 */
	bt_validation_output_put_types(&validation_output);

	/*
	 * Freeze the stream class since the event header must not be changed
	 * anymore.
	 */
	bt_stream_class_freeze(stream_class);

	/*
	 * It is safe to set the stream class's unique clock class
	 * now because the stream class is frozen.
	 */
	if (expected_clock_class) {
		BT_MOVE(stream_class->clock_class, expected_clock_class);
	}

	/*
	 * Mark stream class, and event class as valid since
	 * they're all frozen now.
	 */
	stream_class->valid = 1;
	event_class->valid = 1;

	/* Put stuff we borrowed from the event class */
	BT_PUT(stream_class);
	BT_PUT(trace);
	BT_LOGD("Created event object: addr=%p, event-class-name=\"%s\", "
		"event-class-id=%" PRId64,
		event, bt_event_class_get_name(event->event_class),
		bt_event_class_get_id(event_class));
	return event;

error:
	bt_validation_output_put_types(&validation_output);
	BT_PUT(event);
	BT_PUT(stream_class);
	BT_PUT(trace);
	BT_PUT(event_header);
	BT_PUT(stream_event_context);
	BT_PUT(event_context);
	BT_PUT(event_payload);
	bt_put(expected_clock_class);
	assert(!packet_header_type);
	assert(!packet_context_type);
	assert(!event_header_type);
	assert(!stream_event_ctx_type);
	assert(!event_context_type);
	assert(!event_payload_type);

	return event;
}

struct bt_event_class *bt_event_get_class(struct bt_event *event)
{
	struct bt_event_class *event_class = NULL;

	if (!event) {
		BT_LOGW_STR("Invalid parameter: event is NULL.");
		goto end;
	}

	event_class = bt_get(bt_event_borrow_event_class(event));
end:
	return event_class;
}

struct bt_stream *bt_event_get_stream(struct bt_event *event)
{
	struct bt_stream *stream = NULL;

	if (!event) {
		BT_LOGW_STR("Invalid parameter: event is NULL.");
		goto end;
	}

	/*
	 * If the event has a parent, then this is its (writer) stream.
	 * If the event has no parent, then if it has a packet, this
	 * is its (non-writer) stream.
	 */
	if (event->base.parent) {
		stream = (struct bt_stream *) bt_object_get_parent(event);
	} else {
		if (event->packet) {
			stream = bt_get(event->packet->stream);
		}
	}

end:
	return stream;
}

int bt_event_set_payload(struct bt_event *event,
		const char *name,
		struct bt_field *payload)
{
	int ret = 0;

	if (!event || !payload) {
		BT_LOGW("Invalid parameter: event or payload field is NULL: "
			"event-addr=%p, payload-field-addr=%p",
			event, payload);
		ret = -1;
		goto end;
	}

	if (event->frozen) {
		BT_LOGW("Invalid parameter: event is frozen: addr=%p, "
			"event-class-name=\"%s\", event-class-id=%" PRId64,
			event, bt_event_class_get_name(event->event_class),
			bt_event_class_get_id(event->event_class));
		ret = -1;
		goto end;
	}

	if (name) {
		ret = bt_field_structure_set_field_by_name(
			event->fields_payload, name, payload);
	} else {
		struct bt_field_type *payload_type;

		payload_type = bt_field_get_type(payload);

		if (bt_field_type_compare(payload_type,
				event->event_class->fields) == 0) {
			bt_put(event->fields_payload);
			bt_get(payload);
			event->fields_payload = payload;
		} else {
			BT_LOGW("Invalid parameter: payload field type is different from the expected field type: "
				"event-addr=%p, event-class-name=\"%s\", "
				"event-class-id=%" PRId64,
				event,
				bt_event_class_get_name(event->event_class),
				bt_event_class_get_id(event->event_class));
			ret = -1;
		}

		bt_put(payload_type);
	}

	if (ret) {
		BT_LOGW("Failed to set event's payload field: event-addr=%p, "
			"event-class-name=\"%s\", event-class-id=%" PRId64 ", "
			"payload-field-name=\"%s\", payload-field-addr=%p",
			event, bt_event_class_get_name(event->event_class),
			bt_event_class_get_id(event->event_class),
			name, payload);
	} else {
		BT_LOGV("Set event's payload field: event-addr=%p, "
			"event-class-name=\"%s\", event-class-id=%" PRId64 ", "
			"payload-field-name=\"%s\", payload-field-addr=%p",
			event, bt_event_class_get_name(event->event_class),
			bt_event_class_get_id(event->event_class),
			name, payload);
	}

end:
	return ret;
}

struct bt_field *bt_event_get_event_payload(struct bt_event *event)
{
	struct bt_field *payload = NULL;

	if (!event) {
		BT_LOGW_STR("Invalid parameter: event is NULL.");
		goto end;
	}

	if (!event->fields_payload) {
		BT_LOGV("Event has no current payload field: addr=%p, "
			"event-class-name=\"%s\", event-class-id=%" PRId64,
			event, bt_event_class_get_name(event->event_class),
			bt_event_class_get_id(event->event_class));
		goto end;
	}

	payload = event->fields_payload;
	bt_get(payload);
end:
	return payload;
}

int bt_event_set_event_payload(struct bt_event *event,
		struct bt_field *payload)
{
	return bt_event_set_payload(event, NULL, payload);
}

struct bt_field *bt_event_get_payload(struct bt_event *event,
		const char *name)
{
	struct bt_field *field = NULL;

	if (!event) {
		BT_LOGW_STR("Invalid parameter: event is NULL.");
		goto end;
	}

	if (name) {
		field = bt_field_structure_get_field_by_name(
			event->fields_payload, name);
	} else {
		field = event->fields_payload;
		bt_get(field);
	}
end:
	return field;
}

struct bt_field *bt_event_get_payload_by_index(
		struct bt_event *event, uint64_t index)
{
	struct bt_field *field = NULL;

	if (!event) {
		BT_LOGW_STR("Invalid parameter: event is NULL.");
		goto end;
	}

	field = bt_field_structure_get_field_by_index(event->fields_payload,
		index);
end:
	return field;
}

struct bt_field *bt_event_get_header(
		struct bt_event *event)
{
	struct bt_field *header = NULL;

	if (!event) {
		BT_LOGW_STR("Invalid parameter: event is NULL.");
		goto end;
	}

	if (!event->event_header) {
		BT_LOGV("Event has no current header field: addr=%p, "
			"event-class-name=\"%s\", event-class-id=%" PRId64,
			event, bt_event_class_get_name(event->event_class),
			bt_event_class_get_id(event->event_class));
		goto end;
	}

	header = event->event_header;
	bt_get(header);
end:
	return header;
}

int bt_event_set_header(struct bt_event *event,
		struct bt_field *header)
{
	int ret = 0;
	struct bt_field_type *field_type = NULL;
	struct bt_stream_class *stream_class = NULL;

	if (!event) {
		BT_LOGW_STR("Invalid parameter: event is NULL.");
		ret = -1;
		goto end;
	}

	if (event->frozen) {
		BT_LOGW("Invalid parameter: event is frozen: addr=%p, "
			"event-class-name=\"%s\", event-class-id=%" PRId64,
			event, bt_event_class_get_name(event->event_class),
			bt_event_class_get_id(event->event_class));
		ret = -1;
		goto end;
	}

	stream_class = (struct bt_stream_class *) bt_object_get_parent(
			event->event_class);
	/*
	 * Ensure the provided header's type matches the one registered to the
	 * stream class.
	 */
	if (header) {
		field_type = bt_field_get_type(header);
		if (bt_field_type_compare(field_type,
				stream_class->event_header_type)) {
			BT_LOGW("Invalid parameter: header field type is different from the expected field type: "
				"event-addr=%p, event-class-name=\"%s\", "
				"event-class-id=%" PRId64,
				event,
				bt_event_class_get_name(event->event_class),
				bt_event_class_get_id(event->event_class));
			ret = -1;
			goto end;
		}
	} else {
		if (stream_class->event_header_type) {
			BT_LOGW("Invalid parameter: setting no event header but event header field type is not NULL: "
				"event-addr=%p, event-class-name=\"%s\", "
				"event-class-id=%" PRId64 ", "
				"event-header-ft-addr=%p",
				event,
				bt_event_class_get_name(event->event_class),
				bt_event_class_get_id(event->event_class),
				stream_class->event_header_type);
			ret = -1;
			goto end;
		}
	}

	bt_put(event->event_header);
	event->event_header = bt_get(header);
	BT_LOGV("Set event's header field: event-addr=%p, "
		"event-class-name=\"%s\", event-class-id=%" PRId64 ", "
		"header-field-addr=%p",
		event, bt_event_class_get_name(event->event_class),
		bt_event_class_get_id(event->event_class), header);
end:
	bt_put(stream_class);
	bt_put(field_type);
	return ret;
}

struct bt_field *bt_event_get_event_context(
		struct bt_event *event)
{
	struct bt_field *context = NULL;

	if (!event) {
		BT_LOGW_STR("Invalid parameter: event is NULL.");
		goto end;
	}

	if (!event->context_payload) {
		BT_LOGV("Event has no current context field: addr=%p, "
			"event-class-name=\"%s\", event-class-id=%" PRId64,
			event, bt_event_class_get_name(event->event_class),
			bt_event_class_get_id(event->event_class));
		goto end;
	}

	context = event->context_payload;
	bt_get(context);
end:
	return context;
}

int bt_event_set_event_context(struct bt_event *event,
		struct bt_field *context)
{
	int ret = 0;
	struct bt_field_type *field_type = NULL;

	if (!event) {
		BT_LOGW_STR("Invalid parameter: event is NULL.");
		ret = -1;
		goto end;
	}

	if (event->frozen) {
		BT_LOGW("Invalid parameter: event is frozen: addr=%p, "
			"event-class-name=\"%s\", event-class-id=%" PRId64,
			event, bt_event_class_get_name(event->event_class),
			bt_event_class_get_id(event->event_class));
		ret = -1;
		goto end;
	}

	if (context) {
		field_type = bt_field_get_type(context);

		if (bt_field_type_compare(field_type,
				event->event_class->context)) {
			BT_LOGW("Invalid parameter: context field type is different from the expected field type: "
				"event-addr=%p, event-class-name=\"%s\", "
				"event-class-id=%" PRId64,
				event,
				bt_event_class_get_name(event->event_class),
				bt_event_class_get_id(event->event_class));
			ret = -1;
			goto end;
		}
	} else {
		if (event->event_class->context) {
			BT_LOGW("Invalid parameter: setting no event context but event context field type is not NULL: "
				"event-addr=%p, event-class-name=\"%s\", "
				"event-class-id=%" PRId64 ", "
				"event-context-ft-addr=%p",
				event,
				bt_event_class_get_name(event->event_class),
				bt_event_class_get_id(event->event_class),
				event->event_class->context);
			ret = -1;
			goto end;
		}
	}

	bt_put(event->context_payload);
	event->context_payload = bt_get(context);
	BT_LOGV("Set event's context field: event-addr=%p, "
		"event-class-name=\"%s\", event-class-id=%" PRId64 ", "
		"context-field-addr=%p",
		event, bt_event_class_get_name(event->event_class),
		bt_event_class_get_id(event->event_class), context);
end:
	bt_put(field_type);
	return ret;
}

struct bt_field *bt_event_get_stream_event_context(
		struct bt_event *event)
{
	struct bt_field *stream_event_context = NULL;

	if (!event) {
		BT_LOGW_STR("Invalid parameter: event is NULL.");
		goto end;
	}

	if (!event->stream_event_context) {
		BT_LOGV("Event has no current stream event context field: addr=%p, "
			"event-class-name=\"%s\", event-class-id=%" PRId64,
			event, bt_event_class_get_name(event->event_class),
			bt_event_class_get_id(event->event_class));
		goto end;
	}

	stream_event_context = event->stream_event_context;
end:
	return bt_get(stream_event_context);
}

int bt_event_set_stream_event_context(struct bt_event *event,
		struct bt_field *stream_event_context)
{
	int ret = 0;
	struct bt_field_type *field_type = NULL;
	struct bt_stream_class *stream_class = NULL;

	if (!event) {
		BT_LOGW_STR("Invalid parameter: event is NULL.");
		ret = -1;
		goto end;
	}

	if (event->frozen) {
		BT_LOGW("Invalid parameter: event is frozen: addr=%p, "
			"event-class-name=\"%s\", event-class-id=%" PRId64,
			event, bt_event_class_get_name(event->event_class),
			bt_event_class_get_id(event->event_class));
		ret = -1;
		goto end;
	}

	stream_class = bt_event_class_get_stream_class(event->event_class);
	/*
	 * We should not have been able to create the event without associating
	 * the event class to a stream class.
	 */
	assert(stream_class);

	if (stream_event_context) {
		field_type = bt_field_get_type(stream_event_context);
		if (bt_field_type_compare(field_type,
				stream_class->event_context_type)) {
			BT_LOGW("Invalid parameter: stream event context field type is different from the expected field type: "
				"event-addr=%p, event-class-name=\"%s\", "
				"event-class-id=%" PRId64,
				event,
				bt_event_class_get_name(event->event_class),
				bt_event_class_get_id(event->event_class));
			ret = -1;
			goto end;
		}
	} else {
		if (stream_class->event_context_type) {
			BT_LOGW("Invalid parameter: setting no stream event context but stream event context field type is not NULL: "
				"event-addr=%p, event-class-name=\"%s\", "
				"event-class-id=%" PRId64 ", "
				"stream-event-context-ft-addr=%p",
				event,
				bt_event_class_get_name(event->event_class),
				bt_event_class_get_id(event->event_class),
				stream_class->event_context_type);
			ret = -1;
			goto end;
		}
	}

	bt_get(stream_event_context);
	BT_MOVE(event->stream_event_context, stream_event_context);
	BT_LOGV("Set event's stream event context field: event-addr=%p, "
		"event-class-name=\"%s\", event-class-id=%" PRId64 ", "
		"stream-event-context-field-addr=%p",
		event, bt_event_class_get_name(event->event_class),
		bt_event_class_get_id(event->event_class),
		stream_event_context);
end:
	BT_PUT(stream_class);
	bt_put(field_type);
	return ret;
}

/* Pre-2.0 CTF writer backward compatibility */
void bt_ctf_event_get(struct bt_event *event)
{
	bt_get(event);
}

/* Pre-2.0 CTF writer backward compatibility */
void bt_ctf_event_put(struct bt_event *event)
{
	bt_put(event);
}

void bt_event_destroy(struct bt_object *obj)
{
	struct bt_event *event;

	event = container_of(obj, struct bt_event, base);
	BT_LOGD("Destroying event: addr=%p, "
		"event-class-name=\"%s\", event-class-id=%" PRId64,
		event, bt_event_class_get_name(event->event_class),
		bt_event_class_get_id(event->event_class));

	if (!event->base.parent) {
		/*
		 * Event was keeping a reference to its class since it shared no
		 * common ancestor with it to guarantee they would both have the
		 * same lifetime.
		 */
		bt_put(event->event_class);
	}
	g_hash_table_destroy(event->clock_values);
	BT_LOGD_STR("Putting event's header field.");
	bt_put(event->event_header);
	BT_LOGD_STR("Putting event's stream event context field.");
	bt_put(event->stream_event_context);
	BT_LOGD_STR("Putting event's context field.");
	bt_put(event->context_payload);
	BT_LOGD_STR("Putting event's payload field.");
	bt_put(event->fields_payload);
	BT_LOGD_STR("Putting event's packet.");
	bt_put(event->packet);
	g_free(event);
}

struct bt_clock_value *bt_event_get_clock_value(
		struct bt_event *event, struct bt_clock_class *clock_class)
{
	struct bt_clock_value *clock_value = NULL;

	if (!event || !clock_class) {
		BT_LOGW("Invalid parameter: event or clock class is NULL: "
			"event-addr=%p, clock-class-addr=%p",
			event, clock_class);
		goto end;
	}

	clock_value = g_hash_table_lookup(event->clock_values, clock_class);
	if (!clock_value) {
		BT_LOGV("No clock value associated to the given clock class: "
			"event-addr=%p, event-class-name=\"%s\", "
			"event-class-id=%" PRId64 ", clock-class-addr=%p, "
			"clock-class-name=\"%s\"", event,
			bt_event_class_get_name(event->event_class),
			bt_event_class_get_id(event->event_class),
			clock_class, bt_clock_class_get_name(clock_class));
		goto end;
	}

	bt_get(clock_value);
end:
	return clock_value;
}

int bt_event_set_clock_value(struct bt_event *event,
		struct bt_clock_value *value)
{
	int ret = 0;
	struct bt_trace *trace;
	struct bt_stream_class *stream_class;
	struct bt_event_class *event_class;
	struct bt_clock_class *clock_class = NULL;

	if (!event || !value) {
		BT_LOGW("Invalid parameter: event or clock value is NULL: "
			"event-addr=%p, clock-value-addr=%p",
			event, value);
		ret = -1;
		goto end;
	}

	if (event->frozen) {
		BT_LOGW("Invalid parameter: event is frozen: addr=%p, "
			"event-class-name=\"%s\", event-class-id=%" PRId64,
			event, bt_event_class_get_name(event->event_class),
			bt_event_class_get_id(event->event_class));
		ret = -1;
		goto end;
	}

	clock_class = bt_clock_value_get_class(value);
	event_class = bt_event_borrow_event_class(event);
	assert(event_class);
	stream_class = bt_event_class_borrow_stream_class(event_class);
	assert(stream_class);
	trace = bt_stream_class_borrow_trace(stream_class);
	assert(trace);

	if (!bt_trace_has_clock_class(trace, clock_class)) {
		BT_LOGW("Invalid parameter: clock class is not part of event's trace: "
			"event-addr=%p, event-class-name=\"%s\", "
			"event-class-id=%" PRId64 ", clock-class-addr=%p, "
			"clock-class-name=\"%s\"",
			event, bt_event_class_get_name(event->event_class),
			bt_event_class_get_id(event->event_class),
			clock_class, bt_clock_class_get_name(clock_class));
		ret = -1;
		goto end;
	}

	g_hash_table_insert(event->clock_values, clock_class, bt_get(value));
	BT_LOGV("Set event's clock value: "
		"event-addr=%p, event-class-name=\"%s\", "
		"event-class-id=%" PRId64 ", clock-class-addr=%p, "
		"clock-class-name=\"%s\", clock-value-addr=%p, "
		"clock-value-cycles=%" PRIu64,
		event, bt_event_class_get_name(event->event_class),
		bt_event_class_get_id(event->event_class),
		clock_class, bt_clock_class_get_name(clock_class),
		value, value->value);
	clock_class = NULL;

end:
	bt_put(clock_class);
	return ret;
}

BT_HIDDEN
int bt_event_validate(struct bt_event *event)
{
	/* Make sure each field's payload has been set */
	int ret;
	struct bt_stream_class *stream_class = NULL;

	assert(event);
	if (event->event_header) {
		ret = bt_field_validate(event->event_header);
		if (ret) {
			BT_LOGD("Invalid event's header field: "
					"event-addr=%p, event-class-name=\"%s\", "
					"event-class-id=%" PRId64,
					event, bt_event_class_get_name(event->event_class),
					bt_event_class_get_id(event->event_class));
			goto end;
		}
	}

	stream_class = bt_event_class_get_stream_class(event->event_class);
	/*
	 * We should not have been able to create the event without associating
	 * the event class to a stream class.
	 */
	assert(stream_class);
	if (stream_class->event_context_type) {
		ret = bt_field_validate(event->stream_event_context);
		if (ret) {
			BT_LOGD("Invalid event's stream event context field: "
				"event-addr=%p, event-class-name=\"%s\", "
				"event-class-id=%" PRId64,
				event,
				bt_event_class_get_name(event->event_class),
				bt_event_class_get_id(event->event_class));
			goto end;
		}
	}

	ret = bt_field_validate(event->fields_payload);
	if (ret) {
		BT_LOGD("Invalid event's payload field: "
			"event-addr=%p, event-class-name=\"%s\", "
			"event-class-id=%" PRId64,
			event,
			bt_event_class_get_name(event->event_class),
			bt_event_class_get_id(event->event_class));
		goto end;
	}

	if (event->event_class->context) {
		BT_LOGD("Invalid event's context field: "
			"event-addr=%p, event-class-name=\"%s\", "
			"event-class-id=%" PRId64,
			event,
			bt_event_class_get_name(event->event_class),
			bt_event_class_get_id(event->event_class));
		ret = bt_field_validate(event->context_payload);
	}
end:
	bt_put(stream_class);
	return ret;
}

BT_HIDDEN
int bt_event_serialize(struct bt_event *event,
		struct bt_stream_pos *pos,
		enum bt_byte_order native_byte_order)
{
	int ret = 0;

	assert(event);
	assert(pos);

	BT_LOGV_STR("Serializing event's context field.");
	if (event->context_payload) {
		ret = bt_field_serialize(event->context_payload, pos,
			native_byte_order);
		if (ret) {
			BT_LOGW("Cannot serialize event's context field: "
				"event-addr=%p, event-class-name=\"%s\", "
				"event-class-id=%" PRId64,
				event,
				bt_event_class_get_name(event->event_class),
				bt_event_class_get_id(event->event_class));
			goto end;
		}
	}

	BT_LOGV_STR("Serializing event's payload field.");
	if (event->fields_payload) {
		ret = bt_field_serialize(event->fields_payload, pos,
			native_byte_order);
		if (ret) {
			BT_LOGW("Cannot serialize event's payload field: "
				"event-addr=%p, event-class-name=\"%s\", "
				"event-class-id=%" PRId64,
				event,
				bt_event_class_get_name(event->event_class),
				bt_event_class_get_id(event->event_class));
			goto end;
		}
	}
end:
	return ret;
}

struct bt_packet *bt_event_get_packet(struct bt_event *event)
{
	struct bt_packet *packet = NULL;

	if (!event) {
		BT_LOGW_STR("Invalid parameter: event is NULL.");
		goto end;
	}

	if (!event->packet) {
		BT_LOGV("Event has no current packet: addr=%p, "
			"event-class-name=\"%s\", event-class-id=%" PRId64,
			event, bt_event_class_get_name(event->event_class),
			bt_event_class_get_id(event->event_class));
		goto end;
	}

	packet = bt_get(event->packet);
end:
	return packet;
}

int bt_event_set_packet(struct bt_event *event,
		struct bt_packet *packet)
{
	struct bt_stream_class *event_stream_class = NULL;
	struct bt_stream_class *packet_stream_class = NULL;
	struct bt_stream *stream = NULL;
	int ret = 0;

	if (!event || !packet) {
		BT_LOGW("Invalid parameter: event or packet is NULL: "
			"event-addr=%p, packet-addr=%p",
			event, packet);
		ret = -1;
		goto end;
	}

	if (event->frozen) {
		BT_LOGW("Invalid parameter: event is frozen: addr=%p, "
			"event-class-name=\"%s\", event-class-id=%" PRId64,
			event, bt_event_class_get_name(event->event_class),
			bt_event_class_get_id(event->event_class));
		ret = -1;
		goto end;
	}

	/*
	 * Make sure the new packet was created by this event's
	 * stream, if it is set.
	 */
	stream = bt_event_get_stream(event);
	if (stream) {
		if (packet->stream != stream) {
			BT_LOGW("Invalid parameter: packet's stream and event's stream differ: "
				"event-addr=%p, event-class-name=\"%s\", "
				"event-class-id=%" PRId64 ", packet-stream-addr=%p, "
				"event-stream-addr=%p",
				event, bt_event_class_get_name(event->event_class),
				bt_event_class_get_id(event->event_class),
				packet->stream, stream);
			ret = -1;
			goto end;
		}
	} else {
		event_stream_class =
			bt_event_class_get_stream_class(event->event_class);
		packet_stream_class =
			bt_stream_get_class(packet->stream);

		assert(event_stream_class);
		assert(packet_stream_class);

		if (event_stream_class != packet_stream_class) {
			BT_LOGW("Invalid parameter: packet's stream class and event's stream class differ: "
				"event-addr=%p, event-class-name=\"%s\", "
				"event-class-id=%" PRId64 ", packet-stream-class-addr=%p, "
				"event-stream-class-addr=%p",
				event, bt_event_class_get_name(event->event_class),
				bt_event_class_get_id(event->event_class),
				packet_stream_class, event_stream_class);
			ret = -1;
			goto end;
		}
	}

	bt_get(packet);
	BT_MOVE(event->packet, packet);
	BT_LOGV("Set event's packet: event-addr=%p, "
		"event-class-name=\"%s\", event-class-id=%" PRId64 ", "
		"packet-addr=%p",
		event, bt_event_class_get_name(event->event_class),
		bt_event_class_get_id(event->event_class), packet);

end:
	BT_PUT(stream);
	BT_PUT(event_stream_class);
	BT_PUT(packet_stream_class);

	return ret;
}

BT_HIDDEN
void bt_event_freeze(struct bt_event *event)
{
	assert(event);

	if (event->frozen) {
		return;
	}

	BT_LOGD("Freezing event: addr=%p, "
		"event-class-name=\"%s\", event-class-id=%" PRId64,
		event, bt_event_class_get_name(event->event_class),
		bt_event_class_get_id(event->event_class));
	bt_packet_freeze(event->packet);
	BT_LOGD_STR("Freezing event's header field.");
	bt_field_freeze(event->event_header);
	BT_LOGD_STR("Freezing event's stream event context field.");
	bt_field_freeze(event->stream_event_context);
	BT_LOGD_STR("Freezing event's context field.");
	bt_field_freeze(event->context_payload);
	BT_LOGD_STR("Freezing event's payload field.");
	bt_field_freeze(event->fields_payload);
	event->frozen = 1;
}
