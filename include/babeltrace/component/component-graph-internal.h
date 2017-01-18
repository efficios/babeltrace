#ifndef BABELTRACE_COMPONENT_COMPONENT_GRAPH_INTERNAL_H
#define BABELTRACE_COMPONENT_COMPONENT_GRAPH_INTERNAL_H

/*
 * BabelTrace - Component Graph Internal
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

#include <babeltrace/component/component-graph.h>
#include <babeltrace/babeltrace-internal.h>
#include <babeltrace/object-internal.h>
#include <glib.h>

struct bt_component_graph {
	struct bt_object base;
	/* Array of pointers to bt_component_connection. */
	GPtrArray *connections;
	/*
	 * Array of pointers to bt_component.
	 *
	 * Components which were added to the graph, but have not been connected
	 * yet.
	 */
	GPtrArray *loose_components;
	/*
	 * Array of pointers to sink bt_component.
	 *
	 * A reference is held to the Sink components in order to implement the
	 * "run" interface, which executes the sinks in a round-robin pattern.
	 */
	GPtrArray *sinks;
};

#endif /* BABELTRACE_COMPONENT_COMPONENT_GRAPH_INTERNAL_H */
