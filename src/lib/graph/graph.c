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

#define BT_LOG_TAG "LIB/GRAPH"
#include "lib/logging.h"

#include "common/assert.h"
#include "lib/assert-pre.h"
#include "lib/assert-post.h"
#include <babeltrace2/graph/graph.h>
#include <babeltrace2/graph/component.h>
#include <babeltrace2/graph/port.h>
#include "lib/graph/message/message.h"
#include "compat/compiler.h"
#include "common/common.h"
#include <babeltrace2/types.h>
#include <babeltrace2/value.h>
#include "lib/value.h"
#include <unistd.h>
#include <stdbool.h>
#include <glib.h>

#include "component-class-sink-simple.h"
#include "component.h"
#include "component-sink.h"
#include "connection.h"
#include "graph.h"
#include "interrupter.h"
#include "message/event.h"
#include "message/packet.h"

typedef enum bt_graph_listener_func_status
(*port_added_func_t)(const void *, const void *, void *);

typedef enum bt_component_class_initialize_method_status
(*comp_init_method_t)(const void *, void *, const void *, void *);

struct bt_graph_listener_port_added {
	port_added_func_t func;
	void *data;
};

#define INIT_LISTENERS_ARRAY(_type, _listeners)				\
	do {								\
		_listeners = g_array_new(FALSE, TRUE, sizeof(_type));	\
		if (!(_listeners)) {					\
			BT_LIB_LOGE_APPEND_CAUSE(			\
				"Failed to allocate one GArray.");	\
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
	 * 2. This connection's destructor finalizes its active message
	 *    iterators.
	 * 3. A message iterator's finalization function gets a new
	 *    reference on its component (reference count goes from 0 to
	 *    1).
	 * 4. Since this component's reference count goes to 1, it takes
	 *    a reference on its parent (this graph). This graph's
	 *    reference count goes from 0 to 1.
	 * 5. The message iterator's finalization function puts its
	 *    component reference (reference count goes from 1 to 0).
	 * 6. Since this component's reference count goes from 1 to 0,
	 *    it puts its parent (this graph). This graph's reference
	 *    count goes from 1 to 0.
	 * 7. Since this graph's reference count goes from 1 to 0, its
	 *    destructor is called (this function).
	 *
	 * With the incrementation below, the graph's reference count at
	 * step 4 goes from 1 to 2, and from 2 to 1 at step 6. This
	 * ensures that this function is not called two times.
	 */
	BT_LIB_LOGI("Destroying graph: %!+g", graph);
	obj->ref_count++;
	graph->config_state = BT_GRAPH_CONFIGURATION_STATE_DESTROYING;

	if (graph->messages) {
		g_ptr_array_free(graph->messages, TRUE);
		graph->messages = NULL;
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

	if (graph->interrupters) {
		BT_LOGD_STR("Putting interrupters.");
		g_ptr_array_free(graph->interrupters, TRUE);
		graph->interrupters = NULL;
	}

	BT_OBJECT_PUT_REF_AND_RESET(graph->default_interrupter);

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

	bt_object_pool_finalize(&graph->event_msg_pool);
	bt_object_pool_finalize(&graph->packet_begin_msg_pool);
	bt_object_pool_finalize(&graph->packet_end_msg_pool);
	g_free(graph);
}

static
void destroy_message_event(struct bt_message *msg,
		struct bt_graph *graph)
{
	bt_message_event_destroy(msg);
}

static
void destroy_message_packet_begin(struct bt_message *msg,
		struct bt_graph *graph)
{
	bt_message_packet_destroy(msg);
}

static
void destroy_message_packet_end(struct bt_message *msg,
		struct bt_graph *graph)
{
	bt_message_packet_destroy(msg);
}

static
void notify_message_graph_is_destroyed(struct bt_message *msg)
{
	bt_message_unlink_graph(msg);
}

struct bt_graph *bt_graph_create(uint64_t mip_version)
{
	struct bt_graph *graph;
	int ret;

	BT_ASSERT_PRE_NO_ERROR();
	BT_ASSERT_PRE(mip_version <= bt_get_maximal_mip_version(),
		"Unknown MIP version: mip-version=%" PRIu64 ", "
		"max-mip-version=%" PRIu64,
		mip_version, bt_get_maximal_mip_version());
	BT_LOGI_STR("Creating graph object.");
	graph = g_new0(struct bt_graph, 1);
	if (!graph) {
		BT_LIB_LOGE_APPEND_CAUSE("Failed to allocate one graph.");
		goto end;
	}

	bt_object_init_shared(&graph->base, destroy_graph);
	graph->mip_version = mip_version;
	graph->connections = g_ptr_array_new_with_free_func(
		(GDestroyNotify) bt_object_try_spec_release);
	if (!graph->connections) {
		BT_LIB_LOGE_APPEND_CAUSE("Failed to allocate one GPtrArray.");
		goto error;
	}
	graph->components = g_ptr_array_new_with_free_func(
		(GDestroyNotify) bt_object_try_spec_release);
	if (!graph->components) {
		BT_LIB_LOGE_APPEND_CAUSE("Failed to allocate one GPtrArray.");
		goto error;
	}
	graph->sinks_to_consume = g_queue_new();
	if (!graph->sinks_to_consume) {
		BT_LIB_LOGE_APPEND_CAUSE("Failed to allocate one GQueue.");
		goto error;
	}

	bt_graph_set_can_consume(graph, true);
	INIT_LISTENERS_ARRAY(struct bt_graph_listener_port_added,
		graph->listeners.source_output_port_added);

	if (!graph->listeners.source_output_port_added) {
		goto error;
	}

	INIT_LISTENERS_ARRAY(struct bt_graph_listener_port_added,
		graph->listeners.filter_output_port_added);

	if (!graph->listeners.filter_output_port_added) {
		goto error;
	}

	INIT_LISTENERS_ARRAY(struct bt_graph_listener_port_added,
		graph->listeners.filter_input_port_added);

	if (!graph->listeners.filter_input_port_added) {
		goto error;
	}

	INIT_LISTENERS_ARRAY(struct bt_graph_listener_port_added,
		graph->listeners.sink_input_port_added);

	if (!graph->listeners.sink_input_port_added) {
		goto error;
	}

	graph->interrupters = g_ptr_array_new_with_free_func(
		(GDestroyNotify) bt_object_put_ref_no_null_check);
	if (!graph->interrupters) {
		BT_LIB_LOGE_APPEND_CAUSE("Failed to allocate one GPtrArray.");
		goto error;
	}

	graph->default_interrupter = bt_interrupter_create();
	if (!graph->default_interrupter) {
		BT_LIB_LOGE_APPEND_CAUSE(
			"Failed to create one interrupter object.");
		goto error;
	}

	bt_graph_add_interrupter(graph, graph->default_interrupter);
	ret = bt_object_pool_initialize(&graph->event_msg_pool,
		(bt_object_pool_new_object_func) bt_message_event_new,
		(bt_object_pool_destroy_object_func) destroy_message_event,
		graph);
	if (ret) {
		BT_LIB_LOGE_APPEND_CAUSE(
			"Failed to initialize event message pool: ret=%d",
			ret);
		goto error;
	}

	ret = bt_object_pool_initialize(&graph->packet_begin_msg_pool,
		(bt_object_pool_new_object_func) bt_message_packet_beginning_new,
		(bt_object_pool_destroy_object_func) destroy_message_packet_begin,
		graph);
	if (ret) {
		BT_LIB_LOGE_APPEND_CAUSE(
			"Failed to initialize packet beginning message pool: ret=%d",
			ret);
		goto error;
	}

	ret = bt_object_pool_initialize(&graph->packet_end_msg_pool,
		(bt_object_pool_new_object_func) bt_message_packet_end_new,
		(bt_object_pool_destroy_object_func) destroy_message_packet_end,
		graph);
	if (ret) {
		BT_LIB_LOGE_APPEND_CAUSE(
			"Failed to initialize packet end message pool: ret=%d",
			ret);
		goto error;
	}

	graph->messages = g_ptr_array_new_with_free_func(
		(GDestroyNotify) notify_message_graph_is_destroyed);
	BT_LIB_LOGI("Created graph object: %!+g", graph);

end:
	return (void *) graph;

error:
	BT_OBJECT_PUT_REF_AND_RESET(graph);
	goto end;
}

enum bt_graph_connect_ports_status bt_graph_connect_ports(
		struct bt_graph *graph,
		const struct bt_port_output *upstream_port_out,
		const struct bt_port_input *downstream_port_in,
		const struct bt_connection **user_connection)
{
	enum bt_graph_connect_ports_status status = BT_FUNC_STATUS_OK;
	struct bt_connection *connection = NULL;
	struct bt_port *upstream_port = (void *) upstream_port_out;
	struct bt_port *downstream_port = (void *) downstream_port_in;
	struct bt_component *upstream_component = NULL;
	struct bt_component *downstream_component = NULL;
	enum bt_component_class_port_connected_method_status port_connected_status;
	bool init_can_consume;

	BT_ASSERT_PRE_NO_ERROR();
	BT_ASSERT_PRE_NON_NULL(graph, "Graph");
	BT_ASSERT_PRE_NON_NULL(upstream_port, "Upstream port");
	BT_ASSERT_PRE_NON_NULL(downstream_port, "Downstream port port");
	BT_ASSERT_PRE(
		graph->config_state == BT_GRAPH_CONFIGURATION_STATE_CONFIGURING,
		"Graph is not in the \"configuring\" state: %!+g", graph);
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
	BT_LIB_LOGI("Connecting component ports within graph: "
		"%![graph-]+g, %![up-port-]+p, %![down-port-]+p",
		graph, upstream_port, downstream_port);
	bt_graph_set_can_consume(graph, false);
	upstream_component = bt_port_borrow_component_inline(
		(void *) upstream_port);
	downstream_component = bt_port_borrow_component_inline(
		(void *) downstream_port);

	BT_LOGD_STR("Creating connection.");
	connection = bt_connection_create(graph, (void *) upstream_port,
		(void *) downstream_port);
	if (!connection) {
		BT_LIB_LOGE_APPEND_CAUSE("Cannot create connection object.");
		status = BT_FUNC_STATUS_MEMORY_ERROR;
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
	port_connected_status = bt_component_port_connected(upstream_component,
		(void *) upstream_port, (void *) downstream_port);
	if (port_connected_status != BT_FUNC_STATUS_OK) {
		if (port_connected_status < 0) {
			BT_LIB_LOGW_APPEND_CAUSE(
				"Upstream component's \"port connected\" method failed: "
				"status=%s, %![graph-]+g, %![up-comp-]+c, "
				"%![down-comp-]+c, %![up-port-]+p, %![down-port-]+p",
				bt_common_func_status_string(
					port_connected_status),
				graph, upstream_component, downstream_component,
				upstream_port, downstream_port);
		}

		bt_connection_end(connection, true);
		status = (int) port_connected_status;
		goto end;
	}

	connection->notified_upstream_port_connected = true;
	BT_LIB_LOGD("Notifying downstream component that its port is connected: "
		"%![comp-]+c, %![port-]+p", downstream_component,
		downstream_port);
	port_connected_status = bt_component_port_connected(downstream_component,
		(void *) downstream_port, (void *) upstream_port);
	if (port_connected_status != BT_FUNC_STATUS_OK) {
		if (port_connected_status < 0) {
			BT_LIB_LOGW_APPEND_CAUSE(
				"Downstream component's \"port connected\" method failed: "
				"status=%s, %![graph-]+g, %![up-comp-]+c, "
				"%![down-comp-]+c, %![up-port-]+p, %![down-port-]+p",
				bt_common_func_status_string(
					port_connected_status),
				graph, upstream_component, downstream_component,
				upstream_port, downstream_port);
		}

		bt_connection_end(connection, true);
		status = (int) port_connected_status;
		goto end;
	}

	connection->notified_downstream_port_connected = true;

	BT_LIB_LOGI("Connected component ports within graph: "
		"%![graph-]+g, %![up-comp-]+c, %![down-comp-]+c, "
		"%![up-port-]+p, %![down-port-]+p",
		graph, upstream_component, downstream_component,
		upstream_port, downstream_port);

	if (user_connection) {
		/* Move reference to user */
		*user_connection = connection;
	}

end:
	if (status != BT_FUNC_STATUS_OK) {
		bt_graph_make_faulty(graph);
	}

	bt_object_put_ref(connection);
	(void) init_can_consume;
	bt_graph_set_can_consume(graph, init_can_consume);
	return status;
}

static inline
int consume_graph_sink(struct bt_component_sink *comp)
{
	enum bt_component_class_sink_consume_method_status consume_status;
	struct bt_component_class_sink *sink_class = NULL;

	BT_ASSERT_DBG(comp);
	sink_class = (void *) comp->parent.class;
	BT_ASSERT_DBG(sink_class->methods.consume);
	BT_LIB_LOGD("Calling user's consume method: %!+c", comp);
	consume_status = sink_class->methods.consume((void *) comp);
	BT_LOGD("User method returned: status=%s",
		bt_common_func_status_string(consume_status));
	BT_ASSERT_POST_DEV(consume_status == BT_FUNC_STATUS_OK ||
		consume_status == BT_FUNC_STATUS_END ||
		consume_status == BT_FUNC_STATUS_AGAIN ||
		consume_status == BT_FUNC_STATUS_ERROR ||
		consume_status == BT_FUNC_STATUS_MEMORY_ERROR,
		"Invalid component status returned by consuming method: "
		"status=%s", bt_common_func_status_string(consume_status));
	BT_ASSERT_POST_DEV_NO_ERROR_IF_NO_ERROR_STATUS(consume_status);
	if (consume_status) {
		if (consume_status < 0) {
			BT_LIB_LOGW_APPEND_CAUSE(
				"Component's \"consume\" method failed: "
				"status=%s, %![comp-]+c",
				bt_common_func_status_string(consume_status),
				comp);
		}

		goto end;
	}

	BT_LIB_LOGD("Consumed from sink: %![comp-]+c, status=%s",
		comp, bt_common_func_status_string(consume_status));

end:
	return consume_status;
}

/*
 * `node` is removed from the queue of sinks to consume when passed to
 * this function. This function adds it back to the queue if there's
 * still something to consume afterwards.
 */
static inline
int consume_sink_node(struct bt_graph *graph, GList *node)
{
	int status;
	struct bt_component_sink *sink;

	sink = node->data;
	status = consume_graph_sink(sink);
	if (G_UNLIKELY(status != BT_FUNC_STATUS_END)) {
		g_queue_push_tail_link(graph->sinks_to_consume, node);
		goto end;
	}

	/* End reached, the node is not added back to the queue and free'd. */
	g_queue_delete_link(graph->sinks_to_consume, node);

	/* Don't forward an END status if there are sinks left to consume. */
	if (!g_queue_is_empty(graph->sinks_to_consume)) {
		status = BT_FUNC_STATUS_OK;
		goto end;
	}

end:
	BT_LIB_LOGD("Consumed sink node: %![comp-]+c, status=%s",
		sink, bt_common_func_status_string(status));
	return status;
}

BT_HIDDEN
int bt_graph_consume_sink_no_check(struct bt_graph *graph,
		struct bt_component_sink *sink)
{
	int status;
	GList *sink_node;
	int index;

	BT_LIB_LOGD("Making specific sink consume: %![comp-]+c", sink);
	BT_ASSERT_DBG(bt_component_borrow_graph((void *) sink) == graph);

	if (g_queue_is_empty(graph->sinks_to_consume)) {
		BT_LOGD_STR("Graph's sink queue is empty: end of graph.");
		status = BT_FUNC_STATUS_END;
		goto end;
	}

	index = g_queue_index(graph->sinks_to_consume, sink);
	if (index < 0) {
		BT_LIB_LOGD("Sink component is not marked as consumable: "
			"component sink is ended: %![comp-]+c", sink);
		status = BT_FUNC_STATUS_END;
		goto end;
	}

	sink_node = g_queue_pop_nth_link(graph->sinks_to_consume, index);
	BT_ASSERT_DBG(sink_node);
	status = consume_sink_node(graph, sink_node);

end:
	return status;
}

static inline
int consume_no_check(struct bt_graph *graph)
{
	int status = BT_FUNC_STATUS_OK;
	struct bt_component *sink;
	GList *current_node;

	BT_ASSERT_PRE_DEV(graph->has_sink,
		"Graph has no sink component: %!+g", graph);
	BT_LIB_LOGD("Making next sink component consume: %![graph-]+g", graph);

	if (G_UNLIKELY(g_queue_is_empty(graph->sinks_to_consume))) {
		BT_LOGD_STR("Graph's sink queue is empty: end of graph.");
		status = BT_FUNC_STATUS_END;
		goto end;
	}

	current_node = g_queue_pop_head_link(graph->sinks_to_consume);
	sink = current_node->data;
	BT_LIB_LOGD("Chose next sink to consume: %!+c", sink);
	status = consume_sink_node(graph, current_node);

end:
	return status;
}

enum bt_graph_run_once_status bt_graph_run_once(struct bt_graph *graph)
{
	enum bt_graph_run_once_status status;

	BT_ASSERT_PRE_NO_ERROR();
	BT_ASSERT_PRE_DEV_NON_NULL(graph, "Graph");
	BT_ASSERT_PRE_DEV(graph->can_consume,
		"Cannot consume graph in its current state: %!+g", graph);
	BT_ASSERT_PRE_DEV(graph->config_state !=
		BT_GRAPH_CONFIGURATION_STATE_FAULTY,
		"Graph is in a faulty state: %!+g", graph);
	bt_graph_set_can_consume(graph, false);
	status = bt_graph_configure(graph);
	if (G_UNLIKELY(status)) {
		/* bt_graph_configure() logs errors */
		goto end;
	}

	status = consume_no_check(graph);
	bt_graph_set_can_consume(graph, true);

end:
	return status;
}

enum bt_graph_run_status bt_graph_run(struct bt_graph *graph)
{
	enum bt_graph_run_status status;

	BT_ASSERT_PRE_NO_ERROR();
	BT_ASSERT_PRE_NON_NULL(graph, "Graph");
	BT_ASSERT_PRE(graph->can_consume,
		"Cannot consume graph in its current state: %!+g", graph);
	BT_ASSERT_PRE(graph->config_state != BT_GRAPH_CONFIGURATION_STATE_FAULTY,
		"Graph is in a faulty state: %!+g", graph);
	bt_graph_set_can_consume(graph, false);
	status = bt_graph_configure(graph);
	if (G_UNLIKELY(status)) {
		/* bt_graph_configure() logs errors */
		goto end;
	}

	BT_LIB_LOGI("Running graph: %!+g", graph);

	do {
		/*
		 * Check if the graph is interrupted at each iteration.
		 * If the graph was interrupted by another thread or by
		 * a signal handler, this is NOT a warning nor an error;
		 * it was intentional: log with an INFO level only.
		 */
		if (G_UNLIKELY(bt_graph_is_interrupted(graph))) {
			BT_LIB_LOGI("Stopping the graph: "
				"graph was interrupted: %!+g", graph);
			status = BT_FUNC_STATUS_AGAIN;
			goto end;
		}

		status = consume_no_check(graph);
		if (G_UNLIKELY(status == BT_FUNC_STATUS_AGAIN)) {
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
				status = BT_FUNC_STATUS_OK;
			}
		}
	} while (status == BT_FUNC_STATUS_OK);

	if (status == BT_FUNC_STATUS_END) {
		/*
		 * The last call to consume_no_check() returned
		 * `BT_FUNC_STATUS_END`, but bt_graph_run() has no
		 * `BT_GRAPH_RUN_STATUS_END` status: replace with
		 * `BT_GRAPH_RUN_STATUS_OK` (success: graph ran
		 * completely).
		 */
		status = BT_FUNC_STATUS_OK;
	}

end:
	BT_LIB_LOGI("Graph ran: %![graph-]+g, status=%s", graph,
		bt_common_func_status_string(status));
	bt_graph_set_can_consume(graph, true);
	return status;
}

enum bt_graph_add_listener_status
bt_graph_add_source_component_output_port_added_listener(
		struct bt_graph *graph,
		bt_graph_source_component_output_port_added_listener_func func,
		void *data, bt_listener_id *out_listener_id)
{
	struct bt_graph_listener_port_added listener = {
		.func = (port_added_func_t) func,
		.data = data,
	};
	bt_listener_id listener_id;

	BT_ASSERT_PRE_NO_ERROR();
	BT_ASSERT_PRE_NON_NULL(graph, "Graph");
	BT_ASSERT_PRE_NON_NULL(func, "Listener");
	g_array_append_val(graph->listeners.source_output_port_added, listener);
	listener_id = graph->listeners.source_output_port_added->len - 1;
	BT_LIB_LOGD("Added \"source component output port added\" listener to graph: "
		"%![graph-]+g, listener-addr=%p, id=%d", graph, listener,
		listener_id);

	if (listener_id) {
		*out_listener_id = listener_id;
	}

	return BT_FUNC_STATUS_OK;
}

enum bt_graph_add_listener_status
bt_graph_add_filter_component_output_port_added_listener(
		struct bt_graph *graph,
		bt_graph_filter_component_output_port_added_listener_func func,
		void *data, bt_listener_id *out_listener_id)
{
	struct bt_graph_listener_port_added listener = {
		.func = (port_added_func_t) func,
		.data = data,
	};
	bt_listener_id listener_id;

	BT_ASSERT_PRE_NO_ERROR();
	BT_ASSERT_PRE_NON_NULL(graph, "Graph");
	BT_ASSERT_PRE_NON_NULL(func, "Listener");
	g_array_append_val(graph->listeners.filter_output_port_added, listener);
	listener_id = graph->listeners.filter_output_port_added->len - 1;
	BT_LIB_LOGD("Added \"filter component output port added\" listener to graph: "
		"%![graph-]+g, listener-addr=%p, id=%d", graph, listener,
		listener_id);

	if (listener_id) {
		*out_listener_id = listener_id;
	}

	return BT_FUNC_STATUS_OK;
}

enum bt_graph_add_listener_status
bt_graph_add_filter_component_input_port_added_listener(
		struct bt_graph *graph,
		bt_graph_filter_component_input_port_added_listener_func func,
		void *data, bt_listener_id *out_listener_id)
{
	struct bt_graph_listener_port_added listener = {
		.func = (port_added_func_t) func,
		.data = data,
	};
	bt_listener_id listener_id;

	BT_ASSERT_PRE_NO_ERROR();
	BT_ASSERT_PRE_NON_NULL(graph, "Graph");
	BT_ASSERT_PRE_NON_NULL(func, "Listener");
	g_array_append_val(graph->listeners.filter_input_port_added, listener);
	listener_id = graph->listeners.filter_input_port_added->len - 1;
	BT_LIB_LOGD("Added \"filter component input port added\" listener to graph: "
		"%![graph-]+g, listener-addr=%p, id=%d", graph, listener,
		listener_id);

	if (listener_id) {
		*out_listener_id = listener_id;
	}

	return BT_FUNC_STATUS_OK;
}

enum bt_graph_add_listener_status
bt_graph_add_sink_component_input_port_added_listener(
		struct bt_graph *graph,
		bt_graph_sink_component_input_port_added_listener_func func,
		void *data, bt_listener_id *out_listener_id)
{
	struct bt_graph_listener_port_added listener = {
		.func = (port_added_func_t) func,
		.data = data,
	};
	bt_listener_id listener_id;

	BT_ASSERT_PRE_NO_ERROR();
	BT_ASSERT_PRE_NON_NULL(graph, "Graph");
	BT_ASSERT_PRE_NON_NULL(func, "Listener");
	g_array_append_val(graph->listeners.sink_input_port_added, listener);
	listener_id = graph->listeners.sink_input_port_added->len - 1;
	BT_LIB_LOGD("Added \"sink component input port added\" listener to graph: "
		"%![graph-]+g, listener-addr=%p, id=%d", graph, listener,
		listener_id);

	if (listener_id) {
		*out_listener_id = listener_id;
	}

	return BT_FUNC_STATUS_OK;
}

BT_HIDDEN
enum bt_graph_listener_func_status bt_graph_notify_port_added(
		struct bt_graph *graph, struct bt_port *port)
{
	uint64_t i;
	GArray *listeners;
	struct bt_component *comp;
	enum bt_graph_listener_func_status status = BT_FUNC_STATUS_OK;

	BT_ASSERT(graph);
	BT_ASSERT(port);
	BT_LIB_LOGD("Notifying graph listeners that a port was added: "
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
			bt_common_abort();
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
			bt_common_abort();
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
			bt_common_abort();
		}

		break;
	}
	default:
		bt_common_abort();
	}

	for (i = 0; i < listeners->len; i++) {
		struct bt_graph_listener_port_added *listener =
			&g_array_index(listeners,
				struct bt_graph_listener_port_added, i);


		BT_ASSERT(listener->func);
		status = listener->func(comp, port, listener->data);
		BT_ASSERT_POST_NO_ERROR_IF_NO_ERROR_STATUS(status);
		if (status != BT_FUNC_STATUS_OK) {
			goto end;
		}
	}

end:
	return status;
}

BT_HIDDEN
void bt_graph_remove_connection(struct bt_graph *graph,
		struct bt_connection *connection)
{
	BT_ASSERT(graph);
	BT_ASSERT(connection);
	BT_LIB_LOGD("Removing graph's connection: %![graph-]+g, %![conn-]+x",
		graph, connection);
	g_ptr_array_remove(graph->connections, connection);
}

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
int add_component_with_init_method_data(
		struct bt_graph *graph,
		struct bt_component_class *comp_cls,
		comp_init_method_t init_method,
		const char *name, const struct bt_value *params,
		void *init_method_data, bt_logging_level log_level,
		const struct bt_component **user_component)
{
	int status = BT_FUNC_STATUS_OK;
	enum bt_component_class_initialize_method_status init_status;
	struct bt_component *component = NULL;
	int ret;
	bool init_can_consume;
	struct bt_value *new_params = NULL;

	BT_ASSERT(comp_cls);
	BT_ASSERT_PRE_NON_NULL(graph, "Graph");
	BT_ASSERT_PRE_NON_NULL(name, "Name");
	BT_ASSERT_PRE(
		graph->config_state == BT_GRAPH_CONFIGURATION_STATE_CONFIGURING,
		"Graph is not in the \"configuring\" state: %!+g", graph);
	BT_ASSERT_PRE(!component_name_exists(graph, name),
		"Duplicate component name: %!+g, name=\"%s\"", graph, name);
	BT_ASSERT_PRE(!params || bt_value_is_map(params),
		"Parameter value is not a map value: %!+v", params);
	init_can_consume = graph->can_consume;
	bt_graph_set_can_consume(graph, false);
	BT_LIB_LOGI("Adding component to graph: "
		"%![graph-]+g, %![cc-]+C, name=\"%s\", log-level=%s, "
		"%![params-]+v, init-method-data-addr=%p",
		graph, comp_cls, name,
		bt_common_logging_level_string(log_level), params,
		init_method_data);

	if (!params) {
		new_params = bt_value_map_create();
		if (!new_params) {
			BT_LIB_LOGE_APPEND_CAUSE(
				"Cannot create empty map value object.");
			status = BT_FUNC_STATUS_MEMORY_ERROR;
			goto end;
		}

		params = new_params;
	}

	ret = bt_component_create(comp_cls, name, log_level, &component);
	if (ret) {
		BT_LIB_LOGE_APPEND_CAUSE(
			"Cannot create empty component object: ret=%d",
			ret);
		status = BT_FUNC_STATUS_MEMORY_ERROR;
		goto end;
	}

	/*
	 * The user's initialization method needs to see that this
	 * component is part of the graph. If the user method fails, we
	 * immediately remove the component from the graph's components.
	 */
	g_ptr_array_add(graph->components, component);
	bt_component_set_graph(component, graph);
	bt_value_freeze(params);

	if (init_method) {
		/*
		 * There is no use for config objects right now, so just pass
		 * NULL.
		 */
		BT_LOGD_STR("Calling user's initialization method.");
		init_status = init_method(component, NULL, params, init_method_data);
		BT_LOGD("User method returned: status=%s",
			bt_common_func_status_string(init_status));
		BT_ASSERT_POST_DEV_NO_ERROR_IF_NO_ERROR_STATUS(init_status);
		if (init_status != BT_FUNC_STATUS_OK) {
			if (init_status < 0) {
				BT_LIB_LOGW_APPEND_CAUSE(
					"Component initialization method failed: "
					"status=%s, %![comp-]+c",
					bt_common_func_status_string(init_status),
					component);
			}

			status = init_status;
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
	 * sink queue to be consumed by bt_graph_run() or
	 * bt_graph_run_once().
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
	BT_LIB_LOGI("Added component to graph: "
		"%![graph-]+g, %![cc-]+C, name=\"%s\", log-level=%s, "
		"%![params-]+v, init-method-data-addr=%p, %![comp-]+c",
		graph, comp_cls, name,
		bt_common_logging_level_string(log_level), params,
		init_method_data, component);

	if (user_component) {
		/* Move reference to user */
		*user_component = component;
	}

end:
	if (status != BT_FUNC_STATUS_OK) {
		bt_graph_make_faulty(graph);
	}

	bt_object_put_ref(component);
	bt_object_put_ref(new_params);
	(void) init_can_consume;
	bt_graph_set_can_consume(graph, init_can_consume);
	return status;
}

enum bt_graph_add_component_status
bt_graph_add_source_component_with_initialize_method_data(
		struct bt_graph *graph,
		const struct bt_component_class_source *comp_cls,
		const char *name, const struct bt_value *params,
		void *init_method_data, bt_logging_level log_level,
		const struct bt_component_source **component)
{
	BT_ASSERT_PRE_NO_ERROR();
	BT_ASSERT_PRE_NON_NULL(comp_cls, "Component class");
	return add_component_with_init_method_data(graph,
		(void *) comp_cls, (comp_init_method_t) comp_cls->methods.init,
		name, params, init_method_data, log_level, (void *) component);
}

enum bt_graph_add_component_status bt_graph_add_source_component(
		struct bt_graph *graph,
		const struct bt_component_class_source *comp_cls,
		const char *name, const struct bt_value *params,
		enum bt_logging_level log_level,
		const struct bt_component_source **component)
{
	BT_ASSERT_PRE_NO_ERROR();
	return bt_graph_add_source_component_with_initialize_method_data(
		graph, comp_cls, name, params, NULL, log_level, component);
}

enum bt_graph_add_component_status
bt_graph_add_filter_component_with_initialize_method_data(
		struct bt_graph *graph,
		const struct bt_component_class_filter *comp_cls,
		const char *name, const struct bt_value *params,
		void *init_method_data, enum bt_logging_level log_level,
		const struct bt_component_filter **component)
{
	BT_ASSERT_PRE_NO_ERROR();
	BT_ASSERT_PRE_NON_NULL(comp_cls, "Component class");
	return add_component_with_init_method_data(graph,
		(void *) comp_cls, (comp_init_method_t) comp_cls->methods.init,
		name, params, init_method_data, log_level, (void *) component);
}

enum bt_graph_add_component_status bt_graph_add_filter_component(
		struct bt_graph *graph,
		const struct bt_component_class_filter *comp_cls,
		const char *name, const struct bt_value *params,
		enum bt_logging_level log_level,
		const struct bt_component_filter **component)
{
	BT_ASSERT_PRE_NO_ERROR();
	return bt_graph_add_filter_component_with_initialize_method_data(
		graph, comp_cls, name, params, NULL, log_level, component);
}

enum bt_graph_add_component_status
bt_graph_add_sink_component_with_initialize_method_data(
		struct bt_graph *graph,
		const struct bt_component_class_sink *comp_cls,
		const char *name, const struct bt_value *params,
		void *init_method_data, enum bt_logging_level log_level,
		const struct bt_component_sink **component)
{
	BT_ASSERT_PRE_NO_ERROR();
	BT_ASSERT_PRE_NON_NULL(comp_cls, "Component class");
	return add_component_with_init_method_data(graph,
		(void *) comp_cls, (comp_init_method_t) comp_cls->methods.init,
		name, params, init_method_data, log_level, (void *) component);
}

enum bt_graph_add_component_status bt_graph_add_sink_component(
		struct bt_graph *graph,
		const struct bt_component_class_sink *comp_cls,
		const char *name, const struct bt_value *params,
		enum bt_logging_level log_level,
		const struct bt_component_sink **component)
{
	BT_ASSERT_PRE_NO_ERROR();
	return bt_graph_add_sink_component_with_initialize_method_data(
		graph, comp_cls, name, params, NULL, log_level, component);
}

enum bt_graph_add_component_status
bt_graph_add_simple_sink_component(struct bt_graph *graph, const char *name,
		bt_graph_simple_sink_component_initialize_func init_func,
		bt_graph_simple_sink_component_consume_func consume_func,
		bt_graph_simple_sink_component_finalize_func finalize_func,
		void *user_data, const bt_component_sink **component)
{
	enum bt_graph_add_component_status status;
	struct bt_component_class_sink *comp_cls;
	struct simple_sink_init_method_data init_method_data = {
		.init_func = init_func,
		.consume_func = consume_func,
		.finalize_func = finalize_func,
		.user_data = user_data,
	};

	BT_ASSERT_PRE_NO_ERROR();

	/*
	 * Other preconditions are checked by
	 * bt_graph_add_sink_component_with_init_method_data().
	 */
	BT_ASSERT_PRE_NON_NULL(consume_func, "Consume function");

	comp_cls = bt_component_class_sink_simple_borrow();
	if (!comp_cls) {
		BT_LIB_LOGE_APPEND_CAUSE(
			"Cannot borrow simple sink component class.");
		status = BT_FUNC_STATUS_MEMORY_ERROR;
		goto end;
	}

	status = bt_graph_add_sink_component_with_initialize_method_data(graph,
		comp_cls, name, NULL, &init_method_data,
		BT_LOGGING_LEVEL_NONE, component);

end:
	return status;
}

BT_HIDDEN
void bt_graph_add_message(struct bt_graph *graph,
		struct bt_message *msg)
{
	BT_ASSERT(graph);
	BT_ASSERT(msg);

	/*
	 * It's okay not to take a reference because, when a
	 * message's reference count drops to 0, either:
	 *
	 * * It is recycled back to one of this graph's pool.
	 * * It is destroyed because it doesn't have any link to any
	 *   graph, which means the original graph is already destroyed.
	 */
	g_ptr_array_add(graph->messages, msg);
}

BT_HIDDEN
bool bt_graph_is_interrupted(const struct bt_graph *graph)
{
	BT_ASSERT_DBG(graph);
	return bt_interrupter_array_any_is_set(graph->interrupters);
}

enum bt_graph_add_interrupter_status bt_graph_add_interrupter(
		struct bt_graph *graph, const struct bt_interrupter *intr)
{
	BT_ASSERT_PRE_NO_ERROR();
	BT_ASSERT_PRE_NON_NULL(graph, "Graph");
	BT_ASSERT_PRE_NON_NULL(intr, "Interrupter");
	g_ptr_array_add(graph->interrupters, (void *) intr);
	bt_object_get_ref_no_null_check(intr);
	BT_LIB_LOGD("Added interrupter to graph: %![graph-]+g, %![intr-]+z",
		graph, intr);
	return BT_FUNC_STATUS_OK;
}

struct bt_interrupter *bt_graph_borrow_default_interrupter(bt_graph *graph)
{
	BT_ASSERT_PRE_NON_NULL(graph, "Graph");
	return graph->default_interrupter;
}

void bt_graph_get_ref(const struct bt_graph *graph)
{
	bt_object_get_ref(graph);
}

void bt_graph_put_ref(const struct bt_graph *graph)
{
	bt_object_put_ref(graph);
}
