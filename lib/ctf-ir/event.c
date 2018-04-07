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

#include <babeltrace/assert-pre-internal.h>
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
#include <babeltrace/ctf-ir/trace.h>
#include <babeltrace/ctf-ir/trace-internal.h>
#include <babeltrace/ctf-ir/validation-internal.h>
#include <babeltrace/ctf-ir/packet-internal.h>
#include <babeltrace/ctf-ir/utils.h>
#include <babeltrace/ref.h>
#include <babeltrace/ctf-ir/attributes-internal.h>
#include <babeltrace/compiler-internal.h>
#include <babeltrace/assert-internal.h>
#include <inttypes.h>

static
void bt_event_destroy(struct bt_object *obj);

static
int bt_event_common_validate_types_for_create(
		struct bt_event_class_common *event_class,
		struct bt_validation_output *validation_output,
		bt_validation_flag_copy_field_type_func copy_field_type_func)
{
	int ret;
	enum bt_validation_flag validation_flags =
		BT_VALIDATION_FLAG_STREAM |
		BT_VALIDATION_FLAG_EVENT;
	struct bt_trace_common *trace = NULL;
	struct bt_stream_class_common *stream_class = NULL;
	struct bt_field_type_common *packet_header_type = NULL;
	struct bt_field_type_common *packet_context_type = NULL;
	struct bt_field_type_common *event_header_type = NULL;
	struct bt_field_type_common *stream_event_ctx_type = NULL;
	struct bt_field_type_common *event_context_type = NULL;
	struct bt_field_type_common *event_payload_type = NULL;
	int trace_valid = 0;
	struct bt_value *environment = NULL;

	stream_class = bt_event_class_common_borrow_stream_class(event_class);
	BT_ASSERT(stream_class);
	trace = bt_stream_class_common_borrow_trace(stream_class);
	if (trace) {
		BT_LOGD_STR("Event class is part of a trace.");
		packet_header_type = bt_trace_common_get_packet_header_field_type(trace);
		trace_valid = trace->valid;
		BT_ASSERT(trace_valid);
		environment = trace->environment;
	}

	packet_context_type = bt_stream_class_common_get_packet_context_field_type(
		stream_class);
	event_header_type = bt_stream_class_common_get_event_header_field_type(
		stream_class);
	stream_event_ctx_type = bt_stream_class_common_get_event_context_field_type(
		stream_class);
	event_context_type = bt_event_class_common_get_context_field_type(event_class);
	event_payload_type = bt_event_class_common_get_payload_field_type(event_class);
	ret = bt_validate_class_types(environment, packet_header_type,
		packet_context_type, event_header_type, stream_event_ctx_type,
		event_context_type, event_payload_type, trace_valid,
		stream_class->valid, event_class->valid,
		validation_output, validation_flags, copy_field_type_func);
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

	if ((validation_output->valid_flags & validation_flags) !=
			validation_flags) {
		/* Invalid trace/stream class/event class */
		BT_LOGW("Invalid trace, stream class, or event class: "
			"valid-flags=0x%x", validation_output->valid_flags);
		goto error;
	}

	goto end;

error:
	bt_validation_output_put_types(validation_output);
	ret = -1;

end:
	BT_ASSERT(!packet_header_type);
	BT_ASSERT(!packet_context_type);
	BT_ASSERT(!event_header_type);
	BT_ASSERT(!stream_event_ctx_type);
	BT_ASSERT(!event_context_type);
	BT_ASSERT(!event_payload_type);
	return ret;
}

static
int bt_event_common_create_fields(
		struct bt_validation_output *validation_output,
		void *(*create_field_func)(void *),
		struct bt_field_common **header_field,
		struct bt_field_common **stream_event_context_field,
		struct bt_field_common **context_field,
		struct bt_field_common **payload_field)
{
	int ret = 0;

	if (validation_output->event_header_type) {
		BT_LOGD("Creating initial event header field: ft-addr=%p",
			validation_output->event_header_type);
		*header_field =
			create_field_func(validation_output->event_header_type);
		if (!*header_field) {
			BT_LOGE_STR("Cannot create initial event header field object.");
			goto error;
		}
	}

	if (validation_output->stream_event_ctx_type) {
		BT_LOGD("Creating initial stream event context field: ft-addr=%p",
			validation_output->stream_event_ctx_type);
		*stream_event_context_field = create_field_func(
			validation_output->stream_event_ctx_type);
		if (!*stream_event_context_field) {
			BT_LOGE_STR("Cannot create initial stream event context field object.");
			goto error;
		}
	}

	if (validation_output->event_context_type) {
		BT_LOGD("Creating initial event context field: ft-addr=%p",
			validation_output->event_context_type);
		*context_field = create_field_func(
			validation_output->event_context_type);
		if (!*context_field) {
			BT_LOGE_STR("Cannot create initial event context field object.");
			goto error;
		}
	}

	if (validation_output->event_payload_type) {
		BT_LOGD("Creating initial event payload field: ft-addr=%p",
			validation_output->event_payload_type);
		*payload_field = create_field_func(
			validation_output->event_payload_type);
		if (!*payload_field) {
			BT_LOGE_STR("Cannot create initial event payload field object.");
			goto error;
		}
	}

	goto end;

error:
	BT_PUT(*header_field);
	BT_PUT(*stream_event_context_field);
	BT_PUT(*context_field);
	BT_PUT(*payload_field);
	ret = -1;

end:
	return ret;
}

BT_HIDDEN
int _bt_event_common_validate(struct bt_event_common *event)
{
	int ret = 0;
	struct bt_stream_class_common *stream_class;

	BT_ASSERT(event);
	if (event->header_field) {
		ret = bt_field_common_validate_recursive(event->header_field);
		if (ret) {
			BT_ASSERT_PRE_MSG("Invalid event's header field: "
				"%![event-]+_e, %![field-]+_f",
				event, event->header_field);
			goto end;
		}
	}

	stream_class = bt_event_class_common_borrow_stream_class(event->class);

	/*
	 * We should not have been able to create the event without associating
	 * the event class to a stream class.
	 */
	BT_ASSERT(stream_class);

	if (stream_class->event_context_field_type) {
		ret = bt_field_common_validate_recursive(
			event->stream_event_context_field);
		if (ret) {
			BT_ASSERT_PRE_MSG("Invalid event's stream event context field: "
				"%![event-]+_e, %![field-]+_f",
				event, event->stream_event_context_field);
			goto end;
		}
	}

	if (event->class->context_field_type) {
		ret = bt_field_common_validate_recursive(event->context_field);
		if (ret) {
			BT_ASSERT_PRE_MSG("Invalid event's payload field: "
				"%![event-]+_e, %![field-]+_f",
				event, event->context_field);
			goto end;
		}
	}

	ret = bt_field_common_validate_recursive(event->payload_field);
	if (ret) {
		BT_ASSERT_PRE_MSG("Invalid event's payload field: "
			"%![event-]+_e, %![field-]+_f",
			event, event->payload_field);
		goto end;
	}

end:
	return ret;
}

BT_HIDDEN
void _bt_event_common_freeze(struct bt_event_common *event)
{
	BT_ASSERT(event);

	if (event->frozen) {
		return;
	}

	BT_LOGD("Freezing event: addr=%p, "
		"event-class-name=\"%s\", event-class-id=%" PRId64,
		event, bt_event_class_common_get_name(event->class),
		bt_event_class_common_get_id(event->class));
	BT_LOGD_STR("Freezing event's header field.");
	bt_field_common_freeze_recursive(event->header_field);
	BT_LOGD_STR("Freezing event's stream event context field.");
	bt_field_common_freeze_recursive(event->stream_event_context_field);
	BT_LOGD_STR("Freezing event's context field.");
	bt_field_common_freeze_recursive(event->context_field);
	BT_LOGD_STR("Freezing event's payload field.");
	bt_field_common_freeze_recursive(event->payload_field);
	event->frozen = 1;
}

BT_HIDDEN
int bt_event_common_initialize(struct bt_event_common *event,
		struct bt_event_class_common *event_class,
		struct bt_clock_class *init_expected_clock_class,
		bt_object_release_func release_func,
		bt_validation_flag_copy_field_type_func field_type_copy_func,
		bool must_be_in_trace,
		int (*map_clock_classes_func)(struct bt_stream_class_common *stream_class,
			struct bt_field_type_common *packet_context_field_type,
			struct bt_field_type_common *event_header_field_type),
		void *(*create_field_func)(void *))
{
	int ret;
	struct bt_trace_common *trace = NULL;
	struct bt_stream_class_common *stream_class = NULL;
	struct bt_field_common *event_header = NULL;
	struct bt_field_common *stream_event_context = NULL;
	struct bt_field_common *event_context = NULL;
	struct bt_field_common *event_payload = NULL;
	struct bt_validation_output validation_output = { 0 };
	struct bt_clock_class *expected_clock_class =
		init_expected_clock_class ? bt_get(init_expected_clock_class) :
		NULL;

	BT_ASSERT_PRE_NON_NULL(event_class, "Event class");
	BT_LOGD("Initializing common event object: event-class-addr=%p, "
		"event-class-name=\"%s\", event-class-id=%" PRId64,
		event_class, bt_event_class_common_get_name(event_class),
		bt_event_class_common_get_id(event_class));

	stream_class = bt_event_class_common_borrow_stream_class(event_class);
	BT_ASSERT_PRE(stream_class,
		"Event class is not part of a stream class: %!+_E", event_class);

	/* The event class was frozen when added to its stream class */
	BT_ASSERT(event_class->frozen);
	trace = bt_stream_class_common_borrow_trace(stream_class);

	if (must_be_in_trace) {
		BT_ASSERT_PRE(trace,
			"Event class's stream class is not part of a trace: "
			"%![ec-]+_E, %![ec-]+_S", event_class, stream_class);
	}

	/*
	 * This must be called before anything that can fail because on
	 * failure, the caller releases the reference to `event` to
	 * destroy it.
	 */
	bt_object_init(event, release_func);

	if (!stream_class->frozen) {
		/*
		 * Because this function freezes the stream class,
		 * validate that this stream class contains at most a
		 * single clock class so that we set its expected clock
		 * class for future checks.
		 */
		ret = bt_stream_class_common_validate_single_clock_class(
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
				stream_class,
				bt_stream_class_common_get_id(stream_class),
				bt_stream_class_common_get_name(stream_class),
				expected_clock_class,
				expected_clock_class ?
					bt_clock_class_get_name(expected_clock_class) :
					NULL);
			goto error;
		}
	}

	/* Validate the trace, the stream class, and the event class */
	ret = bt_event_common_validate_types_for_create(
		event_class, &validation_output, field_type_copy_func);
	if (ret) {
		/* bt_event_common_validate_types_for_create() logs errors */
		goto error;
	}

	if (map_clock_classes_func) {
		/*
		 * Safe to automatically map selected fields to the
		 * stream's clock's class here because the stream class
		 * is about to be frozen.
		 */
		if (map_clock_classes_func(stream_class,
				validation_output.packet_context_type,
				validation_output.event_header_type)) {
			BT_LOGW_STR("Cannot automatically map selected stream class's "
				"field types to stream class's clock's class.");
			goto error;
		}
	}

	/*
	 * event does not share a common ancestor with the event class; it has
	 * to guarantee its existence by holding a reference. This reference
	 * shall be released once the event is associated to a stream since,
	 * from that point, the event and its class will share the same
	 * lifetime.
	 */
	event->class = bt_get(event_class);

	ret = bt_event_common_create_fields(&validation_output,
		create_field_func, &event_header,
		&stream_event_context, &event_context, &event_payload);
	if (ret) {
		/* bt_event_common_create_fields() logs errors */
		goto error;
	}

	/*
	 * At this point all the fields are created, potentially from
	 * validated copies of field types, so that the field types and
	 * fields can be replaced in the trace, stream class,
	 * event class, and created event.
	 */
	bt_validation_replace_types(trace, stream_class, event_class,
		&validation_output,
		BT_VALIDATION_FLAG_STREAM | BT_VALIDATION_FLAG_EVENT);
	BT_MOVE(event->header_field, event_header);
	BT_MOVE(event->stream_event_context_field, stream_event_context);
	BT_MOVE(event->context_field, event_context);
	BT_MOVE(event->payload_field, event_payload);

	/*
	 * Put what was not moved in bt_validation_replace_types().
	 */
	bt_validation_output_put_types(&validation_output);

	/*
	 * Freeze the stream class since the event header must not be changed
	 * anymore.
	 */
	bt_stream_class_common_freeze(stream_class);

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
	BT_LOGD("Initialized event object: addr=%p, event-class-name=\"%s\", "
		"event-class-id=%" PRId64,
		event, bt_event_class_common_get_name(event->class),
		bt_event_class_common_get_id(event->class));
	goto end;

error:
	bt_validation_output_put_types(&validation_output);
	bt_put(expected_clock_class);
	bt_put(event_header);
	bt_put(stream_event_context);
	bt_put(event_context);
	bt_put(event_payload);
	ret = -1;

end:
	return ret;
}

struct bt_event *bt_event_create(struct bt_event_class *event_class)
{
	int ret;
	struct bt_event *event = NULL;

	event = g_new0(struct bt_event, 1);
	if (!event) {
		BT_LOGE_STR("Failed to allocate one event.");
		goto error;
	}

	ret = bt_event_common_initialize(BT_TO_COMMON(event),
		BT_TO_COMMON(event_class), NULL, bt_event_destroy,
		(bt_validation_flag_copy_field_type_func) bt_field_type_copy,
		true, NULL, (void *) bt_field_create);
	if (ret) {
		/* bt_event_common_initialize() logs errors */
		goto error;
	}

	event->clock_values = g_hash_table_new_full(g_direct_hash,
			g_direct_equal, bt_put, bt_put);
	assert(event->clock_values);
	goto end;

error:
	BT_PUT(event);

end:
	return event;
}

struct bt_event_class *bt_event_get_class(struct bt_event *event)
{
	BT_ASSERT_PRE_NON_NULL(event, "Event");
	return bt_get(bt_event_common_borrow_class(BT_TO_COMMON(event)));
}

BT_HIDDEN
struct bt_stream *bt_event_borrow_stream(struct bt_event *event)
{
	struct bt_stream *stream = NULL;

	BT_ASSERT(event);

	if (event->packet) {
		stream = event->packet->stream;
	}

	return stream;
}

struct bt_stream *bt_event_get_stream(struct bt_event *event)
{
	BT_ASSERT_PRE_NON_NULL(event, "Event");
	return bt_get(bt_event_borrow_stream(event));
}

struct bt_field *bt_event_get_payload(struct bt_event *event)
{
	return BT_FROM_COMMON(bt_event_common_get_payload(BT_TO_COMMON(event)));
}

int bt_event_set_payload(struct bt_event *event, struct bt_field *field)
{
	return bt_event_common_set_payload(BT_TO_COMMON(event),
		(void *) field);
}

struct bt_field *bt_event_get_header(struct bt_event *event)
{
	return BT_FROM_COMMON(bt_event_common_get_header(BT_TO_COMMON(event)));
}

int bt_event_set_header(struct bt_event *event, struct bt_field *field)
{
	return bt_event_common_set_header(BT_TO_COMMON(event), (void *) field);
}

struct bt_field *bt_event_get_context(struct bt_event *event)
{
	return BT_FROM_COMMON(bt_event_common_get_context(BT_TO_COMMON(event)));
}

int bt_event_set_context(struct bt_event *event, struct bt_field *field)
{
	return bt_event_common_set_context(BT_TO_COMMON(event), (void *) field);
}

struct bt_field *bt_event_get_stream_event_context(
		struct bt_event *event)
{
	return BT_FROM_COMMON(bt_event_common_get_stream_event_context(
		BT_TO_COMMON(event)));
}

int bt_event_set_stream_event_context(struct bt_event *event,
		struct bt_field *field)
{
	return bt_event_common_set_stream_event_context(
		BT_TO_COMMON(event), (void *) field);
}

void bt_event_destroy(struct bt_object *obj)
{
	struct bt_event *event = (void *) obj;

	bt_event_common_finalize(obj);
	g_hash_table_destroy(event->clock_values);
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
			bt_event_class_common_get_name(event->common.class),
			bt_event_class_common_get_id(event->common.class),
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
	BT_ASSERT_PRE_EVENT_COMMON_HOT(BT_TO_COMMON(event), "Event");
	clock_class = bt_clock_value_get_class(value);
	event_class = BT_FROM_COMMON(event->common.class);
	BT_ASSERT(event_class);
	stream_class = bt_event_class_borrow_stream_class(event_class);
	BT_ASSERT(stream_class);
	trace = bt_stream_class_borrow_trace(stream_class);
	BT_ASSERT(trace);
	BT_ASSERT_PRE(bt_trace_common_has_clock_class(BT_TO_COMMON(trace),
		clock_class),
		"Clock class is not part of event's trace: "
		"%![event-]+e, %![clock-class-]+K",
		event, clock_class);
	g_hash_table_insert(event->clock_values, clock_class, bt_get(value));
	BT_LOGV("Set event's clock value: "
		"event-addr=%p, event-class-name=\"%s\", "
		"event-class-id=%" PRId64 ", clock-class-addr=%p, "
		"clock-class-name=\"%s\", clock-value-addr=%p, "
		"clock-value-cycles=%" PRIu64,
		event, bt_event_class_common_get_name(event->common.class),
		bt_event_class_common_get_id(event->common.class),
		clock_class, bt_clock_class_get_name(clock_class),
		value, value->value);
	clock_class = NULL;
	bt_put(clock_class);
	return 0;
}

struct bt_packet *bt_event_get_packet(struct bt_event *event)
{
	struct bt_packet *packet = NULL;

	BT_ASSERT_PRE_NON_NULL(event, "Event");
	if (!event->packet) {
		BT_LOGV("Event has no current packet: addr=%p, "
			"event-class-name=\"%s\", event-class-id=%" PRId64,
			event, bt_event_class_common_get_name(event->common.class),
			bt_event_class_common_get_id(event->common.class));
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
	BT_ASSERT_PRE_EVENT_COMMON_HOT(BT_TO_COMMON(event), "Event");

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
		BT_ASSERT_PRE(bt_event_class_borrow_stream_class(
			BT_FROM_COMMON(event->common.class)) ==
			BT_FROM_COMMON(packet->stream->common.stream_class),
			"Packet's stream class and event's stream class differ: "
			"%![event-]+e, %![packet-]+a",
			event, packet);
	}

	bt_get(packet);
	BT_MOVE(event->packet, packet);
	BT_LOGV("Set event's packet: event-addr=%p, "
		"event-class-name=\"%s\", event-class-id=%" PRId64 ", "
		"packet-addr=%p",
		event, bt_event_class_common_get_name(event->common.class),
		bt_event_class_common_get_id(event->common.class), packet);
	return 0;
}

BT_HIDDEN
void _bt_event_freeze(struct bt_event *event)
{
	_bt_event_common_freeze(BT_TO_COMMON(event));
	BT_LOGD_STR("Freezing event's packet.");
	bt_packet_freeze(event->packet);
}
