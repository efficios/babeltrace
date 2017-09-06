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

/* Types */
struct bt_component;

/* Status */
enum bt_component_status {
	BT_COMPONENT_STATUS_OK = 0,
	BT_COMPONENT_STATUS_END = 1,
	BT_COMPONENT_STATUS_AGAIN = 11,
	BT_COMPONENT_STATUS_REFUSE_PORT_CONNECTION = 111,
	BT_COMPONENT_STATUS_ERROR = -1,
	BT_COMPONENT_STATUS_UNSUPPORTED = -2,
	BT_COMPONENT_STATUS_INVALID = -22,
	BT_COMPONENT_STATUS_NOMEM = -12,
	BT_COMPONENT_STATUS_NOT_FOUND = -19,
	BT_COMPONENT_STATUS_GRAPH_IS_CANCELED = 125,
};

/* General functions */
const char *bt_component_get_name(struct bt_component *component);
struct bt_component_class *bt_component_get_class(
		struct bt_component *component);
enum bt_component_class_type bt_component_get_class_type(
		struct bt_component *component);
struct bt_graph *bt_component_get_graph(struct bt_component *component);
struct bt_component *bt_component_from_private(
		struct bt_private_component *private_component);

/* Source component functions */
int64_t bt_component_source_get_output_port_count(
		struct bt_component *component);
struct bt_port *bt_component_source_get_output_port_by_name(
		struct bt_component *component, const char *name);
struct bt_port *bt_component_source_get_output_port_by_index(
		struct bt_component *component, uint64_t index);

/* Private source component functions */
struct bt_private_port *
bt_private_component_source_get_output_private_port_by_name(
		struct bt_private_component *private_component,
		const char *name);
struct bt_private_port *
bt_private_component_source_get_output_private_port_by_index(
		struct bt_private_component *private_component,
		uint64_t index);
enum bt_component_status
bt_private_component_source_add_output_private_port(
		struct bt_private_component *private_component,
		const char *name, void *user_data,
		struct bt_private_port **BTOUTPRIVPORT);

/* Filter component functions */
int64_t bt_component_filter_get_input_port_count(
		struct bt_component *component);
struct bt_port *bt_component_filter_get_input_port_by_name(
		struct bt_component *component, const char *name);
struct bt_port *bt_component_filter_get_input_port_by_index(
		struct bt_component *component, uint64_t index);
int64_t bt_component_filter_get_output_port_count(
		struct bt_component *component);
struct bt_port *bt_component_filter_get_output_port_by_name(
		struct bt_component *component, const char *name);
struct bt_port *bt_component_filter_get_output_port_by_index(
		struct bt_component *component, uint64_t index);

/* Private filter component functions */
struct bt_private_port *
bt_private_component_filter_get_output_private_port_by_name(
		struct bt_private_component *private_component,
		const char *name);
struct bt_private_port *
bt_private_component_filter_get_output_private_port_by_index(
		struct bt_private_component *private_component, uint64_t index);
enum bt_component_status
bt_private_component_filter_add_output_private_port(
		struct bt_private_component *private_component,
		const char *name, void *user_data,
		struct bt_private_port **BTOUTPRIVPORT);
struct bt_private_port *
bt_private_component_filter_get_input_private_port_by_name(
		struct bt_private_component *private_component,
		const char *name);
struct bt_private_port *
bt_private_component_filter_get_input_private_port_by_index(
		struct bt_private_component *private_component, uint64_t index);
enum bt_component_status
bt_private_component_filter_add_input_private_port(
		struct bt_private_component *private_component,
		const char *name, void *user_data,
		struct bt_private_port **BTOUTPRIVPORT);

/* Sink component functions */
int64_t bt_component_sink_get_input_port_count(
		struct bt_component *component);
struct bt_port *bt_component_sink_get_input_port_by_name(
		struct bt_component *component, const char *name);
struct bt_port *bt_component_sink_get_input_port_by_index(
		struct bt_component *component, uint64_t index);

/* Private sink component functions */
struct bt_private_port *
bt_private_component_sink_get_input_private_port_by_name(
		struct bt_private_component *private_component,
		const char *name);
struct bt_private_port *
bt_private_component_sink_get_input_private_port_by_index(
		struct bt_private_component *private_component, uint64_t index);
enum bt_component_status
bt_private_component_sink_add_input_private_port(
		struct bt_private_component *private_component,
		const char *name, void *user_data,
		struct bt_private_port **BTOUTPRIVPORT);
