/*
 * writer.c
 *
 * Babeltrace CTF Writer Output Plugin Event Handling
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

#include <babeltrace/ctf-ir/event.h>
#include <babeltrace/ctf-ir/packet.h>
#include <babeltrace/ctf-ir/event-class.h>
#include <babeltrace/ctf-ir/stream.h>
#include <babeltrace/ctf-ir/stream-class.h>
#include <babeltrace/ctf-ir/clock-class.h>
#include <babeltrace/ctf-ir/fields.h>
#include <babeltrace/ctf-writer/stream-class.h>
#include <babeltrace/ctf-writer/stream.h>

#include <ctfcopytrace.h>

#include "writer.h"

static
struct bt_ctf_stream_class *insert_new_stream_class(
		struct writer_component *writer_component,
		struct bt_ctf_writer *ctf_writer,
		struct bt_ctf_stream_class *stream_class)
{
	struct bt_ctf_stream_class *writer_stream_class = NULL;
	struct bt_ctf_trace *trace, *writer_trace;
	enum bt_component_status ret;

	trace = bt_ctf_stream_class_get_trace(stream_class);
	if (!trace) {
		fprintf(writer_component->err,
				"[error] %s in %s:%d\n", __func__, __FILE__,
				__LINE__);
		goto end;
	}

	writer_trace = bt_ctf_writer_get_trace(ctf_writer);
	if (!writer_trace) {
		bt_put(trace);
		fprintf(writer_component->err,
				"[error] %s in %s:%d\n", __func__, __FILE__,
				__LINE__);
		ret = BT_COMPONENT_STATUS_ERROR;
		goto end_put_trace;
	}

	ret = ctf_copy_clock_classes(writer_component->err, writer_trace,
			writer_stream_class, trace);
	if (ret != BT_COMPONENT_STATUS_OK) {
		bt_put(trace);
		fprintf(writer_component->err,
				"[error] %s in %s:%d\n", __func__, __FILE__,
				__LINE__);
		goto end_put_writer_trace;
	}

	writer_stream_class = ctf_copy_stream_class(writer_component->err,
			stream_class, writer_trace, true);
	if (!writer_stream_class) {
		fprintf(writer_component->err, "[error] Failed to copy stream class\n");
		fprintf(writer_component->err, "[error] %s in %s:%d\n",
				__func__, __FILE__, __LINE__);
		goto end_put_writer_trace;
	}

	g_hash_table_insert(writer_component->stream_class_map,
			(gpointer) stream_class, writer_stream_class);

end_put_writer_trace:
	bt_put(writer_trace);
end_put_trace:
	bt_put(trace);
end:
	return writer_stream_class;
}

static
struct bt_ctf_stream *insert_new_stream(
		struct writer_component *writer_component,
		struct bt_ctf_writer *ctf_writer,
		struct bt_ctf_stream_class *stream_class,
		struct bt_ctf_stream *stream)
{
	struct bt_ctf_stream *writer_stream;
	struct bt_ctf_stream_class *writer_stream_class;

	writer_stream_class = g_hash_table_lookup(
			writer_component->stream_class_map,
			(gpointer) stream_class);
	if (writer_stream_class) {
		if (!bt_get(writer_stream_class)) {
			writer_stream = NULL;
			fprintf(writer_component->err, "[error] %s in %s:%d\n",
					__func__, __FILE__, __LINE__);
			goto end;
		}
	} else {
		writer_stream_class = insert_new_stream_class(
				writer_component, ctf_writer, stream_class);
		if (!writer_stream_class) {
			writer_stream = NULL;
			fprintf(writer_component->err, "[error] %s in %s:%d\n",
					__func__, __FILE__, __LINE__);
			goto end_put;
		}
	}

	writer_stream = bt_ctf_writer_create_stream(ctf_writer,
			writer_stream_class);
	if (!writer_stream) {
		fprintf(writer_component->err, "[error] %s in %s:%d\n",
				__func__, __FILE__, __LINE__);
		goto end_put;
	}

	g_hash_table_insert(writer_component->stream_map, (gpointer) stream,
			writer_stream);

	bt_ctf_writer_flush_metadata(ctf_writer);

end_put:
	bt_put(writer_stream_class);
end:
	return writer_stream;
}

static
struct bt_ctf_stream *lookup_stream(struct writer_component *writer_component,
		struct bt_ctf_stream *stream)
{
	return (struct bt_ctf_stream *) g_hash_table_lookup(
			writer_component->stream_map,
			(gpointer) stream);
}

static
struct bt_ctf_event_class *get_event_class(struct writer_component *writer_component,
		struct bt_ctf_stream_class *writer_stream_class,
		struct bt_ctf_event_class *event_class)
{
	return bt_ctf_stream_class_get_event_class_by_name(writer_stream_class,
			bt_ctf_event_class_get_name(event_class));
}

struct bt_ctf_writer *insert_new_writer(
		struct writer_component *writer_component,
		struct bt_ctf_trace *trace)
{
	struct bt_ctf_writer *ctf_writer;
	struct bt_ctf_trace *writer_trace;
	char trace_name[PATH_MAX];
	enum bt_component_status ret;

	snprintf(trace_name, PATH_MAX, "%s/%s_%03d",
			writer_component->base_path->str,
			writer_component->trace_name_base->str,
			writer_component->trace_id++);
	printf_verbose("CTF-Writer creating trace in %s\n", trace_name);

	ctf_writer = bt_ctf_writer_create(trace_name);
	if (!ctf_writer) {
		fprintf(writer_component->err, "[error] %s in %s:%d\n",
				__func__, __FILE__, __LINE__);
		goto end;
	}

	writer_trace = bt_ctf_writer_get_trace(ctf_writer);
	if (!writer_trace) {
		ret = BT_COMPONENT_STATUS_ERROR;
		fprintf(writer_component->err,
				"[error] %s in %s:%d\n", __func__, __FILE__,
				__LINE__);
		goto end;
	}

	ret = ctf_copy_trace(writer_component->err, trace, writer_trace);
	bt_put(writer_trace);
	if (ret != BT_COMPONENT_STATUS_OK) {
		fprintf(writer_component->err, "[error] Failed to copy trace\n");
		fprintf(writer_component->err, "[error] %s in %s:%d\n",
				__func__, __FILE__, __LINE__);
		BT_PUT(ctf_writer);
		goto end;
	}

	g_hash_table_insert(writer_component->trace_map, (gpointer) trace,
			ctf_writer);

end:
	return ctf_writer;
}

static
struct bt_ctf_writer *get_writer(struct writer_component *writer_component,
		struct bt_ctf_stream_class *stream_class)
{
	struct bt_ctf_trace *trace;
	struct bt_ctf_writer *ctf_writer;

	trace = bt_ctf_stream_class_get_trace(stream_class);
	if (!trace) {
		ctf_writer = NULL;
		fprintf(writer_component->err, "[error] %s in %s:%d\n",
				__func__, __FILE__, __LINE__);
		goto end;
	}

	ctf_writer = g_hash_table_lookup(writer_component->trace_map,
			(gpointer) trace);
	if (ctf_writer) {
		if (!bt_get(ctf_writer)) {
			ctf_writer = NULL;
			fprintf(writer_component->err, "[error] %s in %s:%d\n",
					__func__, __FILE__, __LINE__);
			goto end;
		}
	} else {
		ctf_writer = insert_new_writer(writer_component, trace);
	}
	bt_put(trace);

end:
	return ctf_writer;
}

static
struct bt_ctf_stream *get_writer_stream(
		struct writer_component *writer_component,
		struct bt_ctf_packet *packet, struct bt_ctf_stream *stream)
{
	struct bt_ctf_stream_class *stream_class;
	struct bt_ctf_writer *ctf_writer;
	struct bt_ctf_stream *writer_stream;

	stream_class = bt_ctf_stream_get_class(stream);
	if (!stream_class) {
		writer_stream = NULL;
		fprintf(writer_component->err, "[error] %s in %s:%d\n",
				__func__, __FILE__, __LINE__);
		goto end;
	}

	ctf_writer = get_writer(writer_component, stream_class);
	if (!ctf_writer) {
		writer_stream = NULL;
		fprintf(writer_component->err, "[error] %s in %s:%d\n",
				__func__, __FILE__, __LINE__);
		goto end_put_stream_class;
	}

	writer_stream = lookup_stream(writer_component, stream);

	if (writer_stream) {
		if (!bt_get(writer_stream)) {
			writer_stream = NULL;
			fprintf(writer_component->err, "[error] %s in %s:%d\n",
					__func__, __FILE__, __LINE__);
			goto end_put_stream_class;
		}
	} else {
		writer_stream = insert_new_stream(writer_component, ctf_writer,
				stream_class, stream);
		bt_get(writer_stream);
	}

	bt_put(ctf_writer);
end_put_stream_class:
	bt_put(stream_class);
end:
	return writer_stream;
}

BT_HIDDEN
enum bt_component_status writer_new_packet(
		struct writer_component *writer_component,
		struct bt_ctf_packet *packet)
{
	struct bt_ctf_stream *stream, *writer_stream;
	struct bt_ctf_field *writer_packet_context;
	enum bt_component_status ret = BT_COMPONENT_STATUS_OK;
	int int_ret;

	stream = bt_ctf_packet_get_stream(packet);
	if (!stream) {
		ret = BT_COMPONENT_STATUS_ERROR;
		fprintf(writer_component->err, "[error] %s in %s:%d\n",
				__func__, __FILE__, __LINE__);
		goto end;
	}

	writer_stream = get_writer_stream(writer_component, packet, stream);
	if (!writer_stream) {
		ret = BT_COMPONENT_STATUS_ERROR;
		fprintf(writer_component->err, "[error] %s in %s:%d\n",
				__func__, __FILE__, __LINE__);
		goto end_put;
	}

	writer_packet_context = ctf_copy_packet_context(writer_component->err,
			packet, writer_stream);
	if (!writer_packet_context) {
		ret = BT_COMPONENT_STATUS_ERROR;
		fprintf(writer_component->err, "[error] %s in %s:%d\n",
				__func__, __FILE__, __LINE__);
		goto end_put;
	}

	int_ret = bt_ctf_stream_set_packet_context(writer_stream,
			writer_packet_context);
	if (int_ret < 0) {
		ret = BT_COMPONENT_STATUS_ERROR;
		fprintf(writer_component->err, "[error] %s in %s:%d\n", __func__,
				__FILE__, __LINE__);
		goto end_put_writer_packet_context;
	}

	bt_put(writer_stream);

end_put_writer_packet_context:
	bt_put(writer_packet_context);
end_put:
	bt_put(stream);
end:
	return ret;
}

BT_HIDDEN
enum bt_component_status writer_close_packet(
		struct writer_component *writer_component,
		struct bt_ctf_packet *packet)
{
	struct bt_ctf_stream *stream, *writer_stream;
	enum bt_component_status ret;

	stream = bt_ctf_packet_get_stream(packet);
	if (!stream) {
		ret = BT_COMPONENT_STATUS_ERROR;
		fprintf(writer_component->err, "[error] %s in %s:%d\n",
				__func__, __FILE__, __LINE__);
		goto end;
	}

	writer_stream = lookup_stream(writer_component, stream);
	if (!writer_stream) {
		ret = BT_COMPONENT_STATUS_ERROR;
		fprintf(writer_component->err, "[error] %s in %s:%d\n",
				__func__, __FILE__, __LINE__);
		goto end_put;
	}

	if (!bt_get(writer_stream)) {
		fprintf(writer_component->err,
				"[error] Failed to get reference on writer stream\n");
		fprintf(writer_component->err, "[error] %s in %s:%d\n",
				__func__, __FILE__, __LINE__);
		ret = BT_COMPONENT_STATUS_ERROR;
		goto end_put;
	}

	ret = bt_ctf_stream_flush(writer_stream);
	if (ret < 0) {
		fprintf(writer_component->err,
				"[error] Failed to flush packet\n");
		ret = BT_COMPONENT_STATUS_ERROR;
	}

	ret = BT_COMPONENT_STATUS_OK;

	bt_put(writer_stream);

end_put:
	bt_put(stream);
end:
	return ret;
}

BT_HIDDEN
enum bt_component_status writer_output_event(
		struct writer_component *writer_component,
		struct bt_ctf_event *event)
{
	enum bt_component_status ret;
	struct bt_ctf_event_class *event_class, *writer_event_class;
	struct bt_ctf_stream *stream, *writer_stream;
	struct bt_ctf_stream_class *stream_class, *writer_stream_class;
	struct bt_ctf_event *writer_event;
	const char *event_name;
	int int_ret;

	event_class = bt_ctf_event_get_class(event);
	if (!event_class) {
		ret = BT_COMPONENT_STATUS_ERROR;
		fprintf(writer_component->err, "[error] %s in %s:%d\n", __func__,
				__FILE__, __LINE__);
		goto end;
	}

	event_name = bt_ctf_event_class_get_name(event_class);
	if (!event_name) {
		ret = BT_COMPONENT_STATUS_ERROR;
		fprintf(writer_component->err, "[error] %s in %s:%d\n", __func__,
				__FILE__, __LINE__);
		goto end_put_event_class;
	}

	stream = bt_ctf_event_get_stream(event);
	if (!stream) {
		ret = BT_COMPONENT_STATUS_ERROR;
		fprintf(writer_component->err, "[error] %s in %s:%d\n", __func__,
				__FILE__, __LINE__);
		goto end_put_event_class;
	}

	writer_stream = lookup_stream(writer_component, stream);
	if (!writer_stream || !bt_get(writer_stream)) {
		ret = BT_COMPONENT_STATUS_ERROR;
		fprintf(writer_component->err, "[error] %s in %s:%d\n", __func__,
				__FILE__, __LINE__);
		goto end_put_stream;
	}

	stream_class = bt_ctf_event_class_get_stream_class(event_class);
	if (!stream_class) {
		ret = BT_COMPONENT_STATUS_ERROR;
		fprintf(writer_component->err, "[error] %s in %s:%d\n", __func__,
				__FILE__, __LINE__);
		goto end_put_writer_stream;
	}

	writer_stream_class = g_hash_table_lookup(
			writer_component->stream_class_map,
			(gpointer) stream_class);
	if (!writer_stream_class || !bt_get(writer_stream_class)) {
		ret = BT_COMPONENT_STATUS_ERROR;
		fprintf(writer_component->err, "[error] %s in %s:%d\n", __func__,
				__FILE__, __LINE__);
		goto end_put_stream_class;
	}

	writer_event_class = get_event_class(writer_component,
			writer_stream_class, event_class);
	if (!writer_event_class) {
		writer_event_class = ctf_copy_event_class(writer_component->err,
				event_class);
		if (!writer_event_class) {
			ret = BT_COMPONENT_STATUS_ERROR;
			fprintf(writer_component->err, "[error] %s in %s:%d\n",
					__func__, __FILE__, __LINE__);
			goto end_put_writer_stream_class;
		}
		int_ret = bt_ctf_stream_class_add_event_class(
				writer_stream_class, writer_event_class);
		if (int_ret) {
			ret = BT_COMPONENT_STATUS_ERROR;
			fprintf(writer_component->err, "[error] %s in %s:%d\n",
					__func__, __FILE__, __LINE__);
			goto end_put_writer_stream_class;
		}
	}

	writer_event = ctf_copy_event(writer_component->err, event,
			writer_event_class, true);
	if (!writer_event) {
		ret = BT_COMPONENT_STATUS_ERROR;
		fprintf(writer_component->err, "[error] %s in %s:%d\n", __func__,
				__FILE__, __LINE__);
		fprintf(writer_component->err, "[error] Failed to copy event %s\n",
				bt_ctf_event_class_get_name(writer_event_class));
		goto end_put_writer_event_class;
	}

	int_ret = bt_ctf_stream_append_event(writer_stream, writer_event);
	if (int_ret < 0) {
		ret = BT_COMPONENT_STATUS_ERROR;
		fprintf(writer_component->err, "[error] %s in %s:%d\n", __func__,
				__FILE__, __LINE__);
		fprintf(writer_component->err, "[error] Failed to append event %s\n",
				bt_ctf_event_class_get_name(writer_event_class));
		goto end_put_writer_event;
	}

	ret = BT_COMPONENT_STATUS_OK;

end_put_writer_event:
	bt_put(writer_event);
end_put_writer_event_class:
	bt_put(writer_event_class);
end_put_writer_stream_class:
	bt_put(writer_stream_class);
end_put_stream_class:
	bt_put(stream_class);
end_put_writer_stream:
	bt_put(writer_stream);
end_put_stream:
	bt_put(stream);
end_put_event_class:
	bt_put(event_class);
end:
	return ret;
}
