#ifndef BABELTRACE2_GRAPH_GRAPH_H
#define BABELTRACE2_GRAPH_GRAPH_H

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

#include <babeltrace2/types.h>
#include <babeltrace2/logging.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum bt_graph_listener_func_status {
	BT_GRAPH_LISTENER_FUNC_STATUS_OK		= __BT_FUNC_STATUS_OK,
	BT_GRAPH_LISTENER_FUNC_STATUS_ERROR		= __BT_FUNC_STATUS_ERROR,
	BT_GRAPH_LISTENER_FUNC_STATUS_MEMORY_ERROR	= __BT_FUNC_STATUS_MEMORY_ERROR,
} bt_graph_listener_func_status;

typedef bt_graph_listener_func_status
(*bt_graph_filter_component_input_port_added_listener_func)(
		const bt_component_filter *component,
		const bt_port_input *port, void *data);

typedef bt_graph_listener_func_status
(*bt_graph_sink_component_input_port_added_listener_func)(
		const bt_component_sink *component,
		const bt_port_input *port, void *data);

typedef bt_graph_listener_func_status
(*bt_graph_source_component_output_port_added_listener_func)(
		const bt_component_source *component,
		const bt_port_output *port, void *data);

typedef bt_graph_listener_func_status
(*bt_graph_filter_component_output_port_added_listener_func)(
		const bt_component_filter *component,
		const bt_port_output *port, void *data);

typedef bt_graph_listener_func_status
(*bt_graph_source_filter_component_ports_connected_listener_func)(
		const bt_component_source *source_component,
		const bt_component_filter *filter_component,
		const bt_port_output *upstream_port,
		const bt_port_input *downstream_port, void *data);

typedef bt_graph_listener_func_status
(*bt_graph_source_sink_component_ports_connected_listener_func)(
		const bt_component_source *source_component,
		const bt_component_sink *sink_component,
		const bt_port_output *upstream_port,
		const bt_port_input *downstream_port, void *data);

typedef bt_graph_listener_func_status
(*bt_graph_filter_filter_component_ports_connected_listener_func)(
		const bt_component_filter *filter_component_upstream,
		const bt_component_filter *filter_component_downstream,
		const bt_port_output *upstream_port,
		const bt_port_input *downstream_port,
		void *data);

typedef bt_graph_listener_func_status
(*bt_graph_filter_sink_component_ports_connected_listener_func)(
		const bt_component_filter *filter_component,
		const bt_component_sink *sink_component,
		const bt_port_output *upstream_port,
		const bt_port_input *downstream_port, void *data);

typedef void (* bt_graph_listener_removed_func)(void *data);

typedef enum bt_graph_simple_sink_component_init_func_status {
	BT_GRAPH_SIMPLE_SINK_COMPONENT_INIT_FUNC_STATUS_OK		= __BT_FUNC_STATUS_OK,
	BT_GRAPH_SIMPLE_SINK_COMPONENT_INIT_FUNC_STATUS_ERROR		= __BT_FUNC_STATUS_ERROR,
	BT_GRAPH_SIMPLE_SINK_COMPONENT_INIT_FUNC_STATUS_MEMORY_ERROR	= __BT_FUNC_STATUS_MEMORY_ERROR,
} bt_graph_simple_sink_component_init_func_status;

typedef bt_graph_simple_sink_component_init_func_status
(*bt_graph_simple_sink_component_init_func)(
		bt_self_component_port_input_message_iterator *iterator,
		void *data);

typedef enum bt_graph_simple_sink_component_consume_func_status {
	BT_GRAPH_SIMPLE_SINK_COMPONENT_CONSUME_FUNC_STATUS_OK			= __BT_FUNC_STATUS_OK,
	BT_GRAPH_SIMPLE_SINK_COMPONENT_CONSUME_FUNC_STATUS_ERROR		= __BT_FUNC_STATUS_ERROR,
	BT_GRAPH_SIMPLE_SINK_COMPONENT_CONSUME_FUNC_STATUS_MEMORY_ERROR	= 	__BT_FUNC_STATUS_MEMORY_ERROR,
	BT_GRAPH_SIMPLE_SINK_COMPONENT_CONSUME_FUNC_STATUS_AGAIN		= __BT_FUNC_STATUS_AGAIN,
	BT_GRAPH_SIMPLE_SINK_COMPONENT_CONSUME_FUNC_STATUS_END			= __BT_FUNC_STATUS_END,
} bt_graph_simple_sink_component_consume_func_status;

typedef bt_graph_simple_sink_component_consume_func_status
(*bt_graph_simple_sink_component_consume_func)(
		bt_self_component_port_input_message_iterator *iterator,
		void *data);

typedef void (*bt_graph_simple_sink_component_finalize_func)(void *data);

extern bt_graph *bt_graph_create(uint64_t mip_version);

typedef enum bt_graph_add_component_status {
	BT_GRAPH_ADD_COMPONENT_STATUS_OK		= __BT_FUNC_STATUS_OK,
	BT_GRAPH_ADD_COMPONENT_STATUS_ERROR		= __BT_FUNC_STATUS_ERROR,
	BT_GRAPH_ADD_COMPONENT_STATUS_MEMORY_ERROR	= __BT_FUNC_STATUS_MEMORY_ERROR,
} bt_graph_add_component_status;

extern bt_graph_add_component_status
bt_graph_add_source_component(bt_graph *graph,
		const bt_component_class_source *component_class,
		const char *name, const bt_value *params,
		bt_logging_level log_level, const bt_component_source **component);

extern bt_graph_add_component_status
bt_graph_add_source_component_with_init_method_data(
		bt_graph *graph,
		const bt_component_class_source *component_class,
		const char *name, const bt_value *params,
		void *init_method_data, bt_logging_level log_level,
		const bt_component_source **component);

extern bt_graph_add_component_status
bt_graph_add_filter_component(bt_graph *graph,
		const bt_component_class_filter *component_class,
		const char *name, const bt_value *params,
		bt_logging_level log_level,
		const bt_component_filter **component);

extern bt_graph_add_component_status
bt_graph_add_filter_component_with_init_method_data(
		bt_graph *graph,
		const bt_component_class_filter *component_class,
		const char *name, const bt_value *params,
		void *init_method_data, bt_logging_level log_level,
		const bt_component_filter **component);

extern bt_graph_add_component_status
bt_graph_add_sink_component(
		bt_graph *graph, const bt_component_class_sink *component_class,
		const char *name, const bt_value *params,
		bt_logging_level log_level,
		const bt_component_sink **component);

extern bt_graph_add_component_status
bt_graph_add_sink_component_with_init_method_data(
		bt_graph *graph, const bt_component_class_sink *component_class,
		const char *name, const bt_value *params,
		void *init_method_data, bt_logging_level log_level,
		const bt_component_sink **component);

extern bt_graph_add_component_status
bt_graph_add_simple_sink_component(bt_graph *graph, const char *name,
		bt_graph_simple_sink_component_init_func init_func,
		bt_graph_simple_sink_component_consume_func consume_func,
		bt_graph_simple_sink_component_finalize_func finalize_func,
		void *user_data, const bt_component_sink **component);

typedef enum bt_graph_connect_ports_status {
	BT_GRAPH_CONNECT_PORTS_STATUS_OK		= __BT_FUNC_STATUS_OK,
	BT_GRAPH_CONNECT_PORTS_STATUS_ERROR		= __BT_FUNC_STATUS_ERROR,
	BT_GRAPH_CONNECT_PORTS_STATUS_MEMORY_ERROR	= __BT_FUNC_STATUS_MEMORY_ERROR,
} bt_graph_connect_ports_status;

extern bt_graph_connect_ports_status bt_graph_connect_ports(bt_graph *graph,
		const bt_port_output *upstream,
		const bt_port_input *downstream,
		const bt_connection **connection);

typedef enum bt_graph_run_status {
	BT_GRAPH_RUN_STATUS_OK			= __BT_FUNC_STATUS_OK,
	BT_GRAPH_RUN_STATUS_ERROR		= __BT_FUNC_STATUS_ERROR,
	BT_GRAPH_RUN_STATUS_MEMORY_ERROR	= __BT_FUNC_STATUS_MEMORY_ERROR,
	BT_GRAPH_RUN_STATUS_AGAIN		= __BT_FUNC_STATUS_AGAIN,
	BT_GRAPH_RUN_STATUS_END			= __BT_FUNC_STATUS_END,
} bt_graph_run_status;

extern bt_graph_run_status bt_graph_run(bt_graph *graph);

typedef enum bt_graph_run_once_status {
	BT_GRAPH_RUN_ONCE_STATUS_OK		= __BT_FUNC_STATUS_OK,
	BT_GRAPH_RUN_ONCE_STATUS_ERROR		= __BT_FUNC_STATUS_ERROR,
	BT_GRAPH_RUN_ONCE_STATUS_MEMORY_ERROR	= __BT_FUNC_STATUS_MEMORY_ERROR,
	BT_GRAPH_RUN_ONCE_STATUS_AGAIN		= __BT_FUNC_STATUS_AGAIN,
	BT_GRAPH_RUN_ONCE_STATUS_END		= __BT_FUNC_STATUS_END,
} bt_graph_run_once_status;

extern bt_graph_run_once_status bt_graph_run_once(bt_graph *graph);

typedef enum bt_graph_add_listener_status {
	BT_GRAPH_ADD_LISTENER_STATUS_OK			= __BT_FUNC_STATUS_OK,
	BT_GRAPH_ADD_LISTENER_STATUS_MEMORY_ERROR	= __BT_FUNC_STATUS_MEMORY_ERROR,
} bt_graph_add_listener_status;

extern bt_graph_add_listener_status
bt_graph_add_filter_component_input_port_added_listener(
		bt_graph *graph,
		bt_graph_filter_component_input_port_added_listener_func listener,
		bt_graph_listener_removed_func listener_removed, void *data,
		bt_listener_id *listener_id);

extern bt_graph_add_listener_status
bt_graph_add_sink_component_input_port_added_listener(
		bt_graph *graph,
		bt_graph_sink_component_input_port_added_listener_func listener,
		bt_graph_listener_removed_func listener_removed, void *data,
		bt_listener_id *listener_id);

extern bt_graph_add_listener_status
bt_graph_add_source_component_output_port_added_listener(
		bt_graph *graph,
		bt_graph_source_component_output_port_added_listener_func listener,
		bt_graph_listener_removed_func listener_removed, void *data,
		bt_listener_id *listener_id);

extern bt_graph_add_listener_status
bt_graph_add_filter_component_output_port_added_listener(
		bt_graph *graph,
		bt_graph_filter_component_output_port_added_listener_func listener,
		bt_graph_listener_removed_func listener_removed, void *data,
		bt_listener_id *listener_id);

extern bt_graph_add_listener_status
bt_graph_add_source_filter_component_ports_connected_listener(
		bt_graph *graph,
		bt_graph_source_filter_component_ports_connected_listener_func listener,
		bt_graph_listener_removed_func listener_removed, void *data,
		bt_listener_id *listener_id);

extern bt_graph_add_listener_status
bt_graph_add_filter_filter_component_ports_connected_listener(
		bt_graph *graph,
		bt_graph_filter_filter_component_ports_connected_listener_func listener,
		bt_graph_listener_removed_func listener_removed, void *data,
		bt_listener_id *listener_id);

extern bt_graph_add_listener_status
bt_graph_add_source_sink_component_ports_connected_listener(
		bt_graph *graph,
		bt_graph_source_sink_component_ports_connected_listener_func listener,
		bt_graph_listener_removed_func listener_removed, void *data,
		bt_listener_id *listener_id);

extern bt_graph_add_listener_status
bt_graph_add_filter_sink_component_ports_connected_listener(
		bt_graph *graph,
		bt_graph_filter_sink_component_ports_connected_listener_func listener,
		bt_graph_listener_removed_func listener_removed, void *data,
		bt_listener_id *listener_id);

typedef enum bt_graph_add_interrupter_status {
	BT_GRAPH_ADD_INTERRUPTER_STATUS_OK		= __BT_FUNC_STATUS_OK,
	BT_GRAPH_ADD_INTERRUPTER_STATUS_MEMORY_ERROR	= __BT_FUNC_STATUS_MEMORY_ERROR,
} bt_graph_add_interrupter_status;

extern bt_graph_add_interrupter_status bt_graph_add_interrupter(bt_graph *graph,
		const bt_interrupter *interrupter);

extern void bt_graph_interrupt(bt_graph *graph);

#ifdef __cplusplus
}
#endif

#endif /* BABELTRACE2_GRAPH_GRAPH_H */
