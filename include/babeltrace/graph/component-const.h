#ifndef BABELTRACE_GRAPH_COMPONENT_CONST_H
#define BABELTRACE_GRAPH_COMPONENT_CONST_H

/*
 * Copyright 2017-2018 Philippe Proulx <pproulx@efficios.com>
 * Copyright 2015 Jérémie Galarneau <jeremie.galarneau@efficios.com>
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

/* For bt_component_class_type */
#include <babeltrace/graph/component-class-const.h>

/*
 * For bt_bool, bt_component_class, bt_component_graph, bt_component,
 * bt_value, bt_port
 */
#include <babeltrace/types.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Get component's name.
 *
 * @param component	Component instance of which to get the name
 * @returns		Returns a pointer to the component's name
 */
extern const char *bt_component_get_name(const bt_component *component);

/**
 * Get component's class.
 *
 * @param component	Component instance of which to get the class
 * @returns		The component's class
 */
extern const bt_component_class *bt_component_borrow_class_const(
		const bt_component *component);

extern bt_component_class_type bt_component_get_class_type(
		const bt_component *component);

static inline
bt_bool bt_component_is_source(const bt_component *component)
{
	return bt_component_get_class_type(component) ==
		BT_COMPONENT_CLASS_TYPE_SOURCE;
}

static inline
bt_bool bt_component_is_filter(const bt_component *component)
{
	return bt_component_get_class_type(component) ==
		BT_COMPONENT_CLASS_TYPE_FILTER;
}

static inline
bt_bool bt_component_is_sink(const bt_component *component)
{
	return bt_component_get_class_type(component) ==
		BT_COMPONENT_CLASS_TYPE_SINK;
}

extern bt_bool bt_component_graph_is_canceled(
		const bt_component *component);

extern void bt_component_get_ref(const bt_component *component);

extern void bt_component_put_ref(const bt_component *component);

#define BT_COMPONENT_PUT_REF_AND_RESET(_var)		\
	do {						\
		bt_component_put_ref(_var);		\
		(_var) = NULL;				\
	} while (0)

#define BT_COMPONENT_MOVE_REF(_var_dst, _var_src)	\
	do {						\
		bt_component_put_ref(_var_dst);		\
		(_var_dst) = (_var_src);		\
		(_var_src) = NULL;			\
	} while (0)

#ifdef __cplusplus
}
#endif

#endif /* BABELTRACE_GRAPH_COMPONENT_CONST_H */
