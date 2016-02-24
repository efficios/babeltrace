/*
 * component-class.c
 *
 * Babeltrace Plugin Component Class
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
#include <babeltrace/plugin/component-class-internal.h>
#include <babeltrace/ref.h>
#include <glib.h>

static
void bt_component_class_destroy(struct bt_object *obj)
{
	struct bt_component_class *class;

	assert(obj);
	class = container_of(obj, struct bt_component_class, base);
	if (class->name) {
		g_string_free(class->name, TRUE);
	}
	if (class->description) {
		g_string_free(class->description, TRUE);
	}

	bt_put(class->plugin);
	g_free(class);
}

BT_HIDDEN
struct bt_component_class *bt_component_class_create(
		enum bt_component_type type, const char *name,
		const char *description, struct bt_plugin *plugin)
{
	struct bt_component_class *class;

	class = g_new0(struct bt_component_class, 1);
	if (!class) {
		goto end;
	}

	bt_object_init(class, bt_component_class_destroy);
	class->type = type;
	class->name = g_string_new(name);
	class->description = g_string_new(description);
	if (!class->name || !class->description) {
	        BT_PUT(class);
		goto end;
	}

	class->plugin = bt_get(plugin);
end:
	return class;
}

const char *bt_component_class_get_name(
		struct bt_component_class *component_class)
{
	return component_class ? component_class->name->str : NULL;
}

enum bt_component_type bt_component_class_get_type(
		struct bt_component_class *component_class)
{
	return component_class ? component_class->type :
			BT_COMPONENT_TYPE_UNKNOWN;
}

struct bt_plugin *bt_component_class_get_plugin(
		struct bt_component_class *component_class)
{
	return component_class ? bt_get(component_class->plugin) :
			NULL;
}

const char *bt_component_class_get_description(
		struct bt_component_class *component_class)
{
	return component_class ? component_class->description->str : NULL;
}

