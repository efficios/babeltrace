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
#include <babeltrace/ctf-ir/packet-internal.h>
#include <babeltrace/ctf-ir/fields.h>
#include <babeltrace/ctf-ir/fields-internal.h>
#include <babeltrace/ctf-ir/event-class-internal.h>
#include <babeltrace/ctf-ir/clock-value-internal.h>
#include <babeltrace/ctf-ir/field-wrapper-internal.h>
#include <babeltrace/ctf-ir/validation-internal.h>
#include <babeltrace/object-internal.h>
#include <babeltrace/assert-internal.h>
#include <glib.h>

struct bt_stream_pos;

struct bt_event_common {
	struct bt_object base;
	struct bt_event_class_common *class;
	struct bt_field_wrapper *header_field;
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
void _bt_event_common_set_is_frozen(struct bt_event_common *event,
		bool is_frozen);

BT_HIDDEN
void _bt_event_set_is_frozen(struct bt_event *event, bool is_frozen);

#ifdef BT_DEV_MODE
# define bt_event_common_validate	_bt_event_common_validate
# define bt_event_common_set_is_frozen	_bt_event_common_set_is_frozen
# define bt_event_set_is_frozen		_bt_event_set_is_frozen
#else
# define bt_event_common_validate(_event)			0
# define bt_event_common_set_is_frozen(_event, _is_frozen)
# define bt_event_set_is_frozen(_event, _is_frozen)
#endif

#define BT_ASSERT_PRE_EVENT_COMMON_HOT(_event, _name)			\
	BT_ASSERT_PRE_HOT((_event), (_name), ": +%!+_e", (_event))

static inline
struct bt_event_class_common *bt_event_common_borrow_class(
		struct bt_event_common *event)
{
	BT_ASSERT(event);
	return event->class;
}

typedef void *(*create_field_func)(void *);
typedef void (*release_field_func)(void *);
typedef void *(*create_header_field_func)(void *, void *);
typedef void (*release_header_field_func)(void *, void *);

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
		release_header_field_func release_header_field_func);

static inline
struct bt_field_common *bt_event_common_borrow_payload(
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

end:
	return payload;
}

static inline
struct bt_field_common *bt_event_common_borrow_header(
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

	header = event->header_field->field;

end:
	return header;
}

static inline
struct bt_field_common *bt_event_common_borrow_context(
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

end:
	return context;
}

static inline
struct bt_field_common *bt_event_common_borrow_stream_event_context(
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
	return stream_event_context;
}

static inline
void bt_event_common_finalize(struct bt_object *obj,
		void (*field_release_func)(void *),
		void (*header_field_release_func)(void *, struct bt_event_common *))
{
	struct bt_event_common *event = (void *) obj;

	BT_LOGD("Destroying event: addr=%p, "
		"event-class-name=\"%s\", event-class-id=%" PRId64,
		event,
		event->class ? bt_event_class_common_get_name(event->class) : NULL,
		event->class ? bt_event_class_common_get_id(event->class) : INT64_C(-1));

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

BT_UNUSED
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

BT_HIDDEN
void bt_event_destroy(struct bt_event *event);

static inline
void bt_event_reset(struct bt_event *event)
{
	BT_ASSERT(event);
	bt_event_set_is_frozen(event, false);
	bt_event_reset_dev_mode(event);
	bt_object_put_no_null_check(&event->packet->base);
	event->packet = NULL;
}

static inline
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
	bt_object_put_no_null_check(&event_class->common.base);
}

static inline
void bt_event_set_packet(struct bt_event *event, struct bt_packet *packet)
{
	BT_ASSERT_PRE_NON_NULL(event, "Event");
	BT_ASSERT_PRE_NON_NULL(packet, "Packet");
	BT_ASSERT_PRE_EVENT_COMMON_HOT(BT_TO_COMMON(event), "Event");

#ifdef BT_DEV_MODE
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
#endif

	BT_ASSERT(!event->packet);
	event->packet = packet;
	bt_object_get_no_null_check_no_parent_check(&event->packet->base);
	BT_LOGV("Set event's packet: event-addr=%p, "
		"event-class-name=\"%s\", event-class-id=%" PRId64 ", "
		"packet-addr=%p",
		event, bt_event_class_common_get_name(event->common.class),
		bt_event_class_common_get_id(event->common.class), packet);
}

static inline
struct bt_event *bt_event_create(struct bt_event_class *event_class,
		struct bt_packet *packet)
{
	struct bt_event *event = NULL;

	BT_ASSERT(event_class);
	event = bt_object_pool_create_object(&event_class->event_pool);
	if (unlikely(!event)) {
		BT_LIB_LOGE("Cannot allocate one event from event class's event pool: "
			"%![event-class-]+E", event_class);
		goto end;
	}

	if (unlikely(!event->common.class)) {
		event->common.class = BT_TO_COMMON(event_class);
		bt_object_get_no_null_check(&event_class->common.base);
	}

	BT_ASSERT(packet);
	bt_event_set_packet(event, packet);
	goto end;

end:
	return event;
}

BT_HIDDEN
struct bt_event *bt_event_new(struct bt_event_class *event_class);

#endif /* BABELTRACE_CTF_IR_EVENT_INTERNAL_H */
