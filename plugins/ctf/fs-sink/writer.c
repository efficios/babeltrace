/*
 * writer.c
 *
 * Babeltrace CTF Writer Output Plugin
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

#include <babeltrace/ctf-ir/packet.h>
#include <babeltrace/plugin/plugin-dev.h>
#include <babeltrace/graph/connection.h>
#include <babeltrace/graph/component.h>
#include <babeltrace/graph/private-component.h>
#include <babeltrace/graph/component-sink.h>
#include <babeltrace/graph/private-component-sink.h>
#include <babeltrace/graph/private-port.h>
#include <babeltrace/graph/private-connection.h>
#include <babeltrace/graph/notification.h>
#include <babeltrace/graph/notification-iterator.h>
#include <babeltrace/graph/notification-event.h>
#include <babeltrace/graph/notification-packet.h>
#include <babeltrace/graph/notification-stream.h>
#include <plugins-common.h>
#include <stdio.h>
#include <stdbool.h>
#include <glib.h>
#include "writer.h"
#include <assert.h>

static
gboolean empty_trace_map(gpointer key, gpointer value, gpointer user_data)
{
	struct fs_writer *fs_writer = value;
	struct writer_component *writer_component = user_data;

	fs_writer->trace_static = 1;
	writer_close(writer_component, fs_writer);

	return TRUE;
}

static
void destroy_writer_component_data(struct writer_component *writer_component)
{
	bt_put(writer_component->input_iterator);

	g_hash_table_foreach_remove(writer_component->trace_map,
			empty_trace_map, writer_component);
	g_hash_table_destroy(writer_component->trace_map);

	g_string_free(writer_component->base_path, true);
	g_string_free(writer_component->trace_name_base, true);
}

BT_HIDDEN
void writer_component_finalize(struct bt_private_component *component)
{
	struct writer_component *writer_component = (struct writer_component *)
		bt_private_component_get_user_data(component);

	destroy_writer_component_data(writer_component);
	g_free(writer_component);
}

static
void free_fs_writer(struct fs_writer *fs_writer)
{
	bt_put(fs_writer->writer);
	g_free(fs_writer);
}

static
struct writer_component *create_writer_component(void)
{
	struct writer_component *writer_component;

	writer_component = g_new0(struct writer_component, 1);
	if (!writer_component) {
		goto end;
	}

	writer_component->err = stderr;
	writer_component->trace_id = 0;
	writer_component->trace_name_base = g_string_new("trace");
	if (!writer_component->trace_name_base) {
		g_free(writer_component);
		writer_component = NULL;
		goto end;
	}

	/*
	 * Reader to writer corresponding structures.
	 */
	writer_component->trace_map = g_hash_table_new_full(g_direct_hash,
			g_direct_equal, NULL, (GDestroyNotify) free_fs_writer);

end:
	return writer_component;
}

static
enum bt_component_status handle_notification(
		struct writer_component *writer_component,
		struct bt_notification *notification)
{
	enum bt_component_status ret = BT_COMPONENT_STATUS_OK;

	if (!writer_component) {
		ret = BT_COMPONENT_STATUS_ERROR;
		goto end;
	}

	switch (bt_notification_get_type(notification)) {
	case BT_NOTIFICATION_TYPE_PACKET_BEGIN:
	{
		struct bt_ctf_packet *packet =
			bt_notification_packet_begin_get_packet(notification);

		if (!packet) {
			ret = BT_COMPONENT_STATUS_ERROR;
			goto end;
		}

		ret = writer_new_packet(writer_component, packet);
		bt_put(packet);
		break;
	}
	case BT_NOTIFICATION_TYPE_PACKET_END:
	{
		struct bt_ctf_packet *packet =
			bt_notification_packet_end_get_packet(notification);

		if (!packet) {
			ret = BT_COMPONENT_STATUS_ERROR;
			goto end;
		}
		ret = writer_close_packet(writer_component, packet);
		bt_put(packet);
		break;
	}
	case BT_NOTIFICATION_TYPE_EVENT:
	{
		struct bt_ctf_event *event = bt_notification_event_get_event(
				notification);

		if (!event) {
			ret = BT_COMPONENT_STATUS_ERROR;
			goto end;
		}
		ret = writer_output_event(writer_component, event);
		bt_put(event);
		if (ret != BT_COMPONENT_STATUS_OK) {
			goto end;
		}
		break;
	}
	case BT_NOTIFICATION_TYPE_STREAM_BEGIN:
	{
		struct bt_ctf_stream *stream =
			bt_notification_stream_begin_get_stream(notification);

		if (!stream) {
			ret = BT_COMPONENT_STATUS_ERROR;
			goto end;
		}
		ret = writer_stream_begin(writer_component, stream);
		bt_put(stream);
		break;
	}
	case BT_NOTIFICATION_TYPE_STREAM_END:
	{
		struct bt_ctf_stream *stream =
			bt_notification_stream_end_get_stream(notification);

		if (!stream) {
			ret = BT_COMPONENT_STATUS_ERROR;
			goto end;
		}
		ret = writer_stream_end(writer_component, stream);
		bt_put(stream);
		break;
	}
	default:
		puts("Unhandled notification type");
	}
end:
	return ret;
}

BT_HIDDEN
void writer_component_port_connected(
		struct bt_private_component *component,
		struct bt_private_port *self_port,
		struct bt_port *other_port)
{
	struct bt_private_connection *connection;
	struct writer_component *writer;
	enum bt_connection_status conn_status;
	static const enum bt_notification_type notif_types[] = {
		BT_NOTIFICATION_TYPE_EVENT,
		BT_NOTIFICATION_TYPE_PACKET_BEGIN,
		BT_NOTIFICATION_TYPE_PACKET_END,
		BT_NOTIFICATION_TYPE_STREAM_BEGIN,
		BT_NOTIFICATION_TYPE_STREAM_END,
		BT_NOTIFICATION_TYPE_SENTINEL,
	};

	writer = bt_private_component_get_user_data(component);
	assert(writer);
	assert(!writer->input_iterator);
	connection = bt_private_port_get_private_connection(self_port);
	assert(connection);
	conn_status = bt_private_connection_create_notification_iterator(
		connection, notif_types, &writer->input_iterator);
	if (conn_status != BT_CONNECTION_STATUS_OK) {
		writer->error = true;
	}

	bt_put(connection);
}

BT_HIDDEN
enum bt_component_status writer_run(struct bt_private_component *component)
{
	enum bt_component_status ret;
	struct bt_notification *notification = NULL;
	struct bt_notification_iterator *it;
	struct writer_component *writer_component =
		bt_private_component_get_user_data(component);
	enum bt_notification_iterator_status it_ret;

	if (unlikely(writer_component->error)) {
		ret = BT_COMPONENT_STATUS_ERROR;
		goto end;
	}

	it = writer_component->input_iterator;
	assert(it);
	it_ret = bt_notification_iterator_next(it);

	switch (it_ret) {
	case BT_NOTIFICATION_ITERATOR_STATUS_END:
		ret = BT_COMPONENT_STATUS_END;
		BT_PUT(writer_component->input_iterator);
		goto end;
	case BT_NOTIFICATION_ITERATOR_STATUS_AGAIN:
		ret = BT_COMPONENT_STATUS_AGAIN;
		goto end;
	case BT_NOTIFICATION_ITERATOR_STATUS_OK:
		break;
	default:
		ret = BT_COMPONENT_STATUS_ERROR;
		goto end;
	}

	notification = bt_notification_iterator_get_notification(it);
	assert(notification);
	ret = handle_notification(writer_component, notification);
end:
	bt_put(notification);
	return ret;
}

static
enum bt_component_status apply_one_bool(const char *key,
		struct bt_value *params,
		bool *option,
		bool *found)
{
	enum bt_component_status ret = BT_COMPONENT_STATUS_OK;
	struct bt_value *value = NULL;
	enum bt_value_status status;
	bt_bool bool_val;

	value = bt_value_map_get(params, key);
	if (!value) {
		goto end;
	}
	status = bt_value_bool_get(value, &bool_val);
	switch (status) {
	case BT_VALUE_STATUS_OK:
		break;
	default:
		ret = BT_COMPONENT_STATUS_ERROR;
		goto end;
	}
	*option = (bool) bool_val;
	if (found) {
		*found = true;
	}
end:
	bt_put(value);
	return ret;
}

BT_HIDDEN
enum bt_component_status writer_component_init(
	struct bt_private_component *component, struct bt_value *params,
	UNUSED_VAR void *init_method_data)
{
	enum bt_component_status ret;
	enum bt_value_status value_ret;
	struct writer_component *writer_component = create_writer_component();
	struct bt_value *value = NULL;
	const char *path;

	if (!writer_component) {
		ret = BT_COMPONENT_STATUS_NOMEM;
		goto end;
	}

	ret = bt_private_component_sink_add_input_private_port(component,
		"in", NULL, NULL);
	if (ret != BT_COMPONENT_STATUS_OK) {
		goto end;
	}

	value = bt_value_map_get(params, "path");
	if (!value || bt_value_is_null(value) || !bt_value_is_string(value)) {
		fprintf(writer_component->err,
				"[error] output path parameter required\n");
		ret = BT_COMPONENT_STATUS_INVALID;
		goto error;
	}

	value_ret = bt_value_string_get(value, &path);
	if (value_ret != BT_VALUE_STATUS_OK) {
		ret = BT_COMPONENT_STATUS_INVALID;
		goto error;
	}
	bt_put(value);

	writer_component->base_path = g_string_new(path);
	if (!writer_component) {
		ret = BT_COMPONENT_STATUS_ERROR;
		goto error;
	}

	writer_component->single_trace = false;
	ret = apply_one_bool("single-trace", params,
			&writer_component->single_trace, NULL);
	if (ret != BT_COMPONENT_STATUS_OK) {
		goto end;
	}

	ret = bt_private_component_set_user_data(component, writer_component);
	if (ret != BT_COMPONENT_STATUS_OK) {
		goto error;
	}

end:
	return ret;
error:
	destroy_writer_component_data(writer_component);
	g_free(writer_component);
	return ret;
}
