#ifndef BABELTRACE_COMPONENT_COMPONENT_GRAPH_H
#define BABELTRACE_COMPONENT_COMPONENT_GRAPH_H

/*
 * BabelTrace - Babeltrace Component Graph Interface
 *
 * Copyright 2016 Jérémie Galarneau <jeremie.galarneau@efficios.com>
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

#include <babeltrace/component/component-class.h>
#include <babeltrace/component/component.h>
#include <babeltrace/values.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

enum bt_component_graph_status {
	BT_COMPONENT_GRAPH_STATUS_OK = 0,
};

/*
Graph Ownership:

                  Graph
                    ^
                    |
               Connection
                 ^     ^
                /       \
         ComponentA    ComponentB

1) A graph only owns a set of connections.
2) Components should _never_ own each other.
3) A component can keep the complete graph "alive".

*/
extern struct bt_component_graph *bt_component_graph_create(void);

/**
 * Creates a connection object which owns both components, invokes the
 * components' connection callback, and add the connection to the component
 * graph's set of connection.
 *
 * Will add any component that is not already part of the graph.
 */
extern enum bt_component_graph_status bt_component_graph_connect(
		struct bt_component_graph *graph, struct bt_component *upstream,
		struct bt_component *downstream);

/**
 * Add component to the graph
 */
extern enum bt_component_graph_status bt_component_graph_add_component(
		struct bt_component_graph *graph,
		struct bt_component *component);

/**
 * Add a component as a "sibling" of the origin component. Sibling share
 * connections equivalent to each other at the time of connection (same
 * parents and children).
 */
extern enum bt_component_graph_status bt_component_graph_add_component_as_sibling(
		struct bt_component_graph *graph, struct bt_component *origin,
		struct bt_component *new_component);

/**
 * Runs "bt_component_sink_consume()" on all sinks in round-robin until they all
 * indicate that the end is reached on that an error occured.
 */
extern enum bt_component_graph_status bt_component_graph_run(
		struct bt_component_graph *graph);

#ifdef __cplusplus
}
#endif

#endif /* BABELTRACE_COMPONENT_COMPONENT_GRAPH_H */
