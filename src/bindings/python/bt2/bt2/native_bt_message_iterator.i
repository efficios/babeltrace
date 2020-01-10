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
