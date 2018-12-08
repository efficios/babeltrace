#ifndef BABELTRACE_GRAPH_COMPONENT_CLASS_H
#define BABELTRACE_GRAPH_COMPONENT_CLASS_H

/*
 * Copyright 2017-2018 Philippe Proulx <pproulx@efficios.com>
 * Copyright 2016 Jérémie Galarneau <jeremie.galarneau@efficios.com>
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

/* For enum bt_query_executor_status */
#include <babeltrace/graph/query-executor-const.h>

/* For bt_component_class */
#include <babeltrace/types.h>

#ifdef __cplusplus
extern "C" {
#endif

enum bt_query_status {
	BT_QUERY_STATUS_OK = BT_QUERY_EXECUTOR_STATUS_OK,
	BT_QUERY_STATUS_AGAIN = BT_QUERY_EXECUTOR_STATUS_AGAIN,
	BT_QUERY_STATUS_ERROR = BT_QUERY_EXECUTOR_STATUS_ERROR,
	BT_QUERY_STATUS_NOMEM = BT_QUERY_EXECUTOR_STATUS_NOMEM,
	BT_QUERY_STATUS_INVALID_OBJECT = BT_QUERY_EXECUTOR_STATUS_INVALID_OBJECT,
	BT_QUERY_STATUS_INVALID_PARAMS = BT_QUERY_EXECUTOR_STATUS_INVALID_PARAMS,
};

extern int bt_component_class_set_description(
		bt_component_class *component_class,
		const char *description);

extern int bt_component_class_set_help(
		bt_component_class *component_class,
		const char *help);

#ifdef __cplusplus
}
#endif

#endif /* BABELTRACE_GRAPH_COMPONENT_CLASS_H */
