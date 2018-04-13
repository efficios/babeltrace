/*
 * clock-class-priority-map.c
 *
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

#define BT_LOG_TAG "CC-PRIO-MAP"
#include <babeltrace/lib-logging-internal.h>

#include <babeltrace/graph/clock-class-priority-map.h>
#include <babeltrace/graph/clock-class-priority-map-internal.h>
#include <babeltrace/ctf-ir/clock-class.h>
#include <babeltrace/ctf-ir/clock-class-internal.h>
#include <babeltrace/babeltrace-internal.h>
#include <babeltrace/compiler-internal.h>
#include <babeltrace/ref.h>
#include <babeltrace/assert-internal.h>
#include <babeltrace/assert-pre-internal.h>
#include <stdint.h>
#include <inttypes.h>
#include <glib.h>

static
void bt_clock_class_priority_map_destroy(struct bt_object *obj)
{
	struct bt_clock_class_priority_map *cc_prio_map = (void *) obj;

	BT_LOGD("Destroying component class priority map object: addr=%p",
		cc_prio_map);

	if (cc_prio_map->entries) {
		BT_LOGD("Putting clock classes.");
		g_ptr_array_free(cc_prio_map->entries, TRUE);
	}

	if (cc_prio_map->prios) {
		g_hash_table_destroy(cc_prio_map->prios);
	}

	g_free(cc_prio_map);
}

struct bt_clock_class_priority_map *bt_clock_class_priority_map_create()
{
	struct bt_clock_class_priority_map *cc_prio_map = NULL;

	BT_LOGD_STR("Creating clock class priority map object.");

	cc_prio_map = g_new0(struct bt_clock_class_priority_map, 1);
	if (!cc_prio_map) {
		BT_LOGE_STR("Failed to allocate one clock class priority map.");
		goto error;
	}

	bt_object_init(cc_prio_map, bt_clock_class_priority_map_destroy);
	cc_prio_map->entries = g_ptr_array_new_with_free_func(
		(GDestroyNotify) bt_put);
	if (!cc_prio_map->entries) {
		BT_LOGE_STR("Failed to allocate a GPtrArray.");
		goto error;
	}

	cc_prio_map->prios = g_hash_table_new_full(g_direct_hash,
		g_direct_equal, NULL, (GDestroyNotify) g_free);
	if (!cc_prio_map->entries) {
		BT_LOGE_STR("Failed to allocate a GHashTable.");
		goto error;
	}

	BT_LOGD("Created clock class priority map object: addr=%p",
		cc_prio_map);
	goto end;

error:
	BT_PUT(cc_prio_map);

end:
	return cc_prio_map;
}

int64_t bt_clock_class_priority_map_get_clock_class_count(
		struct bt_clock_class_priority_map *cc_prio_map)
{
	BT_ASSERT_PRE_NON_NULL(cc_prio_map, "Clock class priority map");
	return (int64_t) cc_prio_map->entries->len;
}

struct bt_clock_class *bt_clock_class_priority_map_borrow_clock_class_by_index(
		struct bt_clock_class_priority_map *cc_prio_map,
		uint64_t index)
{
	BT_ASSERT_PRE_NON_NULL(cc_prio_map, "Clock class priority map");
	BT_ASSERT_PRE(index < cc_prio_map->entries->len,
		"Index is out of bounds: index=%" PRIu64 ", count=%" PRIu64,
		index, cc_prio_map->entries->len);
	return g_ptr_array_index(cc_prio_map->entries, index);
}

struct bt_clock_class *bt_clock_class_priority_map_borrow_clock_class_by_name(
		struct bt_clock_class_priority_map *cc_prio_map,
		const char *name)
{
	size_t i;
	struct bt_clock_class *clock_class = NULL;

	BT_ASSERT_PRE_NON_NULL(cc_prio_map, "Clock class priority map");
	BT_ASSERT_PRE_NON_NULL(name, "Name");

	for (i = 0; i < cc_prio_map->entries->len; i++) {
		struct bt_clock_class *cur_cc =
			g_ptr_array_index(cc_prio_map->entries, i);
		const char *cur_cc_name =
			bt_clock_class_get_name(cur_cc);

		BT_ASSERT(cur_cc_name);

		if (strcmp(cur_cc_name, name) == 0) {
			clock_class = cur_cc;
			goto end;
		}
	}

end:
	return clock_class;
}


struct clock_class_prio {
	uint64_t prio;
	struct bt_clock_class *clock_class;
};

static
void current_highest_prio_gh_func(gpointer key, gpointer value,
		gpointer user_data)
{
	struct clock_class_prio *func_data = user_data;
	uint64_t *prio = value;

	if (*prio <= func_data->prio) {
		func_data->prio = *prio;
		func_data->clock_class = key;
	}
}

static
struct clock_class_prio bt_clock_class_priority_map_current_highest_prio(
		struct bt_clock_class_priority_map *cc_prio_map)
{
	struct clock_class_prio func_data = {
		.prio = -1ULL,
		.clock_class = NULL,
	};

	g_hash_table_foreach(cc_prio_map->prios, current_highest_prio_gh_func,
		&func_data);
	return func_data;
}

struct bt_clock_class *
bt_clock_class_priority_map_borrow_highest_priority_clock_class(
		struct bt_clock_class_priority_map *cc_prio_map)
{
	BT_ASSERT_PRE_NON_NULL(cc_prio_map, "Clock class priority map");
	return cc_prio_map->highest_prio_cc;
}

int bt_clock_class_priority_map_get_clock_class_priority(
		struct bt_clock_class_priority_map *cc_prio_map,
		struct bt_clock_class *clock_class, uint64_t *priority)
{
	int ret = 0;
	uint64_t *prio;

	BT_ASSERT_PRE_NON_NULL(cc_prio_map, "Clock class priority map");
	BT_ASSERT_PRE_NON_NULL(clock_class, "Clock class");
	BT_ASSERT_PRE_NON_NULL(priority, "Priority");
	prio = g_hash_table_lookup(cc_prio_map->prios, clock_class);
	if (!prio) {
		BT_LOGV("Clock class does not exist in clock class priority map: "
			"cc-prio-map-addr=%p, clock-class-addr=%p, "
			"clock-class-name=\"%s\"",
			cc_prio_map, clock_class,
			bt_clock_class_get_name(clock_class));
		ret = -1;
		goto end;
	}

	*priority = *prio;

end:
	return ret;
}

int bt_clock_class_priority_map_add_clock_class(
		struct bt_clock_class_priority_map *cc_prio_map,
		struct bt_clock_class *clock_class, uint64_t priority)
{
	int ret = 0;
	uint64_t *prio_ptr = NULL;
	struct clock_class_prio cc_prio;

	// FIXME when available: check
	// bt_clock_class_is_valid(clock_class)
	BT_ASSERT_PRE_NON_NULL(cc_prio_map, "Clock class priority map");
	BT_ASSERT_PRE_NON_NULL(clock_class, "Clock class");
	BT_ASSERT_PRE_HOT(cc_prio_map, "Clock class priority map", "");

	/* Check for existing clock class */
	prio_ptr = g_hash_table_lookup(cc_prio_map->prios, clock_class);
	if (prio_ptr) {
		*prio_ptr = priority;
		prio_ptr = NULL;
		goto set_highest_prio;
	}

	prio_ptr = g_new(uint64_t, 1);
	if (!prio_ptr) {
		BT_LOGE_STR("Failed to allocate a uint64_t.");
		ret = -1;
		goto end;
	}

	*prio_ptr = priority;
	bt_get(clock_class);
	g_ptr_array_add(cc_prio_map->entries, clock_class);
	g_hash_table_insert(cc_prio_map->prios, clock_class, prio_ptr);
	prio_ptr = NULL;

set_highest_prio:
	cc_prio = bt_clock_class_priority_map_current_highest_prio(
		cc_prio_map);
	BT_ASSERT(cc_prio.clock_class);
	cc_prio_map->highest_prio_cc = cc_prio.clock_class;
	BT_LOGV("Added clock class to clock class priority map: "
		"cc-prio-map-addr=%p, added-clock-class-addr=%p, "
		"added-clock-class-name=\"%s\", "
		"highest-prio-clock-class-addr=%p, "
		"highest-prio-clock-class-name=\"%s\"",
		cc_prio_map, clock_class,
		bt_clock_class_get_name(clock_class),
		cc_prio.clock_class,
		bt_clock_class_get_name(cc_prio.clock_class));

end:
	g_free(prio_ptr);

	return ret;
}

struct bt_clock_class_priority_map *bt_clock_class_priority_map_copy(
		struct bt_clock_class_priority_map *orig_cc_prio_map)
{
	struct bt_clock_class_priority_map *cc_prio_map;
	size_t i;

	cc_prio_map = bt_clock_class_priority_map_create();
	if (!cc_prio_map) {
		BT_LOGE_STR("Cannot create empty clock class priority map.");
		goto error;
	}

	for (i = 0; i < orig_cc_prio_map->entries->len; i++) {
		struct bt_clock_class *clock_class =
			g_ptr_array_index(orig_cc_prio_map->entries, i);
		uint64_t *prio = g_hash_table_lookup(orig_cc_prio_map->prios,
			clock_class);
		int ret = bt_clock_class_priority_map_add_clock_class(
			cc_prio_map, clock_class, *prio);

		if (ret) {
			BT_LOGE("Cannot add clock class to clock class priority map copy: "
				"cc-prio-map-copy-addr=%p, clock-class-addr=%p, "
				"clock-class-name=\"%s\"",
				cc_prio_map, clock_class,
				bt_clock_class_get_name(clock_class));
			goto error;
		}
	}

	cc_prio_map->highest_prio_cc = orig_cc_prio_map->highest_prio_cc;
	BT_LOGD("Copied clock class priority map: "
		"original-addr=%p, copy-addr=%p",
		orig_cc_prio_map, cc_prio_map);
	goto end;

error:
	BT_PUT(cc_prio_map);

end:
	return cc_prio_map;
}
