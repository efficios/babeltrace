#ifndef BABELTRACE2_GRAPH_QUERY_EXECUTOR_H
#define BABELTRACE2_GRAPH_QUERY_EXECUTOR_H

/*
 * Copyright (c) 2010-2019 EfficiOS Inc. and Linux Foundation
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

#ifndef __BT_IN_BABELTRACE_H
# error "Please include <babeltrace2/babeltrace.h> instead."
#endif

#include <babeltrace2/types.h>
#include <babeltrace2/logging.h>

#ifdef __cplusplus
extern "C" {
#endif

extern
bt_query_executor *bt_query_executor_create(
		const bt_component_class *component_class, const char *object,
		const bt_value *params);

typedef enum bt_query_executor_query_status {
	BT_QUERY_EXECUTOR_QUERY_STATUS_OK		= __BT_FUNC_STATUS_OK,
	BT_QUERY_EXECUTOR_QUERY_STATUS_AGAIN		= __BT_FUNC_STATUS_AGAIN,
	BT_QUERY_EXECUTOR_QUERY_STATUS_ERROR		= __BT_FUNC_STATUS_ERROR,
	BT_QUERY_EXECUTOR_QUERY_STATUS_MEMORY_ERROR	= __BT_FUNC_STATUS_MEMORY_ERROR,
	BT_QUERY_EXECUTOR_QUERY_STATUS_UNKNOWN_OBJECT	= __BT_FUNC_STATUS_UNKNOWN_OBJECT,
} bt_query_executor_query_status;

extern
bt_query_executor_query_status bt_query_executor_query(
		bt_query_executor *query_executor, const bt_value **result);

typedef enum bt_query_executor_add_interrupter_status {
	BT_QUERY_EXECUTOR_ADD_INTERRUPTER_STATUS_OK	= __BT_FUNC_STATUS_OK,
	BT_QUERY_EXECUTOR_ADD_INTERRUPTER_MEMORY_ERROR	= __BT_FUNC_STATUS_MEMORY_ERROR,
} bt_query_executor_add_interrupter_status;

extern bt_query_executor_add_interrupter_status
bt_query_executor_add_interrupter(bt_query_executor *query_executor,
		const bt_interrupter *interrupter);

extern void bt_query_executor_interrupt(bt_query_executor *query_executor);

typedef enum bt_query_executor_set_logging_level_status {
	BT_QUERY_EXECUTOR_SET_LOGGING_LEVEL_STATUS_OK	= __BT_FUNC_STATUS_OK,
} bt_query_executor_set_logging_level_status;

extern bt_query_executor_set_logging_level_status
bt_query_executor_set_logging_level(bt_query_executor *query_executor,
		bt_logging_level logging_level);

#ifdef __cplusplus
}
#endif

#endif /* BABELTRACE2_GRAPH_QUERY_EXECUTOR_H */
