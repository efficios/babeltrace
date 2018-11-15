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

#define BT_LOG_TAG "PORT"
#include <babeltrace/lib-logging-internal.h>

#include <babeltrace/graph/port.h>
#include <babeltrace/graph/component-internal.h>
#include <babeltrace/graph/port-internal.h>
#include <babeltrace/graph/connection-internal.h>
#include <babeltrace/object-internal.h>
#include <babeltrace/compiler-internal.h>
#include <babeltrace/assert-internal.h>

static
void bt_port_destroy(struct bt_object *obj)
{
	struct bt_port *port = container_of(obj, struct bt_port, base);

	BT_LOGD("Destroying port: addr=%p, name=\"%s\", comp-addr=%p",
		port, bt_port_get_name(port), obj->parent);

	if (port->name) {
		g_string_free(port->name, TRUE);
	}

	g_free(port);
}

struct bt_port *bt_port_borrow_from_private(
		struct bt_private_port *private_port)
{
	return (void *) private_port;
}

BT_HIDDEN
struct bt_port *bt_port_create(struct bt_component *parent_component,
		enum bt_port_type type, const char *name, void *user_data)
{
	struct bt_port *port = NULL;

	BT_ASSERT(name);
	BT_ASSERT(parent_component);
	BT_ASSERT(type == BT_PORT_TYPE_INPUT || type == BT_PORT_TYPE_OUTPUT);

	if (strlen(name) == 0) {
		BT_LOGW_STR("Invalid parameter: name is an empty string.");
		goto end;
	}

	port = g_new0(struct bt_port, 1);
	if (!port) {
		BT_LOGE_STR("Failed to allocate one port.");
		goto end;
	}

	BT_LOGD("Creating port for component: "
		"comp-addr=%p, comp-name=\"%s\", port-type=%s, "
		"port-name=\"%s\"",
		parent_component, bt_component_get_name(parent_component),
		bt_port_type_string(type), name);

	bt_object_init_shared_with_parent(&port->base, bt_port_destroy);
	port->name = g_string_new(name);
	if (!port->name) {
		BT_LOGE_STR("Failed to allocate one GString.");
		BT_OBJECT_PUT_REF_AND_RESET(port);
		goto end;
	}

	port->type = type;
	port->user_data = user_data;
	bt_object_set_parent(&port->base, &parent_component->base);
	BT_LOGD("Created port for component: "
		"comp-addr=%p, comp-name=\"%s\", port-type=%s, "
		"port-name=\"%s\", port-addr=%p",
		parent_component, bt_component_get_name(parent_component),
		bt_port_type_string(type), name, port);

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

	if (!port) {
		BT_LOGW_STR("Invalid parameter: port is NULL.");
		goto end;
	}

	if (!port->connection) {
		/* Not an error: means disconnected */
		goto end;
	}

	connection = bt_object_get_ref(port->connection);

end:
	return connection;
}

struct bt_component *bt_port_get_component(struct bt_port *port)
{
	return (struct bt_component *) bt_object_get_parent(&port->base);
}

struct bt_private_connection *bt_private_port_get_private_connection(
		struct bt_private_port *private_port)
{
	return bt_private_connection_from_connection(bt_port_get_connection(
		bt_port_borrow_from_private(private_port)));
}

struct bt_private_component *bt_private_port_get_private_component(
		struct bt_private_port *private_port)
{
	return bt_private_component_from_component(bt_port_get_component(
		bt_port_borrow_from_private(private_port)));
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
	BT_LOGV("Set port's connection: "
		"port-addr=%p, port-name=\"%s\", conn-addr=%p",
		port, bt_port_get_name(port), connection);
}

enum bt_port_status bt_private_port_remove_from_component(
		struct bt_private_port *private_port)
{
	enum bt_port_status status = BT_PORT_STATUS_OK;
	struct bt_port *port = bt_port_borrow_from_private(private_port);
	struct bt_component *comp = NULL;
	enum bt_component_status comp_status;

	if (!port) {
		BT_LOGW_STR("Invalid parameter: private port is NULL.");
		status = BT_PORT_STATUS_INVALID;
		goto end;
	}

	comp = (void *) bt_object_get_parent(&port->base);
	if (!comp) {
		BT_LOGV("Port already removed from its component: "
			"port-addr=%p, port-name=\"%s\", ",
			port, bt_port_get_name(port));
		goto end;
	}

	/* bt_component_remove_port() logs details */
	comp_status = bt_component_remove_port(comp, port);
	BT_ASSERT(comp_status != BT_COMPONENT_STATUS_INVALID);
	if (comp_status < 0) {
		status = BT_PORT_STATUS_ERROR;
		goto end;
	}

end:
	bt_object_put_ref(comp);
	return status;
}

enum bt_port_status bt_port_disconnect(struct bt_port *port)
{
	enum bt_port_status status = BT_PORT_STATUS_OK;

	if (!port) {
		BT_LOGW_STR("Invalid parameter: port is NULL.");
		status = BT_PORT_STATUS_INVALID;
		goto end;
	}

	if (port->connection) {
		bt_connection_end(port->connection, true);
		BT_LOGV("Disconnected port: "
			"port-addr=%p, port-name=\"%s\"",
			port, bt_port_get_name(port));
	}

end:
	return status;
}

bt_bool bt_port_is_connected(struct bt_port *port)
{
	int ret;

	if (!port) {
		BT_LOGW_STR("Invalid parameter: port is NULL.");
		ret = -1;
		goto end;
	}

	ret = port->connection ? 1 : 0;

end:
	return ret;
}

void *bt_private_port_get_user_data(
		struct bt_private_port *private_port)
{
	return private_port ?
		bt_port_borrow_from_private(private_port)->user_data : NULL;
}
