/*
 * source.c
 *
 * Babeltrace Source Component
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

#include <babeltrace/ref.h>
#include <babeltrace/compiler-internal.h>
#include <babeltrace/graph/component-source-internal.h>
#include <babeltrace/graph/component-internal.h>
#include <babeltrace/graph/port-internal.h>
#include <babeltrace/graph/notification-iterator.h>
#include <babeltrace/graph/notification-iterator-internal.h>

BT_HIDDEN
enum bt_component_status bt_component_source_validate(
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

	if (component->class->type != BT_COMPONENT_CLASS_TYPE_SOURCE) {
		ret = BT_COMPONENT_STATUS_INVALID;
		goto end;
	}

end:
	return ret;
}

BT_HIDDEN
void bt_component_source_destroy(struct bt_component *component)
{
}

BT_HIDDEN
struct bt_component *bt_component_source_create(
		struct bt_component_class *class, struct bt_value *params)
{
	struct bt_component_source *source = NULL;

	source = g_new0(struct bt_component_source, 1);
	if (!source) {
		goto end;
	}

end:
	return source ? &source->parent : NULL;
}

int64_t bt_component_source_get_output_port_count(
		struct bt_component *component, uint64_t *count)
{
	int64_t ret;

	if (!component || !count ||
			component->class->type != BT_COMPONENT_CLASS_TYPE_SOURCE) {
	        ret = (int64_t) -1;
		goto end;
	}

	ret = bt_component_get_output_port_count(component);
end:
	return ret;
}

struct bt_port *bt_component_source_get_output_port_by_name(
		struct bt_component *component, const char *name)
{
	struct bt_port *port = NULL;

	if (!component || !name ||
			component->class->type != BT_COMPONENT_CLASS_TYPE_SOURCE) {
		goto end;
	}

	port = bt_component_get_output_port_by_name(component, name);
end:
	return port;
}

struct bt_port *bt_component_source_get_output_port_by_index(
		struct bt_component *component, uint64_t index)
{
	struct bt_port *port = NULL;

	if (!component ||
			component->class->type != BT_COMPONENT_CLASS_TYPE_SOURCE) {
		goto end;
	}

	port = bt_component_get_output_port_by_index(component, index);
end:
	return port;
}

struct bt_port *bt_component_source_get_default_output_port(
		struct bt_component *component)
{
	return bt_component_source_get_output_port_by_name(component,
			DEFAULT_OUTPUT_PORT_NAME);
}

struct bt_private_port *
bt_private_component_source_get_output_private_port_by_name(
		struct bt_private_component *private_component,
		const char *name)
{
	return bt_private_port_from_port(
		bt_component_source_get_output_port_by_name(
			bt_component_from_private(private_component), name));
}

struct bt_private_port *
bt_private_component_source_get_output_private_port_by_index(
		struct bt_private_component *private_component, uint64_t index)
{
	return bt_private_port_from_port(
		bt_component_source_get_output_port_by_index(
			bt_component_from_private(private_component), index));
}

struct bt_private_port *bt_private_component_source_get_default_output_private_port(
		struct bt_private_component *private_component)
{
	return bt_private_port_from_port(
		bt_component_source_get_default_output_port(
			bt_component_from_private(private_component)));
}

struct bt_private_port *bt_private_component_source_add_output_private_port(
		struct bt_private_component *private_component,
		const char *name, void *user_data)
{
	struct bt_port *port = NULL;
	struct bt_component *component =
		bt_component_from_private(private_component);

	if (!component ||
			component->class->type != BT_COMPONENT_CLASS_TYPE_SOURCE) {
		goto end;
	}

	port = bt_component_add_output_port(component, name, user_data);
end:
	return bt_private_port_from_port(port);
}
