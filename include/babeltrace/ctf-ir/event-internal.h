#ifndef BABELTRACE_CTF_IR_EVENT_INTERNAL_H
#define BABELTRACE_CTF_IR_EVENT_INTERNAL_H

/*
 * Babeltrace - CTF IR: Event internal
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

#include <babeltrace/assert-pre-internal.h>
#include <babeltrace/babeltrace-internal.h>
#include <babeltrace/values.h>
#include <babeltrace/ctf-ir/stream-class.h>
#include <babeltrace/ctf-ir/stream.h>
#include <babeltrace/ctf-ir/packet.h>
#include <babeltrace/ctf-ir/fields.h>
#include <babeltrace/ctf-ir/fields-internal.h>
#include <babeltrace/ctf-ir/event-class-internal.h>
#include <babeltrace/ctf-ir/validation-internal.h>
#include <babeltrace/object-internal.h>
#include <babeltrace/assert-internal.h>
#include <glib.h>

struct bt_stream_pos;

struct bt_event_common {
	struct bt_object base;
	struct bt_event_class_common *class;
	struct bt_field_common *header_field;
	struct bt_field_common *stream_event_context_field;
	struct bt_field_common *context_field;
	struct bt_field_common *payload_field;
	int frozen;
};

struct bt_event {
	struct bt_event_common common;

	/* Maps clock classes to bt_clock_value. */
	GHashTable *clock_values;
	struct bt_packet *packet;
};

BT_HIDDEN
int _bt_event_common_validate(struct bt_event_common *event);

BT_HIDDEN
void _bt_event_common_freeze(struct bt_event_common *event);

BT_HIDDEN
void _bt_event_freeze(struct bt_event *event);

#ifdef BT_DEV_MODE
# define bt_event_common_validate		_bt_event_common_validate
# define bt_event_common_freeze			_bt_event_common_freeze
# define bt_event_freeze			_bt_event_freeze
#else
# define bt_event_common_validate(_event)	0
# define bt_event_common_freeze(_event)
# define bt_event_freeze(_event)
#endif

#define BT_ASSERT_PRE_EVENT_COMMON_HOT(_event, _name)			\
	BT_ASSERT_PRE_HOT((_event), (_name), ": +%!+_e", (_event))

static inline struct bt_packet *bt_event_borrow_packet(struct bt_event *event)
{
	BT_ASSERT(event);
	return event->packet;
}

BT_HIDDEN
struct bt_stream *bt_event_borrow_stream(struct bt_event *event);

static inline
struct bt_event_class_common *bt_event_common_borrow_class(
		struct bt_event_common *event)
{
	BT_ASSERT(event);
	return event->class;
}

static inline
struct bt_event_class *bt_event_borrow_class(struct bt_event *event)
{
	return BT_FROM_COMMON(bt_event_common_borrow_class(
		BT_TO_COMMON(event)));
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
		void *(*create_field_func)(void *));

static inline
struct bt_field_common *bt_event_common_get_payload(
		struct bt_event_common *event)
{
	struct bt_field_common *payload = NULL;

	BT_ASSERT_PRE_NON_NULL(event, "Event");

	if (!event->payload_field) {
		BT_LOGV("Event has no current payload field: addr=%p, "
			"event-class-name=\"%s\", event-class-id=%" PRId64,
			event, bt_event_class_common_get_name(event->class),
			bt_event_class_common_get_id(event->class));
		goto end;
	}

	payload = event->payload_field;
	bt_get(payload);

end:
	return payload;
}

static inline
int bt_event_common_set_payload(struct bt_event_common *event,
		struct bt_field_common *payload)
{
	BT_ASSERT_PRE_NON_NULL(event, "Event");
	BT_ASSERT_PRE_EVENT_COMMON_HOT(event, "Event");

	if (payload) {
		BT_ASSERT_PRE(bt_field_type_common_compare(payload->type,
			event->class->payload_field_type) == 0,
			"Payload field's type is different from the "
			"expected field type: %![event-]+_e, %![ft-]+_F, "
			"%![expected-ft-]+_F",
			event, payload->type,
			event->class->payload_field_type);
	} else {
		BT_ASSERT_PRE(!event->class->payload_field_type,
			"Setting no event payload field, "
			"but event payload field type is not NULL: ",
			"%![event-]+_e, %![payload-ft-]+_F",
			event, event->class->payload_field_type);
	}

	bt_put(event->payload_field);
	event->payload_field = bt_get(payload);
	BT_LOGV("Set event's payload field: event-addr=%p, "
		"event-class-name=\"%s\", event-class-id=%" PRId64 ", "
		"payload-field-addr=%p",
		event, bt_event_class_common_get_name(event->class),
		bt_event_class_common_get_id(event->class), payload);
	return 0;
}

static inline
struct bt_field_common *bt_event_common_get_header(
		struct bt_event_common *event)
{
	struct bt_field_common *header = NULL;

	BT_ASSERT_PRE_NON_NULL(event, "Event");

	if (!event->header_field) {
		BT_LOGV("Event has no current header field: addr=%p, "
			"event-class-name=\"%s\", event-class-id=%" PRId64,
			event, bt_event_class_common_get_name(event->class),
			bt_event_class_common_get_id(event->class));
		goto end;
	}

	header = event->header_field;
	bt_get(header);

end:
	return header;
}

static inline
int bt_event_common_set_header(struct bt_event_common *event,
		struct bt_field_common *header)
{
	BT_ASSERT_PRE_NON_NULL(event, "Event");
	BT_ASSERT_PRE_EVENT_COMMON_HOT(event, "Event");

	/*
	 * Ensure the provided header's type matches the one registered to the
	 * stream class.
	 */
	if (header) {
		BT_ASSERT_PRE(bt_field_type_common_compare(header->type,
			bt_event_class_common_borrow_stream_class(event->class)->event_header_field_type) == 0,
			"Header field's type is different from the "
			"expected field type: %![event-]+_e, %![ft-]+_F, "
			"%![expected-ft-]+_F",
			event, header->type,
			bt_event_class_common_borrow_stream_class(event->class)->event_header_field_type);
	} else {
		BT_ASSERT_PRE(!bt_event_class_common_borrow_stream_class(event->class)->event_header_field_type,
			"Setting no event header field, "
			"but event header field type is not NULL: ",
			"%![event-]+_e, %![header-ft-]+_F",
			event,
			bt_event_class_common_borrow_stream_class(event->class)->event_header_field_type);
	}

	bt_put(event->header_field);
	event->header_field = bt_get(header);
	BT_LOGV("Set event's header field: event-addr=%p, "
		"event-class-name=\"%s\", event-class-id=%" PRId64 ", "
		"header-field-addr=%p",
		event, bt_event_class_common_get_name(event->class),
		bt_event_class_common_get_id(event->class), header);
	return 0;
}

static inline
struct bt_field_common *bt_event_common_get_context(
		struct bt_event_common *event)
{
	struct bt_field_common *context = NULL;

	BT_ASSERT_PRE_NON_NULL(event, "Event");

	if (!event->context_field) {
		BT_LOGV("Event has no current context field: addr=%p, "
			"event-class-name=\"%s\", event-class-id=%" PRId64,
			event, bt_event_class_common_get_name(event->class),
			bt_event_class_common_get_id(event->class));
		goto end;
	}

	context = event->context_field;
	bt_get(context);

end:
	return context;
}

static inline
int bt_event_common_set_context(struct bt_event_common *event,
		struct bt_field_common *context)
{
	BT_ASSERT_PRE_NON_NULL(event, "Event");
	BT_ASSERT_PRE_EVENT_COMMON_HOT(event, "Event");

	if (context) {
		BT_ASSERT_PRE(bt_field_type_common_compare(context->type,
			event->class->context_field_type) == 0,
			"Context field's type is different from the "
			"expected field type: %![event-]+_e, %![ft-]+_F, "
			"%![expected-ft-]+_F",
			event, context->type, event->class->context_field_type);
	} else {
		BT_ASSERT_PRE(!event->class->context_field_type,
			"Setting no event context field, "
			"but event context field type is not NULL: ",
			"%![event-]+_e, %![context-ft-]+_F",
			event, event->class->context_field_type);
	}

	bt_put(event->context_field);
	event->context_field = bt_get(context);
	BT_LOGV("Set event's context field: event-addr=%p, "
		"event-class-name=\"%s\", event-class-id=%" PRId64 ", "
		"context-field-addr=%p",
		event, bt_event_class_common_get_name(event->class),
		bt_event_class_common_get_id(event->class), context);
	return 0;
}

static inline
struct bt_field_common *bt_event_common_get_stream_event_context(
		struct bt_event_common *event)
{
	struct bt_field_common *stream_event_context = NULL;

	BT_ASSERT_PRE_NON_NULL(event, "Event");

	if (!event->stream_event_context_field) {
		BT_LOGV("Event has no current stream event context field: addr=%p, "
			"event-class-name=\"%s\", event-class-id=%" PRId64,
			event, bt_event_class_common_get_name(event->class),
			bt_event_class_common_get_id(event->class));
		goto end;
	}

	stream_event_context = event->stream_event_context_field;

end:
	return bt_get(stream_event_context);
}

static inline
int bt_event_common_set_stream_event_context(struct bt_event_common *event,
		struct bt_field_common *stream_event_context)
{
	BT_ASSERT_PRE_NON_NULL(event, "Event");
	BT_ASSERT_PRE_EVENT_COMMON_HOT(event, "Event");

	if (stream_event_context) {
		BT_ASSERT_PRE(bt_field_type_common_compare(stream_event_context->type,
			bt_event_class_common_borrow_stream_class(event->class)->event_context_field_type) == 0,
			"Stream event context field's type is different from the "
			"expected field type: %![event-]+_e, %![ft-]+_F, "
			"%![expected-ft-]+_F",
			event, stream_event_context->type,
			bt_event_class_common_borrow_stream_class(event->class)->event_context_field_type);
	} else {
		BT_ASSERT_PRE(!bt_event_class_common_borrow_stream_class(event->class)->event_context_field_type,
			"Setting no stream event context field, "
			"but stream event context field type is not NULL: ",
			"%![event-]+_e, %![context-ft-]+_F",
			event,
			bt_event_class_common_borrow_stream_class(event->class)->event_context_field_type);
	}

	bt_get(stream_event_context);
	BT_MOVE(event->stream_event_context_field, stream_event_context);
	BT_LOGV("Set event's stream event context field: event-addr=%p, "
		"event-class-name=\"%s\", event-class-id=%" PRId64 ", "
		"stream-event-context-field-addr=%p",
		event, bt_event_class_common_get_name(event->class),
		bt_event_class_common_get_id(event->class),
		stream_event_context);
	return 0;
}

static inline
void bt_event_common_finalize(struct bt_object *obj)
{
	struct bt_event_common *event = (void *) obj;

	BT_LOGD("Destroying event: addr=%p, "
		"event-class-name=\"%s\", event-class-id=%" PRId64,
		event, bt_event_class_common_get_name(event->class),
		bt_event_class_common_get_id(event->class));

	if (!event->base.parent) {
		/*
		 * Event was keeping a reference to its class since it shared no
		 * common ancestor with it to guarantee they would both have the
		 * same lifetime.
		 */
		bt_put(event->class);
	}

	bt_put(event->header_field);
	BT_LOGD_STR("Putting event's stream event context field.");
	bt_put(event->stream_event_context_field);
	BT_LOGD_STR("Putting event's context field.");
	bt_put(event->context_field);
	BT_LOGD_STR("Putting event's payload field.");
	bt_put(event->payload_field);
	BT_LOGD_STR("Putting event's packet.");
}

#endif /* BABELTRACE_CTF_IR_EVENT_INTERNAL_H */
