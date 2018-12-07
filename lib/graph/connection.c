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

#define BT_LOG_TAG "CONNECTION"
#include <babeltrace/lib-logging-internal.h>

#include <babeltrace/graph/notification-iterator-internal.h>
#include <babeltrace/graph/component-internal.h>
#include <babeltrace/graph/connection-internal.h>
#include <babeltrace/graph/connection-const.h>
#include <babeltrace/graph/graph-internal.h>
#include <babeltrace/graph/port-internal.h>
#include <babeltrace/object-internal.h>
#include <babeltrace/compiler-internal.h>
#include <babeltrace/object.h>
#include <babeltrace/assert-internal.h>
#include <babeltrace/assert-pre-internal.h>
#include <stdlib.h>
#include <glib.h>

static
void destroy_connection(struct bt_object *obj)
{
	struct bt_connection *connection = container_of(obj,
			struct bt_connection, base);

	BT_LIB_LOGD("Destroying connection: %!+x", connection);

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
	connection->iterators = NULL;

	/*
	 * No bt_object_put_ref on ports as a connection only holds _weak_
	 * references to them.
	 */
	g_free(connection);
}

static
void try_remove_connection_from_graph(struct bt_connection *connection)
{
	void *graph = (void *) bt_object_borrow_parent(&connection->base);

	if (connection->base.ref_count > 0 ||
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
	BT_LIB_LOGD("Removing self from graph's connections: "
		"%![graph-]+g, %![conn-]+x", graph, connection);
	bt_graph_remove_connection(graph, connection);
}

static
void parent_is_owner(struct bt_object *obj)
{
	struct bt_connection *connection = container_of(obj,
			struct bt_connection, base);

	try_remove_connection_from_graph(connection);
}

BT_HIDDEN
struct bt_connection *bt_connection_create(struct bt_graph *graph,
		struct bt_port *upstream_port,
		struct bt_port *downstream_port)
{
	struct bt_connection *connection = NULL;

	BT_LIB_LOGD("Creating connection: "
		"%![graph-]+g, %![up-port-]+p, %![down-port-]+p",
		graph, upstream_port, downstream_port);
	connection = g_new0(struct bt_connection, 1);
	if (!connection) {
		BT_LOGE_STR("Failed to allocate one connection.");
		goto end;
	}

	bt_object_init_shared_with_parent(&connection->base,
		destroy_connection);
	bt_object_set_parent_is_owner_listener_func(&connection->base,
		parent_is_owner);
	connection->iterators = g_ptr_array_new();
	if (!connection->iterators) {
		BT_LOGE_STR("Failed to allocate a GPtrArray.");
		BT_OBJECT_PUT_REF_AND_RESET(connection);
		goto end;
	}

	/* Weak references are taken, see comment in header. */
	connection->upstream_port = upstream_port;
	connection->downstream_port = downstream_port;
	BT_LIB_LOGD("Setting upstream port's connection: %!+p", upstream_port);
	bt_port_set_connection(upstream_port, connection);
	BT_LIB_LOGD("Setting downstream port's connection: %!+p",
		downstream_port);
	bt_port_set_connection(downstream_port, connection);
	bt_object_set_parent(&connection->base, &graph->base);
	BT_LIB_LOGD("Created connection: %!+x", connection);

end:
	return connection;
}

BT_HIDDEN
void bt_connection_end(struct bt_connection *conn, bool try_remove_from_graph)
{
	struct bt_component *downstream_comp = NULL;
	struct bt_component *upstream_comp = NULL;
	struct bt_port *downstream_port = conn->downstream_port;
	struct bt_port *upstream_port = conn->upstream_port;
	struct bt_graph *graph = bt_connection_borrow_graph(conn);
	size_t i;

	BT_LIB_LOGD("Ending connection: %!+x, try-remove-from-graph=%d",
		conn, try_remove_from_graph);

	/*
	 * Any of the following notification callback functions could
	 * remove one of the connection's ports from its component. To
	 * make sure that at least logging in called functions works
	 * with existing objects, get a local reference on both ports.
	 */
	bt_object_get_ref(downstream_port);
	bt_object_get_ref(upstream_port);

	if (downstream_port) {
		BT_LIB_LOGD("Disconnecting connection's downstream port: %!+p",
			downstream_port);
		downstream_comp = bt_port_borrow_component_inline(
			downstream_port);
		bt_port_set_connection(downstream_port, NULL);
		conn->downstream_port = NULL;
	}

	if (upstream_port) {
		BT_LIB_LOGD("Disconnecting connection's upstream port: %!+p",
			upstream_port);
		upstream_comp = bt_port_borrow_component_inline(
			upstream_port);
		bt_port_set_connection(upstream_port, NULL);
		conn->upstream_port = NULL;
	}

	if (downstream_comp && conn->notified_downstream_port_connected &&
			!conn->notified_downstream_port_disconnected) {
		/* bt_component_port_disconnected() logs details */
		bt_component_port_disconnected(downstream_comp,
			downstream_port);
		conn->notified_downstream_port_disconnected = true;
	}

	if (upstream_comp && conn->notified_upstream_port_connected &&
			!conn->notified_upstream_port_disconnected) {
		/* bt_component_port_disconnected() logs details */
		bt_component_port_disconnected(upstream_comp, upstream_port);
		conn->notified_upstream_port_disconnected = true;
	}

	BT_ASSERT(graph);

	if (conn->notified_graph_ports_connected &&
			!conn->notified_graph_ports_disconnected) {
		/* bt_graph_notify_ports_disconnected() logs details */
		bt_graph_notify_ports_disconnected(graph, upstream_comp,
			downstream_comp, upstream_port, downstream_port);
		conn->notified_graph_ports_disconnected = true;
	}

	/*
	 * It is safe to put the local port references now that we don't
	 * need them anymore. This could indeed destroy them.
	 */
	bt_object_put_ref(downstream_port);
	bt_object_put_ref(upstream_port);

	/*
	 * Because this connection is ended, finalize (cancel) each
	 * notification iterator created from it.
	 */
	for (i = 0; i < conn->iterators->len; i++) {
		struct bt_self_component_port_input_notification_iterator *iterator =
			g_ptr_array_index(conn->iterators, i);

		BT_LIB_LOGD("Finalizing notification iterator created by "
			"this ended connection: %![iter-]+i", iterator);
		bt_self_component_port_input_notification_iterator_finalize(
			iterator);

		/*
		 * Make sure this iterator does not try to remove itself
		 * from this connection's iterators on destruction
		 * because this connection won't exist anymore.
		 */
		bt_self_component_port_input_notification_iterator_set_connection(
			iterator, NULL);
	}

	g_ptr_array_set_size(conn->iterators, 0);

	if (try_remove_from_graph) {
		try_remove_connection_from_graph(conn);
	}
}

const struct bt_port_output *bt_connection_borrow_upstream_port_const(
		const struct bt_connection *connection)
{
	BT_ASSERT_PRE_NON_NULL(connection, "Connection");
	return (void *) connection->upstream_port;
}

const struct bt_port_input *bt_connection_borrow_downstream_port_const(
		const struct bt_connection *connection)
{
	BT_ASSERT_PRE_NON_NULL(connection, "Connection");
	return (void *) connection->downstream_port;
}

BT_HIDDEN
void bt_connection_remove_iterator(struct bt_connection *conn,
		struct bt_self_component_port_input_notification_iterator *iterator)
{
	g_ptr_array_remove(conn->iterators, iterator);
	BT_LIB_LOGV("Removed notification iterator from connection: "
		"%![conn-]+x, %![iter-]+i", conn, iterator);
	try_remove_connection_from_graph(conn);
}

bt_bool bt_connection_is_ended(const struct bt_connection *connection)
{
	return !connection->downstream_port && !connection->upstream_port;
}
