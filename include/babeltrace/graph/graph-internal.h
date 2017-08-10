#ifndef BABELTRACE_COMPONENT_COMPONENT_GRAPH_INTERNAL_H
#define BABELTRACE_COMPONENT_COMPONENT_GRAPH_INTERNAL_H

/*
 * BabelTrace - Component Graph Internal
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

#include <babeltrace/graph/graph.h>
#include <babeltrace/graph/component-status.h>
#include <babeltrace/babeltrace-internal.h>
#include <babeltrace/object-internal.h>
#include <stdlib.h>
#include <glib.h>

struct bt_component;
struct bt_port;

struct bt_graph {
	/**
	 * A component graph contains components and point-to-point connection
	 * between these components.
	 *
	 * In terms of ownership:
	 * 1) The graph is the components' parent,
	 * 2) The graph is the connnections' parent,
	 * 3) Components share the ownership of their connections,
	 * 4) A connection holds weak references to its two component endpoints.
	 */
	struct bt_object base;

	/* Array of pointers to bt_connection. */
	GPtrArray *connections;
	/* Array of pointers to bt_component. */
	GPtrArray *components;
	/* Queue of pointers (weak references) to sink bt_components. */
	GQueue *sinks_to_consume;

	bt_bool canceled;
	bt_bool in_remove_listener;
	bt_bool has_sink;

	struct {
		GArray *port_added;
		GArray *port_removed;
		GArray *ports_connected;
		GArray *ports_disconnected;
	} listeners;
};

BT_HIDDEN
void bt_graph_notify_port_added(struct bt_graph *graph, struct bt_port *port);

BT_HIDDEN
void bt_graph_notify_port_removed(struct bt_graph *graph,
		struct bt_component *comp, struct bt_port *port);

BT_HIDDEN
void bt_graph_notify_ports_connected(struct bt_graph *graph,
		struct bt_port *upstream_port, struct bt_port *downstream_port);

BT_HIDDEN
void bt_graph_notify_ports_disconnected(struct bt_graph *graph,
		struct bt_component *upstream_comp,
		struct bt_component *downstream_comp,
		struct bt_port *upstream_port,
		struct bt_port *downstream_port);

BT_HIDDEN
void bt_graph_remove_connection(struct bt_graph *graph,
		struct bt_connection *connection);

static inline
const char *bt_graph_status_string(enum bt_graph_status status)
{
	switch (status) {
	case BT_GRAPH_STATUS_CANCELED:
		return "BT_GRAPH_STATUS_CANCELED";
	case BT_GRAPH_STATUS_AGAIN:
		return "BT_GRAPH_STATUS_AGAIN";
	case BT_GRAPH_STATUS_END:
		return "BT_GRAPH_STATUS_END";
	case BT_GRAPH_STATUS_OK:
		return "BT_GRAPH_STATUS_OK";
	case BT_GRAPH_STATUS_INVALID:
		return "BT_GRAPH_STATUS_INVALID";
	case BT_GRAPH_STATUS_NO_SINK:
		return "BT_GRAPH_STATUS_NO_SINK";
	case BT_GRAPH_STATUS_ERROR:
		return "BT_GRAPH_STATUS_ERROR";
	case BT_GRAPH_STATUS_COMPONENT_REFUSES_PORT_CONNECTION:
		return "BT_GRAPH_STATUS_COMPONENT_REFUSES_PORT_CONNECTION";
	case BT_GRAPH_STATUS_NOMEM:
		return "BT_GRAPH_STATUS_NOMEM";
	default:
		return "(unknown)";
	}
}

static inline
enum bt_graph_status bt_graph_status_from_component_status(
		enum bt_component_status comp_status)
{
	switch (comp_status) {
	case BT_COMPONENT_STATUS_OK:
		return BT_GRAPH_STATUS_OK;
	case BT_COMPONENT_STATUS_END:
		return BT_GRAPH_STATUS_END;
	case BT_COMPONENT_STATUS_AGAIN:
		return BT_GRAPH_STATUS_AGAIN;
	case BT_COMPONENT_STATUS_REFUSE_PORT_CONNECTION:
		return BT_GRAPH_STATUS_COMPONENT_REFUSES_PORT_CONNECTION;
	case BT_COMPONENT_STATUS_ERROR:
		return BT_GRAPH_STATUS_ERROR;
	case BT_COMPONENT_STATUS_UNSUPPORTED:
		return BT_GRAPH_STATUS_ERROR;
	case BT_COMPONENT_STATUS_INVALID:
		return BT_GRAPH_STATUS_INVALID;
	case BT_COMPONENT_STATUS_NOMEM:
		return BT_GRAPH_STATUS_NOMEM;
	case BT_COMPONENT_STATUS_NOT_FOUND:
		return BT_GRAPH_STATUS_ERROR;
	default:
#ifdef BT_LOGF
		BT_LOGF("Unknown component status: status=%d", comp_status);
#endif
		abort();
	}
}

#endif /* BABELTRACE_COMPONENT_COMPONENT_GRAPH_INTERNAL_H */
