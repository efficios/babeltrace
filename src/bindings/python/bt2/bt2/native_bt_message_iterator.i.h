/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2017 Philippe Proulx <pproulx@efficios.com>
 */

static
bt_message_iterator_create_from_message_iterator_status
bt_bt2_message_iterator_create_from_message_iterator(
		bt_self_message_iterator *self_msg_iter,
		bt_self_component_port_input *input_port,
		bt_message_iterator **message_iterator)
{
	bt_message_iterator_create_from_message_iterator_status
		status;

	status = bt_message_iterator_create_from_message_iterator(
		self_msg_iter, input_port, message_iterator);

	if (status != BT_MESSAGE_ITERATOR_CREATE_FROM_MESSAGE_ITERATOR_STATUS_OK) {
		*message_iterator = NULL;
	}

	return status;
}

static
bt_message_iterator_create_from_sink_component_status
bt_bt2_message_iterator_create_from_sink_component(
		bt_self_component_sink *self_comp,
		bt_self_component_port_input *input_port,
		bt_message_iterator **message_iterator)
{
	bt_message_iterator_create_from_sink_component_status
		status;

	status = bt_message_iterator_create_from_sink_component(
		self_comp, input_port, message_iterator);

	if (status != BT_MESSAGE_ITERATOR_CREATE_FROM_SINK_COMPONENT_STATUS_OK) {
		*message_iterator = NULL;
	}

	return status;
}

static PyObject *bt_bt2_get_user_component_from_user_msg_iter(
		bt_self_message_iterator *self_message_iterator)
{
	bt_self_component *self_component = bt_self_message_iterator_borrow_component(self_message_iterator);
	PyObject *py_comp;

	BT_ASSERT_DBG(self_component);
	py_comp = bt_self_component_get_data(self_component);
	BT_ASSERT_DBG(py_comp);

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
PyObject *get_msg_range_common(bt_message_iterator_next_status status,
		bt_message_array_const messages, uint64_t message_count)
{
	PyObject *py_status;
	PyObject *py_return_tuple;
	PyObject *py_msg_list;

	py_return_tuple = PyTuple_New(2);
	BT_ASSERT(py_return_tuple);

	/* Set tuple[0], status. */
	py_status = SWIG_From_long_SS_long(status);
	PyTuple_SET_ITEM(py_return_tuple, 0, py_status);

	/* Set tuple[1], message list on success, None otherwise. */
	if (status == __BT_FUNC_STATUS_OK) {
		py_msg_list = create_pylist_from_messages(messages, message_count);
	} else {
		py_msg_list = Py_None;
		Py_INCREF(py_msg_list);
	}

	PyTuple_SET_ITEM(py_return_tuple, 1, py_msg_list);

	return py_return_tuple;
}

static PyObject *bt_bt2_self_component_port_input_get_msg_range(
		bt_message_iterator *iter)
{
	bt_message_array_const messages;
	uint64_t message_count = 0;
	bt_message_iterator_next_status status;

	status = bt_message_iterator_next(iter,
		&messages, &message_count);
	return get_msg_range_common(status, messages, message_count);
}
