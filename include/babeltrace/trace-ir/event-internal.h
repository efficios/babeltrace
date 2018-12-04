#ifndef BABELTRACE_TRACE_IR_EVENT_INTERNAL_H
#define BABELTRACE_TRACE_IR_EVENT_INTERNAL_H

/*
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

/* Protection: this file uses BT_LIB_LOG*() macros directly */
#ifndef BABELTRACE_LIB_LOGGING_INTERNAL_H
# error Please define include <babeltrace/lib-logging-internal.h> before including this file.
#endif

#include <babeltrace/assert-pre-internal.h>
#include <babeltrace/babeltrace-internal.h>
#include <babeltrace/values.h>
#include <babeltrace/trace-ir/clock-value-internal.h>
#include <babeltrace/trace-ir/stream-class.h>
#include <babeltrace/trace-ir/stream.h>
#include <babeltrace/trace-ir/stream-internal.h>
#include <babeltrace/trace-ir/packet.h>
#include <babeltrace/trace-ir/packet-internal.h>
#include <babeltrace/trace-ir/fields.h>
#include <babeltrace/trace-ir/fields-internal.h>
#include <babeltrace/trace-ir/event-class-internal.h>
#include <babeltrace/trace-ir/field-wrapper-internal.h>
#include <babeltrace/object-internal.h>
#include <babeltrace/assert-internal.h>
#include <glib.h>

#define BT_ASSERT_PRE_EVENT_HOT(_event) \
	BT_ASSERT_PRE_HOT(((const struct bt_event *) (_event)), 	\
		"Event", ": %!+e", (_event))

struct bt_event {
	struct bt_object base;
	struct bt_event_class *class;
	struct bt_packet *packet;
	struct bt_field_wrapper *header_field;
	struct bt_field *common_context_field;
	struct bt_field *specific_context_field;
	struct bt_field *payload_field;
	struct bt_clock_value *default_cv;
	bool frozen;
};

BT_HIDDEN
void bt_event_destroy(struct bt_event *event);

BT_HIDDEN
struct bt_event *bt_event_new(struct bt_event_class *event_class);

BT_HIDDEN
void _bt_event_set_is_frozen(const struct bt_event *event, bool is_frozen);

#ifdef BT_DEV_MODE
# define bt_event_set_is_frozen		_bt_event_set_is_frozen
#else
# define bt_event_set_is_frozen(_event, _is_frozen)
#endif

BT_UNUSED
static inline
void _bt_event_reset_dev_mode(struct bt_event *event)
{
	BT_ASSERT(event);

	if (event->header_field) {
		bt_field_set_is_frozen(
			event->header_field->field, false);
		bt_field_reset(
			event->header_field->field);
	}

	if (event->common_context_field) {
		bt_field_set_is_frozen(
			event->common_context_field, false);
		bt_field_reset(
			event->common_context_field);
	}

	if (event->specific_context_field) {
		bt_field_set_is_frozen(
			event->specific_context_field, false);
		bt_field_reset(event->specific_context_field);
	}

	if (event->payload_field) {
		bt_field_set_is_frozen(
			event->payload_field, false);
		bt_field_reset(event->payload_field);
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
	BT_LIB_LOGD("Resetting event: %!+e", event);
	bt_event_set_is_frozen(event, false);

	if (event->default_cv) {
		bt_clock_value_reset(event->default_cv);
	}

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
	 *    we put the event class reference because this bt_object_put_ref()
	 *    could destroy the event class, also destroying its
	 *    event pool, thus also destroying our event object (this
	 *    would result in an invalid write access).
	 *
	 * 3. Recycle the event object.
	 *
	 * 4. Put our event class reference.
	 */
	bt_event_reset(event);
	event_class = event->class;
	BT_ASSERT(event_class);
	event->class = NULL;
	bt_object_pool_recycle_object(&event_class->event_pool, event);
	bt_object_put_no_null_check(&event_class->base);
}

static inline
void bt_event_set_packet(struct bt_event *event, struct bt_packet *packet)
{
	BT_ASSERT_PRE_NON_NULL(event, "Event");
	BT_ASSERT_PRE_NON_NULL(packet, "Packet");
	BT_ASSERT_PRE_EVENT_HOT(event);
	BT_ASSERT_PRE(bt_event_class_borrow_stream_class(
		event->class) == packet->stream->class,
		"Packet's stream class and event's stream class differ: "
		"%![event-]+e, %![packet-]+a", event, packet);

	BT_ASSERT(!event->packet);
	event->packet = packet;
	bt_object_get_no_null_check_no_parent_check(&event->packet->base);
	BT_LIB_LOGV("Set event's packet: %![event-]+e, %![packet-]+a",
		event, packet);
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
			"%![ec-]+E", event_class);
		goto end;
	}

	if (likely(!event->class)) {
		event->class = event_class;
		bt_object_get_no_null_check(&event_class->base);
	}

	BT_ASSERT(packet);
	bt_event_set_packet(event, packet);
	goto end;

end:
	return event;
}

#endif /* BABELTRACE_TRACE_IR_EVENT_INTERNAL_H */
