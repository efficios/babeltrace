#ifndef BABELTRACE_GRAPH_COMPONENT_CLASS_H
#define BABELTRACE_GRAPH_COMPONENT_CLASS_H

/*
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

/* For bt_bool */
#include <babeltrace/types.h>

#ifdef __cplusplus
extern "C" {
#endif

struct bt_component_class;

enum bt_component_class_type {
	BT_COMPONENT_CLASS_TYPE_SOURCE =	0,
	BT_COMPONENT_CLASS_TYPE_FILTER =	1,
	BT_COMPONENT_CLASS_TYPE_SINK =		2,
};

extern const char *bt_component_class_get_name(
		struct bt_component_class *component_class);

extern const char *bt_component_class_get_description(
		struct bt_component_class *component_class);

extern const char *bt_component_class_get_help(
		struct bt_component_class *component_class);

extern enum bt_component_class_type bt_component_class_get_type(
		struct bt_component_class *component_class);

static inline
bt_bool bt_component_class_is_source(struct bt_component_class *component_class)
{
	return bt_component_class_get_type(component_class) ==
		BT_COMPONENT_CLASS_TYPE_SOURCE;
}

static inline
bt_bool bt_component_class_is_filter(struct bt_component_class *component_class)
{
	return bt_component_class_get_type(component_class) ==
		BT_COMPONENT_CLASS_TYPE_FILTER;
}

static inline
bt_bool bt_component_class_is_sink(struct bt_component_class *component_class)
{
	return bt_component_class_get_type(component_class) ==
		BT_COMPONENT_CLASS_TYPE_SINK;
}

#ifdef __cplusplus
}
#endif

#endif /* BABELTRACE_GRAPH_COMPONENT_CLASS_H */
