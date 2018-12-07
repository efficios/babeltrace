#ifndef BABELTRACE_GRAPH_PORT_INTERNAL_H
#define BABELTRACE_GRAPH_PORT_INTERNAL_H

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

#include <babeltrace/graph/port-const.h>

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
	BT_ASSERT(port);
	return (void *) bt_object_borrow_parent(&port->base);
}

static inline
const char *bt_port_type_string(enum bt_port_type port_type)
{
	switch (port_type) {
	case BT_PORT_TYPE_INPUT:
		return "BT_PORT_TYPE_INPUT";
	case BT_PORT_TYPE_OUTPUT:
		return "BT_PORT_TYPE_OUTPUT";
	default:
		return "(unknown)";
	}
}

#endif /* BABELTRACE_GRAPH_PORT_INTERNAL_H */
