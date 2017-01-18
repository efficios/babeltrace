/*
 * component-graph.c
 *
 * Babeltrace Plugin Component Graph
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

#include <babeltrace/component/component-graph-internal.h>
#include <babeltrace/compiler.h>

static void bt_component_graph_destroy(struct bt_object *obj)
{
	struct bt_component_graph *graph = container_of(obj,
			struct bt_component_graph, base);

	if (graph->connections) {
		g_ptr_array_free(graph->connections, TRUE);
	}
	if (graph->sinks) {
		g_ptr_array_free(graph->sinks, TRUE);
	}
	g_free(graph);
}

struct bt_component_graph *bt_component_graph_create(void)
{
	struct bt_component_graph *graph;

	graph = g_new0(struct bt_component_graph, 1);
	if (!graph) {
		goto end;
	}

	bt_object_init(graph, bt_component_graph_destroy);

	graph->connections = g_ptr_array_new_with_free_func(bt_put);
	if (!graph->connections) {
		goto error;
	}
	graph->sinks = g_ptr_array_new_with_free_func(bt_put);
	if (!graph->sinks) {
		goto error;
	}
end:
	return graph;
error:
	BT_PUT(graph);
	goto end;
}

enum bt_component_graph_status bt_component_graph_connect(
		struct bt_component_graph *graph, struct bt_component *upstream,
		struct bt_component *downstream)
{
	return BT_COMPONENT_GRAPH_STATUS_OK;
}

enum bt_component_graph_status bt_component_graph_add_component(
		struct bt_component_graph *graph,
		struct bt_component *component)
{
	return BT_COMPONENT_GRAPH_STATUS_OK;
}

enum bt_component_graph_status bt_component_graph_add_component_as_sibling(
		struct bt_component_graph *graph, struct bt_component *origin,
		struct bt_component *new_component)
{
	return BT_COMPONENT_GRAPH_STATUS_OK;
}

enum bt_component_graph_status bt_component_graph_run(
		struct bt_component_graph *graph)
{
	return BT_COMPONENT_GRAPH_STATUS_OK;
}

