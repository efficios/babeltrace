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

#define BT_LOG_TAG "PLUGIN-CTF-LTTNG-UTILS-DEBUG-INFO-FLT"
#include "logging.h"

#include <babeltrace/babeltrace.h>
#include <plugins-common.h>
#include <babeltrace/assert-internal.h>
#include "debug-info.h"
#include "copy.h"

static
gboolean empty_trace_map(gpointer key, gpointer value, gpointer user_data)
{
	struct debug_info_trace *di_trace = value;

	di_trace->trace_static = 1;
	debug_info_close_trace(di_trace->debug_it, di_trace);

	return TRUE;
}

static
void destroy_debug_info_data(struct debug_info_component *debug_info)
{
	free(debug_info->arg_debug_info_field_name);
	g_free(debug_info);
}

static
void destroy_debug_info_component(bt_self_component *component)
{
	void *data = bt_self_component_get_user_data(component);
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
void unref_trace(struct debug_info_trace *di_trace)
{
	bt_trace_put_ref(di_trace->writer_trace);
	g_free(di_trace);
}

static
void debug_info_iterator_destroy(bt_self_message_iterator *it)
{
	struct debug_info_iterator *it_data;

	it_data = bt_self_message_iterator_get_user_data(it);
	BT_ASSERT(it_data);

	if (it_data->input_iterator_group) {
		g_ptr_array_free(it_data->input_iterator_group, TRUE);
	}

	g_hash_table_foreach_remove(it_data->trace_map,
			empty_trace_map, it_data);
	g_hash_table_destroy(it_data->trace_map);

	bt_message_put_ref(it_data->current_message);
	bt_object_put_ref(it_data->input_iterator);

	g_free(it_data);
}

static
const bt_message *handle_message(FILE *err,
		struct debug_info_iterator *debug_it,
		const bt_message *message)
{
	const bt_message *new_message = NULL;

	switch (bt_message_get_type(message)) {
	case BT_MESSAGE_TYPE_PACKET_BEGINNING:
	{
		const bt_packet *packet =
			bt_message_packet_beginning_get_packet(message);
		const bt_packet *writer_packet;

		if (!packet) {
			goto end;
		}

		writer_packet = debug_info_new_packet(debug_it, packet);
		BT_ASSERT(writer_packet);
		new_message = bt_message_packet_beginning_create(
				writer_packet);
		BT_ASSERT(new_message);
		bt_packet_put_ref(packet);
		bt_packet_put_ref(writer_packet);
		break;
	}
	case BT_MESSAGE_TYPE_PACKET_END:
	{
		const bt_packet *packet =
			bt_message_packet_end_get_packet(message);
		const bt_packet *writer_packet;

		if (!packet) {
			goto end;
		}

		writer_packet = debug_info_close_packet(debug_it, packet);
		BT_ASSERT(writer_packet);
		new_message = bt_message_packet_end_create(
				writer_packet);
		BT_ASSERT(new_message);
		bt_packet_put_ref(packet);
		bt_packet_put_ref(writer_packet);
		break;
	}
	case BT_MESSAGE_TYPE_EVENT:
	{
		const bt_event *event = bt_message_event_get_event(
				message);
		const bt_event *writer_event;
		bt_clock_class_priority_map *cc_prio_map =
			bt_message_event_get_clock_class_priority_map(
					message);

		if (!event) {
			goto end;
		}
		writer_event = debug_info_output_event(debug_it, event);
		BT_ASSERT(writer_event);
		new_message = bt_message_event_create(writer_event,
				cc_prio_map);
		bt_object_put_ref(cc_prio_map);
		BT_ASSERT(new_message);
		bt_object_put_ref(event);
		bt_object_put_ref(writer_event);
		break;
	}
	case BT_MESSAGE_TYPE_STREAM_BEGINNING:
	{
		const bt_stream *stream =
			bt_message_stream_beginning_get_stream(message);
		const bt_stream *writer_stream;

		if (!stream) {
			goto end;
		}

		writer_stream = debug_info_stream_begin(debug_it, stream);
		BT_ASSERT(writer_stream);
		new_message = bt_message_stream_beginning_create(
				writer_stream);
		BT_ASSERT(new_message);
		bt_stream_put_ref(stream);
		bt_stream_put_ref(writer_stream);
		break;
	}
	case BT_MESSAGE_TYPE_STREAM_END:
	{
		const bt_stream *stream =
			bt_message_stream_end_get_stream(message);
		const bt_stream *writer_stream;

		if (!stream) {
			goto end;
		}

		writer_stream = debug_info_stream_end(debug_it, stream);
		BT_ASSERT(writer_stream);
		new_message = bt_message_stream_end_create(
				writer_stream);
		BT_ASSERT(new_message);
		bt_stream_put_ref(stream);
		bt_stream_put_ref(writer_stream);
		break;
	}
	default:
		new_message = bt_message_get_ref(message);
		break;
	}

end:
	return new_message;
}

static
bt_message_iterator_next_method_return debug_info_iterator_next(
		bt_self_message_iterator *iterator)
{
	struct debug_info_iterator *debug_it = NULL;
	bt_self_component *component = NULL;
	struct debug_info_component *debug_info = NULL;
	bt_message_iterator *source_it = NULL;
	const bt_message *message;
	bt_message_iterator_next_method_return ret = {
		.status = BT_MESSAGE_ITERATOR_STATUS_OK,
		.message = NULL,
	};

	debug_it = bt_self_message_iterator_get_user_data(iterator);
	BT_ASSERT(debug_it);

	component = bt_self_message_iterator_get_private_component(iterator);
	BT_ASSERT(component);
	debug_info = bt_self_component_get_user_data(component);
	BT_ASSERT(debug_info);

	source_it = debug_it->input_iterator;

	ret.status = bt_message_iterator_next(source_it);
	if (ret.status != BT_MESSAGE_ITERATOR_STATUS_OK) {
		goto end;
	}

	message = bt_message_iterator_get_message(
			source_it);
	if (!message) {
		ret.status = BT_MESSAGE_ITERATOR_STATUS_ERROR;
		goto end;
	}

	ret.message = handle_message(debug_info->err, debug_it,
			message);
	BT_ASSERT(ret.message);
	bt_message_put_ref(message);

end:
	bt_object_put_ref(component);
	return ret;
}

static
enum bt_message_iterator_status debug_info_iterator_init(
		bt_self_message_iterator *iterator,
		struct bt_private_port *port)
{
	enum bt_message_iterator_status ret =
		BT_MESSAGE_ITERATOR_STATUS_OK;
	enum bt_message_iterator_status it_ret;
	enum bt_connection_status conn_status;
	struct bt_private_connection *connection = NULL;
	bt_self_component *component =
		bt_self_message_iterator_get_private_component(iterator);
	struct debug_info_iterator *it_data = g_new0(struct debug_info_iterator, 1);
	struct bt_private_port *input_port;

	if (!it_data) {
		ret = BT_MESSAGE_ITERATOR_STATUS_NOMEM;
		goto end;
	}

	input_port = bt_self_component_filter_get_input_port_by_name(
			component, "in");
	if (!input_port) {
		ret = BT_MESSAGE_ITERATOR_STATUS_ERROR;
		goto end;
	}

	connection = bt_private_port_get_connection(input_port);
	bt_object_put_ref(input_port);
	if (!connection) {
		ret = BT_MESSAGE_ITERATOR_STATUS_ERROR;
		goto end;
	}

	conn_status = bt_private_connection_create_message_iterator(
			connection, &it_data->input_iterator);
	if (conn_status != BT_CONNECTION_STATUS_OK) {
		ret = BT_MESSAGE_ITERATOR_STATUS_ERROR;
		goto end;
	}

	it_data->debug_info_component = (struct debug_info_component *)
		bt_self_component_get_user_data(component);
	it_data->err = it_data->debug_info_component->err;
	it_data->trace_map = g_hash_table_new_full(g_direct_hash,
			g_direct_equal, NULL, (GDestroyNotify) unref_trace);

	it_ret = bt_self_message_iterator_set_user_data(iterator, it_data);
	if (it_ret) {
		goto end;
	}

end:
	bt_object_put_ref(connection);
	bt_object_put_ref(component);
	return ret;
}

static
enum bt_component_status init_from_params(
		struct debug_info_component *debug_info_component,
		bt_value *params)
{
	bt_value *value = NULL;
	enum bt_component_status ret = BT_COMPONENT_STATUS_OK;

	BT_ASSERT(params);

        value = bt_value_map_get(params, "debug-info-field-name");
	if (value) {
		enum bt_value_status value_ret;
		const char *tmp;

		tmp = bt_value_string_get(value);
		strcpy(debug_info_component->arg_debug_info_field_name, tmp);
		bt_value_put_ref(value);
	} else {
		debug_info_component->arg_debug_info_field_name =
			malloc(strlen("debug_info") + 1);
		if (!debug_info_component->arg_debug_info_field_name) {
			ret = BT_COMPONENT_STATUS_NOMEM;
			BT_LOGE_STR("Missing field name.");
			goto end;
		}
		sprintf(debug_info_component->arg_debug_info_field_name,
				"debug_info");
	}
	if (ret != BT_COMPONENT_STATUS_OK) {
		goto end;
	}

        value = bt_value_map_get(params, "debug-info-dir");
	if (value) {
		enum bt_value_status value_ret;

		debug_info_component->arg_debug_dir = bt_value_string_get(value);
	}
	bt_value_put_ref(value);
	if (ret != BT_COMPONENT_STATUS_OK) {
		goto end;
	}

        value = bt_value_map_get(params, "target-prefix");
	if (value) {
		enum bt_value_status value_ret;

		debug_info_component->arg_target_prefix = bt_value_string_get(value);
	}
	bt_value_put_ref(value);
	if (ret != BT_COMPONENT_STATUS_OK) {
		goto end;
	}

        value = bt_value_map_get(params, "full-path");
	if (value) {
		enum bt_value_status value_ret;
		bt_bool bool_val;

		bool_val = bt_value_bool_get(value);

		debug_info_component->arg_full_path = bool_val;
	}
	bt_value_put_ref(value);
	if (ret != BT_COMPONENT_STATUS_OK) {
		goto end;
	}

end:
	return ret;
}

enum bt_component_status debug_info_component_init(
	bt_self_component *component, bt_value *params,
	UNUSED_VAR void *init_method_data)
{
	enum bt_component_status ret;
	struct debug_info_component *debug_info = create_debug_info_component_data();

	if (!debug_info) {
		ret = BT_COMPONENT_STATUS_NOMEM;
		goto end;
	}

	ret = bt_self_component_set_user_data(component, debug_info);
	if (ret != BT_COMPONENT_STATUS_OK) {
		goto error;
	}

	ret = bt_self_component_filter_add_input_port(
		component, "in", NULL, NULL);
	if (ret != BT_COMPONENT_STATUS_OK) {
		goto end;
	}

	ret = bt_self_component_filter_add_output_port(
		component, "out", NULL, NULL);
	if (ret != BT_COMPONENT_STATUS_OK) {
		goto end;
	}

	ret = init_from_params(debug_info, params);
end:
	return ret;
error:
	destroy_debug_info_data(debug_info);
	return ret;
}

#ifndef BT_BUILT_IN_PLUGINS
BT_PLUGIN_MODULE();
#endif

/* Initialize plug-in entry points. */
BT_PLUGIN_WITH_ID(lttng_utils, "lttng-utils");
BT_PLUGIN_DESCRIPTION_WITH_ID(lttng_utils, "LTTng utilities");
BT_PLUGIN_AUTHOR_WITH_ID(lttng_utils, "Julien Desfossez");
BT_PLUGIN_LICENSE_WITH_ID(lttng_utils, "MIT");

BT_PLUGIN_FILTER_COMPONENT_CLASS_WITH_ID(lttng_utils, debug_info, "debug-info",
	debug_info_iterator_next);
BT_PLUGIN_FILTER_COMPONENT_CLASS_DESCRIPTION_WITH_ID(lttng_utils, debug_info,
	"Augment compatible events with debugging information.");
BT_PLUGIN_FILTER_COMPONENT_CLASS_INIT_METHOD_WITH_ID(lttng_utils,
	debug_info, debug_info_component_init);
BT_PLUGIN_FILTER_COMPONENT_CLASS_FINALIZE_METHOD_WITH_ID(lttng_utils,
	debug_info, destroy_debug_info_component);
BT_PLUGIN_FILTER_COMPONENT_CLASS_MESSAGE_ITERATOR_INIT_METHOD_WITH_ID(
	lttng_utils, debug_info, debug_info_iterator_init);
BT_PLUGIN_FILTER_COMPONENT_CLASS_MESSAGE_ITERATOR_FINALIZE_METHOD_WITH_ID(
	lttng_utils, debug_info, debug_info_iterator_destroy);
