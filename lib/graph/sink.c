/*
 * sink.c
 *
 * Babeltrace Sink Component
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

#define BT_LOG_TAG "COMP-SINK"
#include <babeltrace/lib-logging-internal.h>

#include <babeltrace/compiler-internal.h>
#include <babeltrace/values.h>
#include <babeltrace/graph/private-component.h>
#include <babeltrace/graph/component-sink-internal.h>
#include <babeltrace/graph/component-internal.h>
#include <babeltrace/graph/notification.h>
#include <babeltrace/graph/graph.h>
#include <babeltrace/assert-internal.h>
#include <babeltrace/assert-internal.h>

BT_HIDDEN
void bt_component_sink_destroy(struct bt_component *component)
{
}

BT_HIDDEN
struct bt_component *bt_component_sink_create(
		struct bt_component_class *class)
{
	struct bt_component_sink *sink = NULL;

	sink = g_new0(struct bt_component_sink, 1);
	if (!sink) {
		BT_LOGE_STR("Failed to allocate one sink component.");
		goto end;
	}

end:
	return sink ? &sink->parent : NULL;
}

int64_t bt_component_sink_get_input_port_count(struct bt_component *component)
{
	int64_t ret;

	if (!component) {
		BT_LOGW_STR("Invalid parameter: component is NULL.");
		ret = (int64_t) -1;
		goto end;
	}

	if (component->class->type != BT_COMPONENT_CLASS_TYPE_SINK) {
		BT_LOGW("Invalid parameter: component's class is not a sink component class: "
			"comp-addr=%p, comp-name=\"%s\", comp-class-type=%s",
			component, bt_component_get_name(component),
			bt_component_class_type_string(component->class->type));
		ret = (int64_t) -1;
		goto end;
	}

	/* bt_component_get_input_port_count() logs details/errors */
	ret = bt_component_get_input_port_count(component);

end:
	return ret;
}

struct bt_port *bt_component_sink_get_input_port_by_name(
		struct bt_component *component, const char *name)
{
	struct bt_port *port = NULL;

	if (!component) {
		BT_LOGW_STR("Invalid parameter: component is NULL.");
		goto end;
	}

	if (!name) {
		BT_LOGW_STR("Invalid parameter: name is NULL.");
		goto end;
	}

	if (component->class->type != BT_COMPONENT_CLASS_TYPE_SINK) {
		BT_LOGW("Invalid parameter: component's class is not a sink component class: "
			"comp-addr=%p, comp-name=\"%s\", comp-class-type=%s",
			component, bt_component_get_name(component),
			bt_component_class_type_string(component->class->type));
		goto end;
	}

	/* bt_component_get_input_port_by_name() logs details/errors */
	port = bt_component_get_input_port_by_name(component, name);

end:
	return port;
}

struct bt_port *bt_component_sink_get_input_port_by_index(
		struct bt_component *component, uint64_t index)
{
	struct bt_port *port = NULL;

	if (!component) {
		BT_LOGW_STR("Invalid parameter: component is NULL.");
		goto end;
	}

	if (component->class->type != BT_COMPONENT_CLASS_TYPE_SINK) {
		BT_LOGW("Invalid parameter: component's class is not a sink component class: "
			"comp-addr=%p, comp-name=\"%s\", comp-class-type=%s",
			component, bt_component_get_name(component),
			bt_component_class_type_string(component->class->type));
		goto end;
	}

	/* bt_component_get_input_port_by_index() logs details/errors */
	port = bt_component_get_input_port_by_index(component, index);

end:
	return port;
}

struct bt_private_port *
bt_private_component_sink_get_input_private_port_by_index(
		struct bt_private_component *private_component, uint64_t index)
{
	/* bt_component_sink_get_input_port_by_index() logs details/errors */
	return bt_private_port_from_port(
		bt_component_sink_get_input_port_by_index(
			bt_component_borrow_from_private(private_component), index));
}

struct bt_private_port *
bt_private_component_sink_get_input_private_port_by_name(
		struct bt_private_component *private_component,
		const char *name)
{
	/* bt_component_sink_get_input_port_by_name() logs details/errors */
	return bt_private_port_from_port(
		bt_component_sink_get_input_port_by_name(
			bt_component_borrow_from_private(private_component), name));
}

enum bt_component_status bt_private_component_sink_add_input_private_port(
		struct bt_private_component *private_component,
		const char *name, void *user_data,
		struct bt_private_port **user_priv_port)
{
	enum bt_component_status status = BT_COMPONENT_STATUS_OK;
	struct bt_port *port = NULL;
	struct bt_component *component =
		bt_component_borrow_from_private(private_component);
	struct bt_graph *graph;

	if (!component) {
		BT_LOGW_STR("Invalid parameter: component is NULL.");
		status = BT_COMPONENT_STATUS_INVALID;
		goto end;
	}

	if (component->class->type != BT_COMPONENT_CLASS_TYPE_SINK) {
		BT_LOGW("Invalid parameter: component's class is not a sink component class: "
			"comp-addr=%p, comp-name=\"%s\", comp-class-type=%s",
			component, bt_component_get_name(component),
			bt_component_class_type_string(component->class->type));
		status = BT_COMPONENT_STATUS_INVALID;
		goto end;
	}

	graph = bt_component_borrow_graph(component);

	if (graph && bt_graph_is_canceled(graph)) {
		BT_LOGW("Cannot add input port to sink component: graph is canceled: "
			"comp-addr=%p, comp-name=\"%s\", graph-addr=%p",
			component, bt_component_get_name(component),
			bt_component_borrow_graph(component));
		status = BT_COMPONENT_STATUS_GRAPH_IS_CANCELED;
		goto end;
	}

	/* bt_component_add_input_port() logs details/errors */
	port = bt_component_add_input_port(component, name, user_data);
	if (!port) {
		status = BT_COMPONENT_STATUS_NOMEM;
		goto end;
	}

	if (user_priv_port) {
		/* Move reference to user */
		*user_priv_port = bt_private_port_from_port(port);
		port = NULL;
	}

end:
	bt_object_put_ref(port);
	return status;
}
