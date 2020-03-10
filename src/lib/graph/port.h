/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright 2017-2018 Philippe Proulx <pproulx@efficios.com>
 * Copyright 2017 Jérémie Galarneau <jeremie.galarneau@efficios.com>
 */

#ifndef BABELTRACE_GRAPH_PORT_INTERNAL_H
#define BABELTRACE_GRAPH_PORT_INTERNAL_H

#include <babeltrace2/graph/port.h>
#include "common/macros.h"

struct bt_port {
	struct bt_object base;
	enum bt_port_type type;
	GString *name;
	struct bt_connection *connection;
	void *user_data;
};

struct bt_component;

BT_HIDDEN
struct bt_port *bt_port_create(struct bt_component *parent_component,
		enum bt_port_type type, const char *name, void *user_data);

BT_HIDDEN
void bt_port_set_connection(struct bt_port *port,
		struct bt_connection *connection);

static inline
struct bt_component *bt_port_borrow_component_inline(const struct bt_port *port)
{
	BT_ASSERT_DBG(port);
	return (void *) bt_object_borrow_parent(&port->base);
}

static inline
const char *bt_port_type_string(enum bt_port_type port_type)
{
	switch (port_type) {
	case BT_PORT_TYPE_INPUT:
		return "INPUT";
	case BT_PORT_TYPE_OUTPUT:
		return "OUTPUT";
	default:
		return "(unknown)";
	}
}

#endif /* BABELTRACE_GRAPH_PORT_INTERNAL_H */
