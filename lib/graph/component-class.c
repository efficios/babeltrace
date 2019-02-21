/*
 * Copyright 2017-2018 Philippe Proulx <pproulx@efficios.com>
 * Copyright 2016 Jérémie Galarneau <jeremie.galarneau@efficios.com>
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

#include <babeltrace/assert-internal.h>
#include <babeltrace/assert-pre-internal.h>
#include <babeltrace/compiler-internal.h>
#include <babeltrace/graph/component-class.h>
#include <babeltrace/graph/component-class-const.h>
#include <babeltrace/graph/component-class-source.h>
#include <babeltrace/graph/component-class-source-const.h>
#include <babeltrace/graph/component-class-filter.h>
#include <babeltrace/graph/component-class-filter-const.h>
#include <babeltrace/graph/component-class-sink.h>
#include <babeltrace/graph/component-class-sink-const.h>
#include <babeltrace/graph/component-class-internal.h>
#include <babeltrace/types.h>
#include <glib.h>

#define BT_ASSERT_PRE_COMP_CLS_HOT(_cc) \
	BT_ASSERT_PRE_HOT(((const struct bt_component_class *) (_cc)),	\
		"Component class", ": %!+C", (_cc))

static
void destroy_component_class(struct bt_object *obj)
{
	struct bt_component_class *class;
	int i;

	BT_ASSERT(obj);
	class = container_of(obj, struct bt_component_class, base);

	BT_LIB_LOGD("Destroying component class: %!+C", class);

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
		class->name = NULL;
	}

	if (class->description) {
		g_string_free(class->description, TRUE);
		class->description = NULL;
	}

	if (class->help) {
		g_string_free(class->help, TRUE);
		class->help = NULL;
	}

	if (class->destroy_listeners) {
		g_array_free(class->destroy_listeners, TRUE);
		class->destroy_listeners = NULL;
	}

	g_free(class);
}

static
int bt_component_class_init(struct bt_component_class *class,
		enum bt_component_class_type type, const char *name)
{
	int ret = 0;

	bt_object_init_shared(&class->base, destroy_component_class);
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
	BT_OBJECT_PUT_REF_AND_RESET(class);
	ret = -1;

end:
	return ret;
}

struct bt_component_class_source *bt_component_class_source_create(
		const char *name,
		bt_component_class_source_message_iterator_next_method method)
{
	struct bt_component_class_source *source_class = NULL;
	int ret;

	BT_ASSERT_PRE_NON_NULL(name, "Name");
	BT_ASSERT_PRE_NON_NULL(method, "Message iterator next method");
	BT_LOGD("Creating source component class: "
		"name=\"%s\", msg-iter-next-method-addr=%p",
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

	source_class->methods.msg_iter_next = method;
	BT_LIB_LOGD("Created source component class: %!+C", source_class);

end:
	return (void *) source_class;
}

struct bt_component_class_filter *bt_component_class_filter_create(
		const char *name,
		bt_component_class_filter_message_iterator_next_method method)
{
	struct bt_component_class_filter *filter_class = NULL;
	int ret;

	BT_ASSERT_PRE_NON_NULL(name, "Name");
	BT_ASSERT_PRE_NON_NULL(method, "Message iterator next method");
	BT_LOGD("Creating filter component class: "
		"name=\"%s\", msg-iter-next-method-addr=%p",
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

	filter_class->methods.msg_iter_next = method;
	BT_LIB_LOGD("Created filter component class: %!+C", filter_class);

end:
	return (void *) filter_class;
}

struct bt_component_class_sink *bt_component_class_sink_create(
		const char *name, bt_component_class_sink_consume_method method)
{
	struct bt_component_class_sink *sink_class = NULL;
	int ret;

	BT_ASSERT_PRE_NON_NULL(name, "Name");
	BT_ASSERT_PRE_NON_NULL(method, "Consume next method");
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
	BT_LIB_LOGD("Created sink component class: %!+C", sink_class);

end:
	return (void *) sink_class;
}

enum bt_component_class_status
bt_component_class_source_set_init_method(
		struct bt_component_class_source *comp_cls,
		bt_component_class_source_init_method method)
{
	BT_ASSERT_PRE_NON_NULL(comp_cls, "Component class");
	BT_ASSERT_PRE_NON_NULL(method, "Method");
	BT_ASSERT_PRE_COMP_CLS_HOT(comp_cls);
	comp_cls->methods.init = method;
	BT_LIB_LOGV("Set source component class's initialization method: "
		"%!+C", comp_cls);
	return BT_COMPONENT_CLASS_STATUS_OK;
}

enum bt_component_class_status
bt_component_class_filter_set_init_method(
		struct bt_component_class_filter *comp_cls,
		bt_component_class_filter_init_method method)
{
	BT_ASSERT_PRE_NON_NULL(comp_cls, "Component class");
	BT_ASSERT_PRE_NON_NULL(method, "Method");
	BT_ASSERT_PRE_COMP_CLS_HOT(comp_cls);
	comp_cls->methods.init = method;
	BT_LIB_LOGV("Set filter component class's initialization method: "
		"%!+C", comp_cls);
	return BT_COMPONENT_CLASS_STATUS_OK;
}

enum bt_component_class_status
bt_component_class_sink_set_init_method(
		struct bt_component_class_sink *comp_cls,
		bt_component_class_sink_init_method method)
{
	BT_ASSERT_PRE_NON_NULL(comp_cls, "Component class");
	BT_ASSERT_PRE_NON_NULL(method, "Method");
	BT_ASSERT_PRE_COMP_CLS_HOT(comp_cls);
	comp_cls->methods.init = method;
	BT_LIB_LOGV("Set sink component class's initialization method: "
		"%!+C", comp_cls);
	return BT_COMPONENT_CLASS_STATUS_OK;
}

enum bt_component_class_status
bt_component_class_source_set_finalize_method(
		struct bt_component_class_source *comp_cls,
		bt_component_class_source_finalize_method method)
{
	BT_ASSERT_PRE_NON_NULL(comp_cls, "Component class");
	BT_ASSERT_PRE_NON_NULL(method, "Method");
	BT_ASSERT_PRE_COMP_CLS_HOT(comp_cls);
	comp_cls->methods.finalize = method;
	BT_LIB_LOGV("Set source component class's finalization method: "
		"%!+C", comp_cls);
	return BT_COMPONENT_CLASS_STATUS_OK;
}

enum bt_component_class_status
bt_component_class_filter_set_finalize_method(
		struct bt_component_class_filter *comp_cls,
		bt_component_class_filter_finalize_method method)
{
	BT_ASSERT_PRE_NON_NULL(comp_cls, "Component class");
	BT_ASSERT_PRE_NON_NULL(method, "Method");
	BT_ASSERT_PRE_COMP_CLS_HOT(comp_cls);
	comp_cls->methods.finalize = method;
	BT_LIB_LOGV("Set filter component class's finalization method: "
		"%!+C", comp_cls);
	return BT_COMPONENT_CLASS_STATUS_OK;
}

enum bt_component_class_status
bt_component_class_sink_set_finalize_method(
		struct bt_component_class_sink *comp_cls,
		bt_component_class_sink_finalize_method method)
{
	BT_ASSERT_PRE_NON_NULL(comp_cls, "Component class");
	BT_ASSERT_PRE_NON_NULL(method, "Method");
	BT_ASSERT_PRE_COMP_CLS_HOT(comp_cls);
	comp_cls->methods.finalize = method;
	BT_LIB_LOGV("Set sink component class's finalization method: "
		"%!+C", comp_cls);
	return BT_COMPONENT_CLASS_STATUS_OK;
}

enum bt_component_class_status
bt_component_class_source_set_query_method(
		struct bt_component_class_source *comp_cls,
		bt_component_class_source_query_method method)
{
	BT_ASSERT_PRE_NON_NULL(comp_cls, "Component class");
	BT_ASSERT_PRE_NON_NULL(method, "Method");
	BT_ASSERT_PRE_COMP_CLS_HOT(comp_cls);
	comp_cls->methods.query = method;
	BT_LIB_LOGV("Set source component class's query method: "
		"%!+C", comp_cls);
	return BT_COMPONENT_CLASS_STATUS_OK;
}

enum bt_component_class_status
bt_component_class_filter_set_query_method(
		struct bt_component_class_filter *comp_cls,
		bt_component_class_filter_query_method method)
{
	BT_ASSERT_PRE_NON_NULL(comp_cls, "Component class");
	BT_ASSERT_PRE_NON_NULL(method, "Method");
	BT_ASSERT_PRE_COMP_CLS_HOT(comp_cls);
	comp_cls->methods.query = method;
	BT_LIB_LOGV("Set filter component class's query method: "
		"%!+C", comp_cls);
	return BT_COMPONENT_CLASS_STATUS_OK;
}

enum bt_component_class_status
bt_component_class_sink_set_query_method(
		struct bt_component_class_sink *comp_cls,
		bt_component_class_sink_query_method method)
{
	BT_ASSERT_PRE_NON_NULL(comp_cls, "Component class");
	BT_ASSERT_PRE_NON_NULL(method, "Method");
	BT_ASSERT_PRE_COMP_CLS_HOT(comp_cls);
	comp_cls->methods.query = method;
	BT_LIB_LOGV("Set sink component class's query method: "
		"%!+C", comp_cls);
	return BT_COMPONENT_CLASS_STATUS_OK;
}

enum bt_component_class_status
bt_component_class_filter_set_accept_input_port_connection_method(
		struct bt_component_class_filter *comp_cls,
		bt_component_class_filter_accept_input_port_connection_method method)
{
	BT_ASSERT_PRE_NON_NULL(comp_cls, "Component class");
	BT_ASSERT_PRE_NON_NULL(method, "Method");
	BT_ASSERT_PRE_COMP_CLS_HOT(comp_cls);
	comp_cls->methods.accept_input_port_connection = method;
	BT_LIB_LOGV("Set filter component class's \"accept input port connection\" method"
		": %!+C", comp_cls);
	return BT_COMPONENT_CLASS_STATUS_OK;
}

enum bt_component_class_status
bt_component_class_sink_set_accept_input_port_connection_method(
		struct bt_component_class_sink *comp_cls,
		bt_component_class_sink_accept_input_port_connection_method method)
{
	BT_ASSERT_PRE_NON_NULL(comp_cls, "Component class");
	BT_ASSERT_PRE_NON_NULL(method, "Method");
	BT_ASSERT_PRE_COMP_CLS_HOT(comp_cls);
	comp_cls->methods.accept_input_port_connection = method;
	BT_LIB_LOGV("Set sink component class's \"accept input port connection\" method"
		": %!+C", comp_cls);
	return BT_COMPONENT_CLASS_STATUS_OK;
}

enum bt_component_class_status
bt_component_class_source_set_accept_output_port_connection_method(
		struct bt_component_class_source *comp_cls,
		bt_component_class_source_accept_output_port_connection_method method)
{
	BT_ASSERT_PRE_NON_NULL(comp_cls, "Component class");
	BT_ASSERT_PRE_NON_NULL(method, "Method");
	BT_ASSERT_PRE_COMP_CLS_HOT(comp_cls);
	comp_cls->methods.accept_output_port_connection = method;
	BT_LIB_LOGV("Set source component class's \"accept output port connection\" method"
		": %!+C", comp_cls);
	return BT_COMPONENT_CLASS_STATUS_OK;
}

enum bt_component_class_status
bt_component_class_filter_set_accept_output_port_connection_method(
		struct bt_component_class_filter *comp_cls,
		bt_component_class_filter_accept_output_port_connection_method method)
{
	BT_ASSERT_PRE_NON_NULL(comp_cls, "Component class");
	BT_ASSERT_PRE_NON_NULL(method, "Method");
	BT_ASSERT_PRE_COMP_CLS_HOT(comp_cls);
	comp_cls->methods.accept_output_port_connection = method;
	BT_LIB_LOGV("Set filter component class's \"accept output port connection\" method"
		": %!+C", comp_cls);
	return BT_COMPONENT_CLASS_STATUS_OK;
}

enum bt_component_class_status
bt_component_class_filter_set_input_port_connected_method(
		struct bt_component_class_filter *comp_cls,
		bt_component_class_filter_input_port_connected_method method)
{
	BT_ASSERT_PRE_NON_NULL(comp_cls, "Component class");
	BT_ASSERT_PRE_NON_NULL(method, "Method");
	BT_ASSERT_PRE_COMP_CLS_HOT(comp_cls);
	comp_cls->methods.input_port_connected = method;
	BT_LIB_LOGV("Set filter component class's \"input port connected\" method"
		": %!+C", comp_cls);
	return BT_COMPONENT_CLASS_STATUS_OK;
}

enum bt_component_class_status
bt_component_class_sink_set_input_port_connected_method(
		struct bt_component_class_sink *comp_cls,
		bt_component_class_sink_input_port_connected_method method)
{
	BT_ASSERT_PRE_NON_NULL(comp_cls, "Component class");
	BT_ASSERT_PRE_NON_NULL(method, "Method");
	BT_ASSERT_PRE_COMP_CLS_HOT(comp_cls);
	comp_cls->methods.input_port_connected = method;
	BT_LIB_LOGV("Set sink component class's \"input port connected\" method"
		": %!+C", comp_cls);
	return BT_COMPONENT_CLASS_STATUS_OK;
}

enum bt_component_class_status
bt_component_class_source_set_output_port_connected_method(
		struct bt_component_class_source *comp_cls,
		bt_component_class_source_output_port_connected_method method)
{
	BT_ASSERT_PRE_NON_NULL(comp_cls, "Component class");
	BT_ASSERT_PRE_NON_NULL(method, "Method");
	BT_ASSERT_PRE_COMP_CLS_HOT(comp_cls);
	comp_cls->methods.output_port_connected = method;
	BT_LIB_LOGV("Set source component class's \"output port connected\" method"
		": %!+C", comp_cls);
	return BT_COMPONENT_CLASS_STATUS_OK;
}

enum bt_component_class_status
bt_component_class_filter_set_output_port_connected_method(
		struct bt_component_class_filter *comp_cls,
		bt_component_class_filter_output_port_connected_method method)
{
	BT_ASSERT_PRE_NON_NULL(comp_cls, "Component class");
	BT_ASSERT_PRE_NON_NULL(method, "Method");
	BT_ASSERT_PRE_COMP_CLS_HOT(comp_cls);
	comp_cls->methods.output_port_connected = method;
	BT_LIB_LOGV("Set filter component class's \"output port connected\" method"
		": %!+C", comp_cls);
	return BT_COMPONENT_CLASS_STATUS_OK;
}

enum bt_component_class_status
bt_component_class_sink_set_graph_is_configured_method(
		struct bt_component_class_sink *comp_cls,
		bt_component_class_sink_graph_is_configured_method method)
{
	BT_ASSERT_PRE_NON_NULL(comp_cls, "Component class");
	BT_ASSERT_PRE_NON_NULL(method, "Method");
	BT_ASSERT_PRE_COMP_CLS_HOT(comp_cls);
	comp_cls->methods.graph_is_configured = method;
	BT_LIB_LOGV("Set sink component class's \"graph is configured\" method"
		": %!+C", comp_cls);
	return BT_COMPONENT_CLASS_STATUS_OK;
}

int bt_component_class_source_set_message_iterator_init_method(
		struct bt_component_class_source *comp_cls,
		bt_component_class_source_message_iterator_init_method method)
{
	BT_ASSERT_PRE_NON_NULL(comp_cls, "Component class");
	BT_ASSERT_PRE_NON_NULL(method, "Method");
	BT_ASSERT_PRE_COMP_CLS_HOT(comp_cls);
	comp_cls->methods.msg_iter_init = method;
	BT_LIB_LOGV("Set source component class's message iterator initialization method"
		": %!+C", comp_cls);
	return BT_COMPONENT_CLASS_STATUS_OK;
}

enum bt_component_class_status
bt_component_class_filter_set_message_iterator_init_method(
		struct bt_component_class_filter *comp_cls,
		bt_component_class_filter_message_iterator_init_method method)
{
	BT_ASSERT_PRE_NON_NULL(comp_cls, "Component class");
	BT_ASSERT_PRE_NON_NULL(method, "Method");
	BT_ASSERT_PRE_COMP_CLS_HOT(comp_cls);
	comp_cls->methods.msg_iter_init = method;
	BT_LIB_LOGV("Set filter component class's message iterator initialization method"
		": %!+C", comp_cls);
	return BT_COMPONENT_CLASS_STATUS_OK;
}

enum bt_component_class_status
bt_component_class_source_set_message_iterator_finalize_method(
		struct bt_component_class_source *comp_cls,
		bt_component_class_source_message_iterator_finalize_method method)
{
	BT_ASSERT_PRE_NON_NULL(comp_cls, "Component class");
	BT_ASSERT_PRE_NON_NULL(method, "Method");
	BT_ASSERT_PRE_COMP_CLS_HOT(comp_cls);
	comp_cls->methods.msg_iter_finalize = method;
	BT_LIB_LOGV("Set source component class's message iterator finalization method"
		": %!+C", comp_cls);
	return BT_COMPONENT_CLASS_STATUS_OK;
}

enum bt_component_class_status
bt_component_class_filter_set_message_iterator_finalize_method(
		struct bt_component_class_filter *comp_cls,
		bt_component_class_filter_message_iterator_finalize_method method)
{
	BT_ASSERT_PRE_NON_NULL(comp_cls, "Component class");
	BT_ASSERT_PRE_NON_NULL(method, "Method");
	BT_ASSERT_PRE_COMP_CLS_HOT(comp_cls);
	comp_cls->methods.msg_iter_finalize = method;
	BT_LIB_LOGV("Set filter component class's message iterator finalization method"
		": %!+C", comp_cls);
	return BT_COMPONENT_CLASS_STATUS_OK;
}

enum bt_component_class_status
bt_component_class_filter_set_message_iterator_seek_ns_from_origin_method(
		struct bt_component_class_filter *comp_cls,
		bt_component_class_filter_message_iterator_seek_ns_from_origin_method method)
{
	BT_ASSERT_PRE_NON_NULL(comp_cls, "Component class");
	BT_ASSERT_PRE_NON_NULL(method, "Method");
	BT_ASSERT_PRE_COMP_CLS_HOT(comp_cls);
	comp_cls->methods.msg_iter_seek_ns_from_origin = method;
	BT_LIB_LOGV("Set filter component class's message iterator \"seek nanoseconds from origin\" method"
		": %!+C", comp_cls);
	return BT_COMPONENT_CLASS_STATUS_OK;
}

enum bt_component_class_status
bt_component_class_source_set_message_iterator_seek_ns_from_origin_method(
		struct bt_component_class_source *comp_cls,
		bt_component_class_source_message_iterator_seek_ns_from_origin_method method)
{
	BT_ASSERT_PRE_NON_NULL(comp_cls, "Component class");
	BT_ASSERT_PRE_NON_NULL(method, "Method");
	BT_ASSERT_PRE_COMP_CLS_HOT(comp_cls);
	comp_cls->methods.msg_iter_seek_ns_from_origin = method;
	BT_LIB_LOGV("Set source component class's message iterator \"seek nanoseconds from origin\" method"
		": %!+C", comp_cls);
	return BT_COMPONENT_CLASS_STATUS_OK;
}

enum bt_component_class_status
bt_component_class_filter_set_message_iterator_seek_beginning_method(
		struct bt_component_class_filter *comp_cls,
		bt_component_class_filter_message_iterator_seek_beginning_method method)
{
	BT_ASSERT_PRE_NON_NULL(comp_cls, "Component class");
	BT_ASSERT_PRE_NON_NULL(method, "Method");
	BT_ASSERT_PRE_COMP_CLS_HOT(comp_cls);
	comp_cls->methods.msg_iter_seek_beginning = method;
	BT_LIB_LOGV("Set filter component class's message iterator \"seek beginning\" method"
		": %!+C", comp_cls);
	return BT_COMPONENT_CLASS_STATUS_OK;
}

enum bt_component_class_status
bt_component_class_source_set_message_iterator_seek_beginning_method(
		struct bt_component_class_source *comp_cls,
		bt_component_class_source_message_iterator_seek_beginning_method method)
{
	BT_ASSERT_PRE_NON_NULL(comp_cls, "Component class");
	BT_ASSERT_PRE_NON_NULL(method, "Method");
	BT_ASSERT_PRE_COMP_CLS_HOT(comp_cls);
	comp_cls->methods.msg_iter_seek_beginning = method;
	BT_LIB_LOGV("Set source component class's message iterator \"seek beginning\" method"
		": %!+C", comp_cls);
	return BT_COMPONENT_CLASS_STATUS_OK;
}

enum bt_component_class_status
bt_component_class_filter_set_message_iterator_can_seek_beginning_method(
		struct bt_component_class_filter *comp_cls,
		bt_component_class_filter_message_iterator_can_seek_beginning_method method)
{
	BT_ASSERT_PRE_NON_NULL(comp_cls, "Component class");
	BT_ASSERT_PRE_NON_NULL(method, "Method");
	BT_ASSERT_PRE_COMP_CLS_HOT(comp_cls);
	comp_cls->methods.msg_iter_can_seek_beginning = method;
	BT_LIB_LOGV("Set filter component class's message iterator \"can seek beginning\" method"
		": %!+C", comp_cls);
	return BT_COMPONENT_CLASS_STATUS_OK;
}

enum bt_component_class_status
bt_component_class_source_set_message_iterator_can_seek_beginning_method(
		struct bt_component_class_source *comp_cls,
		bt_component_class_source_message_iterator_can_seek_beginning_method method)
{
	BT_ASSERT_PRE_NON_NULL(comp_cls, "Component class");
	BT_ASSERT_PRE_NON_NULL(method, "Method");
	BT_ASSERT_PRE_COMP_CLS_HOT(comp_cls);
	comp_cls->methods.msg_iter_can_seek_beginning = method;
	BT_LIB_LOGV("Set source component class's message iterator \"can seek beginning\" method"
		": %!+C", comp_cls);
	return BT_COMPONENT_CLASS_STATUS_OK;
}

enum bt_component_class_status
bt_component_class_filter_set_message_iterator_can_seek_ns_from_origin_method(
		struct bt_component_class_filter *comp_cls,
		bt_component_class_filter_message_iterator_can_seek_ns_from_origin_method method)
{
	BT_ASSERT_PRE_NON_NULL(comp_cls, "Component class");
	BT_ASSERT_PRE_NON_NULL(method, "Method");
	BT_ASSERT_PRE_COMP_CLS_HOT(comp_cls);
	comp_cls->methods.msg_iter_can_seek_ns_from_origin = method;
	BT_LIB_LOGV("Set filter component class's message iterator \"can seek nanoseconds from origin\" method"
		": %!+C", comp_cls);
	return BT_COMPONENT_CLASS_STATUS_OK;
}

enum bt_component_class_status
bt_component_class_source_set_message_iterator_can_seek_ns_from_origin_method(
		struct bt_component_class_source *comp_cls,
		bt_component_class_source_message_iterator_can_seek_ns_from_origin_method method)
{
	BT_ASSERT_PRE_NON_NULL(comp_cls, "Component class");
	BT_ASSERT_PRE_NON_NULL(method, "Method");
	BT_ASSERT_PRE_COMP_CLS_HOT(comp_cls);
	comp_cls->methods.msg_iter_can_seek_ns_from_origin = method;
	BT_LIB_LOGV("Set source component class's message iterator \"can seek nanoseconds from origin\" method"
		": %!+C", comp_cls);
	return BT_COMPONENT_CLASS_STATUS_OK;
}

bt_component_class_status bt_component_class_set_description(
		struct bt_component_class *comp_cls,
		const char *description)
{
	BT_ASSERT_PRE_NON_NULL(comp_cls, "Component class");
	BT_ASSERT_PRE_NON_NULL(description, "Description");
	BT_ASSERT_PRE_COMP_CLS_HOT(comp_cls);
	g_string_assign(comp_cls->description, description);
	BT_LIB_LOGV("Set component class's description: "
		"addr=%p, name=\"%s\", type=%s",
		comp_cls,
		bt_component_class_get_name(comp_cls),
		bt_component_class_type_string(comp_cls->type));
	return BT_COMPONENT_CLASS_STATUS_OK;
}

bt_component_class_status bt_component_class_set_help(
		struct bt_component_class *comp_cls,
		const char *help)
{
	BT_ASSERT_PRE_NON_NULL(comp_cls, "Component class");
	BT_ASSERT_PRE_NON_NULL(help, "Help");
	BT_ASSERT_PRE_COMP_CLS_HOT(comp_cls);
	g_string_assign(comp_cls->help, help);
	BT_LIB_LOGV("Set component class's help text: %!+C", comp_cls);
	return BT_COMPONENT_CLASS_STATUS_OK;
}

const char *bt_component_class_get_name(const struct bt_component_class *comp_cls)
{
	BT_ASSERT_PRE_NON_NULL(comp_cls, "Component class");
	return comp_cls->name->str;
}

enum bt_component_class_type bt_component_class_get_type(
		const struct bt_component_class *comp_cls)
{
	BT_ASSERT_PRE_NON_NULL(comp_cls, "Component class");
	return comp_cls->type;
}

const char *bt_component_class_get_description(
		const struct bt_component_class *comp_cls)
{
	BT_ASSERT_PRE_NON_NULL(comp_cls, "Component class");
	return comp_cls->description &&
		comp_cls->description->str[0] != '\0' ?
		comp_cls->description->str : NULL;
}

const char *bt_component_class_get_help(
		const struct bt_component_class *comp_cls)
{
	BT_ASSERT_PRE_NON_NULL(comp_cls, "Component class");
	return comp_cls->help &&
		comp_cls->help->str[0] != '\0' ? comp_cls->help->str : NULL;
}

BT_HIDDEN
void bt_component_class_add_destroy_listener(
		struct bt_component_class *comp_cls,
		bt_component_class_destroy_listener_func func, void *data)
{
	struct bt_component_class_destroy_listener listener;

	BT_ASSERT(comp_cls);
	BT_ASSERT(func);
	listener.func = func;
	listener.data = data;
	g_array_append_val(comp_cls->destroy_listeners, listener);
	BT_LIB_LOGV("Added destroy listener to component class: "
		"%![cc-]+C, listener-func-addr=%p", comp_cls, func);
}

BT_HIDDEN
void _bt_component_class_freeze(const struct bt_component_class *comp_cls)
{
	BT_ASSERT(comp_cls);
	BT_LIB_LOGD("Freezing component class: %!+C", comp_cls);
	((struct bt_component_class *) comp_cls)->frozen = true;
}

void bt_component_class_get_ref(
		const struct bt_component_class *component_class)
{
	bt_object_get_ref(component_class);
}

void bt_component_class_put_ref(
		const struct bt_component_class *component_class)
{
	bt_object_put_ref(component_class);
}

void bt_component_class_source_get_ref(
		const struct bt_component_class_source *component_class_source)
{
	bt_object_get_ref(component_class_source);
}

void bt_component_class_source_put_ref(
		const struct bt_component_class_source *component_class_source)
{
	bt_object_put_ref(component_class_source);
}

void bt_component_class_filter_get_ref(
		const struct bt_component_class_filter *component_class_filter)
{
	bt_object_get_ref(component_class_filter);
}

void bt_component_class_filter_put_ref(
		const struct bt_component_class_filter *component_class_filter)
{
	bt_object_put_ref(component_class_filter);
}

void bt_component_class_sink_get_ref(
		const struct bt_component_class_sink *component_class_sink)
{
	bt_object_get_ref(component_class_sink);
}

void bt_component_class_sink_put_ref(
		const struct bt_component_class_sink *component_class_sink)
{
	bt_object_put_ref(component_class_sink);
}
