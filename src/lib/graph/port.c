/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright 2017-2018 Philippe Proulx <pproulx@efficios.com>
 * Copyright 2017 Jérémie Galarneau <jeremie.galarneau@efficios.com>
 */

#define BT_LOG_TAG "LIB/PORT"
#include "lib/logging.h"

#include "common/assert.h"
#include "lib/assert-cond.h"
#include <babeltrace2/graph/port.h>
#include <babeltrace2/graph/self-component-port.h>
#include "lib/object.h"
#include "compat/compiler.h"

#include "component.h"
#include "connection.h"
#include "port.h"

static
void destroy_port(struct bt_object *obj)
{
	struct bt_port *port = (void *) obj;

	BT_LIB_LOGI("Destroying port: %!+p", port);

	if (port->name) {
		g_string_free(port->name, TRUE);
		port->name = NULL;
	}

	g_free(port);
}

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
		BT_LIB_LOGE_APPEND_CAUSE("Failed to allocate one port.");
		goto end;
	}

	BT_LIB_LOGI("Creating port for component: %![comp-]+c, port-type=%s, "
		"port-name=\"%s\"", parent_component, bt_port_type_string(type),
		name);
	bt_object_init_shared_with_parent(&port->base, destroy_port);
	port->name = g_string_new(name);
	if (!port->name) {
		BT_LIB_LOGE_APPEND_CAUSE("Failed to allocate one GString.");
		BT_OBJECT_PUT_REF_AND_RESET(port);
		goto end;
	}

	port->type = type;
	port->user_data = user_data;
	bt_object_set_parent(&port->base, &parent_component->base);
	BT_LIB_LOGI("Created port for component: "
		"%![comp-]+c, %![port-]+p", parent_component, port);

end:
	return port;
}

BT_EXPORT
const char *bt_port_get_name(const struct bt_port *port)
{
	BT_ASSERT_PRE_DEV_PORT_NON_NULL(port);
	return port->name->str;
}

BT_EXPORT
enum bt_port_type bt_port_get_type(const struct bt_port *port)
{
	BT_ASSERT_PRE_DEV_PORT_NON_NULL(port);
	return port->type;
}

BT_EXPORT
const struct bt_connection *bt_port_borrow_connection_const(
		const struct bt_port *port)
{
	BT_ASSERT_PRE_DEV_PORT_NON_NULL(port);
	return port->connection;
}

BT_EXPORT
const struct bt_component *bt_port_borrow_component_const(
		const struct bt_port *port)
{
	BT_ASSERT_PRE_DEV_PORT_NON_NULL(port);
	return bt_port_borrow_component_inline(port);
}

BT_EXPORT
struct bt_self_component *bt_self_component_port_borrow_component(
		struct bt_self_component_port *port)
{
	BT_ASSERT_PRE_DEV_PORT_NON_NULL(port);
	return (void *) bt_object_borrow_parent((void *) port);
}

void bt_port_set_connection(struct bt_port *port,
		struct bt_connection *connection)
{
	/*
	 * Don't take a reference on connection as its existence is
	 * guaranteed by the existence of the graph in which the
	 * connection exists.
	 */
	port->connection = connection;
	BT_LIB_LOGI("Set port's connection: %![port-]+p, %![conn-]+x", port,
		connection);
}

BT_EXPORT
bt_bool bt_port_is_connected(const struct bt_port *port)
{
	BT_ASSERT_PRE_DEV_PORT_NON_NULL(port);
	return port->connection ? BT_TRUE : BT_FALSE;
}

BT_EXPORT
void *bt_self_component_port_get_data(const struct bt_self_component_port *port)
{
	BT_ASSERT_PRE_DEV_PORT_NON_NULL(port);
	return ((struct bt_port *) port)->user_data;
}

BT_EXPORT
void bt_port_get_ref(const struct bt_port *port)
{
	bt_object_get_ref(port);
}

BT_EXPORT
void bt_port_put_ref(const struct bt_port *port)
{
	bt_object_put_ref(port);
}

BT_EXPORT
void bt_port_input_get_ref(const struct bt_port_input *port_input)
{
	bt_object_get_ref(port_input);
}

BT_EXPORT
void bt_port_input_put_ref(const struct bt_port_input *port_input)
{
	bt_object_put_ref(port_input);
}

BT_EXPORT
void bt_port_output_get_ref(const struct bt_port_output *port_output)
{
	bt_object_get_ref(port_output);
}

BT_EXPORT
void bt_port_output_put_ref(const struct bt_port_output *port_output)
{
	bt_object_put_ref(port_output);
}
