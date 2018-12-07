#ifndef BABELTRACE_GRAPH_PORT_CONST_H
#define BABELTRACE_GRAPH_PORT_CONST_H

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

#include <stdint.h>

/* For bt_bool */
#include <babeltrace/types.h>

#ifdef __cplusplus
extern "C" {
#endif

struct bt_port;
struct bt_connection;
struct bt_component;

enum bt_port_type {
	BT_PORT_TYPE_INPUT = 0,
	BT_PORT_TYPE_OUTPUT = 1,
};

extern const char *bt_port_get_name(const struct bt_port *port);

extern enum bt_port_type bt_port_get_type(const struct bt_port *port);

extern const struct bt_connection *bt_port_borrow_connection_const(
		const struct bt_port *port);

extern const struct bt_component *bt_port_borrow_component_const(
		const struct bt_port *port);

extern bt_bool bt_port_is_connected(const struct bt_port *port);

static inline
bt_bool bt_port_is_input(const struct bt_port *port)
{
	return bt_port_get_type(port) == BT_PORT_TYPE_INPUT;
}

static inline
bt_bool bt_port_is_output(const struct bt_port *port)
{
	return bt_port_get_type(port) == BT_PORT_TYPE_OUTPUT;
}

extern void bt_port_get_ref(const struct bt_port *port);

extern void bt_port_put_ref(const struct bt_port *port);

#define BT_PORT_PUT_REF_AND_RESET(_var)		\
	do {					\
		bt_port_put_ref(_var);		\
		(_var) = NULL;			\
	} while (0)

#define BT_PORT_MOVE_REF(_var_dst, _var_src)	\
	do {					\
		bt_port_put_ref(_var_dst);	\
		(_var_dst) = (_var_src);	\
		(_var_src) = NULL;		\
	} while (0)

#ifdef __cplusplus
}
#endif

#endif /* BABELTRACE_GRAPH_PORT_CONST_H */
