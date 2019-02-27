#ifndef BABELTRACE_TRACE_IR_CLOCK_CLASS_INTERNAL_H
#define BABELTRACE_TRACE_IR_CLOCK_CLASS_INTERNAL_H

/*
 * Copyright 2017-2018 Philippe Proulx <pproulx@efficios.com>
 * Copyright 2013, 2014 Jérémie Galarneau <jeremie.galarneau@efficios.com>
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

#include <babeltrace/trace-ir/clock-class.h>
#include <babeltrace/object-internal.h>
#include <babeltrace/babeltrace-internal.h>
#include <babeltrace/object-pool-internal.h>
#include <babeltrace/compat/uuid-internal.h>
#include <babeltrace/types.h>
#include <babeltrace/property-internal.h>
#include <babeltrace/assert-internal.h>
#include <stdbool.h>
#include <stdint.h>
#include <glib.h>

#define NS_PER_S_I	INT64_C(1000000000)
#define NS_PER_S_U	UINT64_C(1000000000)

struct bt_clock_class {
	struct bt_object base;

	struct {
		GString *str;

		/* NULL or `str->str` above */
		const char *value;
	} name;

	struct {
		GString *str;

		/* NULL or `str->str` above */
		const char *value;
	} description;

	uint64_t frequency;
	uint64_t precision;
	int64_t offset_seconds;
	uint64_t offset_cycles;

	struct {
		uint8_t uuid[BABELTRACE_UUID_LEN];

		/* NULL or `uuid` above */
		bt_uuid value;
	} uuid;

	bool origin_is_unix_epoch;

	/*
	 * This is computed every time you call
	 * bt_clock_class_set_frequency() or
	 * bt_clock_class_set_offset(), as well as initially. It is the
	 * base offset in nanoseconds including both `offset_seconds`
	 * and `offset_cycles` above in the result. It is used to
	 * accelerate future calls to
	 * bt_clock_snapshot_get_ns_from_origin() and
	 * bt_clock_class_cycles_to_ns_from_origin().
	 *
	 * `overflows` is true if the base offset cannot be computed
	 * because of an overflow.
	 */
	struct {
		int64_t value_ns;
		bool overflows;
	} base_offset;

	/* Pool of `struct bt_clock_snapshot *` */
	struct bt_object_pool cs_pool;

	bool frozen;
};

BT_HIDDEN
void _bt_clock_class_freeze(const struct bt_clock_class *clock_class);

#ifdef BT_DEV_MODE
# define bt_clock_class_freeze		_bt_clock_class_freeze
#else
# define bt_clock_class_freeze(_cc)
#endif

BT_HIDDEN
bt_bool bt_clock_class_is_valid(struct bt_clock_class *clock_class);

static inline
int bt_clock_class_clock_value_from_ns_from_origin(
		struct bt_clock_class *cc, int64_t ns_from_origin,
		uint64_t *raw_value)
{
	int ret = 0;
	int64_t offset_in_ns;
	uint64_t value_in_ns;
	uint64_t rem_value_in_ns;
	uint64_t value_periods;
	uint64_t value_period_cycles;
	int64_t ns_to_add;

	BT_ASSERT(cc);
	BT_ASSERT(raw_value);

	/* Compute offset part of requested value, in nanoseconds */
	if (!bt_safe_to_mul_int64(cc->offset_seconds, NS_PER_S_I)) {
		ret = -1;
		goto end;
	}

	offset_in_ns = cc->offset_seconds * NS_PER_S_I;

	if (cc->frequency == NS_PER_S_U) {
		ns_to_add = (int64_t) cc->offset_cycles;
	} else {
		if (!bt_safe_to_mul_int64((int64_t) cc->offset_cycles,
				NS_PER_S_I)) {
			ret = -1;
			goto end;
		}

		ns_to_add = ((int64_t) cc->offset_cycles * NS_PER_S_I) /
			(int64_t) cc->frequency;
	}

	if (!bt_safe_to_add_int64(offset_in_ns, ns_to_add)) {
		ret = -1;
		goto end;
	}

	offset_in_ns += ns_to_add;

	/* Value part in nanoseconds */
	if (ns_from_origin < offset_in_ns) {
		ret = -1;
		goto end;
	}

	value_in_ns = (uint64_t) (ns_from_origin - offset_in_ns);

	/* Number of whole clock periods in `value_in_ns` */
	value_periods = value_in_ns / NS_PER_S_U;

	/* Remaining nanoseconds in cycles + whole clock periods in cycles */
	rem_value_in_ns = value_in_ns - value_periods * NS_PER_S_U;

	if (value_periods > UINT64_MAX / cc->frequency) {
		ret = -1;
		goto end;
	}

	if (!bt_safe_to_mul_uint64(value_periods, cc->frequency)) {
		ret = -1;
		goto end;
	}

	value_period_cycles = value_periods * cc->frequency;

	if (!bt_safe_to_mul_uint64(cc->frequency, rem_value_in_ns)) {
		ret = -1;
		goto end;
	}

	if (!bt_safe_to_add_uint64(cc->frequency * rem_value_in_ns / NS_PER_S_U,
			value_period_cycles)) {
		ret = -1;
		goto end;
	}

	*raw_value = cc->frequency * rem_value_in_ns / NS_PER_S_U +
		value_period_cycles;

end:
	return ret;
}

#endif /* BABELTRACE_TRACE_IR_CLOCK_CLASS_INTERNAL_H */
