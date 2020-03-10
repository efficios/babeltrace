/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2017 Philippe Proulx <pproulx@efficios.com>
 */

%include <babeltrace2/graph/component-class.h>
%include <babeltrace2/graph/component-class-dev.h>
%include <babeltrace2/graph/self-component-class.h>

%{
#include "native_bt_component_class.i.h"
%}

struct bt_component_class_source *bt_bt2_component_class_source_create(
		PyObject *py_cls, const char *name, const char *description,
		const char *help);
struct bt_component_class_filter *bt_bt2_component_class_filter_create(
		PyObject *py_cls, const char *name, const char *description,
		const char *help);
struct bt_component_class_sink *bt_bt2_component_class_sink_create(
		PyObject *py_cls, const char *name, const char *description,
		const char *help);
void bt_bt2_unregister_cc_ptr_to_py_cls(const bt_component_class *comp_cls);
bool bt_bt2_is_python_component_class(const bt_component_class *comp_cls);
