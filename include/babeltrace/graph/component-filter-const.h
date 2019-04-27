#ifndef BABELTRACE_GRAPH_COMPONENT_FILTER_CONST_H
#define BABELTRACE_GRAPH_COMPONENT_FILTER_CONST_H

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

/*
 * For bt_component, bt_component_filter, bt_port_input, bt_port_output,
 * __BT_UPCAST_CONST
 */
#include <babeltrace/types.h>

#ifdef __cplusplus
extern "C" {
#endif

static inline
const bt_component *bt_component_filter_as_component_const(
		const bt_component_filter *component)
{
	return __BT_UPCAST_CONST(bt_component, component);
}

extern uint64_t bt_component_filter_get_input_port_count(
		const bt_component_filter *component);

extern const bt_port_input *
bt_component_filter_borrow_input_port_by_name_const(
		const bt_component_filter *component, const char *name);

extern const bt_port_input *
bt_component_filter_borrow_input_port_by_index_const(
		const bt_component_filter *component, uint64_t index);

extern uint64_t bt_component_filter_get_output_port_count(
		const bt_component_filter *component);

extern const bt_port_output *
bt_component_filter_borrow_output_port_by_name_const(
		const bt_component_filter *component, const char *name);

extern const bt_port_output *
bt_component_filter_borrow_output_port_by_index_const(
		const bt_component_filter *component, uint64_t index);

extern void bt_component_filter_get_ref(
		const bt_component_filter *component_filter);

extern void bt_component_filter_put_ref(
		const bt_component_filter *component_filter);

#define BT_COMPONENT_FILTER_PUT_REF_AND_RESET(_var)		\
	do {							\
		bt_component_filter_put_ref(_var);		\
		(_var) = NULL;					\
	} while (0)

#define BT_COMPONENT_FILTER_MOVE_REF(_var_dst, _var_src)	\
	do {							\
		bt_component_filter_put_ref(_var_dst);		\
		(_var_dst) = (_var_src);			\
		(_var_src) = NULL;				\
	} while (0)

#ifdef __cplusplus
}
#endif

#endif /* BABELTRACE_GRAPH_COMPONENT_FILTER_CONST_H */
