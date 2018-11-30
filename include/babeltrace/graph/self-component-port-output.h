#ifndef BABELTRACE_GRAPH_SELF_COMPONENT_PORT_OUTPUT_H
#define BABELTRACE_GRAPH_SELF_COMPONENT_PORT_OUTPUT_H

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

/* For enum bt_self_component_port_status */
#include <babeltrace/graph/self-component-port.h>

#ifdef __cplusplus
extern "C" {
#endif

struct bt_port_output;
struct bt_self_component_port;
struct bt_self_component_port_output;

static inline
struct bt_self_component_port *
bt_self_component_port_output_as_self_component_port(
		struct bt_self_component_port_output *self_component_port)
{
	return (void *) self_component_port;
}

static inline
struct bt_port_output *bt_self_component_port_output_as_port_output(
		struct bt_self_component_port_output *self_component_port)
{
	return (void *) self_component_port;
}

#ifdef __cplusplus
}
#endif

#endif /* BABELTRACE_GRAPH_SELF_COMPONENT_PORT_OUTPUT_H */
