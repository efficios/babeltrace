#ifndef BABELTRACE_GRAPH_GRAPH_H
#define BABELTRACE_GRAPH_GRAPH_H

/*
 * Copyright 2017-2018 Philippe Proulx <pproulx@efficios.com>
 * Copyright 2017 Jérémie Galarneau <jeremie.galarneau@efficios.com>
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

/*
 * For bt_bool, bt_component, bt_component_class,
 * bt_component_class_filter, bt_component_class_sink,
 * bt_component_class_source, bt_component_filter, bt_component_sink,
 * bt_component_source, bt_connection, bt_graph, bt_port_input,
 * bt_port_output, bt_value
 */
#include <babeltrace/types.h>

/* For enum bt_graph_status */
#include <babeltrace/graph/graph-const.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*bt_graph_filter_component_input_port_added_listener_func)(
		const bt_component_filter *component,
		const bt_port_input *port, void *data);

typedef void (*bt_graph_sink_component_input_port_added_listener_func)(
		const bt_component_sink *component,
		const bt_port_input *port, void *data);

typedef void (*bt_graph_source_component_output_port_added_listener_func)(
		const bt_component_source *component,
		const bt_port_output *port, void *data);

typedef void (*bt_graph_filter_component_output_port_added_listener_func)(
		const bt_component_filter *component,
		const bt_port_output *port, void *data);

typedef void (*bt_graph_filter_component_input_port_removed_listener_func)(
		const bt_component_filter *component,
		const bt_port_input *port, void *data);

typedef void (*bt_graph_sink_component_input_port_removed_listener_func)(
		const bt_component_sink *component,
		const bt_port_input *port, void *data);

typedef void (*bt_graph_source_component_output_port_removed_listener_func)(
		const bt_component_source *component,
		const bt_port_output *port, void *data);

typedef void (*bt_graph_filter_component_output_port_removed_listener_func)(
		const bt_component_filter *component,
		const bt_port_output *port, void *data);

typedef void (*bt_graph_source_filter_component_ports_connected_listener_func)(
		const bt_component_source *source_component,
		const bt_component_filter *filter_component,
		const bt_port_output *upstream_port,
		const bt_port_input *downstream_port, void *data);

typedef void (*bt_graph_source_sink_component_ports_connected_listener_func)(
		const bt_component_source *source_component,
		const bt_component_sink *sink_component,
		const bt_port_output *upstream_port,
		const bt_port_input *downstream_port, void *data);

typedef void (*bt_graph_filter_sink_component_ports_connected_listener_func)(
		const bt_component_filter *filter_component,
		const bt_component_sink *sink_component,
		const bt_port_output *upstream_port,
		const bt_port_input *downstream_port, void *data);

typedef void (*bt_graph_source_filter_component_ports_disconnected_listener_func)(
		const bt_component_source *source_component,
		const bt_component_filter *filter_component,
		const bt_port_output *upstream_port,
		const bt_port_input *downstream_port,
		void *data);

typedef void (*bt_graph_source_sink_component_ports_disconnected_listener_func)(
		const bt_component_source *source_component,
		const bt_component_sink *sink_component,
		const bt_port_output *upstream_port,
		const bt_port_input *downstream_port,
		void *data);

typedef void (*bt_graph_filter_sink_component_ports_disconnected_listener_func)(
		const bt_component_filter *filter_component,
		const bt_component_sink *sink_component,
		const bt_port_output *upstream_port,
		const bt_port_input *downstream_port,
		void *data);

typedef void (* bt_graph_listener_removed_func)(void *data);

extern bt_graph *bt_graph_create(void);

extern enum bt_graph_status bt_graph_add_source_component(
		bt_graph *graph,
		const bt_component_class_source *component_class,
		const char *name, const bt_value *params,
		const bt_component_source **component);

extern enum bt_graph_status
bt_graph_add_source_component_with_init_method_data(
		bt_graph *graph,
		const bt_component_class_source *component_class,
		const char *name, const bt_value *params,
		void *init_method_data,
		const bt_component_source **component);

extern enum bt_graph_status bt_graph_add_filter_component(
		bt_graph *graph,
		const bt_component_class_filter *component_class,
		const char *name, const bt_value *params,
		const bt_component_filter **component);

extern enum bt_graph_status
bt_graph_add_filter_component_with_init_method_data(
		bt_graph *graph,
		const bt_component_class_filter *component_class,
		const char *name, const bt_value *params,
		void *init_method_data,
		const bt_component_filter **component);

extern enum bt_graph_status bt_graph_add_sink_component(
		bt_graph *graph,
		const bt_component_class_sink *component_class,
		const char *name, const bt_value *params,
		const bt_component_sink **component);

extern enum bt_graph_status
bt_graph_add_sink_component_with_init_method_data(
		bt_graph *graph,
		const bt_component_class_sink *component_class,
		const char *name, const bt_value *params,
		void *init_method_data,
		const bt_component_sink **component);

extern enum bt_graph_status bt_graph_connect_ports(
		bt_graph *graph,
		const bt_port_output *upstream,
		const bt_port_input *downstream,
		const bt_connection **connection);

extern enum bt_graph_status bt_graph_run(bt_graph *graph);

extern enum bt_graph_status bt_graph_consume(
		bt_graph *graph);

extern enum bt_graph_status
bt_graph_add_filter_component_input_port_added_listener(
		bt_graph *graph,
		bt_graph_filter_component_input_port_added_listener_func listener,
		bt_graph_listener_removed_func listener_removed, void *data,
		int *listener_id);

extern enum bt_graph_status
bt_graph_add_sink_component_input_port_added_listener(
		bt_graph *graph,
		bt_graph_sink_component_input_port_added_listener_func listener,
		bt_graph_listener_removed_func listener_removed, void *data,
		int *listener_id);

extern enum bt_graph_status
bt_graph_add_source_component_output_port_added_listener(
		bt_graph *graph,
		bt_graph_source_component_output_port_added_listener_func listener,
		bt_graph_listener_removed_func listener_removed, void *data,
		int *listener_id);

extern enum bt_graph_status
bt_graph_add_filter_component_output_port_added_listener(
		bt_graph *graph,
		bt_graph_filter_component_output_port_added_listener_func listener,
		bt_graph_listener_removed_func listener_removed, void *data,
		int *listener_id);

extern enum bt_graph_status
bt_graph_add_filter_component_input_port_removed_listener(
		bt_graph *graph,
		bt_graph_filter_component_input_port_removed_listener_func listener,
		bt_graph_listener_removed_func listener_removed, void *data,
		int *listener_id);

extern enum bt_graph_status
bt_graph_add_sink_component_input_port_removed_listener(
		bt_graph *graph,
		bt_graph_sink_component_input_port_removed_listener_func listener,
		bt_graph_listener_removed_func listener_removed, void *data,
		int *listener_id);

extern enum bt_graph_status
bt_graph_add_source_component_output_port_removed_listener(
		bt_graph *graph,
		bt_graph_source_component_output_port_removed_listener_func listener,
		bt_graph_listener_removed_func listener_removed, void *data,
		int *listener_id);

extern enum bt_graph_status
bt_graph_add_filter_component_output_port_removed_listener(
		bt_graph *graph,
		bt_graph_filter_component_output_port_removed_listener_func listener,
		bt_graph_listener_removed_func listener_removed, void *data,
		int *listener_id);

extern enum bt_graph_status
bt_graph_add_source_filter_component_ports_connected_listener(
		bt_graph *graph,
		bt_graph_source_filter_component_ports_connected_listener_func listener,
		bt_graph_listener_removed_func listener_removed, void *data,
		int *listener_id);

extern enum bt_graph_status
bt_graph_add_source_sink_component_ports_connected_listener(
		bt_graph *graph,
		bt_graph_source_sink_component_ports_connected_listener_func listener,
		bt_graph_listener_removed_func listener_removed, void *data,
		int *listener_id);

extern enum bt_graph_status
bt_graph_add_filter_sink_component_ports_connected_listener(
		bt_graph *graph,
		bt_graph_filter_sink_component_ports_connected_listener_func listener,
		bt_graph_listener_removed_func listener_removed, void *data,
		int *listener_id);

extern enum bt_graph_status
bt_graph_add_source_filter_component_ports_disconnected_listener(
		bt_graph *graph,
		bt_graph_source_filter_component_ports_disconnected_listener_func listener,
		bt_graph_listener_removed_func listener_removed, void *data,
		int *listener_id);

extern enum bt_graph_status
bt_graph_add_source_sink_component_ports_disconnected_listener(
		bt_graph *graph,
		bt_graph_source_sink_component_ports_disconnected_listener_func listener,
		bt_graph_listener_removed_func listener_removed, void *data,
		int *listener_id);

extern enum bt_graph_status
bt_graph_add_filter_sink_component_ports_disconnected_listener(
		bt_graph *graph,
		bt_graph_filter_sink_component_ports_disconnected_listener_func listener,
		bt_graph_listener_removed_func listener_removed, void *data,
		int *listener_id);

extern enum bt_graph_status bt_graph_cancel(bt_graph *graph);

#ifdef __cplusplus
}
#endif

#endif /* BABELTRACE_GRAPH_GRAPH_H */
