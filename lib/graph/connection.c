/*
 * connection.c
 *
 * Babeltrace Connection
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

#define BT_LOG_TAG "CONNECTION"
#include <babeltrace/lib-logging-internal.h>

#include <babeltrace/graph/notification-iterator-internal.h>
#include <babeltrace/graph/component-internal.h>
#include <babeltrace/graph/component-source-internal.h>
#include <babeltrace/graph/component-filter-internal.h>
#include <babeltrace/graph/connection-internal.h>
#include <babeltrace/graph/private-connection.h>
#include <babeltrace/graph/graph-internal.h>
#include <babeltrace/graph/port-internal.h>
#include <babeltrace/object-internal.h>
#include <babeltrace/compiler-internal.h>
#include <babeltrace/assert-internal.h>
#include <stdlib.h>
#include <glib.h>

static
void bt_connection_destroy(struct bt_object *obj)
{
	struct bt_connection *connection = container_of(obj,
			struct bt_connection, base);

	BT_LOGD("Destroying connection: addr=%p", connection);

	/*
	 * Make sure that each notification iterator which was created
	 * for this connection is finalized before we destroy it. Once a
	 * notification iterator is finalized, all its method return
	 * NULL or the BT_NOTIFICATION_ITERATOR_STATUS_CANCELED status.
	 *
	 * Because connections are destroyed before components within a
	 * graph, this ensures that notification iterators are always
	 * finalized before their upstream component.
	 *
	 * Ending the connection does exactly this. We pass `false` to
	 * bt_connection_end() here to avoid removing this connection
	 * from the graph: if we're here, we're already in the graph's
	 * destructor.
	 */
	bt_connection_end(connection, false);
	g_ptr_array_free(connection->iterators, TRUE);

	/*
	 * No bt_put on ports as a connection only holds _weak_
	 * references to them.
	 */
	g_free(connection);
}

static
void bt_connection_try_remove_from_graph(struct bt_connection *connection)
{
	void *graph = bt_object_borrow_parent(&connection->base);

	if (connection->base.ref_count.count > 0 ||
			connection->downstream_port ||
			connection->upstream_port ||
			connection->iterators->len > 0) {
		return;
	}

	/*
	 * At this point we know that:
	 *
	 * 1. The connection is ended (ports were disconnected).
	 * 2. All the notification iterators that this connection
	 *    created, if any, are finalized.
	 * 3. The connection's reference count is 0, so only the
	 *    parent (graph) owns this connection after this call.
	 *
	 * In other words, no other object than the graph knows this
	 * connection.
	 *
	 * It is safe to remove the connection from the graph, therefore
	 * destroying it.
	 */
	BT_LOGD("Removing self from graph's connections: "
		"graph-addr=%p, conn-addr=%p", graph, connection);
	bt_graph_remove_connection(graph, connection);
}

static
void bt_connection_parent_is_owner(struct bt_object *obj)
{
	struct bt_connection *connection = container_of(obj,
			struct bt_connection, base);

	bt_connection_try_remove_from_graph(connection);
}

struct bt_connection *bt_connection_from_private(
		struct bt_private_connection *private_connection)
{
	return bt_get(bt_connection_borrow_from_private(private_connection));
}

BT_HIDDEN
struct bt_connection *bt_connection_create(
		struct bt_graph *graph,
		struct bt_port *upstream_port,
		struct bt_port *downstream_port)
{
	struct bt_connection *connection = NULL;

	if (bt_port_get_type(upstream_port) != BT_PORT_TYPE_OUTPUT) {
		BT_LOGW_STR("Invalid parameter: upstream port is not an output port.");
		goto end;
	}
	if (bt_port_get_type(downstream_port) != BT_PORT_TYPE_INPUT) {
		BT_LOGW_STR("Invalid parameter: downstream port is not an input port.");
		goto end;
	}

	BT_LOGD("Creating connection: "
		"graph-addr=%p, upstream-port-addr=%p, uptream-port-name=\"%s\", "
		"downstream-port-addr=%p, downstream-port-name=\"%s\"",
		graph, upstream_port, bt_port_get_name(upstream_port),
		downstream_port, bt_port_get_name(downstream_port));
	connection = g_new0(struct bt_connection, 1);
	if (!connection) {
		BT_LOGE_STR("Failed to allocate one connection.");
		goto end;
	}

	bt_object_init(connection, bt_connection_destroy);
	bt_object_set_parent_is_owner_listener(connection,
		bt_connection_parent_is_owner);
	connection->iterators = g_ptr_array_new();
	if (!connection->iterators) {
		BT_LOGE_STR("Failed to allocate a GPtrArray.");
		BT_PUT(connection);
		goto end;
	}

	/* Weak references are taken, see comment in header. */
	connection->upstream_port = upstream_port;
	connection->downstream_port = downstream_port;
	BT_LOGD_STR("Setting upstream port's connection.");
	bt_port_set_connection(upstream_port, connection);
	BT_LOGD_STR("Setting downstream port's connection.");
	bt_port_set_connection(downstream_port, connection);
	bt_object_set_parent(connection, &graph->base);
	BT_LOGD("Created connection: "
		"graph-addr=%p, upstream-port-addr=%p, uptream-port-name=\"%s\", "
		"downstream-port-addr=%p, downstream-port-name=\"%s\", "
		"conn-addr=%p",
		graph, upstream_port, bt_port_get_name(upstream_port),
		downstream_port, bt_port_get_name(downstream_port),
		connection);

end:
	return connection;
}

BT_HIDDEN
void bt_connection_end(struct bt_connection *conn,
		bool try_remove_from_graph)
{
	struct bt_component *downstream_comp = NULL;
	struct bt_component *upstream_comp = NULL;
	struct bt_port *downstream_port = conn->downstream_port;
	struct bt_port *upstream_port = conn->upstream_port;
	struct bt_graph *graph = bt_connection_borrow_graph(conn);
	size_t i;

	BT_LOGD("Ending connection: conn-addr=%p, try-remove-from-graph=%d",
		conn, try_remove_from_graph);

	if (downstream_port) {
		BT_LOGD("Disconnecting connection's downstream port: "
			"port-addr=%p, port-name=\"%s\"",
			downstream_port, bt_port_get_name(downstream_port));
		downstream_comp = bt_port_get_component(downstream_port);
		bt_port_set_connection(downstream_port, NULL);
		conn->downstream_port = NULL;
	}

	if (upstream_port) {
		BT_LOGD("Disconnecting connection's upstream port: "
			"port-addr=%p, port-name=\"%s\"",
			upstream_port, bt_port_get_name(upstream_port));
		upstream_comp = bt_port_get_component(upstream_port);
		bt_port_set_connection(upstream_port, NULL);
		conn->upstream_port = NULL;
	}

	if (downstream_comp) {
		/* bt_component_port_disconnected() logs details */
		bt_component_port_disconnected(downstream_comp,
			downstream_port);
	}

	if (upstream_comp) {
		/* bt_component_port_disconnected() logs details */
		bt_component_port_disconnected(upstream_comp, upstream_port);
	}

	BT_ASSERT(graph);
	/* bt_graph_notify_ports_disconnected() logs details */
	bt_graph_notify_ports_disconnected(graph, upstream_comp,
		downstream_comp, upstream_port, downstream_port);
	bt_put(downstream_comp);
	bt_put(upstream_comp);

	/*
	 * Because this connection is ended, finalize (cancel) each
	 * notification iterator created from it.
	 */
	for (i = 0; i < conn->iterators->len; i++) {
		struct bt_notification_iterator_private_connection *iterator =
			g_ptr_array_index(conn->iterators, i);

		BT_LOGD("Finalizing notification iterator created by this ended connection: "
			"conn-addr=%p, iter-addr=%p", conn, iterator);
		bt_private_connection_notification_iterator_finalize(iterator);

		/*
		 * Make sure this iterator does not try to remove itself
		 * from this connection's iterators on destruction
		 * because this connection won't exist anymore.
		 */
		bt_private_connection_notification_iterator_set_connection(
			iterator, NULL);
	}

	g_ptr_array_set_size(conn->iterators, 0);

	if (try_remove_from_graph) {
		bt_connection_try_remove_from_graph(conn);
	}
}

struct bt_port *bt_connection_get_upstream_port(
		struct bt_connection *connection)
{
	return connection ? bt_get(connection->upstream_port) : NULL;
}

struct bt_port *bt_connection_get_downstream_port(
		struct bt_connection *connection)
{
	return connection ? bt_get(connection->downstream_port) : NULL;
}

enum bt_connection_status
bt_private_connection_create_notification_iterator(
		struct bt_private_connection *private_connection,
		const enum bt_notification_type *notification_types,
		struct bt_notification_iterator **user_iterator)
{
	enum bt_component_class_type upstream_comp_class_type;
	struct bt_notification_iterator_private_connection *iterator = NULL;
	struct bt_port *upstream_port = NULL;
	struct bt_component *upstream_component = NULL;
	struct bt_component_class *upstream_comp_class = NULL;
	struct bt_connection *connection = NULL;
	bt_component_class_notification_iterator_init_method init_method = NULL;
	enum bt_connection_status status;
	static const enum bt_notification_type all_notif_types[] = {
		BT_NOTIFICATION_TYPE_ALL,
		BT_NOTIFICATION_TYPE_SENTINEL,
	};

	if (!private_connection) {
		BT_LOGW_STR("Invalid parameter: private connection is NULL.");
		status = BT_CONNECTION_STATUS_INVALID;
		goto end;
	}

	if (!user_iterator) {
		BT_LOGW_STR("Invalid parameter: notification iterator pointer is NULL.");
		status = BT_CONNECTION_STATUS_INVALID;
		goto end;
	}

	connection = bt_connection_borrow_from_private(private_connection);

	if (bt_graph_is_canceled(bt_connection_borrow_graph(connection))) {
		BT_LOGW("Cannot create notification iterator from connection: "
			"connection's graph is canceled: "
			"conn-addr=%p, upstream-port-addr=%p, "
			"upstream-port-name=\"%s\", upstream-comp-addr=%p, "
			"upstream-comp-name=\"%s\", graph-addr=%p",
			connection, connection->upstream_port,
			bt_port_get_name(connection->upstream_port),
			upstream_component,
			bt_component_get_name(upstream_component),
			bt_connection_borrow_graph(connection));
		status = BT_CONNECTION_STATUS_GRAPH_IS_CANCELED;
		goto end;
	}

	if (bt_connection_is_ended(connection)) {
		BT_LOGW("Invalid parameter: connection is ended: "
			"conn-addr=%p", connection);
		status = BT_CONNECTION_STATUS_IS_ENDED;
		goto end;
	}

	if (!notification_types) {
		BT_LOGD_STR("No notification types: subscribing to all notifications.");
		notification_types = all_notif_types;
	}

	upstream_port = connection->upstream_port;
	BT_ASSERT(upstream_port);
	upstream_component = bt_port_get_component(upstream_port);
	BT_ASSERT(upstream_component);
	upstream_comp_class = upstream_component->class;
	BT_LOGD("Creating notification iterator from connection: "
		"conn-addr=%p, upstream-port-addr=%p, "
		"upstream-port-name=\"%s\", upstream-comp-addr=%p, "
		"upstream-comp-name=\"%s\"",
		connection, connection->upstream_port,
		bt_port_get_name(connection->upstream_port),
		upstream_component, bt_component_get_name(upstream_component));
	upstream_comp_class_type =
		bt_component_get_class_type(upstream_component);
	BT_ASSERT(upstream_comp_class_type == BT_COMPONENT_CLASS_TYPE_SOURCE ||
			upstream_comp_class_type == BT_COMPONENT_CLASS_TYPE_FILTER);
	status = bt_private_connection_notification_iterator_create(upstream_component,
		upstream_port, notification_types, connection, &iterator);
	if (status != BT_CONNECTION_STATUS_OK) {
		BT_LOGW("Cannot create notification iterator from connection.");
		goto end;
	}

	switch (upstream_comp_class_type) {
	case BT_COMPONENT_CLASS_TYPE_SOURCE:
	{
		struct bt_component_class_source *source_class =
			container_of(upstream_comp_class,
				struct bt_component_class_source, parent);
		init_method = source_class->methods.iterator.init;
		break;
	}
	case BT_COMPONENT_CLASS_TYPE_FILTER:
	{
		struct bt_component_class_filter *filter_class =
			container_of(upstream_comp_class,
				struct bt_component_class_filter, parent);
		init_method = filter_class->methods.iterator.init;
		break;
	}
	default:
		/* Unreachable. */
		BT_LOGF("Unknown component class type: type=%d",
			upstream_comp_class_type);
		abort();
	}

	if (init_method) {
		enum bt_notification_iterator_status iter_status;

		BT_LOGD("Calling user's initialization method: iter-addr=%p",
			iterator);
		iter_status = init_method(
			bt_private_connection_private_notification_iterator_from_notification_iterator((void *) iterator),
			bt_private_port_from_port(upstream_port));
		BT_LOGD("User method returned: status=%s",
			bt_notification_iterator_status_string(iter_status));
		if (iter_status != BT_NOTIFICATION_ITERATOR_STATUS_OK) {
			BT_LOGW_STR("Initialization method failed.");
			status = bt_connection_status_from_notification_iterator_status(
				iter_status);
			goto end;
		}
	}

	iterator->state = BT_PRIVATE_CONNECTION_NOTIFICATION_ITERATOR_STATE_ACTIVE;
	g_ptr_array_add(connection->iterators, iterator);
	BT_LOGD("Created notification iterator from connection: "
		"conn-addr=%p, upstream-port-addr=%p, "
		"upstream-port-name=\"%s\", upstream-comp-addr=%p, "
		"upstream-comp-name=\"%s\", iter-addr=%p",
		connection, connection->upstream_port,
		bt_port_get_name(connection->upstream_port),
		upstream_component, bt_component_get_name(upstream_component),
		iterator);

	/* Move reference to user */
	*user_iterator = (void *) iterator;
	iterator = NULL;

end:
	bt_put(upstream_component);
	bt_put(iterator);
	return status;
}

BT_HIDDEN
void bt_connection_remove_iterator(struct bt_connection *conn,
		struct bt_notification_iterator_private_connection *iterator)
{
	g_ptr_array_remove(conn->iterators, iterator);
	BT_LOGV("Removed notification iterator from connection: "
		"conn-addr=%p, iter-addr=%p", conn, iterator);
	bt_connection_try_remove_from_graph(conn);
}

bt_bool bt_connection_is_ended(struct bt_connection *connection)
{
	return !connection->downstream_port && !connection->upstream_port;
}
