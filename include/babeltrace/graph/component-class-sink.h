#ifndef BABELTRACE_GRAPH_COMPONENT_CLASS_SINK_H
#define BABELTRACE_GRAPH_COMPONENT_CLASS_SINK_H

/*
 * Babeltrace - Component Class Interface.
 *
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

/* For component class method type definitions */
#include <babeltrace/graph/component-class.h>

#ifdef __cplusplus
extern "C" {
#endif

struct bt_component_class;
struct bt_private_component;

typedef enum bt_component_status (*bt_component_class_sink_consume_method)(
	struct bt_private_component *private_component);

extern
struct bt_component_class *bt_component_class_sink_create(const char *name,
		bt_component_class_sink_consume_method consume_method);

#ifdef __cplusplus
}
#endif

#endif /* BABELTRACE_GRAPH_COMPONENT_CLASS_SINK_H */
