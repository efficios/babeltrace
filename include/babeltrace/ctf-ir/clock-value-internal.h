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
#include <stdbool.h>
#include <stdint.h>

struct bt_clock_class;

struct bt_clock_value {
	struct bt_object base;
	struct bt_clock_class *clock_class;
	uint64_t value;
	bool ns_from_epoch_overflows;
	int64_t ns_from_epoch;
	bool is_set;
	bool frozen;
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

BT_UNUSED
static inline
void _bt_clock_value_set_is_frozen(struct bt_clock_value *clock_value,
		bool is_frozen)
{
	BT_ASSERT(clock_value);
	clock_value->frozen = is_frozen;
}

static inline
uint64_t ns_from_value(uint64_t frequency, uint64_t value)
{
	uint64_t ns;

	if (frequency == UINT64_C(1000000000)) {
		ns = value;
	} else {
		double dblres = ((1e9 * (double) value) / (double) frequency);

		if (dblres >= (double) UINT64_MAX) {
			/* Overflows uint64_t */
			ns = -1ULL;
		} else {
			ns = (uint64_t) dblres;
		}
	}

	return ns;
}

static inline
int ns_from_epoch(struct bt_clock_class *clock_class, uint64_t value,
		int64_t *ns_from_epoch, bool *overflows)
{
	int ret = 0;
	int64_t diff;
	int64_t s_ns;
	uint64_t u_ns;
	uint64_t cycles;

	*overflows = false;

	/* Initialize nanosecond timestamp to clock's offset in seconds */
	if (clock_class->offset_s <= (INT64_MIN / INT64_C(1000000000)) ||
			clock_class->offset_s >= (INT64_MAX / INT64_C(1000000000))) {
		/*
		 * Overflow: offset in seconds converted to nanoseconds
		 * is outside the int64_t range.
		 */
		*overflows = true;
		goto end;
	}

	*ns_from_epoch = clock_class->offset_s * INT64_C(1000000000);

	/* Add offset in cycles */
	if (clock_class->offset < 0) {
		cycles = (uint64_t) -clock_class->offset;
	} else {
		cycles = (uint64_t) clock_class->offset;
	}

	u_ns = ns_from_value(clock_class->frequency, cycles);

	if (u_ns == UINT64_C(-1) || u_ns >= INT64_MAX) {
		/*
		 * Overflow: offset in cycles converted to nanoseconds
		 * is outside the int64_t range.
		 */
		*overflows = true;
		goto end;
	}

	s_ns = (int64_t) u_ns;
	BT_ASSERT(s_ns >= 0);

	if (clock_class->offset < 0) {
		if (*ns_from_epoch >= 0) {
			/*
			 * Offset in cycles is negative so it must also
			 * be negative once converted to nanoseconds.
			 */
			s_ns = -s_ns;
			goto offset_ok;
		}

		diff = *ns_from_epoch - INT64_MIN;

		if (s_ns >= diff) {
			/*
			 * Overflow: current timestamp in nanoseconds
			 * plus the offset in cycles converted to
			 * nanoseconds is outside the int64_t range.
			 */
			*overflows = true;
			goto end;
		}

		/*
		 * Offset in cycles is negative so it must also be
		 * negative once converted to nanoseconds.
		 */
		s_ns = -s_ns;
	} else {
		if (*ns_from_epoch <= 0) {
			goto offset_ok;
		}

		diff = INT64_MAX - *ns_from_epoch;

		if (s_ns >= diff) {
			/*
			 * Overflow: current timestamp in nanoseconds
			 * plus the offset in cycles converted to
			 * nanoseconds is outside the int64_t range.
			 */
			*overflows = true;
			goto end;
		}
	}

offset_ok:
	*ns_from_epoch += s_ns;

	/* Add clock value (cycles) */
	u_ns = ns_from_value(clock_class->frequency, value);

	if (u_ns == -1ULL || u_ns >= INT64_MAX) {
		/*
		 * Overflow: value converted to nanoseconds is outside
		 * the int64_t range.
		 */
		*overflows = true;
		goto end;
	}

	s_ns = (int64_t) u_ns;
	BT_ASSERT(s_ns >= 0);

	/* Clock value (cycles) is always positive */
	if (*ns_from_epoch <= 0) {
		goto value_ok;
	}

	diff = INT64_MAX - *ns_from_epoch;

	if (s_ns >= diff) {
		/*
		 * Overflow: current timestamp in nanoseconds plus the
		 * clock value converted to nanoseconds is outside the
		 * int64_t range.
		 */
		*overflows = true;
		goto end;
	}

value_ok:
	*ns_from_epoch += s_ns;

end:
	if (*overflows) {
		*ns_from_epoch = 0;
		ret = -1;
	}

	return ret;
}

static inline
void set_ns_from_epoch(struct bt_clock_value *clock_value)
{
	(void) ns_from_epoch(clock_value->clock_class,
		clock_value->value, &clock_value->ns_from_epoch,
		&clock_value->ns_from_epoch_overflows);
}

static inline
void bt_clock_value_set_raw_value(struct bt_clock_value *clock_value,
		uint64_t cycles)
{
	BT_ASSERT(clock_value);

	clock_value->value = cycles;
	set_ns_from_epoch(clock_value);
	bt_clock_value_set(clock_value);
}

static inline
int bt_clock_value_set_value_inline(struct bt_clock_value *clock_value,
		uint64_t raw_value)
{
#ifdef BT_ASSERT_PRE_NON_NULL
	BT_ASSERT_PRE_NON_NULL(clock_value, "Clock value");
#endif

#ifdef BT_ASSERT_PRE_HOT
	BT_ASSERT_PRE_HOT(clock_value, "Clock value", ": %!+k", clock_value);
#endif

	bt_clock_value_set_raw_value(clock_value, raw_value);
	return 0;
}

#ifdef BT_DEV_MODE
# define bt_clock_value_set_is_frozen	_bt_clock_value_set_is_frozen
#else
# define bt_clock_value_set_is_frozen(_x, _f)
#endif /* BT_DEV_MODE */

BT_HIDDEN
struct bt_clock_value *bt_clock_value_create(
		struct bt_clock_class *clock_class);

BT_HIDDEN
void bt_clock_value_recycle(struct bt_clock_value *clock_value);

BT_HIDDEN
void bt_clock_value_set_raw_value(struct bt_clock_value *clock_value,
		uint64_t cycles);

#endif /* BABELTRACE_CTF_IR_CLOCK_VALUE_INTERNAL_H */
