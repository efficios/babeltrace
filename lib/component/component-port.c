/*
 * component-port.c
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

#include <babeltrace/component/component-internal.h>
#include <babeltrace/component/component-port-internal.h>
#include <babeltrace/object-internal.h>
#include <babeltrace/compiler.h>

static
void bt_port_destroy(struct bt_object *obj)
{
	struct bt_port *port = container_of(obj, struct bt_port, base);

	if (port->name) {
		g_string_free(port->name, TRUE);
	}
	if (port->connections) {
		g_ptr_array_free(port->connections, TRUE);
	}
	g_free(port);
}

BT_HIDDEN
struct bt_port *bt_port_create(struct bt_component *parent_component,
		enum bt_port_type type, const char *name)
{
	struct bt_port *port;

	assert(name);
	assert(parent_component);
	assert(type == BT_PORT_TYPE_INPUT || type == BT_PORT_TYPE_OUTPUT);

	if (*name == '\0') {
		port = NULL;
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
	port->connections = g_ptr_array_new();
	if (!port->connections) {
		BT_PUT(port);
		goto end;
	}

	port->max_connection_count = 1;

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

enum bt_port_status bt_port_get_connection_count(struct bt_port *port,
		uint64_t *count)
{
	enum bt_port_status status = BT_PORT_STATUS_OK;

	if (!port || !count) {
		status = BT_PORT_STATUS_INVALID;
		goto end;
	}

	*count = (uint64_t) port->connections->len;
end:
	return status;
}

struct bt_connection *bt_port_get_connection(struct bt_port *port, int index)
{
	struct bt_connection *connection;

	if (!port || index < 0 || index >= port->connections->len) {
		connection = NULL;
		goto end;
	}

	connection = bt_get(g_ptr_array_index(port->connections, index));
end:
	return connection;
}

struct bt_component *bt_port_get_component(struct bt_port *port)
{
	return (struct bt_component *) bt_object_get_parent(port);
}

BT_HIDDEN
int bt_port_add_connection(struct bt_port *port,
		struct bt_connection *connection)
{
	int ret = 0;

	if (port->connections->len == port->max_connection_count) {
		ret = -1;
		goto end;
	}

	/*
	 * Don't take a reference on connection as its existence is guaranteed
	 * by the existence of the graph in which the connection exists.
	 */
	g_ptr_array_add(port->connections, connection);
end:
	return ret;
}

enum bt_port_status bt_port_get_maximum_connection_count(
		struct bt_port *port, uint64_t *count)
{
	enum bt_port_status status = BT_PORT_STATUS_OK;

	if (!port || !count) {
		status = BT_PORT_STATUS_INVALID;
		goto end;
	}

	*count = port->max_connection_count;
end:
	return status;
}

enum bt_port_status bt_port_set_maximum_connection_count(
		struct bt_port *port, uint64_t count)
{
	enum bt_port_status status = BT_PORT_STATUS_OK;

	if (!port || count < port->connections->len || count == 0) {
		status = BT_PORT_STATUS_INVALID;
		goto end;
	}

	port->max_connection_count = count;
end:
	return status;
}
