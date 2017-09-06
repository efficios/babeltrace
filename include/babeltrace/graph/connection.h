#ifndef BABELTRACE_GRAPH_CONNECTION_H
#define BABELTRACE_GRAPH_CONNECTION_H

/*
 * BabelTrace - Babeltrace Component Connection Interface
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

/* For bt_bool */
#include <babeltrace/types.h>

#ifdef __cplusplus
extern "C" {
#endif

struct bt_component;
struct bt_connection;

enum bt_connection_status {
	BT_CONNECTION_STATUS_GRAPH_IS_CANCELED = 125,
	BT_CONNECTION_STATUS_OK = 0,
	BT_CONNECTION_STATUS_INVALID = -22,
	BT_CONNECTION_STATUS_ERROR = -1,
	BT_CONNECTION_STATUS_NOMEM = -12,
	BT_CONNECTION_STATUS_IS_ENDED = 104,
};

/* Returns the "downstream" input port. */
extern struct bt_port *bt_connection_get_downstream_port(
		struct bt_connection *connection);
/* Returns the "upstream" output port. */
extern struct bt_port *bt_connection_get_upstream_port(
		struct bt_connection *connection);

extern bt_bool bt_connection_is_ended(struct bt_connection *connection);

#ifdef __cplusplus
}
#endif

#endif /* BABELTRACE_GRAPH_CONNECTION_H */
