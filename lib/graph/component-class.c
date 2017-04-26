/*
 * component-class.c
 *
 * Babeltrace Plugin Component Class
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

#include <babeltrace/compiler-internal.h>
#include <babeltrace/graph/component-class-internal.h>
#include <babeltrace/ref.h>
#include <stdbool.h>
#include <glib.h>

static
void bt_component_class_destroy(struct bt_object *obj)
{
	struct bt_component_class *class;
	int i;

	assert(obj);
	class = container_of(obj, struct bt_component_class, base);

	/* Call destroy listeners in reverse registration order */
	for (i = class->destroy_listeners->len - 1; i >= 0; i--) {
		struct bt_component_class_destroy_listener *listener =
			&g_array_index(class->destroy_listeners,
				struct bt_component_class_destroy_listener,
				i);

		listener->func(class, listener->data);
	}

	if (class->name) {
		g_string_free(class->name, TRUE);
	}
	if (class->description) {
		g_string_free(class->description, TRUE);
	}
	if (class->help) {
		g_string_free(class->help, TRUE);
	}
	if (class->destroy_listeners) {
		g_array_free(class->destroy_listeners, TRUE);
	}

	g_free(class);
}

static
int bt_component_class_init(struct bt_component_class *class,
		enum bt_component_class_type type, const char *name)
{
	int ret = 0;

	bt_object_init(class, bt_component_class_destroy);
	class->type = type;
	class->name = g_string_new(name);
	if (!class->name) {
		goto error;
	}

	class->description = g_string_new(NULL);
	if (!class->description) {
		goto error;
	}

	class->help = g_string_new(NULL);
	if (!class->help) {
		goto error;
	}

	class->destroy_listeners = g_array_new(FALSE, TRUE,
		sizeof(struct bt_component_class_destroy_listener));
	if (!class->destroy_listeners) {
		goto error;
	}

	goto end;

error:
	BT_PUT(class);
	ret = -1;

end:
	return ret;
}

struct bt_component_class *bt_component_class_source_create(const char *name,
		bt_component_class_notification_iterator_next_method notification_iterator_next_method)
{
	struct bt_component_class_source *source_class = NULL;
	int ret;

	if (!name || !notification_iterator_next_method) {
		goto end;
	}

	source_class = g_new0(struct bt_component_class_source, 1);
	if (!source_class) {
		goto end;
	}

	ret = bt_component_class_init(&source_class->parent,
		BT_COMPONENT_CLASS_TYPE_SOURCE, name);
	if (ret) {
		/*
		 * If bt_component_class_init() fails, the component
		 * class is put, therefore its memory is already
		 * freed.
		 */
		source_class = NULL;
		goto end;
	}

	source_class->methods.iterator.next = notification_iterator_next_method;

end:
	return &source_class->parent;
}

struct bt_component_class *bt_component_class_filter_create(const char *name,
		bt_component_class_notification_iterator_next_method notification_iterator_next_method)
{
	struct bt_component_class_filter *filter_class = NULL;
	int ret;

	if (!name || !notification_iterator_next_method) {
		goto end;
	}

	filter_class = g_new0(struct bt_component_class_filter, 1);
	if (!filter_class) {
		goto end;
	}

	ret = bt_component_class_init(&filter_class->parent,
		BT_COMPONENT_CLASS_TYPE_FILTER, name);
	if (ret) {
		/*
		 * If bt_component_class_init() fails, the component
		 * class is put, therefore its memory is already
		 * freed.
		 */
		filter_class = NULL;
		goto end;
	}

	filter_class->methods.iterator.next = notification_iterator_next_method;

end:
	return &filter_class->parent;
}

struct bt_component_class *bt_component_class_sink_create(const char *name,
		bt_component_class_sink_consume_method consume_method)
{
	struct bt_component_class_sink *sink_class = NULL;
	int ret;

	if (!name || !consume_method) {
		goto end;
	}

	sink_class = g_new0(struct bt_component_class_sink, 1);
	if (!sink_class) {
		goto end;
	}

	ret = bt_component_class_init(&sink_class->parent,
		BT_COMPONENT_CLASS_TYPE_SINK, name);
	if (ret) {
		/*
		 * If bt_component_class_init() fails, the component
		 * class is put, therefore its memory is already
		 * freed.
		 */
		sink_class = NULL;
		goto end;
	}

	sink_class->methods.consume = consume_method;

end:
	return &sink_class->parent;
}

int bt_component_class_set_init_method(
		struct bt_component_class *component_class,
		bt_component_class_init_method init_method)
{
	int ret = 0;

	if (!component_class || component_class->frozen || !init_method) {
		ret = -1;
		goto end;
	}

	component_class->methods.init = init_method;

end:
	return ret;
}

int bt_component_class_set_query_method(
		struct bt_component_class *component_class,
		bt_component_class_query_method query_method)
{
	int ret = 0;

	if (!component_class || component_class->frozen || !query_method) {
		ret = -1;
		goto end;
	}

	component_class->methods.query = query_method;

end:
	return ret;
}

int bt_component_class_set_accept_port_connection_method(
		struct bt_component_class *component_class,
		bt_component_class_accept_port_connection_method method)
{
	int ret = 0;

	if (!component_class || component_class->frozen || !method) {
		ret = -1;
		goto end;
	}

	component_class->methods.accept_port_connection = method;

end:
	return ret;
}

int bt_component_class_set_port_connected_method(
		struct bt_component_class *component_class,
		bt_component_class_port_connected_method method)
{
	int ret = 0;

	if (!component_class || component_class->frozen || !method) {
		ret = -1;
		goto end;
	}

	component_class->methods.port_connected = method;

end:
	return ret;
}

int bt_component_class_set_port_disconnected_method(
		struct bt_component_class *component_class,
		bt_component_class_port_disconnected_method method)
{
	int ret = 0;

	if (!component_class || component_class->frozen || !method) {
		ret = -1;
		goto end;
	}

	component_class->methods.port_disconnected = method;

end:
	return ret;
}

int bt_component_class_set_finalize_method(
		struct bt_component_class *component_class,
		bt_component_class_finalize_method finalize_method)
{
	int ret = 0;

	if (!component_class || component_class->frozen || !finalize_method) {
		ret = -1;
		goto end;
	}

	component_class->methods.finalize = finalize_method;

end:
	return ret;
}

int bt_component_class_source_set_notification_iterator_init_method(
		struct bt_component_class *component_class,
		bt_component_class_notification_iterator_init_method notification_iterator_init_method)
{
	struct bt_component_class_source *source_class;
	int ret = 0;

	if (!component_class || component_class->frozen ||
			!notification_iterator_init_method ||
			component_class->type != BT_COMPONENT_CLASS_TYPE_SOURCE) {
		ret = -1;
		goto end;
	}

	source_class = container_of(component_class,
		struct bt_component_class_source, parent);
	source_class->methods.iterator.init = notification_iterator_init_method;

end:
	return ret;
}

int bt_component_class_source_set_notification_iterator_finalize_method(
		struct bt_component_class *component_class,
		bt_component_class_notification_iterator_finalize_method notification_iterator_finalize_method)
{
	struct bt_component_class_source *source_class;
	int ret = 0;

	if (!component_class || component_class->frozen ||
			!notification_iterator_finalize_method ||
			component_class->type != BT_COMPONENT_CLASS_TYPE_SOURCE) {
		ret = -1;
		goto end;
	}

	source_class = container_of(component_class,
		struct bt_component_class_source, parent);
	source_class->methods.iterator.finalize =
		notification_iterator_finalize_method;

end:
	return ret;
}

int bt_component_class_source_set_notification_iterator_seek_time_method(
		struct bt_component_class *component_class,
		bt_component_class_notification_iterator_seek_time_method notification_iterator_seek_time_method)
{
	struct bt_component_class_source *source_class;
	int ret = 0;

	if (!component_class || component_class->frozen ||
			!notification_iterator_seek_time_method ||
			component_class->type != BT_COMPONENT_CLASS_TYPE_SOURCE) {
		ret = -1;
		goto end;
	}

	source_class = container_of(component_class,
		struct bt_component_class_source, parent);
	source_class->methods.iterator.seek_time =
		notification_iterator_seek_time_method;

end:
	return ret;
}

int bt_component_class_filter_set_notification_iterator_init_method(
		struct bt_component_class *component_class,
		bt_component_class_notification_iterator_init_method notification_iterator_init_method)
{
	struct bt_component_class_filter *filter_class;
	int ret = 0;

	if (!component_class || component_class->frozen ||
			!notification_iterator_init_method ||
			component_class->type != BT_COMPONENT_CLASS_TYPE_FILTER) {
		ret = -1;
		goto end;
	}

	filter_class = container_of(component_class,
		struct bt_component_class_filter, parent);
	filter_class->methods.iterator.init = notification_iterator_init_method;

end:
	return ret;
}

int bt_component_class_filter_set_notification_iterator_finalize_method(
		struct bt_component_class *component_class,
		bt_component_class_notification_iterator_finalize_method notification_iterator_finalize_method)
{
	struct bt_component_class_filter *filter_class;
	int ret = 0;

	if (!component_class || component_class->frozen ||
			!notification_iterator_finalize_method ||
			component_class->type != BT_COMPONENT_CLASS_TYPE_FILTER) {
		ret = -1;
		goto end;
	}

	filter_class = container_of(component_class,
		struct bt_component_class_filter, parent);
	filter_class->methods.iterator.finalize =
		notification_iterator_finalize_method;

end:
	return ret;
}

int bt_component_class_filter_set_notification_iterator_seek_time_method(
		struct bt_component_class *component_class,
		bt_component_class_notification_iterator_seek_time_method notification_iterator_seek_time_method)
{
	struct bt_component_class_filter *filter_class;
	int ret = 0;

	if (!component_class || component_class->frozen ||
			!notification_iterator_seek_time_method ||
			component_class->type != BT_COMPONENT_CLASS_TYPE_FILTER) {
		ret = -1;
		goto end;
	}

	filter_class = container_of(component_class,
		struct bt_component_class_filter, parent);
	filter_class->methods.iterator.seek_time =
		notification_iterator_seek_time_method;

end:
	return ret;
}

int bt_component_class_set_description(
		struct bt_component_class *component_class,
		const char *description)
{
	int ret = 0;

	if (!component_class || component_class->frozen || !description) {
		ret = -1;
		goto end;
	}

	g_string_assign(component_class->description, description);

end:
	return ret;
}

int bt_component_class_set_help(
		struct bt_component_class *component_class,
		const char *help)
{
	int ret = 0;

	if (!component_class || component_class->frozen || !help) {
		ret = -1;
		goto end;
	}

	g_string_assign(component_class->help, help);

end:
	return ret;
}

const char *bt_component_class_get_name(
		struct bt_component_class *component_class)
{
	return component_class ? component_class->name->str : NULL;
}

enum bt_component_class_type bt_component_class_get_type(
		struct bt_component_class *component_class)
{
	return component_class ? component_class->type :
			BT_COMPONENT_CLASS_TYPE_UNKNOWN;
}

const char *bt_component_class_get_description(
		struct bt_component_class *component_class)
{
	return component_class && component_class->description &&
		component_class->description->str[0] != '\0' ?
		component_class->description->str : NULL;
}

const char *bt_component_class_get_help(
		struct bt_component_class *component_class)
{
	return component_class && component_class->help &&
		component_class->help->str[0] != '\0' ?
		component_class->help->str : NULL;
}

BT_HIDDEN
void bt_component_class_add_destroy_listener(struct bt_component_class *class,
		bt_component_class_destroy_listener_func func, void *data)
{
	struct bt_component_class_destroy_listener listener;

	assert(class);
	assert(func);
	listener.func = func;
	listener.data = data;
	g_array_append_val(class->destroy_listeners, listener);
}

int bt_component_class_freeze(
		struct bt_component_class *component_class)
{
	int ret = 0;

	if (!component_class) {
		ret = -1;
		goto end;
	}

	component_class->frozen = true;

end:
	return ret;
}

struct bt_value *bt_component_class_query(
		struct bt_component_class *component_class,
		const char *object, struct bt_value *params)
{
	struct bt_value *results = NULL;

	if (!component_class || !object || !params ||
			!component_class->methods.query) {
		goto end;
	}

	results = component_class->methods.query(component_class,
		object, params);

end:
	return results;
}
