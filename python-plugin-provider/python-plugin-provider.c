/*
 * python-plugin-provider.c
 *
 * Babeltrace Python plugin provider
 *
 * Copyright 2017 Philippe Proulx <pproulx@efficios.com>
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

#define BT_LOG_TAG "PLUGIN-PY"
#include "logging.h"

#include <babeltrace/babeltrace-internal.h>
#include <babeltrace/compiler-internal.h>
#include <babeltrace/ref.h>
#include <babeltrace/plugin/plugin.h>
#include <babeltrace/plugin/plugin-internal.h>
#include <babeltrace/graph/component-class.h>
#include <babeltrace/graph/component-class-internal.h>
#include <stdlib.h>
#include <signal.h>
#include <Python.h>
#include <glib.h>
#include <gmodule.h>

#define PYTHON_PLUGIN_FILE_PREFIX	"bt_plugin_"
#define PYTHON_PLUGIN_FILE_PREFIX_LEN	(sizeof(PYTHON_PLUGIN_FILE_PREFIX) - 1)
#define PYTHON_PLUGIN_FILE_EXT		".py"
#define PYTHON_PLUGIN_FILE_EXT_LEN	(sizeof(PYTHON_PLUGIN_FILE_EXT) - 1)

enum python_state {
	/* init_python() not called yet */
	PYTHON_STATE_NOT_INITED,

	/* init_python() called once with success */
	PYTHON_STATE_FULLY_INITIALIZED,

	/* init_python() called once without success */
	PYTHON_STATE_CANNOT_INITIALIZE,
} python_state = PYTHON_STATE_NOT_INITED;

static PyObject *py_try_load_plugin_module_func = NULL;
static bool python_was_initialized_by_us;

static
void print_python_traceback_warn(void)
{
	if (BT_LOG_ON_WARN && Py_IsInitialized() && PyErr_Occurred()) {
		BT_LOGW_STR("Exception occured: traceback: ");
		PyErr_Print();
	}
}

static
void pyerr_clear(void)
{
	if (Py_IsInitialized()) {
		PyErr_Clear();
	}
}

static
void init_python(void)
{
	PyObject *py_bt2_py_plugin_mod = NULL;
	const char *dis_python_env;
#ifndef __MINGW32__
	sighandler_t old_sigint = signal(SIGINT, SIG_DFL);
#endif

	if (python_state != PYTHON_STATE_NOT_INITED) {
		goto end;
	}

	/*
	 * User can disable Python plugin support with the
	 * BABELTRACE_DISABLE_PYTHON_PLUGINS environment variable set to
	 * 1.
	 */
	dis_python_env = getenv("BABELTRACE_DISABLE_PYTHON_PLUGINS");
	if (dis_python_env && strcmp(dis_python_env, "1") == 0) {
		BT_LOGI_STR("Python plugin support is disabled because `BABELTRACE_DISABLE_PYTHON_PLUGINS=1`.");
		python_state = PYTHON_STATE_CANNOT_INITIALIZE;
		goto end;
	}

	if (!Py_IsInitialized()) {
		BT_LOGI_STR("Python interpreter is not initialized: initializing Python interpreter.");
		Py_InitializeEx(0);
		python_was_initialized_by_us = true;
		BT_LOGI("Initialized Python interpreter: version=\"%s\"",
			Py_GetVersion());
	} else {
		BT_LOGI("Python interpreter is already initialized: version=\"%s\"",
			Py_GetVersion());
	}

	py_bt2_py_plugin_mod = PyImport_ImportModule("bt2.py_plugin");
	if (!py_bt2_py_plugin_mod) {
		BT_LOGI_STR("Cannot import bt2.py_plugin Python module: Python plugin support is disabled.");
		python_state = PYTHON_STATE_CANNOT_INITIALIZE;
		goto end;
	}

	py_try_load_plugin_module_func =
		PyObject_GetAttrString(py_bt2_py_plugin_mod, "_try_load_plugin_module");
	if (!py_try_load_plugin_module_func) {
		BT_LOGI_STR("Cannot get _try_load_plugin_module attribute from bt2.py_plugin Python module: Python plugin support is disabled.");
		python_state = PYTHON_STATE_CANNOT_INITIALIZE;
		goto end;
	}

	python_state = PYTHON_STATE_FULLY_INITIALIZED;

end:
#ifndef __MINGW32__
	if (old_sigint != SIG_ERR) {
		(void) signal(SIGINT, old_sigint);
	}
#endif

	print_python_traceback_warn();
	pyerr_clear();
	Py_XDECREF(py_bt2_py_plugin_mod);
	return;
}

__attribute__((destructor)) static
void fini_python(void) {
	if (Py_IsInitialized() && python_was_initialized_by_us) {
		if (py_try_load_plugin_module_func) {
			Py_DECREF(py_try_load_plugin_module_func);
			py_try_load_plugin_module_func = NULL;
		}

		Py_Finalize();
		BT_LOGI_STR("Finalized Python interpreter.");
	}

	python_state = PYTHON_STATE_NOT_INITED;
}

static
struct bt_plugin *bt_plugin_from_python_plugin_info(PyObject *plugin_info)
{
	struct bt_plugin *plugin = NULL;
	PyObject *py_name = NULL;
	PyObject *py_author = NULL;
	PyObject *py_description = NULL;
	PyObject *py_license = NULL;
	PyObject *py_version = NULL;
	PyObject *py_comp_class_addrs = NULL;
	const char *name = NULL;
	const char *author = NULL;
	const char *description = NULL;
	const char *license = NULL;
	unsigned int major = 0, minor = 0, patch = 0;
	const char *version_extra = NULL;
	int ret;

	BT_ASSERT(plugin_info);
	BT_ASSERT(python_state == PYTHON_STATE_FULLY_INITIALIZED);
	py_name = PyObject_GetAttrString(plugin_info, "name");
	if (!py_name) {
		BT_LOGW("Cannot find `name` attribute in Python plugin info object: "
			"py-plugin-info-addr=%p", plugin_info);
		goto error;
	}

	py_author = PyObject_GetAttrString(plugin_info, "author");
	if (!py_author) {
		BT_LOGW("Cannot find `author` attribute in Python plugin info object: "
			"py-plugin-info-addr=%p", plugin_info);
		goto error;
	}

	py_description = PyObject_GetAttrString(plugin_info, "description");
	if (!py_description) {
		BT_LOGW("Cannot find `desciption` attribute in Python plugin info object: "
			"py-plugin-info-addr=%p", plugin_info);
		goto error;
	}

	py_license = PyObject_GetAttrString(plugin_info, "license");
	if (!py_license) {
		BT_LOGW("Cannot find `license` attribute in Python plugin info object: "
			"py-plugin-info-addr=%p", plugin_info);
		goto error;
	}

	py_version = PyObject_GetAttrString(plugin_info, "version");
	if (!py_version) {
		BT_LOGW("Cannot find `version` attribute in Python plugin info object: "
			"py-plugin-info-addr=%p", plugin_info);
		goto error;
	}

	py_comp_class_addrs = PyObject_GetAttrString(plugin_info,
		"comp_class_addrs");
	if (!py_comp_class_addrs) {
		BT_LOGW("Cannot find `comp_class_addrs` attribute in Python plugin info object: "
			"py-plugin-info-addr=%p", plugin_info);
		goto error;
	}

	if (PyUnicode_Check(py_name)) {
		name = PyUnicode_AsUTF8(py_name);
		if (!name) {
			BT_LOGW("Cannot decode Python plugin name string: "
				"py-plugin-info-addr=%p", plugin_info);
			goto error;
		}
	} else {
		/* Plugin name is mandatory */
		BT_LOGW("Plugin name is not a string: "
			"py-plugin-info-addr=%p", plugin_info);
		goto error;
	}

	if (PyUnicode_Check(py_author)) {
		author = PyUnicode_AsUTF8(py_author);
		if (!author) {
			BT_LOGW("Cannot decode Python plugin author string: "
				"py-plugin-info-addr=%p", plugin_info);
			goto error;
		}
	}

	if (PyUnicode_Check(py_description)) {
		description = PyUnicode_AsUTF8(py_description);
		if (!description) {
			BT_LOGW("Cannot decode Python plugin description string: "
				"py-plugin-info-addr=%p", plugin_info);
			goto error;
		}
	}

	if (PyUnicode_Check(py_license)) {
		license = PyUnicode_AsUTF8(py_license);
		if (!license) {
			BT_LOGW("Cannot decode Python plugin license string: "
				"py-plugin-info-addr=%p", plugin_info);
			goto error;
		}
	}

	if (PyTuple_Check(py_version)) {
		if (PyTuple_Size(py_version) >= 3) {
			PyObject *py_major = PyTuple_GetItem(py_version, 0);
			PyObject *py_minor = PyTuple_GetItem(py_version, 1);
			PyObject *py_patch = PyTuple_GetItem(py_version, 2);

			BT_ASSERT(py_major);
			BT_ASSERT(py_minor);
			BT_ASSERT(py_patch);

			if (PyLong_Check(py_major)) {
				major = PyLong_AsUnsignedLong(py_major);
			}

			if (PyLong_Check(py_minor)) {
				minor = PyLong_AsUnsignedLong(py_minor);
			}

			if (PyLong_Check(py_patch)) {
				patch = PyLong_AsUnsignedLong(py_patch);
			}

			if (PyErr_Occurred()) {
				/* Overflow error, most probably */
				BT_LOGW("Invalid Python plugin version format: "
					"py-plugin-info-addr=%p", plugin_info);
				goto error;
			}
		}

		if (PyTuple_Size(py_version) >= 4) {
			PyObject *py_extra = PyTuple_GetItem(py_version, 3);

			BT_ASSERT(py_extra);

			if (PyUnicode_Check(py_extra)) {
				version_extra = PyUnicode_AsUTF8(py_extra);
				if (!version_extra) {
				BT_LOGW("Cannot decode Python plugin version's extra string: "
					"py-plugin-info-addr=%p", plugin_info);
					goto error;
				}
			}
		}
	}

	plugin = bt_plugin_create_empty(BT_PLUGIN_TYPE_PYTHON);
	if (!plugin) {
		BT_LOGE_STR("Cannot create empty plugin object.");
		goto error;
	}

	bt_plugin_set_name(plugin, name);

	if (description) {
		bt_plugin_set_description(plugin, description);
	}

	if (author) {
		bt_plugin_set_author(plugin, author);
	}

	if (license) {
		bt_plugin_set_license(plugin, license);
	}

	bt_plugin_set_version(plugin, major, minor, patch, version_extra);

	if (PyList_Check(py_comp_class_addrs)) {
		size_t i;

		for (i = 0; i < PyList_Size(py_comp_class_addrs); i++) {
			struct bt_component_class *comp_class;
			PyObject *py_comp_class_addr;

			py_comp_class_addr =
				PyList_GetItem(py_comp_class_addrs, i);
			BT_ASSERT(py_comp_class_addr);
			if (PyLong_Check(py_comp_class_addr)) {
				comp_class = (struct bt_component_class *)
					PyLong_AsUnsignedLongLong(py_comp_class_addr);
			} else {
				BT_LOGW("Component class address is not an integer in Python plugin info object: "
					"py-plugin-info-addr=%p, index=%zu",
					plugin_info, i);
				continue;
			}

			ret = bt_plugin_add_component_class(plugin, comp_class);
			if (ret < 0) {
				BT_LOGE("Cannot add component class to plugin: "
					"py-plugin-info-addr=%p, "
					"plugin-addr=%p, plugin-name=\"%s\", "
					"comp-class-addr=%p, "
					"comp-class-name=\"%s\", "
					"comp-class-type=%s",
					plugin_info,
					plugin, bt_plugin_get_name(plugin),
					comp_class,
					bt_component_class_get_name(comp_class),
					bt_component_class_type_string(
						bt_component_class_get_type(comp_class)));
				continue;
			}
		}
	}

	bt_plugin_freeze(plugin);

	goto end;

error:
	print_python_traceback_warn();
	pyerr_clear();
	BT_PUT(plugin);

end:
	Py_XDECREF(py_name);
	Py_XDECREF(py_author);
	Py_XDECREF(py_description);
	Py_XDECREF(py_license);
	Py_XDECREF(py_version);
	Py_XDECREF(py_comp_class_addrs);
	return plugin;
}

G_MODULE_EXPORT
struct bt_plugin_set *bt_plugin_python_create_all_from_file(const char *path)
{
	struct bt_plugin_set *plugin_set = NULL;
	struct bt_plugin *plugin = NULL;
	PyObject *py_plugin_info = NULL;
	gchar *basename = NULL;
	size_t path_len;

	BT_ASSERT(path);

	if (python_state == PYTHON_STATE_CANNOT_INITIALIZE) {
		/*
		 * We do not even care about the rest of the function
		 * here because we already know Python cannot be fully
		 * initialized.
		 */
		goto error;
	}

	BT_LOGD("Creating all Python plugins from file: path=\"%s\"", path);
	path_len = strlen(path);

	/* File name ends with `.py` */
	if (strncmp(path + path_len - PYTHON_PLUGIN_FILE_EXT_LEN,
			PYTHON_PLUGIN_FILE_EXT,
			PYTHON_PLUGIN_FILE_EXT_LEN) != 0) {
		BT_LOGD("Skipping non-Python file: path=\"%s\"", path);
		goto error;
	}

	/* File name starts with `bt_plugin_` */
	basename = g_path_get_basename(path);
	if (!basename) {
		BT_LOGW("Cannot get path's basename: path=\"%s\"", path);
		goto error;
	}

	if (strncmp(basename, PYTHON_PLUGIN_FILE_PREFIX,
			PYTHON_PLUGIN_FILE_PREFIX_LEN) != 0) {
		BT_LOGD("Skipping Python file not starting with `%s`: "
			"path=\"%s\"", PYTHON_PLUGIN_FILE_PREFIX, path);
		goto error;
	}

	/*
	 * Initialize Python now.
	 *
	 * This is not done in the library contructor because the
	 * interpreter is somewhat slow to initialize. If you don't
	 * have any potential Python plugins, you don't need to endure
	 * this waiting time everytime you load the library.
	 */
	init_python();
	if (python_state != PYTHON_STATE_FULLY_INITIALIZED) {
		/*
		 * For some reason we cannot initialize Python,
		 * import the required modules, and get the required
		 * attributes from them.
		 */
		BT_LOGI("Failed to initialize Python interpreter.");
		goto error;
	}

	/*
	 * Call bt2.py_plugin._try_load_plugin_module() with this path
	 * to get plugin info if the plugin is loadable and complete.
	 * This function returns None when there's an error, but just in
	 * case we also manually clear the last Python error state.
	 */
	BT_LOGD_STR("Getting Python plugin info object from Python module.");
	py_plugin_info = PyObject_CallFunction(py_try_load_plugin_module_func,
		"(s)", path);
	if (!py_plugin_info || py_plugin_info == Py_None) {
		BT_LOGW("Cannot load Python plugin: path=\"%s\"", path);
		print_python_traceback_warn();
		PyErr_Clear();
		goto error;
	}

	/*
	 * Get bt_plugin from plugin info object.
	 */
	plugin = bt_plugin_from_python_plugin_info(py_plugin_info);
	if (!plugin) {
		BT_LOGW("Cannot create plugin object from Python plugin info object: "
			"path=\"%s\", py-plugin-info-addr=%p",
			path, py_plugin_info);
		goto error;
	}

	bt_plugin_set_path(plugin, path);
	plugin_set = bt_plugin_set_create();
	if (!plugin_set) {
		BT_LOGE_STR("Cannot create empty plugin set.");
		goto error;
	}

	bt_plugin_set_add_plugin(plugin_set, plugin);
	BT_LOGD("Created all Python plugins from file: path=\"%s\", "
		"plugin-addr=%p, plugin-name=\"%s\"",
		path, plugin, bt_plugin_get_name(plugin));
	goto end;

error:
	BT_PUT(plugin_set);

end:
	bt_put(plugin);
	Py_XDECREF(py_plugin_info);
	g_free(basename);
	return plugin_set;
}
