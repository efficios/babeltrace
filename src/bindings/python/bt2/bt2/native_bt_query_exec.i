/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2017 Philippe Proulx <pproulx@efficios.com>
 */

%include <babeltrace2/graph/private-query-executor.h>
%include <babeltrace2/graph/query-executor.h>

%{
#include "native_bt_query_exec.i.h"
%}

bt_query_executor *bt_bt2_query_executor_create(
		const bt_component_class *component_class, const char *object,
		const bt_value *params, PyObject *py_obj);
