/*
 * plugin.c
 *
 * Babeltrace Debug Info Plug-in
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

#include <babeltrace/graph/notification-iterator.h>
#include <babeltrace/graph/private-notification-iterator.h>
#include <babeltrace/graph/notification.h>
#include <babeltrace/graph/notification-event.h>
#include <babeltrace/graph/notification-stream.h>
#include <babeltrace/graph/notification-packet.h>
#include <babeltrace/graph/component-filter.h>
#include <babeltrace/graph/private-component-filter.h>
#include <babeltrace/graph/private-port.h>
#include <babeltrace/graph/private-connection.h>
#include <babeltrace/graph/private-component.h>
#include <babeltrace/plugin/plugin-dev.h>
#include <plugins-common.h>
#include <assert.h>
#include "debug-info.h"
#include "copy.h"

static
void destroy_debug_info_data(struct debug_info_component *debug_info)
{
	free(debug_info->arg_debug_info_field_name);
	g_free(debug_info);
}

static
void destroy_debug_info_component(struct bt_private_component *component)
{
	void *data = bt_private_component_get_user_data(component);
	destroy_debug_info_data(data);
}

static
struct debug_info_component *create_debug_info_component_data(void)
{
	struct debug_info_component *debug_info;

	debug_info = g_new0(struct debug_info_component, 1);
	if (!debug_info) {
		goto end;
	}
	debug_info->err = stderr;

end:
	return debug_info;
}

static
void unref_debug_info(struct debug_info *debug_info)
{
	debug_info_destroy(debug_info);
}

static
void unref_trace(struct bt_ctf_trace *trace)
{
	bt_put(trace);
}

static
void unref_stream(struct bt_ctf_stream *stream)
{
	bt_put(stream);
}

static
void unref_packet(struct bt_ctf_packet *packet)
{
	bt_put(packet);
}

static
void unref_stream_class(struct bt_ctf_stream_class *stream_class)
{
	bt_put(stream_class);
}

static
void debug_info_iterator_destroy(struct bt_private_notification_iterator *it)
{
	struct debug_info_iterator *it_data;

	it_data = bt_private_notification_iterator_get_user_data(it);
	assert(it_data);

	if (it_data->input_iterator_group) {
		g_ptr_array_free(it_data->input_iterator_group, TRUE);
	}
	bt_put(it_data->current_notification);
	bt_put(it_data->input_iterator);
	g_hash_table_destroy(it_data->trace_map);
	g_hash_table_destroy(it_data->stream_map);
	g_hash_table_destroy(it_data->stream_class_map);
	g_hash_table_destroy(it_data->packet_map);
	g_hash_table_destroy(it_data->trace_debug_map);
	g_free(it_data);
}

static
struct bt_notification *handle_notification(FILE *err,
		struct debug_info_iterator *debug_it,
		struct bt_notification *notification)
{
	struct bt_notification *new_notification = NULL;

	switch (bt_notification_get_type(notification)) {
	case BT_NOTIFICATION_TYPE_PACKET_BEGIN:
	{
		struct bt_ctf_packet *packet =
			bt_notification_packet_begin_get_packet(notification);
		struct bt_ctf_packet *writer_packet;

		if (!packet) {
			goto end;
		}

		writer_packet = debug_info_new_packet(debug_it, packet);
		assert(writer_packet);
		new_notification = bt_notification_packet_begin_create(
				writer_packet);
		assert(new_notification);
		bt_put(packet);
		break;
	}
	case BT_NOTIFICATION_TYPE_PACKET_END:
	{
		struct bt_ctf_packet *packet =
			bt_notification_packet_end_get_packet(notification);
		struct bt_ctf_packet *writer_packet;

		if (!packet) {
			goto end;
		}

		writer_packet = debug_info_close_packet(debug_it, packet);
		assert(writer_packet);
		new_notification = bt_notification_packet_end_create(
				writer_packet);
		assert(new_notification);
		bt_put(packet);
		bt_put(writer_packet);
		break;
	}
	case BT_NOTIFICATION_TYPE_EVENT:
	{
		struct bt_ctf_event *event = bt_notification_event_get_event(
				notification);
		struct bt_ctf_event *writer_event;
		struct bt_clock_class_priority_map *cc_prio_map =
			bt_notification_event_get_clock_class_priority_map(
					notification);

		if (!event) {
			goto end;
		}
		writer_event = debug_info_output_event(debug_it, event);
		assert(writer_event);
		new_notification = bt_notification_event_create(writer_event,
				cc_prio_map);
		bt_put(cc_prio_map);
		assert(new_notification);
		bt_put(event);
		bt_put(writer_event);
		break;
	}
	case BT_NOTIFICATION_TYPE_STREAM_END:
	{
		struct bt_ctf_stream *stream =
			bt_notification_stream_end_get_stream(notification);
		struct bt_ctf_stream *writer_stream;

		if (!stream) {
			goto end;
		}

		writer_stream = debug_info_stream_end(debug_it, stream);
		assert(writer_stream);
		new_notification = bt_notification_stream_end_create(
				writer_stream);
		assert(new_notification);
		bt_put(stream);
		bt_put(writer_stream);
		break;
	}
	default:
		puts("Unhandled notification type");
	}

end:
	return new_notification;
}

static
struct bt_notification_iterator_next_return debug_info_iterator_next(
		struct bt_private_notification_iterator *iterator)
{
	struct debug_info_iterator *debug_it = NULL;
	struct bt_private_component *component = NULL;
	struct debug_info_component *debug_info = NULL;
	struct bt_notification_iterator *source_it = NULL;
	struct bt_notification *notification;
	struct bt_notification_iterator_next_return ret = {
		.status = BT_NOTIFICATION_ITERATOR_STATUS_OK,
		.notification = NULL,
	};

	debug_it = bt_private_notification_iterator_get_user_data(iterator);
	assert(debug_it);

	component = bt_private_notification_iterator_get_private_component(iterator);
	assert(component);
	debug_info = bt_private_component_get_user_data(component);
	assert(debug_info);

	source_it = debug_it->input_iterator;

	ret.status = bt_notification_iterator_next(source_it);
	if (ret.status != BT_NOTIFICATION_ITERATOR_STATUS_OK) {
		goto end;
	}

	notification = bt_notification_iterator_get_notification(
			source_it);
	if (!notification) {
		ret.status = BT_NOTIFICATION_ITERATOR_STATUS_ERROR;
		goto end;
	}

	ret.notification = handle_notification(debug_info->err, debug_it,
			notification);
	assert(ret.notification);
	bt_put(notification);

end:
	bt_put(component);
	return ret;
}

/*
static
struct bt_notification *debug_info_iterator_get(
		struct bt_private_notification_iterator *iterator)
{
	struct debug_info_iterator *debug_it;

	debug_it = bt_private_notification_iterator_get_user_data(iterator);
	assert(debug_it);

	if (!debug_it->current_notification) {
		enum bt_notification_iterator_status it_ret;

		it_ret = debug_info_iterator_next(iterator);
		if (it_ret) {
			goto end;
		}
	}

end:
	return bt_get(debug_it->current_notification);
}
*/

static
enum bt_notification_iterator_status debug_info_iterator_seek_time(
		struct bt_private_notification_iterator *iterator, int64_t time)
{
	enum bt_notification_iterator_status ret;

	ret = BT_NOTIFICATION_ITERATOR_STATUS_OK;

	return ret;
}

static
enum bt_notification_iterator_status debug_info_iterator_init(
		struct bt_private_notification_iterator *iterator,
		struct bt_private_port *port)
{
	enum bt_notification_iterator_status ret =
		BT_NOTIFICATION_ITERATOR_STATUS_OK;
	enum bt_notification_iterator_status it_ret;
	struct bt_private_port *input_port = NULL;
	struct bt_private_connection *connection = NULL;
	struct bt_private_component *component =
		bt_private_notification_iterator_get_private_component(iterator);
	struct debug_info_iterator *it_data = g_new0(struct debug_info_iterator, 1);

	if (!it_data) {
		ret = BT_NOTIFICATION_ITERATOR_STATUS_NOMEM;
		goto end;
	}

	/* Create a new iterator on the upstream component. */
	input_port = bt_private_component_filter_get_input_private_port_by_name(
		component, "in");
	assert(input_port);
	connection = bt_private_port_get_private_connection(input_port);
	assert(connection);

	it_data->input_iterator = bt_private_connection_create_notification_iterator(
			connection, NULL);
	if (!it_data->input_iterator) {
		ret = BT_NOTIFICATION_ITERATOR_STATUS_NOMEM;
		goto end;
	}
	it_data->debug_info_component = (struct debug_info_component *)
		bt_private_component_get_user_data(component);

	it_data->err = it_data->debug_info_component->err;
	it_data->trace_map = g_hash_table_new_full(g_direct_hash,
			g_direct_equal, NULL, (GDestroyNotify) unref_trace);
	it_data->stream_map = g_hash_table_new_full(g_direct_hash,
			g_direct_equal, NULL, (GDestroyNotify) unref_stream);
	it_data->stream_class_map = g_hash_table_new_full(g_direct_hash,
			g_direct_equal, NULL, (GDestroyNotify) unref_stream_class);
	it_data->packet_map = g_hash_table_new_full(g_direct_hash,
			g_direct_equal, NULL, (GDestroyNotify) unref_packet);
	it_data->trace_debug_map = g_hash_table_new_full(g_direct_hash,
			g_direct_equal, NULL, (GDestroyNotify) unref_debug_info);

	it_ret = bt_private_notification_iterator_set_user_data(iterator, it_data);
	if (it_ret) {
		goto end;
	}

end:
	bt_put(connection);
	bt_put(input_port);
	return ret;
}

static
enum bt_component_status init_from_params(
		struct debug_info_component *debug_info_component,
		struct bt_value *params)
{
	struct bt_value *value = NULL;
	enum bt_component_status ret = BT_COMPONENT_STATUS_OK;

	assert(params);

        value = bt_value_map_get(params, "debug-info-field-name");
	if (value) {
		enum bt_value_status value_ret;
		const char *tmp;

		value_ret = bt_value_string_get(value, &tmp);
		if (value_ret) {
			ret = BT_COMPONENT_STATUS_INVALID;
			printf_error("Failed to retrieve debug-info-field-name value. "
					"Expecting a string");
		}
		strcpy(debug_info_component->arg_debug_info_field_name, tmp);
		bt_put(value);
	} else {
		debug_info_component->arg_debug_info_field_name =
			malloc(strlen("debug_info") + 1);
		if (!debug_info_component->arg_debug_info_field_name) {
			ret = BT_COMPONENT_STATUS_NOMEM;
			printf_error();
		}
		sprintf(debug_info_component->arg_debug_info_field_name,
				"debug_info");
	}
	if (ret != BT_COMPONENT_STATUS_OK) {
		goto end;
	}

        value = bt_value_map_get(params, "debug-dir");
	if (value) {
		enum bt_value_status value_ret;

		value_ret = bt_value_string_get(value,
				&debug_info_component->arg_debug_dir);
		if (value_ret) {
			ret = BT_COMPONENT_STATUS_INVALID;
			printf_error("Failed to retrieve debug-dir value. "
					"Expecting a string");
		}
	}
	bt_put(value);
	if (ret != BT_COMPONENT_STATUS_OK) {
		goto end;
	}

        value = bt_value_map_get(params, "target-prefix");
	if (value) {
		enum bt_value_status value_ret;

		value_ret = bt_value_string_get(value,
				&debug_info_component->arg_target_prefix);
		if (value_ret) {
			ret = BT_COMPONENT_STATUS_INVALID;
			printf_error("Failed to retrieve target-prefix value. "
					"Expecting a string");
		}
	}
	bt_put(value);
	if (ret != BT_COMPONENT_STATUS_OK) {
		goto end;
	}

        value = bt_value_map_get(params, "full-path");
	if (value) {
		enum bt_value_status value_ret;

		value_ret = bt_value_bool_get(value,
				&debug_info_component->arg_full_path);
		if (value_ret) {
			ret = BT_COMPONENT_STATUS_INVALID;
			printf_error("Failed to retrieve full-path value. "
					"Expecting a boolean");
		}
	}
	bt_put(value);
	if (ret != BT_COMPONENT_STATUS_OK) {
		goto end;
	}

end:
	return ret;
}

enum bt_component_status debug_info_component_init(
	struct bt_private_component *component, struct bt_value *params,
	UNUSED_VAR void *init_method_data)
{
	enum bt_component_status ret;
	struct debug_info_component *debug_info = create_debug_info_component_data();

	if (!debug_info) {
		ret = BT_COMPONENT_STATUS_NOMEM;
		goto end;
	}

	ret = bt_private_component_set_user_data(component, debug_info);
	if (ret != BT_COMPONENT_STATUS_OK) {
		goto error;
	}

	ret = init_from_params(debug_info, params);
end:
	return ret;
error:
	destroy_debug_info_data(debug_info);
	return ret;
}

/* Initialize plug-in entry points. */
BT_PLUGIN(debug_info);
BT_PLUGIN_DESCRIPTION("Babeltrace Debug Informations Plug-In.");
BT_PLUGIN_AUTHOR("Julien Desfossez");
BT_PLUGIN_LICENSE("MIT");

BT_PLUGIN_FILTER_COMPONENT_CLASS(debug_info, debug_info_iterator_next);
BT_PLUGIN_FILTER_COMPONENT_CLASS_DESCRIPTION(debug_info,
	"Add the debug information to events if possible.");
BT_PLUGIN_FILTER_COMPONENT_CLASS_INIT_METHOD(debug_info, debug_info_component_init);
BT_PLUGIN_FILTER_COMPONENT_CLASS_FINALIZE_METHOD(debug_info, destroy_debug_info_component);
BT_PLUGIN_FILTER_COMPONENT_CLASS_NOTIFICATION_ITERATOR_INIT_METHOD(debug_info,
	debug_info_iterator_init);
BT_PLUGIN_FILTER_COMPONENT_CLASS_NOTIFICATION_ITERATOR_FINALIZE_METHOD(debug_info,
	debug_info_iterator_destroy);
BT_PLUGIN_FILTER_COMPONENT_CLASS_NOTIFICATION_ITERATOR_SEEK_TIME_METHOD(debug_info,
	debug_info_iterator_seek_time);
