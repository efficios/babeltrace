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

%{
#include <babeltrace/plugin/plugin.h>
#include <assert.h>
%}

/* Types */
struct bt_plugin;

/* Status */
enum bt_plugin_status {
	BT_PLUGIN_STATUS_OK =		0,
	BT_PLUGIN_STATUS_ERROR =	-1,
	BT_PLUGIN_STATUS_NOMEM =	-4,
};

/* Functions */
const char *bt_plugin_get_name(struct bt_plugin *plugin);
const char *bt_plugin_get_author(struct bt_plugin *plugin);
const char *bt_plugin_get_license(struct bt_plugin *plugin);
const char *bt_plugin_get_description(struct bt_plugin *plugin);
const char *bt_plugin_get_path(struct bt_plugin *plugin);
enum bt_plugin_status bt_plugin_get_version(struct bt_plugin *plugin,
		unsigned int *OUTPUT, unsigned int *OUTPUT, unsigned int *OUTPUT,
		const char **BTOUTSTR);
int bt_plugin_get_component_class_count(struct bt_plugin *plugin);
struct bt_component_class *bt_plugin_get_component_class(
		struct bt_plugin *plugin, size_t index);
struct bt_component_class *bt_plugin_get_component_class_by_name_and_type(
		struct bt_plugin *plugin, const char *name,
		enum bt_component_class_type type);
struct bt_plugin *bt_plugin_create_from_name(const char *plugin_name);

%{
static PyObject *bt_py3_plugin_ptrs_list_from_bt_plugins(struct bt_plugin **plugins)
{
	PyObject *py_plugin_ptrs = NULL;
	struct bt_plugin **plugin_at;

	if (!plugins) {
		goto error;
	}

	py_plugin_ptrs = PyList_New(0);
	if (!py_plugin_ptrs) {
		goto error;
	}

	plugin_at = plugins;

	while (*plugin_at) {
		struct bt_plugin *plugin = *plugin_at;
		PyObject *py_plugin_ptr;
		int ret;

		py_plugin_ptr = SWIG_NewPointerObj(SWIG_as_voidptr(plugin),
			SWIGTYPE_p_bt_plugin, 0);
		if (!py_plugin_ptr) {
			goto error;
		}

		ret = PyList_Append(py_plugin_ptrs, py_plugin_ptr);
		Py_DECREF(py_plugin_ptr);
		if (ret < 0) {
			goto error;
		}

		plugin_at++;
	}

	goto end;

error:
	Py_XDECREF(py_plugin_ptrs);
	py_plugin_ptrs = Py_None;
	Py_INCREF(py_plugin_ptrs);

	if (plugins) {
		/*
		 * Put existing plugin references since they are not
		 * moved to the caller.
		 */
		plugin_at = plugins;

		while (*plugin_at) {
			bt_put(*plugin_at);
			plugin_at++;
		}
	}

end:
	PyErr_Clear();
	free(plugins);
	return py_plugin_ptrs;
}

static PyObject *bt_py3_plugin_create_all_from_file(const char *path)
{
	return bt_py3_plugin_ptrs_list_from_bt_plugins(
		bt_plugin_create_all_from_file(path));
}

static PyObject *bt_py3_plugin_create_all_from_dir(const char *path,
		bool recurse)
{
	return bt_py3_plugin_ptrs_list_from_bt_plugins(
		bt_plugin_create_all_from_dir(path, recurse));
}
%}

PyObject *bt_py3_plugin_create_all_from_file(const char *path);
PyObject *bt_py3_plugin_create_all_from_dir(const char *path,
		bool recurse);
