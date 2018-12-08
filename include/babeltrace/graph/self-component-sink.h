#ifndef BABELTRACE_GRAPH_SELF_COMPONENT_SINK_H
#define BABELTRACE_GRAPH_SELF_COMPONENT_SINK_H

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

/* For bt_self_component_status */
#include <babeltrace/graph/self-component.h>

/*
 * For bt_component_sink, bt_self_component, bt_self_component_sink,
 * bt_self_component_port_input
 */
#include <babeltrace/types.h>

#ifdef __cplusplus
extern "C" {
#endif

static inline
bt_self_component *bt_self_component_sink_as_self_component(
		bt_self_component_sink *self_comp_sink)
{
	return (void *) self_comp_sink;
}

static inline
const bt_component_sink *
bt_self_component_sink_as_component_sink(
		bt_self_component_sink *self_comp_sink)
{
	return (const void *) self_comp_sink;
}

extern bt_self_component_port_input *
bt_self_component_sink_borrow_input_port_by_name(
		bt_self_component_sink *self_component,
		const char *name);

extern bt_self_component_port_input *
bt_self_component_sink_borrow_input_port_by_index(
		bt_self_component_sink *self_component, uint64_t index);

extern bt_self_component_status
bt_self_component_sink_add_input_port(
		bt_self_component_sink *self_component,
		const char *name, void *user_data,
		bt_self_component_port_input **self_component_port);

#ifdef __cplusplus
}
#endif

#endif /* BABELTRACE_GRAPH_SELF_COMPONENT_SINK_H */
