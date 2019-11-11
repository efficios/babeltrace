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

struct bt_value_map_get_keys_data {
	struct bt_value *keys;
};

static bt_value_map_foreach_entry_const_func_status bt_value_map_get_keys_cb(
		const char *key, const struct bt_value *object, void *data)
{
	int status;
	struct bt_value_map_get_keys_data *priv_data = data;

	status = bt_value_array_append_string_element(priv_data->keys, key);
	BT_ASSERT(status == __BT_FUNC_STATUS_OK ||
		status == __BT_FUNC_STATUS_MEMORY_ERROR);
	return status;
}

static struct bt_value *bt_value_map_get_keys(const struct bt_value *map_obj)
{
	bt_value_map_foreach_entry_const_status status;
	struct bt_value_map_get_keys_data data;

	data.keys = bt_value_array_create();
	if (!data.keys) {
		return NULL;
	}

	status = bt_value_map_foreach_entry_const(map_obj, bt_value_map_get_keys_cb,
		&data);
	if (status != __BT_FUNC_STATUS_OK) {
		goto error;
	}

	goto end;

error:
	if (data.keys) {
		BT_VALUE_PUT_REF_AND_RESET(data.keys);
	}

end:
	return data.keys;
}
