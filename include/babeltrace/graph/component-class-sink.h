#ifndef BABELTRACE_GRAPH_COMPONENT_CLASS_SINK_H
#define BABELTRACE_GRAPH_COMPONENT_CLASS_SINK_H

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

/* For bt_query_status */
#include <babeltrace/graph/component-class.h>

/* For bt_component_class_status */
#include <babeltrace/graph/component-class-const.h>

/*
 * For bt_component_class, bt_component_class_sink, bt_port_output,
 * bt_query_executor, bt_self_component_class_sink,
 * bt_self_component_sink, bt_self_component_port_input, bt_value,
 * __BT_UPCAST
 */
#include <babeltrace/types.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef bt_self_component_status (*bt_component_class_sink_init_method)(
		bt_self_component_sink *self_component,
		const bt_value *params, void *init_method_data);

typedef void (*bt_component_class_sink_finalize_method)(
		bt_self_component_sink *self_component);

typedef bt_query_status
(*bt_component_class_sink_query_method)(
		bt_self_component_class_sink *comp_class,
		const bt_query_executor *query_executor,
		const char *object, const bt_value *params,
		const bt_value **result);

typedef bt_self_component_status
(*bt_component_class_sink_accept_input_port_connection_method)(
		bt_self_component_sink *self_component,
		bt_self_component_port_input *self_port,
		const bt_port_output *other_port);

typedef bt_self_component_status
(*bt_component_class_sink_input_port_connected_method)(
		bt_self_component_sink *self_component,
		bt_self_component_port_input *self_port,
		const bt_port_output *other_port);

typedef bt_self_component_status
(*bt_component_class_sink_graph_is_configured_method)(
		bt_self_component_sink *self_component);

typedef bt_self_component_status (*bt_component_class_sink_consume_method)(
	bt_self_component_sink *self_component);

static inline
bt_component_class *bt_component_class_sink_as_component_class(
		bt_component_class_sink *comp_cls_sink)
{
	return __BT_UPCAST(bt_component_class, comp_cls_sink);
}

extern
bt_component_class_sink *bt_component_class_sink_create(
		const char *name,
		bt_component_class_sink_consume_method method);

extern bt_component_class_status bt_component_class_sink_set_init_method(
		bt_component_class_sink *comp_class,
		bt_component_class_sink_init_method method);

extern bt_component_class_status bt_component_class_sink_set_finalize_method(
		bt_component_class_sink *comp_class,
		bt_component_class_sink_finalize_method method);

extern bt_component_class_status
bt_component_class_sink_set_accept_input_port_connection_method(
		bt_component_class_sink *comp_class,
		bt_component_class_sink_accept_input_port_connection_method method);

extern bt_component_class_status
bt_component_class_sink_set_input_port_connected_method(
		bt_component_class_sink *comp_class,
		bt_component_class_sink_input_port_connected_method method);

extern bt_component_class_status
bt_component_class_sink_set_graph_is_configured_method(
		bt_component_class_sink *comp_class,
		bt_component_class_sink_graph_is_configured_method method);

extern bt_component_class_status bt_component_class_sink_set_query_method(
		bt_component_class_sink *comp_class,
		bt_component_class_sink_query_method method);

#ifdef __cplusplus
}
#endif

#endif /* BABELTRACE_GRAPH_COMPONENT_CLASS_SINK_H */
