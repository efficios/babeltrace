/*
 * filter.c
 *
 * Babeltrace Filter Component
 *
 * Copyright 2016 Jérémie Galarneau <jeremie.galarneau@efficios.com>
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

#include <babeltrace/compiler.h>
#include <babeltrace/values.h>
#include <babeltrace/component/component-filter-internal.h>
#include <babeltrace/component/component-internal.h>
#include <babeltrace/component/component-class-internal.h>
#include <babeltrace/component/notification/notification.h>
#include <babeltrace/component/notification/iterator-internal.h>

BT_HIDDEN
struct bt_notification_iterator *bt_component_filter_create_notification_iterator(
		struct bt_component *component)
{
	return bt_component_create_iterator(component, NULL);
}

BT_HIDDEN
struct bt_notification_iterator *bt_component_filter_create_notification_iterator_with_init_method_data(
		struct bt_component *component, void *init_method_data)
{
	return bt_component_create_iterator(component, init_method_data);
}

static
void bt_component_filter_destroy(struct bt_component *component)
{
	struct bt_component_filter *filter = container_of(component,
			struct bt_component_filter, parent);

	if (filter->input_ports) {
		g_ptr_array_free(filter->input_ports, TRUE);
	}

	if (filter->output_ports) {
		g_ptr_array_free(filter->output_ports, TRUE);
	}
}

BT_HIDDEN
struct bt_component *bt_component_filter_create(
		struct bt_component_class *class, struct bt_value *params)
{
	int ret;
	struct bt_component_filter *filter = NULL;
	enum bt_component_status status;

	filter = g_new0(struct bt_component_filter, 1);
	if (!filter) {
		goto end;
	}

	filter->parent.class = bt_get(class);
	status = bt_component_init(&filter->parent, bt_component_filter_destroy);
	if (status != BT_COMPONENT_STATUS_OK) {
		goto error;
	}

	ret = bt_component_init_input_ports(&filter->parent,
			&filter->input_ports);
	if (ret) {
		goto error;
	}

	ret = bt_component_init_output_ports(&filter->parent,
			&filter->output_ports);
	if (ret) {
		goto error;
	}

end:
	return filter ? &filter->parent : NULL;
error:
	BT_PUT(filter);
	goto end;
}

BT_HIDDEN
enum bt_component_status bt_component_filter_validate(
		struct bt_component *component)
{
	enum bt_component_status ret = BT_COMPONENT_STATUS_OK;

	if (!component) {
		ret = BT_COMPONENT_STATUS_INVALID;
		goto end;
	}

	if (!component->class) {
		ret = BT_COMPONENT_STATUS_INVALID;
		goto end;
	}

	if (component->class->type != BT_COMPONENT_CLASS_TYPE_FILTER) {
		ret = BT_COMPONENT_STATUS_INVALID;
		goto end;
	}

	/* Enforce iterator limits. */
end:
	return ret;
}

int bt_component_filter_get_input_port_count(struct bt_component *component)
{
	int ret;
	struct bt_component_filter *filter;

	if (!component) {
		ret = -1;
		goto end;
	}

	if (component->class->type != BT_COMPONENT_CLASS_TYPE_FILTER) {
		ret = -1;
		goto end;
	}

	filter = container_of(component, struct bt_component_filter, parent);
	ret = filter->input_ports->len;
end:
	return ret;
}

struct bt_port *bt_component_filter_get_input_port(
		struct bt_component *component, const char *name)
{
	struct bt_component_filter *filter;
	struct bt_port *ret_port = NULL;

	if (!component || !name ||
			component->class->type != BT_COMPONENT_CLASS_TYPE_FILTER) {
		goto end;
	}

	filter = container_of(component, struct bt_component_filter, parent);
	ret_port = bt_component_get_port(filter->input_ports, name);
end:
	return ret_port;
}

struct bt_port *bt_component_filter_get_input_port_at_index(
		struct bt_component *component, int index)
{
	struct bt_port *port = NULL;
	struct bt_component_filter *filter;

	if (!component ||
			component->class->type != BT_COMPONENT_CLASS_TYPE_FILTER) {
		goto end;
	}

	filter = container_of(component, struct bt_component_filter, parent);
	port = bt_component_get_port_at_index(filter->input_ports, index);
end:
	return port;
}

struct bt_port *bt_component_filter_get_default_input_port(
		struct bt_component *component)
{
	return bt_component_filter_get_input_port(component,
			DEFAULT_INPUT_PORT_NAME);
}

struct bt_port *bt_component_filter_add_input_port(
		struct bt_component *component, const char *name)
{
	struct bt_port *port;
	struct bt_component_filter *filter;

	if (!component ||
			component->class->type != BT_COMPONENT_CLASS_TYPE_FILTER) {
		port = NULL;
		goto end;
	}

	filter = container_of(component, struct bt_component_filter, parent);
	port = bt_component_add_port(component, filter->input_ports,
			BT_PORT_TYPE_INPUT, name);
end:
	return port;
}

enum bt_component_status bt_component_filter_remove_input_port(
		struct bt_component *component, const char *name)
{
	enum bt_component_status status;
	struct bt_component_filter *filter;

	if (!component ||
			component->class->type != BT_COMPONENT_CLASS_TYPE_FILTER) {
		status = BT_COMPONENT_STATUS_INVALID;
		goto end;
	}

	filter = container_of(component, struct bt_component_filter, parent);
	status = bt_component_remove_port(component, filter->input_ports,
			name);
end:
	return status;
}

int bt_component_filter_get_output_port_count(struct bt_component *component)
{
	int ret;
	struct bt_component_filter *filter;

	if (!component) {
		ret = -1;
		goto end;
	}

	if (component->class->type != BT_COMPONENT_CLASS_TYPE_FILTER) {
		ret = -1;
		goto end;
	}

	filter = container_of(component, struct bt_component_filter, parent);
	ret = filter->output_ports->len;
end:
	return ret;
}

struct bt_port *bt_component_filter_get_output_port(
		struct bt_component *component, const char *name)
{
	struct bt_component_filter *filter;
	struct bt_port *ret_port = NULL;

	if (!component || !name ||
			component->class->type != BT_COMPONENT_CLASS_TYPE_FILTER) {
		goto end;
	}

	filter = container_of(component, struct bt_component_filter, parent);
	ret_port = bt_component_get_port(filter->output_ports, name);
end:
	return ret_port;
}

struct bt_port *bt_component_filter_get_output_port_at_index(
		struct bt_component *component, int index)
{
	struct bt_port *port = NULL;
	struct bt_component_filter *filter;

	if (!component ||
			component->class->type != BT_COMPONENT_CLASS_TYPE_FILTER) {
		goto end;
	}

	filter = container_of(component, struct bt_component_filter, parent);
	port = bt_component_get_port_at_index(filter->output_ports, index);
end:
	return port;
}

struct bt_port *bt_component_filter_get_default_output_port(
		struct bt_component *component)
{
	return bt_component_filter_get_output_port(component,
			DEFAULT_OUTPUT_PORT_NAME);
}

struct bt_port *bt_component_filter_add_output_port(
		struct bt_component *component, const char *name)
{
	struct bt_port *port;
	struct bt_component_filter *filter;

	if (!component ||
			component->class->type != BT_COMPONENT_CLASS_TYPE_FILTER) {
		port = NULL;
		goto end;
	}

	filter = container_of(component, struct bt_component_filter, parent);
	port = bt_component_add_port(component, filter->output_ports,
			BT_PORT_TYPE_OUTPUT, name);
end:
	return port;
}

enum bt_component_status bt_component_filter_remove_output_port(
		struct bt_component *component, const char *name)
{
	enum bt_component_status status;
	struct bt_component_filter *filter;

	if (!component ||
			component->class->type != BT_COMPONENT_CLASS_TYPE_FILTER) {
		status = BT_COMPONENT_STATUS_INVALID;
		goto end;
	}

	filter = container_of(component, struct bt_component_filter, parent);
	status = bt_component_remove_port(component, filter->output_ports,
			name);
end:
	return status;
}
