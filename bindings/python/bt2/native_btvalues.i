/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2016 Philippe Proulx <pproulx@efficios.com>
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
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

/* Remove prefix from `bt_value_null` */
%rename(value_null) bt_value_null;

/* Type and status */
struct bt_value;

enum bt_value_type {
	BT_VALUE_TYPE_UNKNOWN =		-1,
	BT_VALUE_TYPE_NULL =		0,
	BT_VALUE_TYPE_BOOL =		1,
	BT_VALUE_TYPE_INTEGER =		2,
	BT_VALUE_TYPE_FLOAT =		3,
	BT_VALUE_TYPE_STRING =		4,
	BT_VALUE_TYPE_ARRAY =		5,
	BT_VALUE_TYPE_MAP =		6,
};

enum bt_value_type bt_value_get_type(const struct bt_value *object);

enum bt_value_status {
	BT_VALUE_STATUS_FROZEN =	-4,
	BT_VALUE_STATUS_CANCELLED =	-3,
	BT_VALUE_STATUS_INVAL =		-22,
	BT_VALUE_STATUS_ERROR =		-1,
	BT_VALUE_STATUS_OK =		0,
};

/* Null value object singleton */
struct bt_value * const bt_value_null;

/* Common functions */
enum bt_value_status bt_value_freeze(struct bt_value *object);
int bt_value_is_frozen(const struct bt_value *object);
struct bt_value *bt_value_copy(const struct bt_value *object);
int bt_value_compare(const struct bt_value *object_a,
		const struct bt_value *object_b);

/* Boolean value object functions */
struct bt_value *bt_value_bool_create(void);
struct bt_value *bt_value_bool_create_init(int val);
enum bt_value_status bt_value_bool_get(
		const struct bt_value *bool_obj, int *OUTPUT);
enum bt_value_status bt_value_bool_set(struct bt_value *bool_obj,
		int val);

/* Integer value object functions */
struct bt_value *bt_value_integer_create(void);
struct bt_value *bt_value_integer_create_init(int64_t val);
enum bt_value_status bt_value_integer_get(
		const struct bt_value *integer_obj, int64_t *OUTPUT);
enum bt_value_status bt_value_integer_set(
		struct bt_value *integer_obj, int64_t val);

/* Floating point number value object functions */
struct bt_value *bt_value_float_create(void);
struct bt_value *bt_value_float_create_init(double val);
enum bt_value_status bt_value_float_get(
		const struct bt_value *float_obj, double *OUTPUT);
enum bt_value_status bt_value_float_set(
		struct bt_value *float_obj, double val);

/* String value object functions */
struct bt_value *bt_value_string_create(void);
struct bt_value *bt_value_string_create_init(const char *val);
enum bt_value_status bt_value_string_set(struct bt_value *string_obj,
		const char *val);
enum bt_value_status bt_value_string_get(
		const struct bt_value *string_obj, const char **BTOUTSTR);

/* Array value object functions */
struct bt_value *bt_value_array_create(void);
int bt_value_array_size(const struct bt_value *array_obj);
struct bt_value *bt_value_array_get(const struct bt_value *array_obj,
		size_t index);
enum bt_value_status bt_value_array_append(struct bt_value *array_obj,
		struct bt_value *element_obj);
enum bt_value_status bt_value_array_set(struct bt_value *array_obj,
		size_t index, struct bt_value *element_obj);

/* Map value object functions */
struct bt_value *bt_value_map_create(void);
int bt_value_map_size(const struct bt_value *map_obj);
struct bt_value *bt_value_map_get(const struct bt_value *map_obj,
		const char *key);
int bt_value_map_has_key(const struct bt_value *map_obj,
		const char *key);
enum bt_value_status bt_value_map_insert(
		struct bt_value *map_obj, const char *key,
		struct bt_value *element_obj);
struct bt_value *bt_value_map_extend(struct bt_value *base_map_obj,
		struct bt_value *extension_map_obj);

%{
struct bt_value_map_get_keys_private_data {
	struct bt_value *keys;
};

static int bt_value_map_get_keys_private_cb(const char *key,
		struct bt_value *object, void *data)
{
	enum bt_value_status status;
	struct bt_value_map_get_keys_private_data *priv_data = data;

	status = bt_value_array_append_string(priv_data->keys, key);
	if (status != BT_VALUE_STATUS_OK) {
		return BT_FALSE;
	}

	return BT_TRUE;
}

static struct bt_value *bt_value_map_get_keys_private(
		const struct bt_value *map_obj)
{
	enum bt_value_status status;
	struct bt_value_map_get_keys_private_data data;

	data.keys = bt_value_array_create();
	if (!data.keys) {
		return NULL;
	}

	status = bt_value_map_foreach(map_obj, bt_value_map_get_keys_private_cb,
		&data);
	if (status != BT_VALUE_STATUS_OK) {
		goto error;
	}

	goto end;

error:
	if (data.keys) {
		BT_PUT(data.keys);
	}

end:
	return data.keys;
}
%}

struct bt_value *bt_value_map_get_keys_private(const struct bt_value *map_obj);
