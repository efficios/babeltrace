#ifndef BABELTRACE_GRAPH_GRAPH_INTERNAL_H
#define BABELTRACE_GRAPH_GRAPH_INTERNAL_H

/*
 * Copyright 2017-2018 Philippe Proulx <pproulx@efficios.com>
 * Copyright 2017 Jérémie Galarneau <jeremie.galarneau@efficios.com>
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
#include <babeltrace/graph/connection-internal.h>
#include <babeltrace/graph/message-const.h>
#include <babeltrace/babeltrace-internal.h>
#include <babeltrace/object-internal.h>
#include <babeltrace/object-pool-internal.h>
#include <babeltrace/assert-internal.h>
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

	bool canceled;
	bool in_remove_listener;
	bool has_sink;

	/*
	 * If this is false, then the public API's consuming
	 * functions (bt_graph_consume() and bt_graph_run()) return
	 * BT_GRAPH_STATUS_CANNOT_CONSUME. The internal "no check"
	 * functions always work.
	 *
	 * In bt_port_output_message_iterator_create(), on success,
	 * this flag is cleared so that the iterator remains the only
	 * consumer for the graph's lifetime.
	 */
	bool can_consume;

	struct {
		GArray *source_output_port_added;
		GArray *filter_output_port_added;
		GArray *filter_input_port_added;
		GArray *sink_input_port_added;
		GArray *source_output_port_removed;
		GArray *filter_output_port_removed;
		GArray *filter_input_port_removed;
		GArray *sink_input_port_removed;
		GArray *source_filter_ports_connected;
		GArray *source_sink_ports_connected;
		GArray *filter_filter_ports_connected;
		GArray *filter_sink_ports_connected;
		GArray *source_filter_ports_disconnected;
		GArray *source_sink_ports_disconnected;
		GArray *filter_filter_ports_disconnected;
		GArray *filter_sink_ports_disconnected;
	} listeners;

	/* Pool of `struct bt_message_event *` */
	struct bt_object_pool event_msg_pool;

	/* Pool of `struct bt_message_packet_beginning *` */
	struct bt_object_pool packet_begin_msg_pool;

	/* Pool of `struct bt_message_packet_end *` */
	struct bt_object_pool packet_end_msg_pool;

	/*
	 * Array of `struct bt_message *` (weak).
	 *
	 * This is an array of all the messages ever created from
	 * this graph. Some of them can be in one of the pools above,
	 * some of them can be at large. Because each message has a
	 * weak pointer to the graph containing its pool, we need to
	 * notify each message that the graph is gone on graph
	 * destruction.
	 *
	 * TODO: When we support a maximum size for object pools,
	 * add a way for a message to remove itself from this
	 * array (on destruction).
	 */
	GPtrArray *messages;
};

static inline
void _bt_graph_set_can_consume(struct bt_graph *graph, bool can_consume)
{
	BT_ASSERT(graph);
	graph->can_consume = can_consume;
}

#ifdef BT_DEV_MODE
# define bt_graph_set_can_consume	_bt_graph_set_can_consume
#else
# define bt_graph_set_can_consume(_graph, _can_consume)
#endif

BT_HIDDEN
enum bt_graph_status bt_graph_consume_sink_no_check(struct bt_graph *graph,
		struct bt_component_sink *sink);

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

/*
 * This only works with a component which is not connected at this
 * point.
 *
 * Also the reference count of `component` should be 0 when you call
 * this function, which means only `graph` owns the component, so it
 * is safe to destroy.
 */
BT_HIDDEN
int bt_graph_remove_unconnected_component(struct bt_graph *graph,
		struct bt_component *component);

BT_HIDDEN
void bt_graph_add_message(struct bt_graph *graph,
		struct bt_message *msg);

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

#endif /* BABELTRACE_GRAPH_GRAPH_INTERNAL_H */
