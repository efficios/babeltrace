#ifndef BABELTRACE_COMPONENT_PORT_INTERNAL_H
#define BABELTRACE_COMPONENT_PORT_INTERNAL_H

/*
 * BabelTrace - Babeltrace Component Port
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

#include <babeltrace/graph/port.h>

struct bt_port {
	struct bt_object base;
	enum bt_port_type type;
	GString *name;
	struct bt_connection *connection;
	void *user_data;
};

static inline
struct bt_port *bt_port_borrow_from_private(
		struct bt_private_port *private_port)
{
	return (void *) private_port;
}

static inline
struct bt_private_port *bt_private_port_from_port(
		struct bt_port *port)
{
	return (void *) port;
}

BT_HIDDEN
struct bt_port *bt_port_create(struct bt_component *parent_component,
		enum bt_port_type type, const char *name, void *user_data);

BT_HIDDEN
void bt_port_set_connection(struct bt_port *port,
		struct bt_connection *connection);

static inline
const char *bt_port_type_string(enum bt_port_type port_type)
{
	switch (port_type) {
	case BT_PORT_TYPE_INPUT:
		return "BT_PORT_TYPE_INPUT";
	case BT_PORT_TYPE_OUTPUT:
		return "BT_PORT_TYPE_OUTPUT";
	case BT_PORT_TYPE_UNKOWN:
		return "BT_PORT_TYPE_UNKOWN";
	default:
		return "(unknown)";
	}
}

#endif /* BABELTRACE_COMPONENT_PORT_INTERNAL_H */
