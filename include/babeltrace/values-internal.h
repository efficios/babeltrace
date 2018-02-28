#ifndef BABELTRACE_VALUES_INTERNAL_H
#define BABELTRACE_VALUES_INTERNAL_H

/*
 * Babeltrace - Value objects
 *
 * Copyright (c) 2015-2017 EfficiOS Inc. and Linux Foundation
 * Copyright (c) 2015-2017 Philippe Proulx <pproulx@efficios.com>
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

#include <babeltrace/values.h>

BT_HIDDEN
enum bt_value_status _bt_value_freeze(struct bt_value *object);

#ifdef BT_DEV_MODE
# define bt_value_freeze	_bt_value_freeze
#else
# define bt_value_freeze(_value)
#endif /* BT_DEV_MODE */

static inline
const char *bt_value_status_string(enum bt_value_status status)
{
	switch (status) {
	case BT_VALUE_STATUS_CANCELED:
		return "BT_VALUE_STATUS_CANCELED";
	case BT_VALUE_STATUS_INVAL:
		return "BT_VALUE_STATUS_INVAL";
	case BT_VALUE_STATUS_ERROR:
		return "BT_VALUE_STATUS_ERROR";
	case BT_VALUE_STATUS_OK:
		return "BT_VALUE_STATUS_OK";
	default:
		return "(unknown)";
	}
};

static inline
const char *bt_value_type_string(enum bt_value_type type)
{
	switch (type) {
	case BT_VALUE_TYPE_UNKNOWN:
		return "BT_VALUE_TYPE_UNKNOWN";
	case BT_VALUE_TYPE_NULL:
		return "BT_VALUE_TYPE_NULL";
	case BT_VALUE_TYPE_BOOL:
		return "BT_VALUE_TYPE_BOOL";
	case BT_VALUE_TYPE_INTEGER:
		return "BT_VALUE_TYPE_INTEGER";
	case BT_VALUE_TYPE_FLOAT:
		return "BT_VALUE_TYPE_FLOAT";
	case BT_VALUE_TYPE_STRING:
		return "BT_VALUE_TYPE_STRING";
	case BT_VALUE_TYPE_ARRAY:
		return "BT_VALUE_TYPE_ARRAY";
	case BT_VALUE_TYPE_MAP:
		return "BT_VALUE_TYPE_MAP";
	default:
		return "(unknown)";
	}
};

#endif /* BABELTRACE_VALUES_INTERNAL_H */
