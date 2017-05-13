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

#include <babeltrace/graph/component-internal.h>
#include <babeltrace/graph/graph-internal.h>
#include <babeltrace/graph/connection-internal.h>
#include <babeltrace/graph/component-sink-internal.h>
#include <babeltrace/graph/component-source.h>
#include <babeltrace/graph/component-filter.h>
#include <babeltrace/graph/port.h>
#include <babeltrace/compiler-internal.h>
#include <babeltrace/types.h>
#include <unistd.h>
#include <glib.h>

struct bt_graph_listener {
	void *func;
	void *data;
};

static
void bt_graph_destroy(struct bt_object *obj)
{
	struct bt_graph *graph = container_of(obj,
			struct bt_graph, base);

	/*
	 * The graph's reference count is 0 if we're here. Increment
	 * it to avoid a double-destroy (possibly infinitely recursive)
	 * in this situation:
	 *
	 * 1. We put and destroy a connection.
	 * 2. This connection's destructor finalizes its active
	 *    notification iterators.
	 * 3. A notification iterator's finalization function gets a
	 *    new reference on its component (reference count goes from
	 *    0 to 1).
	 * 4. Since this component's reference count goes to 1, it takes
	 *    a reference on its parent (this graph). This graph's
	 *    reference count goes from 0 to 1.
	 * 5. The notification iterator's finalization function puts its
	 *    component reference (reference count goes from 1 to 0).
	 * 6. Since this component's reference count goes from 1 to 0,
	 *    it puts its parent (this graph). This graph's reference
	 *    count goes from 1 to 0.
	 * 7. Since this graph's reference count goes from 1 to 0,
	 *    its destructor is called (this function).
	 *
	 * With the incrementation below, the graph's reference count at
	 * step 4 goes from 1 to 2, and from 2 to 1 at step 6. This
	 * ensures that this function is not called two times.
	 */
	obj->ref_count.count++;

	if (graph->connections) {
		g_ptr_array_free(graph->connections, TRUE);
	}
	if (graph->components) {
		g_ptr_array_free(graph->components, TRUE);
	}
	if (graph->sinks_to_consume) {
		g_queue_free(graph->sinks_to_consume);
	}

	if (graph->listeners.port_added) {
		g_array_free(graph->listeners.port_added, TRUE);
	}

	if (graph->listeners.port_removed) {
		g_array_free(graph->listeners.port_removed, TRUE);
	}

	if (graph->listeners.ports_connected) {
		g_array_free(graph->listeners.ports_connected, TRUE);
	}

	if (graph->listeners.ports_disconnected) {
		g_array_free(graph->listeners.ports_disconnected, TRUE);
	}

	g_free(graph);
}

static
int init_listeners_array(GArray **listeners)
{
	int ret = 0;

	assert(listeners);
	*listeners = g_array_new(FALSE, TRUE, sizeof(struct bt_graph_listener));
	if (!*listeners) {
		ret = -1;
		goto end;
	}

end:
	return ret;
}

struct bt_graph *bt_graph_create(void)
{
	struct bt_graph *graph;
	int ret;

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

	ret = init_listeners_array(&graph->listeners.port_added);
	if (ret) {
		goto error;
	}

	ret = init_listeners_array(&graph->listeners.port_removed);
	if (ret) {
		goto error;
	}

	ret = init_listeners_array(&graph->listeners.ports_connected);
	if (ret) {
		goto error;
	}

	ret = init_listeners_array(&graph->listeners.ports_disconnected);
	if (ret) {
		goto error;
	}

end:
	return graph;
error:
	BT_PUT(graph);
	goto end;
}

struct bt_connection *bt_graph_connect_ports(struct bt_graph *graph,
		struct bt_port *upstream_port,
		struct bt_port *downstream_port)
{
	struct bt_connection *connection = NULL;
	struct bt_graph *upstream_graph = NULL;
	struct bt_graph *downstream_graph = NULL;
	struct bt_component *upstream_component = NULL;
	struct bt_component *downstream_component = NULL;
	enum bt_component_status component_status;
	bt_bool upstream_was_already_in_graph;
	bt_bool downstream_was_already_in_graph;

	if (!graph || !upstream_port || !downstream_port) {
		goto end;
	}

	if (graph->canceled) {
		goto end;
	}

	/* Ensure appropriate types for upstream and downstream ports. */
	if (bt_port_get_type(upstream_port) != BT_PORT_TYPE_OUTPUT) {
		goto end;
	}
	if (bt_port_get_type(downstream_port) != BT_PORT_TYPE_INPUT) {
		goto end;
	}

	/* Ensure that both ports are currently unconnected. */
	if (bt_port_is_connected(upstream_port)) {
		fprintf(stderr, "Upstream port is already connected\n");
		goto end;
	}

	if (bt_port_is_connected(downstream_port)) {
		fprintf(stderr, "Downstream port is already connected\n");
		goto end;
	}

	/*
	 * Ensure that both ports are still attached to their creating
	 * component.
	 */
	upstream_component = bt_port_get_component(upstream_port);
	if (!upstream_component) {
		fprintf(stderr, "Upstream port does not belong to a component\n");
		goto end;
	}

	downstream_component = bt_port_get_component(downstream_port);
	if (!downstream_component) {
		fprintf(stderr, "Downstream port does not belong to a component\n");
		goto end;
	}

	/* Ensure the components are not already part of another graph. */
	upstream_graph = bt_component_get_graph(upstream_component);
	if (upstream_graph && (graph != upstream_graph)) {
		fprintf(stderr, "Upstream component is already part of another graph\n");
		goto error;
	}
	upstream_was_already_in_graph = (graph == upstream_graph);
	downstream_graph = bt_component_get_graph(downstream_component);
	if (downstream_graph && (graph != downstream_graph)) {
		fprintf(stderr, "Downstream component is already part of another graph\n");
		goto error;
	}
	downstream_was_already_in_graph = (graph == downstream_graph);

	/*
	 * At this point the ports are not connected yet. Both
	 * components need to accept an eventual connection to their
	 * port by the other port before we continue.
	 */
	component_status = bt_component_accept_port_connection(
		upstream_component, upstream_port, downstream_port);
	if (component_status != BT_COMPONENT_STATUS_OK) {
		goto error;
	}
	component_status = bt_component_accept_port_connection(
		downstream_component, downstream_port, upstream_port);
	if (component_status != BT_COMPONENT_STATUS_OK) {
		goto error;
	}

	connection = bt_connection_create(graph, upstream_port,
			downstream_port);
	if (!connection) {
		goto error;
	}

	/*
	 * Ownership of upstream_component/downstream_component and of
	 * the connection object is transferred to the graph.
	 */
	g_ptr_array_add(graph->connections, connection);

	if (!upstream_was_already_in_graph) {
		g_ptr_array_add(graph->components, upstream_component);
		bt_component_set_graph(upstream_component, graph);
	}
	if (!downstream_was_already_in_graph) {
		g_ptr_array_add(graph->components, downstream_component);
		bt_component_set_graph(downstream_component, graph);
		if (bt_component_get_class_type(downstream_component) ==
				BT_COMPONENT_CLASS_TYPE_SINK) {
			g_queue_push_tail(graph->sinks_to_consume,
					downstream_component);
		}
	}

	/*
	 * The graph is now the parent of these components which
	 * garantees their existence for the duration of the graph's
	 * lifetime.
	 */

	/*
	 * Notify both components that their port is connected.
	 */
	bt_component_port_connected(upstream_component, upstream_port,
		downstream_port);
	bt_component_port_connected(downstream_component, downstream_port,
		upstream_port);

	/*
	 * Notify the graph's creator that both ports are connected.
	 */
	bt_graph_notify_ports_connected(graph, upstream_port, downstream_port);

end:
	bt_put(upstream_graph);
	bt_put(downstream_graph);
	bt_put(upstream_component);
	bt_put(downstream_component);
	return connection;

error:
	BT_PUT(upstream_component);
	BT_PUT(downstream_component);
	goto end;
}

static
enum bt_component_status get_component_port_counts(
		struct bt_component *component, int64_t *input_count,
		int64_t *output_count)
{
	enum bt_component_status ret;

	switch (bt_component_get_class_type(component)) {
	case BT_COMPONENT_CLASS_TYPE_SOURCE:
		*output_count =
			bt_component_source_get_output_port_count(component);
		if (*output_count < 0) {
			ret = BT_COMPONENT_STATUS_ERROR;
			goto end;
		}
		break;
	case BT_COMPONENT_CLASS_TYPE_FILTER:
		*output_count =
			bt_component_filter_get_output_port_count(component);
		if (*output_count < 0) {
			ret = BT_COMPONENT_STATUS_ERROR;
			goto end;
		}
		*input_count =
			bt_component_filter_get_input_port_count(component);
		if (*input_count < 0) {
			ret = BT_COMPONENT_STATUS_ERROR;
			goto end;
		}
		break;
	case BT_COMPONENT_CLASS_TYPE_SINK:
		*input_count =
			bt_component_sink_get_input_port_count(component);
		if (*input_count < 0) {
			ret = BT_COMPONENT_STATUS_ERROR;
			goto end;
		}
		break;
	default:
		assert(BT_FALSE);
		break;
	}
	ret = BT_COMPONENT_STATUS_OK;
end:
	return ret;
}

enum bt_graph_status bt_graph_add_component_as_sibling(struct bt_graph *graph,
		struct bt_component *origin,
		struct bt_component *new_component)
{
	int64_t origin_input_port_count = 0;
	int64_t origin_output_port_count = 0;
	int64_t new_input_port_count = 0;
	int64_t new_output_port_count = 0;
	enum bt_graph_status status = BT_GRAPH_STATUS_OK;
	struct bt_graph *origin_graph = NULL;
	struct bt_graph *new_graph = NULL;
	struct bt_port *origin_port = NULL;
	struct bt_port *new_port = NULL;
	struct bt_port *upstream_port = NULL;
	struct bt_port *downstream_port = NULL;
	struct bt_connection *origin_connection = NULL;
	struct bt_connection *new_connection = NULL;
        int64_t port_index;

	if (!graph || !origin || !new_component) {
		status = BT_GRAPH_STATUS_INVALID;
		goto end;
	}

	if (graph->canceled) {
		status = BT_GRAPH_STATUS_CANCELED;
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
			&origin_output_port_count) != BT_COMPONENT_STATUS_OK) {
		status = BT_GRAPH_STATUS_INVALID;
		goto end;
	}
	if (get_component_port_counts(new_component, &new_input_port_count,
			&new_output_port_count) != BT_COMPONENT_STATUS_OK) {
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
		origin_port = bt_component_get_input_port_by_index(origin,
			port_index);
		if (!origin_port) {
			status = BT_GRAPH_STATUS_ERROR;
			goto error_disconnect;
		}

		new_port = bt_component_get_input_port_by_index(new_component,
			port_index);
		if (!new_port) {
			status = BT_GRAPH_STATUS_ERROR;
			goto error_disconnect;
		}

	        origin_connection = bt_port_get_connection(origin_port);
		if (origin_connection) {
			upstream_port = bt_connection_get_upstream_port(
				origin_connection);
			if (!upstream_port) {
				goto error_disconnect;
			}

			new_connection = bt_graph_connect_ports(graph,
				upstream_port, new_port);
			if (!new_connection) {
				goto error_disconnect;
			}
		}

		BT_PUT(upstream_port);
		BT_PUT(origin_connection);
		BT_PUT(new_connection);
		BT_PUT(origin_port);
		BT_PUT(new_port);
	}

	/* Replicate output connections. */
	for (port_index = 0; port_index < origin_output_port_count; port_index++) {
		origin_port = bt_component_get_output_port_by_index(origin,
			port_index);
		if (!origin_port) {
			status = BT_GRAPH_STATUS_ERROR;
			goto error_disconnect;
		}
		new_port = bt_component_get_output_port_by_index(new_component,
			port_index);
		if (!new_port) {
			status = BT_GRAPH_STATUS_ERROR;
			goto error_disconnect;
		}

	        origin_connection = bt_port_get_connection(origin_port);
		if (origin_connection) {
			downstream_port = bt_connection_get_downstream_port(
					origin_connection);
			if (!downstream_port) {
				goto error_disconnect;
			}

			new_connection = bt_graph_connect_ports(graph,
				new_port, downstream_port);
			if (!new_connection) {
				goto error_disconnect;
			}
		}

		BT_PUT(downstream_port);
		BT_PUT(origin_connection);
		BT_PUT(new_connection);
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

enum bt_graph_status bt_graph_consume(struct bt_graph *graph)
{
	struct bt_component *sink;
	enum bt_graph_status status = BT_GRAPH_STATUS_OK;
	enum bt_component_status comp_status;
	GList *current_node;

	if (!graph) {
		status = BT_GRAPH_STATUS_INVALID;
		goto end;
	}

	if (graph->canceled) {
		status = BT_GRAPH_STATUS_CANCELED;
		goto end;
	}

	if (g_queue_is_empty(graph->sinks_to_consume)) {
		status = BT_GRAPH_STATUS_END;
		goto end;
	}

	current_node = g_queue_pop_head_link(graph->sinks_to_consume);
	sink = current_node->data;
	comp_status = bt_component_sink_consume(sink);
	switch (comp_status) {
	case BT_COMPONENT_STATUS_OK:
		break;
	case BT_COMPONENT_STATUS_END:
		status = BT_GRAPH_STATUS_END;
		break;
	case BT_COMPONENT_STATUS_AGAIN:
		status = BT_GRAPH_STATUS_AGAIN;
		break;
	case BT_COMPONENT_STATUS_INVALID:
		status = BT_GRAPH_STATUS_INVALID;
		break;
	default:
		status = BT_GRAPH_STATUS_ERROR;
		break;
	}

	if (status != BT_GRAPH_STATUS_END) {
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

enum bt_graph_status bt_graph_run(struct bt_graph *graph)
{
	enum bt_graph_status status = BT_GRAPH_STATUS_OK;

	if (!graph) {
		status = BT_GRAPH_STATUS_INVALID;
		goto end;
	}

	do {
		status = bt_graph_consume(graph);
		if (status == BT_GRAPH_STATUS_AGAIN) {
			/*
			 * If AGAIN is received and there are multiple
			 * sinks, go ahead and consume from the next
			 * sink.
			 *
			 * However, in the case where a single sink is
			 * left, the caller can decide to busy-wait and
			 * call bt_graph_run() continuously until the
			 * source is ready or it can decide to sleep for
			 * an arbitrary amount of time.
			 */
			if (graph->sinks_to_consume->length > 1) {
				status = BT_GRAPH_STATUS_OK;
			}
		}
	} while (status == BT_GRAPH_STATUS_OK);

	if (g_queue_is_empty(graph->sinks_to_consume)) {
		status = BT_GRAPH_STATUS_END;
	}
end:
	return status;
}

static
void add_listener(GArray *listeners, void *func, void *data)
{
	struct bt_graph_listener listener = {
		.func = func,
		.data = data,
	};

	g_array_append_val(listeners, listener);
}

enum bt_graph_status bt_graph_add_port_added_listener(
		struct bt_graph *graph,
		bt_graph_port_added_listener listener, void *data)
{
	enum bt_graph_status status = BT_GRAPH_STATUS_OK;

	if (!graph || !listener) {
		status = BT_GRAPH_STATUS_INVALID;
		goto end;
	}

	add_listener(graph->listeners.port_added, listener, data);

end:
	return status;
}

enum bt_graph_status bt_graph_add_port_removed_listener(
		struct bt_graph *graph,
		bt_graph_port_removed_listener listener, void *data)
{
	enum bt_graph_status status = BT_GRAPH_STATUS_OK;

	if (!graph || !listener) {
		status = BT_GRAPH_STATUS_INVALID;
		goto end;
	}

	add_listener(graph->listeners.port_removed, listener, data);

end:
	return status;
}

enum bt_graph_status bt_graph_add_ports_connected_listener(
		struct bt_graph *graph,
		bt_graph_ports_connected_listener listener, void *data)
{
	enum bt_graph_status status = BT_GRAPH_STATUS_OK;

	if (!graph || !listener) {
		status = BT_GRAPH_STATUS_INVALID;
		goto end;
	}

	add_listener(graph->listeners.ports_connected, listener, data);

end:
	return status;
}

enum bt_graph_status bt_graph_add_ports_disconnected_listener(
		struct bt_graph *graph,
		bt_graph_ports_disconnected_listener listener, void *data)
{
	enum bt_graph_status status = BT_GRAPH_STATUS_OK;

	if (!graph || !listener) {
		status = BT_GRAPH_STATUS_INVALID;
		goto end;
	}

	add_listener(graph->listeners.ports_disconnected, listener, data);

end:
	return status;
}

BT_HIDDEN
void bt_graph_notify_port_added(struct bt_graph *graph, struct bt_port *port)
{
	size_t i;

	for (i = 0; i < graph->listeners.port_added->len; i++) {
		struct bt_graph_listener listener =
			g_array_index(graph->listeners.port_added,
				struct bt_graph_listener, i);
		bt_graph_port_added_listener func = listener.func;

		assert(func);
		func(port, listener.data);
	}
}

BT_HIDDEN
void bt_graph_notify_port_removed(struct bt_graph *graph,
		struct bt_component *comp, struct bt_port *port)
{
	size_t i;

	for (i = 0; i < graph->listeners.port_removed->len; i++) {
		struct bt_graph_listener listener =
			g_array_index(graph->listeners.port_removed,
				struct bt_graph_listener, i);
		bt_graph_port_removed_listener func = listener.func;

		assert(func);
		func(comp, port, listener.data);
	}
}

BT_HIDDEN
void bt_graph_notify_ports_connected(struct bt_graph *graph,
		struct bt_port *upstream_port, struct bt_port *downstream_port)
{
	size_t i;

	for (i = 0; i < graph->listeners.ports_connected->len; i++) {
		struct bt_graph_listener listener =
			g_array_index(graph->listeners.ports_connected,
				struct bt_graph_listener, i);
		bt_graph_ports_connected_listener func = listener.func;

		assert(func);
		func(upstream_port, downstream_port, listener.data);
	}
}

BT_HIDDEN
void bt_graph_notify_ports_disconnected(struct bt_graph *graph,
		struct bt_component *upstream_comp,
		struct bt_component *downstream_comp,
		struct bt_port *upstream_port, struct bt_port *downstream_port)
{
	size_t i;

	for (i = 0; i < graph->listeners.ports_disconnected->len; i++) {
		struct bt_graph_listener listener =
			g_array_index(graph->listeners.ports_disconnected,
				struct bt_graph_listener, i);
		bt_graph_ports_disconnected_listener func = listener.func;

		assert(func);
		func(upstream_comp, downstream_comp, upstream_port,
			downstream_port, listener.data);
	}
}

extern enum bt_graph_status bt_graph_cancel(struct bt_graph *graph)
{
	enum bt_graph_status ret = BT_GRAPH_STATUS_OK;

	if (!graph) {
		ret = BT_GRAPH_STATUS_INVALID;
		goto end;
	}

	graph->canceled = BT_TRUE;

end:
	return ret;
}

extern bt_bool bt_graph_is_canceled(struct bt_graph *graph)
{
	return graph ? graph->canceled : BT_FALSE;
}
