/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2017 Philippe Proulx <pproulx@efficios.com>
 */

static
bt_component_descriptor_set_add_descriptor_status
bt_bt2_component_descriptor_set_add_descriptor_with_initialize_method_data(
		bt_component_descriptor_set *comp_descr_set,
		const bt_component_class *comp_cls,
		const bt_value *params, PyObject *obj)
{
	return bt_component_descriptor_set_add_descriptor_with_initialize_method_data(
		comp_descr_set, comp_cls, params, obj == Py_None ? NULL : obj);
}
