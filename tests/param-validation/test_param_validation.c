/*
 * Copyright (c) EfficiOS Inc.
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

#include "tap/tap.h"
#include "param-parse/param-parse.h"
#include "plugins/common/param-validation/param-validation.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static
enum bt_param_validation_status run_test(
		const char *params_str,
		const struct bt_param_validation_map_value_entry_descr *entries,
		const char *test_name,
		const char *expected_error)
{
	GString *err = g_string_new(NULL);
	const bt_value *params;
	enum bt_param_validation_status status;
	gchar *validate_error = NULL;

	if (!err) {
		fprintf(stderr, "Failed to allocated a GString.\n");
		abort();
	}

	params = bt_param_parse(params_str, err);

	if (!params) {
		fprintf(stderr, "Could not parse params: `%s`, %s\n",
			params_str, err->str);
		abort();
	}

	status = bt_param_validation_validate(params, entries, &validate_error);

	if (expected_error) {
		const char *fmt;

		/* We expect a failure. */
		ok(status == BT_PARAM_VALIDATION_STATUS_VALIDATION_ERROR,
			"%s: validation fails", test_name);
		ok(validate_error, "%s: error string is not NULL", test_name);

		fmt = "%s: error string contains expected string";
		if (validate_error && strstr(validate_error, expected_error)) {
			pass(fmt, test_name);
		} else {
			fail(fmt, test_name);
			diag("could not find `%s` in `%s`", expected_error, validate_error);
		}

		g_free(validate_error);
	} else {
		/* We expect a success. */
		ok(status == BT_PARAM_VALIDATION_STATUS_OK, "%s: validation succeeds", test_name);
		ok(!validate_error, "%s: error string is NULL", test_name);
	}

	bt_value_put_ref(params);
	g_string_free(err, TRUE);

	return status;
}

static
void test_map_valid(void)
{
	const struct bt_param_validation_map_value_entry_descr entries[] = {
		{ "carotte", BT_PARAM_VALIDATION_MAP_VALUE_ENTRY_MANDATORY, { .type = BT_VALUE_TYPE_SIGNED_INTEGER } },
		{ "fenouil", BT_PARAM_VALIDATION_MAP_VALUE_ENTRY_OPTIONAL, { .type = BT_VALUE_TYPE_STRING } },
		{ "panais", BT_PARAM_VALIDATION_MAP_VALUE_ENTRY_OPTIONAL, { .type =  BT_VALUE_TYPE_BOOL } },
		BT_PARAM_VALIDATION_MAP_VALUE_ENTRY_END
	};

	run_test("carotte=2,fenouil=\"miam\"", entries, "valid map", NULL);
}

static
void test_map_missing_key(void)
{
	const struct bt_param_validation_map_value_entry_descr entries[] = {
		{ "carotte", BT_PARAM_VALIDATION_MAP_VALUE_ENTRY_MANDATORY, { .type = BT_VALUE_TYPE_SIGNED_INTEGER } },
		{ "tomate", BT_PARAM_VALIDATION_MAP_VALUE_ENTRY_MANDATORY, { .type = BT_VALUE_TYPE_SIGNED_INTEGER } },
		BT_PARAM_VALIDATION_MAP_VALUE_ENTRY_END
	};

	run_test("carotte=2", entries, "missing key in map",
		"Error validating parameters: missing mandatory entry `tomate`");
}

static
void test_map_unexpected_key(void)
{
	const struct bt_param_validation_map_value_entry_descr entries[] = {
		{ "carotte", BT_PARAM_VALIDATION_MAP_VALUE_ENTRY_MANDATORY, { .type = BT_VALUE_TYPE_SIGNED_INTEGER } },
		BT_PARAM_VALIDATION_MAP_VALUE_ENTRY_END
	};

	run_test("tomate=2", entries, "unexpected key in map", "unexpected key `tomate`");
}

static
void test_map_invalid_entry_value_type(void)
{
	const struct bt_param_validation_map_value_entry_descr entries[] = {
		{ "carottes", BT_PARAM_VALIDATION_MAP_VALUE_ENTRY_MANDATORY, { .type = BT_VALUE_TYPE_SIGNED_INTEGER } },
		BT_PARAM_VALIDATION_MAP_VALUE_ENTRY_END
	};

	run_test("carottes=\"orange\"", entries, "map entry with unexpected type",
		"Error validating parameter `carottes`: unexpected type: expected-type=SIGNED_INTEGER, actual-type=STRING");
}

static
void test_nested_error(void)
{
	const struct bt_param_validation_map_value_entry_descr poireau_entries[] = {
		{ "navet", BT_PARAM_VALIDATION_MAP_VALUE_ENTRY_MANDATORY, { .type = BT_VALUE_TYPE_SIGNED_INTEGER } },
		BT_PARAM_VALIDATION_MAP_VALUE_ENTRY_END,
	};

	const struct bt_param_validation_map_value_entry_descr carottes_elem_entries[] = {
		{ "poireau", BT_PARAM_VALIDATION_MAP_VALUE_ENTRY_MANDATORY, { BT_VALUE_TYPE_MAP, .map = {
			.entries = poireau_entries,
		} } },
		BT_PARAM_VALIDATION_MAP_VALUE_ENTRY_END,
	};

	const struct bt_param_validation_value_descr carottes_elem = {
		.type = BT_VALUE_TYPE_MAP,
		.map = {
			.entries = carottes_elem_entries,
		}
	};

	const struct bt_param_validation_map_value_entry_descr entries[] = {
		{ "carottes", BT_PARAM_VALIDATION_MAP_VALUE_ENTRY_MANDATORY, { BT_VALUE_TYPE_ARRAY, .array = {
			.min_length = 0,
			.max_length = BT_PARAM_VALIDATION_INFINITE,
			.element_type = &carottes_elem,
		} } },
		BT_PARAM_VALIDATION_MAP_VALUE_ENTRY_END
	};

	run_test("carottes=[{poireau={navet=7}}, {poireau={}}]", entries, "error nested in maps and arrays",
		"Error validating parameter `carottes[1].poireau`: missing mandatory entry `navet`");
}

static
void test_array_valid(void)
{
	const struct bt_param_validation_value_descr carotte_elem = { .type = BT_VALUE_TYPE_BOOL, {} };

	const struct bt_param_validation_map_value_entry_descr entries[] = {
		{ "carotte", BT_PARAM_VALIDATION_MAP_VALUE_ENTRY_MANDATORY, { BT_VALUE_TYPE_ARRAY, .array = {
			.min_length = 2,
			.max_length = 22,
			.element_type = &carotte_elem,
		} } },
		BT_PARAM_VALIDATION_MAP_VALUE_ENTRY_END
	};

	run_test("carotte=[true, false, true]", entries, "valid array", NULL);
}

static
void test_array_empty_valid(void)
{
	const struct bt_param_validation_value_descr carotte_elem = { .type = BT_VALUE_TYPE_BOOL, {} };

	const struct bt_param_validation_map_value_entry_descr entries[] = {
		{ "carotte", BT_PARAM_VALIDATION_MAP_VALUE_ENTRY_MANDATORY, { BT_VALUE_TYPE_ARRAY, .array = {
			.min_length = 0,
			.max_length = 2,
			.element_type = &carotte_elem,
		} } },
		BT_PARAM_VALIDATION_MAP_VALUE_ENTRY_END
	};

	run_test("carotte=[]", entries, "valid empty array", NULL);
}

static
void test_array_invalid_too_small(void)
{
	const struct bt_param_validation_value_descr carotte_elem = { .type = BT_VALUE_TYPE_BOOL, {} };

	const struct bt_param_validation_map_value_entry_descr entries[] = {
		{ "carotte", BT_PARAM_VALIDATION_MAP_VALUE_ENTRY_MANDATORY, { BT_VALUE_TYPE_ARRAY, .array = {
			.min_length = 1,
			.max_length = 100,
			.element_type = &carotte_elem,
		} } },
		BT_PARAM_VALIDATION_MAP_VALUE_ENTRY_END
	};

	run_test("carotte=[]", entries, "array too small",
		"Error validating parameter `carotte`: array is smaller than the minimum length: array-length=0, min-length=1");
}

static
void test_array_invalid_too_large(void)
{
	const struct bt_param_validation_value_descr carotte_elem = { .type = BT_VALUE_TYPE_BOOL, {} };

	const struct bt_param_validation_map_value_entry_descr entries[] = {
		{ "carotte", BT_PARAM_VALIDATION_MAP_VALUE_ENTRY_MANDATORY, { BT_VALUE_TYPE_ARRAY, .array = {
			.min_length = 2,
			.max_length = 2,
			.element_type = &carotte_elem,
		} } },
		BT_PARAM_VALIDATION_MAP_VALUE_ENTRY_END
	};

	run_test("carotte=[true, false, false]", entries, "array too large",
		"Error validating parameter `carotte`: array is larger than the maximum length: array-length=3, max-length=2");
}

static
void test_array_invalid_elem_type(void)
{
	const struct bt_param_validation_value_descr carotte_elem = { .type = BT_VALUE_TYPE_BOOL, {} };

	const struct bt_param_validation_map_value_entry_descr entries[] = {
		{ "carotte", BT_PARAM_VALIDATION_MAP_VALUE_ENTRY_MANDATORY, { BT_VALUE_TYPE_ARRAY, .array = {
			.min_length = 3,
			.max_length = 3,
			.element_type = &carotte_elem,
		} } },
		BT_PARAM_VALIDATION_MAP_VALUE_ENTRY_END
	};

	run_test("carotte=[true, false, 2]", entries, "array with invalid element type",
		"Error validating parameter `carotte[2]`: unexpected type: expected-type=BOOL, actual-type=SIGNED_INTEGER");
}

static
void test_string_valid_without_choices(void)
{
	const struct bt_param_validation_map_value_entry_descr entries[] = {
		{ "haricot", BT_PARAM_VALIDATION_MAP_VALUE_ENTRY_MANDATORY, { .type = BT_VALUE_TYPE_STRING, { } } },
		BT_PARAM_VALIDATION_MAP_VALUE_ENTRY_END
	};

	run_test("haricot=\"vert\"", entries, "valid string without choices", NULL);
}

static
void test_string_valid_with_choices(void)
{
	const char *haricot_choices[] = {"vert", "jaune", "rouge", NULL};
	const struct bt_param_validation_map_value_entry_descr entries[] = {
		{ "haricot", BT_PARAM_VALIDATION_MAP_VALUE_ENTRY_MANDATORY, { BT_VALUE_TYPE_STRING, .string = {
			.choices = haricot_choices,
		} } },
		BT_PARAM_VALIDATION_MAP_VALUE_ENTRY_END
	};

	run_test("haricot=\"jaune\"", entries, "valid string with choices", NULL);
}

static
void test_string_invalid_choice(void)
{
	const char *haricot_choices[] = {"vert", "jaune", "rouge", NULL};
	const struct bt_param_validation_map_value_entry_descr entries[] = {
		{ "haricot", BT_PARAM_VALIDATION_MAP_VALUE_ENTRY_MANDATORY, { BT_VALUE_TYPE_STRING, .string = {
			.choices = haricot_choices,
		} } },
		BT_PARAM_VALIDATION_MAP_VALUE_ENTRY_END
	};

	run_test("haricot=\"violet\"", entries, "string with invalid choice",
		"Error validating parameter `haricot`: string is not amongst the available choices: string=violet, choices=[vert, jaune, rouge]");
}

static
enum bt_param_validation_status custom_validation_func_valid(
		const bt_value *value,
		struct bt_param_validation_context *context)
{
	ok(bt_value_get_type(value) == BT_VALUE_TYPE_UNSIGNED_INTEGER,
		"type of value passed to custom function is as expected");
	ok(bt_value_integer_unsigned_get(value) == 1234,
		"value passed to custom function is as expected");
	return BT_PARAM_VALIDATION_STATUS_OK;
}

static
void test_custom_validation_func_valid(void)
{
	const struct bt_param_validation_map_value_entry_descr entries[] = {
		{ "navet", BT_PARAM_VALIDATION_MAP_VALUE_ENTRY_MANDATORY, {
			.validation_func = custom_validation_func_valid,
		} },
		BT_PARAM_VALIDATION_MAP_VALUE_ENTRY_END
	};

	run_test("navet=+1234", entries, "custom validation function with valid value", NULL);
}

static
enum bt_param_validation_status custom_validation_func_invalid(
		const bt_value *value,
		struct bt_param_validation_context *context)
{
	return bt_param_validation_error(context, "wrooooong");
}

static
void test_custom_validation_func_invalid(void)
{
	const struct bt_param_validation_map_value_entry_descr entries[] = {
		{ "navet", BT_PARAM_VALIDATION_MAP_VALUE_ENTRY_MANDATORY, {
			.validation_func = custom_validation_func_invalid,
		} },
		BT_PARAM_VALIDATION_MAP_VALUE_ENTRY_END
	};

	run_test("navet=+1234", entries, "custom validation function with invalid value",
		"Error validating parameter `navet`: wrooooong");
}

int main(void)
{
	plan_tests(41);

	test_map_valid();

	test_map_missing_key();
	test_map_unexpected_key();
	test_map_invalid_entry_value_type();

	test_array_valid();
	test_array_empty_valid();

	test_array_invalid_too_small();
	test_array_invalid_too_large();
	test_array_invalid_elem_type();

	test_string_valid_without_choices();
	test_string_valid_with_choices();

	test_string_invalid_choice();

	test_custom_validation_func_valid();
	test_custom_validation_func_invalid();

	test_nested_error();

	return exit_status();
}
