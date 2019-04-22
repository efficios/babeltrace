/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2017 Philippe Proulx <pproulx@efficios.com>
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
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

/* From query-executor-const.h */

typedef enum bt_query_executor_status {
	BT_QUERY_EXECUTOR_STATUS_OK = 0,
	BT_QUERY_EXECUTOR_STATUS_AGAIN = 11,
	BT_QUERY_EXECUTOR_STATUS_UNSUPPORTED = 95,
	BT_QUERY_EXECUTOR_STATUS_CANCELED = 125,
	BT_QUERY_EXECUTOR_STATUS_ERROR = -1,
	BT_QUERY_EXECUTOR_STATUS_NOMEM = -12,
	BT_QUERY_EXECUTOR_STATUS_INVALID_OBJECT = -23,
	BT_QUERY_EXECUTOR_STATUS_INVALID_PARAMS = -24,
} bt_query_executor_status;

extern
bt_bool bt_query_executor_is_canceled(
		const bt_query_executor *query_executor);

extern void bt_query_executor_get_ref(
		const bt_query_executor *query_executor);

extern void bt_query_executor_put_ref(
		const bt_query_executor *query_executor);

/* From query-executor.h */

extern
bt_query_executor *bt_query_executor_create(void);

extern
bt_query_executor_status bt_query_executor_query(
		bt_query_executor *query_executor,
		const bt_component_class *component_class,
		const char *object, const bt_value *params,
		const bt_value **OUT_VALUE);

extern
bt_query_executor_status bt_query_executor_cancel(
		bt_query_executor *query_executor);
