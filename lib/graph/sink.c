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

#include <babeltrace/compiler-internal.h>
#include <babeltrace/values.h>
#include <babeltrace/graph/component-sink-internal.h>
#include <babeltrace/graph/component-internal.h>
#include <babeltrace/graph/notification.h>

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

BT_HIDDEN
void bt_component_sink_destroy(struct bt_component *component)
{
}

BT_HIDDEN
struct bt_component *bt_component_sink_create(
		struct bt_component_class *class, struct bt_value *params)
{
	struct bt_component_sink *sink = NULL;

	sink = g_new0(struct bt_component_sink, 1);
	if (!sink) {
		goto end;
	}

end:
	return sink ? &sink->parent : NULL;
}

BT_HIDDEN
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
	ret = sink_class->methods.consume(bt_private_component_from_component(component));
end:
	return ret;
}

enum bt_component_status bt_component_sink_get_input_port_count(
		struct bt_component *component, uint64_t *count)
{
	enum bt_component_status status = BT_COMPONENT_STATUS_OK;

	if (!component || !count ||
			component->class->type != BT_COMPONENT_CLASS_TYPE_SINK) {
	        status = BT_COMPONENT_STATUS_INVALID;
		goto end;
	}

	*count = bt_component_get_input_port_count(component);
end:
	return status;
}

struct bt_port *bt_component_sink_get_input_port(
		struct bt_component *component, const char *name)
{
	struct bt_port *port = NULL;

	if (!component || !name ||
			component->class->type != BT_COMPONENT_CLASS_TYPE_SINK) {
		goto end;
	}

	port = bt_component_get_input_port(component, name);
end:
	return port;
}

struct bt_port *bt_component_sink_get_input_port_at_index(
		struct bt_component *component, int index)
{
	struct bt_port *port = NULL;

	if (!component ||
			component->class->type != BT_COMPONENT_CLASS_TYPE_SINK) {
		goto end;
	}

	port = bt_component_get_input_port_at_index(component, index);
end:
	return port;
}

struct bt_port *bt_component_sink_get_default_input_port(
		struct bt_component *component)
{
	return bt_component_sink_get_input_port(component,
			DEFAULT_INPUT_PORT_NAME);
}

struct bt_private_port *
bt_private_component_sink_get_input_private_port_at_index(
		struct bt_private_component *private_component, int index)
{
	return bt_private_port_from_port(
		bt_component_sink_get_input_port_at_index(
			bt_component_from_private(private_component), index));
}

struct bt_private_port *bt_private_component_sink_get_default_input_private_port(
		struct bt_private_component *private_component)
{
	return bt_private_port_from_port(
		bt_component_sink_get_default_input_port(
			bt_component_from_private(private_component)));
}

struct bt_private_port *bt_private_component_sink_add_input_private_port(
		struct bt_private_component *private_component,
		const char *name)
{
	struct bt_port *port = NULL;
	struct bt_component *component =
		bt_component_from_private(private_component);

	if (!component ||
			component->class->type != BT_COMPONENT_CLASS_TYPE_SINK) {
		goto end;
	}

	port = bt_component_add_input_port(component, name);
end:
	return bt_private_port_from_port(port);
}
