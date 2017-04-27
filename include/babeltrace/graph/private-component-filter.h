#ifndef BABELTRACE_COMPONENT_PRIVATE_COMPONENT_SINK_H
#define BABELTRACE_COMPONENT_PRIVATE_COMPONENT_SINK_H

/*
 * Copyright 2017 Philippe Proulx <pproulx@efficios.com>
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
#include <babeltrace/graph/component.h>

#ifdef __cplusplus
extern "C" {
#endif

struct bt_private_component;
struct bt_private_port;

extern struct bt_private_port *
bt_private_component_filter_get_output_private_port_by_name(
		struct bt_private_component *private_component,
		const char *name);

extern struct bt_private_port *
bt_private_component_filter_get_output_private_port_by_index(
		struct bt_private_component *private_component, uint64_t index);

extern struct bt_private_port *
bt_private_component_filter_get_default_output_private_port(
		struct bt_private_component *private_component);

extern struct bt_private_port *
bt_private_component_filter_add_output_private_port(
		struct bt_private_component *private_component,
		const char *name, void *user_data);

extern struct bt_private_port *
bt_private_component_filter_get_input_private_port_by_name(
		struct bt_private_component *private_component,
		const char *name);

extern struct bt_private_port *
bt_private_component_filter_get_input_private_port_by_index(
		struct bt_private_component *private_component, uint64_t index);

extern struct bt_private_port *
bt_private_component_filter_get_default_input_private_port(
		struct bt_private_component *private_component);

extern struct bt_private_port *
bt_private_component_filter_add_input_private_port(
		struct bt_private_component *private_component,
		const char *name, void *user_data);

#ifdef __cplusplus
}
#endif

#endif /* BABELTRACE_COMPONENT_PRIVATE_COMPONENT_SINK_H */
