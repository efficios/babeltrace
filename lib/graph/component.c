/*
 * component.c
 *
 * Babeltrace Plugin Component
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

#define BT_LOG_TAG "COMP"
#include <babeltrace/lib-logging-internal.h>

#include <babeltrace/graph/private-component.h>
#include <babeltrace/graph/component.h>
#include <babeltrace/graph/component-internal.h>
#include <babeltrace/graph/component-class-internal.h>
#include <babeltrace/graph/component-source-internal.h>
#include <babeltrace/graph/component-filter-internal.h>
#include <babeltrace/graph/component-sink-internal.h>
#include <babeltrace/graph/private-connection.h>
#include <babeltrace/graph/connection-internal.h>
#include <babeltrace/graph/graph-internal.h>
#include <babeltrace/graph/notification-iterator-internal.h>
#include <babeltrace/graph/private-connection-private-notification-iterator.h>
#include <babeltrace/babeltrace-internal.h>
#include <babeltrace/compiler-internal.h>
#include <babeltrace/ref.h>
#include <babeltrace/types.h>
#include <babeltrace/values.h>
#include <babeltrace/values-internal.h>
#include <babeltrace/assert-internal.h>
#include <stdint.h>
#include <inttypes.h>

static
struct bt_component * (* const component_create_funcs[])(
		struct bt_component_class *) = {
	[BT_COMPONENT_CLASS_TYPE_SOURCE] = bt_component_source_create,
	[BT_COMPONENT_CLASS_TYPE_SINK] = bt_component_sink_create,
	[BT_COMPONENT_CLASS_TYPE_FILTER] = bt_component_filter_create,
};

static
void (*component_destroy_funcs[])(struct bt_component *) = {
	[BT_COMPONENT_CLASS_TYPE_SOURCE] = bt_component_source_destroy,
	[BT_COMPONENT_CLASS_TYPE_SINK] = bt_component_sink_destroy,
	[BT_COMPONENT_CLASS_TYPE_FILTER] = bt_component_filter_destroy,
};

static
void bt_component_destroy(struct bt_object *obj)
{
	struct bt_component *component = NULL;
	struct bt_component_class *component_class = NULL;
	int i;

	if (!obj) {
		return;
	}

	/*
	 * The component's reference count is 0 if we're here. Increment
	 * it to avoid a double-destroy (possibly infinitely recursive).
	 * This could happen for example if the component's finalization
	 * function does bt_get() (or anything that causes bt_get() to
	 * be called) on itself (ref. count goes from 0 to 1), and then
	 * bt_put(): the reference count would go from 1 to 0 again and
	 * this function would be called again.
	 */
	obj->ref_count.count++;
	component = container_of(obj, struct bt_component, base);
	BT_LOGD("Destroying component: addr=%p, name=\"%s\", graph-addr=%p",
		component, bt_component_get_name(component),
		obj->parent);

	/* Call destroy listeners in reverse registration order */
	BT_LOGD_STR("Calling destroy listeners.");

	for (i = component->destroy_listeners->len - 1; i >= 0; i--) {
		struct bt_component_destroy_listener *listener =
			&g_array_index(component->destroy_listeners,
				struct bt_component_destroy_listener, i);

		listener->func(component, listener->data);
	}

	component_class = component->class;

	/*
	 * User data is destroyed first, followed by the concrete
	 * component instance. Do not finalize if the component's user
	 * initialization method failed in the first place.
	 */
	if (component->initialized && component->class->methods.finalize) {
		BT_LOGD_STR("Calling user's finalization method.");
		component->class->methods.finalize(
			bt_private_component_from_component(component));
	}

	if (component->destroy) {
		BT_LOGD_STR("Destroying type-specific data.");
		component->destroy(component);
	}

	if (component->input_ports) {
		BT_LOGD_STR("Destroying input ports.");
		g_ptr_array_free(component->input_ports, TRUE);
	}

	if (component->output_ports) {
		BT_LOGD_STR("Destroying output ports.");
		g_ptr_array_free(component->output_ports, TRUE);
	}

	if (component->destroy_listeners) {
		g_array_free(component->destroy_listeners, TRUE);
	}

	if (component->name) {
		g_string_free(component->name, TRUE);
	}

	BT_LOGD("Putting component class.");
	bt_put(component_class);
	g_free(component);
}

struct bt_component *bt_component_borrow_from_private(
		struct bt_private_component *private_component)
{
	return (void *) private_component;
}

enum bt_component_class_type bt_component_get_class_type(
		struct bt_component *component)
{
	return component ? component->class->type : BT_COMPONENT_CLASS_TYPE_UNKNOWN;
}

static
struct bt_port *bt_component_add_port(
		struct bt_component *component, GPtrArray *ports,
		enum bt_port_type port_type, const char *name, void *user_data)
{
	size_t i;
	struct bt_port *new_port = NULL;
	struct bt_graph *graph = NULL;

	if (!name) {
		BT_LOGW_STR("Invalid parameter: name is NULL.");
		goto end;
	}

	if (strlen(name) == 0) {
		BT_LOGW_STR("Invalid parameter: name is an empty string.");
		goto end;
	}

	BT_LOGD("Adding port to component: comp-addr=%p, comp-name=\"%s\", "
		"port-type=%s, port-name=\"%s\"", component,
		bt_component_get_name(component),
		bt_port_type_string(port_type), name);

	/* Look for a port having the same name. */
	for (i = 0; i < ports->len; i++) {
		const char *port_name;
		struct bt_port *port = g_ptr_array_index(ports, i);

		port_name = bt_port_get_name(port);
		BT_ASSERT(port_name);

		if (!strcmp(name, port_name)) {
			/* Port name clash, abort. */
			BT_LOGW("Invalid parameter: another port with the same name already exists in the component: "
				"other-port-addr=%p", port);
			goto end;
		}
	}

	new_port = bt_port_create(component, port_type, name, user_data);
	if (!new_port) {
		BT_LOGE("Cannot create port object.");
		goto end;
	}

	/*
	 * No name clash, add the port.
	 * The component is now the port's parent; it should _not_
	 * hold a reference to the port since the port's lifetime
	 * is now protected by the component's own lifetime.
	 */
	g_ptr_array_add(ports, new_port);

	/*
	 * Notify the graph's creator that a new port was added.
	 */
	graph = bt_component_get_graph(component);
	if (graph) {
		bt_graph_notify_port_added(graph, new_port);
		BT_PUT(graph);
	}

	BT_LOGD("Created and added port to component: comp-addr=%p, comp-name=\"%s\", "
		"port-type=%s, port-name=\"%s\", port-addr=%p", component,
		bt_component_get_name(component),
		bt_port_type_string(port_type), name, new_port);

end:
	return new_port;
}

BT_HIDDEN
int64_t bt_component_get_input_port_count(struct bt_component *comp)
{
	BT_ASSERT(comp);
	return (int64_t) comp->input_ports->len;
}

BT_HIDDEN
int64_t bt_component_get_output_port_count(struct bt_component *comp)
{
	BT_ASSERT(comp);
	return (int64_t) comp->output_ports->len;
}

BT_HIDDEN
enum bt_component_status bt_component_create(
		struct bt_component_class *component_class,
		const char *name, struct bt_component **user_component)
{
	enum bt_component_status status = BT_COMPONENT_STATUS_OK;
	struct bt_component *component = NULL;
	enum bt_component_class_type type;

	BT_ASSERT(user_component);
	BT_ASSERT(component_class);
	BT_ASSERT(name);

	type = bt_component_class_get_type(component_class);
	BT_LOGD("Creating empty component from component class: "
		"comp-cls-addr=%p, comp-cls-type=%s, name=\"%s\"",
		component_class, bt_component_class_type_string(type), name);
	component = component_create_funcs[type](component_class);
	if (!component) {
		BT_LOGE_STR("Cannot create specific component object.");
		status = BT_COMPONENT_STATUS_NOMEM;
		goto end;
	}

	bt_object_init(component, bt_component_destroy);
	component->class = bt_get(component_class);
	component->destroy = component_destroy_funcs[type];
	component->name = g_string_new(name);
	if (!component->name) {
		BT_LOGE_STR("Failed to allocate one GString.");
		status = BT_COMPONENT_STATUS_NOMEM;
		goto end;
	}

	component->input_ports = g_ptr_array_new_with_free_func(
		bt_object_release);
	if (!component->input_ports) {
		BT_LOGE_STR("Failed to allocate one GPtrArray.");
		status = BT_COMPONENT_STATUS_NOMEM;
		goto end;
	}

	component->output_ports = g_ptr_array_new_with_free_func(
		bt_object_release);
	if (!component->output_ports) {
		BT_LOGE_STR("Failed to allocate one GPtrArray.");
		status = BT_COMPONENT_STATUS_NOMEM;
		goto end;
	}

	component->destroy_listeners = g_array_new(FALSE, TRUE,
		sizeof(struct bt_component_destroy_listener));
	if (!component->destroy_listeners) {
		BT_LOGE_STR("Failed to allocate one GArray.");
		status = BT_COMPONENT_STATUS_NOMEM;
		goto end;
	}

	BT_LOGD("Created empty component from component class: "
		"comp-cls-addr=%p, comp-cls-type=%s, name=\"%s\", comp-addr=%p",
		component_class, bt_component_class_type_string(type), name,
		component);
	BT_MOVE(*user_component, component);

end:
	bt_put(component);
	return status;
}

const char *bt_component_get_name(struct bt_component *component)
{
	const char *ret = NULL;

	if (!component) {
		BT_LOGW_STR("Invalid parameter: component is NULL.");
		goto end;
	}

	ret = component->name->len == 0 ? NULL : component->name->str;

end:
	return ret;
}

struct bt_component_class *bt_component_get_class(
		struct bt_component *component)
{
	return component ? bt_get(component->class) : NULL;
}

void *bt_private_component_get_user_data(
		struct bt_private_component *private_component)
{
	struct bt_component *component =
		bt_component_borrow_from_private(private_component);

	return component ? component->user_data : NULL;
}

enum bt_component_status bt_private_component_set_user_data(
		struct bt_private_component *private_component,
		void *data)
{
	struct bt_component *component =
		bt_component_borrow_from_private(private_component);
	enum bt_component_status ret = BT_COMPONENT_STATUS_OK;

	if (!component) {
		BT_LOGW_STR("Invalid parameter: component is NULL.");
		ret = BT_COMPONENT_STATUS_INVALID;
		goto end;
	}

	component->user_data = data;
	BT_LOGV("Set component's user data: "
		"comp-addr=%p, comp-name=\"%s\", user-data-addr=%p",
		component, bt_component_get_name(component), data);

end:
	return ret;
}

BT_HIDDEN
void bt_component_set_graph(struct bt_component *component,
		struct bt_graph *graph)
{
	bt_object_set_parent(component, graph ? &graph->base : NULL);
}

struct bt_graph *bt_component_borrow_graph(struct bt_component *component)
{
	return (struct bt_graph *) bt_object_borrow_parent(&component->base);
}

static
struct bt_port *bt_component_get_port_by_name(GPtrArray *ports,
		const char *name)
{
	size_t i;
	struct bt_port *ret_port = NULL;

	BT_ASSERT(name);

	for (i = 0; i < ports->len; i++) {
		struct bt_port *port = g_ptr_array_index(ports, i);
		const char *port_name = bt_port_get_name(port);

		if (!port_name) {
			continue;
		}

		if (!strcmp(name, port_name)) {
			ret_port = bt_get(port);
			break;
		}
	}

	return ret_port;
}

BT_HIDDEN
struct bt_port *bt_component_get_input_port_by_name(struct bt_component *comp,
		const char *name)
{
	BT_ASSERT(comp);

	return bt_component_get_port_by_name(comp->input_ports, name);
}

BT_HIDDEN
struct bt_port *bt_component_get_output_port_by_name(struct bt_component *comp,
		const char *name)
{
	BT_ASSERT(comp);

	return bt_component_get_port_by_name(comp->output_ports, name);
}

static
struct bt_port *bt_component_get_port_by_index(GPtrArray *ports, uint64_t index)
{
	struct bt_port *port = NULL;

	if (index >= ports->len) {
		BT_LOGW("Invalid parameter: index is out of bounds: "
			"index=%" PRIu64 ", count=%u",
			index, ports->len);
		goto end;
	}

	port = bt_get(g_ptr_array_index(ports, index));
end:
	return port;
}

BT_HIDDEN
struct bt_port *bt_component_get_input_port_by_index(struct bt_component *comp,
		uint64_t index)
{
	BT_ASSERT(comp);

	return bt_component_get_port_by_index(comp->input_ports, index);
}

BT_HIDDEN
struct bt_port *bt_component_get_output_port_by_index(struct bt_component *comp,
		uint64_t index)
{
	BT_ASSERT(comp);

	return bt_component_get_port_by_index(comp->output_ports, index);
}

BT_HIDDEN
struct bt_port *bt_component_add_input_port(
		struct bt_component *component, const char *name,
		void *user_data)
{
	/* bt_component_add_port() logs details */
	return bt_component_add_port(component, component->input_ports,
		BT_PORT_TYPE_INPUT, name, user_data);
}

BT_HIDDEN
struct bt_port *bt_component_add_output_port(
		struct bt_component *component, const char *name,
		void *user_data)
{
	/* bt_component_add_port() logs details */
	return bt_component_add_port(component, component->output_ports,
		BT_PORT_TYPE_OUTPUT, name, user_data);
}

static
void bt_component_remove_port_by_index(struct bt_component *component,
		GPtrArray *ports, size_t index)
{
	struct bt_port *port;
	struct bt_graph *graph;

	BT_ASSERT(ports);
	BT_ASSERT(index < ports->len);
	port = g_ptr_array_index(ports, index);

	BT_LOGD("Removing port from component: "
		"comp-addr=%p, comp-name=\"%s\", "
		"port-addr=%p, port-name=\"%s\"",
		component, bt_component_get_name(component),
		port, bt_port_get_name(port));

	/* Disconnect both ports of this port's connection, if any */
	if (port->connection) {
		bt_connection_end(port->connection, true);
	}

	/* Remove from parent's array of ports (weak refs) */
	g_ptr_array_remove_index(ports, index);

	/* Detach port from its component parent */
	BT_PUT(port->base.parent);

	/*
	 * Notify the graph's creator that a port is removed.
	 */
	graph = bt_component_get_graph(component);
	if (graph) {
		bt_graph_notify_port_removed(graph, component, port);
		BT_PUT(graph);
	}

	BT_LOGD("Removed port from component: "
		"comp-addr=%p, comp-name=\"%s\", "
		"port-addr=%p, port-name=\"%s\"",
		component, bt_component_get_name(component),
		port, bt_port_get_name(port));
}

BT_HIDDEN
enum bt_component_status bt_component_remove_port(
		struct bt_component *component, struct bt_port *port)
{
	size_t i;
	enum bt_component_status status = BT_COMPONENT_STATUS_OK;
	GPtrArray *ports = NULL;

	if (!component) {
		BT_LOGW_STR("Invalid parameter: component is NULL.");
		status = BT_COMPONENT_STATUS_INVALID;
		goto end;
	}

	if (!port) {
		BT_LOGW_STR("Invalid parameter: port is NULL.");
		status = BT_COMPONENT_STATUS_INVALID;
		goto end;
	}

	if (bt_port_get_type(port) == BT_PORT_TYPE_INPUT) {
		ports = component->input_ports;
	} else if (bt_port_get_type(port) == BT_PORT_TYPE_OUTPUT) {
		ports = component->output_ports;
	}

	BT_ASSERT(ports);

	for (i = 0; i < ports->len; i++) {
		struct bt_port *cur_port = g_ptr_array_index(ports, i);

		if (cur_port == port) {
			bt_component_remove_port_by_index(component,
				ports, i);
			goto end;
		}
	}

	status = BT_COMPONENT_STATUS_NOT_FOUND;
	BT_LOGW("Port to remove from component was not found: "
		"comp-addr=%p, comp-name=\"%s\", "
		"port-addr=%p, port-name=\"%s\"",
		component, bt_component_get_name(component),
		port, bt_port_get_name(port));

end:
	return status;
}

BT_HIDDEN
enum bt_component_status bt_component_accept_port_connection(
		struct bt_component *comp, struct bt_port *self_port,
		struct bt_port *other_port)
{
	enum bt_component_status status = BT_COMPONENT_STATUS_OK;

	BT_ASSERT(comp);
	BT_ASSERT(self_port);
	BT_ASSERT(other_port);

	if (comp->class->methods.accept_port_connection) {
		BT_LOGD("Calling user's \"accept port connection\" method: "
			"comp-addr=%p, comp-name=\"%s\", "
			"self-port-addr=%p, self-port-name=\"%s\", "
			"other-port-addr=%p, other-port-name=\"%s\"",
			comp, bt_component_get_name(comp),
			self_port, bt_port_get_name(self_port),
			other_port, bt_port_get_name(other_port));
		status = comp->class->methods.accept_port_connection(
			bt_private_component_from_component(comp),
			bt_private_port_from_port(self_port),
			other_port);
		BT_LOGD("User method returned: status=%s",
			bt_component_status_string(status));
	}

	return status;
}

BT_HIDDEN
void bt_component_port_connected(struct bt_component *comp,
		struct bt_port *self_port, struct bt_port *other_port)
{
	BT_ASSERT(comp);
	BT_ASSERT(self_port);
	BT_ASSERT(other_port);

	if (comp->class->methods.port_connected) {
		BT_LOGD("Calling user's \"port connected\" method: "
			"comp-addr=%p, comp-name=\"%s\", "
			"self-port-addr=%p, self-port-name=\"%s\", "
			"other-port-addr=%p, other-port-name=\"%s\"",
			comp, bt_component_get_name(comp),
			self_port, bt_port_get_name(self_port),
			other_port, bt_port_get_name(other_port));
		comp->class->methods.port_connected(
			bt_private_component_from_component(comp),
			bt_private_port_from_port(self_port), other_port);
	}
}

BT_HIDDEN
void bt_component_port_disconnected(struct bt_component *comp,
		struct bt_port *port)
{
	BT_ASSERT(comp);
	BT_ASSERT(port);

	if (comp->class->methods.port_disconnected) {
		BT_LOGD("Calling user's \"port disconnected\" method: "
			"comp-addr=%p, comp-name=\"%s\", "
			"port-addr=%p, port-name=\"%s\"",
			comp, bt_component_get_name(comp),
			port, bt_port_get_name(port));
		comp->class->methods.port_disconnected(
			bt_private_component_from_component(comp),
			bt_private_port_from_port(port));
	}
}

BT_HIDDEN
void bt_component_add_destroy_listener(struct bt_component *component,
		bt_component_destroy_listener_func func, void *data)
{
	struct bt_component_destroy_listener listener;

	BT_ASSERT(component);
	BT_ASSERT(func);
	listener.func = func;
	listener.data = data;
	g_array_append_val(component->destroy_listeners, listener);
	BT_LOGV("Added destroy listener: "
		"comp-addr=%p, comp-name=\"%s\", "
		"func-addr=%p, data-addr=%p",
		component, bt_component_get_name(component),
		func, data);
}

BT_HIDDEN
void bt_component_remove_destroy_listener(struct bt_component *component,
		bt_component_destroy_listener_func func, void *data)
{
	size_t i;

	BT_ASSERT(component);
	BT_ASSERT(func);

	for (i = 0; i < component->destroy_listeners->len; i++) {
		struct bt_component_destroy_listener *listener =
			&g_array_index(component->destroy_listeners,
				struct bt_component_destroy_listener, i);

		if (listener->func == func && listener->data == data) {
			g_array_remove_index(component->destroy_listeners, i);
			i--;
			BT_LOGV("Removed destroy listener: "
				"comp-addr=%p, comp-name=\"%s\", "
				"func-addr=%p, data-addr=%p",
				component, bt_component_get_name(component),
				func, data);
		}
	}
}
