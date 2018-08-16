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
#include <babeltrace/ctf-ir/stream-internal.h>
#include <babeltrace/ctf-ir/packet.h>
#include <babeltrace/ctf-ir/packet-internal.h>
#include <babeltrace/ctf-ir/fields.h>
#include <babeltrace/ctf-ir/fields-internal.h>
#include <babeltrace/ctf-ir/event-class-internal.h>
#include <babeltrace/ctf-ir/clock-value-set-internal.h>
#include <babeltrace/ctf-ir/field-wrapper-internal.h>
#include <babeltrace/ctf-ir/validation-internal.h>
#include <babeltrace/object-internal.h>
#include <babeltrace/assert-internal.h>
#include <glib.h>

struct bt_stream_pos;

struct bt_event {
	struct bt_object base;
	struct bt_event_class *class;
	struct bt_field_wrapper *header_field;
	struct bt_field *stream_event_context_field;
	struct bt_field *context_field;
	struct bt_field *payload_field;
	struct bt_clock_value_set cv_set;
	struct bt_packet *packet;
	int frozen;
};

BT_HIDDEN
void bt_event_destroy(struct bt_event *event);

BT_HIDDEN
struct bt_event *bt_event_new(struct bt_event_class *event_class);

BT_HIDDEN
void _bt_event_set_is_frozen(struct bt_event *event, bool is_frozen);

#ifdef BT_DEV_MODE
# define bt_event_set_is_frozen		_bt_event_set_is_frozen
#else
# define bt_event_set_is_frozen(_event, _is_frozen)
#endif

#define BT_ASSERT_PRE_EVENT_HOT(_event, _name)			\
	BT_ASSERT_PRE_HOT((_event), (_name), ": %!+e", (_event))

typedef void *(*create_field_func)(void *);
typedef void (*release_field_func)(void *);
typedef void *(*create_header_field_func)(void *, void *);
typedef void (*release_header_field_func)(void *, void *);

BT_UNUSED
static inline
void _bt_event_reset_dev_mode(struct bt_event *event)
{
	BT_ASSERT(event);

	if (event->header_field) {
		bt_field_set_is_frozen_recursive(
			event->header_field->field, false);
		bt_field_reset_recursive(
			event->header_field->field);
	}

	if (event->stream_event_context_field) {
		bt_field_set_is_frozen_recursive(
			event->stream_event_context_field, false);
		bt_field_reset_recursive(
			event->stream_event_context_field);
	}

	if (event->context_field) {
		bt_field_set_is_frozen_recursive(
			event->context_field, false);
		bt_field_reset_recursive(event->context_field);
	}

	if (event->payload_field) {
		bt_field_set_is_frozen_recursive(
			event->payload_field, false);
		bt_field_reset_recursive(event->payload_field);
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
	bt_event_set_is_frozen(event, false);
	bt_clock_value_set_reset(&event->cv_set);
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
	BT_ASSERT_PRE_EVENT_HOT(event, "Event");
	BT_ASSERT_PRE(bt_event_class_borrow_stream_class(
		event->class) == packet->stream->stream_class,
		"Packet's stream class and event's stream class differ: "
		"%![event-]+e, %![packet-]+a",
		event, packet);

	BT_ASSERT(!event->packet);
	event->packet = packet;
	bt_object_get_no_null_check_no_parent_check(&event->packet->base);
	BT_LOGV("Set event's packet: event-addr=%p, "
		"event-class-name=\"%s\", event-class-id=%" PRId64 ", "
		"packet-addr=%p",
		event, bt_event_class_get_name(event->class),
		bt_event_class_get_id(event->class), packet);
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

	if (unlikely(!event->class)) {
		event->class = event_class;
		bt_object_get_no_null_check(&event_class->base);
	}

	BT_ASSERT(packet);
	bt_event_set_packet(event, packet);
	goto end;

end:
	return event;
}

#endif /* BABELTRACE_CTF_IR_EVENT_INTERNAL_H */
