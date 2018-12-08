#ifndef BABELTRACE_GRAPH_SELF_COMPONENT_PORT_INPUT_MESSAGE_ITERATOR_H
#define BABELTRACE_GRAPH_SELF_COMPONENT_PORT_INPUT_MESSAGE_ITERATOR_H

/*
 * Copyright 2017-2018 Philippe Proulx <pproulx@efficios.com>
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

#include <stdint.h>

/* For bt_message_iterator_status */
#include <babeltrace/graph/message-iterator.h>

/*
 * For bt_component, bt_message_iterator,
 * bt_self_component_port_input_message_iterator,
 * bt_self_component_port_input, bt_message_array_const
 */
#include <babeltrace/types.h>

#ifdef __cplusplus
extern "C" {
#endif

static inline
bt_message_iterator *
bt_self_component_port_input_message_iterator_as_message_iterator(
		bt_self_component_port_input_message_iterator *iterator)
{
	return (void *) iterator;
}

extern bt_self_component_port_input_message_iterator *
bt_self_component_port_input_message_iterator_create(
		bt_self_component_port_input *input_port);

extern bt_component *
bt_self_component_port_input_message_iterator_borrow_component(
		bt_self_component_port_input_message_iterator *iterator);

extern bt_message_iterator_status
bt_self_component_port_input_message_iterator_next(
		bt_self_component_port_input_message_iterator *iterator,
		bt_message_array_const *msgs, uint64_t *count);

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

#endif /* BABELTRACE_GRAPH_SELF_COMPONENT_PORT_INPUT_MESSAGE_ITERATOR_H */
