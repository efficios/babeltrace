#ifndef BABELTRACE_VALUES_CONST_H
#define BABELTRACE_VALUES_CONST_H

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

/* For bt_bool */
#include <babeltrace/types.h>

#ifdef __cplusplus
extern "C" {
#endif

struct bt_value;

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

extern enum bt_value_type bt_value_get_type(const struct bt_value *object);

static inline
bt_bool bt_value_is_null(const struct bt_value *object)
{
	return bt_value_get_type(object) == BT_VALUE_TYPE_NULL;
}

static inline
bt_bool bt_value_is_bool(const struct bt_value *object)
{
	return bt_value_get_type(object) == BT_VALUE_TYPE_BOOL;
}

static inline
bt_bool bt_value_is_integer(const struct bt_value *object)
{
	return bt_value_get_type(object) == BT_VALUE_TYPE_INTEGER;
}

static inline
bt_bool bt_value_is_real(const struct bt_value *object)
{
	return bt_value_get_type(object) == BT_VALUE_TYPE_REAL;
}

static inline
bt_bool bt_value_is_string(const struct bt_value *object)
{
	return bt_value_get_type(object) == BT_VALUE_TYPE_STRING;
}

static inline
bt_bool bt_value_is_array(const struct bt_value *object)
{
	return bt_value_get_type(object) == BT_VALUE_TYPE_ARRAY;
}

static inline
bt_bool bt_value_is_map(const struct bt_value *object)
{
	return bt_value_get_type(object) == BT_VALUE_TYPE_MAP;
}

extern enum bt_value_status bt_value_copy(const struct bt_value *object,
		struct bt_value **copy);

extern bt_bool bt_value_compare(const struct bt_value *object_a,
		const struct bt_value *object_b);

extern bt_bool bt_value_bool_get(const struct bt_value *bool_obj);

extern int64_t bt_value_integer_get(const struct bt_value *integer_obj);

extern double bt_value_real_get(const struct bt_value *real_obj);

extern const char *bt_value_string_get(const struct bt_value *string_obj);

extern uint64_t bt_value_array_get_size(const struct bt_value *array_obj);

static inline
bt_bool bt_value_array_is_empty(const struct bt_value *array_obj)
{
	return bt_value_array_get_size(array_obj) == 0;
}

extern const struct bt_value *bt_value_array_borrow_element_by_index_const(
		const struct bt_value *array_obj, uint64_t index);

extern uint64_t bt_value_map_get_size(const struct bt_value *map_obj);

static inline
bt_bool bt_value_map_is_empty(const struct bt_value *map_obj)
{
	return bt_value_map_get_size(map_obj) == 0;
}

extern const struct bt_value *bt_value_map_borrow_entry_value_const(
		const struct bt_value *map_obj, const char *key);

typedef bt_bool (* bt_value_map_foreach_entry_const_func)(const char *key,
		const struct bt_value *object, void *data);

extern enum bt_value_status bt_value_map_foreach_entry_const(
		const struct bt_value *map_obj,
		bt_value_map_foreach_entry_const_func func, void *data);

extern bt_bool bt_value_map_has_entry(const struct bt_value *map_obj,
		const char *key);

extern enum bt_value_status bt_value_map_extend(
		const struct bt_value *base_map_obj,
		const struct bt_value *extension_map_obj,
		struct bt_value **extended_map_obj);

#ifdef __cplusplus
}
#endif

#endif /* BABELTRACE_VALUES_CONST_H */
