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

#include <stdbool.h>

#include "logging/comp-logging.h"

static
void restore_current_thread_error_and_append_exception_chain_recursive(
		int active_log_level, PyObject *py_exc_value,
		bt_self_component_class *self_component_class,
		bt_self_component *self_component,
		bt_self_message_iterator *self_message_iterator,
		const char *module_name)
{
	PyObject *py_exc_cause_value;
	PyObject *py_exc_type = NULL;
	PyObject *py_exc_tb = NULL;
	PyObject *py_bt_error_msg = NULL;
	GString *gstr = NULL;

	/* If this exception has a (Python) cause, handle that one first. */
	py_exc_cause_value = PyException_GetCause(py_exc_value);
	if (py_exc_cause_value) {
		restore_current_thread_error_and_append_exception_chain_recursive(
			active_log_level, py_exc_cause_value,
			self_component_class, self_component,
			self_message_iterator, module_name);
	}

	py_exc_tb = PyException_GetTraceback(py_exc_value);

	if (PyErr_GivenExceptionMatches(py_exc_value, py_mod_bt2_exc_error_type)) {
		/*
		 * If the raised exception is a bt2._Error, restore the wrapped
		 * error.
		 */
		PyObject *py_error_swig_ptr;
		const bt_error *error;
		int ret;

		/*
		 * We never raise a bt2._Error with a (Python) cause: it should
		 * be the end of the chain.
		 */
		BT_ASSERT(!py_exc_cause_value);

		/*
		 * We steal the error object from the exception, to move
		 * it back as the current thread's error.
		 */
		py_error_swig_ptr = PyObject_GetAttrString(py_exc_value, "_ptr");
		BT_ASSERT(py_error_swig_ptr);

		ret = PyObject_SetAttrString(py_exc_value, "_ptr", Py_None);
		BT_ASSERT(ret == 0);

		ret = SWIG_ConvertPtr(py_error_swig_ptr, (void **) &error,
			SWIGTYPE_p_bt_error, 0);
		BT_ASSERT(ret == 0);

		Py_DECREF(py_error_swig_ptr);

		BT_CURRENT_THREAD_MOVE_ERROR_AND_RESET(error);

		/*
		 * Append a cause with just the traceback and message, not the
		 * full str() of the bt2._Error.  We don't want the causes of
		 * this bt2._Error to be included in the cause we create.
		 */
		gstr = bt_py_common_format_tb(py_exc_tb, active_log_level);
		if (!gstr) {
			/* bt_py_common_format_tb has already warned. */
			goto end;
		}

		g_string_prepend(gstr, "Traceback (most recent call last):\n");

		py_bt_error_msg = PyObject_GetAttrString(py_exc_value, "_msg");
		BT_ASSERT(py_bt_error_msg);

		g_string_append_printf(gstr, "\nbt2._Error: %s",
			PyUnicode_AsUTF8(py_bt_error_msg));
	} else {
		py_exc_type = PyObject_Type(py_exc_value);

		gstr = bt_py_common_format_exception(py_exc_type, py_exc_value,
				py_exc_tb, active_log_level, false);
		if (!gstr) {
			/* bt_py_common_format_exception has already warned. */
			goto end;
		}
	}

	if (self_component_class) {
		BT_CURRENT_THREAD_ERROR_APPEND_CAUSE_FROM_COMPONENT_CLASS(
			self_component_class, "%s", gstr->str);
	} else if (self_component) {
		BT_CURRENT_THREAD_ERROR_APPEND_CAUSE_FROM_COMPONENT(
			self_component, "%s", gstr->str);
	} else if (self_message_iterator) {
		BT_CURRENT_THREAD_ERROR_APPEND_CAUSE_FROM_MESSAGE_ITERATOR(
			self_message_iterator, "%s", gstr->str);
	} else {
		BT_CURRENT_THREAD_ERROR_APPEND_CAUSE_FROM_UNKNOWN(
			module_name, "%s", gstr->str);
	}

end:
	if (gstr) {
		g_string_free(gstr, TRUE);
	}

	Py_XDECREF(py_exc_cause_value);
	Py_XDECREF(py_exc_type);
	Py_XDECREF(py_exc_tb);
	Py_XDECREF(py_bt_error_msg);
}

/*
 * If you have the following code:
 *
 * try:
 *     try:
 *         something_that_raises_bt2_error()
 *     except bt2._Error as e1:
 *         raise ValueError from e1
 * except ValueError as e2:
 *     raise TypeError from e2
 *
 * We will have the following exception chain:
 *
 *     TypeError -> ValueError -> bt2._Error
 *
 * Where the TypeError is the current exception (obtained from PyErr_Fetch).
 *
 * The bt2._Error contains a `struct bt_error *` that used to be the current
 * thread's error, at the moment the exception was raised.
 *
 * This function gets to the bt2._Error and restores the wrapped
 * `struct bt_error *` as the current thread's error.
 *
 * Then, for each exception in the chain, starting with the oldest one, it adds
 * an error cause to the current thread's error.
 */
static
void restore_bt_error_and_append_current_exception_chain(
		int active_log_level,
		bt_self_component_class *self_component_class,
		bt_self_component *self_component,
		bt_self_message_iterator *self_message_iterator,
		const char *module_name)
{
	BT_ASSERT(PyErr_Occurred());

	/* Used to access and restore the current exception. */
	PyObject *py_exc_type;
	PyObject *py_exc_value;
	PyObject *py_exc_tb;

	/* Fetch and normalize the Python exception. */
	PyErr_Fetch(&py_exc_type, &py_exc_value, &py_exc_tb);
	PyErr_NormalizeException(&py_exc_type, &py_exc_value, &py_exc_tb);
	BT_ASSERT(py_exc_type);
	BT_ASSERT(py_exc_value);
	BT_ASSERT(py_exc_tb);

	/*
	 * Set the exception's traceback so it's possible to get it using
	 * PyException_GetTraceback in
	 * restore_current_thread_error_and_append_exception_chain_recursive.
	 */
	PyException_SetTraceback(py_exc_value, py_exc_tb);

	restore_current_thread_error_and_append_exception_chain_recursive(
		active_log_level, py_exc_value, self_component_class,
		self_component, self_message_iterator, module_name);

	PyErr_Restore(py_exc_type, py_exc_value, py_exc_tb);
}

static inline
void log_exception_and_maybe_append_cause(
		int func_log_level,
		int active_log_level,
		bool append_error,
		bt_self_component_class *self_component_class,
		bt_self_component *self_component,
		bt_self_message_iterator *self_message_iterator,
		const char *module_name)
{
	GString *gstr;

	BT_ASSERT(PyErr_Occurred());
	gstr = bt_py_common_format_current_exception(active_log_level);
	if (!gstr) {
		/* bt_py_common_format_current_exception() logs errors */
		goto end;
	}

	BT_COMP_LOG_CUR_LVL(func_log_level, active_log_level, self_component,
		"%s", gstr->str);

	if (append_error) {
		restore_bt_error_and_append_current_exception_chain(
			active_log_level, self_component_class, self_component,
			self_message_iterator, module_name);

	}

end:
	if (gstr) {
		g_string_free(gstr, TRUE);
	}
}

static
bt_logging_level get_self_component_log_level(bt_self_component *self_comp)
{
	return bt_component_get_logging_level(
		bt_self_component_as_component(self_comp));
}

static
bt_logging_level get_self_message_iterator_log_level(
		bt_self_message_iterator *self_msg_iter)
{
	bt_self_component *self_comp =
		bt_self_message_iterator_borrow_component(self_msg_iter);

	return get_self_component_log_level(self_comp);
}

static inline
void loge_exception_append_cause_clear(const char *module_name, int active_log_level)
{
	log_exception_and_maybe_append_cause(BT_LOG_ERROR, active_log_level,
		true, NULL, NULL, NULL, module_name);
	PyErr_Clear();
}

static inline
void logw_exception_clear(int active_log_level)
{
	log_exception_and_maybe_append_cause(BT_LOG_WARNING, active_log_level,
		false, NULL, NULL, NULL, NULL);
	PyErr_Clear();
}
