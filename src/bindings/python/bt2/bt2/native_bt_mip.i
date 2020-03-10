/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2017 Philippe Proulx <pproulx@efficios.com>
 */

%include <babeltrace2/graph/component-descriptor-set.h>
%include <babeltrace2/graph/graph.h>

%{
#include "native_bt_mip.i.h"
%}

bt_component_descriptor_set_add_descriptor_status
bt_bt2_component_descriptor_set_add_descriptor_with_initialize_method_data(
		bt_component_descriptor_set *comp_descr_set,
		const bt_component_class *comp_cls,
		const bt_value *params, PyObject *init_method_data);
