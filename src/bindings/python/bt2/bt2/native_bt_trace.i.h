/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2016 Philippe Proulx <pproulx@efficios.com>
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
