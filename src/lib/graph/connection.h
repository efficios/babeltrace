/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright 2017-2018 Philippe Proulx <pproulx@efficios.com>
 * Copyright 2017 Jérémie Galarneau <jeremie.galarneau@efficios.com>
 */

#ifndef BABELTRACE_GRAPH_CONNECTION_INTERNAL_H
#define BABELTRACE_GRAPH_CONNECTION_INTERNAL_H

#include <babeltrace2/graph/connection.h>
#include "lib/object.h"
#include "common/assert.h"
#include "common/macros.h"
#include <stdbool.h>

#include "message/iterator.h"

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
	 * Weak references to all the message iterators that were
	 * created on this connection.
	 */
	GPtrArray *iterators;

	bool notified_upstream_port_connected;
	bool notified_downstream_port_connected;
	bool notified_graph_ports_connected;
};

struct bt_connection *bt_connection_create(struct bt_graph *graph,
		struct bt_port *upstream_port,
		struct bt_port *downstream_port);

void bt_connection_end(struct bt_connection *conn, bool try_remove_from_graph);

void bt_connection_remove_iterator(struct bt_connection *conn,
		struct bt_message_iterator *iterator);

static inline
struct bt_graph *bt_connection_borrow_graph(struct bt_connection *conn)
{
	BT_ASSERT_DBG(conn);
	return (void *) conn->base.parent;
}

#endif /* BABELTRACE_GRAPH_CONNECTION_INTERNAL_H */
