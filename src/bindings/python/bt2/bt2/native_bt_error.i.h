/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2019 Efficios, Inc.
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

#include <string-format/format-error.h>

static
PyObject *bt_bt2_format_bt_error_cause(const bt_error_cause *error_cause)
{
	gchar *error_cause_str;
	PyObject *py_error_cause_str = NULL;

	error_cause_str = format_bt_error_cause(error_cause, 80,
		bt_python_bindings_bt2_log_level, BT_COMMON_COLOR_WHEN_NEVER);
	BT_ASSERT(error_cause_str);

	py_error_cause_str = PyString_FromString(error_cause_str);

	g_free(error_cause_str);

	return py_error_cause_str;
}

static
PyObject *bt_bt2_format_bt_error(const bt_error *error)
{
	gchar *error_str;
	PyObject *py_error_str = NULL;

	error_str = format_bt_error(error, 80,
		bt_python_bindings_bt2_log_level, BT_COMMON_COLOR_WHEN_NEVER);
	BT_ASSERT(error_str);

	py_error_str = PyString_FromString(error_str);

	g_free(error_str);

	return py_error_str;
}
