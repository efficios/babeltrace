/*
 * attributes.c
 *
 * Babeltrace CTF writer - Attributes
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

#define BT_LOG_TAG "CTF-WRITER-ATTRS"
#include <babeltrace/lib-logging-internal.h>

#include <babeltrace/assert-internal.h>
#include <babeltrace/babeltrace-internal.h>
#include <babeltrace/compat/string-internal.h>
#include <babeltrace/object.h>
#include <babeltrace/values-internal.h>
#include <babeltrace/values.h>
#include <inttypes.h>

#define BT_CTF_ATTR_NAME_INDEX		0
#define BT_CTF_ATTR_VALUE_INDEX		1

BT_HIDDEN
struct bt_value *bt_ctf_attributes_create(void)
{
	struct bt_value *attr_obj;

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
	BT_LOGD_STR("Creating attributes object.");
	attr_obj = bt_value_array_create();
	if (!attr_obj) {
		BT_LOGE_STR("Failed to create array value.");
	} else {
		BT_LOGD("Created attributes object: addr=%p",
			attr_obj);
	}

	return attr_obj;
}

BT_HIDDEN
void bt_ctf_attributes_destroy(struct bt_value *attr_obj)
{
	BT_LOGD("Destroying attributes object: addr=%p", attr_obj);
	bt_object_put_ref(attr_obj);
}

BT_HIDDEN
int64_t bt_ctf_attributes_get_count(struct bt_value *attr_obj)
{
	return bt_value_array_get_size(attr_obj);
}

BT_HIDDEN
const char *bt_ctf_attributes_get_field_name(struct bt_value *attr_obj,
		uint64_t index)
{
	int rc;
	const char *ret = NULL;
	struct bt_value *attr_field_obj = NULL;
	struct bt_value *attr_field_name_obj = NULL;

	if (!attr_obj) {
		BT_LOGW_STR("Invalid parameter: attributes object is NULL.");
		goto end;
	}

	if (index >= bt_value_array_get_size(attr_obj)) {
		BT_LOGW("Invalid parameter: index is out of bounds: "
			"index=%" PRIu64 ", count=%" PRId64,
			index, bt_value_array_get_size(attr_obj));
		goto end;
	}

	attr_field_obj = bt_value_array_borrow_element_by_index(attr_obj, index);
	if (!attr_field_obj) {
		BT_LOGE("Cannot get attributes object's array value's element by index: "
			"value-addr=%p, index=%" PRIu64, attr_obj, index);
		goto end;
	}

	attr_field_name_obj = bt_value_array_borrow_element_by_index(attr_field_obj,
		BT_CTF_ATTR_NAME_INDEX);
	if (!attr_field_name_obj) {
		BT_LOGE("Cannot get attribute array value's element by index: "
			"value-addr=%p, index=%" PRIu64, attr_field_obj,
			(uint64_t) BT_CTF_ATTR_NAME_INDEX);
		goto end;
	}

	rc = bt_value_string_get(attr_field_name_obj, &ret);
	if (rc) {
		BT_LOGE("Cannot get raw value from string value: value-addr=%p",
			attr_field_name_obj);
		ret = NULL;
	}

end:
	return ret;
}

BT_HIDDEN
struct bt_value *bt_ctf_attributes_borrow_field_value(struct bt_value *attr_obj,
		uint64_t index)
{
	struct bt_value *value_obj = NULL;
	struct bt_value *attr_field_obj = NULL;

	if (!attr_obj) {
		BT_LOGW_STR("Invalid parameter: attributes object is NULL.");
		goto end;
	}

	if (index >= bt_value_array_get_size(attr_obj)) {
		BT_LOGW("Invalid parameter: index is out of bounds: "
			"index=%" PRIu64 ", count=%" PRId64,
			index, bt_value_array_get_size(attr_obj));
		goto end;
	}

	attr_field_obj = bt_value_array_borrow_element_by_index(attr_obj, index);
	if (!attr_field_obj) {
		BT_LOGE("Cannot get attributes object's array value's element by index: "
			"value-addr=%p, index=%" PRIu64, attr_obj, index);
		goto end;
	}

	value_obj = bt_value_array_borrow_element_by_index(attr_field_obj,
		BT_CTF_ATTR_VALUE_INDEX);
	if (!value_obj) {
		BT_LOGE("Cannot get attribute array value's element by index: "
			"value-addr=%p, index=%" PRIu64, attr_field_obj,
			(uint64_t) BT_CTF_ATTR_VALUE_INDEX);
	}

end:
	return value_obj;
}

static
struct bt_value *bt_ctf_attributes_borrow_field_by_name(
		struct bt_value *attr_obj, const char *name)
{
	uint64_t i;
	int64_t attr_size;
	struct bt_value *value_obj = NULL;
	struct bt_value *attr_field_name_obj = NULL;

	attr_size = bt_value_array_get_size(attr_obj);
	if (attr_size < 0) {
		BT_LOGE("Cannot get array value's size: value-addr=%p",
			attr_obj);
		goto error;
	}

	for (i = 0; i < attr_size; ++i) {
		int ret;
		const char *field_name;

		value_obj = bt_value_array_borrow_element_by_index(attr_obj, i);
		if (!value_obj) {
			BT_LOGE("Cannot get attributes object's array value's element by index: "
				"value-addr=%p, index=%" PRIu64, attr_obj, i);
			goto error;
		}

		attr_field_name_obj = bt_value_array_borrow_element_by_index(value_obj,
			BT_CTF_ATTR_NAME_INDEX);
		if (!attr_field_name_obj) {
			BT_LOGE("Cannot get attribute array value's element by index: "
				"value-addr=%p, index=%" PRIu64,
				value_obj, (int64_t) BT_CTF_ATTR_NAME_INDEX);
			goto error;
		}

		ret = bt_value_string_get(attr_field_name_obj, &field_name);
		if (ret) {
			BT_LOGE("Cannot get raw value from string value: value-addr=%p",
				attr_field_name_obj);
			goto error;
		}

		if (!strcmp(field_name, name)) {
			break;
		}

		value_obj = NULL;
	}

	return value_obj;

error:
	value_obj = NULL;
	return value_obj;
}

BT_HIDDEN
int bt_ctf_attributes_set_field_value(struct bt_value *attr_obj,
		const char *name, struct bt_value *value_obj)
{
	int ret = 0;
	struct bt_value *attr_field_obj = NULL;

	if (!attr_obj || !name || !value_obj) {
		BT_LOGW("Invalid parameter: attributes object, name, or value object is NULL: "
			"attr-value-addr=%p, name-addr=%p, value-addr=%p",
			attr_obj, name, value_obj);
		ret = -1;
		goto end;
	}

	attr_field_obj = bt_ctf_attributes_borrow_field_by_name(attr_obj, name);
	if (attr_field_obj) {
		ret = bt_value_array_set_element_by_index(attr_field_obj,
			BT_CTF_ATTR_VALUE_INDEX, value_obj);
		attr_field_obj = NULL;
		goto end;
	}

	attr_field_obj = bt_value_array_create();
	if (!attr_field_obj) {
		BT_LOGE_STR("Failed to create empty array value.");
		ret = -1;
		goto end;
	}

	ret = bt_value_array_append_string_element(attr_field_obj, name);
	ret |= bt_value_array_append_element(attr_field_obj, value_obj);
	if (ret) {
		BT_LOGE("Cannot append elements to array value: addr=%p",
			attr_field_obj);
		goto end;
	}

	ret = bt_value_array_append_element(attr_obj, attr_field_obj);
	if (ret) {
		BT_LOGE("Cannot append element to array value: "
			"array-value-addr=%p, element-value-addr=%p",
			attr_obj, attr_field_obj);
	}

end:
	bt_object_put_ref(attr_field_obj);
	return ret;
}

BT_HIDDEN
struct bt_value *bt_ctf_attributes_borrow_field_value_by_name(
		struct bt_value *attr_obj, const char *name)
{
	struct bt_value *value_obj = NULL;
	struct bt_value *attr_field_obj = NULL;

	if (!attr_obj || !name) {
		BT_LOGW("Invalid parameter: attributes object or name is NULL: "
			"value-addr=%p, name-addr=%p", attr_obj, name);
		goto end;
	}

	attr_field_obj = bt_ctf_attributes_borrow_field_by_name(attr_obj, name);
	if (!attr_field_obj) {
		BT_LOGD("Cannot find attributes object's field by name: "
			"value-addr=%p, name=\"%s\"", attr_obj, name);
		goto end;
	}

	value_obj = bt_value_array_borrow_element_by_index(attr_field_obj,
		BT_CTF_ATTR_VALUE_INDEX);
	if (!value_obj) {
		BT_LOGE("Cannot get attribute array value's element by index: "
			"value-addr=%p, index=%" PRIu64, attr_field_obj,
			(uint64_t) BT_CTF_ATTR_VALUE_INDEX);
	}

end:
	return value_obj;
}

BT_HIDDEN
int bt_ctf_attributes_freeze(struct bt_value *attr_obj)
{
	uint64_t i;
	int64_t count;
	int ret = 0;

	if (!attr_obj) {
		BT_LOGW_STR("Invalid parameter: attributes object is NULL.");
		ret = -1;
		goto end;
	}

	BT_LOGD("Freezing attributes object: value-addr=%p", attr_obj);
	count = bt_value_array_get_size(attr_obj);
	BT_ASSERT(count >= 0);

	/*
	 * We do not freeze the array value object itself here, since
	 * internal stuff could need to modify/add attributes. Each
	 * attribute is frozen one by one.
	 */
	for (i = 0; i < count; ++i) {
		struct bt_value *obj = NULL;

		obj = bt_ctf_attributes_borrow_field_value(attr_obj, i);
		if (!obj) {
			BT_LOGE("Cannot get attributes object's field value by index: "
				"value-addr=%p, index=%" PRIu64,
				attr_obj, i);
			ret = -1;
			goto end;
		}

		bt_value_freeze(obj);
	}

end:
	return ret;
}
