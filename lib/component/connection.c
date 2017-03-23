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

#include <babeltrace/component/component-internal.h>
#include <babeltrace/component/component-source-internal.h>
#include <babeltrace/component/component-filter-internal.h>
#include <babeltrace/component/connection-internal.h>
#include <babeltrace/component/graph-internal.h>
#include <babeltrace/component/port-internal.h>
#include <babeltrace/object-internal.h>
#include <babeltrace/compiler.h>
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
void bt_connection_disconnect_ports(struct bt_connection *conn,
		struct bt_component *acting_comp)
{
	struct bt_component *downstream_comp = NULL;
	struct bt_component *upstream_comp = NULL;
	struct bt_port *downstream_port = conn->downstream_port;
	struct bt_port *upstream_port = conn->upstream_port;

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

	if (downstream_comp && downstream_comp != acting_comp &&
			downstream_comp->class->methods.port_disconnected) {
		bt_component_port_disconnected(downstream_comp,
			downstream_port);
	}

	if (upstream_comp && upstream_comp != acting_comp &&
			upstream_comp->class->methods.port_disconnected) {
		bt_component_port_disconnected(upstream_comp, upstream_port);
	}

	// TODO: notify graph user: component's port disconnected

	bt_put(downstream_comp);
	bt_put(upstream_comp);
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
bt_connection_create_notification_iterator(struct bt_connection *connection)
{
	struct bt_component *upstream_component = NULL;
	struct bt_notification_iterator *it = NULL;

	if (!connection) {
		goto end;
	}

	if (!connection->upstream_port || !connection->downstream_port) {
		goto end;
	}

	upstream_component = bt_port_get_component(connection->upstream_port);
	assert(upstream_component);

	switch (bt_component_get_class_type(upstream_component)) {
	case BT_COMPONENT_CLASS_TYPE_SOURCE:
		it = bt_component_source_create_notification_iterator(
				upstream_component);
		break;
	case BT_COMPONENT_CLASS_TYPE_FILTER:
		it = bt_component_filter_create_notification_iterator(
				upstream_component);
		break;
	default:
		goto end;
	}
end:
	bt_put(upstream_component);
	return it;
}
