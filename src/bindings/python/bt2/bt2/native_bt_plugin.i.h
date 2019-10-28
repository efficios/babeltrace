/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2017 Philippe Proulx <pproulx@efficios.com>
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
