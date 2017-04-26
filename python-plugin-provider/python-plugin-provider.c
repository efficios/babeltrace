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

#include <babeltrace/babeltrace-internal.h>
#include <babeltrace/compiler-internal.h>
#include <babeltrace/ref.h>
#include <babeltrace/plugin/plugin-internal.h>
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

static
void print_python_traceback_verbose(void)
{
	if (Py_IsInitialized() && PyErr_Occurred() && babeltrace_verbose) {
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
	sighandler_t old_sigint = signal(SIGINT, SIG_DFL);

	if (python_state != PYTHON_STATE_NOT_INITED) {
		goto end;
	}

	/*
	 * User can disable Python plugin support with the
	 * BABELTRACE_DISABLE_PYTHON_PLUGINS environment variable set to
	 * 1.
	 */
	dis_python_env = getenv("BABELTRACE_DISABLE_PYTHON_PLUGINS");
	if (dis_python_env && dis_python_env[0] == '1' &&
			dis_python_env[1] == '\0') {
		printf_verbose("Python plugin support is disabled by BABELTRACE_DISABLE_PYTHON_PLUGINS environment variable\n");
		python_state = PYTHON_STATE_CANNOT_INITIALIZE;
		goto end;
	}

	if (!Py_IsInitialized()) {
		Py_InitializeEx(0);
		printf_verbose("Initialized Python:\n%s\n", Py_GetVersion());
	}

	py_bt2_py_plugin_mod = PyImport_ImportModule("bt2.py_plugin");
	if (!py_bt2_py_plugin_mod) {
		printf_verbose("Cannot import bt2.py_plugin Python module\n");
		python_state = PYTHON_STATE_CANNOT_INITIALIZE;
		goto end;
	}

	py_try_load_plugin_module_func =
		PyObject_GetAttrString(py_bt2_py_plugin_mod, "_try_load_plugin_module");
	if (!py_try_load_plugin_module_func) {
		printf_verbose("Cannot get _try_load_plugin_module attribute from bt2.py_plugin Python module\n");
		python_state = PYTHON_STATE_CANNOT_INITIALIZE;
		goto end;
	}

	python_state = PYTHON_STATE_FULLY_INITIALIZED;

end:
	if (old_sigint != SIG_ERR) {
		(void) signal(SIGINT, old_sigint);
	}

	print_python_traceback_verbose();
	pyerr_clear();
	Py_XDECREF(py_bt2_py_plugin_mod);
	return;
}

__attribute__((destructor)) static
void fini_python(void) {
	if (Py_IsInitialized()) {
		if (py_try_load_plugin_module_func) {
			Py_DECREF(py_try_load_plugin_module_func);
			py_try_load_plugin_module_func = NULL;
		}

		Py_Finalize();
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

	assert(plugin_info);
	assert(python_state == PYTHON_STATE_FULLY_INITIALIZED);
	py_name = PyObject_GetAttrString(plugin_info, "name");
	if (!py_name) {
		printf_verbose("Cannot find `name` attribute in plugin info\n");
		goto error;
	}

	py_author = PyObject_GetAttrString(plugin_info, "author");
	if (!py_author) {
		printf_verbose("Cannot find `author` attribute in plugin info\n");
		goto error;
	}

	py_description = PyObject_GetAttrString(plugin_info, "description");
	if (!py_description) {
		printf_verbose("Cannot find `description` attribute in plugin info\n");
		goto error;
	}

	py_license = PyObject_GetAttrString(plugin_info, "license");
	if (!py_license) {
		printf_verbose("Cannot find `license` attribute in plugin info\n");
		goto error;
	}

	py_version = PyObject_GetAttrString(plugin_info, "version");
	if (!py_version) {
		printf_verbose("Cannot find `version` attribute in plugin info\n");
		goto error;
	}

	py_comp_class_addrs = PyObject_GetAttrString(plugin_info,
		"comp_class_addrs");
	if (!py_comp_class_addrs) {
		printf_verbose("Cannot find `comp_class_addrs` attribute in plugin info\n");
		goto error;
	}

	if (PyUnicode_Check(py_name)) {
		name = PyUnicode_AsUTF8(py_name);
		if (!name) {
			printf_verbose("Cannot decode plugin name string\n");
			goto error;
		}
	} else {
		/* Plugin name is mandatory */
		printf_verbose("Plugin name is not a string\n");
		goto error;
	}

	if (PyUnicode_Check(py_author)) {
		author = PyUnicode_AsUTF8(py_author);
		if (!author) {
			printf_verbose("Cannot decode plugin author string\n");
			goto error;
		}
	}

	if (PyUnicode_Check(py_description)) {
		description = PyUnicode_AsUTF8(py_description);
		if (!description) {
			printf_verbose("Cannot decode plugin description string\n");
			goto error;
		}
	}

	if (PyUnicode_Check(py_license)) {
		license = PyUnicode_AsUTF8(py_license);
		if (!license) {
			printf_verbose("Cannot decode plugin license string\n");
			goto error;
		}
	}

	if (PyTuple_Check(py_version)) {
		if (PyTuple_Size(py_version) >= 3) {
			PyObject *py_major = PyTuple_GetItem(py_version, 0);
			PyObject *py_minor = PyTuple_GetItem(py_version, 1);
			PyObject *py_patch = PyTuple_GetItem(py_version, 2);

			assert(py_major);
			assert(py_minor);
			assert(py_patch);

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
				printf_verbose("Invalid plugin version format\n");
				goto error;
			}
		}

		if (PyTuple_Size(py_version) >= 4) {
			PyObject *py_extra = PyTuple_GetItem(py_version, 3);

			assert(py_extra);

			if (PyUnicode_Check(py_extra)) {
				version_extra = PyUnicode_AsUTF8(py_extra);
				if (!version_extra) {
					printf_verbose("Cannot decode plugin version's extra string\n");
					goto error;
				}
			}
		}
	}

	plugin = bt_plugin_create_empty(BT_PLUGIN_TYPE_PYTHON);
	if (!plugin) {
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
			assert(py_comp_class_addr);
			if (PyLong_Check(py_comp_class_addr)) {
				comp_class = (struct bt_component_class *)
					PyLong_AsUnsignedLongLong(py_comp_class_addr);
			} else {
				printf_verbose("Component class address #%zu: not an integer\n",
					i);
				continue;
			}

			ret = bt_plugin_add_component_class(plugin, comp_class);
			if (ret < 0) {
				printf_verbose("Cannot add component class #%zu\n",
					i);
				continue;
			}
		}
	}

	bt_plugin_freeze(plugin);

	goto end;

error:
	print_python_traceback_verbose();
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

	assert(path);

	if (python_state == PYTHON_STATE_CANNOT_INITIALIZE) {
		/*
		 * We do not even care about the rest of the function
		 * here because we already know Python cannot be fully
		 * initialized.
		 */
		goto error;
	}

	path_len = strlen(path);

	/* File name ends with `.py` */
	if (strncmp(path + path_len - PYTHON_PLUGIN_FILE_EXT_LEN,
			PYTHON_PLUGIN_FILE_EXT,
			PYTHON_PLUGIN_FILE_EXT_LEN) != 0) {
		printf_verbose("Skipping non-Python file: `%s`\n",
			path);
		goto error;
	}

	/* File name starts with `bt_plugin_` */
	basename = g_path_get_basename(path);
	if (!basename) {
		goto error;
	}

	if (strncmp(basename, PYTHON_PLUGIN_FILE_PREFIX,
			PYTHON_PLUGIN_FILE_PREFIX_LEN) != 0) {
		printf_verbose("Skipping Python file not starting with `%s`: `%s`\n",
			PYTHON_PLUGIN_FILE_PREFIX, path);
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
		goto error;
	}

	/*
	 * Call bt2.py_plugin._try_load_plugin_module() with this path
	 * to get plugin info if the plugin is loadable and complete.
	 * This function returns None when there's an error, but just in
	 * case we also manually clear the last Python error state.
	 */
	py_plugin_info = PyObject_CallFunction(py_try_load_plugin_module_func,
		"(s)", path);
	if (!py_plugin_info || py_plugin_info == Py_None) {
		printf_verbose("Cannot load Python plugin `%s`:\n", path);
		print_python_traceback_verbose();
		PyErr_Clear();
		goto error;
	}

	/*
	 * Get bt_plugin from plugin info object.
	 */
	plugin = bt_plugin_from_python_plugin_info(py_plugin_info);
	if (!plugin) {
		goto error;
	}

	bt_plugin_set_path(plugin, path);
	plugin_set = bt_plugin_set_create();
	if (!plugin_set) {
		goto error;
	}

	bt_plugin_set_add_plugin(plugin_set, plugin);
	goto end;

error:
	BT_PUT(plugin_set);

end:
	bt_put(plugin);
	Py_XDECREF(py_plugin_info);
	g_free(basename);
	return plugin_set;
}
