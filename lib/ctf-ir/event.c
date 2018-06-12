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
		packet_header_type =
			bt_trace_common_borrow_packet_header_field_type(trace);
		trace_valid = trace->valid;
		BT_ASSERT(trace_valid);
		environment = trace->environment;
	}

	packet_context_type =
		bt_stream_class_common_borrow_packet_context_field_type(
			stream_class);
	event_header_type =
		bt_stream_class_common_borrow_event_header_field_type(
			stream_class);
	stream_event_ctx_type =
		bt_stream_class_common_borrow_event_context_field_type(
			stream_class);
	event_context_type =
		bt_event_class_common_borrow_context_field_type(event_class);
	event_payload_type =
		bt_event_class_common_borrow_payload_field_type(event_class);
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
int bt_event_common_create_fields(
		struct bt_stream_class_common *stream_class,
		struct bt_validation_output *validation_output,
		create_field_func create_field_func,
		release_field_func release_field_func,
		create_header_field_func create_header_field_func,
		release_header_field_func release_header_field_func,
		struct bt_field_wrapper **header_field,
		struct bt_field_common **stream_event_context_field,
		struct bt_field_common **context_field,
		struct bt_field_common **payload_field)
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
int _bt_event_common_validate(struct bt_event_common *event)
{
	int ret = 0;
	struct bt_stream_class_common *stream_class;

	BT_ASSERT(event);
	if (event->header_field) {
		ret = bt_field_common_validate_recursive(
			event->header_field->field);
		if (ret) {
			BT_ASSERT_PRE_MSG("Invalid event's header field: "
				"%![event-]+_e, %![field-]+_f",
				event, event->header_field->field);
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

	if (event->header_field) {
		BT_LOGD_STR("Freezing event's header field.");
		bt_field_common_set_is_frozen_recursive(
			event->header_field->field, true);
	}

	if (event->stream_event_context_field) {
		BT_LOGD_STR("Freezing event's stream event context field.");
		bt_field_common_set_is_frozen_recursive(
			event->stream_event_context_field, true);
	}

	if (event->context_field) {
		BT_LOGD_STR("Freezing event's context field.");
		bt_field_common_set_is_frozen_recursive(event->context_field, true);
	}

	if (event->payload_field) {
		BT_LOGD_STR("Freezing event's payload field.");
		bt_field_common_set_is_frozen_recursive(event->payload_field, true);
	}

	event->frozen = 1;
}

BT_HIDDEN
int bt_event_common_initialize(struct bt_event_common *event,
		struct bt_event_class_common *event_class,
		struct bt_clock_class *init_expected_clock_class,
		bool is_shared_with_parent, bt_object_release_func release_func,
		bt_validation_flag_copy_field_type_func field_type_copy_func,
		bool must_be_in_trace,
		int (*map_clock_classes_func)(struct bt_stream_class_common *stream_class,
			struct bt_field_type_common *packet_context_field_type,
			struct bt_field_type_common *event_header_field_type),
		create_field_func create_field_func,
		release_field_func release_field_func,
		create_header_field_func create_header_field_func,
		release_header_field_func release_header_field_func)
{
	int ret;
	struct bt_trace_common *trace = NULL;
	struct bt_stream_class_common *stream_class = NULL;
	struct bt_field_wrapper *event_header = NULL;
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
	if (is_shared_with_parent) {
		bt_object_init_shared_with_parent(&event->base, release_func);
	} else {
		bt_object_init_unique(&event->base);
	}

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

	ret = bt_event_common_create_fields(stream_class,
		&validation_output,
		create_field_func, release_field_func,
		create_header_field_func, release_header_field_func,
		&event_header, &stream_event_context, &event_context,
		&event_payload);
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
		struct bt_field_type_common *ft)
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

	ret = bt_event_common_initialize(BT_TO_COMMON(event),
		BT_TO_COMMON(event_class), NULL, false, NULL,
		(bt_validation_flag_copy_field_type_func) bt_field_type_copy,
		true, NULL,
		(create_field_func) bt_field_create_recursive,
		(release_field_func) bt_field_destroy_recursive,
		(create_header_field_func) create_event_header_field,
		(release_header_field_func) bt_event_header_field_recycle);
	if (ret) {
		/* bt_event_common_initialize() logs errors */
		goto error;
	}

	event->clock_values = g_hash_table_new_full(g_direct_hash,
			g_direct_equal, NULL,
			(GDestroyNotify) bt_clock_value_recycle);
	BT_ASSERT(event->clock_values);
	stream_class = bt_event_class_borrow_stream_class(event_class);
	BT_ASSERT(stream_class);

	if (stream_class->common.clock_class) {
		struct bt_clock_value *clock_value;

		clock_value = bt_clock_value_create(
			stream_class->common.clock_class);
		if (!clock_value) {
			BT_LIB_LOGE("Cannot create clock value from clock class: "
				"%![cc-]+K", stream_class->common.clock_class);
			goto error;
		}

		g_hash_table_insert(event->clock_values,
			stream_class->common.clock_class, clock_value);
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

BT_ASSERT_PRE_FUNC
static inline
void _bt_event_reset_dev_mode(struct bt_event *event)
{
	GHashTableIter iter;
	gpointer key, value;

	BT_ASSERT(event);

	if (event->common.header_field) {
		bt_field_common_set_is_frozen_recursive(
			event->common.header_field->field, false);
		bt_field_common_reset_recursive(
			event->common.header_field->field);
	}

	if (event->common.stream_event_context_field) {
		bt_field_common_set_is_frozen_recursive(
			event->common.stream_event_context_field, false);
		bt_field_common_reset_recursive(
			event->common.stream_event_context_field);
	}

	if (event->common.context_field) {
		bt_field_common_set_is_frozen_recursive(
			event->common.context_field, false);
		bt_field_common_reset_recursive(event->common.context_field);
	}

	if (event->common.payload_field) {
		bt_field_common_set_is_frozen_recursive(
			event->common.payload_field, false);
		bt_field_common_reset_recursive(event->common.payload_field);
	}

	g_hash_table_iter_init(&iter, event->clock_values);
	while (g_hash_table_iter_next(&iter, &key, &value)) {
		struct bt_clock_value *clock_value = value;

		BT_ASSERT(clock_value);
		bt_clock_value_reset(clock_value);
		bt_clock_value_set_is_frozen(clock_value, false);
	}
}

#ifdef BT_DEV_MODE
# define bt_event_reset_dev_mode	_bt_event_reset_dev_mode
#else
# define bt_event_reset_dev_mode(_x)
#endif

static inline
void bt_event_reset(struct bt_event *event)
{
	BT_ASSERT(event);
	event->common.frozen = false;
	bt_event_reset_dev_mode(event);
	BT_PUT(event->packet);
}

BT_HIDDEN
struct bt_event *bt_event_create(struct bt_event_class *event_class,
		struct bt_packet *packet)
{
	int ret;
	struct bt_event *event = NULL;

	BT_ASSERT(event_class);
	event = bt_object_pool_create_object(&event_class->event_pool);
	if (!event) {
		BT_LIB_LOGE("Cannot allocate one event from event class's event pool: "
			"%![event-class-]+E", event_class);
		goto error;
	}

	if (!event->common.class) {
		event->common.class = bt_get(event_class);
	}

	BT_ASSERT(packet);
	ret = bt_event_set_packet(event, packet);
	if (ret) {
		BT_LIB_LOGE("Cannot set event's packet: "
			"%![event-]+e, %![packet-]+a", event, packet);
		goto error;
	}

	goto end;

error:
	if (event) {
		bt_event_recycle(event);
		event = NULL;
	}

end:
	return event;
}

struct bt_event_class *bt_event_borrow_class(struct bt_event *event)
{
	BT_ASSERT_PRE_NON_NULL(event, "Event");
	return BT_FROM_COMMON(
		bt_event_common_borrow_class(BT_TO_COMMON(event)));
}

struct bt_stream *bt_event_borrow_stream(struct bt_event *event)
{
	BT_ASSERT_PRE_NON_NULL(event, "Event");
	return event->packet ? event->packet->stream : NULL;
}

struct bt_field *bt_event_borrow_payload(struct bt_event *event)
{
	return BT_FROM_COMMON(
		bt_event_common_borrow_payload(BT_TO_COMMON(event)));
}

struct bt_field *bt_event_borrow_header(struct bt_event *event)
{
	return BT_FROM_COMMON(
		bt_event_common_borrow_header(BT_TO_COMMON(event)));
}

struct bt_field *bt_event_borrow_context(struct bt_event *event)
{
	return BT_FROM_COMMON(
		bt_event_common_borrow_context(BT_TO_COMMON(event)));
}

struct bt_field *bt_event_borrow_stream_event_context(
		struct bt_event *event)
{
	return BT_FROM_COMMON(bt_event_common_borrow_stream_event_context(
		BT_TO_COMMON(event)));
}

static
void release_event_header_field(struct bt_field_wrapper *field_wrapper,
		struct bt_event_common *event_common)
{
	struct bt_event *event = BT_FROM_COMMON(event_common);
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

BT_HIDDEN
void bt_event_destroy(struct bt_event *event)
{
	BT_ASSERT(event);
	bt_event_common_finalize((void *) event,
		(void *) bt_field_destroy_recursive,
		(void *) release_event_header_field);
	g_hash_table_destroy(event->clock_values);
	BT_LOGD_STR("Putting event's packet.");
	bt_put(event->packet);
	g_free(event);
}

struct bt_clock_value *bt_event_borrow_clock_value(
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

end:
	return clock_value;
}

struct bt_packet *bt_event_borrow_packet(struct bt_event *event)
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

	packet = event->packet;

end:
	return packet;
}

BT_HIDDEN
int bt_event_set_packet(struct bt_event *event, struct bt_packet *packet)
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

BT_HIDDEN
void bt_event_recycle(struct bt_event *event)
{
	struct bt_event_class *event_class;

	BT_ASSERT(event);
	BT_LIB_LOGD("Recycling event: %!+e", event);

	/*
	 * Those are the important ordered steps:
	 *
	 * 1. Reset the event object (put any permanent reference it
	 *    has, unfreeze it and its fields in developer mode, etc.),
	 *    but do NOT put its class's reference. This event class
	 *    contains the pool to which we're about to recycle this
	 *    event object, so we must guarantee its existence thanks
	 *    to this existing reference.
	 *
	 * 2. Move the event class reference to our `event_class`
	 *    variable so that we can set the event's class member
	 *    to NULL before recycling it. We CANNOT do this after
	 *    we put the event class reference because this bt_put()
	 *    could destroy the event class, also destroying its
	 *    event pool, thus also destroying our event object (this
	 *    would result in an invalid write access).
	 *
	 * 3. Recycle the event object.
	 *
	 * 4. Put our event class reference.
	 */
	bt_event_reset(event);
	event_class = BT_FROM_COMMON(event->common.class);
	BT_ASSERT(event_class);
	event->common.class = NULL;
	bt_object_pool_recycle_object(&event_class->event_pool, event);
	bt_put(event_class);
}

int bt_event_move_header(struct bt_event *event,
		struct bt_event_header_field *header_field)
{
	struct bt_stream_class *stream_class;
	struct bt_field_wrapper *field_wrapper = (void *) header_field;

	BT_ASSERT_PRE_NON_NULL(event, "Event");
	BT_ASSERT_PRE_NON_NULL(field_wrapper, "Header field");
	BT_ASSERT_PRE_HOT(BT_TO_COMMON(event), "Event", ": +%!+e", event);
	stream_class = bt_event_class_borrow_stream_class(
		bt_event_borrow_class(event));
	BT_ASSERT_PRE(stream_class->common.event_header_field_type,
		"Stream class has no event header field type: %!+S",
		stream_class);

	/* Recycle current header field: always exists */
	BT_ASSERT(event->common.header_field);
	bt_event_header_field_recycle(event->common.header_field,
		stream_class);

	/* Move new field */
	event->common.header_field = (void *) field_wrapper;
	return 0;
}
