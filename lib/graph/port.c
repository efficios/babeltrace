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

#define BT_LOG_TAG "PORT"
#include <babeltrace/lib-logging-internal.h>

#include <babeltrace/graph/port-const.h>
#include <babeltrace/graph/port-input-const.h>
#include <babeltrace/graph/port-output-const.h>
#include <babeltrace/graph/self-component-port.h>
#include <babeltrace/graph/self-component-port-input.h>
#include <babeltrace/graph/self-component-port-output.h>
#include <babeltrace/graph/component-internal.h>
#include <babeltrace/graph/port-internal.h>
#include <babeltrace/graph/connection-internal.h>
#include <babeltrace/object-internal.h>
#include <babeltrace/object.h>
#include <babeltrace/compiler-internal.h>
#include <babeltrace/assert-internal.h>
#include <babeltrace/assert-pre-internal.h>

static
void destroy_port(struct bt_object *obj)
{
	struct bt_port *port = (void *) obj;

	BT_LIB_LOGD("Destroying port: %!+p", port);

	if (port->name) {
		g_string_free(port->name, TRUE);
		port->name = NULL;
	}

	g_free(port);
}

BT_HIDDEN
struct bt_port *bt_port_create(struct bt_component *parent_component,
		enum bt_port_type type, const char *name, void *user_data)
{
	struct bt_port *port = NULL;

	BT_ASSERT(name);
	BT_ASSERT(parent_component);
	BT_ASSERT(type == BT_PORT_TYPE_INPUT || type == BT_PORT_TYPE_OUTPUT);
	BT_ASSERT(strlen(name) > 0);
	port = g_new0(struct bt_port, 1);
	if (!port) {
		BT_LOGE_STR("Failed to allocate one port.");
		goto end;
	}

	BT_LIB_LOGD("Creating port for component: %![comp-]+c, port-type=%s, "
		"port-name=\"%s\"", parent_component, bt_port_type_string(type),
		name);
	bt_object_init_shared_with_parent(&port->base, destroy_port);
	port->name = g_string_new(name);
	if (!port->name) {
		BT_LOGE_STR("Failed to allocate one GString.");
		BT_OBJECT_PUT_REF_AND_RESET(port);
		goto end;
	}

	port->type = type;
	port->user_data = user_data;
	bt_object_set_parent(&port->base, &parent_component->base);
	BT_LIB_LOGD("Created port for component: "
		"%![comp-]+c, %![port-]+p", parent_component, port);

end:
	return port;
}

const char *bt_port_get_name(const struct bt_port *port)
{
	BT_ASSERT_PRE_NON_NULL(port, "Port");
	return port->name->str;
}

enum bt_port_type bt_port_get_type(const struct bt_port *port)
{
	BT_ASSERT_PRE_NON_NULL(port, "Port");
	return port->type;
}

const struct bt_connection *bt_port_borrow_connection_const(
		const struct bt_port *port)
{
	BT_ASSERT_PRE_NON_NULL(port, "Port");
	return port->connection;
}

const struct bt_component *bt_port_borrow_component_const(
		const struct bt_port *port)
{
	BT_ASSERT_PRE_NON_NULL(port, "Port");
	return bt_port_borrow_component_inline(port);
}

struct bt_self_component *bt_self_component_port_borrow_component(
		struct bt_self_component_port *port)
{
	BT_ASSERT_PRE_NON_NULL(port, "Port");
	return (void *) bt_object_borrow_parent((void *) port);
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
	BT_LIB_LOGV("Set port's connection: %![port-]+p, %![conn-]+x", port,
		connection);
}

enum bt_self_component_port_status bt_self_component_port_remove_from_component(
		struct bt_self_component_port *self_port)
{
	struct bt_port *port = (void *) self_port;
	struct bt_component *comp = NULL;

	BT_ASSERT_PRE_NON_NULL(port, "Port");

	comp = (void *) bt_object_borrow_parent(&port->base);
	if (!comp) {
		BT_LIB_LOGV("Port already removed from its component: %!+p",
			port);
		goto end;
	}

	/* bt_component_remove_port() logs details */
	bt_component_remove_port(comp, port);

end:
	return BT_SELF_PORT_STATUS_OK;
}

bt_bool bt_port_is_connected(const struct bt_port *port)
{
	BT_ASSERT_PRE_NON_NULL(port, "Port");
	return port->connection ? BT_TRUE : BT_FALSE;
}

void *bt_self_component_port_get_data(const struct bt_self_component_port *port)
{
	BT_ASSERT_PRE_NON_NULL(port, "Port");
	return ((struct bt_port *) port)->user_data;
}
