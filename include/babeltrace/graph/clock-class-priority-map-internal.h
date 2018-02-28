#ifndef BABELTRACE_GRAPH_CLOCK_CLASS_PRIORITY_MAP_INTERNAL_H
#define BABELTRACE_GRAPH_CLOCK_CLASS_PRIORITY_MAP_INTERNAL_H

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
 *
 * The Common Trace Format (CTF) Specification is available at
 * http://www.efficios.com/ctf
 */

#include <stdint.h>
#include <stddef.h>
#include <babeltrace/assert-internal.h>
#include <babeltrace/ctf-ir/clock-class.h>
#include <babeltrace/object-internal.h>
#include <babeltrace/types.h>
#include <glib.h>

struct bt_clock_class_priority_map {
	struct bt_object base;

	/* Array of struct bt_clock_class *, owned by this */
	GPtrArray *entries;

	/* struct bt_clock_class * (weak) to priority (uint64_t *) */
	GHashTable *prios;

	/* Clock class (weak) with the currently highest priority */
	struct bt_clock_class *highest_prio_cc;

	bt_bool frozen;
};

static inline
void _bt_clock_class_priority_map_freeze(
		struct bt_clock_class_priority_map *cc_prio_map)
{
	BT_ASSERT(cc_prio_map);
	cc_prio_map->frozen = BT_TRUE;
}

#ifdef BT_DEV_MODE
# define bt_clock_class_priority_map_freeze	_bt_clock_class_priority_map_freeze
#else
# define bt_clock_class_priority_map_freeze(_cc_prio_map)
#endif /* BT_DEV_MODE */

#endif /* BABELTRACE_GRAPH_CLOCK_CLASS_PRIORITY_MAP_INTERNAL_H */
