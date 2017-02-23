/*
 * graph.c
 *
 * Babeltrace Plugin Component Graph
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

#include <babeltrace/component/component-internal.h>
#include <babeltrace/component/graph-internal.h>
#include <babeltrace/component/connection-internal.h>
#include <babeltrace/component/component-sink-internal.h>
#include <babeltrace/component/component-source.h>
#include <babeltrace/component/component-filter.h>
#include <babeltrace/component/port.h>
#include <babeltrace/compiler.h>
#include <unistd.h>

static
void bt_graph_destroy(struct bt_object *obj)
{
	struct bt_graph *graph = container_of(obj,
			struct bt_graph, base);

	if (graph->components) {
		g_ptr_array_free(graph->components, TRUE);
	}
	if (graph->connections) {
		g_ptr_array_free(graph->connections, TRUE);
	}
	if (graph->sinks_to_consume) {
		g_queue_free(graph->sinks_to_consume);
	}
	g_free(graph);
}

struct bt_graph *bt_graph_create(void)
{
	struct bt_graph *graph;

	graph = g_new0(struct bt_graph, 1);
	if (!graph) {
		goto end;
	}

	bt_object_init(graph, bt_graph_destroy);

	graph->connections = g_ptr_array_new_with_free_func(bt_object_release);
	if (!graph->connections) {
		goto error;
	}
	graph->components = g_ptr_array_new_with_free_func(bt_object_release);
	if (!graph->components) {
		goto error;
	}
	graph->sinks_to_consume = g_queue_new();
	if (!graph->sinks_to_consume) {
		goto error;
	}
end:
	return graph;
error:
	BT_PUT(graph);
	goto end;
}

struct bt_connection *bt_graph_connect(struct bt_graph *graph,
		struct bt_port *upstream_port,
		struct bt_port *downstream_port)
{
	struct bt_connection *connection = NULL;
	struct bt_graph *upstream_graph = NULL;
	struct bt_graph *downstream_graph = NULL;
	struct bt_component *upstream_component = NULL;
	struct bt_component *downstream_component = NULL;
	enum bt_component_status component_status;
	bool components_added = false;

	if (!graph || !upstream_port || !downstream_port) {
		goto end;
	}

	if (bt_port_get_type(upstream_port) != BT_PORT_TYPE_OUTPUT) {
		goto end;
	}
	if (bt_port_get_type(downstream_port) != BT_PORT_TYPE_INPUT) {
		goto end;
	}

	/* Ensure the components are not already part of another graph. */
	upstream_component = bt_port_get_component(upstream_port);
	assert(upstream_component);
	upstream_graph = bt_component_get_graph(upstream_component);
	if (upstream_graph && (graph != upstream_graph)) {
		fprintf(stderr, "Upstream component is already part of another graph\n");
		goto error;
	}

	downstream_component = bt_port_get_component(downstream_port);
	assert(downstream_component);
	downstream_graph = bt_component_get_graph(downstream_component);
	if (downstream_graph && (graph != downstream_graph)) {
		fprintf(stderr, "Downstream component is already part of another graph\n");
		goto error;
	}

	connection = bt_connection_create(graph, upstream_port,
			downstream_port);
	if (!connection) {
		goto error;
	}

	/*
	 * Ownership of up/downstream_component and of the connection object is
	 * transferred to the graph.
	 */
	g_ptr_array_add(graph->connections, connection);
	g_ptr_array_add(graph->components, upstream_component);
	g_ptr_array_add(graph->components, downstream_component);
	if (bt_component_get_class_type(downstream_component) ==
			BT_COMPONENT_CLASS_TYPE_SINK) {
		g_queue_push_tail(graph->sinks_to_consume,
				downstream_component);
	}

	/*
	 * The graph is now the parent of these components which garantees their
	 * existence for the duration of the graph's lifetime.
	 */
	bt_component_set_graph(upstream_component, graph);
	bt_put(upstream_component);
	bt_component_set_graph(downstream_component, graph);
	bt_put(downstream_component);

	/* Rollback the connection from this point on. */
	components_added = true;

	/*
	 * The components and connection are added to the graph before invoking
	 * the new_connection method in order to make them visible to the
	 * components during the method's invocation.
	 */
	component_status = bt_component_new_connection(upstream_component,
			upstream_port, connection);
	if (component_status != BT_COMPONENT_STATUS_OK) {
		goto error;
	}
	component_status = bt_component_new_connection(downstream_component,
			downstream_port, connection);
	if (component_status != BT_COMPONENT_STATUS_OK) {
		goto error;
	}
end:
	bt_put(upstream_graph);
	bt_put(downstream_graph);
	return connection;
error:
	if (components_added) {
		if (bt_component_get_class_type(downstream_component) ==
				BT_COMPONENT_CLASS_TYPE_SINK) {
			g_queue_pop_tail(graph->sinks_to_consume);
		}
		g_ptr_array_set_size(graph->connections,
				graph->connections->len - 1);
		g_ptr_array_set_size(graph->components,
				graph->components->len - 2);
	}
	goto end;
}

static
int get_component_port_counts(struct bt_component *component, int *input_count,
		int *output_count)
{
	int ret = -1;

	switch (bt_component_get_class_type(component)) {
	case BT_COMPONENT_CLASS_TYPE_SOURCE:
		ret = bt_component_source_get_output_port_count(component);
		if (ret < 0) {
			goto end;
		}
		*output_count = ret;
		break;
	case BT_COMPONENT_CLASS_TYPE_FILTER:
		ret = bt_component_filter_get_output_port_count(component);
		if (ret < 0) {
			goto end;
		}
		*output_count = ret;
		break;
		ret = bt_component_filter_get_input_port_count(component);
		if (ret < 0) {
			goto end;
		}
		*input_count = ret;
		break;
	case BT_COMPONENT_CLASS_TYPE_SINK:
		ret = bt_component_sink_get_input_port_count(component);
		if (ret < 0) {
			goto end;
		}
		*input_count = ret;
		break;
	default:
		assert(false);
		break;
	}
	ret = 0;
end:
	return ret;
}

static
struct bt_port *get_input_port(struct bt_component *component, int index)
{
	struct bt_port *port = NULL;

        switch (bt_component_get_class_type(component)) {
	case BT_COMPONENT_CLASS_TYPE_FILTER:
		port = bt_component_filter_get_input_port_at_index(component,
				index);
		break;
	case BT_COMPONENT_CLASS_TYPE_SINK:
		port = bt_component_sink_get_input_port_at_index(component,
				index);
		break;
	default:
	        assert(false);
	}
	return port;
}

static
struct bt_port *get_output_port(struct bt_component *component, int index)
{
	struct bt_port *port = NULL;

        switch (bt_component_get_class_type(component)) {
	case BT_COMPONENT_CLASS_TYPE_SOURCE:
		port = bt_component_source_get_output_port_at_index(component,
				index);
		break;
	case BT_COMPONENT_CLASS_TYPE_FILTER:
		port = bt_component_filter_get_output_port_at_index(component,
				index);
		break;
	default:
	        assert(false);
	}
	return port;
}

enum bt_graph_status bt_graph_add_component_as_sibling(struct bt_graph *graph,
		struct bt_component *origin,
		struct bt_component *new_component)
{
	int origin_input_port_count = 0;
	int origin_output_port_count = 0;
	int new_input_port_count = 0;
	int new_output_port_count = 0;
	enum bt_graph_status status = BT_GRAPH_STATUS_OK;
	struct bt_graph *origin_graph = NULL;
	struct bt_graph *new_graph = NULL;
	struct bt_port *origin_port = NULL;
	struct bt_port *new_port = NULL;
	struct bt_port *upstream_port = NULL;
	struct bt_port *downstream_port = NULL;
	struct bt_connection *origin_connection = NULL;
	struct bt_connection *new_connection = NULL;
        int port_index;

	if (!graph || !origin || !new_component) {
		status = BT_GRAPH_STATUS_INVALID;
		goto end;
	}

	if (bt_component_get_class_type(origin) !=
			bt_component_get_class_type(new_component)) {
		status = BT_GRAPH_STATUS_INVALID;
		goto end;
	}

        origin_graph = bt_component_get_graph(origin);
	if (!origin_graph || (origin_graph != graph)) {
		status = BT_GRAPH_STATUS_INVALID;
		goto end;
	}

        new_graph = bt_component_get_graph(new_component);
	if (new_graph) {
		status = BT_GRAPH_STATUS_ALREADY_IN_A_GRAPH;
		goto end;
	}

	if (get_component_port_counts(origin, &origin_input_port_count,
			&origin_output_port_count)) {
		status = BT_GRAPH_STATUS_INVALID;
		goto end;
	}
	if (get_component_port_counts(new_component, &new_input_port_count,
			&new_output_port_count)) {
		status = BT_GRAPH_STATUS_INVALID;
		goto end;
	}

	if (origin_input_port_count != new_input_port_count ||
			origin_output_port_count != new_output_port_count) {
		status = BT_GRAPH_STATUS_INVALID;
		goto end;
	}

	/* Replicate input connections. */
	for (port_index = 0; port_index< origin_input_port_count; port_index++) {
		uint64_t connection_count, connection_index;

		origin_port = get_input_port(origin, port_index);
		if (!origin_port) {
			status = BT_GRAPH_STATUS_ERROR;
			goto error_disconnect;
		}
		new_port = get_input_port(new_component, port_index);
		if (!new_port) {
			status = BT_GRAPH_STATUS_ERROR;
			goto error_disconnect;
		}

	        if (bt_port_get_connection_count(origin_port, &connection_count) !=
				BT_PORT_STATUS_OK) {
			status = BT_GRAPH_STATUS_ERROR;
			goto error_disconnect;
		}

		for (connection_index = 0; connection_index < connection_count;
				connection_index++) {
		        origin_connection = bt_port_get_connection(origin_port,
					connection_index);
			if (!origin_connection) {
				goto error_disconnect;
			}

			upstream_port = bt_connection_get_output_port(
					origin_connection);
			if (!upstream_port) {
				goto error_disconnect;
			}

			new_connection = bt_graph_connect(graph, upstream_port,
					new_port);
			if (!new_connection) {
				goto error_disconnect;
			}

			BT_PUT(upstream_port);
			BT_PUT(origin_connection);
			BT_PUT(new_connection);
		}
		BT_PUT(origin_port);
		BT_PUT(new_port);
	}

	/* Replicate output connections. */
	for (port_index = 0; port_index< origin_output_port_count; port_index++) {
		uint64_t connection_count, connection_index;

		origin_port = get_output_port(origin, port_index);
		if (!origin_port) {
			status = BT_GRAPH_STATUS_ERROR;
			goto error_disconnect;
		}
		new_port = get_output_port(new_component, port_index);
		if (!new_port) {
			status = BT_GRAPH_STATUS_ERROR;
			goto error_disconnect;
		}

	        if (bt_port_get_connection_count(origin_port, &connection_count) !=
				BT_PORT_STATUS_OK) {
			status = BT_GRAPH_STATUS_ERROR;
			goto error_disconnect;
		}

		for (connection_index = 0; connection_index < connection_count;
				connection_index++) {
		        origin_connection = bt_port_get_connection(origin_port,
					connection_index);
			if (!origin_connection) {
				goto error_disconnect;
			}

			downstream_port = bt_connection_get_input_port(
					origin_connection);
			if (!downstream_port) {
				goto error_disconnect;
			}

			new_connection = bt_graph_connect(graph, new_port,
					downstream_port);
			if (!new_connection) {
				goto error_disconnect;
			}

			BT_PUT(downstream_port);
			BT_PUT(origin_connection);
			BT_PUT(new_connection);
		}
		BT_PUT(origin_port);
		BT_PUT(new_port);
	}
end:
	bt_put(origin_graph);
	bt_put(new_graph);
	bt_put(origin_port);
	bt_put(new_port);
	bt_put(upstream_port);
	bt_put(downstream_port);
	bt_put(origin_connection);
	bt_put(new_connection);
	return status;
error_disconnect:
	/* Destroy all connections of the new component. */
	/* FIXME. */
	goto end;
}

enum bt_component_status bt_graph_consume(struct bt_graph *graph)
{
	struct bt_component *sink;
	enum bt_component_status status;
	GList *current_node;

	if (!graph) {
		status = BT_COMPONENT_STATUS_INVALID;
		goto end;
	}

	if (g_queue_is_empty(graph->sinks_to_consume)) {
		status = BT_COMPONENT_STATUS_END;
		goto end;
	}

	current_node = g_queue_pop_head_link(graph->sinks_to_consume);
	sink = current_node->data;
	status = bt_component_sink_consume(sink);
	if (status != BT_COMPONENT_STATUS_END) {
		g_queue_push_tail_link(graph->sinks_to_consume, current_node);
		goto end;
	}

	/* End reached, the node is not added back to the queue and free'd. */
	g_queue_delete_link(graph->sinks_to_consume, current_node);

	/* Don't forward an END status if there are sinks left to consume. */
	if (!g_queue_is_empty(graph->sinks_to_consume)) {
		status = BT_GRAPH_STATUS_OK;
		goto end;
	}
end:
	return status;
}

enum bt_graph_status bt_graph_run(struct bt_graph *graph,
		enum bt_component_status *_component_status)
{
	enum bt_component_status component_status;
	enum bt_graph_status graph_status = BT_GRAPH_STATUS_OK;

	if (!graph) {
		graph_status = BT_GRAPH_STATUS_INVALID;
		goto error;
	}

	do {
		component_status = bt_graph_consume(graph);
		if (component_status == BT_COMPONENT_STATUS_AGAIN) {
			/*
			 * If AGAIN is received and there are multiple sinks,
			 * go ahead and consume from the next sink.
			 *
			 * However, in the case where a single sink is left,
			 * the caller can decide to busy-wait and call
			 * bt_graph_run continuously until the source is ready
			 * or it can decide to sleep for an arbitrary amount of
			 * time.
			 */
			if (graph->sinks_to_consume->length > 1) {
				component_status = BT_COMPONENT_STATUS_OK;
			}
		}
	} while (component_status == BT_COMPONENT_STATUS_OK);

	if (_component_status) {
		*_component_status = component_status;
	}

	if (g_queue_is_empty(graph->sinks_to_consume)) {
		graph_status = BT_GRAPH_STATUS_END;
	} else if (component_status == BT_COMPONENT_STATUS_AGAIN) {
		graph_status = BT_GRAPH_STATUS_AGAIN;
	} else {
		graph_status = BT_GRAPH_STATUS_ERROR;
	}
error:
	return graph_status;
}
