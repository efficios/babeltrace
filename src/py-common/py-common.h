#ifndef BABELTRACE_PY_COMMON_INTERNAL_H
#define BABELTRACE_PY_COMMON_INTERNAL_H

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

#include <glib.h>
#include <Python.h>
#include <stdbool.h>

#include "common/macros.h"

/*
 * Formats the Python traceback `py_exc_tb` using traceback.format_tb, from the
 * Python standard library, and return it as a Gstring.
 */
BT_HIDDEN
GString *bt_py_common_format_tb(PyObject *py_exc_tb, int log_level);

/*
 * Formats the Python exception described by `py_exc_type`, `py_exc_value`
 * and `py_exc_tb` and returns the formatted string, or `NULL` on error. The
 * returned string does NOT end with a newline.
 *
 * If `chain` is true, include all exceptions in the causality chain
 * (see parameter `chain` of Python's traceback.format_exception).
 */
BT_HIDDEN
GString *bt_py_common_format_exception(PyObject *py_exc_type,
	        PyObject *py_exc_value, PyObject *py_exc_tb,
		int log_level, bool chain);

/*
 * Wrapper for `bt_py_common_format_exception` that passes the Python error
 * indicator (the exception currently being raised).  Always include the
 * full exception chain.
 *
 * You must ensure that the error indicator is set with PyErr_Occurred()
 * before you call this function.
 *
 * This function does not modify the error indicator, that is, anything
 * that is fetched is always restored.
 */
BT_HIDDEN
GString *bt_py_common_format_current_exception(int log_level);

#endif /* BABELTRACE_PY_COMMON_INTERNAL_H */
