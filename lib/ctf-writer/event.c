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

#include <babeltrace/ctf-ir/event-internal.h>
#include <babeltrace/ctf-writer/clock-internal.h>
#include <babeltrace/ctf-writer/event-internal.h>
#include <babeltrace/ctf-writer/event.h>
#include <babeltrace/ctf-writer/fields-internal.h>
#include <babeltrace/ctf-writer/field-types-internal.h>
#include <babeltrace/ctf-writer/field-types.h>
#include <babeltrace/ctf-writer/fields.h>
#include <babeltrace/ctf-writer/stream-class-internal.h>
#include <babeltrace/ctf-writer/stream-class.h>
#include <babeltrace/ctf-writer/trace-internal.h>
#include <babeltrace/ctf-writer/trace.h>
#include <babeltrace/ref.h>
#include <babeltrace/compiler-internal.h>
#include <babeltrace/assert-internal.h>
#include <babeltrace/assert-pre-internal.h>
#include <inttypes.h>

static
void bt_ctf_event_destroy(struct bt_object *obj);

static
int map_clock_classes_func(struct bt_stream_class_common *stream_class,
		struct bt_field_type_common *packet_context_type,
		struct bt_field_type_common *event_header_type)
{
	int ret = bt_ctf_stream_class_map_clock_class(
		BT_FROM_COMMON(stream_class),
		BT_FROM_COMMON(packet_context_type),
		BT_FROM_COMMON(event_header_type));

	if (ret) {
		BT_LOGW_STR("Cannot automatically map selected stream class's field types to stream class's clock's class.");
	}

	return ret;
}

static
void destroy_event_header_field(struct bt_field_wrapper *field_wrapper)
{
	BT_ASSERT(field_wrapper);
	bt_put(field_wrapper->field);
	bt_field_wrapper_destroy(field_wrapper);
}

static
struct bt_field_wrapper *create_event_header_field(
		struct bt_stream_class *stream_class,
		struct bt_field_type_common *ft)
{
	struct bt_field_wrapper *field_wrapper = NULL;
	struct bt_ctf_field *field = bt_ctf_field_create((void *) ft);

	if (!field) {
		goto error;
	}

	field_wrapper = bt_field_wrapper_new(NULL);
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

struct bt_ctf_event *bt_ctf_event_create(struct bt_ctf_event_class *event_class)
{
	int ret;
	struct bt_ctf_event *event = NULL;
	struct bt_clock_class *expected_clock_class = NULL;

	event = g_new0(struct bt_ctf_event, 1);
	if (!event) {
		BT_LOGE_STR("Failed to allocate one CTF writer event.");
		goto error;
	}

	if (event_class) {
		struct bt_ctf_stream_class *stream_class =
			BT_FROM_COMMON(bt_event_class_common_borrow_stream_class(
				BT_TO_COMMON(event_class)));

		if (stream_class && stream_class->clock) {
			expected_clock_class = BT_TO_COMMON(
				stream_class->clock->clock_class);
		}
	}

	ret = bt_event_common_initialize(BT_TO_COMMON(event),
		BT_TO_COMMON(event_class), expected_clock_class,
		true, bt_ctf_event_destroy,
		(bt_validation_flag_copy_field_type_func)
			bt_ctf_field_type_copy,
		false, map_clock_classes_func,
		(create_field_func) bt_ctf_field_create,
		(release_field_func) bt_put,
		(create_header_field_func) create_event_header_field,
		(release_header_field_func) destroy_event_header_field);
	if (ret) {
		/* bt_event_common_initialize() logs errors */
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
	return bt_get(bt_event_common_borrow_class(BT_TO_COMMON(event)));
}

BT_HIDDEN
struct bt_ctf_stream *bt_ctf_event_borrow_stream(struct bt_ctf_event *event)
{
	BT_ASSERT(event);
	return (struct bt_ctf_stream *)
		bt_object_borrow_parent(&BT_TO_COMMON(event)->base);
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
	BT_ASSERT_PRE_EVENT_COMMON_HOT(BT_TO_COMMON(event), "Event");
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
			BT_FROM_COMMON(event->common.payload_field), name);
	} else {
		field = BT_FROM_COMMON(event->common.payload_field);
		bt_get(field);
	}

	return field;
}

struct bt_ctf_field *bt_ctf_event_get_payload_field(
		struct bt_ctf_event *event)
{
	return bt_get(bt_event_common_borrow_payload(BT_TO_COMMON(event)));
}

struct bt_ctf_field *bt_ctf_event_get_header(struct bt_ctf_event *event)
{
	return bt_get(bt_event_common_borrow_header(BT_TO_COMMON(event)));
}

struct bt_ctf_field *bt_ctf_event_get_context(struct bt_ctf_event *event)
{
	return bt_get(bt_event_common_borrow_context(BT_TO_COMMON(event)));
}

struct bt_ctf_field *bt_ctf_event_get_stream_event_context(
		struct bt_ctf_event *event)
{
	return bt_get(bt_event_common_borrow_stream_event_context(
		BT_TO_COMMON(event)));
}

static
void release_event_header_field(struct bt_field_wrapper *field_wrapper,
		struct bt_event_common *event_common)
{
	BT_ASSERT(field_wrapper);
	BT_PUT(field_wrapper->field);
	bt_field_wrapper_destroy(field_wrapper);
}

static
void bt_ctf_event_destroy(struct bt_object *obj)
{
	bt_event_common_finalize(obj, (void *) bt_put,
		(void *) release_event_header_field);
	g_free(obj);
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
				bt_event_class_common_get_name(event->common.class),
				bt_event_class_common_get_id(event->common.class));
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
				bt_event_class_common_get_name(event->common.class),
				bt_event_class_common_get_id(event->common.class));
			goto end;
		}
	}

end:
	return ret;
}

BT_HIDDEN
void _bt_ctf_event_freeze(struct bt_ctf_event *event)
{
	_bt_event_common_freeze(BT_TO_COMMON(event));
}

int bt_ctf_event_set_header(struct bt_ctf_event *event,
		struct bt_ctf_field *header)
{
	BT_ASSERT_PRE_NON_NULL(event, "Event");
	BT_ASSERT_PRE_EVENT_COMMON_HOT(BT_TO_COMMON(event), "Event");

	/*
	 * Ensure the provided header's type matches the one registered to the
	 * stream class.
	 */
	if (header) {
		BT_ASSERT_PRE(bt_field_type_common_compare(
			((struct bt_field_common *) header)->type,
			bt_event_class_common_borrow_stream_class(event->common.class)->event_header_field_type) == 0,
			"Header field's type is different from the "
			"expected field type: %![event-]+we, %![ft-]+wF, "
			"%![expected-ft-]+wF",
			event, ((struct bt_field_common *) header)->type,
			bt_event_class_common_borrow_stream_class(event->common.class)->event_header_field_type);
	} else {
		BT_ASSERT_PRE(!bt_event_class_common_borrow_stream_class(event->common.class)->event_header_field_type,
			"Setting no event header field, "
			"but event header field type is not NULL: ",
			"%![event-]+we, %![header-ft-]+wF",
			event,
			bt_event_class_common_borrow_stream_class(event->common.class)->event_header_field_type);
	}

	bt_put(event->common.header_field->field);
	event->common.header_field->field = bt_get(header);
	BT_LOGV("Set event's header field: event-addr=%p, "
		"event-class-name=\"%s\", event-class-id=%" PRId64 ", "
		"header-field-addr=%p",
		event, bt_event_class_common_get_name(event->common.class),
		bt_event_class_common_get_id(event->common.class), header);
	return 0;
}

int bt_ctf_event_common_set_payload(struct bt_ctf_event *event,
		struct bt_ctf_field *payload)
{
	BT_ASSERT_PRE_NON_NULL(event, "Event");
	BT_ASSERT_PRE_EVENT_COMMON_HOT(BT_TO_COMMON(event), "Event");

	if (payload) {
		BT_ASSERT_PRE(bt_field_type_common_compare(
			((struct bt_field_common *) payload)->type,
			event->common.class->payload_field_type) == 0,
			"Payload field's type is different from the "
			"expected field type: %![event-]+we, %![ft-]+wF, "
			"%![expected-ft-]+wF",
			event,
			((struct bt_field_common *) payload)->type,
			event->common.class->payload_field_type);
	} else {
		BT_ASSERT_PRE(!event->common.class->payload_field_type,
			"Setting no event payload field, "
			"but event payload field type is not NULL: ",
			"%![event-]+we, %![payload-ft-]+wF",
			event, event->common.class->payload_field_type);
	}

	bt_put(event->common.payload_field);
	event->common.payload_field = bt_get(payload);
	BT_LOGV("Set event's payload field: event-addr=%p, "
		"event-class-name=\"%s\", event-class-id=%" PRId64 ", "
		"payload-field-addr=%p",
		event, bt_event_class_common_get_name(event->common.class),
		bt_event_class_common_get_id(event->common.class), payload);
	return 0;
}

int bt_ctf_event_set_context(struct bt_ctf_event *event,
		struct bt_ctf_field *context)
{
	BT_ASSERT_PRE_NON_NULL(event, "Event");
	BT_ASSERT_PRE_EVENT_COMMON_HOT(BT_TO_COMMON(event), "Event");

	if (context) {
		BT_ASSERT_PRE(bt_field_type_common_compare(
			((struct bt_field_common *) context)->type,
			event->common.class->context_field_type) == 0,
			"Context field's type is different from the "
			"expected field type: %![event-]+we, %![ft-]+wF, "
			"%![expected-ft-]+wF",
			event, ((struct bt_field_common *) context)->type,
			event->common.class->context_field_type);
	} else {
		BT_ASSERT_PRE(!event->common.class->context_field_type,
			"Setting no event context field, "
			"but event context field type is not NULL: ",
			"%![event-]+we, %![context-ft-]+wF",
			event, event->common.class->context_field_type);
	}

	bt_put(event->common.context_field);
	event->common.context_field = bt_get(context);
	BT_LOGV("Set event's context field: event-addr=%p, "
		"event-class-name=\"%s\", event-class-id=%" PRId64 ", "
		"context-field-addr=%p",
		event, bt_event_class_common_get_name(event->common.class),
		bt_event_class_common_get_id(event->common.class), context);
	return 0;
}

int bt_ctf_event_set_stream_event_context(struct bt_ctf_event *event,
		struct bt_ctf_field *stream_event_context)
{
	BT_ASSERT_PRE_NON_NULL(event, "Event");
	BT_ASSERT_PRE_EVENT_COMMON_HOT(BT_TO_COMMON(event), "Event");

	if (stream_event_context) {
		BT_ASSERT_PRE(bt_field_type_common_compare(
			((struct bt_field_common *) stream_event_context)->type,
			bt_event_class_common_borrow_stream_class(event->common.class)->event_context_field_type) == 0,
			"Stream event context field's type is different from the "
			"expected field type: %![event-]+we, %![ft-]+wF, "
			"%![expected-ft-]+wF",
			event,
			((struct bt_field_common *) stream_event_context)->type,
			bt_event_class_common_borrow_stream_class(event->common.class)->event_context_field_type);
	} else {
		BT_ASSERT_PRE(!bt_event_class_common_borrow_stream_class(event->common.class)->event_context_field_type,
			"Setting no stream event context field, "
			"but stream event context field type is not NULL: ",
			"%![event-]+we, %![context-ft-]+wF",
			event,
			bt_event_class_common_borrow_stream_class(event->common.class)->event_context_field_type);
	}

	bt_put(event->common.stream_event_context_field);
	event->common.stream_event_context_field = bt_get(stream_event_context);
	BT_LOGV("Set event's stream event context field: event-addr=%p, "
		"event-class-name=\"%s\", event-class-id=%" PRId64 ", "
		"stream-event-context-field-addr=%p",
		event, bt_event_class_common_get_name(event->common.class),
		bt_event_class_common_get_id(event->common.class),
		stream_event_context);
	return 0;
}
