#ifndef BABELTRACE2_GRAPH_SELF_COMPONENT_PORT_INPUT_MESSAGE_ITERATOR_H
#define BABELTRACE2_GRAPH_SELF_COMPONENT_PORT_INPUT_MESSAGE_ITERATOR_H

/*
 * Copyright (c) 2010-2019 EfficiOS Inc. and Linux Foundation
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
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#ifndef __BT_IN_BABELTRACE_H
# error "Please include <babeltrace2/babeltrace.h> instead."
#endif

#include <stdint.h>

#include <babeltrace2/graph/message-iterator.h>
#include <babeltrace2/types.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum bt_self_component_port_input_message_iterator_create_from_message_iterator_status {
	BT_SELF_COMPONENT_PORT_INPUT_MESSAGE_ITERATOR_CREATE_FROM_MESSAGE_ITERATOR_STATUS_OK		= __BT_FUNC_STATUS_OK,
	BT_SELF_COMPONENT_PORT_INPUT_MESSAGE_ITERATOR_CREATE_FROM_MESSAGE_ITERATOR_STATUS_ERROR		= __BT_FUNC_STATUS_ERROR,
	BT_SELF_COMPONENT_PORT_INPUT_MESSAGE_ITERATOR_CREATE_FROM_MESSAGE_ITERATOR_STATUS_MEMORY_ERROR	= __BT_FUNC_STATUS_MEMORY_ERROR,
} bt_self_component_port_input_message_iterator_create_from_message_iterator_status;

typedef enum bt_self_component_port_input_message_iterator_create_from_sink_component_status {
	BT_SELF_COMPONENT_PORT_INPUT_MESSAGE_ITERATOR_CREATE_FROM_SINK_COMPONENT_STATUS_OK		= __BT_FUNC_STATUS_OK,
	BT_SELF_COMPONENT_PORT_INPUT_MESSAGE_ITERATOR_CREATE_FROM_SINK_COMPONENT_STATUS_ERROR		= __BT_FUNC_STATUS_ERROR,
	BT_SELF_COMPONENT_PORT_INPUT_MESSAGE_ITERATOR_CREATE_FROM_SINK_COMPONENT_STATUS_MEMORY_ERROR	= __BT_FUNC_STATUS_MEMORY_ERROR,
} bt_self_component_port_input_message_iterator_create_from_sink_component_status;

static inline
bt_message_iterator *
bt_self_component_port_input_message_iterator_as_message_iterator(
		bt_self_component_port_input_message_iterator *iterator)
{
	return __BT_UPCAST(bt_message_iterator, iterator);
}

extern bt_self_component_port_input_message_iterator_create_from_message_iterator_status
bt_self_component_port_input_message_iterator_create_from_message_iterator(
		bt_self_message_iterator *self_msg_iter,
		bt_self_component_port_input *input_port,
		bt_self_component_port_input_message_iterator **message_iterator);

extern bt_self_component_port_input_message_iterator_create_from_sink_component_status
bt_self_component_port_input_message_iterator_create_from_sink_component(
		bt_self_component_sink *self_comp,
		bt_self_component_port_input *input_port,
		bt_self_component_port_input_message_iterator **message_iterator);

extern bt_component *
bt_self_component_port_input_message_iterator_borrow_component(
		bt_self_component_port_input_message_iterator *iterator);

extern bt_message_iterator_next_status
bt_self_component_port_input_message_iterator_next(
		bt_self_component_port_input_message_iterator *iterator,
		bt_message_array_const *msgs, uint64_t *count);

extern bt_message_iterator_can_seek_ns_from_origin_status
bt_self_component_port_input_message_iterator_can_seek_ns_from_origin(
		bt_self_component_port_input_message_iterator *iterator,
		int64_t ns_from_origin, bt_bool *can_seek);

extern bt_message_iterator_can_seek_beginning_status
bt_self_component_port_input_message_iterator_can_seek_beginning(
		bt_self_component_port_input_message_iterator *iterator,
		bt_bool *can_seek);

extern bt_message_iterator_seek_ns_from_origin_status
bt_self_component_port_input_message_iterator_seek_ns_from_origin(
		bt_self_component_port_input_message_iterator *iterator,
		int64_t ns_from_origin);

extern bt_message_iterator_seek_beginning_status
bt_self_component_port_input_message_iterator_seek_beginning(
		bt_self_component_port_input_message_iterator *iterator);

extern void bt_self_component_port_input_message_iterator_get_ref(
		const bt_self_component_port_input_message_iterator *self_component_port_input_message_iterator);

extern void bt_self_component_port_input_message_iterator_put_ref(
		const bt_self_component_port_input_message_iterator *self_component_port_input_message_iterator);

#define BT_SELF_COMPONENT_PORT_INPUT_MESSAGE_ITERATOR_PUT_REF_AND_RESET(_var) \
	do {								\
		bt_self_component_port_input_message_iterator_put_ref(_var); \
		(_var) = NULL;						\
	} while (0)

#define BT_SELF_COMPONENT_PORT_INPUT_MESSAGE_ITERATOR_MOVE_REF(_var_dst, _var_src) \
	do {								\
		bt_self_component_port_input_message_iterator_put_ref(_var_dst); \
		(_var_dst) = (_var_src);				\
		(_var_src) = NULL;					\
	} while (0)

#ifdef __cplusplus
}
#endif

#endif /* BABELTRACE2_GRAPH_SELF_COMPONENT_PORT_INPUT_MESSAGE_ITERATOR_H */
