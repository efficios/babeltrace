/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2017 Philippe Proulx <pproulx@efficios.com>
 */

/* Output argument typemap for message_iterator (always appends) */
%typemap(in, numinputs=0)
	(bt_message_iterator **)
	(bt_message_iterator *temp_msg_iter = NULL) {
	$1 = &temp_msg_iter;
}

%typemap(argout)
	(bt_message_iterator **) {
	if (*$1) {
		/* SWIG_Python_AppendOutput() steals the created object */
		$result = SWIG_Python_AppendOutput($result,
				SWIG_NewPointerObj(SWIG_as_voidptr(*$1),
					SWIGTYPE_p_bt_message_iterator, 0));
	} else {
		/* SWIG_Python_AppendOutput() steals Py_None */
		Py_INCREF(Py_None);
		$result = SWIG_Python_AppendOutput($result, Py_None);
	}
}

%include <babeltrace2/graph/message-iterator.h>
%include <babeltrace2/graph/self-message-iterator.h>

/* Helper functions for Python */
%{
#include "native_bt_message_iterator.i.h"
%}

bt_message_iterator_create_from_message_iterator_status
bt_bt2_message_iterator_create_from_message_iterator(
		bt_self_message_iterator *self_msg_iter,
		bt_self_component_port_input *input_port,
		bt_message_iterator **message_iterator);
bt_message_iterator_create_from_sink_component_status
bt_bt2_message_iterator_create_from_sink_component(
		bt_self_component_sink *self_comp,
		bt_self_component_port_input *input_port,
		bt_message_iterator **message_iterator);
PyObject *bt_bt2_get_user_component_from_user_msg_iter(
		bt_self_message_iterator *self_message_iterator);
PyObject *bt_bt2_self_component_port_input_get_msg_range(
		bt_message_iterator *iter);
