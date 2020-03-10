/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2016 Philippe Proulx <pproulx@efficios.com>
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
