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

#define BT_LOG_TAG "CTF-WRITER-EVENT"
#include <babeltrace/lib-logging-internal.h>

#include <babeltrace/assert-pre-internal.h>
#include <babeltrace/assert-internal.h>
#include <babeltrace/compiler-internal.h>
#include <babeltrace/ctf-writer/attributes-internal.h>
#include <babeltrace/ctf-writer/clock-class-internal.h>
#include <babeltrace/ctf-writer/clock-internal.h>
#include <babeltrace/ctf-writer/event-class-internal.h>
#include <babeltrace/ctf-writer/event-internal.h>
#include <babeltrace/ctf-writer/event.h>
#include <babeltrace/ctf-writer/field-types-internal.h>
#include <babeltrace/ctf-writer/field-types.h>
#include <babeltrace/ctf-writer/fields-internal.h>
#include <babeltrace/ctf-writer/fields.h>
#include <babeltrace/ctf-writer/stream-class-internal.h>
#include <babeltrace/ctf-writer/stream-class.h>
#include <babeltrace/ctf-writer/stream-internal.h>
#include <babeltrace/ctf-writer/trace-internal.h>
#include <babeltrace/ctf-writer/trace.h>
#include <babeltrace/ctf-writer/utils.h>
#include <babeltrace/ctf-writer/validation-internal.h>
#include <babeltrace/ref.h>
#include <inttypes.h>

static
int bt_ctf_event_common_validate_types_for_create(
		struct bt_ctf_event_class_common *event_class,
		struct bt_ctf_validation_output *validation_output,
		bt_ctf_validation_flag_copy_field_type_func copy_field_type_func)
{
	int ret;
	enum bt_ctf_validation_flag validation_flags =
		BT_CTF_VALIDATION_FLAG_STREAM |
		BT_CTF_VALIDATION_FLAG_EVENT;
	struct bt_ctf_trace_common *trace = NULL;
	struct bt_ctf_stream_class_common *stream_class = NULL;
	struct bt_ctf_field_type_common *packet_header_type = NULL;
	struct bt_ctf_field_type_common *packet_context_type = NULL;
	struct bt_ctf_field_type_common *event_header_type = NULL;
	struct bt_ctf_field_type_common *stream_event_ctx_type = NULL;
	struct bt_ctf_field_type_common *event_context_type = NULL;
	struct bt_ctf_field_type_common *event_payload_type = NULL;
	int trace_valid = 0;
	struct bt_value *environment = NULL;

	stream_class = bt_ctf_event_class_common_borrow_stream_class(event_class);
	BT_ASSERT(stream_class);
	trace = bt_ctf_stream_class_common_borrow_trace(stream_class);
	if (trace) {
		BT_LOGD_STR("Event class is part of a trace.");
		packet_header_type =
			bt_ctf_trace_common_borrow_packet_header_field_type(trace);
		trace_valid = trace->valid;
		BT_ASSERT(trace_valid);
		environment = trace->environment;
	}

	packet_context_type =
		bt_ctf_stream_class_common_borrow_packet_context_field_type(
			stream_class);
	event_header_type =
		bt_ctf_stream_class_common_borrow_event_header_field_type(
			stream_class);
	stream_event_ctx_type =
		bt_ctf_stream_class_common_borrow_event_context_field_type(
			stream_class);
	event_context_type =
		bt_ctf_event_class_common_borrow_context_field_type(event_class);
	event_payload_type =
		bt_ctf_event_class_common_borrow_payload_field_type(event_class);
	ret = bt_ctf_validate_class_types(environment, packet_header_type,
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
	bt_ctf_validation_output_put_types(validation_output);
	ret = -1;

end:
	return ret;
}

static
int bt_ctf_event_common_create_fields(
		struct bt_ctf_stream_class_common *stream_class,
		struct bt_ctf_validation_output *validation_output,
		create_field_func create_field_func,
		release_field_func release_field_func,
		create_header_field_func create_header_field_func,
		release_header_field_func release_header_field_func,
		struct bt_ctf_field_wrapper **header_field,
		struct bt_ctf_field_common **stream_event_context_field,
		struct bt_ctf_field_common **context_field,
		struct bt_ctf_field_common **payload_field)
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
int _bt_ctf_event_common_validate(struct bt_ctf_event_common *event)
{
	int ret = 0;
	struct bt_ctf_stream_class_common *stream_class;

	BT_ASSERT(event);
	if (event->header_field) {
		ret = bt_ctf_field_common_validate_recursive(
			event->header_field->field);
		if (ret) {
			BT_ASSERT_PRE_MSG("Invalid event's header field: "
				"event-addr=%p, field-addr=%p",
				event, event->header_field->field);
			goto end;
		}
	}

	stream_class = bt_ctf_event_class_common_borrow_stream_class(event->class);

	/*
	 * We should not have been able to create the event without associating
	 * the event class to a stream class.
	 */
	BT_ASSERT(stream_class);

	if (stream_class->event_context_field_type) {
		ret = bt_ctf_field_common_validate_recursive(
			event->stream_event_context_field);
		if (ret) {
			BT_ASSERT_PRE_MSG("Invalid event's stream event context field: "
				"event-addr=%p, field-addr=%p",
				event, event->stream_event_context_field);
			goto end;
		}
	}

	if (event->class->context_field_type) {
		ret = bt_ctf_field_common_validate_recursive(event->context_field);
		if (ret) {
			BT_ASSERT_PRE_MSG("Invalid event's payload field: "
				"event-addr=%p, field-addr=%p",
				event, event->context_field);
			goto end;
		}
	}

	ret = bt_ctf_field_common_validate_recursive(event->payload_field);
	if (ret) {
		BT_ASSERT_PRE_MSG("Invalid event's payload field: "
			"event-addr=%p, field-addr=%p",
			event, event->payload_field);
		goto end;
	}

end:
	return ret;
}

BT_HIDDEN
void _bt_ctf_event_common_set_is_frozen(struct bt_ctf_event_common *event,
		bool is_frozen)
{
	BT_ASSERT(event);
	BT_LOGD("Freezing event: addr=%p, "
		"event-class-name=\"%s\", event-class-id=%" PRId64,
		event, bt_ctf_event_class_common_get_name(event->class),
		bt_ctf_event_class_common_get_id(event->class));

	if (event->header_field) {
		BT_LOGD_STR("Freezing event's header field.");
		bt_ctf_field_common_set_is_frozen_recursive(
			event->header_field->field, is_frozen);
	}

	if (event->stream_event_context_field) {
		BT_LOGD_STR("Freezing event's stream event context field.");
		bt_ctf_field_common_set_is_frozen_recursive(
			event->stream_event_context_field, is_frozen);
	}

	if (event->context_field) {
		BT_LOGD_STR("Freezing event's context field.");
		bt_ctf_field_common_set_is_frozen_recursive(event->context_field,
			is_frozen);
	}

	if (event->payload_field) {
		BT_LOGD_STR("Freezing event's payload field.");
		bt_ctf_field_common_set_is_frozen_recursive(event->payload_field,
			is_frozen);
	}

	event->frozen = is_frozen;
}

BT_HIDDEN
int bt_ctf_event_common_initialize(struct bt_ctf_event_common *event,
		struct bt_ctf_event_class_common *event_class,
		struct bt_ctf_clock_class *init_expected_clock_class,
		bool is_shared_with_parent, bt_object_release_func release_func,
		bt_ctf_validation_flag_copy_field_type_func field_type_copy_func,
		bool must_be_in_trace,
		int (*map_clock_classes_func)(struct bt_ctf_stream_class_common *stream_class,
			struct bt_ctf_field_type_common *packet_context_field_type,
			struct bt_ctf_field_type_common *event_header_field_type),
		create_field_func create_field_func,
		release_field_func release_field_func,
		create_header_field_func create_header_field_func,
		release_header_field_func release_header_field_func)
{
	int ret;
	struct bt_ctf_trace_common *trace = NULL;
	struct bt_ctf_stream_class_common *stream_class = NULL;
	struct bt_ctf_field_wrapper *event_header = NULL;
	struct bt_ctf_field_common *stream_event_context = NULL;
	struct bt_ctf_field_common *event_context = NULL;
	struct bt_ctf_field_common *event_payload = NULL;
	struct bt_ctf_validation_output validation_output = { 0 };
	struct bt_ctf_clock_class *expected_clock_class =
		init_expected_clock_class ? bt_get(init_expected_clock_class) :
		NULL;

	BT_ASSERT_PRE_NON_NULL(event_class, "Event class");
	BT_LOGD("Initializing common event object: event-class-addr=%p, "
		"event-class-name=\"%s\", event-class-id=%" PRId64,
		event_class, bt_ctf_event_class_common_get_name(event_class),
		bt_ctf_event_class_common_get_id(event_class));

	stream_class = bt_ctf_event_class_common_borrow_stream_class(event_class);
	BT_ASSERT_PRE(stream_class,
		"Event class is not part of a stream class: event-class-addr=%p",
		event_class);

	/* The event class was frozen when added to its stream class */
	BT_ASSERT(event_class->frozen);
	trace = bt_ctf_stream_class_common_borrow_trace(stream_class);

	if (must_be_in_trace) {
		BT_ASSERT_PRE(trace,
			"Event class's stream class is not part of a trace: "
			"ec-addr=%p, sc-addr=%p", event_class, stream_class);
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
		ret = bt_ctf_stream_class_common_validate_single_clock_class(
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
				bt_ctf_stream_class_common_get_id(stream_class),
				bt_ctf_stream_class_common_get_name(stream_class),
				expected_clock_class,
				expected_clock_class ?
					bt_ctf_clock_class_get_name(expected_clock_class) :
					NULL);
			goto error;
		}
	}

	/* Validate the trace, the stream class, and the event class */
	ret = bt_ctf_event_common_validate_types_for_create(
		event_class, &validation_output, field_type_copy_func);
	if (ret) {
		/* bt_ctf_event_common_validate_types_for_create() logs errors */
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

	ret = bt_ctf_event_common_create_fields(stream_class,
		&validation_output,
		create_field_func, release_field_func,
		create_header_field_func, release_header_field_func,
		&event_header, &stream_event_context, &event_context,
		&event_payload);
	if (ret) {
		/* bt_ctf_event_common_create_fields() logs errors */
		goto error;
	}

	/*
	 * At this point all the fields are created, potentially from
	 * validated copies of field types, so that the field types and
	 * fields can be replaced in the trace, stream class,
	 * event class, and created event.
	 */
	bt_ctf_validation_replace_types(trace, stream_class, event_class,
		&validation_output,
		BT_CTF_VALIDATION_FLAG_STREAM | BT_CTF_VALIDATION_FLAG_EVENT);
	event->header_field = event_header;
	event_header = NULL;
	event->stream_event_context_field = stream_event_context;
	stream_event_context = NULL;
	event->context_field = event_context;
	event_context = NULL;
	event->payload_field = event_payload;
	event_payload = NULL;

	/*
	 * Put what was not moved in bt_ctf_validation_replace_types().
	 */
	bt_ctf_validation_output_put_types(&validation_output);

	/*
	 * Freeze the stream class since the event header must not be changed
	 * anymore.
	 */
	bt_ctf_stream_class_common_freeze(stream_class);

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
		event, bt_ctf_event_class_common_get_name(event->class),
		bt_ctf_event_class_common_get_id(event->class));
	goto end;

error:
	bt_ctf_validation_output_put_types(&validation_output);
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

static
void destroy_event_header_field(struct bt_ctf_field_wrapper *field_wrapper)
{
	BT_ASSERT(field_wrapper);
	bt_put(field_wrapper->field);
	bt_ctf_field_wrapper_destroy(field_wrapper);
}

static
struct bt_ctf_field_wrapper *create_event_header_field(
		struct bt_ctf_stream_class *stream_class,
		struct bt_ctf_field_type_common *ft)
{
	struct bt_ctf_field_wrapper *field_wrapper = NULL;
	struct bt_ctf_field *field = bt_ctf_field_create((void *) ft);

	if (!field) {
		goto error;
	}

	field_wrapper = bt_ctf_field_wrapper_new(NULL);
	if (!field_wrapper) {
		goto error;
	}

	field_wrapper->field = (void *) field;
	field = NULL;
	goto end;

error:
	bt_put(field);

	if (field_wrapper) {
		destroy_event_header_field(field_wrapper);
		field_wrapper = NULL;
	}

end:
	return field_wrapper;
}

static
void release_event_header_field(struct bt_ctf_field_wrapper *field_wrapper,
		struct bt_ctf_event_common *event_common)
{
	BT_ASSERT(field_wrapper);
	BT_PUT(field_wrapper->field);
	bt_ctf_field_wrapper_destroy(field_wrapper);
}

static
void bt_ctf_event_destroy(struct bt_object *obj)
{
	bt_ctf_event_common_finalize(obj, (void *) bt_put,
		(void *) release_event_header_field);
	g_free(obj);
}

struct bt_ctf_event *bt_ctf_event_create(struct bt_ctf_event_class *event_class)
{
	int ret;
	struct bt_ctf_event *event = NULL;
	struct bt_ctf_clock_class *expected_clock_class = NULL;

	event = g_new0(struct bt_ctf_event, 1);
	if (!event) {
		BT_LOGE_STR("Failed to allocate one CTF writer event.");
		goto error;
	}

	if (event_class) {
		struct bt_ctf_stream_class *stream_class =
			BT_CTF_FROM_COMMON(bt_ctf_event_class_common_borrow_stream_class(
				BT_CTF_TO_COMMON(event_class)));

		if (stream_class && stream_class->clock) {
			expected_clock_class = stream_class->clock->clock_class;
		}
	}

	ret = bt_ctf_event_common_initialize(BT_CTF_TO_COMMON(event),
		BT_CTF_TO_COMMON(event_class), expected_clock_class,
		true, bt_ctf_event_destroy,
		(bt_ctf_validation_flag_copy_field_type_func)
			bt_ctf_field_type_copy,
		false, map_clock_classes_func,
		(create_field_func) bt_ctf_field_create,
		(release_field_func) bt_put,
		(create_header_field_func) create_event_header_field,
		(release_header_field_func) destroy_event_header_field);
	if (ret) {
		/* bt_ctf_event_common_initialize() logs errors */
		goto error;
	}

	goto end;

error:
	BT_PUT(event);

end:
	return event;
}

struct bt_ctf_event_class *bt_ctf_event_get_class(struct bt_ctf_event *event)
{
	BT_ASSERT_PRE_NON_NULL(event, "Event");
	return bt_get(bt_ctf_event_common_borrow_class(BT_CTF_TO_COMMON(event)));
}

BT_HIDDEN
struct bt_ctf_stream *bt_ctf_event_borrow_stream(struct bt_ctf_event *event)
{
	BT_ASSERT(event);
	return (struct bt_ctf_stream *)
		bt_object_borrow_parent(&BT_CTF_TO_COMMON(event)->base);
}

struct bt_ctf_stream *bt_ctf_event_get_stream(struct bt_ctf_event *event)
{
	BT_ASSERT_PRE_NON_NULL(event, "Event");
	return bt_get(bt_ctf_event_borrow_stream(event));
}

int bt_ctf_event_set_payload(struct bt_ctf_event *event, const char *name,
		struct bt_ctf_field *field)
{
	BT_ASSERT_PRE_NON_NULL(event, "Event");
	BT_ASSERT_PRE_NON_NULL(field, "Payload field");
	BT_ASSERT_PRE_EVENT_COMMON_HOT(BT_CTF_TO_COMMON(event), "Event");
	return bt_ctf_field_structure_set_field_by_name(
		(void *) event->common.payload_field, name, field);
}

struct bt_ctf_field *bt_ctf_event_get_payload(struct bt_ctf_event *event,
		const char *name)
{
	struct bt_ctf_field *field = NULL;

	BT_ASSERT_PRE_NON_NULL(event, "Event");

	if (name) {
		field = bt_ctf_field_structure_get_field_by_name(
			BT_CTF_FROM_COMMON(event->common.payload_field), name);
	} else {
		field = BT_CTF_FROM_COMMON(event->common.payload_field);
		bt_get(field);
	}

	return field;
}

struct bt_ctf_field *bt_ctf_event_get_payload_field(
		struct bt_ctf_event *event)
{
	return bt_get(bt_ctf_event_common_borrow_payload(BT_CTF_TO_COMMON(event)));
}

struct bt_ctf_field *bt_ctf_event_get_header(struct bt_ctf_event *event)
{
	return bt_get(bt_ctf_event_common_borrow_header(BT_CTF_TO_COMMON(event)));
}

struct bt_ctf_field *bt_ctf_event_get_context(struct bt_ctf_event *event)
{
	return bt_get(bt_ctf_event_common_borrow_context(BT_CTF_TO_COMMON(event)));
}

struct bt_ctf_field *bt_ctf_event_get_stream_event_context(
		struct bt_ctf_event *event)
{
	return bt_get(bt_ctf_event_common_borrow_stream_event_context(
		BT_CTF_TO_COMMON(event)));
}

BT_HIDDEN
int bt_ctf_event_serialize(struct bt_ctf_event *event,
		struct bt_ctf_stream_pos *pos,
		enum bt_ctf_byte_order native_byte_order)
{
	int ret = 0;

	BT_ASSERT(event);
	BT_ASSERT(pos);

	BT_LOGV_STR("Serializing event's context field.");
	if (event->common.context_field) {
		ret = bt_ctf_field_serialize_recursive(
			(void *) event->common.context_field, pos,
			native_byte_order);
		if (ret) {
			BT_LOGW("Cannot serialize event's context field: "
				"event-addr=%p, event-class-name=\"%s\", "
				"event-class-id=%" PRId64,
				event,
				bt_ctf_event_class_common_get_name(event->common.class),
				bt_ctf_event_class_common_get_id(event->common.class));
			goto end;
		}
	}

	BT_LOGV_STR("Serializing event's payload field.");
	if (event->common.payload_field) {
		ret = bt_ctf_field_serialize_recursive(
			(void *) event->common.payload_field, pos,
			native_byte_order);
		if (ret) {
			BT_LOGW("Cannot serialize event's payload field: "
				"event-addr=%p, event-class-name=\"%s\", "
				"event-class-id=%" PRId64,
				event,
				bt_ctf_event_class_common_get_name(event->common.class),
				bt_ctf_event_class_common_get_id(event->common.class));
			goto end;
		}
	}

end:
	return ret;
}

BT_HIDDEN
void _bt_ctf_event_freeze(struct bt_ctf_event *event)
{
	_bt_ctf_event_common_set_is_frozen(BT_CTF_TO_COMMON(event), true);
}

int bt_ctf_event_set_header(struct bt_ctf_event *event,
		struct bt_ctf_field *header)
{
	BT_ASSERT_PRE_NON_NULL(event, "Event");
	BT_ASSERT_PRE_EVENT_COMMON_HOT(BT_CTF_TO_COMMON(event), "Event");

	/*
	 * Ensure the provided header's type matches the one registered to the
	 * stream class.
	 */
	if (header) {
		BT_ASSERT_PRE(bt_ctf_field_type_common_compare(
			((struct bt_ctf_field_common *) header)->type,
			bt_ctf_event_class_common_borrow_stream_class(event->common.class)->event_header_field_type) == 0,
			"Header field's type is different from the "
			"expected field type: event-addr=%p, ft-addr=%p, "
			"expected-ft-addr=%p",
			event, ((struct bt_ctf_field_common *) header)->type,
			bt_ctf_event_class_common_borrow_stream_class(event->common.class)->event_header_field_type);
	} else {
		BT_ASSERT_PRE(!bt_ctf_event_class_common_borrow_stream_class(event->common.class)->event_header_field_type,
			"Setting no event header field, "
			"but event header field type is not NULL: ",
			"event-addr=%p, header-ft-addr=%p",
			event,
			bt_ctf_event_class_common_borrow_stream_class(event->common.class)->event_header_field_type);
	}

	bt_put(event->common.header_field->field);
	event->common.header_field->field = bt_get(header);
	BT_LOGV("Set event's header field: event-addr=%p, "
		"event-class-name=\"%s\", event-class-id=%" PRId64 ", "
		"header-field-addr=%p",
		event, bt_ctf_event_class_common_get_name(event->common.class),
		bt_ctf_event_class_common_get_id(event->common.class), header);
	return 0;
}

int bt_ctf_event_common_set_payload(struct bt_ctf_event *event,
		struct bt_ctf_field *payload)
{
	BT_ASSERT_PRE_NON_NULL(event, "Event");
	BT_ASSERT_PRE_EVENT_COMMON_HOT(BT_CTF_TO_COMMON(event), "Event");

	if (payload) {
		BT_ASSERT_PRE(bt_ctf_field_type_common_compare(
			((struct bt_ctf_field_common *) payload)->type,
			event->common.class->payload_field_type) == 0,
			"Payload field's type is different from the "
			"expected field type: event-addr=%p, ft-addr=%p, "
			"expected-ft-addr=%p",
			event,
			((struct bt_ctf_field_common *) payload)->type,
			event->common.class->payload_field_type);
	} else {
		BT_ASSERT_PRE(!event->common.class->payload_field_type,
			"Setting no event payload field, "
			"but event payload field type is not NULL: ",
			"event-addr=%p, payload-ft-addr=%p",
			event, event->common.class->payload_field_type);
	}

	bt_put(event->common.payload_field);
	event->common.payload_field = bt_get(payload);
	BT_LOGV("Set event's payload field: event-addr=%p, "
		"event-class-name=\"%s\", event-class-id=%" PRId64 ", "
		"payload-field-addr=%p",
		event, bt_ctf_event_class_common_get_name(event->common.class),
		bt_ctf_event_class_common_get_id(event->common.class), payload);
	return 0;
}

int bt_ctf_event_set_context(struct bt_ctf_event *event,
		struct bt_ctf_field *context)
{
	BT_ASSERT_PRE_NON_NULL(event, "Event");
	BT_ASSERT_PRE_EVENT_COMMON_HOT(BT_CTF_TO_COMMON(event), "Event");

	if (context) {
		BT_ASSERT_PRE(bt_ctf_field_type_common_compare(
			((struct bt_ctf_field_common *) context)->type,
			event->common.class->context_field_type) == 0,
			"Context field's type is different from the "
			"expected field type: event-addr=%p, ft-addr=%p, "
			"expected-ft-addr=%p",
			event, ((struct bt_ctf_field_common *) context)->type,
			event->common.class->context_field_type);
	} else {
		BT_ASSERT_PRE(!event->common.class->context_field_type,
			"Setting no event context field, "
			"but event context field type is not NULL: ",
			"event-addr=%p, context-ft-addr=%p",
			event, event->common.class->context_field_type);
	}

	bt_put(event->common.context_field);
	event->common.context_field = bt_get(context);
	BT_LOGV("Set event's context field: event-addr=%p, "
		"event-class-name=\"%s\", event-class-id=%" PRId64 ", "
		"context-field-addr=%p",
		event, bt_ctf_event_class_common_get_name(event->common.class),
		bt_ctf_event_class_common_get_id(event->common.class), context);
	return 0;
}

int bt_ctf_event_set_stream_event_context(struct bt_ctf_event *event,
		struct bt_ctf_field *stream_event_context)
{
	BT_ASSERT_PRE_NON_NULL(event, "Event");
	BT_ASSERT_PRE_EVENT_COMMON_HOT(BT_CTF_TO_COMMON(event), "Event");

	if (stream_event_context) {
		BT_ASSERT_PRE(bt_ctf_field_type_common_compare(
			((struct bt_ctf_field_common *) stream_event_context)->type,
			bt_ctf_event_class_common_borrow_stream_class(event->common.class)->event_context_field_type) == 0,
			"Stream event context field's type is different from the "
			"expected field type: event-addr=%p, ft-addr=%p, "
			"expected-ft-addr=%p",
			event,
			((struct bt_ctf_field_common *) stream_event_context)->type,
			bt_ctf_event_class_common_borrow_stream_class(event->common.class)->event_context_field_type);
	} else {
		BT_ASSERT_PRE(!bt_ctf_event_class_common_borrow_stream_class(event->common.class)->event_context_field_type,
			"Setting no stream event context field, "
			"but stream event context field type is not NULL: ",
			"event-addr=%p, context-ft-addr=%p",
			event,
			bt_ctf_event_class_common_borrow_stream_class(event->common.class)->event_context_field_type);
	}

	bt_put(event->common.stream_event_context_field);
	event->common.stream_event_context_field = bt_get(stream_event_context);
	BT_LOGV("Set event's stream event context field: event-addr=%p, "
		"event-class-name=\"%s\", event-class-id=%" PRId64 ", "
		"stream-event-context-field-addr=%p",
		event, bt_ctf_event_class_common_get_name(event->common.class),
		bt_ctf_event_class_common_get_id(event->common.class),
		stream_event_context);
	return 0;
}
