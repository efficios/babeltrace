/*
 * utils.c
 *
 * Babeltrace CTF IR - Utilities
 *
 * Copyright 2015 Jérémie Galarneau <jeremie.galarneau@efficios.com>
 *
 * Author: Jérémie Galarneau <jeremie.galarneau@efficios.com>
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

#define BT_LOG_TAG "CTF-IR-UTILS"
#include <babeltrace/lib-logging-internal.h>

#include <assert.h>
#include <stdlib.h>
#include <glib.h>
#include <babeltrace/ctf-ir/utils.h>
#include <babeltrace/ctf-ir/field-types.h>
#include <babeltrace/ctf-ir/clock-class.h>
#include <babeltrace/ref.h>

static
const char * const reserved_keywords_str[] = {"align", "callsite",
	"const", "char", "clock", "double", "enum", "env", "event",
	"floating_point", "float", "integer", "int", "long", "short", "signed",
	"stream", "string", "struct", "trace", "typealias", "typedef",
	"unsigned", "variant", "void" "_Bool", "_Complex", "_Imaginary"};

static GHashTable *reserved_keywords_set;
static int init_done;

static
void try_init_reserved_keywords(void)
{
	size_t i;
	const size_t reserved_keywords_count =
		sizeof(reserved_keywords_str) / sizeof(char *);

	if (reserved_keywords_set) {
		return;
	}

	reserved_keywords_set = g_hash_table_new(g_direct_hash, g_direct_equal);
	assert(reserved_keywords_set);

	for (i = 0; i < reserved_keywords_count; i++) {
		gpointer quark = GINT_TO_POINTER(g_quark_from_string(
			reserved_keywords_str[i]));

		g_hash_table_insert(reserved_keywords_set, quark, quark);
	}

	init_done = 1;
}

static __attribute__((destructor))
void trace_finalize(void)
{
	if (reserved_keywords_set) {
		g_hash_table_destroy(reserved_keywords_set);
	}
}

int bt_validate_identifier(const char *input_string)
{
	int ret = 0;
	char *string = NULL;
	char *save_ptr, *token;

	if (!input_string) {
		BT_LOGV_STR("Invalid parameter: input string is NULL.");
		ret = -1;
		goto end;
	}

	try_init_reserved_keywords();

	if (input_string[0] == '\0') {
		ret = -1;
		goto end;
	}

	string = strdup(input_string);
	if (!string) {
		BT_LOGE("strdup() failed.");
		ret = -1;
		goto end;
	}

	token = strtok_r(string, " ", &save_ptr);
	while (token) {
		if (g_hash_table_lookup_extended(reserved_keywords_set,
			GINT_TO_POINTER(g_quark_from_string(token)),
			NULL, NULL)) {
			ret = -1;
			goto end;
		}

		token = strtok_r(NULL, " ", &save_ptr);
	}
end:
	free(string);
	return ret;
}

bt_bool bt_identifier_is_valid(const char *identifier)
{
	return bt_validate_identifier(identifier) == 0;
}

BT_HIDDEN
int bt_validate_single_clock_class(struct bt_field_type *field_type,
		struct bt_clock_class **expected_clock_class)
{
	int ret = 0;

	if (!field_type) {
		goto end;
	}

	assert(expected_clock_class);

	switch (bt_field_type_get_type_id(field_type)) {
	case BT_FIELD_TYPE_ID_INTEGER:
	{
		struct bt_clock_class *mapped_clock_class =
			bt_field_type_integer_get_mapped_clock_class(field_type);

		if (!mapped_clock_class) {
			goto end;
		}

		if (!*expected_clock_class) {
			/* Move reference to output parameter */
			*expected_clock_class = mapped_clock_class;
			mapped_clock_class = NULL;
			BT_LOGV("Setting expected clock class: "
				"expected-clock-class-addr=%p",
				*expected_clock_class);
		} else {
			if (mapped_clock_class != *expected_clock_class) {
				BT_LOGW("Integer field type is not mapped to "
					"the expected clock class: "
					"mapped-clock-class-addr=%p, "
					"mapped-clock-class-name=\"%s\", "
					"expected-clock-class-addr=%p, "
					"expected-clock-class-name=\"%s\"",
					mapped_clock_class,
					bt_clock_class_get_name(mapped_clock_class),
					*expected_clock_class,
					bt_clock_class_get_name(*expected_clock_class));
				bt_put(mapped_clock_class);
				ret = -1;
				goto end;
			}
		}

		bt_put(mapped_clock_class);
		break;
	}
	case BT_FIELD_TYPE_ID_ENUM:
	case BT_FIELD_TYPE_ID_ARRAY:
	case BT_FIELD_TYPE_ID_SEQUENCE:
	{
		struct bt_field_type *subtype = NULL;

		switch (bt_field_type_get_type_id(field_type)) {
		case BT_FIELD_TYPE_ID_ENUM:
			subtype = bt_field_type_enumeration_get_container_type(
				field_type);
			break;
		case BT_FIELD_TYPE_ID_ARRAY:
			subtype = bt_field_type_array_get_element_type(
				field_type);
			break;
		case BT_FIELD_TYPE_ID_SEQUENCE:
			subtype = bt_field_type_sequence_get_element_type(
				field_type);
			break;
		default:
			BT_LOGF("Unexpected field type ID: id=%d",
				bt_field_type_get_type_id(field_type));
			abort();
		}

		assert(subtype);
		ret = bt_validate_single_clock_class(subtype,
			expected_clock_class);
		bt_put(subtype);
		break;
	}
	case BT_FIELD_TYPE_ID_STRUCT:
	{
		uint64_t i;
		int64_t count = bt_field_type_structure_get_field_count(
			field_type);

		for (i = 0; i < count; i++) {
			const char *name;
			struct bt_field_type *member_type;

			ret = bt_field_type_structure_get_field_by_index(
				field_type, &name, &member_type, i);
			assert(ret == 0);
			ret = bt_validate_single_clock_class(member_type,
				expected_clock_class);
			bt_put(member_type);
			if (ret) {
				BT_LOGW("Structure field type's field's type "
					"is not recursively mapped to the "
					"expected clock class: "
					"field-ft-addr=%p, field-name=\"%s\"",
					member_type, name);
				goto end;
			}
		}

		break;
	}
	case BT_FIELD_TYPE_ID_VARIANT:
	{
		uint64_t i;
		int64_t count = bt_field_type_variant_get_field_count(
			field_type);

		for (i = 0; i < count; i++) {
			const char *name;
			struct bt_field_type *member_type;

			ret = bt_field_type_variant_get_field_by_index(
				field_type, &name, &member_type, i);
			assert(ret == 0);
			ret = bt_validate_single_clock_class(member_type,
				expected_clock_class);
			bt_put(member_type);
			if (ret) {
				BT_LOGW("Variant field type's field's type "
					"is not recursively mapped to the "
					"expected clock class: "
					"field-ft-addr=%p, field-name=\"%s\"",
					member_type, name);
				goto end;
			}
		}

		break;
	}
	default:
		break;
	}

end:
	return ret;
}
