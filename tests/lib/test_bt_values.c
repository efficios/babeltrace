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

#include <babeltrace2/babeltrace.h>
#include "common/assert.h"
#include <string.h>
#include "tap/tap.h"

#define NR_TESTS 190

static
void test_null(void)
{
	ok(bt_value_null, "bt_value_null is not NULL");
	ok(bt_value_is_null(bt_value_null),
		"bt_value_null is a null value object");
	bt_value_get_ref(bt_value_null);
	pass("getting bt_value_null does not cause a crash");
	bt_value_put_ref(bt_value_null);
	pass("putting bt_value_null does not cause a crash");
}

static
void test_bool(void)
{
	bt_bool value;
	bt_value *obj;

	obj = bt_value_bool_create();
	ok(obj && bt_value_is_bool(obj),
		"bt_value_bool_create() returns a boolean value object");

	value = BT_TRUE;
	value = bt_value_bool_get(obj);
	ok(!value, "default boolean value object value is BT_FALSE");

	bt_value_bool_set(obj, BT_FALSE);
	bt_value_bool_set(obj, BT_TRUE);
	value = bt_value_bool_get(obj);
	ok(value, "bt_value_bool_set() works");

	BT_VALUE_PUT_REF_AND_RESET(obj);
	pass("putting an existing boolean value object does not cause a crash")

	value = BT_FALSE;
	obj = bt_value_bool_create_init(BT_TRUE);
	ok(obj && bt_value_is_bool(obj),
		"bt_value_bool_create_init() returns a boolean value object");
	value = bt_value_bool_get(obj);
	ok(value,
		"bt_value_bool_create_init() sets the appropriate initial value");

	BT_VALUE_PUT_REF_AND_RESET(obj);
}

static
void test_unsigned_integer(void)
{
	uint64_t value;
	bt_value *obj;

	obj = bt_value_integer_unsigned_create();
	ok(obj && bt_value_is_unsigned_integer(obj),
		"bt_value_integer_unsigned_create() returns an unsigned integer value object");

	value = 1961;
	value = bt_value_integer_unsigned_get(obj);
	ok(value == 0, "default unsigned integer value object value is 0");

	bt_value_integer_unsigned_set(obj, 98765);
	value = bt_value_integer_unsigned_get(obj);
	ok(value == 98765, "bt_value_integer_unsigned_bool_set() works");

	BT_VALUE_PUT_REF_AND_RESET(obj);
	pass("putting an existing unsigned integer value object does not cause a crash")

	obj = bt_value_integer_unsigned_create_init(321456987);
	ok(obj && bt_value_is_unsigned_integer(obj),
		"bt_value_integer_unsigned_create_init() returns an unsigned integer value object");
	value = bt_value_integer_unsigned_get(obj);
	ok(value == 321456987,
		"bt_value_integer_unsigned_create_init() sets the appropriate initial value");

	BT_VALUE_PUT_REF_AND_RESET(obj);
}

static
void test_signed_integer(void)
{
	int64_t value;
	bt_value *obj;

	obj = bt_value_integer_signed_create();
	ok(obj && bt_value_is_signed_integer(obj),
		"bt_value_integer_signed_create() returns a signed integer value object");

	value = 1961;
	value = bt_value_integer_signed_get(obj);
	ok(value == 0, "default signed integer value object value is 0");

	bt_value_integer_signed_set(obj, 98765);
	value = bt_value_integer_signed_get(obj);
	ok(value == 98765, "bt_value_integer_signed_bool_set() works");

	BT_VALUE_PUT_REF_AND_RESET(obj);
	pass("putting an existing signed integer value object does not cause a crash")

	obj = bt_value_integer_signed_create_init(-321456987);
	ok(obj && bt_value_is_signed_integer(obj),
		"bt_value_integer_signed_create_init() returns a signed integer value object");
	value = bt_value_integer_signed_get(obj);
	ok(value == -321456987,
		"bt_value_integer_signed_create_init() sets the appropriate initial value");

	BT_VALUE_PUT_REF_AND_RESET(obj);
}

static
void test_real(void)
{
	double value;
	bt_value *obj;

	obj = bt_value_real_create();
	ok(obj && bt_value_is_real(obj),
		"bt_value_real_create() returns a real number value object");

	value = 17.34;
	value = bt_value_real_get(obj);
	ok(value == 0.,
		"default real number value object value is 0");

	bt_value_real_set(obj, -3.1416);
	value = bt_value_real_get(obj);
	ok(value == -3.1416, "bt_value_real_set() works");

	BT_VALUE_PUT_REF_AND_RESET(obj);
	pass("putting an existing real number value object does not cause a crash")

	obj = bt_value_real_create_init(33.1649758);
	ok(obj && bt_value_is_real(obj),
		"bt_value_real_create_init() returns a real number value object");
	value = bt_value_real_get(obj);
	ok(value == 33.1649758,
		"bt_value_real_create_init() sets the appropriate initial value");

	BT_VALUE_PUT_REF_AND_RESET(obj);
}

static
void test_string(void)
{
	const char *value;
	bt_value *obj;

	obj = bt_value_string_create();
	ok(obj && bt_value_is_string(obj),
		"bt_value_string_create() returns a string value object");

	value = bt_value_string_get(obj);
	ok(value && strcmp(value, "") == 0,
		"default string value object value is \"\"");

	bt_value_string_set(obj, "hello worldz");
	value = bt_value_string_get(obj);
	ok(value && strcmp(value, "hello worldz") == 0,
		"bt_value_string_get() works");

	BT_VALUE_PUT_REF_AND_RESET(obj);
	pass("putting an existing string value object does not cause a crash")

	obj = bt_value_string_create_init("initial value");
	ok(obj && bt_value_is_string(obj),
		"bt_value_string_create_init() returns a string value object");
	value = bt_value_string_get(obj);
	ok(value && strcmp(value, "initial value") == 0,
		"bt_value_string_create_init() sets the appropriate initial value");

	BT_VALUE_PUT_REF_AND_RESET(obj);
}

static
void test_array(void)
{
	int ret;
	bt_bool bool_value;
	int64_t int_value;
	double real_value;
	bt_value *obj;
	const char *string_value;
	bt_value *array_obj;
	bt_value *appended_obj;

	array_obj = bt_value_array_create();
	ok(array_obj && bt_value_is_array(array_obj),
		"bt_value_array_create() returns an array value object");
	ok(bt_value_array_is_empty(array_obj),
		"initial array value object size is 0");

	obj = bt_value_integer_unsigned_create_init(345);
	ret = bt_value_array_append_element(array_obj, obj);
	BT_VALUE_PUT_REF_AND_RESET(obj);
	obj = bt_value_integer_signed_create_init(-507);
	ret |= bt_value_array_append_element(array_obj, obj);
	BT_VALUE_PUT_REF_AND_RESET(obj);
	obj = bt_value_real_create_init(-17.45);
	ret |= bt_value_array_append_element(array_obj, obj);
	BT_VALUE_PUT_REF_AND_RESET(obj);
	obj = bt_value_bool_create_init(BT_TRUE);
	ret |= bt_value_array_append_element(array_obj, obj);
	BT_VALUE_PUT_REF_AND_RESET(obj);
	ret |= bt_value_array_append_element(array_obj,
		bt_value_null);
	ok(!ret, "bt_value_array_append_element() succeeds");
	ok(bt_value_array_get_length(array_obj) == 5,
		"appending an element to an array value object increment its size");

	obj = bt_value_array_borrow_element_by_index(array_obj, 0);
	ok(bt_value_is_unsigned_integer(obj),
		"bt_value_array_borrow_element_by_index() returns an value object with the appropriate type (unsigned integer)");
	int_value = bt_value_integer_unsigned_get(obj);
	ok(int_value == 345,
		"bt_value_array_borrow_element_by_index() returns an value object with the appropriate value (unsigned integer)");
	obj = bt_value_array_borrow_element_by_index(array_obj, 1);
	ok(bt_value_is_signed_integer(obj),
		"bt_value_array_borrow_element_by_index() returns an value object with the appropriate type (signed integer)");
	int_value = bt_value_integer_signed_get(obj);
	ok(int_value == -507,
		"bt_value_array_borrow_element_by_index() returns an value object with the appropriate value (signed integer)");
	obj = bt_value_array_borrow_element_by_index(array_obj, 2);
	ok(bt_value_is_real(obj),
		"bt_value_array_borrow_element_by_index() returns an value object with the appropriate type (real number)");
	real_value = bt_value_real_get(obj);
	ok(real_value == -17.45,
		"bt_value_array_borrow_element_by_index() returns an value object with the appropriate value (real number)");
	obj = bt_value_array_borrow_element_by_index(array_obj, 3);
	ok(bt_value_is_bool(obj),
		"bt_value_array_borrow_element_by_index() returns an value object with the appropriate type (boolean)");
	bool_value = bt_value_bool_get(obj);
	ok(bool_value,
		"bt_value_array_borrow_element_by_index() returns an value object with the appropriate value (boolean)");
	obj = bt_value_array_borrow_element_by_index(array_obj, 4);
	ok(bt_value_null,
		"bt_value_array_borrow_element_by_index() returns an value object with the appropriate type (null)");

	obj = bt_value_integer_signed_create_init(1001);
	BT_ASSERT(obj);
	ok(!bt_value_array_set_element_by_index(array_obj, 2, obj),
		"bt_value_array_set_element_by_index() succeeds");
	BT_VALUE_PUT_REF_AND_RESET(obj);
	obj = bt_value_array_borrow_element_by_index(array_obj, 2);
	ok(bt_value_is_signed_integer(obj),
		"bt_value_array_set_element_by_index() inserts an value object with the appropriate type");
	int_value = bt_value_integer_signed_get(obj);
	BT_ASSERT(!ret);
	ok(int_value == 1001,
		"bt_value_array_set_element_by_index() inserts an value object with the appropriate value");

	ret = bt_value_array_append_bool_element(array_obj,
		BT_FALSE);
	ok(!ret, "bt_value_array_append_bool_element() succeeds");
	ret = bt_value_array_append_unsigned_integer_element(array_obj,
		98765);
	ok(!ret, "bt_value_array_append_unsigned_integer_element() succeeds");
	ret = bt_value_array_append_signed_integer_element(array_obj,
		-10101);
	ok(!ret, "bt_value_array_append_signed_integer_element() succeeds");
	ret = bt_value_array_append_real_element(array_obj,
		2.49578);
	ok(!ret, "bt_value_array_append_real_element() succeeds");
	ret = bt_value_array_append_string_element(array_obj,
		"bt_value");
	ok(!ret, "bt_value_array_append_string_element() succeeds");
	ret = bt_value_array_append_empty_array_element(array_obj, NULL);
	ok(!ret, "bt_value_array_append_empty_array_element() succeeds");
	ret = bt_value_array_append_empty_array_element(array_obj, &appended_obj);
	ok(!ret, "bt_value_array_append_empty_array_element() with returned value object succeeds");
	ok(appended_obj,
		"object returned by bt_value_array_append_empty_array_element() is not NULL");
	ok(bt_value_is_array(appended_obj),
		"object returned by bt_value_array_append_empty_array_element() is an array value");
	ret = bt_value_array_append_empty_map_element(array_obj, NULL);
	ok(!ret, "bt_value_array_append_empty_map_element() succeeds");
	ret = bt_value_array_append_empty_map_element(array_obj, &appended_obj);
	ok(!ret, "bt_value_array_append_empty_map_element() with returned value object succeeds");
	ok(appended_obj,
		"object returned by bt_value_array_append_empty_map_element() is not NULL");
	ok(bt_value_is_map(appended_obj),
		"object returned by bt_value_array_append_empty_map_element() is an array value");

	ok(bt_value_array_get_length(array_obj) == 14,
		"the bt_value_array_append_element_*() functions increment the array value object's size");
	ok(!bt_value_array_is_empty(array_obj),
		"map value object is not empty");

	obj = bt_value_array_borrow_element_by_index(array_obj, 5);
	ok(bt_value_is_bool(obj),
		"bt_value_array_append_bool_element() appends a boolean value object");
	bool_value = bt_value_bool_get(obj);
	ok(!bool_value,
		"bt_value_array_append_bool_element() appends the appropriate value");
	obj = bt_value_array_borrow_element_by_index(array_obj, 6);
	ok(bt_value_is_unsigned_integer(obj),
		"bt_value_array_append_unsigned_integer_element() appends an unsigned integer value object");
	int_value = bt_value_integer_unsigned_get(obj);
	ok(int_value == 98765,
		"bt_value_array_append_unsigned_integer_element() appends the appropriate value");
	obj = bt_value_array_borrow_element_by_index(array_obj, 7);
	ok(bt_value_is_signed_integer(obj),
		"bt_value_array_append_signed_integer_element() appends a signed integer value object");
	int_value = bt_value_integer_signed_get(obj);
	ok(int_value == -10101,
		"bt_value_array_append_signed_integer_element() appends the appropriate value");
	obj = bt_value_array_borrow_element_by_index(array_obj, 8);
	ok(bt_value_is_real(obj),
		"bt_value_array_append_real_element() appends a real number value object");
	real_value = bt_value_real_get(obj);
	ok(real_value == 2.49578,
		"bt_value_array_append_real_element() appends the appropriate value");
	obj = bt_value_array_borrow_element_by_index(array_obj, 9);
	ok(bt_value_is_string(obj),
		"bt_value_array_append_string_element() appends a string value object");
	string_value = bt_value_string_get(obj);
	ok(!ret && string_value && strcmp(string_value, "bt_value") == 0,
		"bt_value_array_append_string_element() appends the appropriate value");
	obj = bt_value_array_borrow_element_by_index(array_obj, 10);
	ok(bt_value_is_array(obj),
		"bt_value_array_append_empty_array_element() appends an array value object");
	ok(bt_value_array_is_empty(obj),
		"bt_value_array_append_empty_array_element() an empty array value object");
	obj = bt_value_array_borrow_element_by_index(array_obj, 11);
	ok(bt_value_is_array(obj),
		"bt_value_array_append_empty_array_element() appends an array value object");
	ok(bt_value_array_is_empty(obj),
		"bt_value_array_append_empty_array_element() an empty array value object");
	obj = bt_value_array_borrow_element_by_index(array_obj, 12);
	ok(bt_value_is_map(obj),
		"bt_value_array_append_empty_map_element() appends a map value object");
	ok(bt_value_map_is_empty(obj),
		"bt_value_array_append_empty_map_element() an empty map value object");
	obj = bt_value_array_borrow_element_by_index(array_obj, 13);
	ok(bt_value_is_map(obj),
		"bt_value_array_append_empty_map_element() appends a map value object");
	ok(bt_value_map_is_empty(obj),
		"bt_value_array_append_empty_map_element() an empty map value object");

	BT_VALUE_PUT_REF_AND_RESET(array_obj);
	pass("putting an existing array value object does not cause a crash")
}

static
bt_value_map_foreach_entry_func_status test_map_foreach_cb_count(
		const char *key, bt_value *object,
	void *data)
{
	int *count = data;

	if (*count == 3) {
		return BT_VALUE_MAP_FOREACH_ENTRY_FUNC_STATUS_INTERRUPT;
	} else if (*count == 4) {
		return BT_VALUE_MAP_FOREACH_ENTRY_FUNC_STATUS_ERROR;
	} else if (*count == 5) {
		return BT_VALUE_MAP_FOREACH_ENTRY_FUNC_STATUS_MEMORY_ERROR;
	}

	(*count)++;

	return BT_VALUE_MAP_FOREACH_ENTRY_FUNC_STATUS_OK;
}

struct map_foreach_checklist {
	bt_bool bool1;
	bt_bool uint;
	bt_bool int1;
	bt_bool real1;
	bt_bool null1;
	bt_bool bool2;
	bt_bool int2;
	bt_bool real2;
	bt_bool string2;
	bt_bool array2;
	bt_bool array3;
	bt_bool map2;
	bt_bool map3;
};

static
bt_value_map_foreach_entry_func_status test_map_foreach_cb_check(
		const char *key, bt_value *object, void *data)
{
	struct map_foreach_checklist *checklist = data;

	if (strcmp(key, "bt_bool") == 0) {
		if (checklist->bool1) {
			fail("test_map_foreach_cb_check(): duplicate key \"bt_bool\"");
		} else {
			bt_bool val = BT_FALSE;

			val = bt_value_bool_get(object);

			if (val) {
				pass("test_map_foreach_cb_check(): \"bt_bool\" value object has the right value");
				checklist->bool1 = BT_TRUE;
			} else {
				fail("test_map_foreach_cb_check(): \"bt_bool\" value object has the wrong value");
			}
		}
	} else if (strcmp(key, "uint") == 0) {
		if (checklist->uint) {
			fail("test_map_foreach_cb_check(): duplicate key \"uint\"");
		} else {
			uint64_t val = 0;

			val = bt_value_integer_unsigned_get(object);

			if (val == 19457) {
				pass("test_map_foreach_cb_check(): \"uint\" value object has the right value");
				checklist->uint = BT_TRUE;
			} else {
				fail("test_map_foreach_cb_check(): \"uint\" value object has the wrong value");
			}
		}
	} else if (strcmp(key, "int") == 0) {
		if (checklist->int1) {
			fail("test_map_foreach_cb_check(): duplicate key \"int\"");
		} else {
			int64_t val = 0;

			val = bt_value_integer_signed_get(object);

			if (val == -12345) {
				pass("test_map_foreach_cb_check(): \"int\" value object has the right value");
				checklist->int1 = BT_TRUE;
			} else {
				fail("test_map_foreach_cb_check(): \"int\" value object has the wrong value");
			}
		}
	} else if (strcmp(key, "real") == 0) {
		if (checklist->real1) {
			fail("test_map_foreach_cb_check(): duplicate key \"real\"");
		} else {
			double val = 0;

			val = bt_value_real_get(object);

			if (val == 5.444) {
				pass("test_map_foreach_cb_check(): \"real\" value object has the right value");
				checklist->real1 = BT_TRUE;
			} else {
				fail("test_map_foreach_cb_check(): \"real\" value object has the wrong value");
			}
		}
	} else if (strcmp(key, "null") == 0) {
		if (checklist->null1) {
			fail("test_map_foreach_cb_check(): duplicate key \"bt_bool\"");
		} else {
			ok(bt_value_is_null(object), "test_map_foreach_cb_check(): success getting \"null\" value object");
			checklist->null1 = BT_TRUE;
		}
	} else if (strcmp(key, "bool2") == 0) {
		if (checklist->bool2) {
			fail("test_map_foreach_cb_check(): duplicate key \"bool2\"");
		} else {
			bt_bool val = BT_FALSE;

			val = bt_value_bool_get(object);

			if (val) {
				pass("test_map_foreach_cb_check(): \"bool2\" value object has the right value");
				checklist->bool2 = BT_TRUE;
			} else {
				fail("test_map_foreach_cb_check(): \"bool2\" value object has the wrong value");
			}
		}
	} else if (strcmp(key, "int2") == 0) {
		if (checklist->int2) {
			fail("test_map_foreach_cb_check(): duplicate key \"int2\"");
		} else {
			int64_t val = 0;

			val = bt_value_integer_signed_get(object);

			if (val == 98765) {
				pass("test_map_foreach_cb_check(): \"int2\" value object has the right value");
				checklist->int2 = BT_TRUE;
			} else {
				fail("test_map_foreach_cb_check(): \"int2\" value object has the wrong value");
			}
		}
	} else if (strcmp(key, "real2") == 0) {
		if (checklist->real2) {
			fail("test_map_foreach_cb_check(): duplicate key \"real2\"");
		} else {
			double val = 0;

			val = bt_value_real_get(object);

			if (val == -49.0001) {
				pass("test_map_foreach_cb_check(): \"real2\" value object has the right value");
				checklist->real2 = BT_TRUE;
			} else {
				fail("test_map_foreach_cb_check(): \"real2\" value object has the wrong value");
			}
		}
	} else if (strcmp(key, "string2") == 0) {
		if (checklist->string2) {
			fail("test_map_foreach_cb_check(): duplicate key \"string2\"");
		} else {
			const char *val;

			val = bt_value_string_get(object);

			if (val && strcmp(val, "bt_value") == 0) {
				pass("test_map_foreach_cb_check(): \"string2\" value object has the right value");
				checklist->string2 = BT_TRUE;
			} else {
				fail("test_map_foreach_cb_check(): \"string2\" value object has the wrong value");
			}
		}
	} else if (strcmp(key, "array2") == 0) {
		if (checklist->array2) {
			fail("test_map_foreach_cb_check(): duplicate key \"array2\"");
		} else {
			ok(bt_value_is_array(object), "test_map_foreach_cb_check(): success getting \"array2\" value object");
			ok(bt_value_array_is_empty(object),
				"test_map_foreach_cb_check(): \"array2\" value object is empty");
			checklist->array2 = BT_TRUE;
		}
	} else if (strcmp(key, "array3") == 0) {
		if (checklist->array3) {
			fail("test_map_foreach_cb_check(): duplicate key \"array3\"");
		} else {
			ok(bt_value_is_array(object), "test_map_foreach_cb_check(): success getting \"array3\" value object");
			ok(bt_value_array_is_empty(object),
				"test_map_foreach_cb_check(): \"array3\" value object is empty");
			checklist->array3 = BT_TRUE;
		}
	} else if (strcmp(key, "map3") == 0) {
		if (checklist->map3) {
			fail("test_map_foreach_cb_check(): duplicate key \"map3\"");
		} else {
			ok(bt_value_is_map(object), "test_map_foreach_cb_check(): success getting \"map3\" value object");
			ok(bt_value_map_is_empty(object),
				"test_map_foreach_cb_check(): \"map3\" value object is empty");
			checklist->map3 = BT_TRUE;
		}
	} else if (strcmp(key, "map2") == 0) {
		if (checklist->map2) {
			fail("test_map_foreach_cb_check(): duplicate key \"map2\"");
		} else {
			ok(bt_value_is_map(object), "test_map_foreach_cb_check(): success getting \"map2\" value object");
			ok(bt_value_map_is_empty(object),
				"test_map_foreach_cb_check(): \"map2\" value object is empty");
			checklist->map2 = BT_TRUE;
		}
	} else {
		fail("test_map_foreach_cb_check(): unknown map key \"%s\"",
			key);
	}

	return BT_VALUE_MAP_FOREACH_ENTRY_FUNC_STATUS_OK;
}

static
void test_map(void)
{
	int ret;
	int count = 0;
	bt_bool bool_value;
	int64_t int_value;
	double real_value;
	bt_value *obj;
	bt_value *map_obj;
	bt_value *inserted_obj;
	struct map_foreach_checklist checklist;

	map_obj = bt_value_map_create();
	ok(map_obj && bt_value_is_map(map_obj),
		"bt_value_map_create() returns a map value object");
	ok(bt_value_map_get_size(map_obj) == 0,
		"initial map value object size is 0");

	obj = bt_value_integer_unsigned_create_init(19457);
	ret = bt_value_map_insert_entry(map_obj, "uint", obj);
	BT_VALUE_PUT_REF_AND_RESET(obj);
	obj = bt_value_integer_signed_create_init(-12345);
	ret |= bt_value_map_insert_entry(map_obj, "int", obj);
	BT_VALUE_PUT_REF_AND_RESET(obj);
	obj = bt_value_real_create_init(5.444);
	ret |= bt_value_map_insert_entry(map_obj, "real", obj);
	BT_VALUE_PUT_REF_AND_RESET(obj);
	obj = bt_value_bool_create();
	ret |= bt_value_map_insert_entry(map_obj, "bt_bool", obj);
	BT_VALUE_PUT_REF_AND_RESET(obj);
	ret |= bt_value_map_insert_entry(map_obj, "null",
		bt_value_null);
	ok(!ret, "bt_value_map_insert_entry() succeeds");
	ok(bt_value_map_get_size(map_obj) == 5,
		"inserting an element into a map value object increment its size");

	obj = bt_value_bool_create_init(BT_TRUE);
	ret = bt_value_map_insert_entry(map_obj, "bt_bool", obj);
	BT_VALUE_PUT_REF_AND_RESET(obj);
	ok(!ret, "bt_value_map_insert_entry() accepts an existing key");

	obj = bt_value_map_borrow_entry_value(map_obj, "life");
	ok(!obj, "bt_value_map_borrow_entry_value() returns NULL with an non existing key");
	obj = bt_value_map_borrow_entry_value(map_obj, "real");
	ok(obj && bt_value_is_real(obj),
		"bt_value_map_borrow_entry_value() returns an value object with the appropriate type (real)");
	real_value = bt_value_real_get(obj);
	ok(real_value == 5.444,
		"bt_value_map_borrow_entry_value() returns an value object with the appropriate value (real)");
	obj = bt_value_map_borrow_entry_value(map_obj, "uint");
	ok(obj && bt_value_is_unsigned_integer(obj),
		"bt_value_map_borrow_entry_value() returns an value object with the appropriate type (unsigned integer)");
	int_value = bt_value_integer_unsigned_get(obj);
	ok(int_value == 19457,
		"bt_value_map_borrow_entry_value() returns an value object with the appropriate value (unsigned integer)");
	obj = bt_value_map_borrow_entry_value(map_obj, "int");
	ok(obj && bt_value_is_signed_integer(obj),
		"bt_value_map_borrow_entry_value() returns an value object with the appropriate type (signed integer)");
	int_value = bt_value_integer_signed_get(obj);
	ok(int_value == -12345,
		"bt_value_map_borrow_entry_value() returns an value object with the appropriate value (signed integer)");
	obj = bt_value_map_borrow_entry_value(map_obj, "null");
	ok(obj && bt_value_is_null(obj),
		"bt_value_map_borrow_entry_value() returns an value object with the appropriate type (null)");
	obj = bt_value_map_borrow_entry_value(map_obj, "bt_bool");
	ok(obj && bt_value_is_bool(obj),
		"bt_value_map_borrow_entry_value() returns an value object with the appropriate type (boolean)");
	bool_value = bt_value_bool_get(obj);
	ok(bool_value,
		"bt_value_map_borrow_entry_value() returns an value object with the appropriate value (boolean)");

	ret = bt_value_map_insert_bool_entry(map_obj, "bool2",
		BT_TRUE);
	ok(!ret, "bt_value_map_insert_bool_entry() succeeds");
	ret = bt_value_map_insert_signed_integer_entry(map_obj, "int2",
		98765);
	ok(!ret, "bt_value_map_insert_signed_integer_entry() succeeds");
	ret = bt_value_map_insert_real_entry(map_obj, "real2",
		-49.0001);
	ok(!ret, "bt_value_map_insert_real_entry() succeeds");
	ret = bt_value_map_insert_string_entry(map_obj, "string2",
		"bt_value");
	ok(!ret, "bt_value_map_insert_string_entry() succeeds");
	ret = bt_value_map_insert_empty_array_entry(map_obj, "array2", NULL);
	ok(!ret, "bt_value_map_insert_empty_array_entry() succeeds");
	ret = bt_value_map_insert_empty_array_entry(map_obj, "array3", &inserted_obj);
	ok(!ret, "bt_value_map_insert_empty_array_entry() with returned value object succeeds");
	ok(inserted_obj,
		"object returned by bt_value_map_insert_empty_array_entry() is not NULL");
	ok(bt_value_is_array(inserted_obj),
		"object returned by bt_value_map_insert_empty_array_entry() is an array value");
	ret = bt_value_map_insert_empty_map_entry(map_obj, "map2", NULL);
	ok(!ret, "bt_value_map_insert_empty_map_entry() succeeds");
	ret = bt_value_map_insert_empty_map_entry(map_obj, "map3", &inserted_obj);
	ok(!ret, "bt_value_map_insert_empty_map_entry() with returned value object succeeds");
	ok(inserted_obj,
		"object returned by bt_value_map_insert_empty_map_entry() is not NULL");
	ok(bt_value_is_map(inserted_obj),
		"object returned by bt_value_map_insert_empty_map_entry() is an array value");

	ok(bt_value_map_get_size(map_obj) == 13,
		"the bt_value_map_insert*() functions increment the map value object's size");

	ok(!bt_value_map_has_entry(map_obj, "hello"),
		"map value object does not have key \"hello\"");
	ok(bt_value_map_has_entry(map_obj, "bt_bool"),
		"map value object has key \"bt_bool\"");
	ok(bt_value_map_has_entry(map_obj, "uint"),
		"map value object has key \"uint\"");
	ok(bt_value_map_has_entry(map_obj, "int"),
		"map value object has key \"int\"");
	ok(bt_value_map_has_entry(map_obj, "real"),
		"map value object has key \"real\"");
	ok(bt_value_map_has_entry(map_obj, "null"),
		"map value object has key \"null\"");
	ok(bt_value_map_has_entry(map_obj, "bool2"),
		"map value object has key \"bool2\"");
	ok(bt_value_map_has_entry(map_obj, "int2"),
		"map value object has key \"int2\"");
	ok(bt_value_map_has_entry(map_obj, "real2"),
		"map value object has key \"real2\"");
	ok(bt_value_map_has_entry(map_obj, "string2"),
		"map value object has key \"string2\"");
	ok(bt_value_map_has_entry(map_obj, "array2"),
		"map value object has key \"array2\"");
	ok(bt_value_map_has_entry(map_obj, "array3"),
		"map value object has key \"array3\"");
	ok(bt_value_map_has_entry(map_obj, "map2"),
		"map value object has key \"map2\"");
	ok(bt_value_map_has_entry(map_obj, "map3"),
		"map value object has key \"map3\"");

	ret = bt_value_map_foreach_entry(map_obj, test_map_foreach_cb_count,
		&count);
	ok(ret == BT_VALUE_MAP_FOREACH_ENTRY_STATUS_INTERRUPTED && count == 3,
		"bt_value_map_foreach_entry() breaks the loop when the user function returns BT_VALUE_MAP_FOREACH_ENTRY_FUNC_STATUS_INTERRUPT");

	count = 4;
	ret = bt_value_map_foreach_entry(map_obj, test_map_foreach_cb_count,
		&count);
	ok(ret == BT_VALUE_MAP_FOREACH_ENTRY_STATUS_USER_ERROR,
		"bt_value_map_foreach_entry() fails when the user function returns BT_VALUE_MAP_FOREACH_ENTRY_FUNC_STATUS_ERROR");
	bt_current_thread_clear_error();

	count = 5;
	ret = bt_value_map_foreach_entry(map_obj, test_map_foreach_cb_count,
		&count);
	ok(ret == BT_VALUE_MAP_FOREACH_ENTRY_STATUS_MEMORY_ERROR,
		"bt_value_map_foreach_entry() fails when the user function returns BT_VALUE_MAP_FOREACH_ENTRY_FUNC_STATUS_MEMORY_ERROR");
	bt_current_thread_clear_error();

	memset(&checklist, 0, sizeof(checklist));
	ret = bt_value_map_foreach_entry(map_obj, test_map_foreach_cb_check,
		&checklist);
	ok(ret == BT_VALUE_MAP_FOREACH_ENTRY_STATUS_OK,
		"bt_value_map_foreach_entry() succeeds with test_map_foreach_cb_check()");
	ok(checklist.bool1 && checklist.uint && checklist.int1 &&
		checklist.real1 && checklist.null1 && checklist.bool2 &&
		checklist.int2 && checklist.real2 && checklist.string2 &&
		checklist.array2 && checklist.map2,
		"bt_value_map_foreach_entry() iterates over all the map value object's elements");

	BT_VALUE_PUT_REF_AND_RESET(map_obj);
	pass("putting an existing map value object does not cause a crash")
}

static
void test_types(void)
{
	test_null();
	test_bool();
	test_unsigned_integer();
	test_signed_integer();
	test_real();
	test_string();
	test_array();
	test_map();
}

static
void test_is_equal_null(void)
{
	ok(bt_value_is_equal(bt_value_null, bt_value_null),
		"null value objects are equivalent");
}

static
void test_is_equal_bool(void)
{
	bt_value *bool1 =
		bt_value_bool_create_init(BT_FALSE);
	bt_value *bool2 =
		bt_value_bool_create_init(BT_TRUE);
	bt_value *bool3 =
		bt_value_bool_create_init(BT_FALSE);

	BT_ASSERT(bool1 && bool2 && bool3);
	ok(!bt_value_is_equal(bt_value_null, bool1),
		"cannot compare null value object and bt_bool value object");
	ok(!bt_value_is_equal(bool1, bool2),
		"boolean value objects are not equivalent (BT_FALSE and BT_TRUE)");
	ok(bt_value_is_equal(bool1, bool3),
		"boolean value objects are equivalent (BT_FALSE and BT_FALSE)");

	BT_VALUE_PUT_REF_AND_RESET(bool1);
	BT_VALUE_PUT_REF_AND_RESET(bool2);
	BT_VALUE_PUT_REF_AND_RESET(bool3);
}

static
void test_is_equal_unsigned_integer(void)
{
	bt_value *int1 =
		bt_value_integer_unsigned_create_init(10);
	bt_value *int2 =
		bt_value_integer_unsigned_create_init(23);
	bt_value *int3 =
		bt_value_integer_unsigned_create_init(10);

	BT_ASSERT(int1 && int2 && int3);
	ok(!bt_value_is_equal(bt_value_null, int1),
		"cannot compare null value object and unsigned integer value object");
	ok(!bt_value_is_equal(int1, int2),
		"unsigned integer value objects are not equivalent (10 and 23)");
	ok(bt_value_is_equal(int1, int3),
		"unsigned integer value objects are equivalent (10 and 10)");

	BT_VALUE_PUT_REF_AND_RESET(int1);
	BT_VALUE_PUT_REF_AND_RESET(int2);
	BT_VALUE_PUT_REF_AND_RESET(int3);
}

static
void test_is_equal_signed_integer(void)
{
	bt_value *int1 =
		bt_value_integer_signed_create_init(10);
	bt_value *int2 =
		bt_value_integer_signed_create_init(-23);
	bt_value *int3 =
		bt_value_integer_signed_create_init(10);

	BT_ASSERT(int1 && int2 && int3);
	ok(!bt_value_is_equal(bt_value_null, int1),
		"cannot compare null value object and signed integer value object");
	ok(!bt_value_is_equal(int1, int2),
		"signed integer value objects are not equivalent (10 and -23)");
	ok(bt_value_is_equal(int1, int3),
		"signed integer value objects are equivalent (10 and 10)");

	BT_VALUE_PUT_REF_AND_RESET(int1);
	BT_VALUE_PUT_REF_AND_RESET(int2);
	BT_VALUE_PUT_REF_AND_RESET(int3);
}

static
void test_is_equal_real(void)
{
	bt_value *real1 =
		bt_value_real_create_init(17.38);
	bt_value *real2 =
		bt_value_real_create_init(-14.23);
	bt_value *real3 =
		bt_value_real_create_init(17.38);

	BT_ASSERT(real1 && real2 && real3);

	ok(!bt_value_is_equal(bt_value_null, real1),
		"cannot compare null value object and real number value object");
	ok(!bt_value_is_equal(real1, real2),
		"real number value objects are not equivalent (17.38 and -14.23)");
	ok(bt_value_is_equal(real1, real3),
		"real number value objects are equivalent (17.38 and 17.38)");

	BT_VALUE_PUT_REF_AND_RESET(real1);
	BT_VALUE_PUT_REF_AND_RESET(real2);
	BT_VALUE_PUT_REF_AND_RESET(real3);
}

static
void test_is_equal_string(void)
{
	bt_value *string1 =
		bt_value_string_create_init("hello");
	bt_value *string2 =
		bt_value_string_create_init("bt_value");
	bt_value *string3 =
		bt_value_string_create_init("hello");

	BT_ASSERT(string1 && string2 && string3);

	ok(!bt_value_is_equal(bt_value_null, string1),
		"cannot compare null value object and string value object");
	ok(!bt_value_is_equal(string1, string2),
		"string value objects are not equivalent (\"hello\" and \"bt_value\")");
	ok(bt_value_is_equal(string1, string3),
		"string value objects are equivalent (\"hello\" and \"hello\")");

	BT_VALUE_PUT_REF_AND_RESET(string1);
	BT_VALUE_PUT_REF_AND_RESET(string2);
	BT_VALUE_PUT_REF_AND_RESET(string3);
}

static
void test_is_equal_array(void)
{
	bt_value *array1 = bt_value_array_create();
	bt_value *array2 = bt_value_array_create();
	bt_value *array3 = bt_value_array_create();
	bt_value_array_append_element_status append_status;

	BT_ASSERT(array1 && array2 && array3);

	ok(bt_value_is_equal(array1, array2),
		"empty array value objects are equivalent");

	append_status = bt_value_array_append_signed_integer_element(array1, 23);
	BT_ASSERT(append_status == BT_VALUE_ARRAY_APPEND_ELEMENT_STATUS_OK);
	append_status = bt_value_array_append_real_element(array1, 14.2);
	BT_ASSERT(append_status == BT_VALUE_ARRAY_APPEND_ELEMENT_STATUS_OK);
	append_status = bt_value_array_append_bool_element(array1, BT_FALSE);
	BT_ASSERT(append_status == BT_VALUE_ARRAY_APPEND_ELEMENT_STATUS_OK);
	append_status = bt_value_array_append_real_element(array2, 14.2);
	BT_ASSERT(append_status == BT_VALUE_ARRAY_APPEND_ELEMENT_STATUS_OK);
	append_status = bt_value_array_append_signed_integer_element(array2, 23);
	BT_ASSERT(append_status == BT_VALUE_ARRAY_APPEND_ELEMENT_STATUS_OK);
	append_status = bt_value_array_append_bool_element(array2, BT_FALSE);
	BT_ASSERT(append_status == BT_VALUE_ARRAY_APPEND_ELEMENT_STATUS_OK);
	append_status = bt_value_array_append_signed_integer_element(array3, 23);
	BT_ASSERT(append_status == BT_VALUE_ARRAY_APPEND_ELEMENT_STATUS_OK);
	append_status = bt_value_array_append_real_element(array3, 14.2);
	BT_ASSERT(append_status == BT_VALUE_ARRAY_APPEND_ELEMENT_STATUS_OK);
	append_status = bt_value_array_append_bool_element(array3, BT_FALSE);
	BT_ASSERT(append_status == BT_VALUE_ARRAY_APPEND_ELEMENT_STATUS_OK);
	BT_ASSERT(bt_value_array_get_length(array1) == 3);
	BT_ASSERT(bt_value_array_get_length(array2) == 3);
	BT_ASSERT(bt_value_array_get_length(array3) == 3);

	ok(!bt_value_is_equal(bt_value_null, array1),
		"cannot compare null value object and array value object");
	ok(!bt_value_is_equal(array1, array2),
		"array value objects are not equivalent ([23, 14.2, BT_FALSE] and [14.2, 23, BT_FALSE])");
	ok(bt_value_is_equal(array1, array3),
		"array value objects are equivalent ([23, 14.2, BT_FALSE] and [23, 14.2, BT_FALSE])");

	BT_VALUE_PUT_REF_AND_RESET(array1);
	BT_VALUE_PUT_REF_AND_RESET(array2);
	BT_VALUE_PUT_REF_AND_RESET(array3);
}

static
void test_is_equal_map(void)
{
	bt_value *map1 = bt_value_map_create();
	bt_value *map2 = bt_value_map_create();
	bt_value *map3 = bt_value_map_create();
	bt_value_map_insert_entry_status insert_status;

	BT_ASSERT(map1 && map2 && map3);

	ok(bt_value_is_equal(map1, map2),
		"empty map value objects are equivalent");


	insert_status = bt_value_map_insert_signed_integer_entry(map1, "one", 23);
	BT_ASSERT(insert_status == BT_VALUE_MAP_INSERT_ENTRY_STATUS_OK);
	insert_status = bt_value_map_insert_real_entry(map1, "two", 14.2);
	BT_ASSERT(insert_status == BT_VALUE_MAP_INSERT_ENTRY_STATUS_OK);
	insert_status = bt_value_map_insert_bool_entry(map1, "three",
		BT_FALSE);
	BT_ASSERT(insert_status == BT_VALUE_MAP_INSERT_ENTRY_STATUS_OK);
	insert_status = bt_value_map_insert_real_entry(map2, "one", 14.2);
	BT_ASSERT(insert_status == BT_VALUE_MAP_INSERT_ENTRY_STATUS_OK);
	insert_status = bt_value_map_insert_signed_integer_entry(map2, "two", 23);
	BT_ASSERT(insert_status == BT_VALUE_MAP_INSERT_ENTRY_STATUS_OK);
	insert_status = bt_value_map_insert_bool_entry(map2, "three",
		BT_FALSE);
	BT_ASSERT(insert_status == BT_VALUE_MAP_INSERT_ENTRY_STATUS_OK);
	insert_status = bt_value_map_insert_bool_entry(map3, "three",
		BT_FALSE);
	BT_ASSERT(insert_status == BT_VALUE_MAP_INSERT_ENTRY_STATUS_OK);
	insert_status = bt_value_map_insert_signed_integer_entry(map3, "one", 23);
	BT_ASSERT(insert_status == BT_VALUE_MAP_INSERT_ENTRY_STATUS_OK);
	insert_status = bt_value_map_insert_real_entry(map3, "two", 14.2);
	BT_ASSERT(insert_status == BT_VALUE_MAP_INSERT_ENTRY_STATUS_OK);
	BT_ASSERT(bt_value_map_get_size(map1) == 3);
	BT_ASSERT(bt_value_map_get_size(map2) == 3);
	BT_ASSERT(bt_value_map_get_size(map3) == 3);

	ok(!bt_value_is_equal(bt_value_null, map1),
		"cannot compare null value object and map value object");
	ok(!bt_value_is_equal(map1, map2),
		"map value objects are not equivalent");
	ok(bt_value_is_equal(map1, map3),
		"map value objects are equivalent");

	BT_VALUE_PUT_REF_AND_RESET(map1);
	BT_VALUE_PUT_REF_AND_RESET(map2);
	BT_VALUE_PUT_REF_AND_RESET(map3);
}

static
void test_is_equal(void)
{
	test_is_equal_null();
	test_is_equal_bool();
	test_is_equal_unsigned_integer();
	test_is_equal_signed_integer();
	test_is_equal_real();
	test_is_equal_string();
	test_is_equal_array();
	test_is_equal_map();
}

static
void test_copy(void)
{
	/*
	 * Here's the deal here. If we make sure that each value object
	 * of our deep copy has a different address than its source, and
	 * that bt_value_is_equal() returns BT_TRUE for the top-level
	 * value object, taking into account that we test the
	 * correctness of bt_value_is_equal() elsewhere, then the deep
	 * copy is a success.
	 */
	bt_value *null_copy_obj;
	bt_value *bool_obj, *bool_copy_obj;
	bt_value *unsigned_integer_obj, *unsigned_integer_copy_obj;
	bt_value *signed_integer_obj, *signed_integer_copy_obj;
	bt_value *real_obj, *real_copy_obj;
	bt_value *string_obj, *string_copy_obj;
	bt_value *array_obj, *array_copy_obj;
	bt_value *map_obj, *map_copy_obj;
	bt_value_array_append_element_status append_status;
	bt_value_map_insert_entry_status insert_status;
	bt_value_copy_status copy_status;

	bool_obj = bt_value_bool_create_init(BT_TRUE);
	unsigned_integer_obj = bt_value_integer_unsigned_create_init(23);
	signed_integer_obj = bt_value_integer_signed_create_init(-47);
	real_obj = bt_value_real_create_init(-3.1416);
	string_obj = bt_value_string_create_init("test");
	array_obj = bt_value_array_create();
	map_obj = bt_value_map_create();

	BT_ASSERT(bool_obj && unsigned_integer_obj && signed_integer_obj &&
		real_obj && string_obj && array_obj && map_obj);

	append_status = bt_value_array_append_element(array_obj, bool_obj);
	BT_ASSERT(append_status == BT_VALUE_ARRAY_APPEND_ELEMENT_STATUS_OK);
	append_status = bt_value_array_append_element(array_obj, unsigned_integer_obj);
	BT_ASSERT(append_status == BT_VALUE_ARRAY_APPEND_ELEMENT_STATUS_OK);
	append_status = bt_value_array_append_element(array_obj, signed_integer_obj);
	BT_ASSERT(append_status == BT_VALUE_ARRAY_APPEND_ELEMENT_STATUS_OK);
	append_status = bt_value_array_append_element(array_obj, real_obj);
	BT_ASSERT(append_status == BT_VALUE_ARRAY_APPEND_ELEMENT_STATUS_OK);
	append_status = bt_value_array_append_element(array_obj, bt_value_null);
	BT_ASSERT(append_status == BT_VALUE_ARRAY_APPEND_ELEMENT_STATUS_OK);
	insert_status = bt_value_map_insert_entry(map_obj, "array", array_obj);
	BT_ASSERT(insert_status == BT_VALUE_MAP_INSERT_ENTRY_STATUS_OK);
	insert_status = bt_value_map_insert_entry(map_obj, "string", string_obj);
	BT_ASSERT(insert_status == BT_VALUE_MAP_INSERT_ENTRY_STATUS_OK);

	copy_status = bt_value_copy(map_obj, &map_copy_obj);
	ok(copy_status == BT_VALUE_COPY_STATUS_OK && map_copy_obj,
		"bt_value_copy() succeeds");

	ok(map_obj != map_copy_obj,
		"bt_value_copy() returns a different pointer (map)");
	string_copy_obj = bt_value_map_borrow_entry_value(map_copy_obj,
		"string");
	ok(string_copy_obj != string_obj,
		"bt_value_copy() returns a different pointer (string)");
	array_copy_obj = bt_value_map_borrow_entry_value(map_copy_obj,
		"array");
	ok(array_copy_obj != array_obj,
		"bt_value_copy() returns a different pointer (array)");
	bool_copy_obj = bt_value_array_borrow_element_by_index(
		array_copy_obj, 0);
	ok(bool_copy_obj != bool_obj,
		"bt_value_copy() returns a different pointer (bool)");
	unsigned_integer_copy_obj = bt_value_array_borrow_element_by_index(
		array_copy_obj, 1);
	ok(unsigned_integer_copy_obj != unsigned_integer_obj,
		"bt_value_copy() returns a different pointer (unsigned integer)");
	signed_integer_copy_obj = bt_value_array_borrow_element_by_index(
		array_copy_obj, 2);
	ok(signed_integer_copy_obj != signed_integer_obj,
		"bt_value_copy() returns a different pointer (signed integer)");
	real_copy_obj = bt_value_array_borrow_element_by_index(
		array_copy_obj, 3);
	ok(real_copy_obj != real_obj,
		"bt_value_copy() returns a different pointer (real)");
	null_copy_obj = bt_value_array_borrow_element_by_index(
		array_copy_obj, 4);
	ok(null_copy_obj == bt_value_null,
		"bt_value_copy() returns the same pointer (null)");

	ok(bt_value_is_equal(map_obj, map_copy_obj),
		"source and destination value objects have the same content");

	BT_VALUE_PUT_REF_AND_RESET(map_copy_obj);
	BT_VALUE_PUT_REF_AND_RESET(bool_obj);
	BT_VALUE_PUT_REF_AND_RESET(unsigned_integer_obj);
	BT_VALUE_PUT_REF_AND_RESET(signed_integer_obj);
	BT_VALUE_PUT_REF_AND_RESET(real_obj);
	BT_VALUE_PUT_REF_AND_RESET(string_obj);
	BT_VALUE_PUT_REF_AND_RESET(array_obj);
	BT_VALUE_PUT_REF_AND_RESET(map_obj);
}

static
bt_bool compare_map_elements(const bt_value *map_a, const bt_value *map_b,
		const char *key)
{
	const bt_value *elem_a = NULL;
	const bt_value *elem_b = NULL;
	bt_bool equal;

	elem_a = bt_value_map_borrow_entry_value_const(map_a, key);
	elem_b = bt_value_map_borrow_entry_value_const(map_b, key);
	equal = bt_value_is_equal(elem_a, elem_b);
	return equal;
}

static
void test_extend(void)
{
	bt_value *base_map = bt_value_map_create();
	bt_value *extension_map = bt_value_map_create();
	bt_value *extended_map = NULL;
	bt_value *array = bt_value_array_create();
	bt_value_map_insert_entry_status insert_status;
	bt_value_copy_status copy_status;
	bt_value_map_extend_status extend_status;

	BT_ASSERT(base_map);
	BT_ASSERT(extension_map);
	BT_ASSERT(array);
	insert_status = bt_value_map_insert_bool_entry(base_map, "file",
		BT_TRUE);
	BT_ASSERT(insert_status == BT_VALUE_MAP_INSERT_ENTRY_STATUS_OK);
	insert_status = bt_value_map_insert_bool_entry(base_map, "edit",
		BT_FALSE);
	BT_ASSERT(insert_status == BT_VALUE_MAP_INSERT_ENTRY_STATUS_OK);
	insert_status = bt_value_map_insert_signed_integer_entry(base_map,
		"selection", 17);
	BT_ASSERT(insert_status == BT_VALUE_MAP_INSERT_ENTRY_STATUS_OK);
	insert_status = bt_value_map_insert_signed_integer_entry(base_map, "find",
		-34);
	BT_ASSERT(insert_status == BT_VALUE_MAP_INSERT_ENTRY_STATUS_OK);
	insert_status = bt_value_map_insert_bool_entry(extension_map, "edit",
		BT_TRUE);
	BT_ASSERT(insert_status == BT_VALUE_MAP_INSERT_ENTRY_STATUS_OK);
	insert_status = bt_value_map_insert_signed_integer_entry(extension_map,
		"find", 101);
	BT_ASSERT(insert_status == BT_VALUE_MAP_INSERT_ENTRY_STATUS_OK);
	insert_status = bt_value_map_insert_real_entry(extension_map,
		"project", -404);
	BT_ASSERT(insert_status == BT_VALUE_MAP_INSERT_ENTRY_STATUS_OK);
	copy_status = bt_value_copy(base_map, &extended_map);
	BT_ASSERT(copy_status == BT_VALUE_COPY_STATUS_OK);
	extend_status = bt_value_map_extend(extended_map, extension_map);
	ok(extend_status == BT_VALUE_MAP_EXTEND_STATUS_OK &&
		extended_map, "bt_value_map_extend() succeeds");
	ok(bt_value_map_get_size(extended_map) == 5,
		"bt_value_map_extend() returns a map object with the correct size");
	ok(compare_map_elements(base_map,
				extended_map, "file"),
		"bt_value_map_extend() picks the appropriate element (file)");
	ok(compare_map_elements(extension_map,
				extended_map, "edit"),
		"bt_value_map_extend() picks the appropriate element (edit)");
	ok(compare_map_elements(base_map,
				extended_map, "selection"),
		"bt_value_map_extend() picks the appropriate element (selection)");
	ok(compare_map_elements(extension_map,
				extended_map, "find"),
		"bt_value_map_extend() picks the appropriate element (find)");
	ok(compare_map_elements(extension_map,
				extended_map, "project"),
		"bt_value_map_extend() picks the appropriate element (project)");

	BT_VALUE_PUT_REF_AND_RESET(array);
	BT_VALUE_PUT_REF_AND_RESET(base_map);
	BT_VALUE_PUT_REF_AND_RESET(extension_map);
	BT_VALUE_PUT_REF_AND_RESET(extended_map);
}

int main(void)
{
	plan_tests(NR_TESTS);
	test_types();
	test_is_equal();
	test_copy();
	test_extend();
	return 0;
}
