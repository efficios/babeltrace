/*
 * Copyright (c) 2019 EfficiOS Inc. and Linux Foundation
 * Copyright (c) 2019 Philippe Proulx <pproulx@efficios.com>
 * Copyright (c) 2019 Simon Marchi <simon.marchi@efficios.com>
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

#define BT_LOG_OUTPUT_LEVEL log_level
#define BT_LOG_TAG "PY-COMMON"
#include "logging/log.h"

#include <Python.h>

#include "common/assert.h"
#include "py-common.h"

BT_HIDDEN
GString *bt_py_common_format_exception(int log_level)
{
	PyObject *type = NULL;
	PyObject *value = NULL;
	PyObject *traceback = NULL;
	PyObject *traceback_module = NULL;
	PyObject *format_exception_func = NULL;
	PyObject *exc_str_list = NULL;
	GString *msg_buf = NULL;
	const char *format_exc_func_name;
	Py_ssize_t i;

	BT_ASSERT(PyErr_Occurred());
	PyErr_Fetch(&type, &value, &traceback);
	BT_ASSERT(type);

	/* Make sure `value` is what we expected: an instance of `type` */
	PyErr_NormalizeException(&type, &value, &traceback);

	/*
	 * Import the standard `traceback` module which contains the
	 * functions to format an exception.
	 */
	traceback_module = PyImport_ImportModule("traceback");
	if (!traceback_module) {
		BT_LOGE_STR("Failed to import `traceback` module.");
		goto error;
	}

	/*
	 * `traceback` can be `NULL`, when we fail to call a Python
	 * function from native code (there is no Python stack at that
	 * point). For example, a function which takes 5 positional
	 * arguments, but 8 were given.
	 */
	format_exc_func_name = traceback ? "format_exception" :
		"format_exception_only";
	format_exception_func = PyObject_GetAttrString(traceback_module,
		format_exc_func_name);
	if (!format_exception_func) {
		BT_LOGE("Cannot find `%s` attribute in `traceback` module.",
			format_exc_func_name);
		goto error;
	}

	if (!PyCallable_Check(format_exception_func)) {
		BT_LOGE("`traceback.%s` attribute is not callable.",
			format_exc_func_name);
		goto error;
	}

	exc_str_list = PyObject_CallFunctionObjArgs(format_exception_func,
		type, value, traceback, NULL);
	if (!exc_str_list) {
		if (BT_LOG_ON_ERROR) {
			BT_LOGE("Failed to call `traceback.%s` function:",
				format_exc_func_name);
			PyErr_Print();
		}

		goto error;
	}

	msg_buf = g_string_new(NULL);

	for (i = 0; i < PyList_Size(exc_str_list); i++) {
		PyObject *exc_str;
		const char *str;

		exc_str = PyList_GetItem(exc_str_list, i);
		BT_ASSERT(exc_str);

		/* `str` is owned by `exc_str`, not by us */
		str = PyUnicode_AsUTF8(exc_str);
		if (!str) {
			if (BT_LOG_ON_ERROR) {
				BT_LOGE_STR("PyUnicode_AsUTF8() failed:");
				PyErr_Print();
			}

			goto error;
		}

		/* `str` has a trailing newline */
		g_string_append(msg_buf, str);
	}

	if (msg_buf->len > 0) {
		/* Remove trailing newline if any */
		if (msg_buf->str[msg_buf->len - 1] == '\n') {
			g_string_truncate(msg_buf, msg_buf->len - 1);
		}
	}

	goto end;

error:
	if (msg_buf) {
		g_string_free(msg_buf, TRUE);
		msg_buf = NULL;
	}

end:
	Py_XDECREF(exc_str_list);
	Py_XDECREF(format_exception_func);
	Py_XDECREF(traceback_module);

	/*
	 * We can safely call PyErr_Restore() because we always call
	 * PyErr_Fetch(), and having an error indicator is a function's
	 * precondition.
	 *
	 * PyErr_Restore() steals our references.
	 */
	PyErr_Restore(type, value, traceback);

	return msg_buf;
}
