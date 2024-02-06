/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright 2017-2018 Philippe Proulx <pproulx@efficios.com>
 */

#ifndef BABELTRACE_TRACE_IR_CLOCK_SNAPSHOT_INTERNAL_H
#define BABELTRACE_TRACE_IR_CLOCK_SNAPSHOT_INTERNAL_H

#include "lib/object.h"
#include <stdbool.h>
#include <stdint.h>

#include "clock-class.h"
#include "utils.h"

struct bt_clock_class;

struct bt_clock_snapshot {
	struct bt_object base;
	struct bt_clock_class *clock_class;
	uint64_t value_cycles;
	bool ns_from_origin_overflows;
	int64_t ns_from_origin;
	bool is_set;
};

static inline
void bt_clock_snapshot_set(struct bt_clock_snapshot *clock_snapshot)
{
	BT_ASSERT_DBG(clock_snapshot);
	clock_snapshot->is_set = true;
}

static inline
void bt_clock_snapshot_reset(struct bt_clock_snapshot *clock_snapshot)
{
	BT_ASSERT_DBG(clock_snapshot);
	clock_snapshot->is_set = false;
}

static inline
void set_ns_from_origin(struct bt_clock_snapshot *clock_snapshot)
{
	if (bt_util_ns_from_origin_clock_class(clock_snapshot->clock_class,
			clock_snapshot->value_cycles,
			&clock_snapshot->ns_from_origin)) {
		clock_snapshot->ns_from_origin_overflows = true;
	}
}

static inline
void bt_clock_snapshot_set_raw_value(struct bt_clock_snapshot *clock_snapshot,
		uint64_t cycles)
{
	BT_ASSERT_DBG(clock_snapshot);
	clock_snapshot->value_cycles = cycles;
	set_ns_from_origin(clock_snapshot);
	bt_clock_snapshot_set(clock_snapshot);
}

void bt_clock_snapshot_destroy(struct bt_clock_snapshot *clock_snapshot);

struct bt_clock_snapshot *bt_clock_snapshot_new(struct bt_clock_class *clock_class);

struct bt_clock_snapshot *bt_clock_snapshot_create(
		struct bt_clock_class *clock_class);

void bt_clock_snapshot_recycle(struct bt_clock_snapshot *clock_snapshot);

#endif /* BABELTRACE_TRACE_IR_CLOCK_SNAPSHOT_INTERNAL_H */
