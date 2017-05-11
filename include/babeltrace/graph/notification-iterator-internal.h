#ifndef BABELTRACE_COMPONENT_NOTIFICATION_ITERATOR_INTERNAL_H
#define BABELTRACE_COMPONENT_NOTIFICATION_ITERATOR_INTERNAL_H

/*
 * BabelTrace - Notification Iterator Internal
 *
 * Copyright 2015 Jérémie Galarneau <jeremie.galarneau@efficios.com>
 * Copyright 2017 Philippe Proulx <pproulx@efficios.com>
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

#include <babeltrace/babeltrace-internal.h>
#include <babeltrace/object-internal.h>
#include <babeltrace/ref-internal.h>
#include <babeltrace/graph/notification.h>
#include <babeltrace/graph/notification-iterator.h>
#include <babeltrace/graph/private-notification-iterator.h>
#include <babeltrace/types.h>

struct bt_port;

enum bt_notification_iterator_notif_type {
	BT_NOTIFICATION_ITERATOR_NOTIF_TYPE_EVENT =		(1U << 0),
	BT_NOTIFICATION_ITERATOR_NOTIF_TYPE_INACTIVITY =	(1U << 1),
	BT_NOTIFICATION_ITERATOR_NOTIF_TYPE_STREAM_BEGIN =	(1U << 2),
	BT_NOTIFICATION_ITERATOR_NOTIF_TYPE_STREAM_END =	(1U << 3),
	BT_NOTIFICATION_ITERATOR_NOTIF_TYPE_PACKET_BEGIN =	(1U << 4),
	BT_NOTIFICATION_ITERATOR_NOTIF_TYPE_PACKET_END =	(1U << 5),
};

struct bt_notification_iterator {
	struct bt_object base;
	struct bt_component *upstream_component; /* owned by this */
	struct bt_port *upstream_port; /* owned by this */
	struct bt_notification *current_notification; /* owned by this */
	GQueue *queue; /* struct bt_notification * (owned by this) */

	/*
	 * This hash table keeps the state of a stream as viewed by
	 * this notification iterator. This is used to:
	 *
	 * * Automatically enqueue "stream begin", "packet begin",
	 *   "packet end", and "stream end" notifications depending
	 *   on the stream's state and on the next notification returned
	 *   by the upstream component.
	 *
	 * * Make sure that, once the notification iterator has seen
	 *   a "stream end" notification for a given stream, that no
	 *   other notifications which refer to this stream can be
	 *   delivered by this iterator.
	 *
	 * The key (struct bt_ctf_stream *) is not owned by this. The
	 * value is an allocated state structure.
	 */
	GHashTable *stream_states;

	/*
	 * This is an array of actions which can be rolled back. It's
	 * similar to the memento pattern, but it's not exactly that. It
	 * is allocated once and reset for each notification to process.
	 * More details near the implementation.
	 */
	GArray *actions;

	/*
	 * This is a mask of notifications to which the user of this
	 * iterator is subscribed
	 * (see enum bt_notification_iterator_notif_type above).
	 */
	uint32_t subscription_mask;

	bt_bool is_ended;
	void *user_data;
};

static inline
struct bt_notification_iterator *bt_notification_iterator_from_private(
		struct bt_private_notification_iterator *private_notification_iterator)
{
	return (void *) private_notification_iterator;
}

static inline
struct bt_private_notification_iterator *
bt_private_notification_iterator_from_notification_iterator(
		struct bt_notification_iterator *notification_iterator)
{
	return (void *) notification_iterator;
}

/**
 * Allocate a notification iterator.
 *
 * @param component		Component instance
 * @returns			A notification iterator instance
 */
BT_HIDDEN
struct bt_notification_iterator *bt_notification_iterator_create(
		struct bt_component *upstream_component,
		struct bt_port *upstream_port,
		const enum bt_notification_type *notification_types);

/**
 * Validate a notification iterator.
 *
 * @param iterator		Notification iterator instance
 * @returns			One of #bt_component_status values
 */
BT_HIDDEN
enum bt_notification_iterator_status bt_notification_iterator_validate(
		struct bt_notification_iterator *iterator);

#endif /* BABELTRACE_COMPONENT_NOTIFICATION_ITERATOR_INTERNAL_H */
