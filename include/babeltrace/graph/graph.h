#ifndef BABELTRACE_GRAPH_GRAPH_H
#define BABELTRACE_GRAPH_GRAPH_H

/*
 * BabelTrace - Babeltrace Graph Interface
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

/* For bt_bool */
#include <babeltrace/types.h>

/* For enum bt_component_status */
#include <babeltrace/graph/component-status.h>

#ifdef __cplusplus
extern "C" {
#endif

struct bt_port;
struct bt_connection;
struct bt_component;
struct bt_component_class;
struct bt_value;

enum bt_graph_status {
	BT_GRAPH_STATUS_COMPONENT_REFUSES_PORT_CONNECTION = 111,
	/** Canceled. */
	BT_GRAPH_STATUS_CANCELED = BT_COMPONENT_STATUS_GRAPH_IS_CANCELED,
	/** No sink can consume at the moment. */
	BT_GRAPH_STATUS_AGAIN = BT_COMPONENT_STATUS_AGAIN,
	/** Downstream component does not support multiple inputs. */
	BT_GRAPH_STATUS_END = BT_COMPONENT_STATUS_END,
	BT_GRAPH_STATUS_OK = BT_COMPONENT_STATUS_OK,
	/** Invalid arguments. */
	BT_GRAPH_STATUS_INVALID = BT_COMPONENT_STATUS_INVALID,
	/** No sink in graph. */
	BT_GRAPH_STATUS_NO_SINK = -6,
	/** General error. */
	BT_GRAPH_STATUS_ERROR = BT_COMPONENT_STATUS_ERROR,
	BT_GRAPH_STATUS_NOMEM = BT_COMPONENT_STATUS_NOMEM,
};

typedef void (*bt_graph_port_added_listener)(struct bt_port *port,
		void *data);
typedef void (*bt_graph_port_removed_listener)(struct bt_component *component,
		struct bt_port *port, void *data);
typedef void (*bt_graph_ports_connected_listener)(struct bt_port *upstream_port,
		struct bt_port *downstream_port, void *data);
typedef void (*bt_graph_ports_disconnected_listener)(
		struct bt_component *upstream_component,
		struct bt_component *downstream_component,
		struct bt_port *upstream_port, struct bt_port *downstream_port,
		void *data);
typedef void (* bt_graph_listener_removed)(void *data);

extern struct bt_graph *bt_graph_create(void);

extern enum bt_graph_status bt_graph_add_component(
		struct bt_graph *graph,
		struct bt_component_class *component_class,
		const char *name, struct bt_value *params,
		struct bt_component **component);

extern enum bt_graph_status bt_graph_add_component_with_init_method_data(
		struct bt_graph *graph,
		struct bt_component_class *component_class,
		const char *name, struct bt_value *params,
		void *init_method_data,
		struct bt_component **component);

/**
 * Creates a connection between two components using the two ports specified
 * and adds the connection and components (if not already added) to the graph.
 */
extern enum bt_graph_status bt_graph_connect_ports(struct bt_graph *graph,
		struct bt_port *upstream, struct bt_port *downstream,
		struct bt_connection **connection);

/**
 * Run graph to completion or until a single sink is left and "AGAIN" is received.
 *
 * Runs "bt_component_sink_consume()" on all sinks in round-robin until they all
 * indicate that the end is reached or that an error occured.
 */
extern enum bt_graph_status bt_graph_run(struct bt_graph *graph);

/**
 * Runs "bt_component_sink_consume()" on the graph's sinks. Each invokation will
 * invoke "bt_component_sink_consume()" on the next sink, in round-robin, until
 * they all indicated that the end is reached.
 */
extern enum bt_graph_status bt_graph_consume(struct bt_graph *graph);

extern int bt_graph_add_port_added_listener(struct bt_graph *graph,
		bt_graph_port_added_listener listener,
		bt_graph_listener_removed listener_removed, void *data);

extern int bt_graph_add_port_removed_listener(struct bt_graph *graph,
		bt_graph_port_removed_listener listener,
		bt_graph_listener_removed listener_removed, void *data);

extern int bt_graph_add_ports_connected_listener(struct bt_graph *graph,
		bt_graph_ports_connected_listener listener,
		bt_graph_listener_removed listener_removed, void *data);

extern int bt_graph_add_ports_disconnected_listener(struct bt_graph *graph,
		bt_graph_ports_disconnected_listener listener,
		bt_graph_listener_removed listener_removed, void *data);

extern enum bt_graph_status bt_graph_cancel(struct bt_graph *graph);
extern bt_bool bt_graph_is_canceled(struct bt_graph *graph);

#ifdef __cplusplus
}
#endif

#endif /* BABELTRACE_GRAPH_GRAPH_H */
