#ifndef BABELTRACE2_VALUE_CONST_H
#define BABELTRACE2_VALUE_CONST_H

/*
 * Copyright (c) 2010-2019 EfficiOS Inc. and Linux Foundation
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

#ifndef __BT_IN_BABELTRACE_H
# error "Please include <babeltrace2/babeltrace.h> instead."
#endif

#include <stdint.h>
#include <stddef.h>

#include <babeltrace2/types.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum bt_value_type {
	/// Null value object.
	BT_VALUE_TYPE_NULL		= 1 << 0,

	/// Boolean value object (holds #BT_TRUE or #BT_FALSE).
	BT_VALUE_TYPE_BOOL		= 1 << 1,

	BT_VALUE_TYPE_INTEGER		= 1 << 2,

	/// Unsigned integer value object (holds an unsigned 64-bit integer raw value).
	BT_VALUE_TYPE_UNSIGNED_INTEGER	= (1 << 3) | BT_VALUE_TYPE_INTEGER,

	/// Signed integer value object (holds a signed 64-bit integer raw value).
	BT_VALUE_TYPE_SIGNED_INTEGER	= (1 << 4) | BT_VALUE_TYPE_INTEGER,

	/// Floating point number value object (holds a \c double raw value).
	BT_VALUE_TYPE_REAL		= 1 << 5,

	/// String value object.
	BT_VALUE_TYPE_STRING		= 1 << 6,

	/// Array value object.
	BT_VALUE_TYPE_ARRAY		= 1 << 7,

	/// Map value object.
	BT_VALUE_TYPE_MAP		= 1 << 8,
} bt_value_type;

extern bt_value_type bt_value_get_type(const bt_value *object);

static inline
bt_bool bt_value_type_is(const bt_value_type type,
		const bt_value_type type_to_check)
{
	return (type & type_to_check) == type_to_check;
}

static inline
bt_bool bt_value_is_null(const bt_value *object)
{
	return bt_value_get_type(object) == BT_VALUE_TYPE_NULL;
}

static inline
bt_bool bt_value_is_bool(const bt_value *object)
{
	return bt_value_get_type(object) == BT_VALUE_TYPE_BOOL;
}

static inline
bt_bool bt_value_is_unsigned_integer(const bt_value *object)
{
	return bt_value_get_type(object) == BT_VALUE_TYPE_UNSIGNED_INTEGER;
}

static inline
bt_bool bt_value_is_signed_integer(const bt_value *object)
{
	return bt_value_get_type(object) == BT_VALUE_TYPE_SIGNED_INTEGER;
}

static inline
bt_bool bt_value_is_real(const bt_value *object)
{
	return bt_value_get_type(object) == BT_VALUE_TYPE_REAL;
}

static inline
bt_bool bt_value_is_string(const bt_value *object)
{
	return bt_value_get_type(object) == BT_VALUE_TYPE_STRING;
}

static inline
bt_bool bt_value_is_array(const bt_value *object)
{
	return bt_value_get_type(object) == BT_VALUE_TYPE_ARRAY;
}

static inline
bt_bool bt_value_is_map(const bt_value *object)
{
	return bt_value_get_type(object) == BT_VALUE_TYPE_MAP;
}

typedef enum bt_value_copy_status {
	BT_VALUE_COPY_STATUS_MEMORY_ERROR	= __BT_FUNC_STATUS_MEMORY_ERROR,
	BT_VALUE_COPY_STATUS_OK			= __BT_FUNC_STATUS_OK,
} bt_value_copy_status;

extern bt_value_copy_status bt_value_copy(const bt_value *object,
		bt_value **copy);

extern bt_bool bt_value_is_equal(const bt_value *object_a,
		const bt_value *object_b);

extern bt_bool bt_value_bool_get(const bt_value *bool_obj);

extern uint64_t bt_value_integer_unsigned_get(const bt_value *integer_obj);

extern int64_t bt_value_integer_signed_get(const bt_value *integer_obj);

extern double bt_value_real_get(const bt_value *real_obj);

extern const char *bt_value_string_get(const bt_value *string_obj);

extern uint64_t bt_value_array_get_length(const bt_value *array_obj);

static inline
bt_bool bt_value_array_is_empty(const bt_value *array_obj)
{
	return bt_value_array_get_length(array_obj) == 0;
}

extern const bt_value *bt_value_array_borrow_element_by_index_const(
		const bt_value *array_obj, uint64_t index);

extern uint64_t bt_value_map_get_size(const bt_value *map_obj);

static inline
bt_bool bt_value_map_is_empty(const bt_value *map_obj)
{
	return bt_value_map_get_size(map_obj) == 0;
}

extern const bt_value *bt_value_map_borrow_entry_value_const(
		const bt_value *map_obj, const char *key);

typedef bt_bool (* bt_value_map_foreach_entry_const_func)(const char *key,
		const bt_value *object, void *data);

typedef enum bt_value_map_foreach_entry_const_status {
	BT_VALUE_MAP_FOREACH_ENTRY_CONST_STATUS_OK		= __BT_FUNC_STATUS_OK,
	BT_VALUE_MAP_FOREACH_ENTRY_CONST_STATUS_INTERRUPTED	= __BT_FUNC_STATUS_INTERRUPTED,
} bt_value_map_foreach_entry_const_status;

extern bt_value_map_foreach_entry_const_status bt_value_map_foreach_entry_const(
		const bt_value *map_obj,
		bt_value_map_foreach_entry_const_func func, void *data);

extern bt_bool bt_value_map_has_entry(const bt_value *map_obj,
		const char *key);

extern void bt_value_get_ref(const bt_value *value);

extern void bt_value_put_ref(const bt_value *value);

#define BT_VALUE_PUT_REF_AND_RESET(_var)		\
	do {						\
		bt_value_put_ref(_var);			\
		(_var) = NULL;				\
	} while (0)

#define BT_VALUE_MOVE_REF(_var_dst, _var_src)		\
	do {						\
		bt_value_put_ref(_var_dst);		\
		(_var_dst) = (_var_src);		\
		(_var_src) = NULL;			\
	} while (0)

#ifdef __cplusplus
}
#endif

#endif /* BABELTRACE2_VALUE_CONST_H */
