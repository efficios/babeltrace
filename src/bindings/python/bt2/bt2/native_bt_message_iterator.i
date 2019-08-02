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

%include <babeltrace2/graph/message-iterator.h>
%include <babeltrace2/graph/port-output-message-iterator.h>
%include <babeltrace2/graph/self-component-port-input-message-iterator.h>
%include <babeltrace2/graph/self-message-iterator.h>

/* Helper functions for Python */
%{
#include "native_bt_message_iterator.i.h"
%}

PyObject *bt_bt2_get_user_component_from_user_msg_iter(
		bt_self_message_iterator *self_message_iterator);
PyObject *bt_bt2_self_component_port_input_get_msg_range(
		bt_self_component_port_input_message_iterator *iter);
PyObject *bt_bt2_port_output_get_msg_range(
		bt_port_output_message_iterator *iter);
