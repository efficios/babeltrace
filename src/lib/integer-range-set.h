#ifndef BABELTRACE_INTEGER_RANGE_SET_INTERNAL_H
#define BABELTRACE_INTEGER_RANGE_SET_INTERNAL_H

/*
 * Copyright 2019 Philippe Proulx <pproulx@efficios.com>
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
 *
 * The Common Trace Format (CTF) Specification is available at
 * http://www.efficios.com/ctf
 */

#include <stdbool.h>
#include <glib.h>
#include <babeltrace2/babeltrace.h>

#include "object.h"

#define BT_INTEGER_RANGE_SET_RANGE_AT_INDEX(_rs, _index)		\
	(&g_array_index((_rs)->ranges, struct bt_integer_range, (_index)))

struct bt_integer_range {
	union {
		uint64_t u;
		int64_t i;
	} lower;
	union {
		uint64_t u;
		int64_t i;
	} upper;
};

struct bt_integer_range_set {
	struct bt_object base;

	/* Array of `struct bt_integer_range` */
	GArray *ranges;

	bool frozen;
};

BT_HIDDEN
void _bt_integer_range_set_freeze(const struct bt_integer_range_set *range_set);

#ifdef BT_DEV_MODE
# define bt_integer_range_set_freeze		_bt_integer_range_set_freeze
#else
# define bt_integer_range_set_freeze(_sc)
#endif

BT_HIDDEN
bool bt_integer_range_set_unsigned_has_overlaps(
		const struct bt_integer_range_set *range_set);

BT_HIDDEN
bool bt_integer_range_set_signed_has_overlaps(
		const struct bt_integer_range_set *range_set);

#endif /* BABELTRACE_INTEGER_RANGE_SET_INTERNAL_H */
