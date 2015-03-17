/*
 * test_bt_objects.c
 *
 * Babeltrace basic object system tests
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

#define _GNU_SOURCE
#include <babeltrace/objects.h>
#include <assert.h>
#include <string.h>
#include "tap/tap.h"

static
void test_null(void)
{
	ok(bt_object_null, "bt_object_null is not NULL");
	ok(bt_object_is_null(bt_object_null),
		"bt_object_null is a null object");
	bt_object_get(bt_object_null);
	pass("getting bt_object_null does not cause a crash");
	bt_object_put(bt_object_null);
	pass("putting bt_object_null does not cause a crash");

	bt_object_get(NULL);
	pass("getting NULL does not cause a crash");
	bt_object_put(NULL);
	pass("putting NULL does not cause a crash");

	ok(bt_object_get_type(NULL) == BT_OBJECT_TYPE_UNKNOWN,
		"bt_object_get_type(NULL) returns BT_OBJECT_TYPE_UNKNOWN");
}

static
void test_bool(void)
{
	int ret;
	bool value;
	struct bt_object *obj;

	obj = bt_object_bool_create();
	ok(obj && bt_object_is_bool(obj),
		"bt_object_bool_create() returns a boolean object");

	value = true;
	ret = bt_object_bool_get(obj, &value);
	ok(!ret && !value, "default boolean object value is false");

	ret = bt_object_bool_set(NULL, true);
	ok(ret, "bt_object_bool_set() fails with an object set to NULL");
	ret = bt_object_bool_get(NULL, &value);
	ok(ret, "bt_object_bool_get() fails with an object set to NULL");

	ret = bt_object_bool_set(obj, true);
	ok(!ret, "bt_object_bool_set() succeeds");
	ret = bt_object_bool_get(obj, &value);
	ok(!ret && value, "bt_object_bool_set() works");

	BT_OBJECT_PUT(obj);
	pass("putting an existing boolean object does not cause a crash")

	obj = bt_object_bool_create_init(true);
	ok(obj && bt_object_is_bool(obj),
		"bt_object_bool_create_init() returns a boolean object");
	ret = bt_object_bool_get(obj, &value);
	ok(!ret && value,
		"bt_object_bool_create_init() sets the appropriate initial value");

	BT_OBJECT_PUT(obj);
}

static
void test_integer(void)
{
	int ret;
	int64_t value;
	struct bt_object *obj;

	obj = bt_object_integer_create();
	ok(obj && bt_object_is_integer(obj),
		"bt_object_integer_create() returns an integer object");

	ret = bt_object_integer_set(NULL, -12345);
	ok(ret, "bt_object_integer_set() fails with an object set to NULL");
	ret = bt_object_integer_get(NULL, &value);
	ok(ret, "bt_object_integer_get() fails with an object set to NULL");

	value = 1961;
	ret = bt_object_integer_get(obj, &value);
	ok(!ret && value == 0, "default integer object value is 0");

	ret = bt_object_integer_set(obj, -12345);
	ok(!ret, "bt_object_integer_set() succeeds");
	ret = bt_object_integer_get(obj, &value);
	ok(!ret && value == -12345, "bt_object_integer_set() works");

	BT_OBJECT_PUT(obj);
	pass("putting an existing integer object does not cause a crash")

	obj = bt_object_integer_create_init(321456987);
	ok(obj && bt_object_is_integer(obj),
		"bt_object_integer_create_init() returns an integer object");
	ret = bt_object_integer_get(obj, &value);
	ok(!ret && value == 321456987,
		"bt_object_integer_create_init() sets the appropriate initial value");

	BT_OBJECT_PUT(obj);
}

static
void test_float(void)
{
	int ret;
	double value;
	struct bt_object *obj;

	obj = bt_object_float_create();
	ok(obj && bt_object_is_float(obj),
		"bt_object_float_create() returns a floating point number object");

	ret = bt_object_float_set(NULL, 1.2345);
	ok(ret, "bt_object_float_set() fails with an object set to NULL");
	ret = bt_object_float_get(NULL, &value);
	ok(ret, "bt_object_float_get() fails with an object set to NULL");

	value = 17.34;
	ret = bt_object_float_get(obj, &value);
	ok(!ret && value == 0., "default floating point number object value is 0");

	ret = bt_object_float_set(obj, -3.1416);
	ok(!ret, "bt_object_float_set() succeeds");
	ret = bt_object_float_get(obj, &value);
	ok(!ret && value == -3.1416, "bt_object_float_set() works");

	BT_OBJECT_PUT(obj);
	pass("putting an existing floating point number object does not cause a crash")

	obj = bt_object_float_create_init(33.1649758);
	ok(obj && bt_object_is_float(obj),
		"bt_object_float_create_init() returns a floating point number object");
	ret = bt_object_float_get(obj, &value);
	ok(!ret && value == 33.1649758,
		"bt_object_float_create_init() sets the appropriate initial value");

	BT_OBJECT_PUT(obj);
}

static
void test_string(void)
{
	int ret;
	const char *value;
	struct bt_object *obj;

	obj = bt_object_string_create();
	ok(obj && bt_object_is_string(obj),
		"bt_object_string_create() returns a string object");

	ret = bt_object_string_set(NULL, "hoho");
	ok(ret, "bt_object_string_set() fails with an object set to NULL");
	value = bt_object_string_get(NULL);
	ok(!value, "bt_object_string_get() fails with an object set to NULL");

	value = bt_object_string_get(obj);
	ok(value && !strcmp(value, ""),
		"default string object value is \"\"");

	ret = bt_object_string_set(obj, "hello worldz");
	ok(!ret, "bt_object_string_set() succeeds");
	value = bt_object_string_get(obj);
	ok(value && !strcmp(value, "hello worldz"),
		"bt_object_string_set() works");
	ret = bt_object_string_set(obj, NULL);
	ok(ret, "bt_object_string_set() does not accept a NULL value");

	BT_OBJECT_PUT(obj);
	pass("putting an existing string object does not cause a crash")

	obj = bt_object_string_create_init(NULL);
	ok(!obj, "bt_object_string_create_init() fails with an initial value set to NULL");
	obj = bt_object_string_create_init("initial value");
	ok(obj && bt_object_is_string(obj),
		"bt_object_string_create_init() returns a string object");
	value = bt_object_string_get(obj);
	ok(value && !strcmp(value, "initial value"),
		"bt_object_string_create_init() sets the appropriate initial value");

	BT_OBJECT_PUT(obj);
}

static
void test_array(void)
{
	int ret;
	bool bool_value;
	int64_t int_value;
	double float_value;
	struct bt_object *obj;
	const char *string_value;
	struct bt_object *array_obj;

	array_obj = bt_object_array_create();
	ok(array_obj && bt_object_is_array(array_obj),
		"bt_object_array_create() returns an array object");
	ok(bt_object_array_is_empty(array_obj),
		"initial array object size is 0");
	ok(bt_object_array_size(NULL) < 0,
		"bt_object_array_size() fails with an array object set to NULL");

	ok(bt_object_array_append(NULL, bt_object_null),
		"bt_object_array_append() fails with an array object set to NULL");

	obj = bt_object_integer_create_init(345);
	ret = bt_object_array_append(array_obj, obj);
	BT_OBJECT_PUT(obj);
	obj = bt_object_float_create_init(-17.45);
	ret |= bt_object_array_append(array_obj, obj);
	BT_OBJECT_PUT(obj);
	obj = bt_object_bool_create_init(true);
	ret |= bt_object_array_append(array_obj, obj);
	BT_OBJECT_PUT(obj);
	ret |= bt_object_array_append(array_obj, bt_object_null);
	ok(!ret, "bt_object_array_append() succeeds");
	ret = bt_object_array_append(NULL, bt_object_null);
	ok(ret, "bt_object_array_append() fails with an array object set to NULL");
	ret = bt_object_array_append(array_obj, NULL);
	ok(ret, "bt_object_array_append() fails with an element object set to NULL");
	ok(bt_object_array_size(array_obj) == 4,
		"appending an element to an array object increment its size");

	obj = bt_object_array_get(array_obj, 4);
	ok(!obj, "getting an array object's element at an index equal to its size fails");
	obj = bt_object_array_get(array_obj, 5);
	ok(!obj, "getting an array object's element at a larger index fails");

	obj = bt_object_array_get(NULL, 2);
	ok(!obj, "bt_object_array_get() fails with an array object set to NULL");

	obj = bt_object_array_get(array_obj, 0);
	ok(obj && bt_object_is_integer(obj),
		"bt_object_array_get() returns an object with the appropriate type (integer)");
	ret = bt_object_integer_get(obj, &int_value);
	ok(!ret && int_value == 345,
		"bt_object_array_get() returns an object with the appropriate value (integer)");
	BT_OBJECT_PUT(obj);
	obj = bt_object_array_get(array_obj, 1);
	ok(obj && bt_object_is_float(obj),
		"bt_object_array_get() returns an object with the appropriate type (floating point number)");
	ret = bt_object_float_get(obj, &float_value);
	ok(!ret && float_value == -17.45,
		"bt_object_array_get() returns an object with the appropriate value (floating point number)");
	BT_OBJECT_PUT(obj);
	obj = bt_object_array_get(array_obj, 2);
	ok(obj && bt_object_is_bool(obj),
		"bt_object_array_get() returns an object with the appropriate type (boolean)");
	ret = bt_object_bool_get(obj, &bool_value);
	ok(!ret && bool_value,
		"bt_object_array_get() returns an object with the appropriate value (boolean)");
	BT_OBJECT_PUT(obj);
	obj = bt_object_array_get(array_obj, 3);
	ok(obj == bt_object_null,
		"bt_object_array_get() returns an object with the appropriate type (null)");

	ok(bt_object_array_set(NULL, 0, bt_object_null),
		"bt_object_array_set() fails with an array object set to NULL");
	ok(bt_object_array_set(array_obj, 0, NULL),
		"bt_object_array_set() fails with an element object set to NULL");
	ok(bt_object_array_set(array_obj, 4, bt_object_null),
		"bt_object_array_set() fails with an invalid index");
	obj = bt_object_integer_create_init(1001);
	assert(obj);
	ok(!bt_object_array_set(array_obj, 2, obj),
		"bt_object_array_set() succeeds");
	BT_OBJECT_PUT(obj);
	obj = bt_object_array_get(array_obj, 2);
	ok(obj && bt_object_is_integer(obj),
		"bt_object_array_set() inserts an object with the appropriate type");
	ret = bt_object_integer_get(obj, &int_value);
	assert(!ret);
	ok(int_value == 1001,
		"bt_object_array_set() inserts an object with the appropriate value");
	BT_OBJECT_PUT(obj);

	ret = bt_object_array_append_bool(array_obj, false);
	ok(!ret, "bt_object_array_append_bool() succeeds");
	ret = bt_object_array_append_bool(NULL, true);
	ok(ret, "bt_object_array_append_bool() fails with an array object set to NULL");
	ret = bt_object_array_append_integer(array_obj, 98765);
	ok(!ret, "bt_object_array_append_integer() succeeds");
	ret = bt_object_array_append_integer(NULL, 18765);
	ok(ret, "bt_object_array_append_integer() fails with an array object set to NULL");
	ret = bt_object_array_append_float(array_obj, 2.49578);
	ok(!ret, "bt_object_array_append_float() succeeds");
	ret = bt_object_array_append_float(NULL, 1.49578);
	ok(ret, "bt_object_array_append_float() fails with an array object set to NULL");
	ret = bt_object_array_append_string(array_obj, "bt_object");
	ok(!ret, "bt_object_array_append_string() succeeds");
	ret = bt_object_array_append_string(NULL, "bt_obj");
	ok(ret, "bt_object_array_append_string() fails with an array object set to NULL");
	ret = bt_object_array_append_array(array_obj);
	ok(!ret, "bt_object_array_append_array() succeeds");
	ret = bt_object_array_append_array(NULL);
	ok(ret, "bt_object_array_append_array() fails with an array object set to NULL");
	ret = bt_object_array_append_map(array_obj);
	ok(!ret, "bt_object_array_append_map() succeeds");
	ret = bt_object_array_append_map(NULL);
	ok(ret, "bt_object_array_append_map() fails with an array object set to NULL");

	ok(bt_object_array_size(array_obj) == 10,
		"the bt_object_array_append_*() functions increment the array object's size");
	ok(!bt_object_array_is_empty(array_obj),
		"map object is not empty");

	obj = bt_object_array_get(array_obj, 4);
	ok(obj && bt_object_is_bool(obj),
		"bt_object_array_append_bool() appends a boolean object");
	ret = bt_object_bool_get(obj, &bool_value);
	ok(!ret && !bool_value,
		"bt_object_array_append_bool() appends the appropriate value");
	BT_OBJECT_PUT(obj);
	obj = bt_object_array_get(array_obj, 5);
	ok(obj && bt_object_is_integer(obj),
		"bt_object_array_append_integer() appends an integer object");
	ret = bt_object_integer_get(obj, &int_value);
	ok(!ret && int_value == 98765,
		"bt_object_array_append_integer() appends the appropriate value");
	BT_OBJECT_PUT(obj);
	obj = bt_object_array_get(array_obj, 6);
	ok(obj && bt_object_is_float(obj),
		"bt_object_array_append_float() appends a floating point number object");
	ret = bt_object_float_get(obj, &float_value);
	ok(!ret && float_value == 2.49578,
		"bt_object_array_append_float() appends the appropriate value");
	BT_OBJECT_PUT(obj);
	obj = bt_object_array_get(array_obj, 7);
	ok(obj && bt_object_is_string(obj),
		"bt_object_array_append_string() appends a string object");
	string_value = bt_object_string_get(obj);
	ok(string_value && !strcmp(string_value, "bt_object"),
		"bt_object_array_append_string() appends the appropriate value");
	BT_OBJECT_PUT(obj);
	obj = bt_object_array_get(array_obj, 8);
	ok(obj && bt_object_is_array(obj),
		"bt_object_array_append_array() appends an array object");
	ok(bt_object_array_is_empty(obj),
		"bt_object_array_append_array() an empty array object");
	BT_OBJECT_PUT(obj);
	obj = bt_object_array_get(array_obj, 9);
	ok(obj && bt_object_is_map(obj),
		"bt_object_array_append_map() appends a map object");
	ok(bt_object_map_is_empty(obj),
		"bt_object_array_append_map() an empty map object");
	BT_OBJECT_PUT(obj);

	BT_OBJECT_PUT(array_obj);
	pass("putting an existing array object does not cause a crash")
}

static
bool test_map_foreach_cb_count(const char *key, struct bt_object *object,
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
bool test_map_foreach_cb_check(const char *key, struct bt_object *object,
	void *data)
{
	int ret;
	struct map_foreach_checklist *checklist = data;

	if (!strcmp(key, "bool")) {
		if (checklist->bool1) {
			fail("test_map_foreach_cb_check(): duplicate key \"bool\"");
		} else {
			bool val = false;

			ret = bt_object_bool_get(object, &val);
			ok(!ret, "test_map_foreach_cb_check(): success getting \"bool\" value");

			if (val) {
				pass("test_map_foreach_cb_check(): \"bool\" object has the right value");
				checklist->bool1 = true;
			}
		}
	} else if (!strcmp(key, "int")) {
		if (checklist->int1) {
			fail("test_map_foreach_cb_check(): duplicate key \"int\"");
		} else {
			int64_t val = 0;

			ret = bt_object_integer_get(object, &val);
			ok(!ret, "test_map_foreach_cb_check(): success getting \"int\" value");

			if (val == 19457) {
				pass("test_map_foreach_cb_check(): \"int\" object has the right value");
				checklist->int1 = true;
			}
		}
	} else if (!strcmp(key, "float")) {
		if (checklist->float1) {
			fail("test_map_foreach_cb_check(): duplicate key \"float\"");
		} else {
			double val = 0;

			ret = bt_object_float_get(object, &val);
			ok(!ret, "test_map_foreach_cb_check(): success getting \"float\" value");

			if (val == 5.444) {
				pass("test_map_foreach_cb_check(): \"float\" object has the right value");
				checklist->float1 = true;
			}
		}
	} else if (!strcmp(key, "null")) {
		if (checklist->null1) {
			fail("test_map_foreach_cb_check(): duplicate key \"bool\"");
		} else {
			ok(bt_object_is_null(object), "test_map_foreach_cb_check(): success getting \"null\" object");
			checklist->null1 = true;
		}
	} else if (!strcmp(key, "bool2")) {
		if (checklist->bool2) {
			fail("test_map_foreach_cb_check(): duplicate key \"bool2\"");
		} else {
			bool val = false;

			ret = bt_object_bool_get(object, &val);
			ok(!ret, "test_map_foreach_cb_check(): success getting \"bool2\" value");

			if (val) {
				pass("test_map_foreach_cb_check(): \"bool2\" object has the right value");
				checklist->bool2 = true;
			}
		}
	} else if (!strcmp(key, "int2")) {
		if (checklist->int2) {
			fail("test_map_foreach_cb_check(): duplicate key \"int2\"");
		} else {
			int64_t val = 0;

			ret = bt_object_integer_get(object, &val);
			ok(!ret, "test_map_foreach_cb_check(): success getting \"int2\" value");

			if (val == 98765) {
				pass("test_map_foreach_cb_check(): \"int2\" object has the right value");
				checklist->int2 = true;
			}
		}
	} else if (!strcmp(key, "float2")) {
		if (checklist->float2) {
			fail("test_map_foreach_cb_check(): duplicate key \"float2\"");
		} else {
			double val = 0;

			ret = bt_object_float_get(object, &val);
			ok(!ret, "test_map_foreach_cb_check(): success getting \"float2\" value");

			if (val == -49.0001) {
				pass("test_map_foreach_cb_check(): \"float2\" object has the right value");
				checklist->float2 = true;
			}
		}
	} else if (!strcmp(key, "string2")) {
		if (checklist->string2) {
			fail("test_map_foreach_cb_check(): duplicate key \"string2\"");
		} else {
			const char *val = bt_object_string_get(object);

			ok(val, "test_map_foreach_cb_check(): success getting \"string2\" value");

			if (val && !strcmp(val, "bt_object")) {
				pass("test_map_foreach_cb_check(): \"string2\" object has the right value");
				checklist->string2 = true;
			}
		}
	} else if (!strcmp(key, "array2")) {
		if (checklist->array2) {
			fail("test_map_foreach_cb_check(): duplicate key \"array2\"");
		} else {
			ok(bt_object_is_array(object), "test_map_foreach_cb_check(): success getting \"array2\" object");
			ok(bt_object_array_is_empty(object),
				"test_map_foreach_cb_check(): \"array2\" object is empty");
			checklist->array2 = true;
		}
	} else if (!strcmp(key, "map2")) {
		if (checklist->map2) {
			fail("test_map_foreach_cb_check(): duplicate key \"map2\"");
		} else {
			ok(bt_object_is_map(object), "test_map_foreach_cb_check(): success getting \"map2\" object");
			ok(bt_object_map_is_empty(object),
				"test_map_foreach_cb_check(): \"map2\" object is empty");
			checklist->map2 = true;
		}
	} else {
		diag("test_map_foreach_cb_check(): unknown map key \"%s\"",
			key);
		fail("test_map_foreach_cb_check(): unknown map key");
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
	struct bt_object *obj;
	struct bt_object *map_obj;
	struct map_foreach_checklist checklist;

	map_obj = bt_object_map_create();
	ok(map_obj && bt_object_is_map(map_obj),
		"bt_object_map_create() returns a map object");
	ok(bt_object_map_size(map_obj) == 0,
		"initial map object size is 0");
	ok(bt_object_map_size(NULL) < 0,
		"bt_object_map_size() fails with a map object set to NULL");

	ok(bt_object_map_insert(NULL, "hello", bt_object_null),
		"bt_object_array_insert() fails with a map object set to NULL");

	ok(bt_object_map_insert(map_obj, NULL, bt_object_null),
		"bt_object_array_insert() fails with a key set to NULL");
	ok(bt_object_map_insert(map_obj, "yeah", NULL),
		"bt_object_array_insert() fails with an element object set to NULL");

	obj = bt_object_integer_create_init(19457);
	ret = bt_object_map_insert(map_obj, "int", obj);
	BT_OBJECT_PUT(obj);
	obj = bt_object_float_create_init(5.444);
	ret |= bt_object_map_insert(map_obj, "float", obj);
	BT_OBJECT_PUT(obj);
	obj = bt_object_bool_create();
	ret |= bt_object_map_insert(map_obj, "bool", obj);
	BT_OBJECT_PUT(obj);
	ret |= bt_object_map_insert(map_obj, "null", bt_object_null);
	ok(!ret, "bt_object_map_insert() succeeds");
	ok(bt_object_map_size(map_obj) == 4,
		"inserting an element into a map object increment its size");

	obj = bt_object_bool_create_init(true);
	ret = bt_object_map_insert(map_obj, "bool", obj);
	BT_OBJECT_PUT(obj);
	ok(!ret, "bt_object_map_insert() accepts an existing key");

	obj = bt_object_map_get(map_obj, NULL);
	ok(!obj, "bt_object_map_get() fails with a key set to NULL");
	obj = bt_object_map_get(NULL, "bool");
	ok(!obj, "bt_object_map_get() fails with a map object set to NULL");

	obj = bt_object_map_get(map_obj, "life");
	ok(!obj, "bt_object_map_get() fails with an non existing key");
	obj = bt_object_map_get(map_obj, "float");
	ok(obj && bt_object_is_float(obj),
		"bt_object_map_get() returns an object with the appropriate type (float)");
	ret = bt_object_float_get(obj, &float_value);
	ok(!ret && float_value == 5.444,
		"bt_object_map_get() returns an object with the appropriate value (float)");
	BT_OBJECT_PUT(obj);
	obj = bt_object_map_get(map_obj, "int");
	ok(obj && bt_object_is_integer(obj),
		"bt_object_map_get() returns an object with the appropriate type (integer)");
	ret = bt_object_integer_get(obj, &int_value);
	ok(!ret && int_value == 19457,
		"bt_object_map_get() returns an object with the appropriate value (integer)");
	BT_OBJECT_PUT(obj);
	obj = bt_object_map_get(map_obj, "null");
	ok(obj && bt_object_is_null(obj),
		"bt_object_map_get() returns an object with the appropriate type (null)");
	obj = bt_object_map_get(map_obj, "bool");
	ok(obj && bt_object_is_bool(obj),
		"bt_object_map_get() returns an object with the appropriate type (boolean)");
	ret = bt_object_bool_get(obj, &bool_value);
	ok(!ret && bool_value,
		"bt_object_map_get() returns an object with the appropriate value (boolean)");
	BT_OBJECT_PUT(obj);

	ret = bt_object_map_insert_bool(map_obj, "bool2", true);
	ok(!ret, "bt_object_map_insert_bool() succeeds");
	ret = bt_object_map_insert_bool(NULL, "bool2", false);
	ok(ret, "bt_object_map_insert_bool() fails with a map object set to NULL");
	ret = bt_object_map_insert_integer(map_obj, "int2", 98765);
	ok(!ret, "bt_object_map_insert_integer() succeeds");
	ret = bt_object_map_insert_integer(NULL, "int2", 1001);
	ok(ret, "bt_object_map_insert_integer() fails with a map object set to NULL");
	ret = bt_object_map_insert_float(map_obj, "float2", -49.0001);
	ok(!ret, "bt_object_map_insert_float() succeeds");
	ret = bt_object_map_insert_float(NULL, "float2", 495);
	ok(ret, "bt_object_map_insert_float() fails with a map object set to NULL");
	ret = bt_object_map_insert_string(map_obj, "string2", "bt_object");
	ok(!ret, "bt_object_map_insert_string() succeeds");
	ret = bt_object_map_insert_string(NULL, "string2", "bt_obj");
	ok(ret, "bt_object_map_insert_string() fails with a map object set to NULL");
	ret = bt_object_map_insert_array(map_obj, "array2");
	ok(!ret, "bt_object_map_insert_array() succeeds");
	ret = bt_object_map_insert_array(NULL, "array2");
	ok(ret, "bt_object_map_insert_array() fails with a map object set to NULL");
	ret = bt_object_map_insert_map(map_obj, "map2");
	ok(!ret, "bt_object_map_insert_map() succeeds");
	ret = bt_object_map_insert_map(NULL, "map2");
	ok(ret, "bt_object_map_insert_map() fails with a map object set to NULL");

	ok(bt_object_map_size(map_obj) == 10,
		"the bt_object_map_insert*() functions increment the map object's size");

	ok(!bt_object_map_has_key(map_obj, "hello"),
		"map object does not have key \"hello\"");
	ok(bt_object_map_has_key(map_obj, "bool"),
		"map object has key \"bool\"");
	ok(bt_object_map_has_key(map_obj, "int"),
		"map object has key \"int\"");
	ok(bt_object_map_has_key(map_obj, "float"),
		"map object has key \"float\"");
	ok(bt_object_map_has_key(map_obj, "null"),
		"map object has key \"null\"");
	ok(bt_object_map_has_key(map_obj, "bool2"),
		"map object has key \"bool2\"");
	ok(bt_object_map_has_key(map_obj, "int2"),
		"map object has key \"int2\"");
	ok(bt_object_map_has_key(map_obj, "float2"),
		"map object has key \"float2\"");
	ok(bt_object_map_has_key(map_obj, "string2"),
		"map object has key \"string2\"");
	ok(bt_object_map_has_key(map_obj, "array2"),
		"map object has key \"array2\"");
	ok(bt_object_map_has_key(map_obj, "map2"),
		"map object has key \"map2\"");

	ret = bt_object_map_foreach(NULL, test_map_foreach_cb_count, &count);
	ok(ret == BT_OBJECT_STATUS_ERROR,
		"bt_object_map_foreach() fails with a map object set to NULL");
	ret = bt_object_map_foreach(map_obj, NULL, &count);
	ok(ret == BT_OBJECT_STATUS_ERROR,
		"bt_object_map_foreach() fails with a user function set to NULL");
	ret = bt_object_map_foreach(map_obj, test_map_foreach_cb_count, &count);
	ok(ret == BT_OBJECT_STATUS_CANCELLED && count == 3,
		"bt_object_map_foreach() breaks the loop when the user function returns false");

	memset(&checklist, 0, sizeof(checklist));
	ret = bt_object_map_foreach(map_obj, test_map_foreach_cb_check,
		&checklist);
	ok(ret == BT_OBJECT_STATUS_OK,
		"bt_object_map_foreach() succeeds with test_map_foreach_cb_check()");
	ok(checklist.bool1 && checklist.int1 && checklist.float1 &&
		checklist.null1 && checklist.bool2 && checklist.int2 &&
		checklist.float2 && checklist.string2 &&
		checklist.array2 && checklist.map2,
		"bt_object_map_foreach() iterates over all the map object's elements");

	BT_OBJECT_PUT(map_obj);
	pass("putting an existing map object does not cause a crash")
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
	ok(!bt_object_compare(bt_object_null, NULL),
		"cannot compare null object and NULL");
	ok(!bt_object_compare(NULL, bt_object_null),
		"cannot compare NULL and null object");
	ok(bt_object_compare(bt_object_null, bt_object_null),
		"null objects are equivalent");
}

static
void test_compare_bool(void)
{
	struct bt_object *bool1 = bt_object_bool_create_init(false);
	struct bt_object *bool2 = bt_object_bool_create_init(true);
	struct bt_object *bool3 = bt_object_bool_create_init(false);

	assert(bool1 && bool2 && bool3);
	ok(!bt_object_compare(bt_object_null, bool1),
		"cannot compare null object and bool object");
	ok(!bt_object_compare(bool1, bool2),
		"integer objects are not equivalent (false and true)");
	ok(bt_object_compare(bool1, bool3),
		"integer objects are equivalent (false and false)");

	BT_OBJECT_PUT(bool1);
	BT_OBJECT_PUT(bool2);
	BT_OBJECT_PUT(bool3);
}

static
void test_compare_integer(void)
{
	struct bt_object *int1 = bt_object_integer_create_init(10);
	struct bt_object *int2 = bt_object_integer_create_init(-23);
	struct bt_object *int3 = bt_object_integer_create_init(10);

	assert(int1 && int2 && int3);
	ok(!bt_object_compare(bt_object_null, int1),
		"cannot compare null object and integer object");
	ok(!bt_object_compare(int1, int2),
		"integer objects are not equivalent (10 and -23)");
	ok(bt_object_compare(int1, int3),
		"integer objects are equivalent (10 and 10)");

	BT_OBJECT_PUT(int1);
	BT_OBJECT_PUT(int2);
	BT_OBJECT_PUT(int3);
}

static
void test_compare_float(void)
{
	struct bt_object *float1 = bt_object_float_create_init(17.38);
	struct bt_object *float2 = bt_object_float_create_init(-14.23);
	struct bt_object *float3 = bt_object_float_create_init(17.38);

	assert(float1 && float2 && float3);

	ok(!bt_object_compare(bt_object_null, float1),
		"cannot compare null object and floating point number object");
	ok(!bt_object_compare(float1, float2),
		"floating point number objects are not equivalent (17.38 and -14.23)");
	ok(bt_object_compare(float1, float3),
		"floating point number objects are equivalent (17.38 and 17.38)");

	BT_OBJECT_PUT(float1);
	BT_OBJECT_PUT(float2);
	BT_OBJECT_PUT(float3);
}

static
void test_compare_string(void)
{
	struct bt_object *string1 = bt_object_string_create_init("hello");
	struct bt_object *string2 = bt_object_string_create_init("bt_object");
	struct bt_object *string3 = bt_object_string_create_init("hello");

	assert(string1 && string2 && string3);

	ok(!bt_object_compare(bt_object_null, string1),
		"cannot compare null object and string object");
	ok(!bt_object_compare(string1, string2),
		"string objects are not equivalent (\"hello\" and \"bt_object\")");
	ok(bt_object_compare(string1, string3),
		"string objects are equivalent (\"hello\" and \"hello\")");

	BT_OBJECT_PUT(string1);
	BT_OBJECT_PUT(string2);
	BT_OBJECT_PUT(string3);
}

static
void test_compare_array(void)
{
	struct bt_object *array1 = bt_object_array_create();
	struct bt_object *array2 = bt_object_array_create();
	struct bt_object *array3 = bt_object_array_create();

	assert(array1 && array2 && array3);

	ok(bt_object_compare(array1, array2),
		"empty array objects are equivalent");

	assert(!bt_object_array_append_integer(array1, 23));
	assert(!bt_object_array_append_float(array1, 14.2));
	assert(!bt_object_array_append_bool(array1, false));
	assert(!bt_object_array_append_float(array2, 14.2));
	assert(!bt_object_array_append_integer(array2, 23));
	assert(!bt_object_array_append_bool(array2, false));
	assert(!bt_object_array_append_integer(array3, 23));
	assert(!bt_object_array_append_float(array3, 14.2));
	assert(!bt_object_array_append_bool(array3, false));
	assert(bt_object_array_size(array1) == 3);
	assert(bt_object_array_size(array2) == 3);
	assert(bt_object_array_size(array3) == 3);

	ok(!bt_object_compare(bt_object_null, array1),
		"cannot compare null object and array object");
	ok(!bt_object_compare(array1, array2),
		"array objects are not equivalent ([23, 14.2, false] and [14.2, 23, false])");
	ok(bt_object_compare(array1, array3),
		"array objects are equivalent ([23, 14.2, false] and [23, 14.2, false])");

	BT_OBJECT_PUT(array1);
	BT_OBJECT_PUT(array2);
	BT_OBJECT_PUT(array3);
}

static
void test_compare_map(void)
{
	struct bt_object *map1 = bt_object_map_create();
	struct bt_object *map2 = bt_object_map_create();
	struct bt_object *map3 = bt_object_map_create();

	assert(map1 && map2 && map3);

	ok(bt_object_compare(map1, map2),
		"empty map objects are equivalent");

	assert(!bt_object_map_insert_integer(map1, "one", 23));
	assert(!bt_object_map_insert_float(map1, "two", 14.2));
	assert(!bt_object_map_insert_bool(map1, "three", false));
	assert(!bt_object_map_insert_float(map2, "one", 14.2));
	assert(!bt_object_map_insert_integer(map2, "two", 23));
	assert(!bt_object_map_insert_bool(map2, "three", false));
	assert(!bt_object_map_insert_bool(map3, "three", false));
	assert(!bt_object_map_insert_integer(map3, "one", 23));
	assert(!bt_object_map_insert_float(map3, "two", 14.2));
	assert(bt_object_map_size(map1) == 3);
	assert(bt_object_map_size(map2) == 3);
	assert(bt_object_map_size(map3) == 3);

	ok(!bt_object_compare(bt_object_null, map1),
		"cannot compare null object and map object");
	ok(!bt_object_compare(map1, map2),
		"map objects are not equivalent");
	ok(bt_object_compare(map1, map3),
		"map objects are equivalent");

	BT_OBJECT_PUT(map1);
	BT_OBJECT_PUT(map2);
	BT_OBJECT_PUT(map3);
}

static
void test_compare(void)
{
	ok(!bt_object_compare(NULL, NULL), "cannot compare NULL and NULL");
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
	 * Here's the deal here. If we make sure that each object
	 * of our deep copy has a different address than its source,
	 * and that bt_object_compare() returns true for the top-level
	 * object, taking into account that we test the correctness of
	 * bt_object_compare() elsewhere, then the deep copy is a
	 * success.
	 */
	struct bt_object *null_copy_obj;
	struct bt_object *bool_obj, *bool_copy_obj;
	struct bt_object *integer_obj, *integer_copy_obj;
	struct bt_object *float_obj, *float_copy_obj;
	struct bt_object *string_obj, *string_copy_obj;
	struct bt_object *array_obj, *array_copy_obj;
	struct bt_object *map_obj, *map_copy_obj;

	bool_obj = bt_object_bool_create_init(true);
	integer_obj = bt_object_integer_create_init(23);
	float_obj = bt_object_float_create_init(-3.1416);
	string_obj = bt_object_string_create_init("test");
	array_obj = bt_object_array_create();
	map_obj = bt_object_map_create();

	assert(bool_obj && integer_obj && float_obj && string_obj &&
		array_obj && map_obj);

	assert(!bt_object_array_append(array_obj, bool_obj));
	assert(!bt_object_array_append(array_obj, integer_obj));
	assert(!bt_object_array_append(array_obj, float_obj));
	assert(!bt_object_array_append(array_obj, bt_object_null));
	assert(!bt_object_map_insert(map_obj, "array", array_obj));
	assert(!bt_object_map_insert(map_obj, "string", string_obj));

	map_copy_obj = bt_object_copy(NULL);
	ok(!map_copy_obj,
		"bt_object_copy() fails with a source object set to NULL");

	map_copy_obj = bt_object_copy(map_obj);
	ok(map_copy_obj,
		"bt_object_copy() succeeds");

	ok(map_obj != map_copy_obj,
		"bt_object_copy() returns a different pointer (map)");
	string_copy_obj = bt_object_map_get(map_copy_obj, "string");
	ok(string_copy_obj != string_obj,
		"bt_object_copy() returns a different pointer (string)");
	array_copy_obj = bt_object_map_get(map_copy_obj, "array");
	ok(array_copy_obj != array_obj,
		"bt_object_copy() returns a different pointer (array)");
	bool_copy_obj = bt_object_array_get(array_copy_obj, 0);
	ok(bool_copy_obj != bool_obj,
		"bt_object_copy() returns a different pointer (bool)");
	integer_copy_obj = bt_object_array_get(array_copy_obj, 1);
	ok(integer_copy_obj != integer_obj,
		"bt_object_copy() returns a different pointer (integer)");
	float_copy_obj = bt_object_array_get(array_copy_obj, 2);
	ok(float_copy_obj != float_obj,
		"bt_object_copy() returns a different pointer (float)");
	null_copy_obj = bt_object_array_get(array_copy_obj, 3);
	ok(null_copy_obj == bt_object_null,
		"bt_object_copy() returns the same pointer (null)");

	ok(bt_object_compare(map_obj, map_copy_obj),
		"source and destination objects have the same content");

	BT_OBJECT_PUT(bool_copy_obj);
	BT_OBJECT_PUT(integer_copy_obj);
	BT_OBJECT_PUT(float_copy_obj);
	BT_OBJECT_PUT(string_copy_obj);
	BT_OBJECT_PUT(array_copy_obj);
	BT_OBJECT_PUT(map_copy_obj);
	BT_OBJECT_PUT(bool_obj);
	BT_OBJECT_PUT(integer_obj);
	BT_OBJECT_PUT(float_obj);
	BT_OBJECT_PUT(string_obj);
	BT_OBJECT_PUT(array_obj);
	BT_OBJECT_PUT(map_obj);
}

static
void test_macros(void)
{
	struct bt_object *obj = bt_object_bool_create();
	struct bt_object *src;
	struct bt_object *dst;

	assert(obj);
	BT_OBJECT_PUT(obj);
	ok(!obj, "BT_OBJECT_PUT() resets the variable to NULL");

	obj = bt_object_bool_create();
	assert(obj);
	src = obj;
	BT_OBJECT_MOVE(dst, src);
	ok(!src, "BT_OBJECT_MOVE() resets the source variable to NULL");
	ok(dst == obj, "BT_OBJECT_MOVE() moves the ownership");

	BT_OBJECT_PUT(dst);
}

int main(void)
{
	plan_no_plan();

	test_macros();
	test_types();
	test_compare();
	test_copy();

	return 0;
}
