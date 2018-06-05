/*
 * test_cc_prio_map.c
 *
 * Copyright 2017 - Philippe Proulx <pproulx@efficios.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; under version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include <babeltrace/ref.h>
#include <babeltrace/graph/clock-class-priority-map.h>
#include <babeltrace/ctf-ir/clock-class.h>
#include <babeltrace/assert-internal.h>

#include "tap/tap.h"

#define NR_TESTS	17

static void test_clock_class_priority_map(void)
{
	struct bt_clock_class_priority_map *cc_prio_map;
	struct bt_clock_class_priority_map *cc_prio_map_copy;
	struct bt_clock_class *cc1;
	struct bt_clock_class *cc2;
	struct bt_clock_class *cc3;
	struct bt_clock_class *cc;
	uint64_t prio;
	int ret;

	cc_prio_map = bt_clock_class_priority_map_create();
	ok(cc_prio_map, "bt_clock_class_priority_map_create() succeeds");
	cc1 = bt_clock_class_create("cc1", 1);
	BT_ASSERT(cc1);
	cc2 = bt_clock_class_create("cc2", 2);
	BT_ASSERT(cc2);
	cc3 = bt_clock_class_create("cc3", 3);
	BT_ASSERT(cc3);
	ok(!bt_clock_class_priority_map_get_highest_priority_clock_class(cc_prio_map),
		"bt_clock_class_priority_map_get_highest_priority_clock_class() returns NULL when there's no clock classes");
	ret = bt_clock_class_priority_map_add_clock_class(cc_prio_map, cc2, 75);
	BT_ASSERT(ret == 0);
	cc = bt_clock_class_priority_map_get_highest_priority_clock_class(cc_prio_map);
	ok(cc == cc2,
		"bt_clock_class_priority_map_get_highest_priority_clock_class() returns the expected clock class (1)");
	BT_PUT(cc);
	ret = bt_clock_class_priority_map_add_clock_class(cc_prio_map, cc1, 1001);
	BT_ASSERT(ret == 0);
	cc = bt_clock_class_priority_map_get_highest_priority_clock_class(cc_prio_map);
	ok(cc == cc2,
		"bt_clock_class_priority_map_get_highest_priority_clock_class() returns the expected clock class (2)");
	BT_PUT(cc);
	ret = bt_clock_class_priority_map_add_clock_class(cc_prio_map, cc3, 11);
	BT_ASSERT(ret == 0);
	cc = bt_clock_class_priority_map_get_highest_priority_clock_class(cc_prio_map);
	ok(cc == cc3,
		"bt_clock_class_priority_map_get_highest_priority_clock_class() returns the expected clock class (3)");
	BT_PUT(cc);
	ret = bt_clock_class_priority_map_get_clock_class_priority(cc_prio_map, cc1, &prio);
	ok(ret == 0, "bt_clock_class_priority_map_get_clock_class_priority() succeeds");
	ok(prio == 1001,
		"bt_clock_class_priority_map_get_clock_class_priority() returns the expected priority (1)");
	ret = bt_clock_class_priority_map_get_clock_class_priority(cc_prio_map, cc2, &prio);
	BT_ASSERT(ret == 0);
	ok(prio == 75,
		"bt_clock_class_priority_map_get_clock_class_priority() returns the expected priority (2)");
	ret = bt_clock_class_priority_map_get_clock_class_priority(cc_prio_map, cc3, &prio);
	BT_ASSERT(ret == 0);
	ok(prio == 11,
		"bt_clock_class_priority_map_get_clock_class_priority() returns the expected priority (3)");
	cc_prio_map_copy = bt_clock_class_priority_map_copy(cc_prio_map);
	ok(cc_prio_map_copy, "bt_clock_class_priority_map_copy() succeeds");
	ret = bt_clock_class_priority_map_get_clock_class_priority(cc_prio_map_copy, cc1, &prio);
	BT_ASSERT(ret == 0);
	ok(prio == 1001,
		"bt_clock_class_priority_map_get_clock_class_priority() returns the expected priority (1, copy)");
	ret = bt_clock_class_priority_map_get_clock_class_priority(cc_prio_map_copy, cc2, &prio);
	BT_ASSERT(ret == 0);
	ok(prio == 75,
		"bt_clock_class_priority_map_get_clock_class_priority() returns the expected priority (2, copy)");
	ret = bt_clock_class_priority_map_get_clock_class_priority(cc_prio_map_copy, cc3, &prio);
	BT_ASSERT(ret == 0);
	ok(prio == 11,
		"bt_clock_class_priority_map_get_clock_class_priority() returns the expected priority (3, copy)");
	cc = bt_clock_class_priority_map_get_highest_priority_clock_class(cc_prio_map_copy);
	ok(cc == cc3,
		"bt_clock_class_priority_map_get_highest_priority_clock_class() returns the expected clock class (copy)");
	BT_PUT(cc);
	ret = bt_clock_class_priority_map_add_clock_class(cc_prio_map_copy, cc3, 253);
	ok(ret == 0, "bt_clock_class_priority_map_add_clock_class() succeeds for an existing clock class");
	ret = bt_clock_class_priority_map_get_clock_class_priority(cc_prio_map_copy, cc3, &prio);
	BT_ASSERT(ret == 0);
	ok(prio == 253,
		"bt_clock_class_priority_map_get_clock_class_priority() returns the expected priority (updated, copy)");
	cc = bt_clock_class_priority_map_get_highest_priority_clock_class(cc_prio_map_copy);
	ok(cc == cc2,
		"bt_clock_class_priority_map_get_highest_priority_clock_class() returns the expected clock class (updated, copy)");
	BT_PUT(cc);

	BT_PUT(cc3);
	BT_PUT(cc2);
	BT_PUT(cc1);
	BT_PUT(cc_prio_map);
	BT_PUT(cc_prio_map_copy);
}

int main(int argc, char **argv)
{
	plan_tests(NR_TESTS);
	test_clock_class_priority_map();
	return exit_status();
}
