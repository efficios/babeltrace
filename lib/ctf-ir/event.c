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

static inline
int bt_event_validate_types_for_create(
		struct bt_event_class *event_class,
		struct bt_validation_output *validation_output,
		bt_validation_flag_copy_field_type_func copy_field_type_func)
{
	int ret;
	enum bt_validation_flag validation_flags =
		BT_VALIDATION_FLAG_STREAM |
		BT_VALIDATION_FLAG_EVENT;
	struct bt_trace *trace = NULL;
	struct bt_stream_class *stream_class = NULL;
	struct bt_field_type *packet_header_type = NULL;
	struct bt_field_type *packet_context_type = NULL;
	struct bt_field_type *event_header_type = NULL;
	struct bt_field_type *stream_event_ctx_type = NULL;
	struct bt_field_type *event_context_type = NULL;
	struct bt_field_type *event_payload_type = NULL;
	int trace_valid = 0;
	struct bt_value *environment = NULL;

	stream_class = bt_event_class_borrow_stream_class(event_class);
	BT_ASSERT(stream_class);
	trace = bt_stream_class_borrow_trace(stream_class);
	if (trace) {
		BT_LOGD_STR("Event class is part of a trace.");
		packet_header_type =
			bt_trace_borrow_packet_header_field_type(trace);
		trace_valid = trace->valid;
		BT_ASSERT(trace_valid);
		environment = trace->environment;
	}

	packet_context_type =
		bt_stream_class_borrow_packet_context_field_type(
			stream_class);
	event_header_type =
		bt_stream_class_borrow_event_header_field_type(
			stream_class);
	stream_event_ctx_type =
		bt_stream_class_borrow_event_context_field_type(
			stream_class);
	event_context_type =
		bt_event_class_borrow_context_field_type(event_class);
	event_payload_type =
		bt_event_class_borrow_payload_field_type(event_class);
	ret = bt_validate_class_types(environment, packet_header_type,
		packet_context_type, event_header_type, stream_event_ctx_type,
		event_context_type, event_payload_type, trace_valid,
		stream_class->valid, event_class->valid,
		validation_output, validation_flags, copy_field_type_func);
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
	return ret;
}

static
int bt_event_create_fields(
		struct bt_stream_class *stream_class,
		struct bt_validation_output *validation_output,
		create_field_func create_field_func,
		release_field_func release_field_func,
		create_header_field_func create_header_field_func,
		release_header_field_func release_header_field_func,
		struct bt_field_wrapper **header_field,
		struct bt_field **stream_event_context_field,
		struct bt_field **context_field,
		struct bt_field **payload_field)
{
	int ret = 0;

	if (validation_output->event_header_type) {
		BT_LOGD("Creating initial event header field: ft-addr=%p",
			validation_output->event_header_type);
		*header_field =
			create_header_field_func(stream_class,
				validation_output->event_header_type);
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
	if (*header_field) {
		release_header_field_func(*header_field, stream_class);
	}

	if (*stream_event_context_field) {
		release_field_func(*stream_event_context_field);
	}

	if (*context_field) {
		release_field_func(*context_field);
	}

	if (*payload_field) {
		release_field_func(*payload_field);
	}

	ret = -1;

end:
	return ret;
}

BT_HIDDEN
int _bt_event_validate(struct bt_event *event)
{
	int ret = 0;
	struct bt_stream_class *stream_class;

	BT_ASSERT(event);
	if (event->header_field) {
		ret = bt_field_validate_recursive(
			event->header_field->field);
		if (ret) {
			BT_ASSERT_PRE_MSG("Invalid event's header field: "
				"%![event-]+e, %![field-]+f",
				event, event->header_field->field);
			goto end;
		}
	}

	stream_class = bt_event_class_borrow_stream_class(event->class);

	/*
	 * We should not have been able to create the event without associating
	 * the event class to a stream class.
	 */
	BT_ASSERT(stream_class);

	if (stream_class->event_context_field_type) {
		ret = bt_field_validate_recursive(
			event->stream_event_context_field);
		if (ret) {
			BT_ASSERT_PRE_MSG("Invalid event's stream event context field: "
				"%![event-]+e, %![field-]+f",
				event, event->stream_event_context_field);
			goto end;
		}
	}

	if (event->class->context_field_type) {
		ret = bt_field_validate_recursive(event->context_field);
		if (ret) {
			BT_ASSERT_PRE_MSG("Invalid event's payload field: "
				"%![event-]+e, %![field-]+f",
				event, event->context_field);
			goto end;
		}
	}

	ret = bt_field_validate_recursive(event->payload_field);
	if (ret) {
		BT_ASSERT_PRE_MSG("Invalid event's payload field: "
			"%![event-]+e, %![field-]+f",
			event, event->payload_field);
		goto end;
	}

end:
	return ret;
}

BT_HIDDEN
void _bt_event_set_is_frozen(struct bt_event *event,
		bool is_frozen)
{
	BT_ASSERT(event);
	BT_LOGD("Freezing event: addr=%p, "
		"event-class-name=\"%s\", event-class-id=%" PRId64,
		event, bt_event_class_get_name(event->class),
		bt_event_class_get_id(event->class));

	if (event->header_field) {
		BT_LOGD_STR("Freezing event's header field.");
		bt_field_set_is_frozen_recursive(
			event->header_field->field, is_frozen);
	}

	if (event->stream_event_context_field) {
		BT_LOGD_STR("Freezing event's stream event context field.");
		bt_field_set_is_frozen_recursive(
			event->stream_event_context_field, is_frozen);
	}

	if (event->context_field) {
		BT_LOGD_STR("Freezing event's context field.");
		bt_field_set_is_frozen_recursive(event->context_field,
			is_frozen);
	}

	if (event->payload_field) {
		BT_LOGD_STR("Freezing event's payload field.");
		bt_field_set_is_frozen_recursive(event->payload_field,
			is_frozen);
	}

	event->frozen = is_frozen;
	BT_LOGD_STR("Freezing event's packet.");
	bt_packet_set_is_frozen(event->packet, is_frozen);
}

static inline
int bt_event_initialize(struct bt_event *event,
		struct bt_event_class *event_class,
		bt_validation_flag_copy_field_type_func field_type_copy_func,
		create_field_func create_field_func,
		release_field_func release_field_func,
		create_header_field_func create_header_field_func,
		release_header_field_func release_header_field_func)
{
	int ret;
	struct bt_trace *trace = NULL;
	struct bt_stream_class *stream_class = NULL;
	struct bt_field_wrapper *event_header = NULL;
	struct bt_field *stream_event_context = NULL;
	struct bt_field *event_context = NULL;
	struct bt_field *event_payload = NULL;
	struct bt_validation_output validation_output = { 0 };
	struct bt_clock_class *expected_clock_class = NULL;

	BT_ASSERT_PRE_NON_NULL(event_class, "Event class");
	BT_LOGD("Initializing event object: event-class-addr=%p, "
		"event-class-name=\"%s\", event-class-id=%" PRId64,
		event_class, bt_event_class_get_name(event_class),
		bt_event_class_get_id(event_class));

	stream_class = bt_event_class_borrow_stream_class(event_class);
	BT_ASSERT_PRE(stream_class,
		"Event class is not part of a stream class: %!+E", event_class);

	/* The event class was frozen when added to its stream class */
	BT_ASSERT(event_class->frozen);
	trace = bt_stream_class_borrow_trace(stream_class);
	BT_ASSERT_PRE(trace,
		"Event class's stream class is not part of a trace: "
		"%![ec-]+E, %![ec-]+S", event_class, stream_class);

	/*
	 * This must be called before anything that can fail because on
	 * failure, the caller releases the reference to `event` to
	 * destroy it.
	 */
	bt_object_init_unique(&event->base);

	if (!stream_class->frozen) {
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
				stream_class,
				bt_stream_class_get_id(stream_class),
				bt_stream_class_get_name(stream_class),
				expected_clock_class,
				expected_clock_class ?
					bt_clock_class_get_name(expected_clock_class) :
					NULL);
			goto error;
		}
	}

	/* Validate the trace, the stream class, and the event class */
	ret = bt_event_validate_types_for_create(
		event_class, &validation_output, field_type_copy_func);
	if (ret) {
		/* bt_event_validate_types_for_create() logs errors */
		goto error;
	}

	/*
	 * event does not share a common ancestor with the event class; it has
	 * to guarantee its existence by holding a reference. This reference
	 * shall be released once the event is associated to a stream since,
	 * from that point, the event and its class will share the same
	 * lifetime.
	 *
	 * TODO: Is this still true now that this API and CTF writer are
	 * two different implementations?
	 */
	event->class = bt_get(event_class);
	ret = bt_event_create_fields(stream_class,
		&validation_output,
		create_field_func, release_field_func,
		create_header_field_func, release_header_field_func,
		&event_header, &stream_event_context, &event_context,
		&event_payload);
	if (ret) {
		/* bt_event_create_fields() logs errors */
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
	event->header_field = event_header;
	event_header = NULL;
	event->stream_event_context_field = stream_event_context;
	stream_event_context = NULL;
	event->context_field = event_context;
	event_context = NULL;
	event->payload_field = event_payload;
	event_payload = NULL;

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
	BT_LOGD("Initialized event object: addr=%p, event-class-name=\"%s\", "
		"event-class-id=%" PRId64,
		event, bt_event_class_get_name(event->class),
		bt_event_class_get_id(event->class));
	goto end;

error:
	bt_validation_output_put_types(&validation_output);
	bt_put(expected_clock_class);

	if (event_header) {
		release_header_field_func(event_header, stream_class);
	}

	if (stream_event_context) {
		release_field_func(stream_event_context);
	}

	if (event_context) {
		release_field_func(event_context);
	}

	if (event_payload) {
		release_field_func(event_payload);
	}

	ret = -1;

end:
	return ret;
}

static
void bt_event_header_field_recycle(struct bt_field_wrapper *field_wrapper,
		struct bt_stream_class *stream_class)
{
	BT_ASSERT(field_wrapper);
	BT_LIB_LOGD("Recycling event header field: "
		"addr=%p, %![sc-]+S, %![field-]+f", field_wrapper,
		stream_class, field_wrapper->field);
	bt_object_pool_recycle_object(
		&stream_class->event_header_field_pool,
		field_wrapper);
}

static
struct bt_field_wrapper *create_event_header_field(
		struct bt_stream_class *stream_class,
		struct bt_field_type *ft)
{
	struct bt_field_wrapper *field_wrapper = NULL;

	field_wrapper = bt_field_wrapper_create(
		&stream_class->event_header_field_pool, (void *) ft);
	if (!field_wrapper) {
		goto error;
	}

	goto end;

error:
	if (field_wrapper) {
		bt_event_header_field_recycle(field_wrapper, stream_class);
		field_wrapper = NULL;
	}

end:
	return field_wrapper;
}

BT_HIDDEN
struct bt_event *bt_event_new(struct bt_event_class *event_class)
{
	int ret;
	struct bt_event *event = NULL;
	struct bt_stream_class *stream_class;

	event = g_new0(struct bt_event, 1);
	if (!event) {
		BT_LOGE_STR("Failed to allocate one event.");
		goto error;
	}

	ret = bt_event_initialize(event, event_class,
		(bt_validation_flag_copy_field_type_func) bt_field_type_copy,
		(create_field_func) bt_field_create_recursive,
		(release_field_func) bt_field_destroy_recursive,
		(create_header_field_func) create_event_header_field,
		(release_header_field_func) bt_event_header_field_recycle);
	if (ret) {
		/* bt_event_initialize() logs errors */
		goto error;
	}

	stream_class = bt_event_class_borrow_stream_class(event_class);
	BT_ASSERT(stream_class);
	ret = bt_clock_value_set_initialize(&event->cv_set);
	if (ret) {
		goto error;
	}

	goto end;

error:
	if (event) {
		bt_event_destroy(event);
		event = NULL;
	}

end:
	return event;
}

struct bt_event_class *bt_event_borrow_class(struct bt_event *event)
{
	BT_ASSERT_PRE_NON_NULL(event, "Event");
	return event->class;
}

struct bt_stream *bt_event_borrow_stream(struct bt_event *event)
{
	BT_ASSERT_PRE_NON_NULL(event, "Event");
	return event->packet ? event->packet->stream : NULL;
}

struct bt_field *bt_event_borrow_payload(struct bt_event *event)
{
	struct bt_field *payload = NULL;

	BT_ASSERT_PRE_NON_NULL(event, "Event");

	if (!event->payload_field) {
		BT_LOGV("Event has no current payload field: addr=%p, "
			"event-class-name=\"%s\", event-class-id=%" PRId64,
			event, bt_event_class_get_name(event->class),
			bt_event_class_get_id(event->class));
		goto end;
	}

	payload = event->payload_field;

end:
	return payload;
}

struct bt_field *bt_event_borrow_header(struct bt_event *event)
{
	struct bt_field *header = NULL;

	BT_ASSERT_PRE_NON_NULL(event, "Event");

	if (!event->header_field) {
		BT_LOGV("Event has no current header field: addr=%p, "
			"event-class-name=\"%s\", event-class-id=%" PRId64,
			event, bt_event_class_get_name(event->class),
			bt_event_class_get_id(event->class));
		goto end;
	}

	header = event->header_field->field;

end:
	return header;
}

struct bt_field *bt_event_borrow_context(struct bt_event *event)
{
	struct bt_field *context = NULL;

	BT_ASSERT_PRE_NON_NULL(event, "Event");

	if (!event->context_field) {
		BT_LOGV("Event has no current context field: addr=%p, "
			"event-class-name=\"%s\", event-class-id=%" PRId64,
			event, bt_event_class_get_name(event->class),
			bt_event_class_get_id(event->class));
		goto end;
	}

	context = event->context_field;

end:
	return context;
}

struct bt_field *bt_event_borrow_stream_event_context(
		struct bt_event *event)
{
	struct bt_field *stream_event_context = NULL;

	BT_ASSERT_PRE_NON_NULL(event, "Event");

	if (!event->stream_event_context_field) {
		BT_LOGV("Event has no current stream event context field: addr=%p, "
			"event-class-name=\"%s\", event-class-id=%" PRId64,
			event, bt_event_class_get_name(event->class),
			bt_event_class_get_id(event->class));
		goto end;
	}

	stream_event_context = event->stream_event_context_field;

end:
	return stream_event_context;
}

static
void release_event_header_field(struct bt_field_wrapper *field_wrapper,
		struct bt_event *event)
{
	struct bt_event_class *event_class = bt_event_borrow_class(event);

	if (!event_class) {
		bt_field_wrapper_destroy(field_wrapper);
	} else {
		struct bt_stream_class *stream_class =
			bt_event_class_borrow_stream_class(event_class);

		BT_ASSERT(stream_class);
		bt_event_header_field_recycle(field_wrapper, stream_class);
	}
}

static inline
void bt_event_finalize(struct bt_object *obj,
		void (*field_release_func)(void *),
		void (*header_field_release_func)(void *, struct bt_event *))
{
	struct bt_event *event = (void *) obj;

	BT_LOGD("Destroying event: addr=%p, "
		"event-class-name=\"%s\", event-class-id=%" PRId64,
		event,
		event->class ? bt_event_class_get_name(event->class) : NULL,
		event->class ? bt_event_class_get_id(event->class) : INT64_C(-1));

	if (event->header_field) {
		BT_LOGD_STR("Releasing event's header field.");
		header_field_release_func(event->header_field, event);
	}

	if (event->stream_event_context_field) {
		BT_LOGD_STR("Releasing event's stream event context field.");
		field_release_func(event->stream_event_context_field);
	}

	if (event->context_field) {
		BT_LOGD_STR("Releasing event's context field.");
		field_release_func(event->context_field);
	}

	if (event->payload_field) {
		BT_LOGD_STR("Releasing event's payload field.");
		field_release_func(event->payload_field);
	}

	/*
	 * Leave this after calling header_field_release_func() because
	 * this function receives the event object and could need its
	 * class to perform some cleanup.
	 */
	if (!event->base.parent) {
		/*
		 * Event was keeping a reference to its class since it shared no
		 * common ancestor with it to guarantee they would both have the
		 * same lifetime.
		 */
		bt_put(event->class);
	}
}

BT_HIDDEN
void bt_event_destroy(struct bt_event *event)
{
	BT_ASSERT(event);
	bt_event_finalize((void *) event,
		(void *) bt_field_destroy_recursive,
		(void *) release_event_header_field);
	bt_clock_value_set_finalize(&event->cv_set);
	BT_LOGD_STR("Putting event's packet.");
	bt_put(event->packet);
	g_free(event);
}

int bt_event_set_clock_value(struct bt_event *event,
		struct bt_clock_class *clock_class, uint64_t raw_value,
		bt_bool is_default)
{
	BT_ASSERT_PRE_NON_NULL(event, "Event");
	BT_ASSERT_PRE_NON_NULL(clock_class, "Clock class");
	BT_ASSERT_PRE_HOT(event, "Event", ": %!+e", event);
	BT_ASSERT_PRE(is_default,
		"You can only set a default clock value as of this version.");
	return bt_clock_value_set_set_clock_value(&event->cv_set, clock_class,
		raw_value, is_default);
}

struct bt_clock_value *bt_event_borrow_default_clock_value(
		struct bt_event *event)
{
	struct bt_clock_value *clock_value = NULL;

	BT_ASSERT_PRE_NON_NULL(event, "Event");
	clock_value = event->cv_set.default_cv;
	if (!clock_value) {
		BT_LIB_LOGV("No default clock value: %![event-]+e", event);
	}

	return clock_value;
}

struct bt_packet *bt_event_borrow_packet(struct bt_event *event)
{
	struct bt_packet *packet = NULL;

	BT_ASSERT_PRE_NON_NULL(event, "Event");
	if (!event->packet) {
		BT_LOGV("Event has no current packet: addr=%p, "
			"event-class-name=\"%s\", event-class-id=%" PRId64,
			event, bt_event_class_get_name(event->class),
			bt_event_class_get_id(event->class));
		goto end;
	}

	packet = event->packet;

end:
	return packet;
}

int bt_event_move_header(struct bt_event *event,
		struct bt_event_header_field *header_field)
{
	struct bt_stream_class *stream_class;
	struct bt_field_wrapper *field_wrapper = (void *) header_field;

	BT_ASSERT_PRE_NON_NULL(event, "Event");
	BT_ASSERT_PRE_NON_NULL(field_wrapper, "Header field");
	BT_ASSERT_PRE_HOT(event, "Event", ": %!+e", event);
	stream_class = bt_event_class_borrow_stream_class(
		bt_event_borrow_class(event));
	BT_ASSERT_PRE(stream_class->event_header_field_type,
		"Stream class has no event header field type: %!+S",
		stream_class);

	/* Recycle current header field: always exists */
	BT_ASSERT(event->header_field);
	bt_event_header_field_recycle(event->header_field,
		stream_class);

	/* Move new field */
	event->header_field = (void *) field_wrapper;
	return 0;
}
