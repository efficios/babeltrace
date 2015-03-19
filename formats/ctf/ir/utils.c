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

#include <string.h>
#include <stdlib.h>
#include <glib.h>
#include <babeltrace/babeltrace-internal.h>
#include <babeltrace/objects.h>

#define BT_CTF_ATTR_NAME_INDEX		0
#define BT_CTF_ATTR_VALUE_INDEX		1

static
const char * const reserved_keywords_str[] = {"align", "callsite",
	"const", "char", "clock", "double", "enum", "env", "event",
	"floating_point", "float", "integer", "int", "long", "short", "signed",
	"stream", "string", "struct", "trace", "typealias", "typedef",
	"unsigned", "variant", "void" "_Bool", "_Complex", "_Imaginary"};

static GHashTable *reserved_keywords_set;
static int init_done;
static int global_data_refcount;

static __attribute__((constructor))
void trace_init(void)
{
	size_t i;
	const size_t reserved_keywords_count =
		sizeof(reserved_keywords_str) / sizeof(char *);

	global_data_refcount++;
	if (init_done) {
		return;
	}

	reserved_keywords_set = g_hash_table_new(g_direct_hash, g_direct_equal);
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
	if (--global_data_refcount == 0) {
		g_hash_table_destroy(reserved_keywords_set);
	}
}

int bt_ctf_validate_identifier(const char *input_string)
{
	int ret = 0;
	char *string = NULL;
	char *save_ptr, *token;

	if (!input_string || input_string[0] == '\0') {
		ret = -1;
		goto end;
	}

	string = strdup(input_string);
	if (!string) {
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

BT_HIDDEN
struct bt_object *bt_ctf_attributes_create(void)
{
	/*
	 * Attributes: array object of array objects, each one
	 * containing two entries: a string object (attributes field
	 * name), and an object (attributes field value).
	 *
	 * Example (JSON representation):
	 *
	 *     [
	 *         ["hostname", "eeppdesk"],
	 *         ["sysname", "Linux"],
	 *         ["tracer_major", 2],
	 *         ["tracer_minor", 5]
	 *     ]
	 */
	return bt_object_array_create();
}

BT_HIDDEN
void bt_ctf_attributes_destroy(struct bt_object *attr_obj)
{
	bt_object_put(attr_obj);
}

BT_HIDDEN
int bt_ctf_attributes_get_count(struct bt_object *attr_obj)
{
	return bt_object_array_size(attr_obj);
}

BT_HIDDEN
const char *bt_ctf_attributes_get_field_name(struct bt_object *attr_obj,
		int index)
{
	int rc;
	const char *ret = NULL;
	struct bt_object *attr_field_obj = NULL;
	struct bt_object *attr_field_name_obj = NULL;

	if (!attr_obj || index < 0) {
		goto end;
	}

	attr_field_obj = bt_object_array_get(attr_obj, index);

	if (!attr_field_obj) {
		goto end;
	}

	attr_field_name_obj = bt_object_array_get(attr_field_obj,
		BT_CTF_ATTR_NAME_INDEX);

	if (!attr_field_name_obj) {
		goto end;
	}

	rc = bt_object_string_get(attr_field_name_obj, &ret);

	if (rc) {
		ret = NULL;
	}

end:
	BT_OBJECT_PUT(attr_field_name_obj);
	BT_OBJECT_PUT(attr_field_obj);

	return ret;
}

BT_HIDDEN
struct bt_object *bt_ctf_attributes_get_field_value(struct bt_object *attr_obj,
		int index)
{
	struct bt_object *value_obj = NULL;
	struct bt_object *attr_field_obj = NULL;

	if (!attr_obj || index < 0) {
		goto end;
	}

	attr_field_obj = bt_object_array_get(attr_obj, index);

	if (!attr_field_obj) {
		goto end;
	}

	value_obj = bt_object_array_get(attr_field_obj,
		BT_CTF_ATTR_VALUE_INDEX);

end:
	BT_OBJECT_PUT(attr_field_obj);

	return value_obj;
}

static
struct bt_object *bt_ctf_attributes_get_field_by_name(
		struct bt_object *attr_obj, const char *name)
{
	int i;
	int attr_size;
	struct bt_object *value_obj = NULL;
	struct bt_object *attr_field_name_obj = NULL;

	attr_size = bt_object_array_size(attr_obj);

	if (attr_size < 0) {
		goto error;
	}

	for (i = 0; i < attr_size; ++i) {
		int ret;
		const char *field_name;

		value_obj = bt_object_array_get(attr_obj, i);

		if (!value_obj) {
			goto error;
		}

		attr_field_name_obj = bt_object_array_get(value_obj, 0);

		if (!attr_field_name_obj) {
			goto error;
		}

		ret = bt_object_string_get(attr_field_name_obj, &field_name);
		if (ret) {
			goto error;
		}

		if (!strcmp(field_name, name)) {
			BT_OBJECT_PUT(attr_field_name_obj);
			break;
		}

		BT_OBJECT_PUT(attr_field_name_obj);
		BT_OBJECT_PUT(value_obj);
	}

	return value_obj;

error:
	BT_OBJECT_PUT(attr_field_name_obj);
	BT_OBJECT_PUT(value_obj);

	return value_obj;
}

BT_HIDDEN
int bt_ctf_attributes_set_field_value(struct bt_object *attr_obj,
		const char *name, struct bt_object *value_obj)
{
	int ret = 0;
	struct bt_object *attr_field_obj = NULL;

	if (!attr_obj || !name || !value_obj) {
		ret = -1;
		goto end;
	}

	attr_field_obj = bt_ctf_attributes_get_field_by_name(attr_obj, name);

	if (attr_field_obj) {
		ret = bt_object_array_set(attr_field_obj,
			BT_CTF_ATTR_VALUE_INDEX, value_obj);
		goto end;
	}

	attr_field_obj = bt_object_array_create();

	if (!attr_field_obj) {
		ret = -1;
		goto end;
	}

	ret = bt_object_array_append_string(attr_field_obj, name);
	ret |= bt_object_array_append(attr_field_obj, value_obj);

	if (ret) {
		goto end;
	}

	ret = bt_object_array_append(attr_obj, attr_field_obj);

end:
	BT_OBJECT_PUT(attr_field_obj);

	return ret;
}

BT_HIDDEN
struct bt_object *bt_ctf_attributes_get_field_value_by_name(
		struct bt_object *attr_obj, const char *name)
{
	struct bt_object *value_obj = NULL;
	struct bt_object *attr_field_obj = NULL;

	if (!attr_obj || !name) {
		goto end;
	}

	attr_field_obj = bt_ctf_attributes_get_field_by_name(attr_obj, name);

	if (!attr_field_obj) {
		goto end;
	}

	value_obj = bt_object_array_get(attr_field_obj,
		BT_CTF_ATTR_VALUE_INDEX);

end:
	BT_OBJECT_PUT(attr_field_obj);

	return value_obj;
}

BT_HIDDEN
int bt_ctf_attributes_freeze(struct bt_object *attr_obj)
{
	int i;
	int count;
	int ret = 0;

	if (!attr_obj) {
		ret = -1;
		goto end;
	}

	count = bt_object_array_size(attr_obj);

	if (count < 0) {
		ret = -1;
		goto end;
	}

	/*
	 * We do not freeze the array itself here, since internal
	 * stuff could need to modify/add attributes. Each attribute
	 * is frozen one by one.
	 */
	for (i = 0; i < count; ++i) {
		struct bt_object *obj = NULL;

		obj = bt_ctf_attributes_get_field_value(attr_obj, i);

		if (!obj) {
			ret = -1;
			goto end;
		}

		bt_object_freeze(obj);
		BT_OBJECT_PUT(obj);
	}

end:

	return ret;
}
