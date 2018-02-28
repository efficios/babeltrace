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
#include <babeltrace/values.h>
#include <babeltrace/values-internal.h>
#include <babeltrace/assert-internal.h>
#include <babeltrace/assert-pre-internal.h>
#include <unistd.h>
#include <glib.h>

struct bt_graph_listener {
	void *func;
	bt_graph_listener_removed removed;
	void *data;
};

static
int init_listeners_array(GArray **listeners)
{
	int ret = 0;

	BT_ASSERT(listeners);
	*listeners = g_array_new(FALSE, TRUE, sizeof(struct bt_graph_listener));
	if (!*listeners) {
		BT_LOGE_STR("Failed to allocate one GArray.");
		ret = -1;
		goto end;
	}

end:
	return ret;
}

static
void call_remove_listeners(GArray *listeners)
{
	size_t i;

	for (i = 0; i < listeners->len; i++) {
		struct bt_graph_listener listener =
			g_array_index(listeners, struct bt_graph_listener, i);

		if (listener.removed) {
			listener.removed(listener.data);
		}
	}
}

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

	/*
	 * Cancel the graph to disallow some operations, like creating
	 * notification iterators and adding ports to components.
	 */
	(void) bt_graph_cancel(graph);

	/* Call all remove listeners */
	call_remove_listeners(graph->listeners.port_added);
	call_remove_listeners(graph->listeners.port_removed);
	call_remove_listeners(graph->listeners.ports_connected);
	call_remove_listeners(graph->listeners.ports_disconnected);

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

	graph->can_consume = BT_TRUE;
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

enum bt_graph_status bt_graph_connect_ports(struct bt_graph *graph,
		struct bt_port *upstream_port, struct bt_port *downstream_port,
		struct bt_connection **user_connection)
{
	enum bt_graph_status status = BT_GRAPH_STATUS_OK;
	struct bt_connection *connection = NULL;
	struct bt_graph *upstream_graph = NULL;
	struct bt_graph *downstream_graph = NULL;
	struct bt_component *upstream_component = NULL;
	struct bt_component *downstream_component = NULL;
	enum bt_component_status component_status;
	bt_bool init_can_consume;

	if (!graph) {
		BT_LOGW_STR("Invalid parameter: graph is NULL.");
		status = BT_GRAPH_STATUS_INVALID;
		goto end;
	}
	init_can_consume = graph->can_consume;

	if (!upstream_port) {
		BT_LOGW_STR("Invalid parameter: upstream port is NULL.");
		status = BT_GRAPH_STATUS_INVALID;
		goto end;
	}

	if (!downstream_port) {
		BT_LOGW_STR("Invalid parameter: downstream port is NULL.");
		status = BT_GRAPH_STATUS_INVALID;
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
		status = BT_GRAPH_STATUS_CANCELED;
		goto end;
	}

	graph->can_consume = BT_FALSE;

	/* Ensure appropriate types for upstream and downstream ports. */
	if (bt_port_get_type(upstream_port) != BT_PORT_TYPE_OUTPUT) {
		BT_LOGW_STR("Invalid parameter: upstream port is not an output port.");
		status = BT_GRAPH_STATUS_INVALID;
		goto end;
	}
	if (bt_port_get_type(downstream_port) != BT_PORT_TYPE_INPUT) {
		BT_LOGW_STR("Invalid parameter: downstream port is not an input port.");
		status = BT_GRAPH_STATUS_INVALID;
		goto end;
	}

	/* Ensure that both ports are currently unconnected. */
	if (bt_port_is_connected(upstream_port)) {
		BT_LOGW_STR("Invalid parameter: upstream port is already connected.");
		status = BT_GRAPH_STATUS_INVALID;
		goto end;
	}

	if (bt_port_is_connected(downstream_port)) {
		BT_LOGW_STR("Invalid parameter: downstream port is already connected.");
		status = BT_GRAPH_STATUS_INVALID;
		goto end;
	}

	/*
	 * Ensure that both ports are still attached to their creating
	 * component.
	 */
	upstream_component = bt_port_get_component(upstream_port);
	if (!upstream_component) {
		BT_LOGW_STR("Invalid parameter: upstream port is loose (does not belong to a component)");
		status = BT_GRAPH_STATUS_INVALID;
		goto end;
	}

	downstream_component = bt_port_get_component(downstream_port);
	if (!downstream_component) {
		BT_LOGW_STR("Invalid parameter: downstream port is loose (does not belong to a component)");
		status = BT_GRAPH_STATUS_INVALID;
		goto end;
	}

	BT_LOGD("Connecting component ports: "
		"upstream-comp-addr=%p, upstream-comp-name=\"%s\", "
		"downstream-comp-addr=%p, downstream-comp-name=\"%s\"",
		upstream_component, bt_component_get_name(upstream_component),
		downstream_component, bt_component_get_name(downstream_component));

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

		status = bt_graph_status_from_component_status(
			component_status);
		goto end;
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

		status = bt_graph_status_from_component_status(
			component_status);
		goto end;
	}

	BT_LOGD_STR("Creating connection.");
	connection = bt_connection_create(graph, upstream_port,
			downstream_port);
	if (!connection) {
		BT_LOGW("Cannot create connection object.");
		status = BT_GRAPH_STATUS_NOMEM;
		goto end;
	}

	BT_LOGD("Connection object created: conn-addr=%p", connection);

	/*
	 * Ownership of upstream_component/downstream_component and of
	 * the connection object is transferred to the graph.
	 */
	g_ptr_array_add(graph->connections, connection);

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

	if (user_connection) {
		/* Move reference to user */
		*user_connection = connection;
		connection = NULL;
	}

end:
	bt_put(upstream_graph);
	bt_put(downstream_graph);
	bt_put(upstream_component);
	bt_put(downstream_component);
	bt_put(connection);
	if (graph) {
		graph->can_consume = init_can_consume;
	}
	return status;
}

static
enum bt_graph_status consume_graph_sink(struct bt_component *sink)
{
	enum bt_graph_status status = BT_GRAPH_STATUS_OK;
	enum bt_component_status comp_status;

	BT_ASSERT(sink);
	comp_status = bt_component_sink_consume(sink);
	BT_LOGV("Consumed from sink: addr=%p, name=\"%s\", status=%s",
		sink, bt_component_get_name(sink),
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

	return status;
}

/*
 * `node` is removed from the queue of sinks to consume when passed to
 * this function. This function adds it back to the queue if there's
 * still something to consume afterwards.
 */
static
enum bt_graph_status consume_sink_node(struct bt_graph *graph,
		GList *node)
{
	enum bt_graph_status status;
	struct bt_component *sink;

	sink = node->data;
	status = consume_graph_sink(sink);
	if (status != BT_GRAPH_STATUS_END) {
		g_queue_push_tail_link(graph->sinks_to_consume, node);
		goto end;
	}

	/* End reached, the node is not added back to the queue and free'd. */
	g_queue_delete_link(graph->sinks_to_consume, node);

	/* Don't forward an END status if there are sinks left to consume. */
	if (!g_queue_is_empty(graph->sinks_to_consume)) {
		status = BT_GRAPH_STATUS_OK;
		goto end;
	}

end:
	BT_LOGV("Consumed sink node: status=%s", bt_graph_status_string(status));
	return status;
}

BT_HIDDEN
enum bt_graph_status bt_graph_consume_sink_no_check(struct bt_graph *graph,
		struct bt_component *sink)
{
	enum bt_graph_status status;
	GList *sink_node;
	int index;

	BT_LOGV("Making specific sink consume: addr=%p, "
		"comp-addr=%p, comp-name=\"%s\"",
		graph, sink, bt_component_get_name(sink));

	BT_ASSERT(bt_component_borrow_graph(sink) == graph);

	if (g_queue_is_empty(graph->sinks_to_consume)) {
		BT_LOGV_STR("Graph's sink queue is empty: end of graph.");
		status = BT_GRAPH_STATUS_END;
		goto end;
	}

	index = g_queue_index(graph->sinks_to_consume, sink);
	if (index < 0) {
		BT_LOGV_STR("Sink is not marked as consumable: sink is ended.");
		status = BT_GRAPH_STATUS_END;
		goto end;
	}

	sink_node = g_queue_pop_nth_link(graph->sinks_to_consume, index);
	BT_ASSERT(sink_node);
	status = consume_sink_node(graph, sink_node);

end:
	return status;
}

BT_HIDDEN
enum bt_graph_status bt_graph_consume_no_check(struct bt_graph *graph)
{
	enum bt_graph_status status = BT_GRAPH_STATUS_OK;
	struct bt_component *sink;
	GList *current_node;

	BT_LOGV("Making next sink consume: addr=%p", graph);
	BT_ASSERT_PRE(graph->has_sink,
		"Graph has no sink component: %!+g", graph);

	if (g_queue_is_empty(graph->sinks_to_consume)) {
		BT_LOGV_STR("Graph's sink queue is empty: end of graph.");
		status = BT_GRAPH_STATUS_END;
		goto end;
	}

	current_node = g_queue_pop_head_link(graph->sinks_to_consume);
	sink = current_node->data;
	BT_LOGV("Chose next sink to consume: comp-addr=%p, comp-name=\"%s\"",
		sink, bt_component_get_name(sink));
	status = consume_sink_node(graph, current_node);

end:
	return status;
}

enum bt_graph_status bt_graph_consume(struct bt_graph *graph)
{
	enum bt_graph_status status;

	BT_ASSERT_PRE_NON_NULL(graph, "Graph");
	BT_ASSERT_PRE(!graph->canceled, "Graph is canceled: %!+g", graph);
	BT_ASSERT_PRE(graph->can_consume,
		"Cannot consume graph in its current state: %!+g", graph);
	graph->can_consume = BT_FALSE;
	status = bt_graph_consume_no_check(graph);
	graph->can_consume = BT_TRUE;
	return BT_GRAPH_STATUS_OK;
}

enum bt_graph_status bt_graph_run(struct bt_graph *graph)
{
	enum bt_graph_status status = BT_GRAPH_STATUS_OK;

	if (!graph) {
		BT_LOGW_STR("Invalid parameter: graph is NULL.");
		status = BT_GRAPH_STATUS_INVALID;
		goto end;
	}

	if (graph->canceled) {
		BT_LOGW("Invalid parameter: graph is canceled: "
			"graph-addr=%p", graph);
		status = BT_GRAPH_STATUS_CANCELED;
		goto end;
	}

	if (!graph->can_consume) {
		BT_LOGW_STR("Cannot run graph in its current state.");
		status = BT_GRAPH_STATUS_CANNOT_CONSUME;
		goto end;
	}

	graph->can_consume = BT_FALSE;
	BT_LOGV("Running graph: addr=%p", graph);

	do {
		/*
		 * Check if the graph is canceled at each iteration. If
		 * the graph was canceled by another thread or by a
		 * signal, this is not a warning nor an error, it was
		 * intentional: log with a DEBUG level only.
		 */
		if (graph->canceled) {
			BT_LOGD("Stopping the graph: graph is canceled: "
				"graph-addr=%p", graph);
			status = BT_GRAPH_STATUS_CANCELED;
			goto end;
		}

		status = bt_graph_consume_no_check(graph);
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
		} else if (status == BT_GRAPH_STATUS_NO_SINK) {
			goto end;
		}
	} while (status == BT_GRAPH_STATUS_OK);

	if (g_queue_is_empty(graph->sinks_to_consume)) {
		status = BT_GRAPH_STATUS_END;
	}

end:
	BT_LOGV("Graph ran: status=%s", bt_graph_status_string(status));
	if (graph) {
		graph->can_consume = BT_TRUE;
	}
	return status;
}

static
int add_listener(GArray *listeners, void *func, void *removed, void *data)
{
	struct bt_graph_listener listener = {
		.func = func,
		.removed = removed,
		.data = data,
	};

	g_array_append_val(listeners, listener);
	return listeners->len - 1;
}

int bt_graph_add_port_added_listener(
		struct bt_graph *graph,
		bt_graph_port_added_listener listener,
		bt_graph_listener_removed listener_removed, void *data)
{
	int ret;

	if (!graph) {
		BT_LOGW_STR("Invalid parameter: graph is NULL.");
		ret = -1;
		goto end;
	}

	if (graph->in_remove_listener) {
		BT_LOGW("Cannot call this function during the execution of a remove listener: "
			"addr=%p", graph);
		ret = -1;
		goto end;
	}

	if (!listener) {
		BT_LOGW_STR("Invalid parameter: listener is NULL.");
		ret = -1;
		goto end;
	}

	ret = add_listener(graph->listeners.port_added, listener,
		listener_removed, data);
	BT_LOGV("Added \"port added\" listener to graph: "
		"graph-addr=%p, listener-addr=%p, pos=%d",
		graph, listener, ret);

end:
	return ret;
}

int bt_graph_add_port_removed_listener(
		struct bt_graph *graph,
		bt_graph_port_removed_listener listener,
		bt_graph_listener_removed listener_removed, void *data)
{
	int ret;

	if (!graph) {
		BT_LOGW_STR("Invalid parameter: graph is NULL.");
		ret = -1;
		goto end;
	}

	if (graph->in_remove_listener) {
		BT_LOGW("Cannot call this function during the execution of a remove listener: "
			"addr=%p", graph);
		ret = -1;
		goto end;
	}

	if (!listener) {
		BT_LOGW_STR("Invalid parameter: listener is NULL.");
		ret = -1;
		goto end;
	}

	ret = add_listener(graph->listeners.port_removed, listener,
		listener_removed, data);
	BT_LOGV("Added \"port removed\" listener to graph: "
		"graph-addr=%p, listener-addr=%p, pos=%d",
		graph, listener, ret);

end:
	return ret;
}

int bt_graph_add_ports_connected_listener(
		struct bt_graph *graph,
		bt_graph_ports_connected_listener listener,
		bt_graph_listener_removed listener_removed, void *data)
{
	int ret;

	if (!graph) {
		BT_LOGW_STR("Invalid parameter: graph is NULL.");
		ret = -1;
		goto end;
	}

	if (graph->in_remove_listener) {
		BT_LOGW("Cannot call this function during the execution of a remove listener: "
			"addr=%p", graph);
		ret = -1;
		goto end;
	}

	if (!listener) {
		BT_LOGW_STR("Invalid parameter: listener is NULL.");
		ret = -1;
		goto end;
	}

	ret = add_listener(graph->listeners.ports_connected, listener,
		listener_removed, data);
	BT_LOGV("Added \"port connected\" listener to graph: "
		"graph-addr=%p, listener-addr=%p, pos=%d",
		graph, listener, ret);

end:
	return ret;
}

int bt_graph_add_ports_disconnected_listener(
		struct bt_graph *graph,
		bt_graph_ports_disconnected_listener listener,
		bt_graph_listener_removed listener_removed, void *data)
{
	int ret;

	if (!graph) {
		BT_LOGW_STR("Invalid parameter: graph is NULL.");
		ret = -1;
		goto end;
	}

	if (graph->in_remove_listener) {
		BT_LOGW("Cannot call this function during the execution of a remove listener: "
			"addr=%p", graph);
		ret = -1;
		goto end;
	}

	if (!listener) {
		BT_LOGW_STR("Invalid parameter: listener is NULL.");
		ret = -1;
		goto end;
	}

	ret = add_listener(graph->listeners.ports_disconnected, listener,
		listener_removed, data);
	BT_LOGV("Added \"port disconnected\" listener to graph: "
		"graph-addr=%p, listener-addr=%p, pos=%d",
		graph, listener, ret);

end:
	return ret;
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

		BT_ASSERT(func);
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

		BT_ASSERT(func);
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

		BT_ASSERT(func);
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

		BT_ASSERT(func);
		func(upstream_comp, downstream_comp, upstream_port,
			downstream_port, listener.data);
	}
}

enum bt_graph_status bt_graph_cancel(struct bt_graph *graph)
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

bt_bool bt_graph_is_canceled(struct bt_graph *graph)
{
	bt_bool canceled = BT_FALSE;

	if (!graph) {
		BT_LOGW_STR("Invalid parameter: graph is NULL.");
		goto end;
	}

	canceled = graph->canceled;

end:
	return canceled;
}

BT_HIDDEN
void bt_graph_remove_connection(struct bt_graph *graph,
		struct bt_connection *connection)
{
	BT_ASSERT(graph);
	BT_ASSERT(connection);
	BT_LOGV("Removing graph's connection: graph-addr=%p, conn-addr=%p",
		graph, connection);
	g_ptr_array_remove(graph->connections, connection);
}

enum bt_graph_status bt_graph_add_component_with_init_method_data(
		struct bt_graph *graph,
		struct bt_component_class *component_class,
		const char *name, struct bt_value *params,
		void *init_method_data,
		struct bt_component **user_component)
{
	enum bt_graph_status graph_status = BT_GRAPH_STATUS_OK;
	enum bt_component_status comp_status;
	struct bt_component *component = NULL;
	enum bt_component_class_type type;
	size_t i;
	bt_bool init_can_consume;

	bt_get(params);

	if (!graph) {
		BT_LOGW_STR("Invalid parameter: graph is NULL.");
		graph_status = BT_GRAPH_STATUS_INVALID;
		goto end;
	}
	init_can_consume = graph->can_consume;

	if (!component_class) {
		BT_LOGW_STR("Invalid parameter: component class is NULL.");
		graph_status = BT_GRAPH_STATUS_INVALID;
		goto end;
	}

	graph->can_consume = BT_FALSE;
	type = bt_component_class_get_type(component_class);
	BT_LOGD("Adding component to graph: "
		"graph-addr=%p, comp-cls-addr=%p, "
		"comp-cls-type=%s, name=\"%s\", params-addr=%p, "
		"init-method-data-addr=%p",
		graph, component_class, bt_component_class_type_string(type),
		name, params, init_method_data);

	if (!name) {
		BT_LOGW_STR("Invalid parameter: name is NULL.");
		graph_status = BT_GRAPH_STATUS_INVALID;
		goto end;
	}

	if (graph->canceled) {
		BT_LOGW_STR("Invalid parameter: graph is canceled.");
		graph_status = BT_GRAPH_STATUS_CANCELED;
		goto end;
	}

	if (type != BT_COMPONENT_CLASS_TYPE_SOURCE &&
			type != BT_COMPONENT_CLASS_TYPE_FILTER &&
			type != BT_COMPONENT_CLASS_TYPE_SINK) {
		BT_LOGW("Invalid parameter: unknown component class type: "
			"type=%d", type);
		graph_status = BT_GRAPH_STATUS_INVALID;
		goto end;
	}

	for (i = 0; i < graph->components->len; i++) {
		void *other_comp = graph->components->pdata[i];

		if (strcmp(name, bt_component_get_name(other_comp)) == 0) {
			BT_LOGW("Invalid parameter: another component with the same name already exists in the graph: "
				"other-comp-addr=%p, name=\"%s\"",
				other_comp, name);
			graph_status = BT_GRAPH_STATUS_INVALID;
			goto end;
		}
	}

	/*
	 * Parameters must be a map value, but we create a convenient
	 * empty one if it's NULL.
	 */
	if (params) {
		if (!bt_value_is_map(params)) {
			BT_LOGW("Invalid parameter: initialization parameters must be a map value: "
				"type=%s",
				bt_value_type_string(bt_value_get_type(params)));
			graph_status = BT_GRAPH_STATUS_INVALID;
			goto end;
		}
	} else {
		params = bt_value_map_create();
		if (!params) {
			BT_LOGE_STR("Cannot create map value object.");
			graph_status = BT_GRAPH_STATUS_NOMEM;
			goto end;
		}
	}

	comp_status = bt_component_create(component_class, name, &component);
	if (comp_status != BT_COMPONENT_STATUS_OK) {
		BT_LOGE("Cannot create empty component object: status=%s",
			bt_component_status_string(comp_status));
		graph_status = bt_graph_status_from_component_status(
			comp_status);
		goto end;
	}

	/*
	 * The user's initialization method needs to see that this
	 * component is part of the graph. If the user method fails, we
	 * immediately remove the component from the graph's components.
	 */
	g_ptr_array_add(graph->components, component);
	bt_component_set_graph(component, graph);

	if (component_class->methods.init) {
		BT_LOGD_STR("Calling user's initialization method.");
		comp_status = component_class->methods.init(
			bt_private_component_from_component(component), params,
			init_method_data);
		BT_LOGD("User method returned: status=%s",
			bt_component_status_string(comp_status));
		if (comp_status != BT_COMPONENT_STATUS_OK) {
			BT_LOGW_STR("Initialization method failed.");
			graph_status = bt_graph_status_from_component_status(
				comp_status);
			bt_component_set_graph(component, NULL);
			g_ptr_array_remove_fast(graph->components, component);
			goto end;
		}
	}

	/*
	 * Mark the component as initialized so that its finalization
	 * method is called when it is destroyed.
	 */
	component->initialized = true;

	/*
	 * If it's a sink component, it needs to be part of the graph's
	 * sink queue to be consumed by bt_graph_consume().
	 */
	if (bt_component_is_sink(component)) {
		graph->has_sink = BT_TRUE;
		g_queue_push_tail(graph->sinks_to_consume, component);
	}

	/*
	 * Freeze the component class now that it's instantiated at
	 * least once.
	 */
	BT_LOGD_STR("Freezing component class.");
	bt_component_class_freeze(component->class);
	BT_LOGD("Added component to graph: "
		"graph-addr=%p, comp-cls-addr=%p, "
		"comp-cls-type=%s, name=\"%s\", params-addr=%p, "
		"init-method-data-addr=%p, comp-addr=%p",
		graph, component_class, bt_component_class_type_string(type),
		name, params, init_method_data, component);

	if (user_component) {
		/* Move reference to user */
		*user_component = component;
		component = NULL;
	}

end:
	bt_put(component);
	bt_put(params);
	if (graph) {
		graph->can_consume = init_can_consume;
	}
	return graph_status;
}

enum bt_graph_status bt_graph_add_component(
		struct bt_graph *graph,
		struct bt_component_class *component_class,
		const char *name, struct bt_value *params,
		struct bt_component **component)
{
	return bt_graph_add_component_with_init_method_data(graph,
		component_class, name, params, NULL, component);
}

BT_HIDDEN
int bt_graph_remove_unconnected_component(struct bt_graph *graph,
		struct bt_component *component)
{
	bt_bool init_can_consume;
	int64_t count;
	uint64_t i;
	int ret = 0;

	BT_ASSERT(graph);
	BT_ASSERT(component);
	BT_ASSERT(component->base.ref_count.count == 0);
	BT_ASSERT(bt_component_borrow_graph(component) == graph);

	init_can_consume = graph->can_consume;
	count = bt_component_get_input_port_count(component);

	for (i = 0; i < count; i++) {
		struct bt_port *port =
			bt_component_get_input_port_by_index(component, i);

		BT_ASSERT(port);
		bt_put(port);

		if (bt_port_is_connected(port)) {
			BT_LOGW("Cannot remove component from graph: "
				"an input port is connected: "
				"graph-addr=%p, comp-addr=%p, "
				"comp-name=\"%s\", connected-port-addr=%p, "
				"connected-port-name=\"%s\"",
				graph, component,
				bt_component_get_name(component),
				port, bt_port_get_name(port));
			goto error;
		}
	}

	count = bt_component_get_output_port_count(component);

	for (i = 0; i < count; i++) {
		struct bt_port *port =
			bt_component_get_output_port_by_index(component, i);

		BT_ASSERT(port);
		bt_put(port);

		if (bt_port_is_connected(port)) {
			BT_LOGW("Cannot remove component from graph: "
				"an output port is connected: "
				"graph-addr=%p, comp-addr=%p, "
				"comp-name=\"%s\", connected-port-addr=%p, "
				"connected-port-name=\"%s\"",
				graph, component,
				bt_component_get_name(component),
				port, bt_port_get_name(port));
			goto error;
		}
	}

	graph->can_consume = BT_FALSE;

	/* Possibly remove from sinks to consume */
	(void) g_queue_remove(graph->sinks_to_consume, component);

	if (graph->sinks_to_consume->length == 0) {
		graph->has_sink = BT_FALSE;
	}

	/*
	 * This calls bt_object_release() on the component, and since
	 * its reference count is 0, its destructor is called. Its
	 * destructor calls the user's finalization method (if set).
	 */
	g_ptr_array_remove(graph->components, component);
	goto end;

error:
	ret = -1;

end:
	graph->can_consume = init_can_consume;
	return ret;
}
