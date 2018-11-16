#ifndef BABELTRACE_PRIVATE_VALUES_H
#define BABELTRACE_PRIVATE_VALUES_H

/*
 * Copyright (c) 2015-2016 EfficiOS Inc. and Linux Foundation
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

/* For enum bt_value_status */
#include <babeltrace/values.h>

#ifdef __cplusplus
extern "C" {
#endif

struct bt_value;
struct bt_private_value;

extern struct bt_private_value *bt_private_value_null;

extern struct bt_value *bt_value_borrow_from_private(
		struct bt_private_value *priv_value);

extern struct bt_private_value *bt_private_value_bool_create(void);


extern struct bt_private_value *bt_private_value_bool_create_init(bt_bool val);

extern enum bt_value_status bt_private_value_bool_set(struct bt_private_value *bool_obj,
		bt_bool val);

extern struct bt_private_value *bt_private_value_integer_create(void);

extern struct bt_private_value *bt_private_value_integer_create_init(int64_t val);

extern enum bt_value_status bt_private_integer_bool_set(
		struct bt_private_value *integer_obj, int64_t val);

extern struct bt_private_value *bt_private_value_real_create(void);

extern struct bt_private_value *bt_private_value_real_create_init(double val);

extern enum bt_value_status bt_private_value_real_set(
		struct bt_private_value *real_obj, double val);

extern struct bt_private_value *bt_private_value_string_create(void);

extern struct bt_private_value *bt_private_value_string_create_init(const char *val);

extern enum bt_value_status bt_private_value_string_set(struct bt_private_value *string_obj,
		const char *val);

extern struct bt_private_value *bt_private_value_array_create(void);

extern struct bt_private_value *bt_private_value_array_borrow_element_by_index(
		const struct bt_private_value *array_obj, uint64_t index);

extern enum bt_value_status bt_private_value_array_append_element(
		struct bt_private_value *array_obj, struct bt_value *element_obj);

extern enum bt_value_status bt_private_value_array_append_bool_element(
		struct bt_private_value *array_obj, bt_bool val);

extern enum bt_value_status bt_private_value_array_append_integer_element(
		struct bt_private_value *array_obj, int64_t val);

extern enum bt_value_status bt_private_value_array_append_real_element(
		struct bt_private_value *array_obj, double val);

extern enum bt_value_status bt_private_value_array_append_string_element(
		struct bt_private_value *array_obj, const char *val);

extern enum bt_value_status bt_private_value_array_append_empty_array_element(
		struct bt_private_value *array_obj);

extern enum bt_value_status bt_private_value_array_append_empty_map_element(
		struct bt_private_value *array_obj);

extern enum bt_value_status bt_private_value_array_set_element_by_index(
		struct bt_private_value *array_obj, uint64_t index,
		struct bt_value *element_obj);

extern struct bt_private_value *bt_private_value_map_create(void);

extern struct bt_private_value *bt_private_value_map_borrow_entry_value(
		const struct bt_private_value *map_obj, const char *key);

typedef bt_bool (* bt_private_value_map_foreach_entry_cb)(const char *key,
	struct bt_private_value *object, void *data);

extern enum bt_value_status bt_private_value_map_foreach_entry(
		const struct bt_private_value *map_obj,
		bt_private_value_map_foreach_entry_cb cb, void *data);

extern enum bt_value_status bt_private_value_map_insert_entry(
		struct bt_private_value *map_obj, const char *key,
		struct bt_value *element_obj);

extern enum bt_value_status bt_private_value_map_insert_bool_entry(
		struct bt_private_value *map_obj, const char *key, bt_bool val);

extern enum bt_value_status bt_private_value_map_insert_integer_entry(
		struct bt_private_value *map_obj, const char *key, int64_t val);

extern enum bt_value_status bt_private_value_map_insert_real_entry(
		struct bt_private_value *map_obj, const char *key, double val);

extern enum bt_value_status bt_private_value_map_insert_string_entry(
		struct bt_private_value *map_obj, const char *key, const char *val);

extern enum bt_value_status bt_private_value_map_insert_empty_array_entry(
		struct bt_private_value *map_obj, const char *key);

extern enum bt_value_status bt_private_value_map_insert_empty_map_entry(
		struct bt_private_value *map_obj, const char *key);

#ifdef __cplusplus
}
#endif

#endif /* BABELTRACE_PRIVATE_VALUES_H */
