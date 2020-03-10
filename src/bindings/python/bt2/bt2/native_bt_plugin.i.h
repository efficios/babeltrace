/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2017 Philippe Proulx <pproulx@efficios.com>
 */

/*
 * Those  bt_bt2_*() functions below ensure that when the API function
 * fails, the output parameter is set to `NULL`.  This is necessary
 * because the epilogue of the `something **OUT` typemap will use that
 * value to make a Python object.  We can't rely on the initial value of
 * `*OUT`; it could point to unreadable memory.
 */

static
bt_property_availability bt_bt2_plugin_get_version(
		const bt_plugin *plugin, unsigned int *major,
		unsigned int *minor, unsigned int *patch, const char **extra)
{
	bt_property_availability ret;

	ret = bt_plugin_get_version(plugin, major, minor, patch, extra);

	if (ret == BT_PROPERTY_AVAILABILITY_NOT_AVAILABLE) {
		*extra = NULL;
	}

	return ret;
}

static
bt_plugin_find_status bt_bt2_plugin_find(const char *plugin_name,
		bt_bool find_in_std_env_var, bt_bool find_in_user_dir,
		bt_bool find_in_sys_dir, bt_bool find_in_static,
		bt_bool fail_on_load_error, const bt_plugin **plugin)
{
	bt_plugin_find_status status;

	status = bt_plugin_find(plugin_name, find_in_std_env_var,
		find_in_user_dir, find_in_sys_dir, find_in_static,
		fail_on_load_error, plugin);
	if (status != __BT_FUNC_STATUS_OK) {
		*plugin = NULL;
	}

	return status;
}

static
bt_plugin_find_all_status bt_bt2_plugin_find_all(bt_bool find_in_std_env_var,
		bt_bool find_in_user_dir, bt_bool find_in_sys_dir,
		bt_bool find_in_static, bt_bool fail_on_load_error,
		const bt_plugin_set **plugin_set)
{
	bt_plugin_find_all_status status;

	status = bt_plugin_find_all(find_in_std_env_var,
		find_in_user_dir, find_in_sys_dir, find_in_static,
		fail_on_load_error, plugin_set);
	if (status != __BT_FUNC_STATUS_OK) {
		*plugin_set = NULL;
	}

	return status;
}

static
bt_plugin_find_all_from_file_status bt_bt2_plugin_find_all_from_file(
		const char *path, bt_bool fail_on_load_error,
		const bt_plugin_set **plugin_set)
{
	bt_plugin_find_all_from_file_status status;

	status = bt_plugin_find_all_from_file(path, fail_on_load_error,
		plugin_set);
	if (status != __BT_FUNC_STATUS_OK) {
		*plugin_set = NULL;
	}

	return status;
}

static
bt_plugin_find_all_from_dir_status bt_bt2_plugin_find_all_from_dir(
		const char *path, bt_bool recurse, bt_bool fail_on_load_error,
		const bt_plugin_set **plugin_set)
{
	bt_plugin_find_all_from_dir_status status;

	status = bt_plugin_find_all_from_dir(path, recurse, fail_on_load_error,
		plugin_set);
	if (status != __BT_FUNC_STATUS_OK) {
		*plugin_set = NULL;
	}

	return status;
}
