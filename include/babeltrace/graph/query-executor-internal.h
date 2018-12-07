#ifndef BABELTRACE_GRAPH_QUERY_EXECUTOR_INTERNAL_H
#define BABELTRACE_GRAPH_QUERY_EXECUTOR_INTERNAL_H

/*
 * Copyright 2017-2018 Philippe Proulx <pproulx@efficios.com>
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

#include <babeltrace/types.h>
#include <babeltrace/object-internal.h>
#include <babeltrace/graph/query-executor.h>
#include <babeltrace/graph/component-class.h>

struct bt_query_executor {
	struct bt_object base;
	bool canceled;
};

static inline const char *bt_query_status_string(enum bt_query_status status)
{
	switch (status) {
	case BT_QUERY_STATUS_OK:
		return "BT_QUERY_STATUS_OK";
	case BT_QUERY_STATUS_AGAIN:
		return "BT_QUERY_STATUS_AGAIN";
	case BT_QUERY_STATUS_ERROR:
		return "BT_QUERY_STATUS_ERROR";
	case BT_QUERY_STATUS_INVALID_OBJECT:
		return "BT_QUERY_STATUS_INVALID_OBJECT";
	case BT_QUERY_STATUS_INVALID_PARAMS:
		return "BT_QUERY_STATUS_INVALID_PARAMS";
	case BT_QUERY_STATUS_NOMEM:
		return "BT_QUERY_STATUS_NOMEM";
	default:
		return "(unknown)";
	}
};

static inline const char *bt_query_executor_status_string(
		enum bt_query_executor_status status)
{
	switch (status) {
	case BT_QUERY_EXECUTOR_STATUS_OK:
		return "BT_QUERY_EXECUTOR_STATUS_OK";
	case BT_QUERY_EXECUTOR_STATUS_AGAIN:
		return "BT_QUERY_EXECUTOR_STATUS_AGAIN";
	case BT_QUERY_EXECUTOR_STATUS_CANCELED:
		return "BT_QUERY_EXECUTOR_STATUS_CANCELED";
	case BT_QUERY_EXECUTOR_STATUS_UNSUPPORTED:
		return "BT_QUERY_EXECUTOR_STATUS_UNSUPPORTED";
	case BT_QUERY_EXECUTOR_STATUS_ERROR:
		return "BT_QUERY_EXECUTOR_STATUS_ERROR";
	case BT_QUERY_EXECUTOR_STATUS_INVALID_OBJECT:
		return "BT_QUERY_EXECUTOR_STATUS_INVALID_OBJECT";
	case BT_QUERY_EXECUTOR_STATUS_INVALID_PARAMS:
		return "BT_QUERY_EXECUTOR_STATUS_INVALID_PARAMS";
	case BT_QUERY_EXECUTOR_STATUS_NOMEM:
		return "BT_QUERY_EXECUTOR_STATUS_NOMEM";
	default:
		return "(unknown)";
	}
};

#endif /* BABELTRACE_GRAPH_QUERY_EXECUTOR_INTERNAL_H */
