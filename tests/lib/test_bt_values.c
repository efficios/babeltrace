/*
 * test_bt_values.c
 *
 * Babeltrace value objects tests
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

#include <babeltrace/ref.h>
#include <babeltrace/values.h>
#include <babeltrace/assert-internal.h>
#include <string.h>
#include "tap/tap.h"

#define NR_TESTS 158

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
}

static
void test_bool(void)
{
	int ret;
	bt_bool value;
	struct bt_value *obj;

	obj = bt_value_bool_create();
	ok(obj && bt_value_is_bool(obj),
		"bt_value_bool_create() returns a boolean value object");

	value = BT_TRUE;
	ret = bt_value_bool_get(obj, &value);
	ok(!ret && !value, "default boolean value object value is BT_FALSE");

	BT_ASSERT(!bt_value_bool_set(obj, BT_FALSE));
	ret = bt_value_bool_set(obj, BT_TRUE);
	ok(!ret, "bt_value_bool_set() succeeds");
	ret = bt_value_bool_get(obj, &value);
	ok(!ret && value, "bt_value_bool_set() works");

	BT_PUT(obj);
	pass("putting an existing boolean value object does not cause a crash")

	value = BT_FALSE;
	obj = bt_value_bool_create_init(BT_TRUE);
	ok(obj && bt_value_is_bool(obj),
		"bt_value_bool_create_init() returns a boolean value object");
	ret = bt_value_bool_get(obj, &value);
	ok(!ret && value,
		"bt_value_bool_create_init() sets the appropriate initial value");

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

	BT_PUT(obj);
}

static
void test_real(void)
{
	int ret;
	double value;
	struct bt_value *obj;

	obj = bt_value_real_create();
	ok(obj && bt_value_is_real(obj),
		"bt_value_real_create() returns a real number value object");

	value = 17.34;
	ret = bt_value_real_get(obj, &value);
	ok(!ret && value == 0.,
		"default real number value object value is 0");

	ret = bt_value_real_set(obj, -3.1416);
	ok(!ret, "bt_value_real_set() succeeds");
	ret = bt_value_real_get(obj, &value);
	ok(!ret && value == -3.1416, "bt_value_real_set() works");

	BT_PUT(obj);
	pass("putting an existing real number value object does not cause a crash")

	obj = bt_value_real_create_init(33.1649758);
	ok(obj && bt_value_is_real(obj),
		"bt_value_real_create_init() returns a real number value object");
	ret = bt_value_real_get(obj, &value);
	ok(!ret && value == 33.1649758,
		"bt_value_real_create_init() sets the appropriate initial value");

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

	obj = bt_value_string_create_init("initial value");
	ok(obj && bt_value_is_string(obj),
		"bt_value_string_create_init() returns a string value object");
	ret = bt_value_string_get(obj, &value);
	ok(!ret && value && !strcmp(value, "initial value"),
		"bt_value_string_create_init() sets the appropriate initial value");

	BT_PUT(obj);
}

static
void test_array(void)
{
	int ret;
	bt_bool bool_value;
	int64_t int_value;
	double real_value;
	struct bt_value *obj;
	const char *string_value;
	struct bt_value *array_obj;

	array_obj = bt_value_array_create();
	ok(array_obj && bt_value_is_array(array_obj),
		"bt_value_array_create() returns an array value object");
	ok(bt_value_array_is_empty(array_obj),
		"initial array value object size is 0");

	obj = bt_value_integer_create_init(345);
	ret = bt_value_array_append(array_obj, obj);
	BT_PUT(obj);
	obj = bt_value_real_create_init(-17.45);
	ret |= bt_value_array_append(array_obj, obj);
	BT_PUT(obj);
	obj = bt_value_bool_create_init(BT_TRUE);
	ret |= bt_value_array_append(array_obj, obj);
	BT_PUT(obj);
	ret |= bt_value_array_append(array_obj, bt_value_null);
	ok(!ret, "bt_value_array_append() succeeds");
	ok(bt_value_array_size(array_obj) == 4,
		"appending an element to an array value object increment its size");

	obj = bt_value_array_get(array_obj, 0);
	ok(obj && bt_value_is_integer(obj),
		"bt_value_array_get() returns an value object with the appropriate type (integer)");
	ret = bt_value_integer_get(obj, &int_value);
	ok(!ret && int_value == 345,
		"bt_value_array_get() returns an value object with the appropriate value (integer)");
	BT_PUT(obj);
	obj = bt_value_array_get(array_obj, 1);
	ok(obj && bt_value_is_real(obj),
		"bt_value_array_get() returns an value object with the appropriate type (real number)");
	ret = bt_value_real_get(obj, &real_value);
	ok(!ret && real_value == -17.45,
		"bt_value_array_get() returns an value object with the appropriate value (real number)");
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

	obj = bt_value_integer_create_init(1001);
	BT_ASSERT(obj);
	ok(!bt_value_array_set(array_obj, 2, obj),
		"bt_value_array_set() succeeds");
	BT_PUT(obj);
	obj = bt_value_array_get(array_obj, 2);
	ok(obj && bt_value_is_integer(obj),
		"bt_value_array_set() inserts an value object with the appropriate type");
	ret = bt_value_integer_get(obj, &int_value);
	BT_ASSERT(!ret);
	ok(int_value == 1001,
		"bt_value_array_set() inserts an value object with the appropriate value");
	BT_PUT(obj);

	ret = bt_value_array_append_bool(array_obj, BT_FALSE);
	ok(!ret, "bt_value_array_append_bool() succeeds");
	ret = bt_value_array_append_integer(array_obj, 98765);
	ok(!ret, "bt_value_array_append_integer() succeeds");
	ret = bt_value_array_append_real(array_obj, 2.49578);
	ok(!ret, "bt_value_array_append_real() succeeds");
	ret = bt_value_array_append_string(array_obj, "bt_value");
	ok(!ret, "bt_value_array_append_string() succeeds");
	ret = bt_value_array_append_empty_array(array_obj);
	ok(!ret, "bt_value_array_append_empty_array() succeeds");
	ret = bt_value_array_append_empty_map(array_obj);
	ok(!ret, "bt_value_array_append_empty_map() succeeds");

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
	ok(obj && bt_value_is_real(obj),
		"bt_value_array_append_real() appends a real number value object");
	ret = bt_value_real_get(obj, &real_value);
	ok(!ret && real_value == 2.49578,
		"bt_value_array_append_real() appends the appropriate value");
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
		"bt_value_array_append_empty_array() appends an array value object");
	ok(bt_value_array_is_empty(obj),
		"bt_value_array_append_empty_array() an empty array value object");
	BT_PUT(obj);
	obj = bt_value_array_get(array_obj, 9);
	ok(obj && bt_value_is_map(obj),
		"bt_value_array_append_empty_map() appends a map value object");
	ok(bt_value_map_is_empty(obj),
		"bt_value_array_append_empty_map() an empty map value object");
	BT_PUT(obj);

	BT_PUT(array_obj);
	pass("putting an existing array value object does not cause a crash")
}

static
bt_bool test_map_foreach_cb_count(const char *key, struct bt_value *object,
	void *data)
{
	int *count = data;

	if (*count == 3) {
		return BT_FALSE;
	}

	(*count)++;

	return BT_TRUE;
}

struct map_foreach_checklist {
	bt_bool bool1;
	bt_bool int1;
	bt_bool real1;
	bt_bool null1;
	bt_bool bool2;
	bt_bool int2;
	bt_bool real2;
	bt_bool string2;
	bt_bool array2;
	bt_bool map2;
};

static
bt_bool test_map_foreach_cb_check(const char *key, struct bt_value *object,
	void *data)
{
	int ret;
	struct map_foreach_checklist *checklist = data;

	if (!strcmp(key, "bt_bool")) {
		if (checklist->bool1) {
			fail("test_map_foreach_cb_check(): duplicate key \"bt_bool\"");
		} else {
			bt_bool val = BT_FALSE;

			ret = bt_value_bool_get(object, &val);
			ok(!ret, "test_map_foreach_cb_check(): success getting \"bt_bool\" value");

			if (val) {
				pass("test_map_foreach_cb_check(): \"bt_bool\" value object has the right value");
				checklist->bool1 = BT_TRUE;
			} else {
				fail("test_map_foreach_cb_check(): \"bt_bool\" value object has the wrong value");
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
				checklist->int1 = BT_TRUE;
			} else {
				fail("test_map_foreach_cb_check(): \"int\" value object has the wrong value");
			}
		}
	} else if (!strcmp(key, "real")) {
		if (checklist->real1) {
			fail("test_map_foreach_cb_check(): duplicate key \"real\"");
		} else {
			double val = 0;

			ret = bt_value_real_get(object, &val);
			ok(!ret, "test_map_foreach_cb_check(): success getting \"real\" value");

			if (val == 5.444) {
				pass("test_map_foreach_cb_check(): \"real\" value object has the right value");
				checklist->real1 = BT_TRUE;
			} else {
				fail("test_map_foreach_cb_check(): \"real\" value object has the wrong value");
			}
		}
	} else if (!strcmp(key, "null")) {
		if (checklist->null1) {
			fail("test_map_foreach_cb_check(): duplicate key \"bt_bool\"");
		} else {
			ok(bt_value_is_null(object), "test_map_foreach_cb_check(): success getting \"null\" value object");
			checklist->null1 = BT_TRUE;
		}
	} else if (!strcmp(key, "bool2")) {
		if (checklist->bool2) {
			fail("test_map_foreach_cb_check(): duplicate key \"bool2\"");
		} else {
			bt_bool val = BT_FALSE;

			ret = bt_value_bool_get(object, &val);
			ok(!ret, "test_map_foreach_cb_check(): success getting \"bool2\" value");

			if (val) {
				pass("test_map_foreach_cb_check(): \"bool2\" value object has the right value");
				checklist->bool2 = BT_TRUE;
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
				checklist->int2 = BT_TRUE;
			} else {
				fail("test_map_foreach_cb_check(): \"int2\" value object has the wrong value");
			}
		}
	} else if (!strcmp(key, "real2")) {
		if (checklist->real2) {
			fail("test_map_foreach_cb_check(): duplicate key \"real2\"");
		} else {
			double val = 0;

			ret = bt_value_real_get(object, &val);
			ok(!ret, "test_map_foreach_cb_check(): success getting \"real2\" value");

			if (val == -49.0001) {
				pass("test_map_foreach_cb_check(): \"real2\" value object has the right value");
				checklist->real2 = BT_TRUE;
			} else {
				fail("test_map_foreach_cb_check(): \"real2\" value object has the wrong value");
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
				checklist->string2 = BT_TRUE;
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
			checklist->array2 = BT_TRUE;
		}
	} else if (!strcmp(key, "map2")) {
		if (checklist->map2) {
			fail("test_map_foreach_cb_check(): duplicate key \"map2\"");
		} else {
			ok(bt_value_is_map(object), "test_map_foreach_cb_check(): success getting \"map2\" value object");
			ok(bt_value_map_is_empty(object),
				"test_map_foreach_cb_check(): \"map2\" value object is empty");
			checklist->map2 = BT_TRUE;
		}
	} else {
		fail("test_map_foreach_cb_check(): unknown map key \"%s\"", key);
	}

	return BT_TRUE;
}

static
void test_map(void)
{
	int ret;
	int count = 0;
	bt_bool bool_value;
	int64_t int_value;
	double real_value;
	struct bt_value *obj;
	struct bt_value *map_obj;
	struct map_foreach_checklist checklist;

	map_obj = bt_value_map_create();
	ok(map_obj && bt_value_is_map(map_obj),
		"bt_value_map_create() returns a map value object");
	ok(bt_value_map_size(map_obj) == 0,
		"initial map value object size is 0");

	obj = bt_value_integer_create_init(19457);
	ret = bt_value_map_insert(map_obj, "int", obj);
	BT_PUT(obj);
	obj = bt_value_real_create_init(5.444);
	ret |= bt_value_map_insert(map_obj, "real", obj);
	BT_PUT(obj);
	obj = bt_value_bool_create();
	ret |= bt_value_map_insert(map_obj, "bt_bool", obj);
	BT_PUT(obj);
	ret |= bt_value_map_insert(map_obj, "null", bt_value_null);
	ok(!ret, "bt_value_map_insert() succeeds");
	ok(bt_value_map_size(map_obj) == 4,
		"inserting an element into a map value object increment its size");

	obj = bt_value_bool_create_init(BT_TRUE);
	ret = bt_value_map_insert(map_obj, "bt_bool", obj);
	BT_PUT(obj);
	ok(!ret, "bt_value_map_insert() accepts an existing key");

	obj = bt_value_map_get(map_obj, "life");
	ok(!obj, "bt_value_map_get() returns NULL with an non existing key");
	obj = bt_value_map_get(map_obj, "real");
	ok(obj && bt_value_is_real(obj),
		"bt_value_map_get() returns an value object with the appropriate type (real)");
	ret = bt_value_real_get(obj, &real_value);
	ok(!ret && real_value == 5.444,
		"bt_value_map_get() returns an value object with the appropriate value (real)");
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
	obj = bt_value_map_get(map_obj, "bt_bool");
	ok(obj && bt_value_is_bool(obj),
		"bt_value_map_get() returns an value object with the appropriate type (boolean)");
	ret = bt_value_bool_get(obj, &bool_value);
	ok(!ret && bool_value,
		"bt_value_map_get() returns an value object with the appropriate value (boolean)");
	BT_PUT(obj);

	ret = bt_value_map_insert_bool(map_obj, "bool2", BT_TRUE);
	ok(!ret, "bt_value_map_insert_bool() succeeds");
	ret = bt_value_map_insert_integer(map_obj, "int2", 98765);
	ok(!ret, "bt_value_map_insert_integer() succeeds");
	ret = bt_value_map_insert_real(map_obj, "real2", -49.0001);
	ok(!ret, "bt_value_map_insert_real() succeeds");
	ret = bt_value_map_insert_string(map_obj, "string2", "bt_value");
	ok(!ret, "bt_value_map_insert_string() succeeds");
	ret = bt_value_map_insert_empty_array(map_obj, "array2");
	ok(!ret, "bt_value_map_insert_empty_array() succeeds");
	ret = bt_value_map_insert_empty_map(map_obj, "map2");
	ok(!ret, "bt_value_map_insert_empty_map() succeeds");

	ok(bt_value_map_size(map_obj) == 10,
		"the bt_value_map_insert*() functions increment the map value object's size");

	ok(!bt_value_map_has_key(map_obj, "hello"),
		"map value object does not have key \"hello\"");
	ok(bt_value_map_has_key(map_obj, "bt_bool"),
		"map value object has key \"bt_bool\"");
	ok(bt_value_map_has_key(map_obj, "int"),
		"map value object has key \"int\"");
	ok(bt_value_map_has_key(map_obj, "real"),
		"map value object has key \"real\"");
	ok(bt_value_map_has_key(map_obj, "null"),
		"map value object has key \"null\"");
	ok(bt_value_map_has_key(map_obj, "bool2"),
		"map value object has key \"bool2\"");
	ok(bt_value_map_has_key(map_obj, "int2"),
		"map value object has key \"int2\"");
	ok(bt_value_map_has_key(map_obj, "real2"),
		"map value object has key \"real2\"");
	ok(bt_value_map_has_key(map_obj, "string2"),
		"map value object has key \"string2\"");
	ok(bt_value_map_has_key(map_obj, "array2"),
		"map value object has key \"array2\"");
	ok(bt_value_map_has_key(map_obj, "map2"),
		"map value object has key \"map2\"");

	ret = bt_value_map_foreach(map_obj, test_map_foreach_cb_count, &count);
	ok(ret == BT_VALUE_STATUS_CANCELED && count == 3,
		"bt_value_map_foreach() breaks the loop when the user function returns BT_FALSE");

	memset(&checklist, 0, sizeof(checklist));
	ret = bt_value_map_foreach(map_obj, test_map_foreach_cb_check,
		&checklist);
	ok(ret == BT_VALUE_STATUS_OK,
		"bt_value_map_foreach() succeeds with test_map_foreach_cb_check()");
	ok(checklist.bool1 && checklist.int1 && checklist.real1 &&
		checklist.null1 && checklist.bool2 && checklist.int2 &&
		checklist.real2 && checklist.string2 &&
		checklist.array2 && checklist.map2,
		"bt_value_map_foreach() iterates over all the map value object's elements");

	BT_PUT(map_obj);
	pass("putting an existing map value object does not cause a crash")
}

static
void test_types(void)
{
	test_null();
	test_bool();
	test_integer();
	test_real();
	test_string();
	test_array();
	test_map();
}

static
void test_compare_null(void)
{
	ok(bt_value_compare(bt_value_null, bt_value_null),
		"null value objects are equivalent");
}

static
void test_compare_bool(void)
{
	struct bt_value *bool1 = bt_value_bool_create_init(BT_FALSE);
	struct bt_value *bool2 = bt_value_bool_create_init(BT_TRUE);
	struct bt_value *bool3 = bt_value_bool_create_init(BT_FALSE);

	BT_ASSERT(bool1 && bool2 && bool3);
	ok(!bt_value_compare(bt_value_null, bool1),
		"cannot compare null value object and bt_bool value object");
	ok(!bt_value_compare(bool1, bool2),
		"boolean value objects are not equivalent (BT_FALSE and BT_TRUE)");
	ok(bt_value_compare(bool1, bool3),
		"boolean value objects are equivalent (BT_FALSE and BT_FALSE)");

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

	BT_ASSERT(int1 && int2 && int3);
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
void test_compare_real(void)
{
	struct bt_value *real1 = bt_value_real_create_init(17.38);
	struct bt_value *real2 = bt_value_real_create_init(-14.23);
	struct bt_value *real3 = bt_value_real_create_init(17.38);

	BT_ASSERT(real1 && real2 && real3);

	ok(!bt_value_compare(bt_value_null, real1),
		"cannot compare null value object and real number value object");
	ok(!bt_value_compare(real1, real2),
		"real number value objects are not equivalent (17.38 and -14.23)");
	ok(bt_value_compare(real1, real3),
		"real number value objects are equivalent (17.38 and 17.38)");

	BT_PUT(real1);
	BT_PUT(real2);
	BT_PUT(real3);
}

static
void test_compare_string(void)
{
	struct bt_value *string1 = bt_value_string_create_init("hello");
	struct bt_value *string2 = bt_value_string_create_init("bt_value");
	struct bt_value *string3 = bt_value_string_create_init("hello");

	BT_ASSERT(string1 && string2 && string3);

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

	BT_ASSERT(array1 && array2 && array3);

	ok(bt_value_compare(array1, array2),
		"empty array value objects are equivalent");

	BT_ASSERT(!bt_value_array_append_integer(array1, 23));
	BT_ASSERT(!bt_value_array_append_real(array1, 14.2));
	BT_ASSERT(!bt_value_array_append_bool(array1, BT_FALSE));
	BT_ASSERT(!bt_value_array_append_real(array2, 14.2));
	BT_ASSERT(!bt_value_array_append_integer(array2, 23));
	BT_ASSERT(!bt_value_array_append_bool(array2, BT_FALSE));
	BT_ASSERT(!bt_value_array_append_integer(array3, 23));
	BT_ASSERT(!bt_value_array_append_real(array3, 14.2));
	BT_ASSERT(!bt_value_array_append_bool(array3, BT_FALSE));
	BT_ASSERT(bt_value_array_size(array1) == 3);
	BT_ASSERT(bt_value_array_size(array2) == 3);
	BT_ASSERT(bt_value_array_size(array3) == 3);

	ok(!bt_value_compare(bt_value_null, array1),
		"cannot compare null value object and array value object");
	ok(!bt_value_compare(array1, array2),
		"array value objects are not equivalent ([23, 14.2, BT_FALSE] and [14.2, 23, BT_FALSE])");
	ok(bt_value_compare(array1, array3),
		"array value objects are equivalent ([23, 14.2, BT_FALSE] and [23, 14.2, BT_FALSE])");

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

	BT_ASSERT(map1 && map2 && map3);

	ok(bt_value_compare(map1, map2),
		"empty map value objects are equivalent");

	BT_ASSERT(!bt_value_map_insert_integer(map1, "one", 23));
	BT_ASSERT(!bt_value_map_insert_real(map1, "two", 14.2));
	BT_ASSERT(!bt_value_map_insert_bool(map1, "three", BT_FALSE));
	BT_ASSERT(!bt_value_map_insert_real(map2, "one", 14.2));
	BT_ASSERT(!bt_value_map_insert_integer(map2, "two", 23));
	BT_ASSERT(!bt_value_map_insert_bool(map2, "three", BT_FALSE));
	BT_ASSERT(!bt_value_map_insert_bool(map3, "three", BT_FALSE));
	BT_ASSERT(!bt_value_map_insert_integer(map3, "one", 23));
	BT_ASSERT(!bt_value_map_insert_real(map3, "two", 14.2));
	BT_ASSERT(bt_value_map_size(map1) == 3);
	BT_ASSERT(bt_value_map_size(map2) == 3);
	BT_ASSERT(bt_value_map_size(map3) == 3);

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
	test_compare_null();
	test_compare_bool();
	test_compare_integer();
	test_compare_real();
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
	 * and that bt_value_compare() returns BT_TRUE for the top-level
	 * value object, taking into account that we test the correctness of
	 * bt_value_compare() elsewhere, then the deep copy is a
	 * success.
	 */
	struct bt_value *null_copy_obj;
	struct bt_value *bool_obj, *bool_copy_obj;
	struct bt_value *integer_obj, *integer_copy_obj;
	struct bt_value *real_obj, *real_copy_obj;
	struct bt_value *string_obj, *string_copy_obj;
	struct bt_value *array_obj, *array_copy_obj;
	struct bt_value *map_obj, *map_copy_obj;

	bool_obj = bt_value_bool_create_init(BT_TRUE);
	integer_obj = bt_value_integer_create_init(23);
	real_obj = bt_value_real_create_init(-3.1416);
	string_obj = bt_value_string_create_init("test");
	array_obj = bt_value_array_create();
	map_obj = bt_value_map_create();

	BT_ASSERT(bool_obj && integer_obj && real_obj && string_obj &&
		array_obj && map_obj);

	BT_ASSERT(!bt_value_array_append(array_obj, bool_obj));
	BT_ASSERT(!bt_value_array_append(array_obj, integer_obj));
	BT_ASSERT(!bt_value_array_append(array_obj, real_obj));
	BT_ASSERT(!bt_value_array_append(array_obj, bt_value_null));
	BT_ASSERT(!bt_value_map_insert(map_obj, "array", array_obj));
	BT_ASSERT(!bt_value_map_insert(map_obj, "string", string_obj));

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
		"bt_value_copy() returns a different pointer (bt_bool)");
	integer_copy_obj = bt_value_array_get(array_copy_obj, 1);
	ok(integer_copy_obj != integer_obj,
		"bt_value_copy() returns a different pointer (integer)");
	real_copy_obj = bt_value_array_get(array_copy_obj, 2);
	ok(real_copy_obj != real_obj,
		"bt_value_copy() returns a different pointer (real)");
	null_copy_obj = bt_value_array_get(array_copy_obj, 3);
	ok(null_copy_obj == bt_value_null,
		"bt_value_copy() returns the same pointer (null)");

	ok(bt_value_compare(map_obj, map_copy_obj),
		"source and destination value objects have the same content");

	BT_PUT(bool_copy_obj);
	BT_PUT(integer_copy_obj);
	BT_PUT(real_copy_obj);
	BT_PUT(string_copy_obj);
	BT_PUT(array_copy_obj);
	BT_PUT(map_copy_obj);
	BT_PUT(bool_obj);
	BT_PUT(integer_obj);
	BT_PUT(real_obj);
	BT_PUT(string_obj);
	BT_PUT(array_obj);
	BT_PUT(map_obj);
}

static
bt_bool compare_map_elements(struct bt_value *map_a, struct bt_value *map_b,
		const char *key)
{
	struct bt_value *elem_a = NULL;
	struct bt_value *elem_b = NULL;
	bt_bool equal;

	elem_a = bt_value_map_get(map_a, key);
	elem_b = bt_value_map_get(map_b, key);
	equal = bt_value_compare(elem_a, elem_b);
	BT_PUT(elem_a);
	BT_PUT(elem_b);

	return equal;
}

static
void test_extend(void)
{
	struct bt_value *base_map = bt_value_map_create();
	struct bt_value *extension_map = bt_value_map_create();
	struct bt_value *extended_map = NULL;
	struct bt_value *array = bt_value_array_create();
	enum bt_value_status status;

	BT_ASSERT(base_map);
	BT_ASSERT(extension_map);
	BT_ASSERT(array);
	status = bt_value_map_insert_bool(base_map, "file", BT_TRUE);
	BT_ASSERT(status == BT_VALUE_STATUS_OK);
	status = bt_value_map_insert_bool(base_map, "edit", BT_FALSE);
	BT_ASSERT(status == BT_VALUE_STATUS_OK);
	status = bt_value_map_insert_integer(base_map, "selection", 17);
	BT_ASSERT(status == BT_VALUE_STATUS_OK);
	status = bt_value_map_insert_integer(base_map, "find", -34);
	BT_ASSERT(status == BT_VALUE_STATUS_OK);
	status = bt_value_map_insert_bool(extension_map, "edit", BT_TRUE);
	BT_ASSERT(status == BT_VALUE_STATUS_OK);
	status = bt_value_map_insert_integer(extension_map, "find", 101);
	BT_ASSERT(status == BT_VALUE_STATUS_OK);
	status = bt_value_map_insert_real(extension_map, "project", -404);
	BT_ASSERT(status == BT_VALUE_STATUS_OK);
	extended_map = bt_value_map_extend(base_map, extension_map);
	ok(extended_map, "bt_value_map_extend() succeeds");
	ok(bt_value_map_size(extended_map) == 5,
		"bt_value_map_extend() returns a map object with the correct size");
	ok(compare_map_elements(base_map, extended_map, "file"),
		"bt_value_map_extend() picks the appropriate element (file)");
	ok(compare_map_elements(extension_map, extended_map, "edit"),
		"bt_value_map_extend() picks the appropriate element (edit)");
	ok(compare_map_elements(base_map, extended_map, "selection"),
		"bt_value_map_extend() picks the appropriate element (selection)");
	ok(compare_map_elements(extension_map, extended_map, "find"),
		"bt_value_map_extend() picks the appropriate element (find)");
	ok(compare_map_elements(extension_map, extended_map, "project"),
		"bt_value_map_extend() picks the appropriate element (project)");

	BT_PUT(array);
	BT_PUT(base_map);
	BT_PUT(extension_map);
	BT_PUT(extended_map);
}

int main(void)
{
	plan_tests(NR_TESTS);
	test_types();
	test_compare();
	test_copy();
	test_extend();
	return 0;
}
