/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2015-2018 Philippe Proulx <pproulx@efficios.com>
 */

#ifndef BABELTRACE_VALUES_INTERNAL_H
#define BABELTRACE_VALUES_INTERNAL_H

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
