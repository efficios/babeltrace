/*
 * copy.c
 *
 * Babeltrace Copy Trace Structure
 *
 * Copyright 2017 Julien Desfossez <jdesfossez@efficios.com>
 *
 * Author: Julien Desfossez <jdesfossez@efficios.com>
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

#define BT_LOG_TAG "PLUGIN-UTILS-TRIMMER-FLT-COPY"
#include "logging.h"

#include <assert.h>
#include <babeltrace/babeltrace.h>

#include <ctfcopytrace.h>
#include "iterator.h"

static
struct bt_ctf_packet *lookup_packet(struct trimmer_iterator *trim_it,
		struct bt_ctf_packet *packet)
{
	return (struct bt_ctf_packet *) g_hash_table_lookup(
			trim_it->packet_map,
			(gpointer) packet);
}

static
struct bt_ctf_packet *insert_new_packet(struct trimmer_iterator *trim_it,
		struct bt_ctf_packet *packet,
		struct bt_ctf_stream *stream)
{
	struct bt_ctf_packet *writer_packet = NULL;
	int ret;

	BT_LOGD_STR("Inserting a new packet.");
	writer_packet = bt_ctf_packet_create(stream);
	if (!writer_packet) {
		BT_LOGE_STR("Failed to create a new packet.");
		goto error;
	}

	ret = ctf_packet_copy_header(trim_it->err, packet, writer_packet);
	if (ret) {
		BT_LOGE_STR("Failed to copy packet header.");
		goto error;
	}

	g_hash_table_insert(trim_it->packet_map, (gpointer) packet,
			writer_packet);
	goto end;

error:
	BT_PUT(writer_packet);
end:
	return writer_packet;
}

BT_HIDDEN
enum bt_component_status update_packet_context_field(FILE *err,
		struct bt_ctf_packet *writer_packet,
		const char *name, int64_t value)
{
	enum bt_component_status ret;
	struct bt_ctf_field *packet_context = NULL, *writer_packet_context = NULL;
	struct bt_ctf_field_type *struct_type = NULL, *field_type = NULL;
	struct bt_ctf_field *field = NULL, *writer_field = NULL;
	int nr_fields, i, int_ret;

	BT_LOGD("Updating packet context field: name=%s", name);
	packet_context = bt_ctf_packet_get_context(writer_packet);
	assert(packet_context);

	struct_type = bt_ctf_field_get_type(packet_context);
	assert(struct_type);

	writer_packet_context = bt_ctf_packet_get_context(writer_packet);
	assert(writer_packet_context);

	nr_fields = bt_ctf_field_type_structure_get_field_count(struct_type);
	for (i = 0; i < nr_fields; i++) {
		const char *field_name;

		field = bt_ctf_field_structure_get_field_by_index(
				packet_context, i);
		if (!field) {
			BT_LOGE("Failed to get field in packet-context: field-name=\"%s\"",
					name);
			goto error;
		}
		if (bt_ctf_field_type_structure_get_field(struct_type,
					&field_name, &field_type, i) < 0) {
			BT_LOGE("Failed to get field: field-name=\"%s\"",
					field_name);
			goto error;
		}
		if (strcmp(field_name, name)) {
			BT_PUT(field_type);
			BT_PUT(field);
			continue;
		}
		if (bt_ctf_field_type_get_type_id(field_type) !=
				BT_CTF_FIELD_TYPE_ID_INTEGER) {
			BT_LOGE("Expecting an integer for this field: field-name=\"%s\"",
					name);
			goto error;
		}

		writer_field = bt_ctf_field_structure_get_field(writer_packet_context,
				field_name);
		assert(writer_field);

		int_ret = bt_ctf_field_unsigned_integer_set_value(writer_field, value);
		assert(int_ret == 0);

		BT_PUT(writer_field);
		BT_PUT(field_type);
		BT_PUT(field);
	}

	ret = BT_COMPONENT_STATUS_OK;
	goto end;

error:
	bt_put(writer_field);
	bt_put(field_type);
	bt_put(field);
	ret = BT_COMPONENT_STATUS_ERROR;
end:
	bt_put(struct_type);
	bt_put(packet_context);
	return ret;
}

BT_HIDDEN
struct bt_ctf_packet *trimmer_new_packet(
		struct trimmer_iterator *trim_it,
		struct bt_ctf_packet *packet)
{
	struct bt_ctf_stream *stream = NULL;
	struct bt_ctf_packet *writer_packet = NULL;
	int int_ret;

	stream = bt_ctf_packet_get_stream(packet);
	assert(stream);

	/*
	 * If a packet was already opened, close it and remove it from
	 * the HT.
	 */
	writer_packet = lookup_packet(trim_it, packet);
	if (writer_packet) {
		g_hash_table_remove(trim_it->packet_map, packet);
		BT_PUT(writer_packet);
	}

	writer_packet = insert_new_packet(trim_it, packet, stream);
	if (!writer_packet) {
		BT_LOGE_STR("Failed to insert new packet.");
		goto error;
	}
	bt_get(writer_packet);

	int_ret = ctf_packet_copy_context(trim_it->err, packet,
			stream, writer_packet);
	if (int_ret < 0) {
		BT_LOGE_STR("Failed to copy packet context.");
		goto error;
	}

	goto end;

error:
	BT_PUT(writer_packet);
end:
	bt_put(stream);
	return writer_packet;
}

BT_HIDDEN
struct bt_ctf_packet *trimmer_close_packet(
		struct trimmer_iterator *trim_it,
		struct bt_ctf_packet *packet)
{
	struct bt_ctf_packet *writer_packet = NULL;

	writer_packet = lookup_packet(trim_it, packet);
	if (!writer_packet) {
		BT_LOGE_STR("Failed to find existing packet.");
		goto end;
	}

	g_hash_table_remove(trim_it->packet_map, packet);

end:
	return writer_packet;
}

BT_HIDDEN
struct bt_ctf_event *trimmer_output_event(
		struct trimmer_iterator *trim_it,
		struct bt_ctf_event *event)
{
	struct bt_ctf_event_class *event_class = NULL;
	struct bt_ctf_event *writer_event = NULL;
	struct bt_ctf_packet *packet = NULL, *writer_packet = NULL;
	const char *event_name;
	int int_ret;

	event_class = bt_ctf_event_get_class(event);
	assert(event_class);

	event_name = bt_ctf_event_class_get_name(event_class);

	writer_event = ctf_copy_event(trim_it->err, event, event_class, false);
	if (!writer_event) {
		BT_LOGE("Failed to copy event: event-class-name=\"%s\", event-name=\"%s\"",
				bt_ctf_event_class_get_name(event_class),
				event_name);
		goto error;
	}

	packet = bt_ctf_event_get_packet(event);
	assert(packet);

	writer_packet = lookup_packet(trim_it, packet);
	if (!writer_packet) {
		BT_LOGE_STR("Failed to find existing packet.");
		goto error;
	}
	bt_get(writer_packet);

	int_ret = bt_ctf_event_set_packet(writer_event, writer_packet);
	if (int_ret < 0) {
		BT_LOGE("Failed to append event: event-class-name=\"%s\", event-name=\"%s\"",
				bt_ctf_event_class_get_name(event_class),
				event_name);
		goto error;
	}

	/* We keep the reference on the writer_event to create a notification. */
	goto end;

error:
	BT_PUT(writer_event);
end:
	bt_put(writer_packet);
	bt_put(packet);
	bt_put(event_class);
	return writer_event;
}
