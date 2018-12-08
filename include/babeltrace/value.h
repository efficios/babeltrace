#ifndef BABELTRACE_VALUES_H
#define BABELTRACE_VALUES_H

/*
 * Copyright (c) 2015-2018 Philippe Proulx <pproulx@efficios.com>
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
#include <stddef.h>

/* For bt_bool, bt_value */
#include <babeltrace/types.h>

/* For enum bt_value_status, enum bt_value_type */
#include <babeltrace/value-const.h>

#ifdef __cplusplus
extern "C" {
#endif

extern bt_value *bt_value_null;

extern bt_value *bt_value_bool_create(void);

extern bt_value *bt_value_bool_create_init(bt_bool val);

extern void bt_value_bool_set(bt_value *bool_obj, bt_bool val);

extern bt_value *bt_value_integer_create(void);

extern bt_value *bt_value_integer_create_init(
		int64_t val);

extern void bt_value_integer_set(bt_value *integer_obj, int64_t val);

extern bt_value *bt_value_real_create(void);

extern bt_value *bt_value_real_create_init(double val);

extern void bt_value_real_set(bt_value *real_obj, double val);

extern bt_value *bt_value_string_create(void);

extern bt_value *bt_value_string_create_init(const char *val);

extern enum bt_value_status bt_value_string_set(bt_value *string_obj,
		const char *val);

extern bt_value *bt_value_array_create(void);

extern bt_value *bt_value_array_borrow_element_by_index(
		bt_value *array_obj, uint64_t index);

extern enum bt_value_status bt_value_array_append_element(
		bt_value *array_obj,
		bt_value *element_obj);

extern enum bt_value_status bt_value_array_append_bool_element(
		bt_value *array_obj, bt_bool val);

extern enum bt_value_status bt_value_array_append_integer_element(
		bt_value *array_obj, int64_t val);

extern enum bt_value_status bt_value_array_append_real_element(
		bt_value *array_obj, double val);

extern enum bt_value_status bt_value_array_append_string_element(
		bt_value *array_obj, const char *val);

extern enum bt_value_status bt_value_array_append_empty_array_element(
		bt_value *array_obj);

extern enum bt_value_status bt_value_array_append_empty_map_element(
		bt_value *array_obj);

extern enum bt_value_status bt_value_array_set_element_by_index(
		bt_value *array_obj, uint64_t index,
		bt_value *element_obj);

extern bt_value *bt_value_map_create(void);

extern bt_value *bt_value_map_borrow_entry_value(
		bt_value *map_obj, const char *key);

typedef bt_bool (* bt_value_map_foreach_entry_func)(const char *key,
		bt_value *object, void *data);

extern enum bt_value_status bt_value_map_foreach_entry(
		bt_value *map_obj,
		bt_value_map_foreach_entry_func func, void *data);

extern enum bt_value_status bt_value_map_insert_entry(
		bt_value *map_obj, const char *key,
		bt_value *element_obj);

extern enum bt_value_status bt_value_map_insert_bool_entry(
		bt_value *map_obj, const char *key, bt_bool val);

extern enum bt_value_status bt_value_map_insert_integer_entry(
		bt_value *map_obj, const char *key, int64_t val);

extern enum bt_value_status bt_value_map_insert_real_entry(
		bt_value *map_obj, const char *key, double val);

extern enum bt_value_status bt_value_map_insert_string_entry(
		bt_value *map_obj, const char *key,
		const char *val);

extern enum bt_value_status bt_value_map_insert_empty_array_entry(
		bt_value *map_obj, const char *key);

extern enum bt_value_status bt_value_map_insert_empty_map_entry(
		bt_value *map_obj, const char *key);

#ifdef __cplusplus
}
#endif

#endif /* BABELTRACE_VALUES_H */
