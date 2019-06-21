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

/* Output argument typemap for plugin output (always appends) */
%typemap(in, numinputs=0)
	(const bt_plugin **)
	(bt_plugin *temp_plugin = NULL) {
	$1 = &temp_plugin;
}

%typemap(argout)
	(const bt_plugin **) {
	if (*$1) {
		/* SWIG_Python_AppendOutput() steals the created object */
		$result = SWIG_Python_AppendOutput($result,
				SWIG_NewPointerObj(SWIG_as_voidptr(*$1),
					SWIGTYPE_p_bt_plugin, 0));
	} else {
		/* SWIG_Python_AppendOutput() steals Py_None */
		Py_INCREF(Py_None);
		$result = SWIG_Python_AppendOutput($result, Py_None);
	}
}

/* Output argument typemap for plugin set output (always appends) */
%typemap(in, numinputs=0)
	(const bt_plugin_set **)
	(bt_plugin_set *temp_plugin_set = NULL) {
	$1 = &temp_plugin_set;
}

%typemap(argout)
	(const bt_plugin_set **) {
	if (*$1) {
		/* SWIG_Python_AppendOutput() steals the created object */
		$result = SWIG_Python_AppendOutput($result,
				SWIG_NewPointerObj(SWIG_as_voidptr(*$1),
					SWIGTYPE_p_bt_plugin_set, 0));
	} else {
		/* SWIG_Python_AppendOutput() steals Py_None */
		Py_INCREF(Py_None);
		$result = SWIG_Python_AppendOutput($result, Py_None);
	}
}

%include <babeltrace2/plugin/plugin-const.h>
%include <babeltrace2/plugin/plugin-set-const.h>

/* Helpers */

bt_property_availability bt_plugin_get_version_wrapper(
		const bt_plugin *plugin, unsigned int *major,
		unsigned int *minor, unsigned int *patch, const char **extra);

bt_plugin_status bt_plugin_find_wrapper(const char *plugin_name,
		bt_bool fail_on_load_error, const bt_plugin **plugin);

bt_plugin_status bt_plugin_find_all_from_file_wrapper(
		const char *path, bt_bool fail_on_load_error,
		const bt_plugin_set **plugin_set);

bt_plugin_status bt_plugin_find_all_from_dir_wrapper(
		const char *path, bt_bool recurse, bt_bool fail_on_load_error,
		const bt_plugin_set **plugin_set);

%{

/*
 * This *_wrapper() functions below ensure that when the API function
 * fails, the output parameter is set to `NULL`.  This is necessary
 * because the epilogue of the `something **OUT` typemap will use that
 * value to make a Python object.  We can't rely on the initial value of
 * `*OUT`; it could point to unreadable memory.
 */

bt_property_availability bt_plugin_get_version_wrapper(
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

bt_plugin_status bt_plugin_find_wrapper(const char *plugin_name,
		bt_bool fail_on_load_error, const bt_plugin **plugin)
{
	bt_plugin_status status;

	status = bt_plugin_find(plugin_name, fail_on_load_error,
		plugin);
	if (status != BT_PLUGIN_STATUS_OK) {
		*plugin = NULL;
	}

	return status;
}

bt_plugin_status bt_plugin_find_all_from_file_wrapper(
		const char *path, bt_bool fail_on_load_error,
		const bt_plugin_set **plugin_set)
{
	bt_plugin_status status;

	status = bt_plugin_find_all_from_file(path, fail_on_load_error,
		plugin_set);
	if (status != BT_PLUGIN_STATUS_OK) {
		*plugin_set = NULL;
	}

	return status;
}

bt_plugin_status bt_plugin_find_all_from_dir_wrapper(
		const char *path, bt_bool recurse, bt_bool fail_on_load_error,
		const bt_plugin_set **plugin_set)
{
	bt_plugin_status status;

	status = bt_plugin_find_all_from_dir(path, recurse, fail_on_load_error,
		plugin_set);
	if (status != BT_PLUGIN_STATUS_OK) {
		*plugin_set = NULL;
	}

	return status;
}

%}
