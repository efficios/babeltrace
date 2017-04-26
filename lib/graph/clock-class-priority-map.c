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

#include <babeltrace/graph/clock-class-priority-map.h>
#include <babeltrace/graph/clock-class-priority-map-internal.h>
#include <babeltrace/ctf-ir/clock-class.h>
#include <babeltrace/ctf-ir/clock-class-internal.h>
#include <babeltrace/babeltrace-internal.h>
#include <babeltrace/compiler-internal.h>
#include <babeltrace/ref.h>
#include <glib.h>

static
void bt_clock_class_priority_map_destroy(struct bt_object *obj)
{
	struct bt_clock_class_priority_map *cc_prio_map = (void *) obj;

	if (!cc_prio_map) {
		return;
	}

	if (cc_prio_map->entries) {
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

	cc_prio_map = g_new0(struct bt_clock_class_priority_map, 1);
	if (!cc_prio_map) {
		goto error;
	}

	bt_object_init(cc_prio_map, bt_clock_class_priority_map_destroy);
	cc_prio_map->entries = g_ptr_array_new_with_free_func(
		(GDestroyNotify) bt_put);
	if (!cc_prio_map->entries) {
		goto error;
	}

	cc_prio_map->prios = g_hash_table_new_full(g_direct_hash,
		g_direct_equal, NULL, (GDestroyNotify) g_free);
	if (!cc_prio_map->entries) {
		goto error;
	}

	goto end;

error:
	BT_PUT(cc_prio_map);

end:
	return cc_prio_map;
}

int bt_clock_class_priority_map_get_clock_class_count(
		struct bt_clock_class_priority_map *cc_prio_map)
{
	int ret = -1;

	if (!cc_prio_map) {
		goto end;
	}

	ret = (int) cc_prio_map->entries->len;

end:
	return ret;
}

struct bt_ctf_clock_class *bt_clock_class_priority_map_get_clock_class(
		struct bt_clock_class_priority_map *cc_prio_map,
		unsigned int index)
{
	struct bt_ctf_clock_class *clock_class = NULL;

	if (!cc_prio_map || index >= cc_prio_map->entries->len) {
		goto end;
	}

	clock_class = g_ptr_array_index(cc_prio_map->entries, index);
	bt_get(clock_class);

end:
	return clock_class;
}

struct bt_ctf_clock_class *bt_clock_class_priority_map_get_clock_class_by_name(
		struct bt_clock_class_priority_map *cc_prio_map,
		const char *name)
{
	size_t i;
	struct bt_ctf_clock_class *clock_class = NULL;

	if (!cc_prio_map || !name) {
		goto end;
	}

	for (i = 0; i < cc_prio_map->entries->len; i++) {
		struct bt_ctf_clock_class *cur_cc =
			g_ptr_array_index(cc_prio_map->entries, i);
		// FIXME when available: use bt_ctf_clock_class_get_name()
		const char *cur_cc_name =
			cur_cc->name ? cur_cc->name->str : NULL;

		if (!cur_cc_name) {
			goto end;
		}

		if (strcmp(cur_cc_name, name) == 0) {
			clock_class = bt_get(cur_cc);
			goto end;
		}
	}

end:
	return clock_class;
}


struct clock_class_prio {
	uint64_t prio;
	struct bt_ctf_clock_class *clock_class;
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
struct clock_class_prio bt_ctf_clock_class_priority_map_current_highest_prio(
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

struct bt_ctf_clock_class *
bt_clock_class_priority_map_get_highest_priority_clock_class(
		struct bt_clock_class_priority_map *cc_prio_map)
{
	struct bt_ctf_clock_class *clock_class = NULL;

	if (!cc_prio_map) {
		goto end;
	}

	clock_class = bt_get(cc_prio_map->highest_prio_cc);

end:
	return clock_class;
}

int bt_clock_class_priority_map_get_clock_class_priority(
		struct bt_clock_class_priority_map *cc_prio_map,
		struct bt_ctf_clock_class *clock_class, uint64_t *priority)
{
	int ret = 0;
	uint64_t *prio;

	if (!cc_prio_map || !clock_class || !priority) {
		ret = -1;
		goto end;
	}

	prio = g_hash_table_lookup(cc_prio_map->prios, clock_class);
	if (!prio) {
		ret = -1;
		goto end;
	}

	*priority = *prio;

end:
	return ret;
}

int bt_clock_class_priority_map_add_clock_class(
		struct bt_clock_class_priority_map *cc_prio_map,
		struct bt_ctf_clock_class *clock_class, uint64_t priority)
{
	int ret = 0;
	uint64_t *prio_ptr = NULL;
	struct clock_class_prio cc_prio;

	// FIXME when available: check
	// bt_ctf_clock_class_is_valid(clock_class)
	if (!cc_prio_map || !clock_class || cc_prio_map->frozen) {
		ret = -1;
		goto end;
	}

	/* Check for duplicate clock classes */
	prio_ptr = g_hash_table_lookup(cc_prio_map->prios, clock_class);
	if (prio_ptr) {
		prio_ptr = NULL;
		ret = -1;
		goto end;
	}

	prio_ptr = g_new(uint64_t, 1);
	if (!prio_ptr) {
		ret = -1;
		goto end;
	}

	*prio_ptr = priority;
	bt_get(clock_class);
	g_ptr_array_add(cc_prio_map->entries, clock_class);
	g_hash_table_insert(cc_prio_map->prios, clock_class, prio_ptr);
	prio_ptr = NULL;
	cc_prio = bt_ctf_clock_class_priority_map_current_highest_prio(
		cc_prio_map);
	assert(cc_prio.clock_class);
	cc_prio_map->highest_prio_cc = cc_prio.clock_class;

end:
	if (prio_ptr) {
		g_free(prio_ptr);
	}

	return ret;
}
