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
	(const bt_plugin **OUT)
	(bt_plugin *temp_plugin = NULL) {
	$1 = &temp_plugin;
}

%typemap(argout)
	(const bt_plugin **OUT) {
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
	(const bt_plugin_set **OUT)
	(bt_plugin_set *temp_plugin_set = NULL) {
	$1 = &temp_plugin_set;
}

%typemap(argout)
	(const bt_plugin_set **OUT) {
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

/* From plugin-const.h */

typedef enum bt_plugin_status {
	BT_PLUGIN_STATUS_OK = 0,
	BT_PLUGIN_STATUS_NOT_FOUND = 2,
	BT_PLUGIN_STATUS_ERROR = -1,
	BT_PLUGIN_STATUS_LOADING_ERROR = -2,
	BT_PLUGIN_STATUS_NOMEM = -12,
} bt_plugin_status;

extern const char *bt_plugin_get_name(const bt_plugin *plugin);

extern const char *bt_plugin_get_author(const bt_plugin *plugin);

extern const char *bt_plugin_get_license(const bt_plugin *plugin);

extern const char *bt_plugin_get_description(const bt_plugin *plugin);

extern const char *bt_plugin_get_path(const bt_plugin *plugin);

extern uint64_t bt_plugin_get_source_component_class_count(
		const bt_plugin *plugin);

extern uint64_t bt_plugin_get_filter_component_class_count(
		const bt_plugin *plugin);

extern uint64_t bt_plugin_get_sink_component_class_count(
		const bt_plugin *plugin);

extern const bt_component_class_source *
bt_plugin_borrow_source_component_class_by_index_const(
		const bt_plugin *plugin, uint64_t index);

extern const bt_component_class_filter *
bt_plugin_borrow_filter_component_class_by_index_const(
		const bt_plugin *plugin, uint64_t index);

extern const bt_component_class_sink *
bt_plugin_borrow_sink_component_class_by_index_const(
		const bt_plugin *plugin, uint64_t index);

extern const bt_component_class_source *
bt_plugin_borrow_source_component_class_by_name_const(
		const bt_plugin *plugin, const char *name);

extern const bt_component_class_filter *
bt_plugin_borrow_filter_component_class_by_name_const(
		const bt_plugin *plugin, const char *name);

extern const bt_component_class_sink *
bt_plugin_borrow_sink_component_class_by_name_const(
		const bt_plugin *plugin, const char *name);

extern void bt_plugin_get_ref(const bt_plugin *plugin);

extern void bt_plugin_put_ref(const bt_plugin *plugin);

/* From plugin-set-const.h */

extern uint64_t bt_plugin_set_get_plugin_count(
		const bt_plugin_set *plugin_set);

extern const bt_plugin *bt_plugin_set_borrow_plugin_by_index_const(
		const bt_plugin_set *plugin_set, uint64_t index);

extern void bt_plugin_set_get_ref(const bt_plugin_set *plugin_set);

extern void bt_plugin_set_put_ref(const bt_plugin_set *plugin_set);

/* Helpers */

bt_property_availability bt_plugin_get_version_wrapper(
		const bt_plugin *plugin, unsigned int *OUT,
		unsigned int *OUT, unsigned int *OUT, const char **OUT);

bt_plugin_status bt_plugin_find_wrapper(const char *plugin_name,
		bt_bool fail_on_load_error, const bt_plugin **OUT);

bt_plugin_status bt_plugin_find_all_from_file_wrapper(
		const char *path, bt_bool fail_on_load_error,
		const bt_plugin_set **OUT);

bt_plugin_status bt_plugin_find_all_from_dir_wrapper(
		const char *path, bt_bool recurse, bt_bool fail_on_load_error,
		const bt_plugin_set **OUT);

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
