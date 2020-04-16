/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright 2017-2018 Philippe Proulx <pproulx@efficios.com>
 * Copyright 2013, 2014 Jérémie Galarneau <jeremie.galarneau@efficios.com>
 */

#ifndef BABELTRACE_TRACE_IR_EVENT_INTERNAL_H
#define BABELTRACE_TRACE_IR_EVENT_INTERNAL_H

/* Protection: this file uses BT_LIB_LOG*() macros directly */
#ifndef BT_LIB_LOG_SUPPORTED
# error Please include "lib/logging.h" before including this file.
#endif

#include "lib/assert-cond.h"
#include "common/macros.h"
#include <babeltrace2/value.h>
#include <babeltrace2/trace-ir/stream-class.h>
#include <babeltrace2/trace-ir/stream.h>
#include <babeltrace2/trace-ir/packet.h>
#include <babeltrace2/trace-ir/field.h>
#include "lib/object.h"
#include "common/assert.h"
#include <glib.h>
#include <stdbool.h>

#include "event-class.h"
#include "field.h"
#include "field-wrapper.h"
#include "packet.h"
#include "stream.h"

#define BT_ASSERT_PRE_DEV_EVENT_HOT(_event)				\
	BT_ASSERT_PRE_DEV_HOT(((const struct bt_event *) (_event)), 	\
		"Event", ": %!+e", (_event))

struct bt_event {
	struct bt_object base;

	/* Owned by this */
	struct bt_event_class *class;

	/* Owned by this (can be `NULL`) */
	struct bt_packet *packet;

	/* Owned by this */
	struct bt_stream *stream;

	struct bt_field *common_context_field;
	struct bt_field *specific_context_field;
	struct bt_field *payload_field;
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

__attribute__((unused))
static inline
void _bt_event_reset_dev_mode(struct bt_event *event)
{
	BT_ASSERT_DBG(event);

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
	BT_ASSERT_DBG(event);
	BT_LIB_LOGD("Resetting event: %!+e", event);
	bt_event_set_is_frozen(event, false);
	bt_object_put_ref_no_null_check(&event->stream->base);
	event->stream = NULL;

	if (event->packet) {
		bt_object_put_ref_no_null_check(&event->packet->base);
		event->packet = NULL;
	}
}

static inline
void bt_event_recycle(struct bt_event *event)
{
	struct bt_event_class *event_class;

	BT_ASSERT_DBG(event);
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
	BT_ASSERT_DBG(event_class);
	event->class = NULL;
	bt_object_pool_recycle_object(&event_class->event_pool, event);
	bt_object_put_ref_no_null_check(&event_class->base);
}

#endif /* BABELTRACE_TRACE_IR_EVENT_INTERNAL_H */
