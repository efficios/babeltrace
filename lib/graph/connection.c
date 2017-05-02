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
#include <glib.h>

static
void bt_connection_destroy(struct bt_object *obj)
{
	struct bt_connection *connection = container_of(obj,
			struct bt_connection, base);

	/*
	 * No bt_put on ports as a connection only holds _weak_ references
	 * to them.
	 */
	g_free(connection);
}

struct bt_connection *bt_connection_from_private_connection(
		struct bt_private_connection *private_connection)
{
	return bt_get(bt_connection_from_private(private_connection));
}

BT_HIDDEN
struct bt_connection *bt_connection_create(
		struct bt_graph *graph,
		struct bt_port *upstream_port,
		struct bt_port *downstream_port)
{
	struct bt_connection *connection = NULL;

	if (bt_port_get_type(upstream_port) != BT_PORT_TYPE_OUTPUT) {
		goto end;
	}
	if (bt_port_get_type(downstream_port) != BT_PORT_TYPE_INPUT) {
		goto end;
	}

	connection = g_new0(struct bt_connection, 1);
	if (!connection) {
		goto end;
	}

	bt_object_init(connection, bt_connection_destroy);
	/* Weak references are taken, see comment in header. */
	connection->upstream_port = upstream_port;
	connection->downstream_port = downstream_port;
	bt_port_set_connection(upstream_port, connection);
	bt_port_set_connection(downstream_port, connection);
	bt_object_set_parent(connection, &graph->base);
end:
	return connection;
}

BT_HIDDEN
void bt_connection_disconnect_ports(struct bt_connection *conn)
{
	struct bt_component *downstream_comp = NULL;
	struct bt_component *upstream_comp = NULL;
	struct bt_port *downstream_port = conn->downstream_port;
	struct bt_port *upstream_port = conn->upstream_port;
	struct bt_graph *graph = (void *) bt_object_get_parent(conn);

	if (downstream_port) {
		downstream_comp = bt_port_get_component(downstream_port);
		bt_port_set_connection(downstream_port, NULL);
		conn->downstream_port = NULL;
	}

	if (upstream_port) {
		upstream_comp = bt_port_get_component(upstream_port);
		bt_port_set_connection(upstream_port, NULL);
		conn->upstream_port = NULL;
	}

	if (downstream_comp) {
		bt_component_port_disconnected(downstream_comp,
			downstream_port);
	}

	if (upstream_comp) {
		bt_component_port_disconnected(upstream_comp, upstream_port);
	}

	assert(graph);
	bt_graph_notify_ports_disconnected(graph, upstream_comp,
		downstream_comp, upstream_port, downstream_port);
	bt_put(downstream_comp);
	bt_put(upstream_comp);
	bt_put(graph);
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

struct bt_notification_iterator *
bt_private_connection_create_notification_iterator(
		struct bt_private_connection *private_connection,
		const enum bt_notification_type *notification_types)
{
	enum bt_notification_iterator_status ret_iterator;
	enum bt_component_class_type upstream_comp_class_type;
	struct bt_notification_iterator *iterator = NULL;
	struct bt_port *upstream_port = NULL;
	struct bt_component *upstream_component = NULL;
	struct bt_component_class *upstream_comp_class = NULL;
	struct bt_connection *connection = NULL;
	bt_component_class_notification_iterator_init_method init_method = NULL;
	static const enum bt_notification_type all_notif_types[] = {
		BT_NOTIFICATION_TYPE_ALL,
		BT_NOTIFICATION_TYPE_SENTINEL,
	};

	if (!private_connection) {
		goto error;
	}

	if (!notification_types) {
		notification_types = all_notif_types;
	}

	connection = bt_connection_from_private(private_connection);

	if (!connection->upstream_port || !connection->downstream_port) {
		goto error;
	}

	upstream_port = connection->upstream_port;
	assert(upstream_port);
	upstream_component = bt_port_get_component(upstream_port);
	assert(upstream_component);
	upstream_comp_class = upstream_component->class;

	if (!upstream_component) {
		goto error;
	}

	upstream_comp_class_type =
		bt_component_get_class_type(upstream_component);
	if (upstream_comp_class_type != BT_COMPONENT_CLASS_TYPE_SOURCE &&
			upstream_comp_class_type != BT_COMPONENT_CLASS_TYPE_FILTER) {
		/* Unsupported operation. */
		goto error;
	}

	iterator = bt_notification_iterator_create(upstream_component,
		upstream_port, notification_types);
	if (!iterator) {
		goto error;
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
		assert(0);
	}

	if (init_method) {
		enum bt_notification_iterator_status status = init_method(
			bt_private_notification_iterator_from_notification_iterator(iterator),
			bt_private_port_from_port(upstream_port));
		if (status < 0) {
			goto error;
		}
	}

	ret_iterator = bt_notification_iterator_validate(iterator);
	if (ret_iterator != BT_NOTIFICATION_ITERATOR_STATUS_OK) {
		goto error;
	}

	goto end;

error:
	BT_PUT(iterator);

end:
	bt_put(upstream_component);
	return iterator;
}
