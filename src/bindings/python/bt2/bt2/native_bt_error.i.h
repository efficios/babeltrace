/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2019 Efficios, Inc.
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
