#ifndef BABELTRACE_GRAPH_NOTIFICATION_ITERATOR_INTERNAL_H
#define BABELTRACE_GRAPH_NOTIFICATION_ITERATOR_INTERNAL_H

/*
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
#include <babeltrace/types.h>
#include <babeltrace/assert-internal.h>
#include <stdbool.h>

struct bt_port;
struct bt_graph;

enum bt_notification_iterator_type {
	BT_NOTIFICATION_ITERATOR_TYPE_SELF_COMPONENT_PORT_INPUT,
	BT_NOTIFICATION_ITERATOR_TYPE_PORT_OUTPUT,
};

enum bt_self_component_port_input_notification_iterator_state {
	/* Iterator is not initialized. */
	BT_SELF_COMPONENT_PORT_INPUT_NOTIFICATION_ITERATOR_STATE_NON_INITIALIZED,

	/* Iterator is active, not at the end yet, and not finalized. */
	BT_SELF_COMPONENT_PORT_INPUT_NOTIFICATION_ITERATOR_STATE_ACTIVE,

	/*
	 * Iterator is ended, not finalized yet: the "next" method
	 * returns BT_NOTIFICATION_ITERATOR_STATUS_END.
	 */
	BT_SELF_COMPONENT_PORT_INPUT_NOTIFICATION_ITERATOR_STATE_ENDED,

	/*
	 * Iterator is finalized, but not at the end yet. This means
	 * that the "next" method can still return queued notifications
	 * before returning the BT_NOTIFICATION_ITERATOR_STATUS_CANCELED
	 * status.
	 */
	BT_SELF_COMPONENT_PORT_INPUT_NOTIFICATION_ITERATOR_STATE_FINALIZED,

	/*
	 * Iterator is finalized and ended: the "next" method always
	 * returns BT_NOTIFICATION_ITERATOR_STATUS_CANCELED.
	 */
	BT_SELF_COMPONENT_PORT_INPUT_NOTIFICATION_ITERATOR_STATE_FINALIZED_AND_ENDED,
};

struct bt_notification_iterator {
	struct bt_object base;
	enum bt_notification_iterator_type type;
	GPtrArray *notifs;
};

struct bt_self_component_port_input_notification_iterator {
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

	enum bt_self_component_port_input_notification_iterator_state state;
	void *user_data;
};

struct bt_port_output_notification_iterator {
	struct bt_notification_iterator base;
	struct bt_graph *graph; /* Owned by this */
	struct bt_component_sink *colander; /* Owned by this */

	/*
	 * Only used temporarily as a bridge between a colander sink and
	 * the user.
	 */
	uint64_t count;
};

BT_HIDDEN
void bt_self_component_port_input_notification_iterator_finalize(
		struct bt_self_component_port_input_notification_iterator *iterator);

BT_HIDDEN
void bt_self_component_port_input_notification_iterator_set_connection(
		struct bt_self_component_port_input_notification_iterator *iterator,
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
	case BT_NOTIFICATION_ITERATOR_STATUS_ERROR:
		return "BT_NOTIFICATION_ITERATOR_STATUS_ERROR";
	case BT_NOTIFICATION_ITERATOR_STATUS_NOMEM:
		return "BT_NOTIFICATION_ITERATOR_STATUS_NOMEM";
	default:
		return "(unknown)";
	}
};

static inline
const char *bt_self_component_port_input_notification_iterator_state_string(
		enum bt_self_component_port_input_notification_iterator_state state)
{
	switch (state) {
	case BT_SELF_COMPONENT_PORT_INPUT_NOTIFICATION_ITERATOR_STATE_ACTIVE:
		return "BT_SELF_COMPONENT_PORT_INPUT_NOTIFICATION_ITERATOR_STATE_ACTIVE";
	case BT_SELF_COMPONENT_PORT_INPUT_NOTIFICATION_ITERATOR_STATE_ENDED:
		return "BT_SELF_COMPONENT_PORT_INPUT_NOTIFICATION_ITERATOR_STATE_ENDED";
	case BT_SELF_COMPONENT_PORT_INPUT_NOTIFICATION_ITERATOR_STATE_FINALIZED:
		return "BT_SELF_COMPONENT_PORT_INPUT_NOTIFICATION_ITERATOR_STATE_FINALIZED";
	case BT_SELF_COMPONENT_PORT_INPUT_NOTIFICATION_ITERATOR_STATE_FINALIZED_AND_ENDED:
		return "BT_SELF_COMPONENT_PORT_INPUT_NOTIFICATION_ITERATOR_STATE_FINALIZED_AND_ENDED";
	default:
		return "(unknown)";
	}
};

#endif /* BABELTRACE_GRAPH_NOTIFICATION_ITERATOR_INTERNAL_H */
