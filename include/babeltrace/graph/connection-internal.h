#ifndef BABELTRACE_GRAPH_CONNECTION_INTERNAL_H
#define BABELTRACE_GRAPH_CONNECTION_INTERNAL_H

/*
 * BabelTrace - Component Connection Internal
 *
 * Copyright 2017 Jérémie Galarneau <jeremie.galarneau@efficios.com>
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

#include <babeltrace/graph/connection.h>
#include <babeltrace/graph/notification-iterator.h>
#include <babeltrace/graph/notification-iterator-internal.h>
#include <babeltrace/graph/private-connection.h>
#include <babeltrace/object-internal.h>
#include <babeltrace/assert-internal.h>
#include <stdbool.h>

struct bt_graph;

struct bt_connection {
	/*
	 * The graph is a connection's parent and the connection is the parent
	 * of all iterators it has created.
	 */
	struct bt_object base;
	/*
	 * Weak references are held to both ports. Their existence is guaranteed
	 * by the existence of the graph and thus, of their respective
	 * components.
	 */
	/* Downstream port. */
	struct bt_port *downstream_port;
	/* Upstream port. */
	struct bt_port *upstream_port;

	/*
	 * Weak references to all the notification iterators that were
	 * created on this connection.
	 */
	GPtrArray *iterators;

	bool notified_upstream_port_connected;
	bool notified_downstream_port_connected;
	bool notified_graph_ports_connected;
};

static inline
struct bt_private_connection *bt_private_connection_from_connection(
		struct bt_connection *connection)
{
	return (void *) connection;
}

BT_HIDDEN
struct bt_connection *bt_connection_create(struct bt_graph *graph,
		struct bt_port *upstream_port,
		struct bt_port *downstream_port);

BT_HIDDEN
void bt_connection_end(struct bt_connection *conn, bool try_remove_from_graph);

BT_HIDDEN
void bt_connection_remove_iterator(struct bt_connection *conn,
		struct bt_notification_iterator_private_connection *iterator);

static inline
struct bt_graph *bt_connection_borrow_graph(struct bt_connection *conn)
{
	BT_ASSERT(conn);
	return (void *) conn->base.parent;
}

static inline
enum bt_connection_status
bt_connection_status_from_notification_iterator_status(
		enum bt_notification_iterator_status iter_status)
{
	switch (iter_status) {
	case BT_NOTIFICATION_ITERATOR_STATUS_CANCELED:
		return BT_CONNECTION_STATUS_ERROR;
	case BT_NOTIFICATION_ITERATOR_STATUS_AGAIN:
		return BT_CONNECTION_STATUS_ERROR;
	case BT_NOTIFICATION_ITERATOR_STATUS_END:
		return BT_CONNECTION_STATUS_ERROR;
	case BT_NOTIFICATION_ITERATOR_STATUS_OK:
		return BT_CONNECTION_STATUS_OK;
	case BT_NOTIFICATION_ITERATOR_STATUS_INVALID:
		return BT_CONNECTION_STATUS_INVALID;
	case BT_NOTIFICATION_ITERATOR_STATUS_ERROR:
		return BT_CONNECTION_STATUS_ERROR;
	case BT_NOTIFICATION_ITERATOR_STATUS_NOMEM:
		return BT_CONNECTION_STATUS_NOMEM;
	case BT_NOTIFICATION_ITERATOR_STATUS_UNSUPPORTED:
		return BT_CONNECTION_STATUS_ERROR;
	default:
#ifdef BT_LOGF
		BT_LOGF("Unknown notification iterator status: status=%d",
			iter_status);
#endif
		abort();
	}
}

static inline
const char *bt_connection_status_string(enum bt_connection_status status)
{
	switch (status) {
	case BT_CONNECTION_STATUS_GRAPH_IS_CANCELED:
		return "BT_CONNECTION_STATUS_GRAPH_IS_CANCELED";
	case BT_CONNECTION_STATUS_OK:
		return "BT_CONNECTION_STATUS_OK";
	case BT_CONNECTION_STATUS_INVALID:
		return "BT_CONNECTION_STATUS_INVALID";
	case BT_CONNECTION_STATUS_ERROR:
		return "BT_CONNECTION_STATUS_ERROR";
	case BT_CONNECTION_STATUS_NOMEM:
		return "BT_CONNECTION_STATUS_NOMEM";
	case BT_CONNECTION_STATUS_IS_ENDED:
		return "BT_CONNECTION_STATUS_IS_ENDED";
	default:
		return "(unknown)";
	}
}

#endif /* BABELTRACE_GRAPH_CONNECTION_INTERNAL_H */
