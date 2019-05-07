#ifndef BABELTRACE_CTF_WRITER_VALUES_INTERNAL_H
#define BABELTRACE_CTF_WRITER_VALUES_INTERNAL_H

/*
 * Copyright (c) 2015-2017 EfficiOS Inc. and Linux Foundation
 * Copyright (c) 2015-2017 Philippe Proulx <pproulx@efficios.com>
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

#include <babeltrace/babeltrace-internal.h>

struct bt_ctf_value;
struct bt_ctf_private_value;

/**
@brief Status codes.
*/
enum bt_ctf_value_status {
	/// Operation canceled.
	BT_CTF_VALUE_STATUS_CANCELED	= 125,

	/// Cannot allocate memory.
	BT_CTF_VALUE_STATUS_NOMEM	= -12,

	/// Okay, no error.
	BT_CTF_VALUE_STATUS_OK		= 0,
};

BT_HIDDEN
enum bt_ctf_value_status _bt_ctf_value_freeze(struct bt_ctf_value *object);

#ifdef BT_DEV_MODE
# define bt_ctf_value_freeze	_bt_ctf_value_freeze
#else
# define bt_ctf_value_freeze(_value)
#endif /* BT_DEV_MODE */

extern struct bt_ctf_value *const bt_ctf_value_null;

enum bt_ctf_value_type {
	/// Null value object.
	BT_CTF_VALUE_TYPE_NULL =		0,

	/// Boolean value object (holds #BT_TRUE or #BT_FALSE).
	BT_CTF_VALUE_TYPE_BOOL =		1,

	/// Integer value object (holds a signed 64-bit integer raw value).
	BT_CTF_VALUE_TYPE_INTEGER =		2,

	/// Floating point number value object (holds a \c double raw value).
	BT_CTF_VALUE_TYPE_REAL =		3,

	/// String value object.
	BT_CTF_VALUE_TYPE_STRING =		4,

	/// Array value object.
	BT_CTF_VALUE_TYPE_ARRAY =		5,

	/// Map value object.
	BT_CTF_VALUE_TYPE_MAP =		6,
};

BT_HIDDEN
enum bt_ctf_value_type bt_ctf_value_get_type(const struct bt_ctf_value *object);

static inline
bt_bool bt_ctf_value_is_null(const struct bt_ctf_value *object)
{
	return bt_ctf_value_get_type(object) == BT_CTF_VALUE_TYPE_NULL;
}

static inline
bt_bool bt_ctf_value_is_bool(const struct bt_ctf_value *object)
{
	return bt_ctf_value_get_type(object) == BT_CTF_VALUE_TYPE_BOOL;
}

static inline
bt_bool bt_ctf_value_is_integer(const struct bt_ctf_value *object)
{
	return bt_ctf_value_get_type(object) == BT_CTF_VALUE_TYPE_INTEGER;
}

static inline
bt_bool bt_ctf_value_is_real(const struct bt_ctf_value *object)
{
	return bt_ctf_value_get_type(object) == BT_CTF_VALUE_TYPE_REAL;
}

static inline
bt_bool bt_ctf_value_is_string(const struct bt_ctf_value *object)
{
	return bt_ctf_value_get_type(object) == BT_CTF_VALUE_TYPE_STRING;
}

static inline
bt_bool bt_ctf_value_is_array(const struct bt_ctf_value *object)
{
	return bt_ctf_value_get_type(object) == BT_CTF_VALUE_TYPE_ARRAY;
}

static inline
bt_bool bt_ctf_value_is_map(const struct bt_ctf_value *object)
{
	return bt_ctf_value_get_type(object) == BT_CTF_VALUE_TYPE_MAP;
}

BT_HIDDEN
enum bt_ctf_value_status bt_ctf_value_copy(struct bt_ctf_private_value **copy,
		const struct bt_ctf_value *object);

BT_HIDDEN
bt_bool bt_ctf_value_compare(const struct bt_ctf_value *object_a,
		const struct bt_ctf_value *object_b);

BT_HIDDEN
bt_bool bt_ctf_value_bool_get(const struct bt_ctf_value *bool_obj);

BT_HIDDEN
int64_t bt_ctf_value_integer_get(const struct bt_ctf_value *integer_obj);

BT_HIDDEN
double bt_ctf_value_real_get(const struct bt_ctf_value *real_obj);

BT_HIDDEN
const char *bt_ctf_value_string_get(const struct bt_ctf_value *string_obj);

BT_HIDDEN
uint64_t bt_ctf_value_array_get_size(const struct bt_ctf_value *array_obj);

static inline
bt_bool bt_ctf_value_array_is_empty(const struct bt_ctf_value *array_obj)
{
	return bt_ctf_value_array_get_size(array_obj) == 0;
}

BT_HIDDEN
struct bt_ctf_value *bt_ctf_value_array_borrow_element_by_index(
		const struct bt_ctf_value *array_obj, uint64_t index);

BT_HIDDEN
uint64_t bt_ctf_value_map_get_size(const struct bt_ctf_value *map_obj);

static inline
bt_bool bt_ctf_value_map_is_empty(const struct bt_ctf_value *map_obj)
{
	return bt_ctf_value_map_get_size(map_obj) == 0;
}

BT_HIDDEN
struct bt_ctf_value *bt_ctf_value_map_borrow_entry_value(
		const struct bt_ctf_value *map_obj, const char *key);

typedef bt_bool (* bt_ctf_value_map_foreach_entry_cb)(const char *key,
	struct bt_ctf_value *object, void *data);

BT_HIDDEN
enum bt_ctf_value_status bt_ctf_value_map_foreach_entry(
		const struct bt_ctf_value *map_obj,
		bt_ctf_value_map_foreach_entry_cb cb, void *data);

BT_HIDDEN
bt_bool bt_ctf_value_map_has_entry(const struct bt_ctf_value *map_obj,
		const char *key);

BT_HIDDEN
enum bt_ctf_value_status bt_ctf_value_map_extend(
		struct bt_ctf_private_value **extended_map_obj,
		const struct bt_ctf_value *base_map_obj,
		const struct bt_ctf_value *extension_map_obj);


struct bt_ctf_value;
struct bt_ctf_private_value;

extern struct bt_ctf_private_value *const bt_ctf_private_value_null;

static inline
struct bt_ctf_value *bt_ctf_private_value_as_value(
		struct bt_ctf_private_value *priv_value)
{
	return (void *) priv_value;
}

BT_HIDDEN
struct bt_ctf_private_value *bt_ctf_private_value_bool_create(void);

BT_HIDDEN
struct bt_ctf_private_value *bt_ctf_private_value_bool_create_init(bt_bool val);

BT_HIDDEN
void bt_ctf_private_value_bool_set(struct bt_ctf_private_value *bool_obj,
		bt_bool val);

BT_HIDDEN
struct bt_ctf_private_value *bt_ctf_private_value_integer_create(void);

BT_HIDDEN
struct bt_ctf_private_value *bt_ctf_private_value_integer_create_init(
		int64_t val);

BT_HIDDEN
void bt_ctf_private_value_integer_set(
		struct bt_ctf_private_value *integer_obj, int64_t val);

BT_HIDDEN
struct bt_ctf_private_value *bt_ctf_private_value_real_create(void);

BT_HIDDEN
struct bt_ctf_private_value *bt_ctf_private_value_real_create_init(double val);

BT_HIDDEN
void bt_ctf_private_value_real_set(
		struct bt_ctf_private_value *real_obj, double val);

BT_HIDDEN
struct bt_ctf_private_value *bt_ctf_private_value_string_create(void);

BT_HIDDEN
struct bt_ctf_private_value *bt_ctf_private_value_string_create_init(
		const char *val);

BT_HIDDEN
enum bt_ctf_value_status bt_ctf_private_value_string_set(
		struct bt_ctf_private_value *string_obj,
		const char *val);

BT_HIDDEN
struct bt_ctf_private_value *bt_ctf_private_value_array_create(void);

BT_HIDDEN
struct bt_ctf_private_value *bt_ctf_private_value_array_borrow_element_by_index(
		const struct bt_ctf_private_value *array_obj, uint64_t index);

BT_HIDDEN
enum bt_ctf_value_status bt_ctf_private_value_array_append_element(
		struct bt_ctf_private_value *array_obj,
		struct bt_ctf_value *element_obj);

BT_HIDDEN
enum bt_ctf_value_status bt_ctf_private_value_array_append_bool_element(
		struct bt_ctf_private_value *array_obj,
		bt_bool val);

BT_HIDDEN
enum bt_ctf_value_status bt_ctf_private_value_array_append_integer_element(
		struct bt_ctf_private_value *array_obj,
		int64_t val);

BT_HIDDEN
enum bt_ctf_value_status bt_ctf_private_value_array_append_real_element(
		struct bt_ctf_private_value *array_obj,
		double val);

BT_HIDDEN
enum bt_ctf_value_status bt_ctf_private_value_array_append_string_element(
		struct bt_ctf_private_value *array_obj, const char *val);

BT_HIDDEN
enum bt_ctf_value_status bt_ctf_private_value_array_append_empty_array_element(
		struct bt_ctf_private_value *array_obj);

BT_HIDDEN
enum bt_ctf_value_status bt_ctf_private_value_array_append_empty_map_element(
		struct bt_ctf_private_value *array_obj);

BT_HIDDEN
enum bt_ctf_value_status bt_ctf_private_value_array_set_element_by_index(
		struct bt_ctf_private_value *array_obj, uint64_t index,
		struct bt_ctf_value *element_obj);

BT_HIDDEN
struct bt_ctf_private_value *bt_ctf_private_value_map_create(void);

BT_HIDDEN
struct bt_ctf_private_value *bt_ctf_private_value_map_borrow_entry_value(
		const struct bt_ctf_private_value *map_obj, const char *key);

typedef bt_bool (* bt_ctf_private_value_map_foreach_entry_cb)(const char *key,
		struct bt_ctf_private_value *object, void *data);

BT_HIDDEN
enum bt_ctf_value_status bt_ctf_private_value_map_foreach_entry(
		const struct bt_ctf_private_value *map_obj,
		bt_ctf_private_value_map_foreach_entry_cb cb, void *data);

BT_HIDDEN
enum bt_ctf_value_status bt_ctf_private_value_map_insert_entry(
		struct bt_ctf_private_value *map_obj, const char *key,
		struct bt_ctf_value *element_obj);

BT_HIDDEN
enum bt_ctf_value_status bt_ctf_private_value_map_insert_bool_entry(
		struct bt_ctf_private_value *map_obj, const char *key, bt_bool val);

BT_HIDDEN
enum bt_ctf_value_status bt_ctf_private_value_map_insert_integer_entry(
		struct bt_ctf_private_value *map_obj, const char *key, int64_t val);

BT_HIDDEN
enum bt_ctf_value_status bt_ctf_private_value_map_insert_real_entry(
		struct bt_ctf_private_value *map_obj, const char *key, double val);

BT_HIDDEN
enum bt_ctf_value_status bt_ctf_private_value_map_insert_string_entry(
		struct bt_ctf_private_value *map_obj, const char *key,
		const char *val);

BT_HIDDEN
enum bt_ctf_value_status bt_ctf_private_value_map_insert_empty_array_entry(
		struct bt_ctf_private_value *map_obj, const char *key);

BT_HIDDEN
enum bt_ctf_value_status bt_ctf_private_value_map_insert_empty_map_entry(
		struct bt_ctf_private_value *map_obj, const char *key);

static inline
const char *bt_ctf_value_type_string(enum bt_ctf_value_type type)
{
	switch (type) {
	case BT_CTF_VALUE_TYPE_NULL:
		return "BT_CTF_VALUE_TYPE_NULL";
	case BT_CTF_VALUE_TYPE_BOOL:
		return "BT_CTF_VALUE_TYPE_BOOL";
	case BT_CTF_VALUE_TYPE_INTEGER:
		return "BT_CTF_VALUE_TYPE_INTEGER";
	case BT_CTF_VALUE_TYPE_REAL:
		return "BT_CTF_VALUE_TYPE_REAL";
	case BT_CTF_VALUE_TYPE_STRING:
		return "BT_CTF_VALUE_TYPE_STRING";
	case BT_CTF_VALUE_TYPE_ARRAY:
		return "BT_CTF_VALUE_TYPE_ARRAY";
	case BT_CTF_VALUE_TYPE_MAP:
		return "BT_CTF_VALUE_TYPE_MAP";
	default:
		return "(unknown)";
	}
};

#endif /* BABELTRACE_CTF_WRITER_VALUES_INTERNAL_H */
