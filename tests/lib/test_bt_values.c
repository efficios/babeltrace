/*
 * test_bt_values.c
 *
 * Babeltrace value value objects tests
 *
 * Copyright (c) 2015 EfficiOS Inc. and Linux Foundation
 * Copyright (c) 2015 Philippe Proulx <pproulx@efficios.com>
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

#include <babeltrace/values.h>
#include <assert.h>
#include <string.h>
#include "tap/tap.h"

static
void test_null(void)
{
	ok(bt_value_null, "bt_value_null is not NULL");
	ok(bt_value_is_null(bt_value_null),
		"bt_value_null is a null value object");
	bt_get(bt_value_null);
	pass("getting bt_value_null does not cause a crash");
	bt_put(bt_value_null);
	pass("putting bt_value_null does not cause a crash");

	bt_get(NULL);
	pass("getting NULL does not cause a crash");
	bt_put(NULL);
	pass("putting NULL does not cause a crash");

	ok(bt_value_get_type(NULL) == BT_VALUE_TYPE_UNKNOWN,
		"bt_value_get_type(NULL) returns BT_VALUE_TYPE_UNKNOWN");
}

static
void test_bool(void)
{
	int ret;
	bool value;
	struct bt_value *obj;

	obj = bt_value_bool_create();
	ok(obj && bt_value_is_bool(obj),
		"bt_value_bool_create() returns a boolean value object");

	value = true;
	ret = bt_value_bool_get(obj, &value);
	ok(!ret && !value, "default boolean value object value is false");

	ret = bt_value_bool_set(NULL, true);
	ok(ret == BT_VALUE_STATUS_INVAL,
		"bt_value_bool_set() fails with an value object set to NULL");
	ret = bt_value_bool_get(NULL, &value);
	ok(ret == BT_VALUE_STATUS_INVAL,
		"bt_value_bool_get() fails with an value object set to NULL");
	ret = bt_value_bool_get(obj, NULL);
	ok(ret == BT_VALUE_STATUS_INVAL,
		"bt_value_bool_get() fails with a return value set to NULL");

	assert(!bt_value_bool_set(obj, false));
	ret = bt_value_bool_set(obj, true);
	ok(!ret, "bt_value_bool_set() succeeds");
	ret = bt_value_bool_get(obj, &value);
	ok(!ret && value, "bt_value_bool_set() works");

	BT_PUT(obj);
	pass("putting an existing boolean value object does not cause a crash")

	value = false;
	obj = bt_value_bool_create_init(true);
	ok(obj && bt_value_is_bool(obj),
		"bt_value_bool_create_init() returns a boolean value object");
	ret = bt_value_bool_get(obj, &value);
	ok(!ret && value,
		"bt_value_bool_create_init() sets the appropriate initial value");

	assert(!bt_value_freeze(obj));
	ok(bt_value_bool_set(obj, false) == BT_VALUE_STATUS_FROZEN,
		"bt_value_bool_set() cannot be called on a frozen boolean value object");
	value = false;
	ret = bt_value_bool_get(obj, &value);
	ok(!ret && value,
		"bt_value_bool_set() does not alter a frozen floating point number value object");

	BT_PUT(obj);
}

static
void test_integer(void)
{
	int ret;
	int64_t value;
	struct bt_value *obj;

	obj = bt_value_integer_create();
	ok(obj && bt_value_is_integer(obj),
		"bt_value_integer_create() returns an integer value object");

	ret = bt_value_integer_set(NULL, -12345);
	ok(ret == BT_VALUE_STATUS_INVAL,
		"bt_value_integer_set() fails with an value object set to NULL");
	ret = bt_value_integer_get(NULL, &value);
	ok(ret == BT_VALUE_STATUS_INVAL,
		"bt_value_integer_get() fails with an value object set to NULL");
	ret = bt_value_integer_get(obj, NULL);
	ok(ret == BT_VALUE_STATUS_INVAL,
		"bt_value_integer_get() fails with a return value set to NULL");

	value = 1961;
	ret = bt_value_integer_get(obj, &value);
	ok(!ret && value == 0, "default integer value object value is 0");

	ret = bt_value_integer_set(obj, -98765);
	ok(!ret, "bt_value_integer_set() succeeds");
	ret = bt_value_integer_get(obj, &value);
	ok(!ret && value == -98765, "bt_value_integer_set() works");

	BT_PUT(obj);
	pass("putting an existing integer value object does not cause a crash")

	obj = bt_value_integer_create_init(321456987);
	ok(obj && bt_value_is_integer(obj),
		"bt_value_integer_create_init() returns an integer value object");
	ret = bt_value_integer_get(obj, &value);
	ok(!ret && value == 321456987,
		"bt_value_integer_create_init() sets the appropriate initial value");

	assert(!bt_value_freeze(obj));
	ok(bt_value_integer_set(obj, 18276) == BT_VALUE_STATUS_FROZEN,
		"bt_value_integer_set() cannot be called on a frozen integer value object");
	value = 17;
	ret = bt_value_integer_get(obj, &value);
	ok(!ret && value == 321456987,
		"bt_value_integer_set() does not alter a frozen integer value object");

	BT_PUT(obj);
}

static
void test_float(void)
{
	int ret;
	double value;
	struct bt_value *obj;

	obj = bt_value_float_create();
	ok(obj && bt_value_is_float(obj),
		"bt_value_float_create() returns a floating point number value object");

	ret = bt_value_float_set(NULL, 1.2345);
	ok(ret == BT_VALUE_STATUS_INVAL,
		"bt_value_float_set() fails with an value object set to NULL");
	ret = bt_value_float_get(NULL, &value);
	ok(ret == BT_VALUE_STATUS_INVAL,
		"bt_value_float_get() fails with an value object set to NULL");
	ret = bt_value_float_get(obj, NULL);
	ok(ret == BT_VALUE_STATUS_INVAL,
		"bt_value_float_get() fails with a return value set to NULL");

	value = 17.34;
	ret = bt_value_float_get(obj, &value);
	ok(!ret && value == 0.,
		"default floating point number value object value is 0");

	ret = bt_value_float_set(obj, -3.1416);
	ok(!ret, "bt_value_float_set() succeeds");
	ret = bt_value_float_get(obj, &value);
	ok(!ret && value == -3.1416, "bt_value_float_set() works");

	BT_PUT(obj);
	pass("putting an existing floating point number value object does not cause a crash")

	obj = bt_value_float_create_init(33.1649758);
	ok(obj && bt_value_is_float(obj),
		"bt_value_float_create_init() returns a floating point number value object");
	ret = bt_value_float_get(obj, &value);
	ok(!ret && value == 33.1649758,
		"bt_value_float_create_init() sets the appropriate initial value");

	assert(!bt_value_freeze(obj));
	ok(bt_value_float_set(obj, 17.88) == BT_VALUE_STATUS_FROZEN,
		"bt_value_float_set() fails with a frozen floating point number value object");
	value = 1.2;
	ret = bt_value_float_get(obj, &value);
	ok(!ret && value == 33.1649758,
		"bt_value_float_set() does not alter a frozen floating point number value object");

	BT_PUT(obj);
}

static
void test_string(void)
{
	int ret;
	const char *value;
	struct bt_value *obj;

	obj = bt_value_string_create();
	ok(obj && bt_value_is_string(obj),
		"bt_value_string_create() returns a string value object");

	ret = bt_value_string_set(NULL, "hoho");
	ok(ret == BT_VALUE_STATUS_INVAL,
		"bt_value_string_set() fails with an value object set to NULL");
	ret = bt_value_string_set(obj, NULL);
	ok(ret == BT_VALUE_STATUS_INVAL,
		"bt_value_string_set() fails with a value set to NULL");
	ret = bt_value_string_get(NULL, &value);
	ok(ret == BT_VALUE_STATUS_INVAL,
		"bt_value_string_get() fails with an value object set to NULL");
	ret = bt_value_string_get(obj, NULL);
	ok(ret == BT_VALUE_STATUS_INVAL,
		"bt_value_string_get() fails with a return value set to NULL");

	ret = bt_value_string_get(obj, &value);
	ok(!ret && value && !strcmp(value, ""),
		"default string value object value is \"\"");

	ret = bt_value_string_set(obj, "hello worldz");
	ok(!ret, "bt_value_string_set() succeeds");
	ret = bt_value_string_get(obj, &value);
	ok(!ret && value && !strcmp(value, "hello worldz"),
		"bt_value_string_get() works");

	BT_PUT(obj);
	pass("putting an existing string value object does not cause a crash")

	obj = bt_value_string_create_init(NULL);
	ok(!obj, "bt_value_string_create_init() fails with an initial value set to NULL");
	obj = bt_value_string_create_init("initial value");
	ok(obj && bt_value_is_string(obj),
		"bt_value_string_create_init() returns a string value object");
	ret = bt_value_string_get(obj, &value);
	ok(!ret && value && !strcmp(value, "initial value"),
		"bt_value_string_create_init() sets the appropriate initial value");

	assert(!bt_value_freeze(obj));
	ok(bt_value_string_set(obj, "new value") == BT_VALUE_STATUS_FROZEN,
		"bt_value_string_set() fails with a frozen string value object");
	value = "";
	ret = bt_value_string_get(obj, &value);
	ok(!ret && value && !strcmp(value, "initial value"),
		"bt_value_string_set() does not alter a frozen string value object");

	BT_PUT(obj);
}

static
void test_array(void)
{
	int ret;
	bool bool_value;
	int64_t int_value;
	double float_value;
	struct bt_value *obj;
	const char *string_value;
	struct bt_value *array_obj;

	array_obj = bt_value_array_create();
	ok(array_obj && bt_value_is_array(array_obj),
		"bt_value_array_create() returns an array value object");
	ok(bt_value_array_is_empty(NULL) == false,
		"bt_value_array_is_empty() returns false with an value object set to NULL");
	ok(bt_value_array_is_empty(array_obj),
		"initial array value object size is 0");
	ok(bt_value_array_size(NULL) == BT_VALUE_STATUS_INVAL,
		"bt_value_array_size() fails with an array value object set to NULL");

	ok(bt_value_array_append(NULL, bt_value_null)
		== BT_VALUE_STATUS_INVAL,
		"bt_value_array_append() fails with an array value object set to NULL");
	ok(bt_value_array_append(array_obj, NULL) == BT_VALUE_STATUS_INVAL,
		"bt_value_array_append() fails with a value set to NULL");

	obj = bt_value_integer_create_init(345);
	ret = bt_value_array_append(array_obj, obj);
	BT_PUT(obj);
	obj = bt_value_float_create_init(-17.45);
	ret |= bt_value_array_append(array_obj, obj);
	BT_PUT(obj);
	obj = bt_value_bool_create_init(true);
	ret |= bt_value_array_append(array_obj, obj);
	BT_PUT(obj);
	ret |= bt_value_array_append(array_obj, bt_value_null);
	ok(!ret, "bt_value_array_append() succeeds");
	ok(bt_value_array_size(array_obj) == 4,
		"appending an element to an array value object increment its size");

	obj = bt_value_array_get(array_obj, 4);
	ok(!obj, "getting an array value object's element at an index equal to its size fails");
	obj = bt_value_array_get(array_obj, 5);
	ok(!obj, "getting an array value object's element at a larger index fails");

	obj = bt_value_array_get(NULL, 2);
	ok(!obj, "bt_value_array_get() fails with an array value object set to NULL");

	obj = bt_value_array_get(array_obj, 0);
	ok(obj && bt_value_is_integer(obj),
		"bt_value_array_get() returns an value object with the appropriate type (integer)");
	ret = bt_value_integer_get(obj, &int_value);
	ok(!ret && int_value == 345,
		"bt_value_array_get() returns an value object with the appropriate value (integer)");
	BT_PUT(obj);
	obj = bt_value_array_get(array_obj, 1);
	ok(obj && bt_value_is_float(obj),
		"bt_value_array_get() returns an value object with the appropriate type (floating point number)");
	ret = bt_value_float_get(obj, &float_value);
	ok(!ret && float_value == -17.45,
		"bt_value_array_get() returns an value object with the appropriate value (floating point number)");
	BT_PUT(obj);
	obj = bt_value_array_get(array_obj, 2);
	ok(obj && bt_value_is_bool(obj),
		"bt_value_array_get() returns an value object with the appropriate type (boolean)");
	ret = bt_value_bool_get(obj, &bool_value);
	ok(!ret && bool_value,
		"bt_value_array_get() returns an value object with the appropriate value (boolean)");
	BT_PUT(obj);
	obj = bt_value_array_get(array_obj, 3);
	ok(obj == bt_value_null,
		"bt_value_array_get() returns an value object with the appropriate type (null)");

	ok(bt_value_array_set(NULL, 0, bt_value_null) ==
		BT_VALUE_STATUS_INVAL,
		"bt_value_array_set() fails with an array value object set to NULL");
	ok(bt_value_array_set(array_obj, 0, NULL) == BT_VALUE_STATUS_INVAL,
		"bt_value_array_set() fails with an element value object set to NULL");
	ok(bt_value_array_set(array_obj, 4, bt_value_null) ==
		BT_VALUE_STATUS_INVAL,
		"bt_value_array_set() fails with an invalid index");
	obj = bt_value_integer_create_init(1001);
	assert(obj);
	ok(!bt_value_array_set(array_obj, 2, obj),
		"bt_value_array_set() succeeds");
	BT_PUT(obj);
	obj = bt_value_array_get(array_obj, 2);
	ok(obj && bt_value_is_integer(obj),
		"bt_value_array_set() inserts an value object with the appropriate type");
	ret = bt_value_integer_get(obj, &int_value);
	assert(!ret);
	ok(int_value == 1001,
		"bt_value_array_set() inserts an value object with the appropriate value");
	BT_PUT(obj);

	ret = bt_value_array_append_bool(array_obj, false);
	ok(!ret, "bt_value_array_append_bool() succeeds");
	ok(bt_value_array_append_bool(NULL, true) == BT_VALUE_STATUS_INVAL,
		"bt_value_array_append_bool() fails with an array value object set to NULL");
	ret = bt_value_array_append_integer(array_obj, 98765);
	ok(!ret, "bt_value_array_append_integer() succeeds");
	ok(bt_value_array_append_integer(NULL, 18765) ==
		BT_VALUE_STATUS_INVAL,
		"bt_value_array_append_integer() fails with an array value object set to NULL");
	ret = bt_value_array_append_float(array_obj, 2.49578);
	ok(!ret, "bt_value_array_append_float() succeeds");
	ok(bt_value_array_append_float(NULL, 1.49578) ==
		BT_VALUE_STATUS_INVAL,
		"bt_value_array_append_float() fails with an array value object set to NULL");
	ret = bt_value_array_append_string(array_obj, "bt_value");
	ok(!ret, "bt_value_array_append_string() succeeds");
	ok(bt_value_array_append_string(NULL, "bt_obj") ==
		BT_VALUE_STATUS_INVAL,
		"bt_value_array_append_string() fails with an array value object set to NULL");
	ret = bt_value_array_append_array(array_obj);
	ok(!ret, "bt_value_array_append_array() succeeds");
	ok(bt_value_array_append_array(NULL) == BT_VALUE_STATUS_INVAL,
		"bt_value_array_append_array() fails with an array value object set to NULL");
	ret = bt_value_array_append_map(array_obj);
	ok(!ret, "bt_value_array_append_map() succeeds");
	ok(bt_value_array_append_map(NULL) == BT_VALUE_STATUS_INVAL,
		"bt_value_array_append_map() fails with an array value object set to NULL");

	ok(bt_value_array_size(array_obj) == 10,
		"the bt_value_array_append_*() functions increment the array value object's size");
	ok(!bt_value_array_is_empty(array_obj),
		"map value object is not empty");

	obj = bt_value_array_get(array_obj, 4);
	ok(obj && bt_value_is_bool(obj),
		"bt_value_array_append_bool() appends a boolean value object");
	ret = bt_value_bool_get(obj, &bool_value);
	ok(!ret && !bool_value,
		"bt_value_array_append_bool() appends the appropriate value");
	BT_PUT(obj);
	obj = bt_value_array_get(array_obj, 5);
	ok(obj && bt_value_is_integer(obj),
		"bt_value_array_append_integer() appends an integer value object");
	ret = bt_value_integer_get(obj, &int_value);
	ok(!ret && int_value == 98765,
		"bt_value_array_append_integer() appends the appropriate value");
	BT_PUT(obj);
	obj = bt_value_array_get(array_obj, 6);
	ok(obj && bt_value_is_float(obj),
		"bt_value_array_append_float() appends a floating point number value object");
	ret = bt_value_float_get(obj, &float_value);
	ok(!ret && float_value == 2.49578,
		"bt_value_array_append_float() appends the appropriate value");
	BT_PUT(obj);
	obj = bt_value_array_get(array_obj, 7);
	ok(obj && bt_value_is_string(obj),
		"bt_value_array_append_string() appends a string value object");
	ret = bt_value_string_get(obj, &string_value);
	ok(!ret && string_value && !strcmp(string_value, "bt_value"),
		"bt_value_array_append_string() appends the appropriate value");
	BT_PUT(obj);
	obj = bt_value_array_get(array_obj, 8);
	ok(obj && bt_value_is_array(obj),
		"bt_value_array_append_array() appends an array value object");
	ok(bt_value_array_is_empty(obj),
		"bt_value_array_append_array() an empty array value object");
	BT_PUT(obj);
	obj = bt_value_array_get(array_obj, 9);
	ok(obj && bt_value_is_map(obj),
		"bt_value_array_append_map() appends a map value object");
	ok(bt_value_map_is_empty(obj),
		"bt_value_array_append_map() an empty map value object");
	BT_PUT(obj);

	assert(!bt_value_freeze(array_obj));
	ok(bt_value_array_append(array_obj, bt_value_null) ==
		BT_VALUE_STATUS_FROZEN,
		"bt_value_array_append() fails with a frozen array value object");
	ok(bt_value_array_append_bool(array_obj, false) ==
		BT_VALUE_STATUS_FROZEN,
		"bt_value_array_append_bool() fails with a frozen array value object");
	ok(bt_value_array_append_integer(array_obj, 23) ==
		BT_VALUE_STATUS_FROZEN,
		"bt_value_array_append_integer() fails with a frozen array value object");
	ok(bt_value_array_append_float(array_obj, 2.34) ==
		BT_VALUE_STATUS_FROZEN,
		"bt_value_array_append_float() fails with a frozen array value object");
	ok(bt_value_array_append_string(array_obj, "yayayayaya") ==
		BT_VALUE_STATUS_FROZEN,
		"bt_value_array_append_string() fails with a frozen array value object");
	ok(bt_value_array_append_array(array_obj) ==
		BT_VALUE_STATUS_FROZEN,
		"bt_value_array_append_array() fails with a frozen array value object");
	ok(bt_value_array_append_map(array_obj) ==
		BT_VALUE_STATUS_FROZEN,
		"bt_value_array_append_map() fails with a frozen array value object");
	ok(bt_value_array_set(array_obj, 2, bt_value_null) ==
		BT_VALUE_STATUS_FROZEN,
		"bt_value_array_set() fails with a frozen array value object");
	ok(bt_value_array_size(array_obj) == 10,
		"appending to a frozen array value object does not change its size");

	obj = bt_value_array_get(array_obj, 1);
	assert(obj);
	ok(bt_value_float_set(obj, 14.52) == BT_VALUE_STATUS_FROZEN,
		"freezing an array value object also freezes its elements");
	BT_PUT(obj);

	BT_PUT(array_obj);
	pass("putting an existing array value object does not cause a crash")
}

static
bool test_map_foreach_cb_count(const char *key, struct bt_value *object,
	void *data)
{
	int *count = data;

	if (*count == 3) {
		return false;
	}

	(*count)++;

	return true;
}

struct map_foreach_checklist {
	bool bool1;
	bool int1;
	bool float1;
	bool null1;
	bool bool2;
	bool int2;
	bool float2;
	bool string2;
	bool array2;
	bool map2;
};

static
bool test_map_foreach_cb_check(const char *key, struct bt_value *object,
	void *data)
{
	int ret;
	struct map_foreach_checklist *checklist = data;

	if (!strcmp(key, "bool")) {
		if (checklist->bool1) {
			fail("test_map_foreach_cb_check(): duplicate key \"bool\"");
		} else {
			bool val = false;

			ret = bt_value_bool_get(object, &val);
			ok(!ret, "test_map_foreach_cb_check(): success getting \"bool\" value");

			if (val) {
				pass("test_map_foreach_cb_check(): \"bool\" value object has the right value");
				checklist->bool1 = true;
			} else {
				fail("test_map_foreach_cb_check(): \"bool\" value object has the wrong value");
			}
		}
	} else if (!strcmp(key, "int")) {
		if (checklist->int1) {
			fail("test_map_foreach_cb_check(): duplicate key \"int\"");
		} else {
			int64_t val = 0;

			ret = bt_value_integer_get(object, &val);
			ok(!ret, "test_map_foreach_cb_check(): success getting \"int\" value");

			if (val == 19457) {
				pass("test_map_foreach_cb_check(): \"int\" value object has the right value");
				checklist->int1 = true;
			} else {
				fail("test_map_foreach_cb_check(): \"int\" value object has the wrong value");
			}
		}
	} else if (!strcmp(key, "float")) {
		if (checklist->float1) {
			fail("test_map_foreach_cb_check(): duplicate key \"float\"");
		} else {
			double val = 0;

			ret = bt_value_float_get(object, &val);
			ok(!ret, "test_map_foreach_cb_check(): success getting \"float\" value");

			if (val == 5.444) {
				pass("test_map_foreach_cb_check(): \"float\" value object has the right value");
				checklist->float1 = true;
			} else {
				fail("test_map_foreach_cb_check(): \"float\" value object has the wrong value");
			}
		}
	} else if (!strcmp(key, "null")) {
		if (checklist->null1) {
			fail("test_map_foreach_cb_check(): duplicate key \"bool\"");
		} else {
			ok(bt_value_is_null(object), "test_map_foreach_cb_check(): success getting \"null\" value object");
			checklist->null1 = true;
		}
	} else if (!strcmp(key, "bool2")) {
		if (checklist->bool2) {
			fail("test_map_foreach_cb_check(): duplicate key \"bool2\"");
		} else {
			bool val = false;

			ret = bt_value_bool_get(object, &val);
			ok(!ret, "test_map_foreach_cb_check(): success getting \"bool2\" value");

			if (val) {
				pass("test_map_foreach_cb_check(): \"bool2\" value object has the right value");
				checklist->bool2 = true;
			} else {
				fail("test_map_foreach_cb_check(): \"bool2\" value object has the wrong value");
			}
		}
	} else if (!strcmp(key, "int2")) {
		if (checklist->int2) {
			fail("test_map_foreach_cb_check(): duplicate key \"int2\"");
		} else {
			int64_t val = 0;

			ret = bt_value_integer_get(object, &val);
			ok(!ret, "test_map_foreach_cb_check(): success getting \"int2\" value");

			if (val == 98765) {
				pass("test_map_foreach_cb_check(): \"int2\" value object has the right value");
				checklist->int2 = true;
			} else {
				fail("test_map_foreach_cb_check(): \"int2\" value object has the wrong value");
			}
		}
	} else if (!strcmp(key, "float2")) {
		if (checklist->float2) {
			fail("test_map_foreach_cb_check(): duplicate key \"float2\"");
		} else {
			double val = 0;

			ret = bt_value_float_get(object, &val);
			ok(!ret, "test_map_foreach_cb_check(): success getting \"float2\" value");

			if (val == -49.0001) {
				pass("test_map_foreach_cb_check(): \"float2\" value object has the right value");
				checklist->float2 = true;
			} else {
				fail("test_map_foreach_cb_check(): \"float2\" value object has the wrong value");
			}
		}
	} else if (!strcmp(key, "string2")) {
		if (checklist->string2) {
			fail("test_map_foreach_cb_check(): duplicate key \"string2\"");
		} else {
			const char *val;

			ret = bt_value_string_get(object, &val);
			ok(!ret, "test_map_foreach_cb_check(): success getting \"string2\" value");

			if (val && !strcmp(val, "bt_value")) {
				pass("test_map_foreach_cb_check(): \"string2\" value object has the right value");
				checklist->string2 = true;
			} else {
				fail("test_map_foreach_cb_check(): \"string2\" value object has the wrong value");
			}
		}
	} else if (!strcmp(key, "array2")) {
		if (checklist->array2) {
			fail("test_map_foreach_cb_check(): duplicate key \"array2\"");
		} else {
			ok(bt_value_is_array(object), "test_map_foreach_cb_check(): success getting \"array2\" value object");
			ok(bt_value_array_is_empty(object),
				"test_map_foreach_cb_check(): \"array2\" value object is empty");
			checklist->array2 = true;
		}
	} else if (!strcmp(key, "map2")) {
		if (checklist->map2) {
			fail("test_map_foreach_cb_check(): duplicate key \"map2\"");
		} else {
			ok(bt_value_is_map(object), "test_map_foreach_cb_check(): success getting \"map2\" value object");
			ok(bt_value_map_is_empty(object),
				"test_map_foreach_cb_check(): \"map2\" value object is empty");
			checklist->map2 = true;
		}
	} else {
		fail("test_map_foreach_cb_check(): unknown map key \"%s\"", key);
	}

	return true;
}

static
void test_map(void)
{
	int ret;
	int count = 0;
	bool bool_value;
	int64_t int_value;
	double float_value;
	struct bt_value *obj;
	struct bt_value *map_obj;
	struct map_foreach_checklist checklist;

	map_obj = bt_value_map_create();
	ok(map_obj && bt_value_is_map(map_obj),
		"bt_value_map_create() returns a map value object");
	ok(bt_value_map_size(map_obj) == 0,
		"initial map value object size is 0");
	ok(bt_value_map_size(NULL) == BT_VALUE_STATUS_INVAL,
		"bt_value_map_size() fails with a map value object set to NULL");

	ok(bt_value_map_insert(NULL, "hello", bt_value_null) ==
		BT_VALUE_STATUS_INVAL,
		"bt_value_array_insert() fails with a map value object set to NULL");
	ok(bt_value_map_insert(map_obj, NULL, bt_value_null) ==
		BT_VALUE_STATUS_INVAL,
		"bt_value_array_insert() fails with a key set to NULL");
	ok(bt_value_map_insert(map_obj, "yeah", NULL) ==
		BT_VALUE_STATUS_INVAL,
		"bt_value_array_insert() fails with an element value object set to NULL");

	obj = bt_value_integer_create_init(19457);
	ret = bt_value_map_insert(map_obj, "int", obj);
	BT_PUT(obj);
	obj = bt_value_float_create_init(5.444);
	ret |= bt_value_map_insert(map_obj, "float", obj);
	BT_PUT(obj);
	obj = bt_value_bool_create();
	ret |= bt_value_map_insert(map_obj, "bool", obj);
	BT_PUT(obj);
	ret |= bt_value_map_insert(map_obj, "null", bt_value_null);
	ok(!ret, "bt_value_map_insert() succeeds");
	ok(bt_value_map_size(map_obj) == 4,
		"inserting an element into a map value object increment its size");

	obj = bt_value_bool_create_init(true);
	ret = bt_value_map_insert(map_obj, "bool", obj);
	BT_PUT(obj);
	ok(!ret, "bt_value_map_insert() accepts an existing key");

	obj = bt_value_map_get(map_obj, NULL);
	ok(!obj, "bt_value_map_get() fails with a key set to NULL");
	obj = bt_value_map_get(NULL, "bool");
	ok(!obj, "bt_value_map_get() fails with a map value object set to NULL");

	obj = bt_value_map_get(map_obj, "life");
	ok(!obj, "bt_value_map_get() fails with an non existing key");
	obj = bt_value_map_get(map_obj, "float");
	ok(obj && bt_value_is_float(obj),
		"bt_value_map_get() returns an value object with the appropriate type (float)");
	ret = bt_value_float_get(obj, &float_value);
	ok(!ret && float_value == 5.444,
		"bt_value_map_get() returns an value object with the appropriate value (float)");
	BT_PUT(obj);
	obj = bt_value_map_get(map_obj, "int");
	ok(obj && bt_value_is_integer(obj),
		"bt_value_map_get() returns an value object with the appropriate type (integer)");
	ret = bt_value_integer_get(obj, &int_value);
	ok(!ret && int_value == 19457,
		"bt_value_map_get() returns an value object with the appropriate value (integer)");
	BT_PUT(obj);
	obj = bt_value_map_get(map_obj, "null");
	ok(obj && bt_value_is_null(obj),
		"bt_value_map_get() returns an value object with the appropriate type (null)");
	obj = bt_value_map_get(map_obj, "bool");
	ok(obj && bt_value_is_bool(obj),
		"bt_value_map_get() returns an value object with the appropriate type (boolean)");
	ret = bt_value_bool_get(obj, &bool_value);
	ok(!ret && bool_value,
		"bt_value_map_get() returns an value object with the appropriate value (boolean)");
	BT_PUT(obj);

	ret = bt_value_map_insert_bool(map_obj, "bool2", true);
	ok(!ret, "bt_value_map_insert_bool() succeeds");
	ok(bt_value_map_insert_bool(NULL, "bool2", false) ==
		BT_VALUE_STATUS_INVAL,
		"bt_value_map_insert_bool() fails with a map value object set to NULL");
	ret = bt_value_map_insert_integer(map_obj, "int2", 98765);
	ok(!ret, "bt_value_map_insert_integer() succeeds");
	ok(bt_value_map_insert_integer(NULL, "int2", 1001) ==
		BT_VALUE_STATUS_INVAL,
		"bt_value_map_insert_integer() fails with a map value object set to NULL");
	ret = bt_value_map_insert_float(map_obj, "float2", -49.0001);
	ok(!ret, "bt_value_map_insert_float() succeeds");
	ok(bt_value_map_insert_float(NULL, "float2", 495) ==
		BT_VALUE_STATUS_INVAL,
		"bt_value_map_insert_float() fails with a map value object set to NULL");
	ret = bt_value_map_insert_string(map_obj, "string2", "bt_value");
	ok(!ret, "bt_value_map_insert_string() succeeds");
	ok(bt_value_map_insert_string(NULL, "string2", "bt_obj") ==
		BT_VALUE_STATUS_INVAL,
		"bt_value_map_insert_string() fails with a map value object set to NULL");
	ret = bt_value_map_insert_array(map_obj, "array2");
	ok(!ret, "bt_value_map_insert_array() succeeds");
	ok(bt_value_map_insert_array(NULL, "array2") == BT_VALUE_STATUS_INVAL,
		"bt_value_map_insert_array() fails with a map value object set to NULL");
	ret = bt_value_map_insert_map(map_obj, "map2");
	ok(!ret, "bt_value_map_insert_map() succeeds");
	ok(bt_value_map_insert_map(NULL, "map2") == BT_VALUE_STATUS_INVAL,
		"bt_value_map_insert_map() fails with a map value object set to NULL");

	ok(bt_value_map_size(map_obj) == 10,
		"the bt_value_map_insert*() functions increment the map value object's size");

	ok(!bt_value_map_has_key(map_obj, "hello"),
		"map value object does not have key \"hello\"");
	ok(bt_value_map_has_key(map_obj, "bool"),
		"map value object has key \"bool\"");
	ok(bt_value_map_has_key(map_obj, "int"),
		"map value object has key \"int\"");
	ok(bt_value_map_has_key(map_obj, "float"),
		"map value object has key \"float\"");
	ok(bt_value_map_has_key(map_obj, "null"),
		"map value object has key \"null\"");
	ok(bt_value_map_has_key(map_obj, "bool2"),
		"map value object has key \"bool2\"");
	ok(bt_value_map_has_key(map_obj, "int2"),
		"map value object has key \"int2\"");
	ok(bt_value_map_has_key(map_obj, "float2"),
		"map value object has key \"float2\"");
	ok(bt_value_map_has_key(map_obj, "string2"),
		"map value object has key \"string2\"");
	ok(bt_value_map_has_key(map_obj, "array2"),
		"map value object has key \"array2\"");
	ok(bt_value_map_has_key(map_obj, "map2"),
		"map value object has key \"map2\"");

	ok(bt_value_map_foreach(NULL, test_map_foreach_cb_count, &count) ==
		BT_VALUE_STATUS_INVAL,
		"bt_value_map_foreach() fails with a map value object set to NULL");
	ok(bt_value_map_foreach(map_obj, NULL, &count) ==
		BT_VALUE_STATUS_INVAL,
		"bt_value_map_foreach() fails with a user function set to NULL");
	ret = bt_value_map_foreach(map_obj, test_map_foreach_cb_count, &count);
	ok(ret == BT_VALUE_STATUS_CANCELLED && count == 3,
		"bt_value_map_foreach() breaks the loop when the user function returns false");

	memset(&checklist, 0, sizeof(checklist));
	ret = bt_value_map_foreach(map_obj, test_map_foreach_cb_check,
		&checklist);
	ok(ret == BT_VALUE_STATUS_OK,
		"bt_value_map_foreach() succeeds with test_map_foreach_cb_check()");
	ok(checklist.bool1 && checklist.int1 && checklist.float1 &&
		checklist.null1 && checklist.bool2 && checklist.int2 &&
		checklist.float2 && checklist.string2 &&
		checklist.array2 && checklist.map2,
		"bt_value_map_foreach() iterates over all the map value object's elements");

	assert(!bt_value_freeze(map_obj));
	ok(bt_value_map_insert(map_obj, "allo", bt_value_null) ==
		BT_VALUE_STATUS_FROZEN,
		"bt_value_map_insert() fails with a frozen map value object");
	ok(bt_value_map_insert_bool(map_obj, "duh", false) ==
		BT_VALUE_STATUS_FROZEN,
		"bt_value_map_insert_bool() fails with a frozen array value object");
	ok(bt_value_map_insert_integer(map_obj, "duh", 23) ==
		BT_VALUE_STATUS_FROZEN,
		"bt_value_map_insert_integer() fails with a frozen array value object");
	ok(bt_value_map_insert_float(map_obj, "duh", 2.34) ==
		BT_VALUE_STATUS_FROZEN,
		"bt_value_map_insert_float() fails with a frozen array value object");
	ok(bt_value_map_insert_string(map_obj, "duh", "yayayayaya") ==
		BT_VALUE_STATUS_FROZEN,
		"bt_value_map_insert_string() fails with a frozen array value object");
	ok(bt_value_map_insert_array(map_obj, "duh") ==
		BT_VALUE_STATUS_FROZEN,
		"bt_value_map_insert_array() fails with a frozen array value object");
	ok(bt_value_map_insert_map(map_obj, "duh") ==
		BT_VALUE_STATUS_FROZEN,
		"bt_value_map_insert_map() fails with a frozen array value object");
	ok(bt_value_map_size(map_obj) == 10,
		"appending to a frozen map value object does not change its size");

	BT_PUT(map_obj);
	pass("putting an existing map value object does not cause a crash")
}

static
void test_types(void)
{
	test_null();
	test_bool();
	test_integer();
	test_float();
	test_string();
	test_array();
	test_map();
}

static
void test_compare_null(void)
{
	ok(!bt_value_compare(bt_value_null, NULL),
		"cannot compare null value object and NULL");
	ok(!bt_value_compare(NULL, bt_value_null),
		"cannot compare NULL and null value object");
	ok(bt_value_compare(bt_value_null, bt_value_null),
		"null value objects are equivalent");
}

static
void test_compare_bool(void)
{
	struct bt_value *bool1 = bt_value_bool_create_init(false);
	struct bt_value *bool2 = bt_value_bool_create_init(true);
	struct bt_value *bool3 = bt_value_bool_create_init(false);

	assert(bool1 && bool2 && bool3);
	ok(!bt_value_compare(bt_value_null, bool1),
		"cannot compare null value object and bool value object");
	ok(!bt_value_compare(bool1, bool2),
		"integer value objects are not equivalent (false and true)");
	ok(bt_value_compare(bool1, bool3),
		"integer value objects are equivalent (false and false)");

	BT_PUT(bool1);
	BT_PUT(bool2);
	BT_PUT(bool3);
}

static
void test_compare_integer(void)
{
	struct bt_value *int1 = bt_value_integer_create_init(10);
	struct bt_value *int2 = bt_value_integer_create_init(-23);
	struct bt_value *int3 = bt_value_integer_create_init(10);

	assert(int1 && int2 && int3);
	ok(!bt_value_compare(bt_value_null, int1),
		"cannot compare null value object and integer value object");
	ok(!bt_value_compare(int1, int2),
		"integer value objects are not equivalent (10 and -23)");
	ok(bt_value_compare(int1, int3),
		"integer value objects are equivalent (10 and 10)");

	BT_PUT(int1);
	BT_PUT(int2);
	BT_PUT(int3);
}

static
void test_compare_float(void)
{
	struct bt_value *float1 = bt_value_float_create_init(17.38);
	struct bt_value *float2 = bt_value_float_create_init(-14.23);
	struct bt_value *float3 = bt_value_float_create_init(17.38);

	assert(float1 && float2 && float3);

	ok(!bt_value_compare(bt_value_null, float1),
		"cannot compare null value object and floating point number value object");
	ok(!bt_value_compare(float1, float2),
		"floating point number value objects are not equivalent (17.38 and -14.23)");
	ok(bt_value_compare(float1, float3),
		"floating point number value objects are equivalent (17.38 and 17.38)");

	BT_PUT(float1);
	BT_PUT(float2);
	BT_PUT(float3);
}

static
void test_compare_string(void)
{
	struct bt_value *string1 = bt_value_string_create_init("hello");
	struct bt_value *string2 = bt_value_string_create_init("bt_value");
	struct bt_value *string3 = bt_value_string_create_init("hello");

	assert(string1 && string2 && string3);

	ok(!bt_value_compare(bt_value_null, string1),
		"cannot compare null value object and string value object");
	ok(!bt_value_compare(string1, string2),
		"string value objects are not equivalent (\"hello\" and \"bt_value\")");
	ok(bt_value_compare(string1, string3),
		"string value objects are equivalent (\"hello\" and \"hello\")");

	BT_PUT(string1);
	BT_PUT(string2);
	BT_PUT(string3);
}

static
void test_compare_array(void)
{
	struct bt_value *array1 = bt_value_array_create();
	struct bt_value *array2 = bt_value_array_create();
	struct bt_value *array3 = bt_value_array_create();

	assert(array1 && array2 && array3);

	ok(bt_value_compare(array1, array2),
		"empty array value objects are equivalent");

	assert(!bt_value_array_append_integer(array1, 23));
	assert(!bt_value_array_append_float(array1, 14.2));
	assert(!bt_value_array_append_bool(array1, false));
	assert(!bt_value_array_append_float(array2, 14.2));
	assert(!bt_value_array_append_integer(array2, 23));
	assert(!bt_value_array_append_bool(array2, false));
	assert(!bt_value_array_append_integer(array3, 23));
	assert(!bt_value_array_append_float(array3, 14.2));
	assert(!bt_value_array_append_bool(array3, false));
	assert(bt_value_array_size(array1) == 3);
	assert(bt_value_array_size(array2) == 3);
	assert(bt_value_array_size(array3) == 3);

	ok(!bt_value_compare(bt_value_null, array1),
		"cannot compare null value object and array value object");
	ok(!bt_value_compare(array1, array2),
		"array value objects are not equivalent ([23, 14.2, false] and [14.2, 23, false])");
	ok(bt_value_compare(array1, array3),
		"array value objects are equivalent ([23, 14.2, false] and [23, 14.2, false])");

	BT_PUT(array1);
	BT_PUT(array2);
	BT_PUT(array3);
}

static
void test_compare_map(void)
{
	struct bt_value *map1 = bt_value_map_create();
	struct bt_value *map2 = bt_value_map_create();
	struct bt_value *map3 = bt_value_map_create();

	assert(map1 && map2 && map3);

	ok(bt_value_compare(map1, map2),
		"empty map value objects are equivalent");

	assert(!bt_value_map_insert_integer(map1, "one", 23));
	assert(!bt_value_map_insert_float(map1, "two", 14.2));
	assert(!bt_value_map_insert_bool(map1, "three", false));
	assert(!bt_value_map_insert_float(map2, "one", 14.2));
	assert(!bt_value_map_insert_integer(map2, "two", 23));
	assert(!bt_value_map_insert_bool(map2, "three", false));
	assert(!bt_value_map_insert_bool(map3, "three", false));
	assert(!bt_value_map_insert_integer(map3, "one", 23));
	assert(!bt_value_map_insert_float(map3, "two", 14.2));
	assert(bt_value_map_size(map1) == 3);
	assert(bt_value_map_size(map2) == 3);
	assert(bt_value_map_size(map3) == 3);

	ok(!bt_value_compare(bt_value_null, map1),
		"cannot compare null value object and map value object");
	ok(!bt_value_compare(map1, map2),
		"map value objects are not equivalent");
	ok(bt_value_compare(map1, map3),
		"map value objects are equivalent");

	BT_PUT(map1);
	BT_PUT(map2);
	BT_PUT(map3);
}

static
void test_compare(void)
{
	ok(!bt_value_compare(NULL, NULL), "cannot compare NULL and NULL");
	test_compare_null();
	test_compare_bool();
	test_compare_integer();
	test_compare_float();
	test_compare_string();
	test_compare_array();
	test_compare_map();
}

static
void test_copy(void)
{
	/*
	 * Here's the deal here. If we make sure that each value object
	 * of our deep copy has a different address than its source,
	 * and that bt_value_compare() returns true for the top-level
	 * value object, taking into account that we test the correctness of
	 * bt_value_compare() elsewhere, then the deep copy is a
	 * success.
	 */
	struct bt_value *null_copy_obj;
	struct bt_value *bool_obj, *bool_copy_obj;
	struct bt_value *integer_obj, *integer_copy_obj;
	struct bt_value *float_obj, *float_copy_obj;
	struct bt_value *string_obj, *string_copy_obj;
	struct bt_value *array_obj, *array_copy_obj;
	struct bt_value *map_obj, *map_copy_obj;

	bool_obj = bt_value_bool_create_init(true);
	integer_obj = bt_value_integer_create_init(23);
	float_obj = bt_value_float_create_init(-3.1416);
	string_obj = bt_value_string_create_init("test");
	array_obj = bt_value_array_create();
	map_obj = bt_value_map_create();

	assert(bool_obj && integer_obj && float_obj && string_obj &&
		array_obj && map_obj);

	assert(!bt_value_array_append(array_obj, bool_obj));
	assert(!bt_value_array_append(array_obj, integer_obj));
	assert(!bt_value_array_append(array_obj, float_obj));
	assert(!bt_value_array_append(array_obj, bt_value_null));
	assert(!bt_value_map_insert(map_obj, "array", array_obj));
	assert(!bt_value_map_insert(map_obj, "string", string_obj));

	map_copy_obj = bt_value_copy(NULL);
	ok(!map_copy_obj,
		"bt_value_copy() fails with a source value object set to NULL");

	map_copy_obj = bt_value_copy(map_obj);
	ok(map_copy_obj,
		"bt_value_copy() succeeds");

	ok(map_obj != map_copy_obj,
		"bt_value_copy() returns a different pointer (map)");
	string_copy_obj = bt_value_map_get(map_copy_obj, "string");
	ok(string_copy_obj != string_obj,
		"bt_value_copy() returns a different pointer (string)");
	array_copy_obj = bt_value_map_get(map_copy_obj, "array");
	ok(array_copy_obj != array_obj,
		"bt_value_copy() returns a different pointer (array)");
	bool_copy_obj = bt_value_array_get(array_copy_obj, 0);
	ok(bool_copy_obj != bool_obj,
		"bt_value_copy() returns a different pointer (bool)");
	integer_copy_obj = bt_value_array_get(array_copy_obj, 1);
	ok(integer_copy_obj != integer_obj,
		"bt_value_copy() returns a different pointer (integer)");
	float_copy_obj = bt_value_array_get(array_copy_obj, 2);
	ok(float_copy_obj != float_obj,
		"bt_value_copy() returns a different pointer (float)");
	null_copy_obj = bt_value_array_get(array_copy_obj, 3);
	ok(null_copy_obj == bt_value_null,
		"bt_value_copy() returns the same pointer (null)");

	ok(bt_value_compare(map_obj, map_copy_obj),
		"source and destination value objects have the same content");

	BT_PUT(bool_copy_obj);
	BT_PUT(integer_copy_obj);
	BT_PUT(float_copy_obj);
	BT_PUT(string_copy_obj);
	BT_PUT(array_copy_obj);
	BT_PUT(map_copy_obj);
	BT_PUT(bool_obj);
	BT_PUT(integer_obj);
	BT_PUT(float_obj);
	BT_PUT(string_obj);
	BT_PUT(array_obj);
	BT_PUT(map_obj);
}

static
void test_macros(void)
{
	struct bt_value *obj = bt_value_bool_create();
	struct bt_value *src;
	struct bt_value *dst;

	assert(obj);
	BT_PUT(obj);
	ok(!obj, "BT_PUT() resets the variable to NULL");

	obj = bt_value_bool_create();
	assert(obj);
	src = obj;
	BT_MOVE(dst, src);
	ok(!src, "BT_MOVE() resets the source variable to NULL");
	ok(dst == obj, "BT_MOVE() moves the ownership");

	BT_PUT(dst);
}

static
void test_freeze(void)
{
	struct bt_value *obj;

	ok(bt_value_freeze(NULL) == BT_VALUE_STATUS_INVAL,
		"bt_value_freeze() fails with an value object set to NULL");
	ok(!bt_value_freeze(bt_value_null),
		"bt_value_freeze() succeeds with a null value object");

	ok(!bt_value_is_frozen(NULL), "NULL is not frozen");
	ok(bt_value_is_frozen(bt_value_null),
		"the null singleton is frozen");
	obj = bt_value_integer_create();
	assert(obj);
	ok(!bt_value_is_frozen(obj),
		"bt_value_is_frozen() returns false with a fresh value object");
	assert(!bt_value_freeze(obj));
	ok(!bt_value_freeze(obj),
		"bt_value_freeze() passes with a frozen value object");
	ok(bt_value_is_frozen(obj),
		"bt_value_is_frozen() returns true with a frozen value object");

	BT_PUT(obj);
}

int main(void)
{
	plan_no_plan();

	test_macros();
	test_freeze();
	test_types();
	test_compare();
	test_copy();

	return 0;
}
