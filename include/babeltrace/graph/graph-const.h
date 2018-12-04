#ifndef BABELTRACE_GRAPH_GRAPH_CONST_H
#define BABELTRACE_GRAPH_GRAPH_CONST_H

/*
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

struct bt_graph;

enum bt_graph_status {
	BT_GRAPH_STATUS_OK = 0,
	BT_GRAPH_STATUS_END = 1,
	BT_GRAPH_STATUS_AGAIN = 11,
	BT_GRAPH_STATUS_COMPONENT_REFUSES_PORT_CONNECTION = 111,
	BT_GRAPH_STATUS_CANCELED = 125,
	BT_GRAPH_STATUS_ERROR = -1,
	BT_GRAPH_STATUS_NO_SINK = -6,
	BT_GRAPH_STATUS_NOMEM = -12,
};

extern bt_bool bt_graph_is_canceled(const struct bt_graph *graph);

#ifdef __cplusplus
}
#endif

#endif /* BABELTRACE_GRAPH_GRAPH_CONST_H */
