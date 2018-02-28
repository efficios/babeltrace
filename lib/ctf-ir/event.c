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
#include <babeltrace/assert-internal.h>
#include <babeltrace/assert-pre-internal.h>
#include <inttypes.h>

#define BT_ASSERT_PRE_EVENT_HOT(_event, _name)				\
	BT_ASSERT_PRE_HOT((_event), (_name), ": +%!+e", (_event))

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

	BT_ASSERT_PRE_NON_NULL(event_class, "Event class");
	BT_LOGD("Creating event object: event-class-addr=%p, "
		"event-class-name=\"%s\", event-class-id=%" PRId64,
		event_class, bt_event_class_get_name(event_class),
		bt_event_class_get_id(event_class));

	stream_class = bt_event_class_get_stream_class(event_class);
	BT_ASSERT_PRE(stream_class,
		"Event class is not part of a stream class: %!+E", event_class);

	/* The event class was frozen when added to its stream class */
	BT_ASSERT(event_class->frozen);

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
		BT_ASSERT(trace_valid);
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
	BT_ASSERT(!packet_header_type);
	BT_ASSERT(!packet_context_type);
	BT_ASSERT(!event_header_type);
	BT_ASSERT(!stream_event_ctx_type);
	BT_ASSERT(!event_context_type);
	BT_ASSERT(!event_payload_type);
	return event;
}

struct bt_event_class *bt_event_get_class(struct bt_event *event)
{
	BT_ASSERT_PRE_NON_NULL(event, "Event");
	return bt_get(bt_event_borrow_event_class(event));
}

BT_HIDDEN
struct bt_stream *bt_event_borrow_stream(struct bt_event *event)
{
	struct bt_stream *stream = NULL;

	BT_ASSERT(event);

	/*
	 * If the event has a parent, then this is its (writer) stream.
	 * If the event has no parent, then if it has a packet, this
	 * is its (non-writer) stream.
	 */
	if (event->base.parent) {
		stream = (struct bt_stream *) bt_object_borrow_parent(event);
	} else {
		if (event->packet) {
			stream = event->packet->stream;
		}
	}

	return stream;
}

struct bt_stream *bt_event_get_stream(struct bt_event *event)
{
	BT_ASSERT_PRE_NON_NULL(event, "Event");
	return bt_get(bt_event_borrow_stream(event));
}

int bt_event_set_payload(struct bt_event *event, const char *name,
		struct bt_field *payload)
{
	int ret = 0;

	BT_ASSERT_PRE_NON_NULL(event, "Event");
	BT_ASSERT_PRE_NON_NULL(payload, "Payload field");
	BT_ASSERT_PRE_EVENT_HOT(event, "Event");

	if (name) {
		ret = bt_field_structure_set_field_by_name(
			event->fields_payload, name, payload);
	} else {
		BT_ASSERT_PRE(bt_field_type_compare(payload->type,
			event->event_class->fields) == 0,
			"Payload field's type is different from the "
			"expected field type: %![event-]+e, %![ft-]+F, "
			"%![expected-ft-]+F",
			event, payload->type, event->event_class->fields);

		bt_put(event->fields_payload);
		bt_get(payload);
		event->fields_payload = payload;
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

	return ret;
}

struct bt_field *bt_event_get_event_payload(struct bt_event *event)
{
	struct bt_field *payload = NULL;

	BT_ASSERT_PRE_NON_NULL(event, "Event");

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

	BT_ASSERT_PRE_NON_NULL(event, "Event");

	if (name) {
		field = bt_field_structure_get_field_by_name(
			event->fields_payload, name);
	} else {
		field = event->fields_payload;
		bt_get(field);
	}

	return field;
}

struct bt_field *bt_event_get_payload_by_index(
		struct bt_event *event, uint64_t index)
{
	struct bt_field *field = NULL;

	BT_ASSERT_PRE_NON_NULL(event, "Event");
	field = bt_field_structure_get_field_by_index(event->fields_payload,
		index);

	return field;
}

struct bt_field *bt_event_get_header(struct bt_event *event)
{
	struct bt_field *header = NULL;

	BT_ASSERT_PRE_NON_NULL(event, "Event");

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

int bt_event_set_header(struct bt_event *event, struct bt_field *header)
{
	BT_ASSERT_PRE_NON_NULL(event, "Event");
	BT_ASSERT_PRE_EVENT_HOT(event, "Event");

	/*
	 * Ensure the provided header's type matches the one registered to the
	 * stream class.
	 */
	if (header) {
		BT_ASSERT_PRE(bt_field_type_compare(header->type,
			bt_event_class_borrow_stream_class(event->event_class)->event_header_type) == 0,
			"Header field's type is different from the "
			"expected field type: %![event-]+e, %![ft-]+F, "
			"%![expected-ft-]+F",
			event, header->type,
			bt_event_class_borrow_stream_class(event->event_class)->event_header_type);
	} else {
		BT_ASSERT_PRE(!bt_event_class_borrow_stream_class(event->event_class)->event_header_type,
			"Setting no event header field, "
			"but event header field type is not NULL: ",
			"%![event-]+e, %![header-ft-]+F",
			event,
			bt_event_class_borrow_stream_class(event->event_class)->event_header_type);
	}

	bt_put(event->event_header);
	event->event_header = bt_get(header);
	BT_LOGV("Set event's header field: event-addr=%p, "
		"event-class-name=\"%s\", event-class-id=%" PRId64 ", "
		"header-field-addr=%p",
		event, bt_event_class_get_name(event->event_class),
		bt_event_class_get_id(event->event_class), header);
	return 0;
}

struct bt_field *bt_event_get_event_context(struct bt_event *event)
{
	struct bt_field *context = NULL;

	BT_ASSERT_PRE_NON_NULL(event, "Event");

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

int bt_event_set_event_context(struct bt_event *event, struct bt_field *context)
{
	BT_ASSERT_PRE_NON_NULL(event, "Event");
	BT_ASSERT_PRE_EVENT_HOT(event, "Event");

	if (context) {
		BT_ASSERT_PRE(bt_field_type_compare(context->type,
			event->event_class->context) == 0,
			"Context field's type is different from the "
			"expected field type: %![event-]+e, %![ft-]+F, "
			"%![expected-ft-]+F",
			event, context->type, event->event_class->context);
	} else {
		BT_ASSERT_PRE(!event->event_class->context,
			"Setting no event context field, "
			"but event context field type is not NULL: ",
			"%![event-]+e, %![context-ft-]+F",
			event, event->event_class->context);
	}

	bt_put(event->context_payload);
	event->context_payload = bt_get(context);
	BT_LOGV("Set event's context field: event-addr=%p, "
		"event-class-name=\"%s\", event-class-id=%" PRId64 ", "
		"context-field-addr=%p",
		event, bt_event_class_get_name(event->event_class),
		bt_event_class_get_id(event->event_class), context);
	return 0;
}

struct bt_field *bt_event_get_stream_event_context(
		struct bt_event *event)
{
	struct bt_field *stream_event_context = NULL;

	BT_ASSERT_PRE_NON_NULL(event, "Event");

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
	BT_ASSERT_PRE_NON_NULL(event, "Event");
	BT_ASSERT_PRE_EVENT_HOT(event, "Event");

	if (stream_event_context) {
		BT_ASSERT_PRE(bt_field_type_compare(stream_event_context->type,
			bt_event_class_borrow_stream_class(event->event_class)->event_context_type) == 0,
			"Stream event context field's type is different from the "
			"expected field type: %![event-]+e, %![ft-]+F, "
			"%![expected-ft-]+F",
			event, stream_event_context->type,
			bt_event_class_borrow_stream_class(event->event_class)->event_context_type);
	} else {
		BT_ASSERT_PRE(!bt_event_class_borrow_stream_class(event->event_class)->event_context_type,
			"Setting no stream event context field, "
			"but stream event context field type is not NULL: ",
			"%![event-]+e, %![context-ft-]+F",
			event,
			bt_event_class_borrow_stream_class(event->event_class)->event_context_type);
	}

	bt_get(stream_event_context);
	BT_MOVE(event->stream_event_context, stream_event_context);
	BT_LOGV("Set event's stream event context field: event-addr=%p, "
		"event-class-name=\"%s\", event-class-id=%" PRId64 ", "
		"stream-event-context-field-addr=%p",
		event, bt_event_class_get_name(event->event_class),
		bt_event_class_get_id(event->event_class),
		stream_event_context);
	return 0;
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

	BT_ASSERT_PRE_NON_NULL(event, "Event");
	BT_ASSERT_PRE_NON_NULL(clock_class, "Clock class");
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
	struct bt_trace *trace;
	struct bt_stream_class *stream_class;
	struct bt_event_class *event_class;
	struct bt_clock_class *clock_class = NULL;

	BT_ASSERT_PRE_NON_NULL(event, "Event");
	BT_ASSERT_PRE_NON_NULL(value, "Clock value");
	BT_ASSERT_PRE_EVENT_HOT(event, "Event");
	clock_class = bt_clock_value_get_class(value);
	event_class = bt_event_borrow_event_class(event);
	BT_ASSERT(event_class);
	stream_class = bt_event_class_borrow_stream_class(event_class);
	BT_ASSERT(stream_class);
	trace = bt_stream_class_borrow_trace(stream_class);
	BT_ASSERT(trace);
	BT_ASSERT_PRE(bt_trace_has_clock_class(trace, clock_class),
		"Clock class is not part of event's trace: "
		"%![event-]+e, %![clock-class-]+K",
		event, clock_class);
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
	bt_put(clock_class);
	return 0;
}

BT_HIDDEN
int _bt_event_validate(struct bt_event *event)
{
	int ret = 0;
	struct bt_stream_class *stream_class;

	BT_ASSERT(event);
	if (event->event_header) {
		ret = bt_field_validate_recursive(event->event_header);
		if (ret) {
			BT_ASSERT_PRE_MSG("Invalid event's header field: "
				"%![event-]+e, %![field-]+f",
				event, event->event_header);
			goto end;
		}
	}

	stream_class = bt_event_class_borrow_stream_class(event->event_class);

	/*
	 * We should not have been able to create the event without associating
	 * the event class to a stream class.
	 */
	BT_ASSERT(stream_class);

	if (stream_class->event_context_type) {
		ret = bt_field_validate_recursive(event->stream_event_context);
		if (ret) {
			BT_ASSERT_PRE_MSG("Invalid event's stream event context field: "
				"%![event-]+e, %![field-]+f",
				event, event->stream_event_context);
			goto end;
		}
	}

	if (event->event_class->context) {
		ret = bt_field_validate_recursive(event->context_payload);
		if (ret) {
			BT_ASSERT_PRE_MSG("Invalid event's payload field: "
				"%![event-]+e, %![field-]+f",
				event, event->context_payload);
			goto end;
		}
	}

	ret = bt_field_validate_recursive(event->fields_payload);
	if (ret) {
		BT_ASSERT_PRE_MSG("Invalid event's payload field: "
			"%![event-]+e, %![field-]+f",
			event, event->fields_payload);
		goto end;
	}

end:
	return ret;
}

BT_HIDDEN
int bt_event_serialize(struct bt_event *event, struct bt_stream_pos *pos,
		enum bt_byte_order native_byte_order)
{
	int ret = 0;

	BT_ASSERT(event);
	BT_ASSERT(pos);

	BT_LOGV_STR("Serializing event's context field.");
	if (event->context_payload) {
		ret = bt_field_serialize_recursive(event->context_payload, pos,
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
		ret = bt_field_serialize_recursive(event->fields_payload, pos,
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

	BT_ASSERT_PRE_NON_NULL(event, "Event");
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
	BT_ASSERT_PRE_NON_NULL(event, "Event");
	BT_ASSERT_PRE_NON_NULL(packet, "Packet");
	BT_ASSERT_PRE_EVENT_HOT(event, "Event");

	/*
	 * Make sure the new packet was created by this event's
	 * stream, if it is set.
	 */
	if (bt_event_borrow_stream(event)) {
		BT_ASSERT_PRE(packet->stream == bt_event_borrow_stream(event),
			"Packet's stream and event's stream differ: "
			"%![event-]+e, %![packet-]+a",
			event, packet);
	} else {
		BT_ASSERT_PRE(bt_event_class_borrow_stream_class(event->event_class) ==
			packet->stream->stream_class,
			"Packet's stream class and event's stream class differ: "
			"%![event-]+e, %![packet-]+a",
			event, packet);
	}

	bt_get(packet);
	BT_MOVE(event->packet, packet);
	BT_LOGV("Set event's packet: event-addr=%p, "
		"event-class-name=\"%s\", event-class-id=%" PRId64 ", "
		"packet-addr=%p",
		event, bt_event_class_get_name(event->event_class),
		bt_event_class_get_id(event->event_class), packet);
	return 0;
}

BT_HIDDEN
void _bt_event_freeze(struct bt_event *event)
{
	BT_ASSERT(event);

	if (event->frozen) {
		return;
	}

	BT_LOGD("Freezing event: addr=%p, "
		"event-class-name=\"%s\", event-class-id=%" PRId64,
		event, bt_event_class_get_name(event->event_class),
		bt_event_class_get_id(event->event_class));
	bt_packet_freeze(event->packet);
	BT_LOGD_STR("Freezing event's header field.");
	bt_field_freeze_recursive(event->event_header);
	BT_LOGD_STR("Freezing event's stream event context field.");
	bt_field_freeze_recursive(event->stream_event_context);
	BT_LOGD_STR("Freezing event's context field.");
	bt_field_freeze_recursive(event->context_payload);
	BT_LOGD_STR("Freezing event's payload field.");
	bt_field_freeze_recursive(event->fields_payload);
	event->frozen = 1;
}
