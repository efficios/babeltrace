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

/* Types */
struct bt_query_executor;

/* Query status */
enum bt_query_status {
	BT_QUERY_STATUS_OK = 0,
	BT_QUERY_STATUS_AGAIN = 11,
	BT_QUERY_STATUS_EXECUTOR_CANCELED = 125,
	BT_QUERY_STATUS_ERROR = -1,
	BT_QUERY_STATUS_INVALID = -22,
	BT_QUERY_STATUS_INVALID_OBJECT = -23,
	BT_QUERY_STATUS_INVALID_PARAMS = -24,
	BT_QUERY_STATUS_NOMEM = -12,
};

/* Functions */
struct bt_query_executor *bt_query_executor_create(void);
enum bt_query_status bt_query_executor_query(
		struct bt_query_executor *query_executor,
		struct bt_component_class *component_class,
		const char *object, struct bt_value *params,
		struct bt_value **BTOUTVALUE);
enum bt_query_status bt_query_executor_cancel(
		struct bt_query_executor *query_executor);
int bt_query_executor_is_canceled(struct bt_query_executor *query_executor);
