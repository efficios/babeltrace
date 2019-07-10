#ifndef BABELTRACE_VALUES_INTERNAL_H
#define BABELTRACE_VALUES_INTERNAL_H

/*
 * Copyright (c) 2015-2018 Philippe Proulx <pproulx@efficios.com>
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

#include <glib.h>
#include <babeltrace2/babeltrace.h>

#include "lib/object.h"
#include "common/macros.h"

struct bt_value {
	struct bt_object base;
	enum bt_value_type type;
	bt_bool frozen;
};

struct bt_value_bool {
	struct bt_value base;
	bt_bool value;
};

struct bt_value_integer {
	struct bt_value base;
	union {
		uint64_t i;
		int64_t u;
	} value;
};

struct bt_value_real {
	struct bt_value base;
	double value;
};

struct bt_value_string {
	struct bt_value base;
	GString *gstr;
};

struct bt_value_array {
	struct bt_value base;
	GPtrArray *garray;
};

struct bt_value_map {
	struct bt_value base;
	GHashTable *ght;
};

BT_HIDDEN
void _bt_value_freeze(const struct bt_value *object);

#ifdef BT_DEV_MODE
# define bt_value_freeze	_bt_value_freeze
#else
# define bt_value_freeze(_value)
#endif /* BT_DEV_MODE */

#endif /* BABELTRACE_VALUES_INTERNAL_H */
