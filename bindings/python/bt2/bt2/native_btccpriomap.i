/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2017 Philippe Proulx <pproulx@efficios.com>
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
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

%{
#include <babeltrace/graph/clock-class-priority-map.h>
%}

/* Type */
struct bt_clock_class_priority_map;

/* Functions */
struct bt_clock_class_priority_map *bt_clock_class_priority_map_create();
int64_t bt_clock_class_priority_map_get_clock_class_count(
		struct bt_clock_class_priority_map *clock_class_priority_map);
struct bt_clock_class *
bt_clock_class_priority_map_get_clock_class_by_index(
		struct bt_clock_class_priority_map *clock_class_priority_map,
		uint64_t index);
struct bt_clock_class *
bt_clock_class_priority_map_get_clock_class_by_name(
		struct bt_clock_class_priority_map *clock_class_priority_map,
		const char *name);
struct bt_clock_class *
bt_clock_class_priority_map_get_highest_priority_clock_class(
		struct bt_clock_class_priority_map *clock_class_priority_map);
int bt_clock_class_priority_map_get_clock_class_priority(
		struct bt_clock_class_priority_map *clock_class_priority_map,
		struct bt_clock_class *clock_class, uint64_t *OUTPUTINIT);
int bt_clock_class_priority_map_add_clock_class(
		struct bt_clock_class_priority_map *clock_class_priority_map,
		struct bt_clock_class *clock_class, uint64_t priority);
struct bt_clock_class_priority_map *bt_clock_class_priority_map_copy(
		struct bt_clock_class_priority_map *clock_class_priority_map);
