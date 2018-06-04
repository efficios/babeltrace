#ifndef BABELTRACE_GRAPH_PRIVATE_PORT_H
#define BABELTRACE_GRAPH_PRIVATE_PORT_H

/*
 * Copyright 2017 Philippe Proulx <pproulx@efficios.com>
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

#ifdef __cplusplus
extern "C" {
#endif

struct bt_port;
struct bt_private_port;
struct bt_private_component;
struct bt_private_connection;

struct bt_port *bt_port_borrow_from_private(
		struct bt_private_port *private_port);
extern struct bt_private_connection *bt_private_port_get_private_connection(
		struct bt_private_port *private_port);
extern struct bt_private_component *bt_private_port_get_private_component(
		struct bt_private_port *private_port);
extern enum bt_port_status bt_private_port_remove_from_component(
		struct bt_private_port *private_port);
extern void *bt_private_port_get_user_data(
		struct bt_private_port *private_port);

#ifdef __cplusplus
}
#endif

#endif /* BABELTRACE_GRAPH_PRIVATE_PORT_H */
