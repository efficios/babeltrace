#ifndef BABELTRACE_GRAPH_NOTIFICATION_ITERATOR_INTERNAL_H
#define BABELTRACE_GRAPH_NOTIFICATION_ITERATOR_INTERNAL_H

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
#include <babeltrace/graph/connection.h>
#include <babeltrace/graph/notification.h>
#include <babeltrace/graph/notification-iterator.h>
#include <babeltrace/graph/private-connection-private-notification-iterator.h>
#include <babeltrace/types.h>
#include <babeltrace/assert-internal.h>
#include <stdbool.h>

struct bt_port;
struct bt_graph;

enum bt_notification_iterator_type {
	BT_NOTIFICATION_ITERATOR_TYPE_PRIVATE_CONNECTION,
	BT_NOTIFICATION_ITERATOR_TYPE_OUTPUT_PORT,
};

enum bt_private_connection_notification_iterator_notif_type {
	BT_PRIVATE_CONNECTION_NOTIFICATION_ITERATOR_NOTIF_TYPE_EVENT =			(1U << 0),
	BT_PRIVATE_CONNECTION_NOTIFICATION_ITERATOR_NOTIF_TYPE_INACTIVITY =		(1U << 1),
	BT_PRIVATE_CONNECTION_NOTIFICATION_ITERATOR_NOTIF_TYPE_STREAM_BEGIN =		(1U << 2),
	BT_PRIVATE_CONNECTION_NOTIFICATION_ITERATOR_NOTIF_TYPE_STREAM_END =		(1U << 3),
	BT_PRIVATE_CONNECTION_NOTIFICATION_ITERATOR_NOTIF_TYPE_PACKET_BEGIN =		(1U << 4),
	BT_PRIVATE_CONNECTION_NOTIFICATION_ITERATOR_NOTIF_TYPE_PACKET_END =		(1U << 5),
	BT_PRIVATE_CONNECTION_NOTIFICATION_ITERATOR_NOTIF_TYPE_DISCARDED_EVENTS =	(1U << 6),
	BT_PRIVATE_CONNECTION_NOTIFICATION_ITERATOR_NOTIF_TYPE_DISCARDED_PACKETS =	(1U << 7),
};

enum bt_private_connection_notification_iterator_state {
	/* Iterator is not initialized. */
	BT_PRIVATE_CONNECTION_NOTIFICATION_ITERATOR_STATE_NON_INITIALIZED,

	/* Iterator is active, not at the end yet, and not finalized. */
	BT_PRIVATE_CONNECTION_NOTIFICATION_ITERATOR_STATE_ACTIVE,

	/*
	 * Iterator is ended, not finalized yet: the "next" method
	 * returns BT_NOTIFICATION_ITERATOR_STATUS_END.
	 */
	BT_PRIVATE_CONNECTION_NOTIFICATION_ITERATOR_STATE_ENDED,

	/*
	 * Iterator is finalized, but not at the end yet. This means
	 * that the "next" method can still return queued notifications
	 * before returning the BT_NOTIFICATION_ITERATOR_STATUS_CANCELED
	 * status.
	 */
	BT_PRIVATE_CONNECTION_NOTIFICATION_ITERATOR_STATE_FINALIZED,

	/*
	 * Iterator is finalized and ended: the "next" method always
	 * returns BT_NOTIFICATION_ITERATOR_STATUS_CANCELED.
	 */
	BT_PRIVATE_CONNECTION_NOTIFICATION_ITERATOR_STATE_FINALIZED_AND_ENDED,
};

struct bt_notification_iterator {
	struct bt_object base;
	enum bt_notification_iterator_type type;
	GPtrArray *notifs;
};

struct bt_notification_iterator_private_connection {
	struct bt_notification_iterator base;
	struct bt_component *upstream_component; /* Weak */
	struct bt_port *upstream_port; /* Weak */
	struct bt_connection *connection; /* Weak */
	struct bt_graph *graph; /* Weak */

	/*
	 * This hash table keeps the state of a stream as viewed by
	 * this notification iterator. This is used to, in developer
	 * mode:
	 *
	 * * Automatically enqueue "stream begin", "packet begin",
	 *   "packet end", and "stream end" notifications depending
	 *   on the stream's state and on the next notification returned
	 *   by the upstream component.
	 *
	 * * Make sure that, once the notification iterator has seen a
	 *   "stream end" notification for a given stream, no other
	 *   notifications which refer to this stream can be delivered
	 *   by this iterator.
	 *
	 * The key (struct bt_stream *) is not owned by this. The
	 * value is an allocated state structure.
	 */
	GHashTable *stream_states;

	enum bt_private_connection_notification_iterator_state state;
	void *user_data;
};

struct bt_notification_iterator_output_port {
	struct bt_notification_iterator base;
	struct bt_graph *graph; /* Owned by this */
	struct bt_component *colander; /* Owned by this */

	/*
	 * Only used temporarily as a bridge between a colander sink and
	 * the user.
	 */
	uint64_t count;
};

static inline
struct bt_private_connection_private_notification_iterator *
bt_private_connection_private_notification_iterator_from_notification_iterator(
		struct bt_notification_iterator_private_connection *iterator)
{
	return (void *) iterator;
}

BT_HIDDEN
enum bt_connection_status bt_private_connection_notification_iterator_create(
		struct bt_component *upstream_comp,
		struct bt_port *upstream_port,
		struct bt_connection *connection,
		struct bt_notification_iterator_private_connection **iterator);

BT_HIDDEN
void bt_private_connection_notification_iterator_finalize(
		struct bt_notification_iterator_private_connection *iterator);

BT_HIDDEN
void bt_private_connection_notification_iterator_set_connection(
		struct bt_notification_iterator_private_connection *iterator,
		struct bt_connection *connection);

static inline
const char *bt_notification_iterator_status_string(
		enum bt_notification_iterator_status status)
{
	switch (status) {
	case BT_NOTIFICATION_ITERATOR_STATUS_CANCELED:
		return "BT_NOTIFICATION_ITERATOR_STATUS_CANCELED";
	case BT_NOTIFICATION_ITERATOR_STATUS_AGAIN:
		return "BT_NOTIFICATION_ITERATOR_STATUS_AGAIN";
	case BT_NOTIFICATION_ITERATOR_STATUS_END:
		return "BT_NOTIFICATION_ITERATOR_STATUS_END";
	case BT_NOTIFICATION_ITERATOR_STATUS_OK:
		return "BT_NOTIFICATION_ITERATOR_STATUS_OK";
	case BT_NOTIFICATION_ITERATOR_STATUS_INVALID:
		return "BT_NOTIFICATION_ITERATOR_STATUS_INVALID";
	case BT_NOTIFICATION_ITERATOR_STATUS_ERROR:
		return "BT_NOTIFICATION_ITERATOR_STATUS_ERROR";
	case BT_NOTIFICATION_ITERATOR_STATUS_NOMEM:
		return "BT_NOTIFICATION_ITERATOR_STATUS_NOMEM";
	case BT_NOTIFICATION_ITERATOR_STATUS_UNSUPPORTED:
		return "BT_NOTIFICATION_ITERATOR_STATUS_UNSUPPORTED";
	default:
		return "(unknown)";
	}
};

static inline
const char *bt_private_connection_notification_iterator_state_string(
		enum bt_private_connection_notification_iterator_state state)
{
	switch (state) {
	case BT_PRIVATE_CONNECTION_NOTIFICATION_ITERATOR_STATE_ACTIVE:
		return "BT_PRIVATE_CONNECTION_NOTIFICATION_ITERATOR_STATE_ACTIVE";
	case BT_PRIVATE_CONNECTION_NOTIFICATION_ITERATOR_STATE_ENDED:
		return "BT_PRIVATE_CONNECTION_NOTIFICATION_ITERATOR_STATE_ENDED";
	case BT_PRIVATE_CONNECTION_NOTIFICATION_ITERATOR_STATE_FINALIZED:
		return "BT_PRIVATE_CONNECTION_NOTIFICATION_ITERATOR_STATE_FINALIZED";
	case BT_PRIVATE_CONNECTION_NOTIFICATION_ITERATOR_STATE_FINALIZED_AND_ENDED:
		return "BT_PRIVATE_CONNECTION_NOTIFICATION_ITERATOR_STATE_FINALIZED_AND_ENDED";
	default:
		return "(unknown)";
	}
};

#endif /* BABELTRACE_GRAPH_NOTIFICATION_ITERATOR_INTERNAL_H */
