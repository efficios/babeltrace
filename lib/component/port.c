/*
 * port.c
 *
 * Babeltrace Port
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
#include <babeltrace/graph/port-internal.h>
#include <babeltrace/graph/connection-internal.h>
#include <babeltrace/object-internal.h>
#include <babeltrace/compiler-internal.h>

static
void bt_port_destroy(struct bt_object *obj)
{
	struct bt_port *port = container_of(obj, struct bt_port, base);

	if (port->name) {
		g_string_free(port->name, TRUE);
	}
	g_free(port);
}

struct bt_port *bt_port_from_private_port(
		struct bt_private_port *private_port)
{
	return bt_get(bt_port_from_private(private_port));
}

BT_HIDDEN
struct bt_port *bt_port_create(struct bt_component *parent_component,
		enum bt_port_type type, const char *name)
{
	struct bt_port *port = NULL;

	assert(name);
	assert(parent_component);
	assert(type == BT_PORT_TYPE_INPUT || type == BT_PORT_TYPE_OUTPUT);

	if (strlen(name) == 0) {
		goto end;
	}

	port = g_new0(struct bt_port, 1);
	if (!port) {
		goto end;
	}

	bt_object_init(port, bt_port_destroy);
	port->name = g_string_new(name);
	if (!port->name) {
		BT_PUT(port);
		goto end;
	}

	port->type = type;

	bt_object_set_parent(port, &parent_component->base);
end:
	return port;
}

const char *bt_port_get_name(struct bt_port *port)
{
	return port ? port->name->str : NULL;
}

enum bt_port_type bt_port_get_type(struct bt_port *port)
{
	return port ? port->type : BT_PORT_TYPE_UNKOWN;
}

struct bt_connection *bt_port_get_connection(struct bt_port *port)
{
	struct bt_connection *connection = NULL;

	if (!port || !port->connection) {
		goto end;
	}

	connection = bt_get(port->connection);
end:
	return connection;
}

struct bt_component *bt_port_get_component(struct bt_port *port)
{
	return (struct bt_component *) bt_object_get_parent(port);
}

struct bt_private_connection *bt_private_port_get_private_connection(
		struct bt_private_port *private_port)
{
	return bt_private_connection_from_connection(bt_port_get_connection(
		bt_port_from_private(private_port)));
}

struct bt_private_component *bt_private_port_get_private_component(
		struct bt_private_port *private_port)
{
	return bt_private_component_from_component(bt_port_get_component(
		bt_port_from_private(private_port)));
}

BT_HIDDEN
void bt_port_set_connection(struct bt_port *port,
		struct bt_connection *connection)
{
	/*
	 * Don't take a reference on connection as its existence is
	 * guaranteed by the existence of the graph in which the
	 * connection exists.
	 */
	port->connection = connection;
}

int bt_private_port_remove_from_component(
		struct bt_private_port *private_port)
{
	int ret = 0;
	struct bt_port *port = bt_port_from_private(private_port);
	struct bt_component *comp = NULL;

	if (!port) {
		ret = -1;
		goto end;
	}

	comp = (void *) bt_object_get_parent(port);
	ret = bt_component_remove_port(comp, port);

end:
	bt_put(comp);
	return ret;
}

int bt_port_disconnect(struct bt_port *port)
{
	int ret = 0;

	if (!port) {
		ret = -1;
		goto end;
	}

	if (port->connection) {
		bt_connection_disconnect_ports(port->connection);
	}

end:
	return ret;
}

int bt_port_is_connected(struct bt_port *port)
{
	int ret;

	if (!port) {
		ret = -1;
		goto end;
	}

	ret = port->connection ? 1 : 0;

end:
	return ret;
}

int bt_private_port_set_user_data(
		struct bt_private_port *private_port, void *user_data)
{
	int ret = 0;

	if (!private_port) {
		ret = -1;
		goto end;
	}

	bt_port_from_private(private_port)->user_data = user_data;

end:
	return ret;
}

void *bt_private_port_get_user_data(
		struct bt_private_port *private_port)
{
	return private_port ?
		bt_port_from_private(private_port)->user_data : NULL;
}
