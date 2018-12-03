#ifndef BABELTRACE_VALUES_H
#define BABELTRACE_VALUES_H

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

/* For bt_bool */
#include <babeltrace/types.h>

#ifdef __cplusplus
extern "C" {
#endif

enum bt_value_status {
	/// Operation canceled.
	BT_VALUE_STATUS_CANCELED	= 125,

	/// Cannot allocate memory.
	BT_VALUE_STATUS_NOMEM		= -12,

	/// Okay, no error.
	BT_VALUE_STATUS_OK		= 0,
};

enum bt_value_type {
	/// Null value object.
	BT_VALUE_TYPE_NULL =		0,

	/// Boolean value object (holds #BT_TRUE or #BT_FALSE).
	BT_VALUE_TYPE_BOOL =		1,

	/// Integer value object (holds a signed 64-bit integer raw value).
	BT_VALUE_TYPE_INTEGER =		2,

	/// Floating point number value object (holds a \c double raw value).
	BT_VALUE_TYPE_REAL =		3,

	/// String value object.
	BT_VALUE_TYPE_STRING =		4,

	/// Array value object.
	BT_VALUE_TYPE_ARRAY =		5,

	/// Map value object.
	BT_VALUE_TYPE_MAP =		6,
};

struct bt_value;

extern struct bt_value *bt_value_null;

extern struct bt_value *bt_value_bool_create(void);

extern struct bt_value *bt_value_bool_create_init(bt_bool val);

extern void bt_value_bool_set(struct bt_value *bool_obj, bt_bool val);

extern struct bt_value *bt_value_integer_create(void);

extern struct bt_value *bt_value_integer_create_init(
		int64_t val);

extern void bt_value_integer_set(struct bt_value *integer_obj, int64_t val);

extern struct bt_value *bt_value_real_create(void);

extern struct bt_value *bt_value_real_create_init(double val);

extern void bt_value_real_set(struct bt_value *real_obj, double val);

extern struct bt_value *bt_value_string_create(void);

extern struct bt_value *bt_value_string_create_init(const char *val);

extern enum bt_value_status bt_value_string_set(struct bt_value *string_obj,
		const char *val);

extern struct bt_value *bt_value_array_create(void);

extern struct bt_value *bt_value_array_borrow_element_by_index(
		struct bt_value *array_obj, uint64_t index);

extern enum bt_value_status bt_value_array_append_element(
		struct bt_value *array_obj,
		struct bt_value *element_obj);

extern enum bt_value_status bt_value_array_append_bool_element(
		struct bt_value *array_obj, bt_bool val);

extern enum bt_value_status bt_value_array_append_integer_element(
		struct bt_value *array_obj, int64_t val);

extern enum bt_value_status bt_value_array_append_real_element(
		struct bt_value *array_obj, double val);

extern enum bt_value_status bt_value_array_append_string_element(
		struct bt_value *array_obj, const char *val);

extern enum bt_value_status bt_value_array_append_empty_array_element(
		struct bt_value *array_obj);

extern enum bt_value_status bt_value_array_append_empty_map_element(
		struct bt_value *array_obj);

extern enum bt_value_status bt_value_array_set_element_by_index(
		struct bt_value *array_obj, uint64_t index,
		struct bt_value *element_obj);

extern struct bt_value *bt_value_map_create(void);

extern struct bt_value *bt_value_map_borrow_entry_value(
		struct bt_value *map_obj, const char *key);

typedef bt_bool (* bt_value_map_foreach_entry_cb)(const char *key,
		struct bt_value *object, void *data);

extern enum bt_value_status bt_value_map_foreach_entry(
		struct bt_value *map_obj,
		bt_value_map_foreach_entry_cb cb, void *data);

extern enum bt_value_status bt_value_map_insert_entry(
		struct bt_value *map_obj, const char *key,
		struct bt_value *element_obj);

extern enum bt_value_status bt_value_map_insert_bool_entry(
		struct bt_value *map_obj, const char *key, bt_bool val);

extern enum bt_value_status bt_value_map_insert_integer_entry(
		struct bt_value *map_obj, const char *key, int64_t val);

extern enum bt_value_status bt_value_map_insert_real_entry(
		struct bt_value *map_obj, const char *key, double val);

extern enum bt_value_status bt_value_map_insert_string_entry(
		struct bt_value *map_obj, const char *key,
		const char *val);

extern enum bt_value_status bt_value_map_insert_empty_array_entry(
		struct bt_value *map_obj, const char *key);

extern enum bt_value_status bt_value_map_insert_empty_map_entry(
		struct bt_value *map_obj, const char *key);

#ifdef __cplusplus
}
#endif

#endif /* BABELTRACE_VALUES_H */
