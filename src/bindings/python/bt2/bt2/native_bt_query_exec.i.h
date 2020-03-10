/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2017 Philippe Proulx <pproulx@efficios.com>
 */

static
bt_query_executor *bt_bt2_query_executor_create(
		const bt_component_class *component_class, const char *object,
		const bt_value *params, PyObject *py_obj)
{
	return bt_query_executor_create_with_method_data(component_class,
		object, params, py_obj == Py_None ? NULL : py_obj);
}
