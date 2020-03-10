/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2016 Philippe Proulx <pproulx@efficios.com>
 */

%include <babeltrace2/trace-ir/trace-class.h>

/* Helper functions for Python */
%{
#include "native_bt_trace_class.i.h"
%}

int bt_bt2_trace_class_add_destruction_listener(
		bt_trace_class *trace_class, PyObject *py_callable,
		bt_listener_id *id);
