#ifndef BABELTRACE_GRAPH_COMPONENT_CLASS_FILTER_H
#define BABELTRACE_GRAPH_COMPONENT_CLASS_FILTER_H

/*
 * Copyright 2017-2018 Philippe Proulx <pproulx@efficios.com>
 * Copyright 2016 Jérémie Galarneau <jeremie.galarneau@efficios.com>
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

/* For bt_self_component_status */
#include <babeltrace/graph/self-component.h>

/* For bt_self_message_iterator_status */
#include <babeltrace/graph/self-message-iterator.h>

/* For bt_query_status */
#include <babeltrace/graph/component-class.h>

/* For bt_component_class_status */
#include <babeltrace/graph/component-class-const.h>

/*
 * For bt_component_class, bt_component_class_filter, bt_port_input,
 * bt_port_output, bt_query_executor, bt_self_component_class_filter,
 * bt_self_component_filter, bt_self_component_port_input,
 * bt_self_component_port_output, bt_value, bt_message_array_const
 */
#include <babeltrace/types.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef bt_self_component_status
(*bt_component_class_filter_init_method)(
		bt_self_component_filter *self_component,
		const bt_value *params, void *init_method_data);

typedef void (*bt_component_class_filter_finalize_method)(
		bt_self_component_filter *self_component);

typedef bt_self_message_iterator_status
(*bt_component_class_filter_message_iterator_init_method)(
		bt_self_message_iterator *message_iterator,
		bt_self_component_filter *self_component,
		bt_self_component_port_output *port);

typedef void
(*bt_component_class_filter_message_iterator_finalize_method)(
		bt_self_message_iterator *message_iterator);

typedef bt_self_message_iterator_status
(*bt_component_class_filter_message_iterator_next_method)(
		bt_self_message_iterator *message_iterator,
		bt_message_array_const msgs, uint64_t capacity,
		uint64_t *count);

typedef bt_query_status
(*bt_component_class_filter_query_method)(
		bt_self_component_class_filter *comp_class,
		const bt_query_executor *query_executor,
		const char *object, const bt_value *params,
		const bt_value **result);

typedef bt_self_component_status
(*bt_component_class_filter_accept_input_port_connection_method)(
		bt_self_component_filter *self_component,
		bt_self_component_port_input *self_port,
		const bt_port_output *other_port);

typedef bt_self_component_status
(*bt_component_class_filter_accept_output_port_connection_method)(
		bt_self_component_filter *self_component,
		bt_self_component_port_output *self_port,
		const bt_port_input *other_port);

typedef bt_self_component_status
(*bt_component_class_filter_input_port_connected_method)(
		bt_self_component_filter *self_component,
		bt_self_component_port_input *self_port,
		const bt_port_output *other_port);

typedef bt_self_component_status
(*bt_component_class_filter_output_port_connected_method)(
		bt_self_component_filter *self_component,
		bt_self_component_port_output *self_port,
		const bt_port_input *other_port);

typedef void
(*bt_component_class_filter_input_port_disconnected_method)(
		bt_self_component_filter *self_component,
		bt_self_component_port_input *self_port);

typedef void
(*bt_component_class_filter_output_port_disconnected_method)(
		bt_self_component_filter *self_component,
		bt_self_component_port_output *self_port);

static inline
bt_component_class *bt_component_class_filter_as_component_class(
		bt_component_class_filter *comp_cls_filter)
{
	return (void *) comp_cls_filter;
}

extern
bt_component_class_filter *bt_component_class_filter_create(
		const char *name,
		bt_component_class_filter_message_iterator_next_method method);

extern bt_component_class_status
bt_component_class_filter_set_init_method(
		bt_component_class_filter *comp_class,
		bt_component_class_filter_init_method method);

extern bt_component_class_status
bt_component_class_filter_set_finalize_method(
		bt_component_class_filter *comp_class,
		bt_component_class_filter_finalize_method method);

extern bt_component_class_status
bt_component_class_filter_set_accept_input_port_connection_method(
		bt_component_class_filter *comp_class,
		bt_component_class_filter_accept_input_port_connection_method method);

extern bt_component_class_status
bt_component_class_filter_set_accept_output_port_connection_method(
		bt_component_class_filter *comp_class,
		bt_component_class_filter_accept_output_port_connection_method method);

extern bt_component_class_status
bt_component_class_filter_set_input_port_connected_method(
		bt_component_class_filter *comp_class,
		bt_component_class_filter_input_port_connected_method method);

extern bt_component_class_status
bt_component_class_filter_set_output_port_connected_method(
		bt_component_class_filter *comp_class,
		bt_component_class_filter_output_port_connected_method method);

extern bt_component_class_status
bt_component_class_filter_set_input_port_disconnected_method(
		bt_component_class_filter *comp_class,
		bt_component_class_filter_input_port_disconnected_method method);

extern bt_component_class_status
bt_component_class_filter_set_output_port_disconnected_method(
		bt_component_class_filter *comp_class,
		bt_component_class_filter_output_port_disconnected_method method);

extern bt_component_class_status
bt_component_class_filter_set_query_method(
		bt_component_class_filter *comp_class,
		bt_component_class_filter_query_method method);

extern bt_component_class_status
bt_component_class_filter_set_message_iterator_init_method(
		bt_component_class_filter *comp_class,
		bt_component_class_filter_message_iterator_init_method method);

extern bt_component_class_status
bt_component_class_filter_set_message_iterator_finalize_method(
		bt_component_class_filter *comp_class,
		bt_component_class_filter_message_iterator_finalize_method method);

#ifdef __cplusplus
}
#endif

#endif /* BABELTRACE_GRAPH_COMPONENT_CLASS_FILTER_H */
