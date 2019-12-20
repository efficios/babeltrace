#ifndef BABELTRACE2_GRAPH_MESSAGE_ITERATOR_CLASS_H
#define BABELTRACE2_GRAPH_MESSAGE_ITERATOR_CLASS_H

/*
 * Copyright (c) 2019 EfficiOS Inc.
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

#ifdef __cplusplus
extern "C" {
#endif

typedef enum bt_message_iterator_class_set_method_status {
	BT_MESSAGE_ITERATOR_CLASS_SET_METHOD_STATUS_OK	= __BT_FUNC_STATUS_OK,
} bt_message_iterator_class_set_method_status;

typedef enum bt_message_iterator_class_initialize_method_status {
	BT_MESSAGE_ITERATOR_CLASS_INITIALIZE_METHOD_STATUS_OK		= __BT_FUNC_STATUS_OK,
	BT_MESSAGE_ITERATOR_CLASS_INITIALIZE_METHOD_STATUS_ERROR	= __BT_FUNC_STATUS_ERROR,
	BT_MESSAGE_ITERATOR_CLASS_INITIALIZE_METHOD_STATUS_MEMORY_ERROR	= __BT_FUNC_STATUS_MEMORY_ERROR,
} bt_message_iterator_class_initialize_method_status;

typedef enum bt_message_iterator_class_next_method_status {
	BT_MESSAGE_ITERATOR_CLASS_NEXT_METHOD_STATUS_OK			= __BT_FUNC_STATUS_OK,
	BT_MESSAGE_ITERATOR_CLASS_NEXT_METHOD_STATUS_AGAIN		= __BT_FUNC_STATUS_AGAIN,
	BT_MESSAGE_ITERATOR_CLASS_NEXT_METHOD_STATUS_ERROR		= __BT_FUNC_STATUS_ERROR,
	BT_MESSAGE_ITERATOR_CLASS_NEXT_METHOD_STATUS_MEMORY_ERROR	= __BT_FUNC_STATUS_MEMORY_ERROR,
	BT_MESSAGE_ITERATOR_CLASS_NEXT_METHOD_STATUS_END		= __BT_FUNC_STATUS_END,
} bt_message_iterator_class_next_method_status;

typedef enum bt_message_iterator_class_seek_ns_from_origin_method_status {
	BT_MESSAGE_ITERATOR_CLASS_SEEK_NS_FROM_ORIGIN_METHOD_STATUS_OK			= __BT_FUNC_STATUS_OK,
	BT_MESSAGE_ITERATOR_CLASS_SEEK_NS_FROM_ORIGIN_METHOD_STATUS_AGAIN		= __BT_FUNC_STATUS_AGAIN,
	BT_MESSAGE_ITERATOR_CLASS_SEEK_NS_FROM_ORIGIN_METHOD_STATUS_ERROR		= __BT_FUNC_STATUS_ERROR,
	BT_MESSAGE_ITERATOR_CLASS_SEEK_NS_FROM_ORIGIN_METHOD_STATUS_MEMORY_ERROR	= __BT_FUNC_STATUS_MEMORY_ERROR,
} bt_message_iterator_class_seek_ns_from_origin_method_status;

typedef enum bt_message_iterator_class_can_seek_ns_from_origin_method_status {
	BT_MESSAGE_ITERATOR_CLASS_CAN_SEEK_NS_FROM_ORIGIN_METHOD_STATUS_OK		= __BT_FUNC_STATUS_OK,
	BT_MESSAGE_ITERATOR_CLASS_CAN_SEEK_NS_FROM_ORIGIN_METHOD_STATUS_AGAIN		= __BT_FUNC_STATUS_AGAIN,
	BT_MESSAGE_ITERATOR_CLASS_CAN_SEEK_NS_FROM_ORIGIN_METHOD_STATUS_ERROR		= __BT_FUNC_STATUS_ERROR,
	BT_MESSAGE_ITERATOR_CLASS_CAN_SEEK_NS_FROM_ORIGIN_METHOD_STATUS_MEMORY_ERROR	= __BT_FUNC_STATUS_MEMORY_ERROR,
} bt_message_iterator_class_can_seek_ns_from_origin_method_status;

typedef enum bt_message_iterator_class_seek_beginning_method_status {
	BT_MESSAGE_ITERATOR_CLASS_SEEK_BEGINNING_METHOD_STATUS_OK		= __BT_FUNC_STATUS_OK,
	BT_MESSAGE_ITERATOR_CLASS_SEEK_BEGINNING_METHOD_STATUS_AGAIN		= __BT_FUNC_STATUS_AGAIN,
	BT_MESSAGE_ITERATOR_CLASS_SEEK_BEGINNING_METHOD_STATUS_ERROR		= __BT_FUNC_STATUS_ERROR,
	BT_MESSAGE_ITERATOR_CLASS_SEEK_BEGINNING_METHOD_STATUS_MEMORY_ERROR	= __BT_FUNC_STATUS_MEMORY_ERROR,
} bt_message_iterator_class_seek_beginning_method_status;

typedef enum bt_message_iterator_class_can_seek_beginning_method_status {
	BT_MESSAGE_ITERATOR_CLASS_CAN_SEEK_BEGINNING_METHOD_STATUS_OK		= __BT_FUNC_STATUS_OK,
	BT_MESSAGE_ITERATOR_CLASS_CAN_SEEK_BEGINNING_METHOD_STATUS_AGAIN	= __BT_FUNC_STATUS_AGAIN,
	BT_MESSAGE_ITERATOR_CLASS_CAN_SEEK_BEGINNING_METHOD_STATUS_ERROR	= __BT_FUNC_STATUS_ERROR,
	BT_MESSAGE_ITERATOR_CLASS_CAN_SEEK_BEGINNING_METHOD_STATUS_MEMORY_ERROR	= __BT_FUNC_STATUS_MEMORY_ERROR,
} bt_message_iterator_class_can_seek_beginning_method_status;

typedef bt_message_iterator_class_initialize_method_status
(*bt_message_iterator_class_initialize_method)(
		bt_self_message_iterator *message_iterator,
		bt_self_message_iterator_configuration *config,
		bt_self_component *self_component,
		bt_self_component_port_output *port);

typedef void
(*bt_message_iterator_class_finalize_method)(
		bt_self_message_iterator *message_iterator);

typedef bt_message_iterator_class_next_method_status
(*bt_message_iterator_class_next_method)(
		bt_self_message_iterator *message_iterator,
		bt_message_array_const msgs, uint64_t capacity,
		uint64_t *count);

typedef bt_message_iterator_class_seek_ns_from_origin_method_status
(*bt_message_iterator_class_seek_ns_from_origin_method)(
		bt_self_message_iterator *message_iterator,
		int64_t ns_from_origin);

typedef bt_message_iterator_class_can_seek_ns_from_origin_method_status
(*bt_message_iterator_class_can_seek_ns_from_origin_method)(
		bt_self_message_iterator *message_iterator,
		int64_t ns_from_origin, bt_bool *can_seek);

typedef bt_message_iterator_class_seek_beginning_method_status
(*bt_message_iterator_class_seek_beginning_method)(
		bt_self_message_iterator *message_iterator);

typedef bt_message_iterator_class_can_seek_beginning_method_status
(*bt_message_iterator_class_can_seek_beginning_method)(
		bt_self_message_iterator *message_iterator, bt_bool *can_seek);

extern void bt_message_iterator_class_get_ref(
		const bt_message_iterator_class *message_iterator_class);

extern void bt_message_iterator_class_put_ref(
		const bt_message_iterator_class *message_iterator_class);

#define BT_MESSAGE_ITERATOR_CLASS_PUT_REF_AND_RESET(_var)	\
	do {							\
		bt_message_iterator_class_put_ref(_var);	\
		(_var) = NULL;					\
	} while (0)

#define BT_MESSAGE_ITERATOR_CLASS_MOVE_MOVE_REF(_var_dst, _var_src)	\
	do {								\
		bt_message_iterator_class_put_ref(_var_dst);		\
		(_var_dst) = (_var_src);				\
		(_var_src) = NULL;					\
	} while (0)

extern bt_message_iterator_class *
bt_message_iterator_class_create(
		bt_message_iterator_class_next_method next_method);

extern bt_message_iterator_class_set_method_status
bt_message_iterator_class_set_initialize_method(
		bt_message_iterator_class *message_iterator_class,
		bt_message_iterator_class_initialize_method method);

extern bt_message_iterator_class_set_method_status
bt_message_iterator_class_set_finalize_method(
		bt_message_iterator_class *message_iterator_class,
		bt_message_iterator_class_finalize_method method);

extern bt_message_iterator_class_set_method_status
bt_message_iterator_class_set_seek_ns_from_origin_methods(
		bt_message_iterator_class *message_iterator_class,
		bt_message_iterator_class_seek_ns_from_origin_method seek_method,
		bt_message_iterator_class_can_seek_ns_from_origin_method can_seek_method);

extern bt_message_iterator_class_set_method_status
bt_message_iterator_class_set_seek_beginning_methods(
		bt_message_iterator_class *message_iterator_class,
		bt_message_iterator_class_seek_beginning_method seek_method,
		bt_message_iterator_class_can_seek_beginning_method can_seek_method);

#ifdef __cplusplus
}
#endif

#endif /* BABELTRACE2_GRAPH_MESSAGE_ITERATOR_CLASS_H */
