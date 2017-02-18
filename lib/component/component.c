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

#include <babeltrace/component/component.h>
#include <babeltrace/component/component-internal.h>
#include <babeltrace/component/component-class-internal.h>
#include <babeltrace/component/component-source-internal.h>
#include <babeltrace/component/component-filter-internal.h>
#include <babeltrace/component/component-sink-internal.h>
#include <babeltrace/component/component-graph-internal.h>
#include <babeltrace/component/notification/iterator-internal.h>
#include <babeltrace/babeltrace-internal.h>
#include <babeltrace/compiler.h>
#include <babeltrace/ref.h>

static
struct bt_component * (* const component_create_funcs[])(
		struct bt_component_class *, struct bt_value *) = {
	[BT_COMPONENT_CLASS_TYPE_SOURCE] = bt_component_source_create,
	[BT_COMPONENT_CLASS_TYPE_SINK] = bt_component_sink_create,
	[BT_COMPONENT_CLASS_TYPE_FILTER] = bt_component_filter_create,
};

static
enum bt_component_status (* const component_validation_funcs[])(
		struct bt_component *) = {
	[BT_COMPONENT_CLASS_TYPE_SOURCE] = bt_component_source_validate,
	[BT_COMPONENT_CLASS_TYPE_SINK] = bt_component_sink_validate,
	[BT_COMPONENT_CLASS_TYPE_FILTER] = bt_component_filter_validate,
};

static
void bt_component_destroy(struct bt_object *obj)
{
	struct bt_component *component = NULL;
	struct bt_component_class *component_class = NULL;

	if (!obj) {
		return;
	}

	component = container_of(obj, struct bt_component, base);
	component_class = component->class;

	/*
	 * User data is destroyed first, followed by the concrete component
	 * instance.
	 */
	if (component->class->methods.destroy) {
		component->class->methods.destroy(component);
	}

	if (component->destroy) {
		component->destroy(component);
	}

	g_string_free(component->name, TRUE);
	bt_put(component_class);
	g_free(component);
}

BT_HIDDEN
enum bt_component_status bt_component_init(struct bt_component *component,
		bt_component_class_destroy_method destroy)
{
	enum bt_component_status ret = BT_COMPONENT_STATUS_OK;

	if (!component) {
		ret = BT_COMPONENT_STATUS_INVALID;
		goto end;
	}

	component->initializing = true;
	component->destroy = destroy;
end:
	return ret;
}

enum bt_component_class_type bt_component_get_class_type(
		struct bt_component *component)
{
	return component ? component->class->type : BT_COMPONENT_CLASS_TYPE_UNKNOWN;
}

BT_HIDDEN
struct bt_notification_iterator *bt_component_create_iterator(
		struct bt_component *component, void *init_method_data)
{
	enum bt_notification_iterator_status ret_iterator;
	enum bt_component_class_type type;
	struct bt_notification_iterator *iterator = NULL;
	struct bt_component_class *class = component->class;

	if (!component) {
		goto error;
	}

	type = bt_component_get_class_type(component);
	if (type != BT_COMPONENT_CLASS_TYPE_SOURCE &&
			type != BT_COMPONENT_CLASS_TYPE_FILTER) {
		/* Unsupported operation. */
		goto error;
	}

	iterator = bt_notification_iterator_create(component);
	if (!iterator) {
		goto error;
	}

	switch (type) {
	case BT_COMPONENT_CLASS_TYPE_SOURCE:
	{
		struct bt_component_class_source *source_class;
		enum bt_notification_iterator_status status;

		source_class = container_of(class, struct bt_component_class_source, parent);

		if (source_class->methods.iterator.init) {
			status = source_class->methods.iterator.init(component,
					iterator, init_method_data);
			if (status < 0) {
				goto error;
			}
		}
		break;
	}
	case BT_COMPONENT_CLASS_TYPE_FILTER:
	{
		struct bt_component_class_filter *filter_class;
		enum bt_notification_iterator_status status;

		filter_class = container_of(class, struct bt_component_class_filter, parent);

		if (filter_class->methods.iterator.init) {
			status = filter_class->methods.iterator.init(component,
					iterator, init_method_data);
			if (status < 0) {
				goto error;
			}
		}
		break;
	}
	default:
		/* Unreachable. */
		assert(0);
	}

	ret_iterator = bt_notification_iterator_validate(iterator);
	if (ret_iterator != BT_NOTIFICATION_ITERATOR_STATUS_OK) {
		goto error;
	}

	return iterator;
error:
	BT_PUT(iterator);
	return iterator;
}

struct bt_component *bt_component_create_with_init_method_data(
		struct bt_component_class *component_class, const char *name,
		struct bt_value *params, void *init_method_data)
{
	int ret;
	struct bt_component *component = NULL;
	enum bt_component_class_type type;

	if (!component_class) {
		goto end;
	}

	type = bt_component_class_get_type(component_class);
	if (type <= BT_COMPONENT_CLASS_TYPE_UNKNOWN ||
			type > BT_COMPONENT_CLASS_TYPE_FILTER) {
		goto end;
	}

	component = component_create_funcs[type](component_class, params);
	if (!component) {
		goto end;
	}

	bt_object_init(component, bt_component_destroy);
	component->name = g_string_new(name);
	if (!component->name) {
		BT_PUT(component);
		goto end;
	}

	if (component_class->methods.init) {
		ret = component_class->methods.init(component, params,
			init_method_data);
		component->initializing = false;
		if (ret != BT_COMPONENT_STATUS_OK) {
			BT_PUT(component);
			goto end;
		}
	}

	component->initializing = false;
	ret = component_validation_funcs[type](component);
	if (ret != BT_COMPONENT_STATUS_OK) {
		BT_PUT(component);
		goto end;
	}

	bt_component_class_freeze(component->class);
end:
	return component;
}

struct bt_component *bt_component_create(
		struct bt_component_class *component_class, const char *name,
		struct bt_value *params)
{
	return bt_component_create_with_init_method_data(component_class, name,
		params, NULL);
}

const char *bt_component_get_name(struct bt_component *component)
{
	const char *ret = NULL;

	if (!component) {
		goto end;
	}

	ret = component->name->len == 0 ? NULL : component->name->str;
end:
	return ret;
}

enum bt_component_status bt_component_set_name(struct bt_component *component,
		const char *name)
{
	enum bt_component_status ret = BT_COMPONENT_STATUS_OK;

	if (!component || !name || name[0] == '\0') {
		ret = BT_COMPONENT_STATUS_INVALID;
		goto end;
	}

	g_string_assign(component->name, name);
end:
	return ret;
}

struct bt_component_class *bt_component_get_class(
		struct bt_component *component)
{
	return component ? bt_get(component->class) : NULL;
}

void *bt_component_get_private_data(struct bt_component *component)
{
	return component ? component->user_data : NULL;
}

enum bt_component_status
bt_component_set_private_data(struct bt_component *component,
		void *data)
{
	enum bt_component_status ret = BT_COMPONENT_STATUS_OK;

	if (!component || !component->initializing) {
		ret = BT_COMPONENT_STATUS_INVALID;
		goto end;
	}

	component->user_data = data;
end:
	return ret;
}

BT_HIDDEN
void bt_component_set_graph(struct bt_component *component,
		struct bt_graph *graph)
{
	struct bt_object *parent = bt_object_get_parent(&component->base);

	assert(!parent || parent == &graph->base);
	if (!parent) {
		bt_object_set_parent(component, &graph->base);
	}
	bt_put(parent);
}

struct bt_graph *bt_component_get_graph(
		struct bt_component *component)
{
	return (struct bt_graph *) bt_object_get_parent(&component->base);
}

BT_HIDDEN
int bt_component_init_input_ports(struct bt_component *component,
		GPtrArray **input_ports)
{
	int ret = 0;
	struct bt_port *default_port;

	*input_ports = g_ptr_array_new_with_free_func(bt_object_release);
	if (!*input_ports) {
		ret = -1;
		goto end;
	}

	default_port = bt_component_add_port(component, *input_ports,
			BT_PORT_TYPE_INPUT, DEFAULT_INPUT_PORT_NAME);
	if (!default_port) {
		ret = -1;
		goto end;
	}
	bt_put(default_port);
end:
	return ret;
}

BT_HIDDEN
int bt_component_init_output_ports(struct bt_component *component,
		GPtrArray **output_ports)
{
	int ret = 0;
	struct bt_port *default_port;

	*output_ports = g_ptr_array_new_with_free_func(bt_object_release);
	if (!*output_ports) {
		ret = -1;
		goto end;
	}

	default_port = bt_component_add_port(component, *output_ports,
			BT_PORT_TYPE_OUTPUT, DEFAULT_OUTPUT_PORT_NAME);
	if (!default_port) {
		ret = -1;
		goto end;
	}
	bt_put(default_port);
end:
	return ret;
}

BT_HIDDEN
struct bt_port *bt_component_get_port(GPtrArray *ports, const char *name)
{
	size_t i;
	struct bt_port *ret_port = NULL;

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
struct bt_port *bt_component_get_port_at_index(GPtrArray *ports, int index)
{
	struct bt_port *port = NULL;

	if (index < 0 || index >= ports->len) {
		goto end;
	}

	port = bt_get(g_ptr_array_index(ports, index));
end:
	return port;
}

BT_HIDDEN
struct bt_port *bt_component_add_port(
		struct bt_component *component,GPtrArray *ports,
		enum bt_port_type port_type, const char *name)
{
	size_t i;
	struct bt_port *new_port = NULL;

	if (!component->initializing || !name || *name == '\0') {
		goto end;
	}

	/* Look for a port having the same name. */
	for (i = 0; i < ports->len; i++) {
		const char *port_name;
		struct bt_port *port = g_ptr_array_index(
				ports, i);

		port_name = bt_port_get_name(port);
		if (!port_name) {
			continue;
		}

		if (!strcmp(name, port_name)) {
			/* Port name clash, abort. */
			goto end;
		}
	}

	new_port = bt_port_create(component, port_type, name);
	if (!new_port) {
		goto end;
	}

	/*
	 * No name clash, add the port.
	 * The component is now the port's parent; it should _not_
	 * hold a reference to the port since the port's lifetime
	 * is now protected by the component's own lifetime.
	 */
	g_ptr_array_add(ports, new_port);
end:
	return new_port;
}

BT_HIDDEN
enum bt_component_status bt_component_remove_port(
		struct bt_component *component, GPtrArray *ports,
		const char *name)
{
	size_t i;
	enum bt_component_status status = BT_COMPONENT_STATUS_OK;

	if (!component->initializing || !name) {
		status = BT_COMPONENT_STATUS_INVALID;
		goto end;
	}

	for (i = 0; i < ports->len; i++) {
		const char *port_name;
		struct bt_port *port = g_ptr_array_index(ports, i);

		port_name = bt_port_get_name(port);
		if (!port_name) {
			continue;
		}

		if (!strcmp(name, port_name)) {
			g_ptr_array_remove_index(ports, i);
			goto end;
		}
	}
	status = BT_COMPONENT_STATUS_NOT_FOUND;
end:
	return status;
}

BT_HIDDEN
enum bt_component_status bt_component_new_connection(
		struct bt_component *component, struct bt_port *own_port,
		struct bt_connection *connection)
{
	enum bt_component_status status = BT_COMPONENT_STATUS_OK;

	if (component->class->methods.new_connection_method) {
		status = component->class->methods.new_connection_method(
				own_port, connection);
	}

	return status;
}
