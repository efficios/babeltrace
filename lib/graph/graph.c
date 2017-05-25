/*
 * graph.c
 *
 * Babeltrace Plugin Component Graph
 *
 * Copyright 2017 Jérémie Galarneau <jeremie.galarneau@efficios.com>
 * Copyright 2017 Philippe Proulx <pproulx@efficios.com>
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

#define BT_LOG_TAG "GRAPH"
#include <babeltrace/lib-logging-internal.h>

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
	BT_LOGD("Destroying graph: addr=%p", graph);
	obj->ref_count.count++;

	if (graph->connections) {
		BT_LOGD_STR("Destroying connections.");
		g_ptr_array_free(graph->connections, TRUE);
	}
	if (graph->components) {
		BT_LOGD_STR("Destroying components.");
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
		BT_LOGE_STR("Failed to allocate one GArray.");
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

	BT_LOGD_STR("Creating graph object.");
	graph = g_new0(struct bt_graph, 1);
	if (!graph) {
		BT_LOGE_STR("Failed to allocate one graph.");
		goto end;
	}

	bt_object_init(graph, bt_graph_destroy);

	graph->connections = g_ptr_array_new_with_free_func(bt_object_release);
	if (!graph->connections) {
		BT_LOGE_STR("Failed to allocate one GPtrArray.");
		goto error;
	}
	graph->components = g_ptr_array_new_with_free_func(bt_object_release);
	if (!graph->components) {
		BT_LOGE_STR("Failed to allocate one GPtrArray.");
		goto error;
	}
	graph->sinks_to_consume = g_queue_new();
	if (!graph->sinks_to_consume) {
		BT_LOGE_STR("Failed to allocate one GQueue.");
		goto error;
	}

	ret = init_listeners_array(&graph->listeners.port_added);
	if (ret) {
		BT_LOGE_STR("Cannot create the \"port added\" listener array.");
		goto error;
	}

	ret = init_listeners_array(&graph->listeners.port_removed);
	if (ret) {
		BT_LOGE_STR("Cannot create the \"port removed\" listener array.");
		goto error;
	}

	ret = init_listeners_array(&graph->listeners.ports_connected);
	if (ret) {
		BT_LOGE_STR("Cannot create the \"port connected\" listener array.");
		goto error;
	}

	ret = init_listeners_array(&graph->listeners.ports_disconnected);
	if (ret) {
		BT_LOGE_STR("Cannot create the \"port disconneted\" listener array.");
		goto error;
	}

	BT_LOGD("Created graph object: addr=%p", graph);

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

	if (!graph) {
		BT_LOGW_STR("Invalid parameter: graph is NULL.");
		goto end;
	}

	if (!upstream_port) {
		BT_LOGW_STR("Invalid parameter: upstream port is NULL.");
		goto end;
	}

	if (!downstream_port) {
		BT_LOGW_STR("Invalid parameter: downstream port is NULL.");
		goto end;
	}

	BT_LOGD("Connecting component ports within graph: "
		"graph-addr=%p, "
		"upstream-port-addr=%p, upstream-port-name=\"%s\", "
		"downstream-port-addr=%p, downstream-port-name=\"%s\"",
		graph, upstream_port, bt_port_get_name(upstream_port),
		downstream_port, bt_port_get_name(downstream_port));

	if (graph->canceled) {
		BT_LOGW_STR("Invalid parameter: graph is canceled.");
		goto end;
	}

	/* Ensure appropriate types for upstream and downstream ports. */
	if (bt_port_get_type(upstream_port) != BT_PORT_TYPE_OUTPUT) {
		BT_LOGW_STR("Invalid parameter: upstream port is not an output port.");
		goto end;
	}
	if (bt_port_get_type(downstream_port) != BT_PORT_TYPE_INPUT) {
		BT_LOGW_STR("Invalid parameter: downstream port is not an input port.");
		goto end;
	}

	/* Ensure that both ports are currently unconnected. */
	if (bt_port_is_connected(upstream_port)) {
		BT_LOGW_STR("Invalid parameter: upstream port is already connected.");
		goto end;
	}

	if (bt_port_is_connected(downstream_port)) {
		BT_LOGW_STR("Invalid parameter: downstream port is already connected.");
		goto end;
	}

	/*
	 * Ensure that both ports are still attached to their creating
	 * component.
	 */
	upstream_component = bt_port_get_component(upstream_port);
	if (!upstream_component) {
		BT_LOGW_STR("Invalid parameter: upstream port is loose (does not belong to a component)");
		goto end;
	}

	downstream_component = bt_port_get_component(downstream_port);
	if (!downstream_component) {
		BT_LOGW_STR("Invalid parameter: downstream port is loose (does not belong to a component)");
		goto end;
	}

	BT_LOGD("Connecting component ports: "
		"upstream-comp-addr=%p, upstream-comp-name=\"%s\", "
		"downstream-comp-addr=%p, downstream-comp-name=\"%s\"",
		upstream_component, bt_component_get_name(upstream_component),
		downstream_component, bt_component_get_name(downstream_component));

	/* Ensure the components are not already part of another graph. */
	upstream_graph = bt_component_get_graph(upstream_component);
	if (upstream_graph && (graph != upstream_graph)) {
		BT_LOGW("Invalid parameter: upstream port's component is already part of another graph: "
			"other-graph-addr=%p", upstream_graph);
		goto error;
	}
	upstream_was_already_in_graph = (graph == upstream_graph);
	downstream_graph = bt_component_get_graph(downstream_component);
	if (downstream_graph && (graph != downstream_graph)) {
		BT_LOGW("Invalid parameter: downstream port's component is already part of another graph: "
			"other-graph-addr=%p", downstream_graph);
		goto error;
	}
	downstream_was_already_in_graph = (graph == downstream_graph);

	/*
	 * At this point the ports are not connected yet. Both
	 * components need to accept an eventual connection to their
	 * port by the other port before we continue.
	 */
	BT_LOGD_STR("Asking upstream component to accept the connection.");
	component_status = bt_component_accept_port_connection(
		upstream_component, upstream_port, downstream_port);
	if (component_status != BT_COMPONENT_STATUS_OK) {
		if (component_status == BT_COMPONENT_STATUS_REFUSE_PORT_CONNECTION) {
			BT_LOGD_STR("Upstream component refused the connection.");
		} else {
			BT_LOGW("Cannot ask upstream component to accept the connection: "
				"status=%s", bt_component_status_string(component_status));
		}

		goto error;
	}

	BT_LOGD_STR("Asking downstream component to accept the connection.");
	component_status = bt_component_accept_port_connection(
		downstream_component, downstream_port, upstream_port);
	if (component_status != BT_COMPONENT_STATUS_OK) {
		if (component_status == BT_COMPONENT_STATUS_REFUSE_PORT_CONNECTION) {
			BT_LOGD_STR("Downstream component refused the connection.");
		} else {
			BT_LOGW("Cannot ask downstream component to accept the connection: "
				"status=%s", bt_component_status_string(component_status));
		}

		goto error;
	}

	BT_LOGD_STR("Creating connection.");
	connection = bt_connection_create(graph, upstream_port,
			downstream_port);
	if (!connection) {
		BT_LOGW("Cannot create connection object.");
		goto error;
	}

	BT_LOGD("Connection object created: conn-addr=%p", connection);

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
	BT_LOGD_STR("Notifying upstream component that its port is connected.");
	bt_component_port_connected(upstream_component, upstream_port,
		downstream_port);
	BT_LOGD_STR("Notifying downstream component that its port is connected.");
	bt_component_port_connected(downstream_component, downstream_port,
		upstream_port);

	/*
	 * Notify the graph's creator that both ports are connected.
	 */
	BT_LOGD_STR("Notifying graph's user that new component ports are connected.");
	bt_graph_notify_ports_connected(graph, upstream_port, downstream_port);
	BT_LOGD("Connected component ports within graph: "
		"graph-addr=%p, "
		"upstream-comp-addr=%p, upstream-comp-name=\"%s\", "
		"downstream-comp-addr=%p, downstream-comp-name=\"%s\", "
		"upstream-port-addr=%p, upstream-port-name=\"%s\", "
		"downstream-port-addr=%p, downstream-port-name=\"%s\"",
		graph,
		upstream_component, bt_component_get_name(upstream_component),
		downstream_component, bt_component_get_name(downstream_component),
		upstream_port, bt_port_get_name(upstream_port),
		downstream_port, bt_port_get_name(downstream_port));

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

enum bt_graph_status bt_graph_consume(struct bt_graph *graph)
{
	struct bt_component *sink;
	enum bt_graph_status status = BT_GRAPH_STATUS_OK;
	enum bt_component_status comp_status;
	GList *current_node;

	if (!graph) {
		BT_LOGW_STR("Invalid parameter: graph is NULL.");
		status = BT_GRAPH_STATUS_INVALID;
		goto end;
	}

	BT_LOGV("Making sink consume: addr=%p", graph);

	if (graph->canceled) {
		BT_LOGW_STR("Invalid parameter: graph is canceled.");
		status = BT_GRAPH_STATUS_CANCELED;
		goto end;
	}

	if (g_queue_is_empty(graph->sinks_to_consume)) {
		BT_LOGV_STR("Graph's sink queue is empty: end of graph.");
		status = BT_GRAPH_STATUS_END;
		goto end;
	}

	current_node = g_queue_pop_head_link(graph->sinks_to_consume);
	sink = current_node->data;
	BT_LOGV("Chose next sink to consume: comp-addr=%p, comp-name=\"%s\"",
		sink, bt_component_get_name(sink));
	comp_status = bt_component_sink_consume(sink);
	BT_LOGV("Consumed from sink: status=%s",
		bt_component_status_string(comp_status));
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
	BT_LOGV("Graph consumed: status=%s", bt_graph_status_string(status));
	return status;
}

enum bt_graph_status bt_graph_run(struct bt_graph *graph)
{
	enum bt_graph_status status = BT_GRAPH_STATUS_OK;

	if (!graph) {
		BT_LOGW_STR("Invalid parameter: graph is NULL.");
		status = BT_GRAPH_STATUS_INVALID;
		goto end;
	}

	BT_LOGV("Running graph: addr=%p", graph);

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
	BT_LOGV("Graph ran: status=%s", bt_graph_status_string(status));
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

	if (!graph) {
		BT_LOGW_STR("Invalid parameter: graph is NULL.");
		status = BT_GRAPH_STATUS_INVALID;
		goto end;
	}

	if (!listener) {
		BT_LOGW_STR("Invalid parameter: listener is NULL.");
		status = BT_GRAPH_STATUS_INVALID;
		goto end;
	}

	add_listener(graph->listeners.port_added, listener, data);
	BT_LOGV("Added \"port added\" listener to graph: "
		"graph-addr=%p, listener-addr=%p", graph, listener);

end:
	return status;
}

enum bt_graph_status bt_graph_add_port_removed_listener(
		struct bt_graph *graph,
		bt_graph_port_removed_listener listener, void *data)
{
	enum bt_graph_status status = BT_GRAPH_STATUS_OK;

	if (!graph) {
		BT_LOGW_STR("Invalid parameter: graph is NULL.");
		status = BT_GRAPH_STATUS_INVALID;
		goto end;
	}

	if (!listener) {
		BT_LOGW_STR("Invalid parameter: listener is NULL.");
		status = BT_GRAPH_STATUS_INVALID;
		goto end;
	}

	add_listener(graph->listeners.port_removed, listener, data);
	BT_LOGV("Added \"port removed\" listener to graph: "
		"graph-addr=%p, listener-addr=%p", graph, listener);

end:
	return status;
}

enum bt_graph_status bt_graph_add_ports_connected_listener(
		struct bt_graph *graph,
		bt_graph_ports_connected_listener listener, void *data)
{
	enum bt_graph_status status = BT_GRAPH_STATUS_OK;

	if (!graph) {
		BT_LOGW_STR("Invalid parameter: graph is NULL.");
		status = BT_GRAPH_STATUS_INVALID;
		goto end;
	}

	if (!listener) {
		BT_LOGW_STR("Invalid parameter: listener is NULL.");
		status = BT_GRAPH_STATUS_INVALID;
		goto end;
	}

	add_listener(graph->listeners.ports_connected, listener, data);
	BT_LOGV("Added \"port connected\" listener to graph: "
		"graph-addr=%p, listener-addr=%p", graph, listener);

end:
	return status;
}

enum bt_graph_status bt_graph_add_ports_disconnected_listener(
		struct bt_graph *graph,
		bt_graph_ports_disconnected_listener listener, void *data)
{
	enum bt_graph_status status = BT_GRAPH_STATUS_OK;

	if (!graph) {
		BT_LOGW_STR("Invalid parameter: graph is NULL.");
		status = BT_GRAPH_STATUS_INVALID;
		goto end;
	}

	if (!listener) {
		BT_LOGW_STR("Invalid parameter: listener is NULL.");
		status = BT_GRAPH_STATUS_INVALID;
		goto end;
	}

	add_listener(graph->listeners.ports_disconnected, listener, data);
	BT_LOGV("Added \"port disconnected\" listener to graph: "
		"graph-addr=%p, listener-addr=%p", graph, listener);

end:
	return status;
}

BT_HIDDEN
void bt_graph_notify_port_added(struct bt_graph *graph, struct bt_port *port)
{
	size_t i;

	BT_LOGV("Notifying graph listeners that a port was added: "
		"graph-addr=%p, port-addr=%p, port-name=\"%s\"",
		graph, port, bt_port_get_name(port));

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

	BT_LOGV("Notifying graph listeners that a port was removed: "
		"graph-addr=%p, port-addr=%p, port-name=\"%s\"",
		graph, port, bt_port_get_name(port));

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

	BT_LOGV("Notifying graph listeners that two ports were connected: "
		"graph-addr=%p, "
		"upstream-port-addr=%p, upstream-port-name=\"%s\", "
		"downstream-port-addr=%p, downstream-port-name=\"%s\"",
		graph, upstream_port, bt_port_get_name(upstream_port),
		downstream_port, bt_port_get_name(downstream_port));

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

	BT_LOGV("Notifying graph listeners that two ports were disconnected: "
		"graph-addr=%p, "
		"upstream-port-addr=%p, upstream-port-name=\"%s\", "
		"downstream-port-addr=%p, downstream-port-name=\"%s\"",
		graph, upstream_port, bt_port_get_name(upstream_port),
		downstream_port, bt_port_get_name(downstream_port));

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
		BT_LOGW_STR("Invalid parameter: graph is NULL.");
		ret = BT_GRAPH_STATUS_INVALID;
		goto end;
	}

	graph->canceled = BT_TRUE;
	BT_LOGV("Canceled graph: addr=%p", graph);

end:
	return ret;
}

extern bt_bool bt_graph_is_canceled(struct bt_graph *graph)
{
	return graph ? graph->canceled : BT_FALSE;
}

BT_HIDDEN
void bt_graph_remove_connection(struct bt_graph *graph,
		struct bt_connection *connection)
{
	assert(graph);
	assert(connection);
	BT_LOGV("Removing graph's connection: graph-addr=%p, conn-addr=%p",
		graph, connection);
	g_ptr_array_remove(graph->connections, connection);
}
