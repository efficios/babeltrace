#ifndef BABELTRACE_GRAPH_GRAPH_H
#define BABELTRACE_GRAPH_GRAPH_H

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

/* For enum bt_component_status */
#include <babeltrace/graph/component-status.h>

#ifdef __cplusplus
extern "C" {
#endif

struct bt_graph;

enum bt_graph_status {
	BT_GRAPH_STATUS_COMPONENT_REFUSES_PORT_CONNECTION = 111,
	/** Canceled. */
	BT_GRAPH_STATUS_CANCELED = BT_COMPONENT_STATUS_GRAPH_IS_CANCELED,
	/** No sink can consume at the moment. */
	BT_GRAPH_STATUS_AGAIN = BT_COMPONENT_STATUS_AGAIN,
	/** Downstream component does not support multiple inputs. */
	BT_GRAPH_STATUS_END = BT_COMPONENT_STATUS_END,
	BT_GRAPH_STATUS_OK = BT_COMPONENT_STATUS_OK,
	/** Invalid arguments. */
	BT_GRAPH_STATUS_INVALID = BT_COMPONENT_STATUS_INVALID,
	/** No sink in graph. */
	BT_GRAPH_STATUS_NO_SINK = -6,
	/** General error. */
	BT_GRAPH_STATUS_ERROR = BT_COMPONENT_STATUS_ERROR,
	BT_GRAPH_STATUS_NOMEM = BT_COMPONENT_STATUS_NOMEM,
};

extern bt_bool bt_graph_is_canceled(struct bt_graph *graph);

#ifdef __cplusplus
}
#endif

#endif /* BABELTRACE_GRAPH_GRAPH_H */
