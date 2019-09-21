/*
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

#define BT_LOG_TAG "LIB/ATTRS"
#include "lib/logging.h"

#include "common/macros.h"
#include <babeltrace2/value.h>
#include "lib/assert-pre.h"
#include "lib/object.h"
#include <babeltrace2/value.h>
#include "lib/value.h"
#include "attributes.h"
#include <inttypes.h>
#include "compat/string.h"
#include "common/assert.h"

#define BT_ATTR_NAME_INDEX		0
#define BT_ATTR_VALUE_INDEX		1

BT_HIDDEN
struct bt_value *bt_attributes_create(void)
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
		BT_LIB_LOGE_APPEND_CAUSE("Failed to create array value.");
	} else {
		BT_LOGD("Created attributes object: addr=%p",
			attr_obj);
	}

	return attr_obj;
}

BT_HIDDEN
void bt_attributes_destroy(struct bt_value *attr_obj)
{
	BT_LOGD("Destroying attributes object: addr=%p", attr_obj);
	BT_OBJECT_PUT_REF_AND_RESET(attr_obj);
}

BT_HIDDEN
uint64_t bt_attributes_get_count(const struct bt_value *attr_obj)
{
	return bt_value_array_get_length(attr_obj);
}

BT_HIDDEN
const char *bt_attributes_get_field_name(const struct bt_value *attr_obj,
		uint64_t index)
{
	const struct bt_value *attr_field_obj = NULL;
	const struct bt_value *attr_field_name_obj = NULL;

	BT_ASSERT_DBG(attr_obj);
	BT_ASSERT_DBG(index < bt_value_array_get_length(attr_obj));
	attr_field_obj = bt_value_array_borrow_element_by_index_const(
		attr_obj, index);

	attr_field_name_obj =
		bt_value_array_borrow_element_by_index_const(attr_field_obj,
			BT_ATTR_NAME_INDEX);

	return bt_value_string_get(attr_field_name_obj);
}

BT_HIDDEN
struct bt_value *bt_attributes_borrow_field_value(
		struct bt_value *attr_obj, uint64_t index)
{
	struct bt_value *attr_field_obj = NULL;

	BT_ASSERT_DBG(attr_obj);
	BT_ASSERT_DBG(index < bt_value_array_get_length(attr_obj));

	attr_field_obj =
		bt_value_array_borrow_element_by_index(attr_obj, index);

	return bt_value_array_borrow_element_by_index( attr_field_obj,
		BT_ATTR_VALUE_INDEX);
}

static
struct bt_value *bt_attributes_borrow_field_by_name(
		struct bt_value *attr_obj, const char *name)
{
	uint64_t i, attr_size;
	struct bt_value *value_obj = NULL;
	struct bt_value *attr_field_name_obj = NULL;

	attr_size = bt_value_array_get_length(attr_obj);
	for (i = 0; i < attr_size; ++i) {
		const char *field_name;

		value_obj = bt_value_array_borrow_element_by_index(
			attr_obj, i);

		attr_field_name_obj =
			bt_value_array_borrow_element_by_index(
				value_obj, BT_ATTR_NAME_INDEX);

		field_name = bt_value_string_get(attr_field_name_obj);

		if (strcmp(field_name, name) == 0) {
			break;
		}

		value_obj = NULL;
	}

	return value_obj;
}

BT_HIDDEN
int bt_attributes_set_field_value(struct bt_value *attr_obj,
		const char *name, struct bt_value *value_obj)
{
	int ret = 0;
	struct bt_value *attr_field_obj = NULL;

	BT_ASSERT(attr_obj);
	BT_ASSERT(name);
	BT_ASSERT(value_obj);
	attr_field_obj = bt_attributes_borrow_field_by_name(attr_obj, name);
	if (attr_field_obj) {
		ret = bt_value_array_set_element_by_index(
			attr_field_obj, BT_ATTR_VALUE_INDEX,
			value_obj);
		attr_field_obj = NULL;
		goto end;
	}

	attr_field_obj = bt_value_array_create();
	if (!attr_field_obj) {
		BT_LIB_LOGE_APPEND_CAUSE("Failed to create empty array value.");
		ret = -1;
		goto end;
	}

	ret = bt_value_array_append_string_element(attr_field_obj,
		name);
	ret |= bt_value_array_append_element(attr_field_obj,
		value_obj);
	if (ret) {
		BT_LIB_LOGE_APPEND_CAUSE(
			"Cannot append elements to array value: %!+v",
			attr_field_obj);
		goto end;
	}

	ret = bt_value_array_append_element(attr_obj,
		attr_field_obj);
	if (ret) {
		BT_LIB_LOGE_APPEND_CAUSE(
			"Cannot append element to array value: "
			"%![array-value-]+v, %![element-value-]+v",
			attr_obj, attr_field_obj);
	}

end:
	bt_object_put_ref(attr_field_obj);
	return ret;
}

BT_HIDDEN
struct bt_value *bt_attributes_borrow_field_value_by_name(
		struct bt_value *attr_obj, const char *name)
{
	struct bt_value *value_obj = NULL;
	struct bt_value *attr_field_obj = NULL;

	BT_ASSERT_DBG(attr_obj);
	BT_ASSERT_DBG(name);
	attr_field_obj = bt_attributes_borrow_field_by_name(attr_obj, name);
	if (!attr_field_obj) {
		BT_LOGD("Cannot find attributes object's field by name: "
			"value-addr=%p, name=\"%s\"", attr_obj, name);
		goto end;
	}

	value_obj = bt_value_array_borrow_element_by_index(
		attr_field_obj, BT_ATTR_VALUE_INDEX);

end:
	return value_obj;
}

BT_HIDDEN
int bt_attributes_freeze(const struct bt_value *attr_obj)
{
	uint64_t i, count;
	int ret = 0;

	BT_ASSERT(attr_obj);
	BT_LOGD("Freezing attributes object: value-addr=%p", attr_obj);

	count = bt_value_array_get_length(attr_obj);

	/*
	 * We do not freeze the array value object itself here, since
	 * internal stuff could need to modify/add attributes. Each
	 * attribute is frozen one by one.
	 */
	for (i = 0; i < count; ++i) {
		struct bt_value *obj = NULL;

		obj = bt_attributes_borrow_field_value(
			(void *) attr_obj, i);
		if (!obj) {
			BT_LIB_LOGE_APPEND_CAUSE(
				"Cannot get attributes object's field value by index: "
				"%![value-]+v, index=%" PRIu64,
				attr_obj, i);
			ret = -1;
			goto end;
		}

		bt_value_freeze(obj);
	}

end:
	return ret;
}
