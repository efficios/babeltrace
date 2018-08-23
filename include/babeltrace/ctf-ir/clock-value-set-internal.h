#ifndef BABELTRACE_GRAPH_CLOCK_VALUE_SET_H
#define BABELTRACE_GRAPH_CLOCK_VALUE_SET_H

/*
 * Copyright 2018 Philippe Proulx <pproulx@efficios.com>
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

#include <stdint.h>
#include <glib.h>
#include <babeltrace/ctf-ir/clock-value-internal.h>
#include <babeltrace/ctf-ir/clock-class-internal.h>
#include <babeltrace/assert-internal.h>

struct bt_clock_value_set {
	/* Unique objects owned by this */
	GPtrArray *clock_values;

	/* Weak; points to one of the clock values above */
	struct bt_clock_value *default_cv;
};

static inline
int bt_clock_value_set_initialize(struct bt_clock_value_set *cv_set)
{
	int ret = 0;

	cv_set->clock_values = g_ptr_array_sized_new(1);
	if (!cv_set->clock_values) {
#ifdef BT_LOGE_STR
		BT_LOGE_STR("Failed to allocate one GPtrArray.");
#endif

		ret = -1;
		goto end;
	}

	cv_set->default_cv = NULL;

end:
	return ret;
}

static inline
void bt_clock_value_set_reset(struct bt_clock_value_set *cv_set)
{
	uint64_t i;

	BT_ASSERT(cv_set);
	BT_ASSERT(cv_set->clock_values);

	for (i = 0; i < cv_set->clock_values->len; i++) {
		struct bt_clock_value *cv = cv_set->clock_values->pdata[i];

		BT_ASSERT(cv);
		bt_clock_value_reset(cv);
	}

	cv_set->default_cv = NULL;
}

static inline
void bt_clock_value_set_finalize(struct bt_clock_value_set *cv_set)
{
	uint64_t i;

	BT_ASSERT(cv_set);

	if (cv_set->clock_values) {
		for (i = 0; i < cv_set->clock_values->len; i++) {
			struct bt_clock_value *cv =
				cv_set->clock_values->pdata[i];

			BT_ASSERT(cv);
			bt_clock_value_recycle(cv);
		}

		g_ptr_array_free(cv_set->clock_values, TRUE);
	}

	cv_set->default_cv = NULL;
}

static inline
int bt_clock_value_set_set_clock_value(struct bt_clock_value_set *cv_set,
		struct bt_clock_class *cc, uint64_t raw_value)
{
	int ret = 0;
	struct bt_clock_value *clock_value = NULL;
	uint64_t i;

	BT_ASSERT(cv_set);
	BT_ASSERT(cc);

	/*
	 * Check if we already have a value for this clock class.
	 *
	 * TODO: When we have many clock classes, make this more
	 * efficient.
	 */
	for (i = 0; i < cv_set->clock_values->len; i++) {
		struct bt_clock_value *cv = cv_set->clock_values->pdata[i];

		BT_ASSERT(cv);

		if (cv->clock_class == cc) {
			clock_value = cv;
			break;
		}
	}

	if (!clock_value) {
		clock_value = bt_clock_value_create(cc);
		if (!clock_value) {
#ifdef BT_LIB_LOGE
			BT_LIB_LOGE("Cannot create a clock value from a clock class: "
				"%![cc-]+K", cc);
#endif

			ret = -1;
			goto end;
		}

		g_ptr_array_add(cv_set->clock_values, clock_value);
	}

	bt_clock_value_set_value_inline(clock_value, raw_value);

end:
	return ret;
}

static inline
void  bt_clock_value_set_set_default_clock_value(
		struct bt_clock_value_set *cv_set, uint64_t raw_value)
{
	BT_ASSERT(cv_set);
	BT_ASSERT(cv_set->default_cv);
	bt_clock_value_set_value_inline(cv_set->default_cv, raw_value);
}

#endif /* BABELTRACE_GRAPH_CLOCK_VALUE_SET_H */
