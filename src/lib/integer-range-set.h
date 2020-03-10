/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright 2019 Philippe Proulx <pproulx@efficios.com>
 *
 * The Common Trace Format (CTF) Specification is available at
 * http://www.efficios.com/ctf
 */

#ifndef BABELTRACE_INTEGER_RANGE_SET_INTERNAL_H
#define BABELTRACE_INTEGER_RANGE_SET_INTERNAL_H

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
