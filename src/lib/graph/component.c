/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright 2017-2018 Philippe Proulx <pproulx@efficios.com>
 * Copyright 2015 Jérémie Galarneau <jeremie.galarneau@efficios.com>
 */

#define BT_LOG_TAG "LIB/COMPONENT"
#include "lib/logging.h"

#include "common/common.h"
#include "common/assert.h"
#include "lib/assert-cond.h"
#include <babeltrace2/graph/self-component.h>
#include <babeltrace2/graph/component.h>
#include <babeltrace2/graph/graph.h>
#include "common/macros.h"
#include "compat/compiler.h"
#include <babeltrace2/types.h>
#include <babeltrace2/value.h>
#include <stdint.h>

#include "component.h"
#include "component-class.h"
#include "component-source.h"
#include "component-filter.h"
#include "component-sink.h"
#include "connection.h"
#include "graph.h"
#include "iterator.h"
#include "port.h"
#include "lib/func-status.h"

static
struct bt_component * (* const component_create_funcs[])(void) = {
	[BT_COMPONENT_CLASS_TYPE_SOURCE] = bt_component_source_create,
	[BT_COMPONENT_CLASS_TYPE_SINK] = bt_component_sink_create,
	[BT_COMPONENT_CLASS_TYPE_FILTER] = bt_component_filter_create,
};

static
void finalize_component(struct bt_component *comp)
{
	typedef void (*method_t)(void *);
	const char *method_name;

	method_t method = NULL;

	BT_ASSERT(comp);

	switch (comp->class->type) {
	case BT_COMPONENT_CLASS_TYPE_SOURCE:
	{
		struct bt_component_class_source *src_cc = (void *) comp->class;

		method = (method_t) src_cc->methods.finalize;
		method_name = "bt_component_class_source_finalize_method";
		break;
	}
	case BT_COMPONENT_CLASS_TYPE_FILTER:
	{
		struct bt_component_class_filter *flt_cc = (void *) comp->class;

		method = (method_t) flt_cc->methods.finalize;
		method_name = "bt_component_class_filter_finalize_method";
		break;
	}
	case BT_COMPONENT_CLASS_TYPE_SINK:
	{
		struct bt_component_class_sink *sink_cc = (void *) comp->class;

		method = (method_t) sink_cc->methods.finalize;
		method_name = "bt_component_class_sink_finalize_method";
		break;
	}
	default:
		bt_common_abort();
	}

	if (method) {
		const struct bt_error *saved_error;

		saved_error = bt_current_thread_take_error();

		BT_LIB_LOGI("Calling user's component finalization method: "
			"%![comp-]+c", comp);
		method(comp);
		BT_ASSERT_POST_NO_ERROR(method_name);

		if (saved_error) {
			BT_CURRENT_THREAD_MOVE_ERROR_AND_RESET(saved_error);
		}
	}
}

static
void destroy_component(struct bt_object *obj)
{
	struct bt_component *component = NULL;
	int i;

	if (!obj) {
		return;
	}

	/*
	 * The component's reference count is 0 if we're here. Increment
	 * it to avoid a double-destroy (possibly infinitely recursive).
	 * This could happen for example if the component's finalization
	 * function does bt_object_get_ref() (or anything that causes
	 * bt_object_get_ref() to be called) on itself (ref. count goes
	 * from 0 to 1), and then bt_object_put_ref(): the reference
	 * count would go from 1 to 0 again and this function would be
	 * called again.
	 */
	obj->ref_count++;
	component = container_of(obj, struct bt_component, base);
	BT_LIB_LOGI("Destroying component: %![comp-]+c, %![graph-]+g",
		component, bt_component_borrow_graph(component));

	/* Call destroy listeners in reverse registration order */
	BT_LOGD_STR("Calling destroy listeners.");

	for (i = component->destroy_listeners->len - 1; i >= 0; i--) {
		struct bt_component_destroy_listener *listener =
			&bt_g_array_index(component->destroy_listeners,
				struct bt_component_destroy_listener, i);

		listener->func(component, listener->data);
	}

	/*
	 * User data is destroyed first, followed by the concrete
	 * component instance. Do not finalize if the component's user
	 * initialization method failed in the first place.
	 */
	if (component->initialized) {
		finalize_component(component);
	}

	if (component->input_ports) {
		BT_LOGD_STR("Destroying input ports.");
		g_ptr_array_free(component->input_ports, TRUE);
		component->input_ports = NULL;
	}

	if (component->output_ports) {
		BT_LOGD_STR("Destroying output ports.");
		g_ptr_array_free(component->output_ports, TRUE);
		component->output_ports = NULL;
	}

	if (component->destroy_listeners) {
		g_array_free(component->destroy_listeners, TRUE);
		component->destroy_listeners = NULL;
	}

	if (component->name) {
		g_string_free(component->name, TRUE);
		component->name = NULL;
	}

	BT_LOGD_STR("Putting component class.");
	BT_OBJECT_PUT_REF_AND_RESET(component->class);
	g_free(component);
}

BT_EXPORT
enum bt_component_class_type bt_component_get_class_type(
		const struct bt_component *component)
{
	BT_ASSERT_PRE_DEV_COMP_NON_NULL(component);
	return component->class->type;
}

static
bool port_name_is_unique(GPtrArray *ports, const char *name)
{
	guint i;
	bool unique;

	for (i = 0; i < ports->len; i++) {
		struct bt_port *port = g_ptr_array_index(ports, i);

		if (strcmp(port->name->str, name) == 0) {
			unique = false;
			goto end;
		}
	}

	unique = true;

end:
	return unique;
}

static
enum bt_self_component_add_port_status add_port(
		struct bt_component *component, GPtrArray *ports,
		enum bt_port_type port_type, const char *name, void *user_data,
		struct bt_port **port, const char *api_func, bool input)
{
	struct bt_port *new_port = NULL;
	struct bt_graph *graph = NULL;
	enum bt_self_component_add_port_status status;

	BT_ASSERT_PRE_NO_ERROR_FROM_FUNC(api_func);
	BT_ASSERT_PRE_COMP_NON_NULL_FROM_FUNC(api_func, component);
	BT_ASSERT_PRE_NAME_NON_NULL_FROM_FUNC(api_func, name);
	BT_ASSERT_PRE_FROM_FUNC(api_func,
		input ? "input" : "output" "-port-name-is-unique",
		port_name_is_unique(component->output_ports, name),
		input ? "Input" : "Output"
			" port name is not unique: name=\"%s\", %![comp-]c",
		name, component);
	BT_ASSERT_PRE_FROM_FUNC(api_func, "name-is-not-empty",
		strlen(name) > 0, "Name is empty");
	graph = bt_component_borrow_graph(component);
	BT_ASSERT_PRE_FROM_FUNC(api_func, "graph-is-not-configured",
		graph->config_state == BT_GRAPH_CONFIGURATION_STATE_CONFIGURING,
		"Component's graph is already configured: "
		"%![comp-]+c, %![graph-]+g", component, graph);

	BT_LIB_LOGI("Adding port to component: %![comp-]+c, "
		"port-type=%s, port-name=\"%s\"", component,
		bt_port_type_string(port_type), name);

	new_port = bt_port_create(component, port_type, name, user_data);
	if (!new_port) {
		BT_LIB_LOGE_APPEND_CAUSE("Cannot create port object.");
		status = BT_FUNC_STATUS_MEMORY_ERROR;
		goto error;
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
	graph = bt_component_borrow_graph(component);
	if (graph) {
		enum bt_graph_listener_func_status listener_status;

		listener_status = bt_graph_notify_port_added(graph, new_port);
		if (listener_status != BT_FUNC_STATUS_OK) {
			bt_graph_make_faulty(graph);
			status = (int) listener_status;
			goto error;
		}
	}

	BT_LIB_LOGI("Created and added port to component: "
		"%![comp-]+c, %![port-]+p", component, new_port);

	*port = new_port;
	status = BT_FUNC_STATUS_OK;

	goto end;
error:
	/*
	 * We need to release the reference that we would otherwise have
	 * returned to the caller.
	 */
	BT_PORT_PUT_REF_AND_RESET(new_port);

end:
	return status;
}

uint64_t bt_component_get_input_port_count(const struct bt_component *comp,
		const char *api_func)
{
	BT_ASSERT_PRE_DEV_COMP_NON_NULL_FROM_FUNC(api_func, comp);
	return (uint64_t) comp->input_ports->len;
}

uint64_t bt_component_get_output_port_count(const struct bt_component *comp,
		const char *api_func)
{
	BT_ASSERT_PRE_DEV_COMP_NON_NULL_FROM_FUNC(api_func, comp);
	return (uint64_t) comp->output_ports->len;
}

int bt_component_create(struct bt_component_class *component_class,
		const char *name, bt_logging_level log_level,
		struct bt_component **user_component)
{
	int ret = 0;
	struct bt_component *component = NULL;
	enum bt_component_class_type type;

	BT_ASSERT(user_component);
	BT_ASSERT(component_class);
	BT_ASSERT(name);
	type = bt_component_class_get_type(component_class);
	BT_LIB_LOGI("Creating empty component from component class: %![cc-]+C, "
		"comp-name=\"%s\", log-level=%s", component_class, name,
		bt_common_logging_level_string(log_level));
	component = component_create_funcs[type]();
	if (!component) {
		BT_LIB_LOGE_APPEND_CAUSE(
			"Cannot create specific component object.");
		ret = -1;
		goto end;
	}

	bt_object_init_shared_with_parent(&component->base, destroy_component);
	component->class = component_class;
	bt_object_get_ref_no_null_check(component->class);
	component->name = g_string_new(name);
	if (!component->name) {
		BT_LIB_LOGE_APPEND_CAUSE("Failed to allocate one GString.");
		ret = -1;
		goto end;
	}

	component->log_level = log_level;
	component->input_ports = g_ptr_array_new_with_free_func(
		(GDestroyNotify) bt_object_try_spec_release);
	if (!component->input_ports) {
		BT_LIB_LOGE_APPEND_CAUSE("Failed to allocate one GPtrArray.");
		ret = -1;
		goto end;
	}

	component->output_ports = g_ptr_array_new_with_free_func(
		(GDestroyNotify) bt_object_try_spec_release);
	if (!component->output_ports) {
		BT_LIB_LOGE_APPEND_CAUSE("Failed to allocate one GPtrArray.");
		ret = -1;
		goto end;
	}

	component->destroy_listeners = g_array_new(FALSE, TRUE,
		sizeof(struct bt_component_destroy_listener));
	if (!component->destroy_listeners) {
		BT_LIB_LOGE_APPEND_CAUSE("Failed to allocate one GArray.");
		ret = -1;
		goto end;
	}

	BT_LIB_LOGI("Created empty component from component class: "
		"%![cc-]+C, %![comp-]+c", component_class, component);
	BT_OBJECT_MOVE_REF(*user_component, component);

end:
	bt_object_put_ref(component);
	return ret;
}

BT_EXPORT
const char *bt_component_get_name(const struct bt_component *component)
{
	BT_ASSERT_PRE_DEV_COMP_NON_NULL(component);
	return component->name->str;
}

BT_EXPORT
const struct bt_component_class *bt_component_borrow_class_const(
		const struct bt_component *component)
{
	BT_ASSERT_PRE_DEV_COMP_NON_NULL(component);
	return component->class;
}

BT_EXPORT
void *bt_self_component_get_data(const struct bt_self_component *self_comp)
{
	struct bt_component *component = (void *) self_comp;

	BT_ASSERT_PRE_DEV_COMP_NON_NULL(component);
	return component->user_data;
}

BT_EXPORT
void bt_self_component_set_data(struct bt_self_component *self_comp,
		void *data)
{
	struct bt_component *component = (void *) self_comp;

	BT_ASSERT_PRE_DEV_COMP_NON_NULL(component);
	component->user_data = data;
	BT_LIB_LOGD("Set component's user data: %!+c", component);
}

void bt_component_set_graph(struct bt_component *component,
		struct bt_graph *graph)
{
	bt_object_set_parent(&component->base,
		graph ? &graph->base : NULL);
}

static
struct bt_port *borrow_port_by_name(GPtrArray *ports,
		const char *name, const char *api_func)
{
	uint64_t i;
	struct bt_port *ret_port = NULL;

	BT_ASSERT_PRE_DEV_NAME_NON_NULL_FROM_FUNC(api_func, name);

	for (i = 0; i < ports->len; i++) {
		struct bt_port *port = g_ptr_array_index(ports, i);

		if (strcmp(name, port->name->str) == 0) {
			ret_port = port;
			break;
		}
	}

	return ret_port;
}

struct bt_port_input *bt_component_borrow_input_port_by_name(
		struct bt_component *comp, const char *name,
		const char *api_func)
{
	BT_ASSERT_PRE_DEV_COMP_NON_NULL_FROM_FUNC(api_func, comp);
	return (void *) borrow_port_by_name(comp->input_ports, name, api_func);
}

struct bt_port_output *bt_component_borrow_output_port_by_name(
		struct bt_component *comp, const char *name,
		const char *api_func)
{
	BT_ASSERT_PRE_DEV_COMP_NON_NULL_FROM_FUNC(api_func, comp);
	return (void *)
		borrow_port_by_name(comp->output_ports, name, api_func);
}

static
struct bt_port *borrow_port_by_index(GPtrArray *ports, uint64_t index,
		const char *api_func)
{
	BT_ASSERT_PRE_DEV_VALID_INDEX_FROM_FUNC(api_func, index, ports->len);
	return g_ptr_array_index(ports, index);
}

struct bt_port_input *bt_component_borrow_input_port_by_index(
		struct bt_component *comp, uint64_t index,
		const char *api_func)
{
	BT_ASSERT_PRE_DEV_COMP_NON_NULL_FROM_FUNC(api_func, comp);
	return (void *)
		borrow_port_by_index(comp->input_ports, index, api_func);
}

struct bt_port_output *bt_component_borrow_output_port_by_index(
		struct bt_component *comp, uint64_t index,
		const char *api_func)
{
	BT_ASSERT_PRE_DEV_COMP_NON_NULL_FROM_FUNC(api_func, comp);
	return (void *)
		borrow_port_by_index(comp->output_ports, index, api_func);
}

enum bt_self_component_add_port_status bt_component_add_input_port(
		struct bt_component *component, const char *name,
		void *user_data, struct bt_port **port, const char *api_func)
{
	/* add_port() logs details and checks preconditions */
	return add_port(component, component->input_ports,
		BT_PORT_TYPE_INPUT, name, user_data, port, api_func, true);
}

enum bt_self_component_add_port_status bt_component_add_output_port(
		struct bt_component *component, const char *name,
		void *user_data, struct bt_port **port,
		const char *api_func)
{
	/* add_port() logs details and checks preconditions */
	return add_port(component, component->output_ports,
		BT_PORT_TYPE_OUTPUT, name, user_data, port, api_func, false);
}

enum bt_component_class_port_connected_method_status
bt_component_port_connected(
		struct bt_component *comp, struct bt_port *self_port,
		struct bt_port *other_port)
{
	typedef enum bt_component_class_port_connected_method_status (*method_t)(
		void *, void *, const void *);

	enum bt_component_class_port_connected_method_status status =
		BT_FUNC_STATUS_OK;
	method_t method = NULL;
	const char *method_name = NULL;

	BT_ASSERT(comp);
	BT_ASSERT(self_port);
	BT_ASSERT(other_port);

	switch (comp->class->type) {
	case BT_COMPONENT_CLASS_TYPE_SOURCE:
	{
		struct bt_component_class_source *src_cc = (void *) comp->class;

		switch (self_port->type) {
		case BT_PORT_TYPE_OUTPUT:
			method = (method_t) src_cc->methods.output_port_connected;
			method_name = "bt_component_class_source_output_port_connected_method";
			break;
		default:
			bt_common_abort();
		}

		break;
	}
	case BT_COMPONENT_CLASS_TYPE_FILTER:
	{
		struct bt_component_class_filter *flt_cc = (void *) comp->class;

		switch (self_port->type) {
		case BT_PORT_TYPE_INPUT:
			method = (method_t) flt_cc->methods.input_port_connected;
			method_name = "bt_component_class_filter_input_port_connected_method";
			break;
		case BT_PORT_TYPE_OUTPUT:
			method = (method_t) flt_cc->methods.output_port_connected;
			method_name = "bt_component_class_filter_output_port_connected_method";
			break;
		default:
			bt_common_abort();
		}

		break;
	}
	case BT_COMPONENT_CLASS_TYPE_SINK:
	{
		struct bt_component_class_sink *sink_cc = (void *) comp->class;

		switch (self_port->type) {
		case BT_PORT_TYPE_INPUT:
			method = (method_t) sink_cc->methods.input_port_connected;
			method_name = "bt_component_class_sink_input_port_connected_method";
			break;
		default:
			bt_common_abort();
		}

		break;
	}
	default:
		bt_common_abort();
	}

	if (method) {
		BT_LIB_LOGD("Calling user's \"port connected\" method: "
			"%![comp-]+c, %![self-port-]+p, %![other-port-]+p",
			comp, self_port, other_port);
		status = (int) method(comp, self_port, (void *) other_port);
		BT_LOGD("User method returned: status=%s",
			bt_common_func_status_string(status));
		BT_ASSERT_POST(method_name, "valid-status",
			status == BT_FUNC_STATUS_OK ||
			status == BT_FUNC_STATUS_ERROR ||
			status == BT_FUNC_STATUS_MEMORY_ERROR,
			"Unexpected returned component status: status=%s",
			bt_common_func_status_string(status));
		BT_ASSERT_POST_NO_ERROR_IF_NO_ERROR_STATUS(method_name, status);
	}

	return status;
}

void bt_component_add_destroy_listener(struct bt_component *component,
		bt_component_destroy_listener_func func, void *data)
{
	struct bt_component_destroy_listener listener;

	BT_ASSERT(component);
	BT_ASSERT(func);
	listener.func = func;
	listener.data = data;
	g_array_append_val(component->destroy_listeners, listener);
	BT_LIB_LOGD("Added destroy listener: %![comp-]+c, "
		"func-addr=%p, data-addr=%p",
		component, func, data);
}

void bt_component_remove_destroy_listener(struct bt_component *component,
		bt_component_destroy_listener_func func, void *data)
{
	uint64_t i;

	BT_ASSERT(component);
	BT_ASSERT(func);

	for (i = 0; i < component->destroy_listeners->len; i++) {
		struct bt_component_destroy_listener *listener =
			&bt_g_array_index(component->destroy_listeners,
				struct bt_component_destroy_listener, i);

		if (listener->func == func && listener->data == data) {
			g_array_remove_index(component->destroy_listeners, i);
			i--;
			BT_LIB_LOGD("Removed destroy listener: %![comp-]+c, "
				"func-addr=%p, data-addr=%p",
				component, func, data);
		}
	}
}

BT_EXPORT
bt_logging_level bt_component_get_logging_level(
		const struct bt_component *component)
{
	BT_ASSERT_PRE_DEV_COMP_NON_NULL(component);
	return component->log_level;
}

BT_EXPORT
uint64_t bt_self_component_get_graph_mip_version(
		bt_self_component *self_component)
{
	struct bt_component *comp = (void *) self_component;

	BT_ASSERT_PRE_COMP_NON_NULL(self_component);
	return bt_component_borrow_graph(comp)->mip_version;
}

BT_EXPORT
void bt_component_get_ref(const struct bt_component *component)
{
	bt_object_get_ref(component);
}

BT_EXPORT
void bt_component_put_ref(const struct bt_component *component)
{
	bt_object_put_ref(component);
}
