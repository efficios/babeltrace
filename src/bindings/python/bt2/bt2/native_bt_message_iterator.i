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

%include <babeltrace2/graph/message-iterator-const.h>
%include <babeltrace2/graph/port-output-message-iterator.h>
%include <babeltrace2/graph/self-component-port-input-message-iterator.h>
%include <babeltrace2/graph/self-message-iterator.h>

/* Helper functions for Python */
%{
static PyObject *bt_py3_get_user_component_from_user_msg_iter(
		bt_self_message_iterator *self_message_iterator)
{
	bt_self_component *self_component = bt_self_message_iterator_borrow_component(self_message_iterator);
	PyObject *py_comp;

	BT_ASSERT(self_component);
	py_comp = bt_self_component_get_data(self_component);
	BT_ASSERT(py_comp);

	/* Return new reference */
	Py_INCREF(py_comp);
	return py_comp;
}

static inline
PyObject *create_pylist_from_messages(bt_message_array_const messages,
		uint64_t message_count)
{
	uint64_t i;
	PyObject *py_msg_list = PyList_New(message_count);
	BT_ASSERT(py_msg_list);
	for (i = 0; i < message_count; i++) {
		PyList_SET_ITEM(py_msg_list, i,
				SWIG_NewPointerObj(SWIG_as_voidptr(messages[i]),
					SWIGTYPE_p_bt_message, 0));
	}

	return py_msg_list;
}

static
PyObject *bt_py3_get_msg_range_common(bt_message_iterator_status status,
		bt_message_array_const messages, uint64_t message_count)
{
	PyObject *py_status;
	PyObject *py_return_tuple;
	PyObject *py_msg_list = Py_None;
	
	py_status = SWIG_From_long_SS_long(status);
	if (status != BT_MESSAGE_ITERATOR_STATUS_OK) {
		goto end;
	}

	py_msg_list = create_pylist_from_messages(messages, message_count);

end:
	py_return_tuple = PyTuple_New(2);
	BT_ASSERT(py_return_tuple);
	PyTuple_SET_ITEM(py_return_tuple, 0, py_status);
	PyTuple_SET_ITEM(py_return_tuple, 1, py_msg_list);

	return py_return_tuple;
}

static PyObject
*bt_py3_self_component_port_input_get_msg_range(
		bt_self_component_port_input_message_iterator *iter)
{
	bt_message_array_const messages;
	uint64_t message_count = 0;
	bt_message_iterator_status status;

	status = bt_self_component_port_input_message_iterator_next(iter, &messages,
				&message_count);
	
	return bt_py3_get_msg_range_common(status, messages, message_count);
}

static PyObject
*bt_py3_port_output_get_msg_range(
		bt_port_output_message_iterator *iter)
{
	bt_message_array_const messages;
	uint64_t message_count = 0;
	bt_message_iterator_status status;

	status =
		bt_port_output_message_iterator_next(iter, &messages,
				&message_count);
	
	return bt_py3_get_msg_range_common(status, messages, message_count);
}
%}

PyObject *bt_py3_get_user_component_from_user_msg_iter(
    bt_self_message_iterator *self_message_iterator);
PyObject *bt_py3_self_component_port_input_get_msg_range(
		bt_self_component_port_input_message_iterator *iter);
PyObject *bt_py3_port_output_get_msg_range(
		bt_port_output_message_iterator *iter);
