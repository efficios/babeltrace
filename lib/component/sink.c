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

#include <babeltrace/compiler.h>
#include <babeltrace/values.h>
#include <babeltrace/component/component-sink-internal.h>
#include <babeltrace/component/component-internal.h>
#include <babeltrace/component/notification/notification.h>

BT_HIDDEN
enum bt_component_status bt_component_sink_validate(
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

	if (component->class->type != BT_COMPONENT_CLASS_TYPE_SINK) {
		ret = BT_COMPONENT_STATUS_INVALID;
		goto end;
	}
end:
	return ret;
}

static
void bt_component_sink_destroy(struct bt_component *component)
{
	struct bt_component_sink *sink = container_of(component,
			struct bt_component_sink, parent);

	if (sink->input_ports) {
		g_ptr_array_free(sink->input_ports, TRUE);
	}
}

BT_HIDDEN
struct bt_component *bt_component_sink_create(
		struct bt_component_class *class, struct bt_value *params)
{
	struct bt_component_sink *sink = NULL;
	enum bt_component_status ret;

	sink = g_new0(struct bt_component_sink, 1);
	if (!sink) {
		goto end;
	}

	sink->parent.class = bt_get(class);
	ret = bt_component_init(&sink->parent, bt_component_sink_destroy);
	if (ret != BT_COMPONENT_STATUS_OK) {
		goto error;
	}

/*
	ret = bt_component_sink_register_notification_type(&sink->parent,
		BT_NOTIFICATION_TYPE_EVENT);
	if (ret != BT_COMPONENT_STATUS_OK) {
		goto error;
	}
*/
	ret = bt_component_init_input_ports(&sink->parent,
			&sink->input_ports);
	if (ret) {
		goto error;
	}

end:
	return sink ? &sink->parent : NULL;
error:
	BT_PUT(sink);
	return NULL;
}

enum bt_component_status bt_component_sink_consume(
		struct bt_component *component)
{
	enum bt_component_status ret = BT_COMPONENT_STATUS_OK;
	struct bt_component_class_sink *sink_class = NULL;

	if (!component) {
		ret = BT_COMPONENT_STATUS_INVALID;
		goto end;
	}

	if (bt_component_get_class_type(component) != BT_COMPONENT_CLASS_TYPE_SINK) {
		ret = BT_COMPONENT_STATUS_UNSUPPORTED;
		goto end;
	}

	sink_class = container_of(component->class, struct bt_component_class_sink, parent);
	assert(sink_class->methods.consume);
	ret = sink_class->methods.consume(component);
end:
	return ret;
}
/*
static
enum bt_component_status bt_component_sink_register_notification_type(
		struct bt_component *component, enum bt_notification_type type)
{
	enum bt_component_status ret = BT_COMPONENT_STATUS_OK;
	struct bt_component_sink *sink = NULL;

	if (!component) {
		ret = BT_COMPONENT_STATUS_INVALID;
		goto end;
	}

	if (bt_component_get_class_type(component) != BT_COMPONENT_CLASS_TYPE_SINK) {
		ret = BT_COMPONENT_STATUS_UNSUPPORTED;
		goto end;
	}

	if (type <= BT_NOTIFICATION_TYPE_UNKNOWN ||
		type >= BT_NOTIFICATION_TYPE_NR) {
		ret = BT_COMPONENT_STATUS_INVALID;
		goto end;
	}
	sink = container_of(component, struct bt_component_sink, parent);
	if (type == BT_NOTIFICATION_TYPE_ALL) {
		sink->registered_notifications_mask = ~(notification_mask_t) 0;
	} else {
		sink->registered_notifications_mask |=
			(notification_mask_t) 1 << type;
	}
end:
	return ret;
}
*/

int bt_component_sink_get_input_port_count(struct bt_component *component)
{
	int ret;
	struct bt_component_sink *sink;

	if (!component) {
		ret = -1;
		goto end;
	}

	if (component->class->type != BT_COMPONENT_CLASS_TYPE_SINK) {
		ret = -1;
		goto end;
	}

	sink = container_of(component, struct bt_component_sink, parent);
	ret = sink->input_ports->len;
end:
	return ret;
}

struct bt_port *bt_component_sink_get_input_port(
		struct bt_component *component, const char *name)
{
	struct bt_component_sink *sink;
	struct bt_port *ret_port = NULL;

	if (!component || !name ||
			component->class->type != BT_COMPONENT_CLASS_TYPE_SINK) {
		goto end;
	}

	sink = container_of(component, struct bt_component_sink, parent);
	ret_port = bt_component_get_port(sink->input_ports, name);
end:
	return ret_port;
}

struct bt_port *bt_component_sink_get_input_port_at_index(
		struct bt_component *component, int index)
{
	struct bt_port *port = NULL;
	struct bt_component_sink *sink;

	if (!component ||
			component->class->type != BT_COMPONENT_CLASS_TYPE_SINK) {
		goto end;
	}

	sink = container_of(component, struct bt_component_sink, parent);
	port = bt_component_get_port_at_index(sink->input_ports, index);
end:
	return port;
}

struct bt_port *bt_component_sink_get_default_input_port(
		struct bt_component *component)
{
	return bt_component_sink_get_input_port(component,
			DEFAULT_INPUT_PORT_NAME);
}

struct bt_port *bt_component_sink_add_input_port(
		struct bt_component *component, const char *name)
{
	struct bt_port *port;
	struct bt_component_sink *sink;

	if (!component ||
			component->class->type != BT_COMPONENT_CLASS_TYPE_SINK) {
		port = NULL;
		goto end;
	}

	sink = container_of(component, struct bt_component_sink, parent);
	port = bt_component_add_port(component, sink->input_ports,
			BT_PORT_TYPE_INPUT, name);
end:
	return port;
}

enum bt_component_status bt_component_sink_remove_input_port(
		struct bt_component *component, const char *name)
{
	enum bt_component_status status;
	struct bt_component_sink *sink;

	if (!component ||
			component->class->type != BT_COMPONENT_CLASS_TYPE_SINK) {
		status = BT_COMPONENT_STATUS_INVALID;
		goto end;
	}

	sink = container_of(component, struct bt_component_sink, parent);
	status = bt_component_remove_port(component, sink->input_ports,
			name);
end:
	return status;
}
