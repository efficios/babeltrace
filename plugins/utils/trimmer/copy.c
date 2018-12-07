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

#include <babeltrace/assert-internal.h>
#include <babeltrace/babeltrace.h>

#include <ctfcopytrace.h>
#include "iterator.h"

static
const struct bt_packet *lookup_packet(struct trimmer_iterator *trim_it,
		const struct bt_packet *packet)
{
	return (const struct bt_packet *) g_hash_table_lookup(
			trim_it->packet_map,
			(gpointer) packet);
}

static
const struct bt_packet *insert_new_packet(struct trimmer_iterator *trim_it,
		const struct bt_packet *packet,
		const struct bt_stream *stream)
{
	const struct bt_packet *writer_packet = NULL;
	int ret;

	BT_LOGD_STR("Inserting a new packet.");
	writer_packet = bt_packet_create(stream);
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
	BT_PACKET_PUT_REF_AND_RESET(writer_packet);
end:
	return writer_packet;
}

BT_HIDDEN
enum bt_component_status update_packet_context_field(FILE *err,
		const struct bt_packet *writer_packet,
		const char *name, int64_t value)
{
	enum bt_component_status ret;
	const struct bt_field *packet_context = NULL, *writer_packet_context = NULL;
	const struct bt_field_class *struct_class = NULL, *field_class = NULL;
	const struct bt_field *field = NULL, *writer_field = NULL;
	int nr_fields, i, int_ret;

	BT_LOGD("Updating packet context field: name=%s", name);
	packet_context = bt_packet_get_context(writer_packet);
	BT_ASSERT(packet_context);

	struct_class = bt_field_get_class(packet_context);
	BT_ASSERT(struct_class);

	writer_packet_context = bt_packet_get_context(writer_packet);
	BT_ASSERT(writer_packet_context);

	nr_fields = bt_field_class_structure_get_field_count(struct_class);
	for (i = 0; i < nr_fields; i++) {
		const char *field_name;

		field = bt_field_structure_get_field_by_index(
				packet_context, i);
		if (!field) {
			BT_LOGE("Failed to get field in packet-context: field-name=\"%s\"",
					name);
			goto error;
		}
		if (bt_field_class_structure_get_field_by_index(struct_class,
					&field_name, &field_class, i) < 0) {
			BT_LOGE("Failed to get field: field-name=\"%s\"",
					field_name);
			goto error;
		}
		if (strcmp(field_name, name)) {
			BT_OBJECT_PUT_REF_AND_RESET(field_class);
			BT_OBJECT_PUT_REF_AND_RESET(field);
			continue;
		}
		if (bt_field_class_id(field_class) !=
				BT_FIELD_CLASS_TYPE_INTEGER) {
			BT_LOGE("Expecting an integer for this field: field-name=\"%s\"",
					name);
			goto error;
		}

		writer_field = bt_field_structure_get_field_by_name(writer_packet_context,
				field_name);
		BT_ASSERT(writer_field);

		int_ret = bt_field_unsigned_integer_set_value(writer_field, value);
		BT_ASSERT(int_ret == 0);

		BT_OBJECT_PUT_REF_AND_RESET(writer_field);
		BT_OBJECT_PUT_REF_AND_RESET(field_class);
		BT_OBJECT_PUT_REF_AND_RESET(field);
	}

	ret = BT_COMPONENT_STATUS_OK;
	goto end;

error:
	bt_object_put_ref(writer_field);
	bt_object_put_ref(field_class);
	bt_object_put_ref(field);
	ret = BT_COMPONENT_STATUS_ERROR;
end:
	bt_object_put_ref(struct_class);
	bt_object_put_ref(packet_context);
	return ret;
}

BT_HIDDEN
const struct bt_packet *trimmer_new_packet(
		struct trimmer_iterator *trim_it,
		const struct bt_packet *packet)
{
	const struct bt_stream *stream = NULL;
	const struct bt_packet *writer_packet = NULL;
	int int_ret;

	stream = bt_packet_get_stream(packet);
	BT_ASSERT(stream);

	/*
	 * If a packet was already opened, close it and remove it from
	 * the HT.
	 */
	writer_packet = lookup_packet(trim_it, packet);
	if (writer_packet) {
		g_hash_table_remove(trim_it->packet_map, packet);
		BT_PACKET_PUT_REF_AND_RESET(writer_packet);
	}

	writer_packet = insert_new_packet(trim_it, packet, stream);
	if (!writer_packet) {
		BT_LOGE_STR("Failed to insert new packet.");
		goto error;
	}
	bt_packet_get_ref(writer_packet);

	int_ret = ctf_packet_copy_context(trim_it->err, packet,
			stream, writer_packet);
	if (int_ret < 0) {
		BT_LOGE_STR("Failed to copy packet context.");
		goto error;
	}

	goto end;

error:
	BT_PACKET_PUT_REF_AND_RESET(writer_packet);
end:
	bt_stream_put_ref(stream);
	return writer_packet;
}

BT_HIDDEN
const struct bt_packet *trimmer_close_packet(
		struct trimmer_iterator *trim_it,
		const struct bt_packet *packet)
{
	const struct bt_packet *writer_packet = NULL;

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
const struct bt_event *trimmer_output_event(
		struct trimmer_iterator *trim_it,
		const struct bt_event *event)
{
	const struct bt_event_class *event_class = NULL;
	const struct bt_event *writer_event = NULL;
	const struct bt_packet *packet = NULL, *writer_packet = NULL;
	const char *event_name;
	int int_ret;

	event_class = bt_event_get_class(event);
	BT_ASSERT(event_class);

	event_name = bt_event_class_get_name(event_class);

	writer_event = ctf_copy_event(trim_it->err, event, event_class, false);
	if (!writer_event) {
		BT_LOGE("Failed to copy event: event-class-name=\"%s\", event-name=\"%s\"",
				bt_event_class_get_name(event_class),
				event_name);
		goto error;
	}

	packet = bt_event_get_packet(event);
	BT_ASSERT(packet);

	writer_packet = lookup_packet(trim_it, packet);
	if (!writer_packet) {
		BT_LOGE_STR("Failed to find existing packet.");
		goto error;
	}
	bt_packet_get_ref(writer_packet);

	int_ret = bt_event_set_packet(writer_event, writer_packet);
	if (int_ret < 0) {
		BT_LOGE("Failed to append event: event-class-name=\"%s\", event-name=\"%s\"",
				bt_event_class_get_name(event_class),
				event_name);
		goto error;
	}

	/* We keep the reference on the writer_event to create a notification. */
	goto end;

error:
	BT_OBJECT_PUT_REF_AND_RESET(writer_event);
end:
	bt_packet_put_ref(writer_packet);
	bt_packet_put_ref(packet);
	bt_event_class_put_ref(event_class);
	return writer_event;
}
