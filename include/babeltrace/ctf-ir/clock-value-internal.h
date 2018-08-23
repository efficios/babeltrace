#ifndef BABELTRACE_CTF_IR_CLOCK_VALUE_INTERNAL_H
#define BABELTRACE_CTF_IR_CLOCK_VALUE_INTERNAL_H

/*
 * Copyright 2017 Philippe Proulx <pproulx@efficios.com>
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
#include <babeltrace/ctf-ir/clock-class-internal.h>
#include <babeltrace/ctf-ir/utils-internal.h>
#include <stdbool.h>
#include <stdint.h>

struct bt_clock_class;

struct bt_clock_value {
	struct bt_object base;
	struct bt_clock_class *clock_class;
	uint64_t value_cycles;
	bool ns_from_origin_overflows;
	int64_t ns_from_origin;
	bool is_set;
};

static inline
void bt_clock_value_set(struct bt_clock_value *clock_value)
{
	BT_ASSERT(clock_value);
	clock_value->is_set = true;
}

static inline
void bt_clock_value_reset(struct bt_clock_value *clock_value)
{
	BT_ASSERT(clock_value);
	clock_value->is_set = false;
}

static inline
void set_ns_from_origin(struct bt_clock_value *clock_value)
{
	if (bt_util_ns_from_origin(clock_value->clock_class, clock_value->value_cycles,
			&clock_value->ns_from_origin)) {
		clock_value->ns_from_origin_overflows = true;
	}

}

static inline
void bt_clock_value_set_raw_value(struct bt_clock_value *clock_value,
		uint64_t cycles)
{
	BT_ASSERT(clock_value);
	clock_value->value_cycles = cycles;
	set_ns_from_origin(clock_value);
	bt_clock_value_set(clock_value);
}

static inline
void bt_clock_value_set_value_inline(struct bt_clock_value *clock_value,
		uint64_t raw_value)
{
	bt_clock_value_set_raw_value(clock_value, raw_value);
}

BT_HIDDEN
void bt_clock_value_destroy(struct bt_clock_value *clock_value);

BT_HIDDEN
struct bt_clock_value *bt_clock_value_new(struct bt_clock_class *clock_class);

BT_HIDDEN
struct bt_clock_value *bt_clock_value_create(
		struct bt_clock_class *clock_class);

BT_HIDDEN
void bt_clock_value_recycle(struct bt_clock_value *clock_value);

BT_HIDDEN
void bt_clock_value_set_raw_value(struct bt_clock_value *clock_value,
		uint64_t cycles);

#endif /* BABELTRACE_CTF_IR_CLOCK_VALUE_INTERNAL_H */
