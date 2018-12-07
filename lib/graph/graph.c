/*
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
#include <babeltrace/graph/graph.h>
#include <babeltrace/graph/graph-const.h>
#include <babeltrace/graph/graph-internal.h>
#include <babeltrace/graph/connection-internal.h>
#include <babeltrace/graph/component-sink-internal.h>
#include <babeltrace/graph/component-source-const.h>
#include <babeltrace/graph/component-filter-const.h>
#include <babeltrace/graph/port-const.h>
#include <babeltrace/graph/notification-internal.h>
#include <babeltrace/graph/notification-event-internal.h>
#include <babeltrace/graph/notification-packet-internal.h>
#include <babeltrace/compiler-internal.h>
#include <babeltrace/common-internal.h>
#include <babeltrace/types.h>
#include <babeltrace/values.h>
#include <babeltrace/values-const.h>
#include <babeltrace/values-internal.h>
#include <babeltrace/object.h>
#include <babeltrace/assert-internal.h>
#include <babeltrace/assert-pre-internal.h>
#include <unistd.h>
#include <glib.h>

typedef void (*port_added_func_t)(const void *, const void *, void *);

typedef void (*port_removed_func_t)(const void *, const void *, void *);

typedef void (*ports_connected_func_t)(const void *, const void *, const void *,
		const void *, void *);

typedef void (*ports_disconnected_func_t)(const void *, const void *,
		const void *, const void *, void *);

typedef enum bt_self_component_status (*comp_init_method_t)(const void *,
		const void *, void *);

struct bt_graph_listener {
	bt_graph_listener_removed_func removed;
	void *data;
};

struct bt_graph_listener_port_added {
	struct bt_graph_listener base;
	port_added_func_t func;
};

struct bt_graph_listener_port_removed {
	struct bt_graph_listener base;
	port_removed_func_t func;
};

struct bt_graph_listener_ports_connected {
	struct bt_graph_listener base;
	ports_connected_func_t func;
};

struct bt_graph_listener_ports_disconnected {
	struct bt_graph_listener base;
	ports_disconnected_func_t func;
};

#define INIT_LISTENERS_ARRAY(_type, _listeners)				\
	do {								\
		_listeners = g_array_new(FALSE, TRUE, sizeof(_type));	\
		if (!(_listeners)) {					\
			BT_LOGE_STR("Failed to allocate one GArray.");	\
		}							\
	} while (0)

#define CALL_REMOVE_LISTENERS(_type, _listeners)			\
	do {								\
		size_t i;						\
									\
		for (i = 0; i < (_listeners)->len; i++) {		\
			_type *listener =				\
				&g_array_index((_listeners), _type, i);	\
									\
			if (listener->base.removed) {			\
				listener->base.removed(listener->base.data); \
			}						\
		}							\
	} while (0)

static
void destroy_graph(struct bt_object *obj)
{
	struct bt_graph *graph = container_of(obj, struct bt_graph, base);

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
	BT_LIB_LOGD("Destroying graph: %!+g", graph);
	obj->ref_count++;

	/*
	 * Cancel the graph to disallow some operations, like creating
	 * notification iterators and adding ports to components.
	 */
	(void) bt_graph_cancel((void *) graph);

	/* Call all remove listeners */
	CALL_REMOVE_LISTENERS(struct bt_graph_listener_port_added,
		graph->listeners.source_output_port_added);
	CALL_REMOVE_LISTENERS(struct bt_graph_listener_port_added,
		graph->listeners.filter_output_port_added);
	CALL_REMOVE_LISTENERS(struct bt_graph_listener_port_added,
		graph->listeners.filter_input_port_added);
	CALL_REMOVE_LISTENERS(struct bt_graph_listener_port_added,
		graph->listeners.sink_input_port_added);
	CALL_REMOVE_LISTENERS(struct bt_graph_listener_port_removed,
		graph->listeners.source_output_port_removed);
	CALL_REMOVE_LISTENERS(struct bt_graph_listener_port_removed,
		graph->listeners.filter_output_port_removed);
	CALL_REMOVE_LISTENERS(struct bt_graph_listener_port_removed,
		graph->listeners.filter_input_port_removed);
	CALL_REMOVE_LISTENERS(struct bt_graph_listener_port_removed,
		graph->listeners.sink_input_port_removed);
	CALL_REMOVE_LISTENERS(struct bt_graph_listener_ports_connected,
		graph->listeners.source_filter_ports_connected);
	CALL_REMOVE_LISTENERS(struct bt_graph_listener_ports_connected,
		graph->listeners.source_sink_ports_connected);
	CALL_REMOVE_LISTENERS(struct bt_graph_listener_ports_connected,
		graph->listeners.filter_sink_ports_connected);
	CALL_REMOVE_LISTENERS(struct bt_graph_listener_ports_disconnected,
		graph->listeners.source_filter_ports_disconnected);
	CALL_REMOVE_LISTENERS(struct bt_graph_listener_ports_disconnected,
		graph->listeners.source_sink_ports_disconnected);
	CALL_REMOVE_LISTENERS(struct bt_graph_listener_ports_disconnected,
		graph->listeners.filter_sink_ports_disconnected);

	if (graph->notifications) {
		g_ptr_array_free(graph->notifications, TRUE);
		graph->notifications = NULL;
	}

	if (graph->connections) {
		BT_LOGD_STR("Destroying connections.");
		g_ptr_array_free(graph->connections, TRUE);
		graph->connections = NULL;
	}

	if (graph->components) {
		BT_LOGD_STR("Destroying components.");
		g_ptr_array_free(graph->components, TRUE);
		graph->components = NULL;
	}

	if (graph->sinks_to_consume) {
		g_queue_free(graph->sinks_to_consume);
		graph->sinks_to_consume = NULL;
	}

	if (graph->listeners.source_output_port_added) {
		g_array_free(graph->listeners.source_output_port_added, TRUE);
		graph->listeners.source_output_port_added = NULL;
	}

	if (graph->listeners.filter_output_port_added) {
		g_array_free(graph->listeners.filter_output_port_added, TRUE);
		graph->listeners.filter_output_port_added = NULL;
	}

	if (graph->listeners.filter_input_port_added) {
		g_array_free(graph->listeners.filter_input_port_added, TRUE);
		graph->listeners.filter_input_port_added = NULL;
	}

	if (graph->listeners.sink_input_port_added) {
		g_array_free(graph->listeners.sink_input_port_added, TRUE);
		graph->listeners.sink_input_port_added = NULL;
	}

	if (graph->listeners.source_output_port_removed) {
		g_array_free(graph->listeners.source_output_port_removed, TRUE);
		graph->listeners.source_output_port_removed = NULL;
	}

	if (graph->listeners.filter_output_port_removed) {
		g_array_free(graph->listeners.filter_output_port_removed, TRUE);
		graph->listeners.filter_output_port_removed = NULL;
	}

	if (graph->listeners.filter_input_port_removed) {
		g_array_free(graph->listeners.filter_input_port_removed, TRUE);
		graph->listeners.filter_input_port_removed = NULL;
	}

	if (graph->listeners.sink_input_port_removed) {
		g_array_free(graph->listeners.sink_input_port_removed, TRUE);
		graph->listeners.sink_input_port_removed = NULL;
	}

	if (graph->listeners.source_filter_ports_connected) {
		g_array_free(graph->listeners.source_filter_ports_connected,
			TRUE);
		graph->listeners.source_filter_ports_connected = NULL;
	}

	if (graph->listeners.source_sink_ports_connected) {
		g_array_free(graph->listeners.source_sink_ports_connected,
			TRUE);
		graph->listeners.source_sink_ports_connected = NULL;
	}

	if (graph->listeners.filter_sink_ports_connected) {
		g_array_free(graph->listeners.filter_sink_ports_connected,
			TRUE);
		graph->listeners.filter_sink_ports_connected = NULL;
	}

	if (graph->listeners.source_filter_ports_disconnected) {
		g_array_free(graph->listeners.source_filter_ports_disconnected,
			TRUE);
		graph->listeners.source_filter_ports_disconnected = NULL;
	}

	if (graph->listeners.source_sink_ports_disconnected) {
		g_array_free(graph->listeners.source_sink_ports_disconnected,
			TRUE);
		graph->listeners.source_sink_ports_disconnected = NULL;
	}

	if (graph->listeners.filter_sink_ports_disconnected) {
		g_array_free(graph->listeners.filter_sink_ports_disconnected,
			TRUE);
		graph->listeners.filter_sink_ports_disconnected = NULL;
	}

	bt_object_pool_finalize(&graph->event_notif_pool);
	bt_object_pool_finalize(&graph->packet_begin_notif_pool);
	bt_object_pool_finalize(&graph->packet_end_notif_pool);
	g_free(graph);
}

static
void destroy_notification_event(struct bt_notification *notif,
		struct bt_graph *graph)
{
	bt_notification_event_destroy(notif);
}

static
void destroy_notification_packet_begin(struct bt_notification *notif,
		struct bt_graph *graph)
{
	bt_notification_packet_beginning_destroy(notif);
}

static
void destroy_notification_packet_end(struct bt_notification *notif,
		struct bt_graph *graph)
{
	bt_notification_packet_end_destroy(notif);
}

static
void notify_notification_graph_is_destroyed(struct bt_notification *notif)
{
	bt_notification_unlink_graph(notif);
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

	bt_object_init_shared(&graph->base, destroy_graph);
	graph->connections = g_ptr_array_new_with_free_func(
		(GDestroyNotify) bt_object_try_spec_release);
	if (!graph->connections) {
		BT_LOGE_STR("Failed to allocate one GPtrArray.");
		goto error;
	}
	graph->components = g_ptr_array_new_with_free_func(
		(GDestroyNotify) bt_object_try_spec_release);
	if (!graph->components) {
		BT_LOGE_STR("Failed to allocate one GPtrArray.");
		goto error;
	}
	graph->sinks_to_consume = g_queue_new();
	if (!graph->sinks_to_consume) {
		BT_LOGE_STR("Failed to allocate one GQueue.");
		goto error;
	}

	bt_graph_set_can_consume(graph, true);
	INIT_LISTENERS_ARRAY(struct bt_graph_listener_port_added,
		graph->listeners.source_output_port_added);

	if (!graph->listeners.source_output_port_added) {
		ret = -1;
		goto error;
	}

	INIT_LISTENERS_ARRAY(struct bt_graph_listener_port_added,
		graph->listeners.filter_output_port_added);

	if (!graph->listeners.filter_output_port_added) {
		ret = -1;
		goto error;
	}

	INIT_LISTENERS_ARRAY(struct bt_graph_listener_port_added,
		graph->listeners.filter_input_port_added);

	if (!graph->listeners.filter_input_port_added) {
		ret = -1;
		goto error;
	}

	INIT_LISTENERS_ARRAY(struct bt_graph_listener_port_added,
		graph->listeners.sink_input_port_added);

	if (!graph->listeners.sink_input_port_added) {
		ret = -1;
		goto error;
	}

	INIT_LISTENERS_ARRAY(struct bt_graph_listener_port_removed,
		graph->listeners.source_output_port_removed);

	if (!graph->listeners.source_output_port_removed) {
		ret = -1;
		goto error;
	}

	INIT_LISTENERS_ARRAY(struct bt_graph_listener_port_removed,
		graph->listeners.filter_output_port_removed);

	if (!graph->listeners.filter_output_port_removed) {
		ret = -1;
		goto error;
	}

	INIT_LISTENERS_ARRAY(struct bt_graph_listener_port_removed,
		graph->listeners.filter_input_port_removed);

	if (!graph->listeners.filter_input_port_removed) {
		ret = -1;
		goto error;
	}

	INIT_LISTENERS_ARRAY(struct bt_graph_listener_port_removed,
		graph->listeners.sink_input_port_removed);

	if (!graph->listeners.sink_input_port_removed) {
		ret = -1;
		goto error;
	}

	INIT_LISTENERS_ARRAY(struct bt_graph_listener_ports_connected,
		graph->listeners.source_filter_ports_connected);

	if (!graph->listeners.source_filter_ports_connected) {
		ret = -1;
		goto error;
	}

	INIT_LISTENERS_ARRAY(struct bt_graph_listener_ports_connected,
		graph->listeners.source_sink_ports_connected);

	if (!graph->listeners.source_sink_ports_connected) {
		ret = -1;
		goto error;
	}

	INIT_LISTENERS_ARRAY(struct bt_graph_listener_ports_connected,
		graph->listeners.filter_sink_ports_connected);

	if (!graph->listeners.filter_sink_ports_connected) {
		ret = -1;
		goto error;
	}

	INIT_LISTENERS_ARRAY(struct bt_graph_listener_ports_disconnected,
		graph->listeners.source_filter_ports_disconnected);

	if (!graph->listeners.source_filter_ports_disconnected) {
		ret = -1;
		goto error;
	}

	INIT_LISTENERS_ARRAY(struct bt_graph_listener_ports_disconnected,
		graph->listeners.source_sink_ports_disconnected);

	if (!graph->listeners.source_sink_ports_disconnected) {
		ret = -1;
		goto error;
	}

	INIT_LISTENERS_ARRAY(struct bt_graph_listener_ports_disconnected,
		graph->listeners.filter_sink_ports_disconnected);

	if (!graph->listeners.filter_sink_ports_disconnected) {
		ret = -1;
		goto error;
	}

	ret = bt_object_pool_initialize(&graph->event_notif_pool,
		(bt_object_pool_new_object_func) bt_notification_event_new,
		(bt_object_pool_destroy_object_func) destroy_notification_event,
		graph);
	if (ret) {
		BT_LOGE("Failed to initialize event notification pool: ret=%d",
			ret);
		goto error;
	}

	ret = bt_object_pool_initialize(&graph->packet_begin_notif_pool,
		(bt_object_pool_new_object_func) bt_notification_packet_beginning_new,
		(bt_object_pool_destroy_object_func) destroy_notification_packet_begin,
		graph);
	if (ret) {
		BT_LOGE("Failed to initialize packet beginning notification pool: ret=%d",
			ret);
		goto error;
	}

	ret = bt_object_pool_initialize(&graph->packet_end_notif_pool,
		(bt_object_pool_new_object_func) bt_notification_packet_end_new,
		(bt_object_pool_destroy_object_func) destroy_notification_packet_end,
		graph);
	if (ret) {
		BT_LOGE("Failed to initialize packet end notification pool: ret=%d",
			ret);
		goto error;
	}

	graph->notifications = g_ptr_array_new_with_free_func(
		(GDestroyNotify) notify_notification_graph_is_destroyed);
	BT_LIB_LOGD("Created graph object: %!+g", graph);

end:
	return (void *) graph;

error:
	BT_OBJECT_PUT_REF_AND_RESET(graph);
	goto end;
}

enum bt_graph_status bt_graph_connect_ports(
		struct bt_graph *graph,
		const struct bt_port_output *upstream_port_out,
		const struct bt_port_input *downstream_port_in,
		const struct bt_connection **user_connection)
{
	enum bt_graph_status status = BT_GRAPH_STATUS_OK;
	struct bt_connection *connection = NULL;
	struct bt_port *upstream_port = (void *) upstream_port_out;
	struct bt_port *downstream_port = (void *) downstream_port_in;
	struct bt_component *upstream_component = NULL;
	struct bt_component *downstream_component = NULL;
	enum bt_self_component_status component_status;
	bool init_can_consume;

	BT_ASSERT_PRE_NON_NULL(graph, "Graph");
	BT_ASSERT_PRE_NON_NULL(upstream_port, "Upstream port");
	BT_ASSERT_PRE_NON_NULL(downstream_port, "Downstream port port");
	BT_ASSERT_PRE(!graph->canceled, "Graph is canceled: %!+g", graph);
	BT_ASSERT_PRE(!bt_port_is_connected(upstream_port),
		"Upstream port is already connected: %!+p", upstream_port);
	BT_ASSERT_PRE(!bt_port_is_connected(downstream_port),
		"Downstream port is already connected: %!+p", downstream_port);
	BT_ASSERT_PRE(bt_port_borrow_component_inline((void *) upstream_port),
		"Upstream port does not belong to a component: %!+p",
		upstream_port);
	BT_ASSERT_PRE(bt_port_borrow_component_inline((void *) downstream_port),
		"Downstream port does not belong to a component: %!+p",
		downstream_port);
	init_can_consume = graph->can_consume;
	BT_LIB_LOGD("Connecting component ports within graph: "
		"%![graph-]+g, %![up-port-]+p, %![down-port-]+p",
		graph, upstream_port, downstream_port);
	bt_graph_set_can_consume(graph, false);
	upstream_component = bt_port_borrow_component_inline(
		(void *) upstream_port);
	downstream_component = bt_port_borrow_component_inline(
		(void *) downstream_port);

	/*
	 * At this point the ports are not connected yet. Both
	 * components need to accept an eventual connection to their
	 * port by the other port before we continue.
	 */
	BT_LIB_LOGD("Asking upstream component to accept the connection: "
		"%![comp-]+c", upstream_component);
	component_status = bt_component_accept_port_connection(
		upstream_component, (void *) upstream_port,
		(void *) downstream_port);
	if (component_status != BT_SELF_COMPONENT_STATUS_OK) {
		if (component_status == BT_SELF_COMPONENT_STATUS_REFUSE_PORT_CONNECTION) {
			BT_LOGD_STR("Upstream component refused the connection.");
		} else {
			BT_LOGW("Cannot ask upstream component to accept the connection: "
				"status=%s", bt_self_component_status_string(component_status));
		}

		status = (int) component_status;
		goto end;
	}

	BT_LIB_LOGD("Asking downstream component to accept the connection: "
		"%![comp-]+c", downstream_component);
	component_status = bt_component_accept_port_connection(
		downstream_component, (void *) downstream_port,
		(void *) upstream_port);
	if (component_status != BT_SELF_COMPONENT_STATUS_OK) {
		if (component_status == BT_SELF_COMPONENT_STATUS_REFUSE_PORT_CONNECTION) {
			BT_LOGD_STR("Downstream component refused the connection.");
		} else {
			BT_LOGW("Cannot ask downstream component to accept the connection: "
				"status=%s", bt_self_component_status_string(component_status));
		}

		status = (int) component_status;
		goto end;
	}

	BT_LOGD_STR("Creating connection.");
	connection = bt_connection_create(graph, (void *) upstream_port,
		(void *) downstream_port);
	if (!connection) {
		BT_LOGW("Cannot create connection object.");
		status = BT_GRAPH_STATUS_NOMEM;
		goto end;
	}

	BT_LIB_LOGD("Connection object created: %!+x", connection);

	/*
	 * Ownership of upstream_component/downstream_component and of
	 * the connection object is transferred to the graph.
	 */
	g_ptr_array_add(graph->connections, connection);

	/*
	 * Notify both components that their port is connected.
	 */
	BT_LIB_LOGD("Notifying upstream component that its port is connected: "
		"%![comp-]+c, %![port-]+p", upstream_component, upstream_port);
	component_status = bt_component_port_connected(upstream_component,
		(void *) upstream_port, (void *) downstream_port);
	if (component_status != BT_SELF_COMPONENT_STATUS_OK) {
		BT_LIB_LOGW("Error while notifying upstream component that its port is connected: "
			"status=%s, %![graph-]+g, %![up-comp-]+c, "
			"%![down-comp-]+c, %![up-port-]+p, %![down-port-]+p",
			bt_self_component_status_string(component_status),
			graph, upstream_component, downstream_component,
			upstream_port, downstream_port);
		bt_connection_end(connection, true);
		status = (int) component_status;
		goto end;
	}

	connection->notified_upstream_port_connected = true;
	BT_LIB_LOGD("Notifying downstream component that its port is connected: "
		"%![comp-]+c, %![port-]+p", downstream_component,
		downstream_port);
	component_status = bt_component_port_connected(downstream_component,
		(void *) downstream_port, (void *) upstream_port);
	if (component_status != BT_SELF_COMPONENT_STATUS_OK) {
		BT_LIB_LOGW("Error while notifying downstream component that its port is connected: "
			"status=%s, %![graph-]+g, %![up-comp-]+c, "
			"%![down-comp-]+c, %![up-port-]+p, %![down-port-]+p",
			bt_self_component_status_string(component_status),
			graph, upstream_component, downstream_component,
			upstream_port, downstream_port);
		bt_connection_end(connection, true);
		status = (int) component_status;
		goto end;
	}

	connection->notified_downstream_port_connected = true;

	/*
	 * Notify the graph's creator that both ports are connected.
	 */
	BT_LOGD_STR("Notifying graph's user that new component ports are connected.");
	bt_graph_notify_ports_connected(graph, upstream_port, downstream_port);
	connection->notified_graph_ports_connected = true;
	BT_LIB_LOGD("Connected component ports within graph: "
		"%![graph-]+g, %![up-comp-]+c, %![down-comp-]+c, "
		"%![up-port-]+p, %![down-port-]+p",
		graph, upstream_component, downstream_component,
		upstream_port, downstream_port);

	if (user_connection) {
		/* Move reference to user */
		*user_connection = connection;
		connection = NULL;
	}

end:
	bt_object_put_ref(connection);
	(void) init_can_consume;
	bt_graph_set_can_consume(graph, init_can_consume);
	return status;
}

static inline
enum bt_graph_status consume_graph_sink(struct bt_component_sink *comp)
{
	enum bt_self_component_status comp_status;
	struct bt_component_class_sink *sink_class = NULL;

	BT_ASSERT(comp);
	sink_class = (void *) comp->parent.class;
	BT_ASSERT(sink_class->methods.consume);
	BT_LIB_LOGD("Calling user's consume method: %!+c", comp);
	comp_status = sink_class->methods.consume((void *) comp);
	BT_LOGD("User method returned: status=%s",
		bt_self_component_status_string(comp_status));
	BT_ASSERT_PRE(comp_status == BT_SELF_COMPONENT_STATUS_OK ||
		comp_status == BT_SELF_COMPONENT_STATUS_END ||
		comp_status == BT_SELF_COMPONENT_STATUS_AGAIN ||
		comp_status == BT_SELF_COMPONENT_STATUS_ERROR ||
		comp_status == BT_SELF_COMPONENT_STATUS_NOMEM,
		"Invalid component status returned by consuming method: "
		"status=%s", bt_self_component_status_string(comp_status));
	if (comp_status < 0) {
		BT_LOGW_STR("Consume method failed.");
		goto end;
	}

	BT_LIB_LOGV("Consumed from sink: %![comp-]+c, status=%s",
		comp, bt_self_component_status_string(comp_status));

end:
	return (int) comp_status;
}

/*
 * `node` is removed from the queue of sinks to consume when passed to
 * this function. This function adds it back to the queue if there's
 * still something to consume afterwards.
 */
static inline
enum bt_graph_status consume_sink_node(struct bt_graph *graph, GList *node)
{
	enum bt_graph_status status;
	struct bt_component_sink *sink;

	sink = node->data;
	status = consume_graph_sink(sink);
	if (unlikely(status != BT_GRAPH_STATUS_END)) {
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
	BT_LIB_LOGV("Consumed sink node: %![comp-]+c, status=%s",
		sink, bt_graph_status_string(status));
	return status;
}

BT_HIDDEN
enum bt_graph_status bt_graph_consume_sink_no_check(struct bt_graph *graph,
		struct bt_component_sink *sink)
{
	enum bt_graph_status status;
	GList *sink_node;
	int index;

	BT_LIB_LOGV("Making specific sink consume: %![comp-]+c", sink);
	BT_ASSERT(bt_component_borrow_graph((void *) sink) == graph);

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

static inline
enum bt_graph_status consume_no_check(struct bt_graph *graph)
{
	enum bt_graph_status status = BT_GRAPH_STATUS_OK;
	struct bt_component *sink;
	GList *current_node;

	BT_ASSERT_PRE(graph->has_sink,
		"Graph has no sink component: %!+g", graph);
	BT_LIB_LOGV("Making next sink consume: %![graph-]+g", graph);

	if (unlikely(g_queue_is_empty(graph->sinks_to_consume))) {
		BT_LOGV_STR("Graph's sink queue is empty: end of graph.");
		status = BT_GRAPH_STATUS_END;
		goto end;
	}

	current_node = g_queue_pop_head_link(graph->sinks_to_consume);
	sink = current_node->data;
	BT_LIB_LOGV("Chose next sink to consume: %!+c", sink);
	status = consume_sink_node(graph, current_node);

end:
	return status;
}

enum bt_graph_status bt_graph_consume(
		struct bt_graph *graph)
{
	enum bt_graph_status status;

	BT_ASSERT_PRE_NON_NULL(graph, "Graph");
	BT_ASSERT_PRE(!graph->canceled, "Graph is canceled: %!+g", graph);
	BT_ASSERT_PRE(graph->can_consume,
		"Cannot consume graph in its current state: %!+g", graph);
	bt_graph_set_can_consume(graph, BT_FALSE);
	status = consume_no_check(graph);
	bt_graph_set_can_consume(graph, BT_TRUE);
	return status;
}

enum bt_graph_status bt_graph_run(struct bt_graph *graph)
{
	enum bt_graph_status status = BT_GRAPH_STATUS_OK;

	BT_ASSERT_PRE_NON_NULL(graph, "Graph");
	BT_ASSERT_PRE(!graph->canceled, "Graph is canceled: %!+g", graph);
	BT_ASSERT_PRE(graph->can_consume,
		"Cannot consume graph in its current state: %!+g", graph);
	bt_graph_set_can_consume(graph, BT_FALSE);
	BT_LIB_LOGV("Running graph: %!+g", graph);

	do {
		/*
		 * Check if the graph is canceled at each iteration. If
		 * the graph was canceled by another thread or by a
		 * signal handler, this is not a warning nor an error,
		 * it was intentional: log with a DEBUG level only.
		 */
		if (unlikely(graph->canceled)) {
			BT_LIB_LOGD("Stopping the graph: graph is canceled: "
				"%!+g", graph);
			status = BT_GRAPH_STATUS_CANCELED;
			goto end;
		}

		status = consume_no_check(graph);
		if (unlikely(status == BT_GRAPH_STATUS_AGAIN)) {
			/*
			 * If AGAIN is received and there are multiple
			 * sinks, go ahead and consume from the next
			 * sink.
			 *
			 * However, in the case where a single sink is
			 * left, the caller can decide to busy-wait and
			 * call bt_graph_run() continuously
			 * until the source is ready or it can decide to
			 * sleep for an arbitrary amount of time.
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
	BT_LIB_LOGV("Graph ran: %![graph-]+g, status=%s", graph,
		bt_graph_status_string(status));
	bt_graph_set_can_consume(graph, BT_TRUE);
	return status;
}

enum bt_graph_status
bt_graph_add_source_component_output_port_added_listener(
		struct bt_graph *graph,
		bt_graph_source_component_output_port_added_listener_func func,
		bt_graph_listener_removed_func listener_removed, void *data,
		int *out_listener_id)
{
	struct bt_graph_listener_port_added listener = {
		.base = {
			.removed = listener_removed,
			.data = data,
		},
		.func = (port_added_func_t) func,
	};
	int listener_id;

	BT_ASSERT_PRE_NON_NULL(graph, "Graph");
	BT_ASSERT_PRE_NON_NULL(func, "Listener");
	BT_ASSERT_PRE_NON_NULL(func, "\"Listener removed\" listener");
	BT_ASSERT_PRE(!graph->in_remove_listener,
		"Graph currently executing a \"listener removed\" listener: "
		"%!+g", graph);
	g_array_append_val(graph->listeners.source_output_port_added, listener);
	listener_id = graph->listeners.source_output_port_added->len - 1;
	BT_LIB_LOGV("Added \"source component output port added\" listener to graph: "
		"%![graph-]+g, listener-addr=%p, id=%d", graph, listener,
		listener_id);

	if (listener_id) {
		*out_listener_id = listener_id;
	}

	return BT_GRAPH_STATUS_OK;
}

enum bt_graph_status
bt_graph_add_filter_component_output_port_added_listener(
		struct bt_graph *graph,
		bt_graph_filter_component_output_port_added_listener_func func,
		bt_graph_listener_removed_func listener_removed, void *data,
		int *out_listener_id)
{
	struct bt_graph_listener_port_added listener = {
		.base = {
			.removed = listener_removed,
			.data = data,
		},
		.func = (port_added_func_t) func,
	};
	int listener_id;

	BT_ASSERT_PRE_NON_NULL(graph, "Graph");
	BT_ASSERT_PRE_NON_NULL(func, "Listener");
	BT_ASSERT_PRE_NON_NULL(func, "\"Listener removed\" listener");
	BT_ASSERT_PRE(!graph->in_remove_listener,
		"Graph currently executing a \"listener removed\" listener: "
		"%!+g", graph);
	g_array_append_val(graph->listeners.filter_output_port_added, listener);
	listener_id = graph->listeners.filter_output_port_added->len - 1;
	BT_LIB_LOGV("Added \"filter component output port added\" listener to graph: "
		"%![graph-]+g, listener-addr=%p, id=%d", graph, listener,
		listener_id);

	if (listener_id) {
		*out_listener_id = listener_id;
	}

	return BT_GRAPH_STATUS_OK;
}

enum bt_graph_status
bt_graph_add_filter_component_input_port_added_listener(
		struct bt_graph *graph,
		bt_graph_filter_component_input_port_added_listener_func func,
		bt_graph_listener_removed_func listener_removed, void *data,
		int *out_listener_id)
{
	struct bt_graph_listener_port_added listener = {
		.base = {
			.removed = listener_removed,
			.data = data,
		},
		.func = (port_added_func_t) func,
	};
	int listener_id;

	BT_ASSERT_PRE_NON_NULL(graph, "Graph");
	BT_ASSERT_PRE_NON_NULL(func, "Listener");
	BT_ASSERT_PRE_NON_NULL(func, "\"Listener removed\" listener");
	BT_ASSERT_PRE(!graph->in_remove_listener,
		"Graph currently executing a \"listener removed\" listener: "
		"%!+g", graph);
	g_array_append_val(graph->listeners.filter_input_port_added, listener);
	listener_id = graph->listeners.filter_input_port_added->len - 1;
	BT_LIB_LOGV("Added \"filter component input port added\" listener to graph: "
		"%![graph-]+g, listener-addr=%p, id=%d", graph, listener,
		listener_id);

	if (listener_id) {
		*out_listener_id = listener_id;
	}

	return BT_GRAPH_STATUS_OK;
}

enum bt_graph_status
bt_graph_add_sink_component_input_port_added_listener(
		struct bt_graph *graph,
		bt_graph_sink_component_input_port_added_listener_func func,
		bt_graph_listener_removed_func listener_removed, void *data,
		int *out_listener_id)
{
	struct bt_graph_listener_port_added listener = {
		.base = {
			.removed = listener_removed,
			.data = data,
		},
		.func = (port_added_func_t) func,
	};
	int listener_id;

	BT_ASSERT_PRE_NON_NULL(graph, "Graph");
	BT_ASSERT_PRE_NON_NULL(func, "Listener");
	BT_ASSERT_PRE_NON_NULL(func, "\"Listener removed\" listener");
	BT_ASSERT_PRE(!graph->in_remove_listener,
		"Graph currently executing a \"listener removed\" listener: "
		"%!+g", graph);
	g_array_append_val(graph->listeners.sink_input_port_added, listener);
	listener_id = graph->listeners.sink_input_port_added->len - 1;
	BT_LIB_LOGV("Added \"sink component input port added\" listener to graph: "
		"%![graph-]+g, listener-addr=%p, id=%d", graph, listener,
		listener_id);

	if (listener_id) {
		*out_listener_id = listener_id;
	}

	return BT_GRAPH_STATUS_OK;
}

enum bt_graph_status
bt_graph_add_source_component_output_port_removed_listener(
		struct bt_graph *graph,
		bt_graph_source_component_output_port_removed_listener_func func,
		bt_graph_listener_removed_func listener_removed, void *data,
		int *out_listener_id)
{
	struct bt_graph_listener_port_removed listener = {
		.base = {
			.removed = listener_removed,
			.data = data,
		},
		.func = (port_removed_func_t) func,
	};
	int listener_id;

	BT_ASSERT_PRE_NON_NULL(graph, "Graph");
	BT_ASSERT_PRE_NON_NULL(func, "Listener");
	BT_ASSERT_PRE_NON_NULL(func, "\"Listener removed\" listener");
	BT_ASSERT_PRE(!graph->in_remove_listener,
		"Graph currently executing a \"listener removed\" listener: "
		"%!+g", graph);
	g_array_append_val(graph->listeners.source_output_port_removed, listener);
	listener_id = graph->listeners.source_output_port_removed->len - 1;
	BT_LIB_LOGV("Added \"source component output port removed\" listener to graph: "
		"%![graph-]+g, listener-addr=%p, id=%d", graph, listener,
		listener_id);

	if (listener_id) {
		*out_listener_id = listener_id;
	}

	return BT_GRAPH_STATUS_OK;
}

enum bt_graph_status
bt_graph_add_filter_component_output_port_removed_listener(
		struct bt_graph *graph,
		bt_graph_filter_component_output_port_removed_listener_func func,
		bt_graph_listener_removed_func listener_removed, void *data,
		int *out_listener_id)
{
	struct bt_graph_listener_port_removed listener = {
		.base = {
			.removed = listener_removed,
			.data = data,
		},
		.func = (port_removed_func_t) func,
	};
	int listener_id;

	BT_ASSERT_PRE_NON_NULL(graph, "Graph");
	BT_ASSERT_PRE_NON_NULL(func, "Listener");
	BT_ASSERT_PRE_NON_NULL(func, "\"Listener removed\" listener");
	BT_ASSERT_PRE(!graph->in_remove_listener,
		"Graph currently executing a \"listener removed\" listener: "
		"%!+g", graph);
	g_array_append_val(graph->listeners.filter_output_port_removed, listener);
	listener_id = graph->listeners.filter_output_port_removed->len - 1;
	BT_LIB_LOGV("Added \"filter component output port removed\" listener to graph: "
		"%![graph-]+g, listener-addr=%p, id=%d", graph, listener,
		listener_id);

	if (listener_id) {
		*out_listener_id = listener_id;
	}

	return BT_GRAPH_STATUS_OK;
}

enum bt_graph_status
bt_graph_add_filter_component_input_port_removed_listener(
		struct bt_graph *graph,
		bt_graph_filter_component_input_port_removed_listener_func func,
		bt_graph_listener_removed_func listener_removed, void *data,
		int *out_listener_id)
{
	struct bt_graph_listener_port_removed listener = {
		.base = {
			.removed = listener_removed,
			.data = data,
		},
		.func = (port_removed_func_t) func,
	};
	int listener_id;

	BT_ASSERT_PRE_NON_NULL(graph, "Graph");
	BT_ASSERT_PRE_NON_NULL(func, "Listener");
	BT_ASSERT_PRE_NON_NULL(func, "\"Listener removed\" listener");
	BT_ASSERT_PRE(!graph->in_remove_listener,
		"Graph currently executing a \"listener removed\" listener: "
		"%!+g", graph);
	g_array_append_val(graph->listeners.filter_input_port_removed, listener);
	listener_id = graph->listeners.filter_input_port_removed->len - 1;
	BT_LIB_LOGV("Added \"filter component input port removed\" listener to graph: "
		"%![graph-]+g, listener-addr=%p, id=%d", graph, listener,
		listener_id);

	if (listener_id) {
		*out_listener_id = listener_id;
	}

	return BT_GRAPH_STATUS_OK;
}

enum bt_graph_status
bt_graph_add_sink_component_input_port_removed_listener(
		struct bt_graph *graph,
		bt_graph_sink_component_input_port_removed_listener_func func,
		bt_graph_listener_removed_func listener_removed, void *data,
		int *out_listener_id)
{
	struct bt_graph_listener_port_removed listener = {
		.base = {
			.removed = listener_removed,
			.data = data,
		},
		.func = (port_removed_func_t) func,
	};
	int listener_id;

	BT_ASSERT_PRE_NON_NULL(graph, "Graph");
	BT_ASSERT_PRE_NON_NULL(func, "Listener");
	BT_ASSERT_PRE_NON_NULL(func, "\"Listener removed\" listener");
	BT_ASSERT_PRE(!graph->in_remove_listener,
		"Graph currently executing a \"listener removed\" listener: "
		"%!+g", graph);
	g_array_append_val(graph->listeners.sink_input_port_removed, listener);
	listener_id = graph->listeners.sink_input_port_removed->len - 1;
	BT_LIB_LOGV("Added \"sink component input port removed\" listener to graph: "
		"%![graph-]+g, listener-addr=%p, id=%d", graph, listener,
		listener_id);

	if (listener_id) {
		*out_listener_id = listener_id;
	}

	return BT_GRAPH_STATUS_OK;
}

enum bt_graph_status
bt_graph_add_source_filter_component_ports_connected_listener(
		struct bt_graph *graph,
		bt_graph_source_filter_component_ports_connected_listener_func func,
		bt_graph_listener_removed_func listener_removed, void *data,
		int *out_listener_id)
{
	struct bt_graph_listener_ports_connected listener = {
		.base = {
			.removed = listener_removed,
			.data = data,
		},
		.func = (ports_connected_func_t) func,
	};
	int listener_id;

	BT_ASSERT_PRE_NON_NULL(graph, "Graph");
	BT_ASSERT_PRE_NON_NULL(func, "Listener");
	BT_ASSERT_PRE_NON_NULL(func, "\"Listener removed\" listener");
	BT_ASSERT_PRE(!graph->in_remove_listener,
		"Graph currently executing a \"listener removed\" listener: "
		"%!+g", graph);
	g_array_append_val(graph->listeners.source_filter_ports_connected,
		listener);
	listener_id = graph->listeners.source_filter_ports_connected->len - 1;
	BT_LIB_LOGV("Added \"source to filter component ports connected\" listener to graph: "
		"%![graph-]+g, listener-addr=%p, id=%d", graph, listener,
		listener_id);

	if (listener_id) {
		*out_listener_id = listener_id;
	}

	return BT_GRAPH_STATUS_OK;
}

enum bt_graph_status
bt_graph_add_source_sink_component_ports_connected_listener(
		struct bt_graph *graph,
		bt_graph_source_sink_component_ports_connected_listener_func func,
		bt_graph_listener_removed_func listener_removed, void *data,
		int *out_listener_id)
{
	struct bt_graph_listener_ports_connected listener = {
		.base = {
			.removed = listener_removed,
			.data = data,
		},
		.func = (ports_connected_func_t) func,
	};
	int listener_id;

	BT_ASSERT_PRE_NON_NULL(graph, "Graph");
	BT_ASSERT_PRE_NON_NULL(func, "Listener");
	BT_ASSERT_PRE_NON_NULL(func, "\"Listener removed\" listener");
	BT_ASSERT_PRE(!graph->in_remove_listener,
		"Graph currently executing a \"listener removed\" listener: "
		"%!+g", graph);
	g_array_append_val(graph->listeners.source_sink_ports_connected,
		listener);
	listener_id = graph->listeners.source_sink_ports_connected->len - 1;
	BT_LIB_LOGV("Added \"source to sink component ports connected\" listener to graph: "
		"%![graph-]+g, listener-addr=%p, id=%d", graph, listener,
		listener_id);

	if (listener_id) {
		*out_listener_id = listener_id;
	}

	return BT_GRAPH_STATUS_OK;
}

enum bt_graph_status
bt_graph_add_filter_sink_component_ports_connected_listener(
		struct bt_graph *graph,
		bt_graph_filter_sink_component_ports_connected_listener_func func,
		bt_graph_listener_removed_func listener_removed, void *data,
		int *out_listener_id)
{
	struct bt_graph_listener_ports_connected listener = {
		.base = {
			.removed = listener_removed,
			.data = data,
		},
		.func = (ports_connected_func_t) func,
	};
	int listener_id;

	BT_ASSERT_PRE_NON_NULL(graph, "Graph");
	BT_ASSERT_PRE_NON_NULL(func, "Listener");
	BT_ASSERT_PRE_NON_NULL(func, "\"Listener removed\" listener");
	BT_ASSERT_PRE(!graph->in_remove_listener,
		"Graph currently executing a \"listener removed\" listener: "
		"%!+g", graph);
	g_array_append_val(graph->listeners.filter_sink_ports_connected,
		listener);
	listener_id = graph->listeners.filter_sink_ports_connected->len - 1;
	BT_LIB_LOGV("Added \"filter to sink component ports connected\" listener to graph: "
		"%![graph-]+g, listener-addr=%p, id=%d", graph, listener,
		listener_id);

	if (listener_id) {
		*out_listener_id = listener_id;
	}

	return BT_GRAPH_STATUS_OK;
}

enum bt_graph_status
bt_graph_add_source_filter_component_ports_disconnected_listener(
		struct bt_graph *graph,
		bt_graph_source_filter_component_ports_disconnected_listener_func func,
		bt_graph_listener_removed_func listener_removed, void *data,
		int *out_listener_id)
{
	struct bt_graph_listener_ports_disconnected listener = {
		.base = {
			.removed = listener_removed,
			.data = data,
		},
		.func = (ports_disconnected_func_t) func,
	};
	int listener_id;

	BT_ASSERT_PRE_NON_NULL(graph, "Graph");
	BT_ASSERT_PRE_NON_NULL(func, "Listener");
	BT_ASSERT_PRE_NON_NULL(func, "\"Listener removed\" listener");
	BT_ASSERT_PRE(!graph->in_remove_listener,
		"Graph currently executing a \"listener removed\" listener: "
		"%!+g", graph);
	g_array_append_val(graph->listeners.source_filter_ports_disconnected,
		listener);
	listener_id = graph->listeners.source_filter_ports_disconnected->len - 1;
	BT_LIB_LOGV("Added \"source to filter component ports disconnected\" listener to graph: "
		"%![graph-]+g, listener-addr=%p, id=%d", graph, listener,
		listener_id);

	if (listener_id) {
		*out_listener_id = listener_id;
	}

	return BT_GRAPH_STATUS_OK;
}

enum bt_graph_status
bt_graph_add_source_sink_component_ports_disconnected_listener(
		struct bt_graph *graph,
		bt_graph_source_sink_component_ports_disconnected_listener_func func,
		bt_graph_listener_removed_func listener_removed, void *data,
		int *out_listener_id)
{
	struct bt_graph_listener_ports_disconnected listener = {
		.base = {
			.removed = listener_removed,
			.data = data,
		},
		.func = (ports_disconnected_func_t) func,
	};
	int listener_id;

	BT_ASSERT_PRE_NON_NULL(graph, "Graph");
	BT_ASSERT_PRE_NON_NULL(func, "Listener");
	BT_ASSERT_PRE_NON_NULL(func, "\"Listener removed\" listener");
	BT_ASSERT_PRE(!graph->in_remove_listener,
		"Graph currently executing a \"listener removed\" listener: "
		"%!+g", graph);
	g_array_append_val(graph->listeners.source_sink_ports_disconnected,
		listener);
	listener_id = graph->listeners.source_sink_ports_disconnected->len - 1;
	BT_LIB_LOGV("Added \"source to sink component ports disconnected\" listener to graph: "
		"%![graph-]+g, listener-addr=%p, id=%d", graph, listener,
		listener_id);

	if (listener_id) {
		*out_listener_id = listener_id;
	}

	return BT_GRAPH_STATUS_OK;
}

enum bt_graph_status
bt_graph_add_filter_sink_component_ports_disconnected_listener(
		struct bt_graph *graph,
		bt_graph_filter_sink_component_ports_disconnected_listener_func func,
		bt_graph_listener_removed_func listener_removed, void *data,
		int *out_listener_id)
{
	struct bt_graph_listener_ports_disconnected listener = {
		.base = {
			.removed = listener_removed,
			.data = data,
		},
		.func = (ports_disconnected_func_t) func,
	};
	int listener_id;

	BT_ASSERT_PRE_NON_NULL(graph, "Graph");
	BT_ASSERT_PRE_NON_NULL(func, "Listener");
	BT_ASSERT_PRE_NON_NULL(func, "\"Listener removed\" listener");
	BT_ASSERT_PRE(!graph->in_remove_listener,
		"Graph currently executing a \"listener removed\" listener: "
		"%!+g", graph);
	g_array_append_val(graph->listeners.filter_sink_ports_disconnected,
		listener);
	listener_id = graph->listeners.filter_sink_ports_disconnected->len - 1;
	BT_LIB_LOGV("Added \"filter to sink component ports disconnected\" listener to graph: "
		"%![graph-]+g, listener-addr=%p, id=%d", graph, listener,
		listener_id);

	if (listener_id) {
		*out_listener_id = listener_id;
	}

	return BT_GRAPH_STATUS_OK;
}

BT_HIDDEN
void bt_graph_notify_port_added(struct bt_graph *graph, struct bt_port *port)
{
	uint64_t i;
	GArray *listeners;
	struct bt_component *comp;

	BT_ASSERT(graph);
	BT_ASSERT(port);
	BT_LIB_LOGV("Notifying graph listeners that a port was added: "
		"%![graph-]+g, %![port-]+p", graph, port);
	comp = bt_port_borrow_component_inline(port);
	BT_ASSERT(comp);

	switch (comp->class->type) {
	case BT_COMPONENT_CLASS_TYPE_SOURCE:
	{
		switch (port->type) {
		case BT_PORT_TYPE_OUTPUT:
			listeners = graph->listeners.source_output_port_added;
			break;
		default:
			abort();
		}

		break;
	}
	case BT_COMPONENT_CLASS_TYPE_FILTER:
	{
		switch (port->type) {
		case BT_PORT_TYPE_INPUT:
			listeners = graph->listeners.filter_input_port_added;
			break;
		case BT_PORT_TYPE_OUTPUT:
			listeners = graph->listeners.filter_output_port_added;
			break;
		default:
			abort();
		}

		break;
	}
	case BT_COMPONENT_CLASS_TYPE_SINK:
	{
		switch (port->type) {
		case BT_PORT_TYPE_INPUT:
			listeners = graph->listeners.sink_input_port_added;
			break;
		default:
			abort();
		}

		break;
	}
	default:
		abort();
	}

	for (i = 0; i < listeners->len; i++) {
		struct bt_graph_listener_port_added *listener =
			&g_array_index(listeners,
				struct bt_graph_listener_port_added, i);

		BT_ASSERT(listener->func);
		listener->func(comp, port, listener->base.data);
	}
}

BT_HIDDEN
void bt_graph_notify_port_removed(struct bt_graph *graph,
		struct bt_component *comp, struct bt_port *port)
{
	uint64_t i;
	GArray *listeners;

	BT_ASSERT(graph);
	BT_ASSERT(port);
	BT_LIB_LOGV("Notifying graph listeners that a port was removed: "
		"%![graph-]+g, %![comp-]+c, %![port-]+p", graph, comp, port);

	switch (comp->class->type) {
	case BT_COMPONENT_CLASS_TYPE_SOURCE:
	{
		switch (port->type) {
		case BT_PORT_TYPE_OUTPUT:
			listeners = graph->listeners.source_output_port_removed;
			break;
		default:
			abort();
		}

		break;
	}
	case BT_COMPONENT_CLASS_TYPE_FILTER:
	{
		switch (port->type) {
		case BT_PORT_TYPE_INPUT:
			listeners = graph->listeners.filter_input_port_removed;
			break;
		case BT_PORT_TYPE_OUTPUT:
			listeners = graph->listeners.filter_output_port_removed;
			break;
		default:
			abort();
		}

		break;
	}
	case BT_COMPONENT_CLASS_TYPE_SINK:
	{
		switch (port->type) {
		case BT_PORT_TYPE_INPUT:
			listeners = graph->listeners.sink_input_port_removed;
			break;
		default:
			abort();
		}

		break;
	}
	default:
		abort();
	}

	for (i = 0; i < listeners->len; i++) {
		struct bt_graph_listener_port_removed *listener =
			&g_array_index(listeners,
				struct bt_graph_listener_port_removed, i);

		BT_ASSERT(listener->func);
		listener->func(comp, port, listener->base.data);
	}
}

BT_HIDDEN
void bt_graph_notify_ports_connected(struct bt_graph *graph,
		struct bt_port *upstream_port, struct bt_port *downstream_port)
{
	uint64_t i;
	GArray *listeners;
	struct bt_component *upstream_comp;
	struct bt_component *downstream_comp;

	BT_ASSERT(graph);
	BT_ASSERT(upstream_port);
	BT_ASSERT(downstream_port);
	BT_LIB_LOGV("Notifying graph listeners that ports were connected: "
		"%![graph-]+g, %![up-port-]+p, %![down-port-]+p",
		graph, upstream_port, downstream_port);
	upstream_comp = bt_port_borrow_component_inline(upstream_port);
	BT_ASSERT(upstream_comp);
	downstream_comp = bt_port_borrow_component_inline(downstream_port);
	BT_ASSERT(downstream_comp);

	switch (upstream_comp->class->type) {
	case BT_COMPONENT_CLASS_TYPE_SOURCE:
	{
		switch (downstream_comp->class->type) {
		case BT_COMPONENT_CLASS_TYPE_FILTER:
			listeners =
				graph->listeners.source_filter_ports_connected;
			break;
		case BT_COMPONENT_CLASS_TYPE_SINK:
			listeners =
				graph->listeners.source_sink_ports_connected;
			break;
		default:
			abort();
		}

		break;
	}
	case BT_COMPONENT_CLASS_TYPE_FILTER:
	{
		switch (downstream_comp->class->type) {
		case BT_COMPONENT_CLASS_TYPE_SINK:
			listeners =
				graph->listeners.filter_sink_ports_connected;
			break;
		default:
			abort();
		}

		break;
	}
	default:
		abort();
	}

	for (i = 0; i < listeners->len; i++) {
		struct bt_graph_listener_ports_connected *listener =
			&g_array_index(listeners,
				struct bt_graph_listener_ports_connected, i);

		BT_ASSERT(listener->func);
		listener->func(upstream_comp, downstream_comp,
			upstream_port, downstream_port, listener->base.data);
	}
}

BT_HIDDEN
void bt_graph_notify_ports_disconnected(struct bt_graph *graph,
		struct bt_component *upstream_comp,
		struct bt_component *downstream_comp,
		struct bt_port *upstream_port,
		struct bt_port *downstream_port)
{
	uint64_t i;
	GArray *listeners;

	BT_ASSERT(graph);
	BT_ASSERT(upstream_comp);
	BT_ASSERT(downstream_comp);
	BT_ASSERT(upstream_port);
	BT_ASSERT(downstream_port);
	BT_LIB_LOGV("Notifying graph listeners that ports were disconnected: "
		"%![graph-]+g, %![up-port-]+p, %![down-port-]+p, "
		"%![up-comp-]+c, %![down-comp-]+c",
		graph, upstream_port, downstream_port, upstream_comp,
		downstream_comp);

	switch (upstream_comp->class->type) {
	case BT_COMPONENT_CLASS_TYPE_SOURCE:
	{
		switch (downstream_comp->class->type) {
		case BT_COMPONENT_CLASS_TYPE_FILTER:
			listeners =
				graph->listeners.source_filter_ports_disconnected;
			break;
		case BT_COMPONENT_CLASS_TYPE_SINK:
			listeners =
				graph->listeners.source_sink_ports_disconnected;
			break;
		default:
			abort();
		}

		break;
	}
	case BT_COMPONENT_CLASS_TYPE_FILTER:
	{
		switch (downstream_comp->class->type) {
		case BT_COMPONENT_CLASS_TYPE_SINK:
			listeners =
				graph->listeners.filter_sink_ports_disconnected;
			break;
		default:
			abort();
		}

		break;
	}
	default:
		abort();
	}

	for (i = 0; i < listeners->len; i++) {
		struct bt_graph_listener_ports_disconnected *listener =
			&g_array_index(listeners,
				struct bt_graph_listener_ports_disconnected, i);

		BT_ASSERT(listener->func);
		listener->func(upstream_comp, downstream_comp,
			upstream_port, downstream_port, listener->base.data);
	}
}

enum bt_graph_status bt_graph_cancel(
		struct bt_graph *graph)
{

	BT_ASSERT_PRE_NON_NULL(graph, "Graph");
	graph->canceled = true;
	BT_LIB_LOGV("Canceled graph: %!+i", graph);
	return BT_GRAPH_STATUS_OK;
}

bt_bool bt_graph_is_canceled(const struct bt_graph *graph)
{
	BT_ASSERT_PRE_NON_NULL(graph, "Graph");
	return graph->canceled ? BT_TRUE : BT_FALSE;
}

BT_HIDDEN
void bt_graph_remove_connection(struct bt_graph *graph,
		struct bt_connection *connection)
{
	BT_ASSERT(graph);
	BT_ASSERT(connection);
	BT_LIB_LOGV("Removing graph's connection: %![graph-]+g, %![conn-]+x",
		graph, connection);
	g_ptr_array_remove(graph->connections, connection);
}

BT_ASSERT_PRE_FUNC
static inline
bool component_name_exists(struct bt_graph *graph, const char *name)
{
	bool exists = false;
	uint64_t i;

	for (i = 0; i < graph->components->len; i++) {
		struct bt_component *other_comp = graph->components->pdata[i];

		if (strcmp(name, bt_component_get_name(other_comp)) == 0) {
			BT_ASSERT_PRE_MSG("Another component with the same name already exists in the graph: "
				"%![other-comp-]+c, name=\"%s\"",
				other_comp, name);
			exists = true;
			goto end;
		}
	}

end:
	return exists;
}

static
enum bt_graph_status add_component_with_init_method_data(
		struct bt_graph *graph,
		struct bt_component_class *comp_cls,
		comp_init_method_t init_method,
		const char *name, const struct bt_value *params,
		void *init_method_data, struct bt_component **user_component)
{
	enum bt_graph_status graph_status = BT_GRAPH_STATUS_OK;
	enum bt_self_component_status comp_status;
	struct bt_component *component = NULL;
	int ret;
	bool init_can_consume;
	struct bt_value *new_params = NULL;

	BT_ASSERT(comp_cls);
	BT_ASSERT_PRE_NON_NULL(graph, "Graph");
	BT_ASSERT_PRE_NON_NULL(name, "Name");
	BT_ASSERT_PRE(!graph->canceled, "Graph is canceled: %!+g", graph);
	BT_ASSERT_PRE(!component_name_exists(graph, name),
		"Duplicate component name: %!+g, name=\"%s\"", graph, name);
	BT_ASSERT_PRE(!params || bt_value_is_map(params),
		"Parameter value is not a map value: %!+v", params);
	init_can_consume = graph->can_consume;
	bt_graph_set_can_consume(graph, false);
	BT_LIB_LOGD("Adding component to graph: "
		"%![graph-]+g, %![cc-]+C, name=\"%s\", %![params-]+v, "
		"init-method-data-addr=%p",
		graph, comp_cls, name, params, init_method_data);

	if (!params) {
		new_params = bt_value_map_create();
		if (!new_params) {
			BT_LOGE_STR("Cannot create map value object.");
			graph_status = BT_GRAPH_STATUS_NOMEM;
			goto end;
		}

		params = new_params;
	}

	ret = bt_component_create(comp_cls, name, &component);
	if (ret) {
		BT_LOGE("Cannot create empty component object: ret=%d",
			ret);
		graph_status = BT_GRAPH_STATUS_NOMEM;
		goto end;
	}

	/*
	 * The user's initialization method needs to see that this
	 * component is part of the graph. If the user method fails, we
	 * immediately remove the component from the graph's components.
	 */
	g_ptr_array_add(graph->components, component);
	bt_component_set_graph(component, graph);

	if (init_method) {
		BT_LOGD_STR("Calling user's initialization method.");
		comp_status = init_method(component, params, init_method_data);
		BT_LOGD("User method returned: status=%s",
			bt_self_component_status_string(comp_status));
		if (comp_status != BT_SELF_COMPONENT_STATUS_OK) {
			BT_LOGW_STR("Initialization method failed.");
			graph_status = (int) comp_status;
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
		graph->has_sink = true;
		g_queue_push_tail(graph->sinks_to_consume, component);
	}

	/*
	 * Freeze the component class now that it's instantiated at
	 * least once.
	 */
	BT_LOGD_STR("Freezing component class.");
	bt_component_class_freeze(comp_cls);
	BT_LIB_LOGD("Added component to graph: "
		"%![graph-]+g, %![cc-]+C, name=\"%s\", %![params-]+v, "
		"init-method-data-addr=%p, %![comp-]+c",
		graph, comp_cls, name, params, init_method_data, component);

	if (user_component) {
		/* Move reference to user */
		*user_component = component;
		component = NULL;
	}

end:
	bt_object_put_ref(component);
	bt_object_put_ref(new_params);
	(void) init_can_consume;
	bt_graph_set_can_consume(graph, init_can_consume);
	return graph_status;
}

enum bt_graph_status
bt_graph_add_source_component_with_init_method_data(
		struct bt_graph *graph,
		const struct bt_component_class_source *comp_cls,
		const char *name, const struct bt_value *params,
		void *init_method_data,
		const struct bt_component_source **component)
{
	BT_ASSERT_PRE_NON_NULL(comp_cls, "Component class");
	return add_component_with_init_method_data(graph,
		(void *) comp_cls, (comp_init_method_t) comp_cls->methods.init,
		name, params, init_method_data, (void *) component);
}

enum bt_graph_status bt_graph_add_source_component(
		struct bt_graph *graph,
		const struct bt_component_class_source *comp_cls,
		const char *name, const struct bt_value *params,
		const struct bt_component_source **component)
{
	return bt_graph_add_source_component_with_init_method_data(
		graph, comp_cls, name, params, NULL, component);
}

enum bt_graph_status
bt_graph_add_filter_component_with_init_method_data(
		struct bt_graph *graph,
		const struct bt_component_class_filter *comp_cls,
		const char *name, const struct bt_value *params,
		void *init_method_data,
		const struct bt_component_filter **component)
{
	BT_ASSERT_PRE_NON_NULL(comp_cls, "Component class");
	return add_component_with_init_method_data(graph,
		(void *) comp_cls, (comp_init_method_t) comp_cls->methods.init,
		name, params, init_method_data, (void *) component);
}

enum bt_graph_status bt_graph_add_filter_component(
		struct bt_graph *graph,
		const struct bt_component_class_filter *comp_cls,
		const char *name, const struct bt_value *params,
		const struct bt_component_filter **component)
{
	return bt_graph_add_filter_component_with_init_method_data(
		graph, comp_cls, name, params, NULL, component);
}

enum bt_graph_status
bt_graph_add_sink_component_with_init_method_data(
		struct bt_graph *graph,
		const struct bt_component_class_sink *comp_cls,
		const char *name, const struct bt_value *params,
		void *init_method_data,
		const struct bt_component_sink **component)
{
	BT_ASSERT_PRE_NON_NULL(comp_cls, "Component class");
	return add_component_with_init_method_data(graph,
		(void *) comp_cls, (comp_init_method_t) comp_cls->methods.init,
		name, params, init_method_data, (void *) component);
}

enum bt_graph_status bt_graph_add_sink_component(
		struct bt_graph *graph,
		const struct bt_component_class_sink *comp_cls,
		const char *name, const struct bt_value *params,
		const struct bt_component_sink **component)
{
	return bt_graph_add_sink_component_with_init_method_data(
		graph, comp_cls, name, params, NULL, component);
}

BT_HIDDEN
int bt_graph_remove_unconnected_component(struct bt_graph *graph,
		struct bt_component *component)
{
	bool init_can_consume;
	uint64_t count;
	uint64_t i;
	int ret = 0;

	BT_ASSERT(graph);
	BT_ASSERT(component);
	BT_ASSERT(component->base.ref_count == 0);
	BT_ASSERT(bt_component_borrow_graph(component) == graph);

	init_can_consume = graph->can_consume;
	count = bt_component_get_input_port_count(component);

	for (i = 0; i < count; i++) {
		struct bt_port *port = (void *)
			bt_component_borrow_input_port_by_index(component, i);

		BT_ASSERT(port);

		if (bt_port_is_connected(port)) {
			BT_LIB_LOGW("Cannot remove component from graph: "
				"an input port is connected: "
				"%![graph-]+g, %![comp-]+c, %![port-]+p",
				graph, component, port);
			goto error;
		}
	}

	count = bt_component_get_output_port_count(component);

	for (i = 0; i < count; i++) {
		struct bt_port *port = (void *)
			bt_component_borrow_output_port_by_index(component, i);

		BT_ASSERT(port);

		if (bt_port_is_connected(port)) {
			BT_LIB_LOGW("Cannot remove component from graph: "
				"an output port is connected: "
				"%![graph-]+g, %![comp-]+c, %![port-]+p",
				graph, component, port);
			goto error;
		}
	}

	bt_graph_set_can_consume(graph, false);

	/* Possibly remove from sinks to consume */
	(void) g_queue_remove(graph->sinks_to_consume, component);

	if (graph->sinks_to_consume->length == 0) {
		graph->has_sink = false;
	}

	/*
	 * This calls bt_object_try_spec_release() on the component, and
	 * since its reference count is 0, its destructor is called. Its
	 * destructor calls the user's finalization method (if set).
	 */
	g_ptr_array_remove(graph->components, component);
	goto end;

error:
	ret = -1;

end:
	(void) init_can_consume;
	bt_graph_set_can_consume(graph, init_can_consume);
	return ret;
}

BT_HIDDEN
void bt_graph_add_notification(struct bt_graph *graph,
		struct bt_notification *notif)
{
	BT_ASSERT(graph);
	BT_ASSERT(notif);

	/*
	 * It's okay not to take a reference because, when a
	 * notification's reference count drops to 0, either:
	 *
	 * * It is recycled back to one of this graph's pool.
	 * * It is destroyed because it doesn't have any link to any
	 *   graph, which means the original graph is already destroyed.
	 */
	g_ptr_array_add(graph->notifications, notif);
}
