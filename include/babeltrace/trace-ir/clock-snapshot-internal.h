#ifndef BABELTRACE_TRACE_IR_CLOCK_SNAPSHOT_INTERNAL_H
#define BABELTRACE_TRACE_IR_CLOCK_SNAPSHOT_INTERNAL_H

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

#include <babeltrace/babeltrace-internal.h>
#include <babeltrace/object-internal.h>
#include <babeltrace/trace-ir/clock-class-internal.h>
#include <babeltrace/trace-ir/utils-internal.h>
#include <stdbool.h>
#include <stdint.h>

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
	BT_ASSERT(clock_snapshot);
	clock_snapshot->is_set = true;
}

static inline
void bt_clock_snapshot_reset(struct bt_clock_snapshot *clock_snapshot)
{
	BT_ASSERT(clock_snapshot);
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
	BT_ASSERT(clock_snapshot);
	clock_snapshot->value_cycles = cycles;
	set_ns_from_origin(clock_snapshot);
	bt_clock_snapshot_set(clock_snapshot);
}

static inline
void bt_clock_snapshot_set_value_inline(struct bt_clock_snapshot *clock_snapshot,
		uint64_t raw_value)
{
	bt_clock_snapshot_set_raw_value(clock_snapshot, raw_value);
}

BT_HIDDEN
void bt_clock_snapshot_destroy(struct bt_clock_snapshot *clock_snapshot);

BT_HIDDEN
struct bt_clock_snapshot *bt_clock_snapshot_new(struct bt_clock_class *clock_class);

BT_HIDDEN
struct bt_clock_snapshot *bt_clock_snapshot_create(
		struct bt_clock_class *clock_class);

BT_HIDDEN
void bt_clock_snapshot_recycle(struct bt_clock_snapshot *clock_snapshot);

#endif /* BABELTRACE_TRACE_IR_CLOCK_SNAPSHOT_INTERNAL_H */
