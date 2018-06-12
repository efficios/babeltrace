#ifndef BABELTRACE_GRAPH_COMPONENT_SINK_INTERNAL_H
#define BABELTRACE_GRAPH_COMPONENT_SINK_INTERNAL_H

/*
 * BabelTrace - Sink Component internal
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

#include <babeltrace/babeltrace-internal.h>
#include <babeltrace/compiler-internal.h>
#include <babeltrace/graph/component-sink.h>
#include <babeltrace/graph/component-internal.h>
#include <babeltrace/graph/component-class-internal.h>

struct bt_value;

//typedef uint32_t notification_mask_t;

struct bt_component_sink {
	struct bt_component parent;
};

/**
 * Allocate a sink component.
 *
 * @param class			Component class
 * @param params		A dictionary of component parameters
 * @returns			A sink component instance
 */
BT_HIDDEN
struct bt_component *bt_component_sink_create(
		struct bt_component_class *class);

BT_HIDDEN
void bt_component_sink_destroy(struct bt_component *component);

static inline
enum bt_component_status bt_component_sink_consume(
		struct bt_component *component)
{
	enum bt_component_status ret = BT_COMPONENT_STATUS_OK;
	struct bt_component_class_sink *sink_class = NULL;

	BT_ASSERT(component);
	BT_ASSERT(bt_component_get_class_type(component) ==
		BT_COMPONENT_CLASS_TYPE_SINK);
	sink_class = container_of(component->class,
		struct bt_component_class_sink, parent);
	BT_ASSERT(sink_class->methods.consume);
	BT_LOGD("Calling user's consume method: "
		"comp-addr=%p, comp-name=\"%s\"",
		component, bt_component_get_name(component));
	ret = sink_class->methods.consume(
		bt_private_component_from_component(component));
	BT_LOGD("User method returned: status=%s",
		bt_component_status_string(ret));
	if (ret < 0) {
		BT_LOGW_STR("Consume method failed.");
	}

	return ret;
}

#endif /* BABELTRACE_GRAPH_COMPONENT_SINK_INTERNAL_H */
