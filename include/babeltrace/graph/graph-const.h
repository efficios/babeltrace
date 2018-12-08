#ifndef BABELTRACE_GRAPH_GRAPH_CONST_H
#define BABELTRACE_GRAPH_GRAPH_CONST_H

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

/* For bt_bool, bt_graph */
#include <babeltrace/types.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum bt_graph_status {
	BT_GRAPH_STATUS_OK = 0,
	BT_GRAPH_STATUS_END = 1,
	BT_GRAPH_STATUS_AGAIN = 11,
	BT_GRAPH_STATUS_COMPONENT_REFUSES_PORT_CONNECTION = 111,
	BT_GRAPH_STATUS_CANCELED = 125,
	BT_GRAPH_STATUS_ERROR = -1,
	BT_GRAPH_STATUS_NO_SINK = -6,
	BT_GRAPH_STATUS_NOMEM = -12,
} bt_graph_status;

extern bt_bool bt_graph_is_canceled(const bt_graph *graph);

extern void bt_graph_get_ref(const bt_graph *graph);

extern void bt_graph_put_ref(const bt_graph *graph);

#define BT_GRAPH_PUT_REF_AND_RESET(_var)	\
	do {					\
		bt_graph_put_ref(_var);		\
		(_var) = NULL;			\
	} while (0)

#define BT_GRAPH_MOVE_REF(_var_dst, _var_src)	\
	do {					\
		bt_graph_put_ref(_var_dst);	\
		(_var_dst) = (_var_src);	\
		(_var_src) = NULL;		\
	} while (0)

#ifdef __cplusplus
}
#endif

#endif /* BABELTRACE_GRAPH_GRAPH_CONST_H */
