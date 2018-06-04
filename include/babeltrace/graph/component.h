#ifndef BABELTRACE_GRAPH_COMPONENT_H
#define BABELTRACE_GRAPH_COMPONENT_H

/*
 * BabelTrace - Babeltrace Component Interface
 *
 * Copyright 2015 Jérémie Galarneau <jeremie.galarneau@efficios.com>
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

/* For BT_COMPONENT_CLASS_TYPE_* */
#include <babeltrace/graph/component-class.h>

/* For bt_bool */
#include <babeltrace/types.h>

/* For bt_get */
#include <babeltrace/ref.h>

#ifdef __cplusplus
extern "C" {
#endif

struct bt_component_class;
struct bt_component_graph;
struct bt_component;
struct bt_value;
struct bt_port;

/**
 * Get component's name.
 *
 * @param component	Component instance of which to get the name
 * @returns		Returns a pointer to the component's name
 */
extern const char *bt_component_get_name(struct bt_component *component);

/**
 * Get component's class.
 *
 * @param component	Component instance of which to get the class
 * @returns		The component's class
 */
extern struct bt_component_class *bt_component_get_class(
		struct bt_component *component);

extern enum bt_component_class_type bt_component_get_class_type(
		struct bt_component *component);

static inline
bt_bool bt_component_is_source(struct bt_component *component)
{
	return bt_component_get_class_type(component) ==
		BT_COMPONENT_CLASS_TYPE_SOURCE;
}

static inline
bt_bool bt_component_is_filter(struct bt_component *component)
{
	return bt_component_get_class_type(component) ==
		BT_COMPONENT_CLASS_TYPE_FILTER;
}

static inline
bt_bool bt_component_is_sink(struct bt_component *component)
{
	return bt_component_get_class_type(component) ==
		BT_COMPONENT_CLASS_TYPE_SINK;
}

extern struct bt_graph *bt_component_borrow_graph(struct bt_component *component);

static inline
struct bt_graph *bt_component_get_graph(struct bt_component *component)
{
	return bt_get(bt_component_borrow_graph(component));
}

#ifdef __cplusplus
}
#endif

#endif /* BABELTRACE_GRAPH_COMPONENT_H */
