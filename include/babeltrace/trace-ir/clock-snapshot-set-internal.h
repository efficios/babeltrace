#ifndef BABELTRACE_GRAPH_CLOCK_SNAPSHOT_SET_H
#define BABELTRACE_GRAPH_CLOCK_SNAPSHOT_SET_H

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
#include <babeltrace/trace-ir/clock-snapshot-internal.h>
#include <babeltrace/trace-ir/clock-class-internal.h>
#include <babeltrace/assert-internal.h>

struct bt_clock_snapshot_set {
	/* Unique objects owned by this */
	GPtrArray *clock_snapshots;

	/* Weak; points to one of the clock snapshots above */
	struct bt_clock_snapshot *default_cs;
};

static inline
int bt_clock_snapshot_set_initialize(struct bt_clock_snapshot_set *cs_set)
{
	int ret = 0;

	cs_set->clock_snapshots = g_ptr_array_sized_new(1);
	if (!cs_set->clock_snapshots) {
#ifdef BT_LOGE_STR
		BT_LOGE_STR("Failed to allocate one GPtrArray.");
#endif

		ret = -1;
		goto end;
	}

	cs_set->default_cs = NULL;

end:
	return ret;
}

static inline
void bt_clock_snapshot_set_reset(struct bt_clock_snapshot_set *cs_set)
{
	uint64_t i;

	BT_ASSERT(cs_set);
	BT_ASSERT(cs_set->clock_snapshots);

	for (i = 0; i < cs_set->clock_snapshots->len; i++) {
		struct bt_clock_snapshot *cs = cs_set->clock_snapshots->pdata[i];

		BT_ASSERT(cs);
		bt_clock_snapshot_reset(cs);
	}

	cs_set->default_cs = NULL;
}

static inline
void bt_clock_snapshot_set_finalize(struct bt_clock_snapshot_set *cs_set)
{
	uint64_t i;

	BT_ASSERT(cs_set);

	if (cs_set->clock_snapshots) {
		for (i = 0; i < cs_set->clock_snapshots->len; i++) {
			struct bt_clock_snapshot *cs =
				cs_set->clock_snapshots->pdata[i];

			BT_ASSERT(cs);
			bt_clock_snapshot_recycle(cs);
		}

		g_ptr_array_free(cs_set->clock_snapshots, TRUE);
	}

	cs_set->default_cs = NULL;
}

static inline
int bt_clock_snapshot_set_set_clock_snapshot(struct bt_clock_snapshot_set *cs_set,
		struct bt_clock_class *cc, uint64_t raw_value)
{
	int ret = 0;
	struct bt_clock_snapshot *clock_snapshot = NULL;
	uint64_t i;

	BT_ASSERT(cs_set);
	BT_ASSERT(cc);

	/*
	 * Check if we already have a value for this clock class.
	 *
	 * TODO: When we have many clock classes, make this more
	 * efficient.
	 */
	for (i = 0; i < cs_set->clock_snapshots->len; i++) {
		struct bt_clock_snapshot *cs = cs_set->clock_snapshots->pdata[i];

		BT_ASSERT(cs);

		if (cs->clock_class == cc) {
			clock_snapshot = cs;
			break;
		}
	}

	if (!clock_snapshot) {
		clock_snapshot = bt_clock_snapshot_create(cc);
		if (!clock_snapshot) {
#ifdef BT_LIB_LOGE
			BT_LIB_LOGE("Cannot create a clock snapshot from a clock class: "
				"%![cc-]+K", cc);
#endif

			ret = -1;
			goto end;
		}

		g_ptr_array_add(cs_set->clock_snapshots, clock_snapshot);
	}

	bt_clock_snapshot_set_value_inline(clock_snapshot, raw_value);

end:
	return ret;
}

static inline
void  bt_clock_snapshot_set_set_default_clock_snapshot(
		struct bt_clock_snapshot_set *cs_set, uint64_t raw_value)
{
	BT_ASSERT(cs_set);
	BT_ASSERT(cs_set->default_cs);
	bt_clock_snapshot_set_value_inline(cs_set->default_cs, raw_value);
}

#endif /* BABELTRACE_GRAPH_CLOCK_SNAPSHOT_SET_H */
