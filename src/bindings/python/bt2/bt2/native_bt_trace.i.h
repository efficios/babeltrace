/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2016 Philippe Proulx <pproulx@efficios.com>
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

static void
trace_destroyed_listener(const bt_trace *trace, void *py_callable)
{
	PyObject *py_trace_ptr = NULL;
	PyObject *py_res = NULL;

	py_trace_ptr = SWIG_NewPointerObj(SWIG_as_voidptr(trace),
		SWIGTYPE_p_bt_trace, 0);
	if (!py_trace_ptr) {
		BT_LOGF_STR("Failed to create a SWIG pointer object.");
		bt_common_abort();
	}

	py_res = PyObject_CallFunction(py_callable, "(O)", py_trace_ptr);
	if (!py_res) {
		logw_exception_clear(BT_LOG_OUTPUT_LEVEL);
		goto end;
	}

	BT_ASSERT(py_res == Py_None);

end:
	Py_DECREF(py_trace_ptr);
	Py_XDECREF(py_res);
}

static
int bt_bt2_trace_add_destruction_listener(bt_trace *trace,
		PyObject *py_callable, bt_listener_id *id)
{
	bt_trace_add_listener_status status;

	BT_ASSERT(trace);
	BT_ASSERT(py_callable);
	status = bt_trace_add_destruction_listener(
		trace, trace_destroyed_listener, py_callable, id);
	if (status == __BT_FUNC_STATUS_OK) {
		Py_INCREF(py_callable);
	}

	return status;
}
