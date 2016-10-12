/*
 * attributes.c
 *
 * Babeltrace CTF IR - Attributes
 *
 * Copyright (c) 2015 EfficiOS Inc. and Linux Foundation
 * Copyright (c) 2015 Philippe Proulx <pproulx@efficios.com>
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
#include <babeltrace/values.h>

#define BT_CTF_ATTR_NAME_INDEX		0
#define BT_CTF_ATTR_VALUE_INDEX		1

BT_HIDDEN
struct bt_value *bt_ctf_attributes_create(void)
{
	/*
	 * Attributes: array value object of array value objects, each one
	 * containing two entries: a string value object (attributes
	 * field name), and a value object (attributes field value).
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
	return bt_value_array_create();
}

BT_HIDDEN
void bt_ctf_attributes_destroy(struct bt_value *attr_obj)
{
	bt_put(attr_obj);
}

BT_HIDDEN
int bt_ctf_attributes_get_count(struct bt_value *attr_obj)
{
	return bt_value_array_size(attr_obj);
}

BT_HIDDEN
const char *bt_ctf_attributes_get_field_name(struct bt_value *attr_obj,
		int index)
{
	int rc;
	const char *ret = NULL;
	struct bt_value *attr_field_obj = NULL;
	struct bt_value *attr_field_name_obj = NULL;

	if (!attr_obj || index < 0) {
		goto end;
	}

	attr_field_obj = bt_value_array_get(attr_obj, index);

	if (!attr_field_obj) {
		goto end;
	}

	attr_field_name_obj = bt_value_array_get(attr_field_obj,
		BT_CTF_ATTR_NAME_INDEX);

	if (!attr_field_name_obj) {
		goto end;
	}

	rc = bt_value_string_get(attr_field_name_obj, &ret);

	if (rc) {
		ret = NULL;
	}

end:
	BT_PUT(attr_field_name_obj);
	BT_PUT(attr_field_obj);
	return ret;
}

BT_HIDDEN
struct bt_value *bt_ctf_attributes_get_field_value(struct bt_value *attr_obj,
		int index)
{
	struct bt_value *value_obj = NULL;
	struct bt_value *attr_field_obj = NULL;

	if (!attr_obj || index < 0) {
		goto end;
	}

	attr_field_obj = bt_value_array_get(attr_obj, index);

	if (!attr_field_obj) {
		goto end;
	}

	value_obj = bt_value_array_get(attr_field_obj,
		BT_CTF_ATTR_VALUE_INDEX);

end:
	BT_PUT(attr_field_obj);
	return value_obj;
}

static
struct bt_value *bt_ctf_attributes_get_field_by_name(
		struct bt_value *attr_obj, const char *name)
{
	int i;
	int attr_size;
	struct bt_value *value_obj = NULL;
	struct bt_value *attr_field_name_obj = NULL;

	attr_size = bt_value_array_size(attr_obj);

	if (attr_size < 0) {
		goto error;
	}

	for (i = 0; i < attr_size; ++i) {
		int ret;
		const char *field_name;

		value_obj = bt_value_array_get(attr_obj, i);

		if (!value_obj) {
			goto error;
		}

		attr_field_name_obj = bt_value_array_get(value_obj, 0);

		if (!attr_field_name_obj) {
			goto error;
		}

		ret = bt_value_string_get(attr_field_name_obj, &field_name);
		if (ret) {
			goto error;
		}

		if (!strcmp(field_name, name)) {
			BT_PUT(attr_field_name_obj);
			break;
		}

		BT_PUT(attr_field_name_obj);
		BT_PUT(value_obj);
	}

	return value_obj;

error:
	BT_PUT(attr_field_name_obj);
	BT_PUT(value_obj);

	return value_obj;
}

BT_HIDDEN
int bt_ctf_attributes_set_field_value(struct bt_value *attr_obj,
		const char *name, struct bt_value *value_obj)
{
	int ret = 0;
	struct bt_value *attr_field_obj = NULL;

	if (!attr_obj || !name || !value_obj) {
		ret = -1;
		goto end;
	}

	attr_field_obj = bt_ctf_attributes_get_field_by_name(attr_obj, name);

	if (attr_field_obj) {
		ret = bt_value_array_set(attr_field_obj,
			BT_CTF_ATTR_VALUE_INDEX, value_obj);
		goto end;
	}

	attr_field_obj = bt_value_array_create();

	if (!attr_field_obj) {
		ret = -1;
		goto end;
	}

	ret = bt_value_array_append_string(attr_field_obj, name);
	ret |= bt_value_array_append(attr_field_obj, value_obj);

	if (ret) {
		goto end;
	}

	ret = bt_value_array_append(attr_obj, attr_field_obj);

end:
	BT_PUT(attr_field_obj);

	return ret;
}

BT_HIDDEN
struct bt_value *bt_ctf_attributes_get_field_value_by_name(
		struct bt_value *attr_obj, const char *name)
{
	struct bt_value *value_obj = NULL;
	struct bt_value *attr_field_obj = NULL;

	if (!attr_obj || !name) {
		goto end;
	}

	attr_field_obj = bt_ctf_attributes_get_field_by_name(attr_obj, name);

	if (!attr_field_obj) {
		goto end;
	}

	value_obj = bt_value_array_get(attr_field_obj,
		BT_CTF_ATTR_VALUE_INDEX);

end:
	BT_PUT(attr_field_obj);

	return value_obj;
}

BT_HIDDEN
int bt_ctf_attributes_freeze(struct bt_value *attr_obj)
{
	int i;
	int count;
	int ret = 0;

	if (!attr_obj) {
		ret = -1;
		goto end;
	}

	count = bt_value_array_size(attr_obj);

	if (count < 0) {
		ret = -1;
		goto end;
	}

	/*
	 * We do not freeze the array value object itself here, since
	 * internal stuff could need to modify/add attributes. Each
	 * attribute is frozen one by one.
	 */
	for (i = 0; i < count; ++i) {
		struct bt_value *obj = NULL;

		obj = bt_ctf_attributes_get_field_value(attr_obj, i);

		if (!obj) {
			ret = -1;
			goto end;
		}

		bt_value_freeze(obj);
		BT_PUT(obj);
	}

end:
	return ret;
}
