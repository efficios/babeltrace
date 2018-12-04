#ifndef BABELTRACE_GRAPH_COMPONENT_SOURCE_CONST_H
#define BABELTRACE_GRAPH_COMPONENT_SOURCE_CONST_H

/*
 * Copyright 2016 Jérémie Galarneau <jeremie.galarneau@efficios.com>
 *
 * Author: Jérémie Galarneau <jeremie.galarneau@efficios.com>
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

#ifdef __cplusplus
extern "C" {
#endif

struct bt_component;
struct bt_component_source;
struct bt_port_output;

static inline
const struct bt_component *bt_component_source_as_component_const(
		const struct bt_component_source *component)
{
	return (void *) component;
}

extern uint64_t bt_component_source_get_output_port_count(
		const struct bt_component_source *component);

extern const struct bt_port_output *
bt_component_source_borrow_output_port_by_name_const(
		const struct bt_component_source *component, const char *name);

extern const struct bt_port_output *
bt_component_source_borrow_output_port_by_index_const(
		const struct bt_component_source *component, uint64_t index);

#ifdef __cplusplus
}
#endif

#endif /* BABELTRACE_GRAPH_COMPONENT_SOURCE_CONST_H */
