#ifndef BABELTRACE_GRAPH_CONNECTION_CONST_H
#define BABELTRACE_GRAPH_CONNECTION_CONST_H

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

/* For bt_bool, bt_port_input, bt_port_output, bt_connection */
#include <babeltrace/types.h>

#ifdef __cplusplus
extern "C" {
#endif

extern const bt_port_input *bt_connection_borrow_downstream_port_const(
		const bt_connection *connection);

extern const bt_port_output *bt_connection_borrow_upstream_port_const(
		const bt_connection *connection);

extern void bt_connection_get_ref(const bt_connection *connection);

extern void bt_connection_put_ref(const bt_connection *connection);

#define BT_CONNECTION_PUT_REF_AND_RESET(_var)		\
	do {						\
		bt_connection_put_ref(_var);		\
		(_var) = NULL;				\
	} while (0)

#define BT_CONNECTION_MOVE_REF(_var_dst, _var_src)	\
	do {						\
		bt_connection_put_ref(_var_dst);	\
		(_var_dst) = (_var_src);		\
		(_var_src) = NULL;			\
	} while (0)

#ifdef __cplusplus
}
#endif

#endif /* BABELTRACE_GRAPH_CONNECTION_CONST_H */
