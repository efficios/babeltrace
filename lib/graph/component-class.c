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

#define BT_LOG_TAG "COMP-CLASS"
#include <babeltrace/lib-logging-internal.h>

#include <babeltrace/compiler-internal.h>
#include <babeltrace/graph/component-class-internal.h>
#include <babeltrace/ref.h>
#include <babeltrace/types.h>
#include <babeltrace/assert-internal.h>
#include <glib.h>

static
void bt_component_class_destroy(struct bt_object *obj)
{
	struct bt_component_class *class;
	int i;

	BT_ASSERT(obj);
	class = container_of(obj, struct bt_component_class, base);

	BT_LOGD("Destroying component class: "
		"addr=%p, name=\"%s\", type=%s",
		class, bt_component_class_get_name(class),
		bt_component_class_type_string(class->type));

	/* Call destroy listeners in reverse registration order */
	for (i = class->destroy_listeners->len - 1; i >= 0; i--) {
		struct bt_component_class_destroy_listener *listener =
			&g_array_index(class->destroy_listeners,
				struct bt_component_class_destroy_listener,
				i);

		BT_LOGD("Calling destroy listener: func-addr=%p, data-addr=%p",
			listener->func, listener->data);
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

	bt_object_init_shared(&class->base, bt_component_class_destroy);
	class->type = type;
	class->name = g_string_new(name);
	if (!class->name) {
		BT_LOGE_STR("Failed to allocate a GString.");
		goto error;
	}

	class->description = g_string_new(NULL);
	if (!class->description) {
		BT_LOGE_STR("Failed to allocate a GString.");
		goto error;
	}

	class->help = g_string_new(NULL);
	if (!class->help) {
		BT_LOGE_STR("Failed to allocate a GString.");
		goto error;
	}

	class->destroy_listeners = g_array_new(FALSE, TRUE,
		sizeof(struct bt_component_class_destroy_listener));
	if (!class->destroy_listeners) {
		BT_LOGE_STR("Failed to allocate a GArray.");
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
		bt_component_class_notification_iterator_next_method method)
{
	struct bt_component_class_source *source_class = NULL;
	int ret;

	if (!name) {
		BT_LOGW_STR("Invalid parameter: name is NULL.");
		goto end;
	}

	if (!method) {
		BT_LOGW_STR("Invalid parameter: method is NULL.");
		goto end;
	}

	BT_LOGD("Creating source component class: "
		"name=\"%s\", notif-iter-next-method-addr=%p",
		name, method);
	source_class = g_new0(struct bt_component_class_source, 1);
	if (!source_class) {
		BT_LOGE_STR("Failed to allocate one source component class.");
		goto end;
	}

	/* bt_component_class_init() logs errors */
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

	source_class->methods.iterator.next = method;
	BT_LOGD("Created source component class: "
		"name=\"%s\", notif-iter-next-method-addr=%p, addr=%p",
		name, method, &source_class->parent);

end:
	return &source_class->parent;
}

struct bt_component_class *bt_component_class_filter_create(const char *name,
		bt_component_class_notification_iterator_next_method method)
{
	struct bt_component_class_filter *filter_class = NULL;
	int ret;

	if (!name) {
		BT_LOGW_STR("Invalid parameter: name is NULL.");
		goto end;
	}

	if (!method) {
		BT_LOGW_STR("Invalid parameter: method is NULL.");
		goto end;
	}

	BT_LOGD("Creating filter component class: "
		"name=\"%s\", notif-iter-next-method-addr=%p",
		name, method);
	filter_class = g_new0(struct bt_component_class_filter, 1);
	if (!filter_class) {
		BT_LOGE_STR("Failed to allocate one filter component class.");
		goto end;
	}

	/* bt_component_class_init() logs errors */
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

	filter_class->methods.iterator.next = method;
	BT_LOGD("Created filter component class: "
		"name=\"%s\", notif-iter-next-method-addr=%p, addr=%p",
		name, method, &filter_class->parent);

end:
	return &filter_class->parent;
}

struct bt_component_class *bt_component_class_sink_create(const char *name,
		bt_component_class_sink_consume_method method)
{
	struct bt_component_class_sink *sink_class = NULL;
	int ret;

	if (!name) {
		BT_LOGW_STR("Invalid parameter: name is NULL.");
		goto end;
	}

	if (!method) {
		BT_LOGW_STR("Invalid parameter: method is NULL.");
		goto end;
	}

	BT_LOGD("Creating sink component class: "
		"name=\"%s\", consume-method-addr=%p",
		name, method);
	sink_class = g_new0(struct bt_component_class_sink, 1);
	if (!sink_class) {
		BT_LOGE_STR("Failed to allocate one sink component class.");
		goto end;
	}

	/* bt_component_class_init() logs errors */
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

	sink_class->methods.consume = method;
	BT_LOGD("Created sink component class: "
		"name=\"%s\", consume-method-addr=%p, addr=%p",
		name, method, &sink_class->parent);

end:
	return &sink_class->parent;
}

int bt_component_class_set_init_method(
		struct bt_component_class *component_class,
		bt_component_class_init_method method)
{
	int ret = 0;

	if (!component_class) {
		BT_LOGW_STR("Invalid parameter: component class is NULL.");
		ret = -1;
		goto end;
	}

	if (!method) {
		BT_LOGW_STR("Invalid parameter: method is NULL.");
		ret = -1;
		goto end;
	}

	if (component_class->frozen) {
		BT_LOGW("Invalid parameter: component class is frozen: "
			"addr=%p, name=\"%s\", type=%s",
			component_class,
			bt_component_class_get_name(component_class),
			bt_component_class_type_string(component_class->type));
		ret = -1;
		goto end;
	}

	component_class->methods.init = method;
	BT_LOGV("Set component class's initialization method: "
		"addr=%p, name=\"%s\", type=%s, method-addr=%p",
		component_class,
		bt_component_class_get_name(component_class),
		bt_component_class_type_string(component_class->type),
		method);

end:
	return ret;
}

int bt_component_class_set_query_method(
		struct bt_component_class *component_class,
		bt_component_class_query_method method)
{
	int ret = 0;

	if (!component_class) {
		BT_LOGW_STR("Invalid parameter: component class is NULL.");
		ret = -1;
		goto end;
	}

	if (!method) {
		BT_LOGW_STR("Invalid parameter: method is NULL.");
		ret = -1;
		goto end;
	}

	if (component_class->frozen) {
		BT_LOGW("Invalid parameter: component class is frozen: "
			"addr=%p, name=\"%s\", type=%s",
			component_class,
			bt_component_class_get_name(component_class),
			bt_component_class_type_string(component_class->type));
		ret = -1;
		goto end;
	}

	component_class->methods.query = method;
	BT_LOGV("Set component class's query method: "
		"addr=%p, name=\"%s\", type=%s, method-addr=%p",
		component_class,
		bt_component_class_get_name(component_class),
		bt_component_class_type_string(component_class->type),
		method);

end:
	return ret;
}

int bt_component_class_set_accept_port_connection_method(
		struct bt_component_class *component_class,
		bt_component_class_accept_port_connection_method method)
{
	int ret = 0;

	if (!component_class) {
		BT_LOGW_STR("Invalid parameter: component class is NULL.");
		ret = -1;
		goto end;
	}

	if (!method) {
		BT_LOGW_STR("Invalid parameter: method is NULL.");
		ret = -1;
		goto end;
	}

	if (component_class->frozen) {
		BT_LOGW("Invalid parameter: component class is frozen: "
			"addr=%p, name=\"%s\", type=%s",
			component_class,
			bt_component_class_get_name(component_class),
			bt_component_class_type_string(component_class->type));
		ret = -1;
		goto end;
	}

	component_class->methods.accept_port_connection = method;
	BT_LOGV("Set component class's \"accept port connection\" method: "
		"addr=%p, name=\"%s\", type=%s, method-addr=%p",
		component_class,
		bt_component_class_get_name(component_class),
		bt_component_class_type_string(component_class->type),
		method);

end:
	return ret;
}

int bt_component_class_set_port_connected_method(
		struct bt_component_class *component_class,
		bt_component_class_port_connected_method method)
{
	int ret = 0;

	if (!component_class) {
		BT_LOGW_STR("Invalid parameter: component class is NULL.");
		ret = -1;
		goto end;
	}

	if (!method) {
		BT_LOGW_STR("Invalid parameter: method is NULL.");
		ret = -1;
		goto end;
	}

	if (component_class->frozen) {
		BT_LOGW("Invalid parameter: component class is frozen: "
			"addr=%p, name=\"%s\", type=%s",
			component_class,
			bt_component_class_get_name(component_class),
			bt_component_class_type_string(component_class->type));
		ret = -1;
		goto end;
	}

	component_class->methods.port_connected = method;
	BT_LOGV("Set component class's \"port connected\" method: "
		"addr=%p, name=\"%s\", type=%s, method-addr=%p",
		component_class,
		bt_component_class_get_name(component_class),
		bt_component_class_type_string(component_class->type),
		method);

end:
	return ret;
}

int bt_component_class_set_port_disconnected_method(
		struct bt_component_class *component_class,
		bt_component_class_port_disconnected_method method)
{
	int ret = 0;

	if (!component_class) {
		BT_LOGW_STR("Invalid parameter: component class is NULL.");
		ret = -1;
		goto end;
	}

	if (!method) {
		BT_LOGW_STR("Invalid parameter: method is NULL.");
		ret = -1;
		goto end;
	}

	if (component_class->frozen) {
		BT_LOGW("Invalid parameter: component class is frozen: "
			"addr=%p, name=\"%s\", type=%s",
			component_class,
			bt_component_class_get_name(component_class),
			bt_component_class_type_string(component_class->type));
		ret = -1;
		goto end;
	}

	component_class->methods.port_disconnected = method;
	BT_LOGV("Set component class's \"port disconnected\" method: "
		"addr=%p, name=\"%s\", type=%s, method-addr=%p",
		component_class,
		bt_component_class_get_name(component_class),
		bt_component_class_type_string(component_class->type),
		method);

end:
	return ret;
}

int bt_component_class_set_finalize_method(
		struct bt_component_class *component_class,
		bt_component_class_finalize_method method)
{
	int ret = 0;

	if (!component_class) {
		BT_LOGW_STR("Invalid parameter: component class is NULL.");
		ret = -1;
		goto end;
	}

	if (!method) {
		BT_LOGW_STR("Invalid parameter: method is NULL.");
		ret = -1;
		goto end;
	}

	if (component_class->frozen) {
		BT_LOGW("Invalid parameter: component class is frozen: "
			"addr=%p, name=\"%s\", type=%s",
			component_class,
			bt_component_class_get_name(component_class),
			bt_component_class_type_string(component_class->type));
		ret = -1;
		goto end;
	}

	component_class->methods.finalize = method;
	BT_LOGV("Set component class's finalization method: "
		"addr=%p, name=\"%s\", type=%s, method-addr=%p",
		component_class,
		bt_component_class_get_name(component_class),
		bt_component_class_type_string(component_class->type),
		method);

end:
	return ret;
}

int bt_component_class_source_set_notification_iterator_init_method(
		struct bt_component_class *component_class,
		bt_component_class_notification_iterator_init_method method)
{
	struct bt_component_class_source *source_class;
	int ret = 0;

	if (!component_class) {
		BT_LOGW_STR("Invalid parameter: component class is NULL.");
		ret = -1;
		goto end;
	}

	if (!method) {
		BT_LOGW_STR("Invalid parameter: method is NULL.");
		ret = -1;
		goto end;
	}

	if (component_class->type != BT_COMPONENT_CLASS_TYPE_SOURCE) {
		BT_LOGW("Invalid parameter: component class is not a source component class: "
			"addr=%p, name=\"%s\", type=%s",
			component_class,
			bt_component_class_get_name(component_class),
			bt_component_class_type_string(component_class->type));
		ret = -1;
		goto end;
	}

	if (component_class->frozen) {
		BT_LOGW("Invalid parameter: component class is frozen: "
			"addr=%p, name=\"%s\", type=%s",
			component_class,
			bt_component_class_get_name(component_class),
			bt_component_class_type_string(component_class->type));
		ret = -1;
		goto end;
	}

	source_class = container_of(component_class,
		struct bt_component_class_source, parent);
	source_class->methods.iterator.init = method;
	BT_LOGV("Set filter component class's notification iterator initialization method: "
		"addr=%p, name=\"%s\", method-addr=%p",
		component_class,
		bt_component_class_get_name(component_class),
		method);

end:
	return ret;
}

int bt_component_class_source_set_notification_iterator_finalize_method(
		struct bt_component_class *component_class,
		bt_component_class_notification_iterator_finalize_method method)
{
	struct bt_component_class_source *source_class;
	int ret = 0;

	if (!component_class) {
		BT_LOGW_STR("Invalid parameter: component class is NULL.");
		ret = -1;
		goto end;
	}

	if (!method) {
		BT_LOGW_STR("Invalid parameter: method is NULL.");
		ret = -1;
		goto end;
	}

	if (component_class->type != BT_COMPONENT_CLASS_TYPE_SOURCE) {
		BT_LOGW("Invalid parameter: component class is not a source component class: "
			"addr=%p, name=\"%s\", type=%s",
			component_class,
			bt_component_class_get_name(component_class),
			bt_component_class_type_string(component_class->type));
		ret = -1;
		goto end;
	}

	if (component_class->frozen) {
		BT_LOGW("Invalid parameter: component class is frozen: "
			"addr=%p, name=\"%s\", type=%s",
			component_class,
			bt_component_class_get_name(component_class),
			bt_component_class_type_string(component_class->type));
		ret = -1;
		goto end;
	}

	source_class = container_of(component_class,
		struct bt_component_class_source, parent);
	source_class->methods.iterator.finalize =
		method;
	BT_LOGV("Set filter component class's notification iterator finalization method: "
		"addr=%p, name=\"%s\", method-addr=%p",
		component_class,
		bt_component_class_get_name(component_class),
		method);

end:
	return ret;
}

int bt_component_class_filter_set_notification_iterator_init_method(
		struct bt_component_class *component_class,
		bt_component_class_notification_iterator_init_method method)
{
	struct bt_component_class_filter *filter_class;
	int ret = 0;

	if (!component_class) {
		BT_LOGW_STR("Invalid parameter: component class is NULL.");
		ret = -1;
		goto end;
	}

	if (!method) {
		BT_LOGW_STR("Invalid parameter: method is NULL.");
		ret = -1;
		goto end;
	}

	if (component_class->type != BT_COMPONENT_CLASS_TYPE_FILTER) {
		BT_LOGW("Invalid parameter: component class is not a filter component class: "
			"addr=%p, name=\"%s\", type=%s",
			component_class,
			bt_component_class_get_name(component_class),
			bt_component_class_type_string(component_class->type));
		ret = -1;
		goto end;
	}

	if (component_class->frozen) {
		BT_LOGW("Invalid parameter: component class is frozen: "
			"addr=%p, name=\"%s\", type=%s",
			component_class,
			bt_component_class_get_name(component_class),
			bt_component_class_type_string(component_class->type));
		ret = -1;
		goto end;
	}

	filter_class = container_of(component_class,
		struct bt_component_class_filter, parent);
	filter_class->methods.iterator.init = method;
	BT_LOGV("Set filter component class's notification iterator initialization method: "
		"addr=%p, name=\"%s\", method-addr=%p",
		component_class,
		bt_component_class_get_name(component_class),
		method);

end:
	return ret;
}

int bt_component_class_filter_set_notification_iterator_finalize_method(
		struct bt_component_class *component_class,
		bt_component_class_notification_iterator_finalize_method method)
{
	struct bt_component_class_filter *filter_class;
	int ret = 0;

	if (!component_class) {
		BT_LOGW_STR("Invalid parameter: component class is NULL.");
		ret = -1;
		goto end;
	}

	if (!method) {
		BT_LOGW_STR("Invalid parameter: method is NULL.");
		ret = -1;
		goto end;
	}

	if (component_class->type != BT_COMPONENT_CLASS_TYPE_FILTER) {
		BT_LOGW("Invalid parameter: component class is not a filter component class: "
			"addr=%p, name=\"%s\", type=%s",
			component_class,
			bt_component_class_get_name(component_class),
			bt_component_class_type_string(component_class->type));
		ret = -1;
		goto end;
	}

	if (component_class->frozen) {
		BT_LOGW("Invalid parameter: component class is frozen: "
			"addr=%p, name=\"%s\", type=%s",
			component_class,
			bt_component_class_get_name(component_class),
			bt_component_class_type_string(component_class->type));
		ret = -1;
		goto end;
	}

	filter_class = container_of(component_class,
		struct bt_component_class_filter, parent);
	filter_class->methods.iterator.finalize =
		method;
	BT_LOGV("Set filter component class's notification iterator finalization method: "
		"addr=%p, name=\"%s\", method-addr=%p",
		component_class,
		bt_component_class_get_name(component_class),
		method);

end:
	return ret;
}

int bt_component_class_set_description(
		struct bt_component_class *component_class,
		const char *description)
{
	int ret = 0;

	if (!component_class) {
		BT_LOGW_STR("Invalid parameter: component class is NULL.");
		ret = -1;
		goto end;
	}

	if (!description) {
		BT_LOGW_STR("Invalid parameter: description is NULL.");
		ret = -1;
		goto end;
	}

	if (component_class->frozen) {
		BT_LOGW("Invalid parameter: component class is frozen: "
			"addr=%p, name=\"%s\", type=%s",
			component_class,
			bt_component_class_get_name(component_class),
			bt_component_class_type_string(component_class->type));
		ret = -1;
		goto end;
	}

	g_string_assign(component_class->description, description);
	BT_LOGV("Set component class's description: "
		"addr=%p, name=\"%s\", type=%s",
		component_class,
		bt_component_class_get_name(component_class),
		bt_component_class_type_string(component_class->type));

end:
	return ret;
}

int bt_component_class_set_help(
		struct bt_component_class *component_class,
		const char *help)
{
	int ret = 0;

	if (!component_class) {
		BT_LOGW_STR("Invalid parameter: component class is NULL.");
		ret = -1;
		goto end;
	}

	if (!help) {
		BT_LOGW_STR("Invalid parameter: help is NULL.");
		ret = -1;
		goto end;
	}

	if (component_class->frozen) {
		BT_LOGW("Invalid parameter: component class is frozen: "
			"addr=%p, name=\"%s\", type=%s",
			component_class,
			bt_component_class_get_name(component_class),
			bt_component_class_type_string(component_class->type));
		ret = -1;
		goto end;
	}

	g_string_assign(component_class->help, help);
	BT_LOGV("Set component class's help text: "
		"addr=%p, name=\"%s\", type=%s",
		component_class,
		bt_component_class_get_name(component_class),
		bt_component_class_type_string(component_class->type));

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

	BT_ASSERT(class);
	BT_ASSERT(func);
	listener.func = func;
	listener.data = data;
	g_array_append_val(class->destroy_listeners, listener);
	BT_LOGV("Component class has no registered query method: "
		"comp-class-addr=%p, comp-class-name=\"%s\", comp-class-type=%s"
		"func-addr=%p, data-addr=%p",
		class,
		bt_component_class_get_name(class),
		bt_component_class_type_string(class->type),
		func, data);
}

int bt_component_class_freeze(
		struct bt_component_class *component_class)
{
	int ret = 0;

	if (!component_class) {
		BT_LOGW_STR("Invalid parameter: component class is NULL.");
		ret = -1;
		goto end;
	}

	if (component_class->frozen) {
		goto end;
	}

	BT_LOGD("Freezing component class: "
		"addr=%p, name=\"%s\", type=%s",
		component_class,
		bt_component_class_get_name(component_class),
		bt_component_class_type_string(component_class->type));
	component_class->frozen = BT_TRUE;

end:
	return ret;
}
