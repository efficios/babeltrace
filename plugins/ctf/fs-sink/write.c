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
#include <assert.h>

#include <ctfcopytrace.h>

#include "writer.h"

static
void unref_stream_class(struct bt_ctf_stream_class *writer_stream_class)
{
	bt_put(writer_stream_class);
}

static
void unref_stream(struct bt_ctf_stream_class *writer_stream)
{
	bt_put(writer_stream);
}

gboolean empty_ht(gpointer key, gpointer value, gpointer user_data)
{
	return TRUE;
}

void destroy_stream_state_key(gpointer key)
{
	g_free((enum fs_writer_stream_state *) key);
}

static
void trace_is_static_listener(struct bt_ctf_trace *trace, void *data)
{
	*((int *) data) = 1;
}

static
struct bt_ctf_stream_class *insert_new_stream_class(
		struct writer_component *writer_component,
		struct fs_writer *fs_writer,
		struct bt_ctf_stream_class *stream_class)
{
	struct bt_ctf_stream_class *writer_stream_class = NULL;
	struct bt_ctf_trace *trace = NULL, *writer_trace = NULL;
	struct bt_ctf_writer *ctf_writer = fs_writer->writer;
	enum bt_component_status ret;

	trace = bt_ctf_stream_class_get_trace(stream_class);
	if (!trace) {
		fprintf(writer_component->err,
				"[error] %s in %s:%d\n", __func__, __FILE__,
				__LINE__);
		goto error;
	}

	writer_trace = bt_ctf_writer_get_trace(ctf_writer);
	if (!writer_trace) {
		fprintf(writer_component->err,
				"[error] %s in %s:%d\n", __func__, __FILE__,
				__LINE__);
		goto error;
	}

	ret = ctf_copy_clock_classes(writer_component->err, writer_trace,
			writer_stream_class, trace);
	if (ret != BT_COMPONENT_STATUS_OK) {
		fprintf(writer_component->err,
				"[error] %s in %s:%d\n", __func__, __FILE__,
				__LINE__);
		goto error;
	}

	writer_stream_class = ctf_copy_stream_class(writer_component->err,
			stream_class, writer_trace, true);
	if (!writer_stream_class) {
		fprintf(writer_component->err, "[error] Failed to copy stream class\n");
		fprintf(writer_component->err, "[error] %s in %s:%d\n",
				__func__, __FILE__, __LINE__);
		goto error;
	}

	g_hash_table_insert(fs_writer->stream_class_map,
			(gpointer) stream_class, writer_stream_class);

	goto end;

error:
	BT_PUT(writer_stream_class);
end:
	bt_put(writer_trace);
	bt_put(trace);
	return writer_stream_class;
}

static
struct fs_writer *insert_new_writer(
		struct writer_component *writer_component,
		struct bt_ctf_trace *trace)
{
	struct bt_ctf_writer *ctf_writer = NULL;
	struct bt_ctf_trace *writer_trace = NULL;
	char trace_name[PATH_MAX];
	enum bt_component_status ret;
	struct bt_ctf_stream *stream = NULL;
	struct fs_writer *fs_writer = NULL;
	int nr_stream, i;

	/* FIXME: replace with trace name when it will work. */
	snprintf(trace_name, PATH_MAX, "%s/%s_%03d",
			writer_component->base_path->str,
			writer_component->trace_name_base->str,
			writer_component->trace_id++);
	printf_verbose("CTF-Writer creating trace in %s\n", trace_name);

	ctf_writer = bt_ctf_writer_create(trace_name);
	if (!ctf_writer) {
		fprintf(writer_component->err, "[error] %s in %s:%d\n",
				__func__, __FILE__, __LINE__);
		goto error;
	}

	writer_trace = bt_ctf_writer_get_trace(ctf_writer);
	if (!writer_trace) {
		fprintf(writer_component->err,
				"[error] %s in %s:%d\n", __func__, __FILE__,
				__LINE__);
		goto error;
	}

	ret = ctf_copy_trace(writer_component->err, trace, writer_trace);
	if (ret != BT_COMPONENT_STATUS_OK) {
		fprintf(writer_component->err, "[error] Failed to copy trace\n");
		fprintf(writer_component->err, "[error] %s in %s:%d\n",
				__func__, __FILE__, __LINE__);
		BT_PUT(ctf_writer);
		goto error;
	}

	fs_writer = g_new0(struct fs_writer, 1);
	if (!fs_writer) {
		fprintf(writer_component->err,
				"[error] %s in %s:%d\n", __func__, __FILE__,
				__LINE__);
		goto error;
	}
	fs_writer->writer = ctf_writer;
	fs_writer->trace = trace;
	fs_writer->writer_trace = writer_trace;
	BT_PUT(writer_trace);
	fs_writer->stream_class_map = g_hash_table_new_full(g_direct_hash,
			g_direct_equal, NULL, (GDestroyNotify) unref_stream_class);
	fs_writer->stream_map = g_hash_table_new_full(g_direct_hash,
			g_direct_equal, NULL, (GDestroyNotify) unref_stream);
	fs_writer->stream_states = g_hash_table_new_full(g_direct_hash,
			g_direct_equal, NULL, destroy_stream_state_key);

	/* Set all the existing streams in the unknown state. */
	nr_stream = bt_ctf_trace_get_stream_count(trace);
	for (i = 0; i < nr_stream; i++) {
		enum fs_writer_stream_state *v;

		stream = bt_ctf_trace_get_stream_by_index(trace, i);
		if (!stream) {
			fprintf(writer_component->err,
					"[error] %s in %s:%d\n", __func__,
					__FILE__, __LINE__);
			goto error;
		}

		v = g_new0(enum fs_writer_stream_state, 1);
		if (!v) {
			fprintf(writer_component->err,
					"[error] %s in %s:%d\n", __func__,
					__FILE__, __LINE__);
			goto error;
		}
		*v = FS_WRITER_UNKNOWN_STREAM;

		g_hash_table_insert(fs_writer->stream_states, stream, v);
		BT_PUT(stream);
	}

	/* Check if the trace is already static or register a listener. */
	if (bt_ctf_trace_is_static(trace)) {
		fs_writer->trace_static = 1;
		fs_writer->static_listener_id = -1;
	} else {
		ret = bt_ctf_trace_add_is_static_listener(trace,
				trace_is_static_listener, &fs_writer->trace_static);
		if (ret < 0) {
			fprintf(writer_component->err,
					"[error] %s in %s:%d\n", __func__, __FILE__,
					__LINE__);
			goto error;
		}
		fs_writer->static_listener_id = ret;
	}

	g_hash_table_insert(writer_component->trace_map, (gpointer) trace,
			fs_writer);

	goto end;

error:
	g_free(fs_writer);
	fs_writer = NULL;
	bt_put(writer_trace);
	bt_put(stream);
	BT_PUT(ctf_writer);
end:
	return fs_writer;
}

static
struct fs_writer *get_fs_writer(struct writer_component *writer_component,
		struct bt_ctf_stream_class *stream_class)
{
	struct bt_ctf_trace *trace = NULL;
	struct fs_writer *fs_writer;

	trace = bt_ctf_stream_class_get_trace(stream_class);
	if (!trace) {
		fprintf(writer_component->err, "[error] %s in %s:%d\n",
				__func__, __FILE__, __LINE__);
		goto error;
	}

	fs_writer = g_hash_table_lookup(writer_component->trace_map,
			(gpointer) trace);
	if (!fs_writer) {
		fs_writer = insert_new_writer(writer_component, trace);
	}
	BT_PUT(trace);
	goto end;

error:
	fs_writer = NULL;
end:
	return fs_writer;
}

static
struct fs_writer *get_fs_writer_from_stream(
		struct writer_component *writer_component,
		struct bt_ctf_stream *stream)
{
	struct bt_ctf_stream_class *stream_class = NULL;
	struct fs_writer *fs_writer;

	stream_class = bt_ctf_stream_get_class(stream);
	if (!stream_class) {
		fprintf(writer_component->err, "[error] %s in %s:%d\n",
				__func__, __FILE__, __LINE__);
		goto error;
	}

	fs_writer = get_fs_writer(writer_component, stream_class);
	goto end;

error:
	fs_writer = NULL;

end:
	bt_put(stream_class);
	return fs_writer;
}

static
struct bt_ctf_stream_class *lookup_stream_class(
		struct writer_component *writer_component,
		struct bt_ctf_stream_class *stream_class)
{
	struct fs_writer *fs_writer = get_fs_writer(
			writer_component, stream_class);
	assert(fs_writer);
	return (struct bt_ctf_stream_class *) g_hash_table_lookup(
			fs_writer->stream_class_map, (gpointer) stream_class);
}

static
struct bt_ctf_stream *lookup_stream(struct writer_component *writer_component,
		struct bt_ctf_stream *stream)
{
	struct fs_writer *fs_writer = get_fs_writer_from_stream(
			writer_component, stream);
	assert(fs_writer);
	return (struct bt_ctf_stream *) g_hash_table_lookup(
			fs_writer->stream_map, (gpointer) stream);
}

static
struct bt_ctf_stream *insert_new_stream(
		struct writer_component *writer_component,
		struct fs_writer *fs_writer,
		struct bt_ctf_stream_class *stream_class,
		struct bt_ctf_stream *stream)
{
	struct bt_ctf_stream *writer_stream = NULL;
	struct bt_ctf_stream_class *writer_stream_class = NULL;
	struct bt_ctf_writer *ctf_writer = bt_get(fs_writer->writer);

	writer_stream_class = lookup_stream_class(writer_component,
			stream_class);
	if (!writer_stream_class) {
		writer_stream_class = insert_new_stream_class(
				writer_component, fs_writer, stream_class);
		if (!writer_stream_class) {
			fprintf(writer_component->err, "[error] %s in %s:%d\n",
					__func__, __FILE__, __LINE__);
			goto error;
		}
	}
	bt_get(writer_stream_class);

	writer_stream = bt_ctf_writer_create_stream(ctf_writer,
			writer_stream_class);
	if (!writer_stream) {
		fprintf(writer_component->err, "[error] %s in %s:%d\n",
				__func__, __FILE__, __LINE__);
		goto error;
	}

	g_hash_table_insert(fs_writer->stream_map, (gpointer) stream,
			writer_stream);

	goto end;

error:
	BT_PUT(writer_stream);
end:
	bt_put(ctf_writer);
	bt_put(writer_stream_class);
	return writer_stream;
}

static
struct bt_ctf_event_class *get_event_class(struct writer_component *writer_component,
		struct bt_ctf_stream_class *writer_stream_class,
		struct bt_ctf_event_class *event_class)
{
	return bt_ctf_stream_class_get_event_class_by_id(writer_stream_class,
			bt_ctf_event_class_get_id(event_class));
}

static
struct bt_ctf_stream *get_writer_stream(
		struct writer_component *writer_component,
		struct bt_ctf_packet *packet, struct bt_ctf_stream *stream)
{
	struct bt_ctf_stream *writer_stream = NULL;

	writer_stream = lookup_stream(writer_component, stream);
	if (!writer_stream) {
		fprintf(writer_component->err, "[error] %s in %s:%d\n",
				__func__, __FILE__, __LINE__);
		goto error;
	}
	bt_get(writer_stream);

	goto end;

error:
	BT_PUT(writer_stream);
end:
	return writer_stream;
}

BT_HIDDEN
void writer_close(struct writer_component *writer_component,
		struct fs_writer *fs_writer)
{
	if (fs_writer->static_listener_id > 0) {
		bt_ctf_trace_remove_is_static_listener(fs_writer->trace,
				fs_writer->static_listener_id);
	}

	/* Empty the stream class HT. */
	g_hash_table_foreach_remove(fs_writer->stream_class_map,
			empty_ht, NULL);
	g_hash_table_destroy(fs_writer->stream_class_map);

	/* Empty the stream HT. */
	g_hash_table_foreach_remove(fs_writer->stream_map,
			empty_ht, NULL);
	g_hash_table_destroy(fs_writer->stream_map);

	/* Empty the stream state HT. */
	g_hash_table_foreach_remove(fs_writer->stream_states,
			empty_ht, NULL);
	g_hash_table_destroy(fs_writer->stream_states);
}

BT_HIDDEN
enum bt_component_status writer_stream_begin(
		struct writer_component *writer_component,
		struct bt_ctf_stream *stream)
{
	struct bt_ctf_stream_class *stream_class = NULL;
	struct fs_writer *fs_writer;
	struct bt_ctf_stream *writer_stream = NULL;
	enum bt_component_status ret = BT_COMPONENT_STATUS_OK;
	enum fs_writer_stream_state *state;

	stream_class = bt_ctf_stream_get_class(stream);
	if (!stream_class) {
		fprintf(writer_component->err, "[error] %s in %s:%d\n",
				__func__, __FILE__, __LINE__);
		goto error;
	}

	fs_writer = get_fs_writer(writer_component, stream_class);
	if (!fs_writer) {
		fprintf(writer_component->err, "[error] %s in %s:%d\n",
				__func__, __FILE__, __LINE__);
		goto error;
	}

	/* Set the stream as active */
	state = g_hash_table_lookup(fs_writer->stream_states, stream);
	if (*state != FS_WRITER_UNKNOWN_STREAM) {
		fprintf(writer_component->err, "[error] Unexpected stream "
				"state %d\n", *state);
		goto error;
	}
	*state = FS_WRITER_ACTIVE_STREAM;

	writer_stream = insert_new_stream(writer_component, fs_writer,
			stream_class, stream);
	if (!writer_stream) {
		fprintf(writer_component->err, "[error] %s in %s:%d\n",
				__func__, __FILE__, __LINE__);
		goto error;
	}
	fs_writer->active_streams++;

	goto end;

error:
	ret = BT_COMPONENT_STATUS_ERROR;
end:
	bt_put(stream_class);
	return ret;
}

void check_completed_trace(gpointer key, gpointer value, gpointer user_data)
{
	enum fs_writer_stream_state *state = value;
	int *trace_completed = user_data;

	if (*state != FS_WRITER_COMPLETED_STREAM) {
		*trace_completed = 0;
	}
}

BT_HIDDEN
enum bt_component_status writer_stream_end(
		struct writer_component *writer_component,
		struct bt_ctf_stream *stream)
{
	struct bt_ctf_stream_class *stream_class = NULL;
	struct fs_writer *fs_writer;
	struct bt_ctf_trace *trace = NULL;
	enum bt_component_status ret = BT_COMPONENT_STATUS_OK;
	enum fs_writer_stream_state *state;

	stream_class = bt_ctf_stream_get_class(stream);
	if (!stream_class) {
		fprintf(writer_component->err, "[error] %s in %s:%d\n",
				__func__, __FILE__, __LINE__);
		goto error;
	}

	fs_writer = get_fs_writer(writer_component, stream_class);
	if (!fs_writer) {
		fprintf(writer_component->err, "[error] %s in %s:%d\n",
				__func__, __FILE__, __LINE__);
		goto error;
	}

	state = g_hash_table_lookup(fs_writer->stream_states, stream);
	if (*state != FS_WRITER_ACTIVE_STREAM) {
		fprintf(writer_component->err, "[error] Unexpected stream "
				"state %d\n", *state);
		goto error;
	}
	*state = FS_WRITER_COMPLETED_STREAM;

	g_hash_table_remove(fs_writer->stream_map, stream);

	if (fs_writer->trace_static) {
		int trace_completed = 1;

		g_hash_table_foreach(fs_writer->stream_states,
				check_completed_trace, &trace_completed);
		if (trace_completed) {
			writer_close(writer_component, fs_writer);
			g_hash_table_remove(writer_component->trace_map,
					fs_writer->trace);
		}
	}

	goto end;

error:
	ret = BT_COMPONENT_STATUS_ERROR;
end:
	BT_PUT(trace);
	BT_PUT(stream_class);
	return ret;
}

BT_HIDDEN
enum bt_component_status writer_new_packet(
		struct writer_component *writer_component,
		struct bt_ctf_packet *packet)
{
	struct bt_ctf_stream *stream = NULL, *writer_stream = NULL;
	struct bt_ctf_field *writer_packet_context = NULL;
	enum bt_component_status ret = BT_COMPONENT_STATUS_OK;
	int int_ret;

	stream = bt_ctf_packet_get_stream(packet);
	if (!stream) {
		fprintf(writer_component->err, "[error] %s in %s:%d\n",
				__func__, __FILE__, __LINE__);
		goto error;
	}

	writer_stream = get_writer_stream(writer_component, packet, stream);
	if (!writer_stream) {
		fprintf(writer_component->err, "[error] %s in %s:%d\n",
				__func__, __FILE__, __LINE__);
		goto error;
	}
	BT_PUT(stream);

	writer_packet_context = ctf_copy_packet_context(writer_component->err,
			packet, writer_stream, 1);
	if (!writer_packet_context) {
		fprintf(writer_component->err, "[error] %s in %s:%d\n",
				__func__, __FILE__, __LINE__);
		goto error;
	}

	int_ret = bt_ctf_stream_set_packet_context(writer_stream,
			writer_packet_context);
	if (int_ret < 0) {
		fprintf(writer_component->err, "[error] %s in %s:%d\n", __func__,
				__FILE__, __LINE__);
		goto error;
	}
	BT_PUT(writer_stream);
	BT_PUT(writer_packet_context);

	goto end;

error:
	ret = BT_COMPONENT_STATUS_ERROR;
end:
	bt_put(writer_stream);
	bt_put(writer_packet_context);
	bt_put(stream);
	return ret;
}

BT_HIDDEN
enum bt_component_status writer_close_packet(
		struct writer_component *writer_component,
		struct bt_ctf_packet *packet)
{
	struct bt_ctf_stream *stream = NULL, *writer_stream = NULL;
	enum bt_component_status ret;

	stream = bt_ctf_packet_get_stream(packet);
	if (!stream) {
		fprintf(writer_component->err, "[error] %s in %s:%d\n",
				__func__, __FILE__, __LINE__);
		goto error;
	}

	writer_stream = lookup_stream(writer_component, stream);
	if (!writer_stream) {
		fprintf(writer_component->err, "[error] %s in %s:%d\n",
				__func__, __FILE__, __LINE__);
		goto error;
	}
	BT_PUT(stream);

	bt_get(writer_stream);

	ret = bt_ctf_stream_flush(writer_stream);
	if (ret < 0) {
		fprintf(writer_component->err,
				"[error] Failed to flush packet\n");
		goto error;
	}
	BT_PUT(writer_stream);

	ret = BT_COMPONENT_STATUS_OK;
	goto end;

error:
	ret = BT_COMPONENT_STATUS_ERROR;
end:
	bt_put(writer_stream);
	bt_put(stream);
	return ret;
}

BT_HIDDEN
enum bt_component_status writer_output_event(
		struct writer_component *writer_component,
		struct bt_ctf_event *event)
{
	enum bt_component_status ret;
	struct bt_ctf_event_class *event_class = NULL, *writer_event_class = NULL;
	struct bt_ctf_stream *stream = NULL, *writer_stream = NULL;
	struct bt_ctf_stream_class *stream_class = NULL, *writer_stream_class = NULL;
	struct bt_ctf_event *writer_event = NULL;
	const char *event_name;
	int int_ret;

	event_class = bt_ctf_event_get_class(event);
	if (!event_class) {
		fprintf(writer_component->err, "[error] %s in %s:%d\n", __func__,
				__FILE__, __LINE__);
		goto error;
	}

	event_name = bt_ctf_event_class_get_name(event_class);
	if (!event_name) {
		fprintf(writer_component->err, "[error] %s in %s:%d\n", __func__,
				__FILE__, __LINE__);
		goto error;
	}

	stream = bt_ctf_event_get_stream(event);
	if (!stream) {
		fprintf(writer_component->err, "[error] %s in %s:%d\n", __func__,
				__FILE__, __LINE__);
		goto error;
	}

	writer_stream = lookup_stream(writer_component, stream);
	if (!writer_stream || !bt_get(writer_stream)) {
		fprintf(writer_component->err, "[error] %s in %s:%d\n", __func__,
				__FILE__, __LINE__);
		goto error;
	}

	stream_class = bt_ctf_event_class_get_stream_class(event_class);
	if (!stream_class) {
		fprintf(writer_component->err, "[error] %s in %s:%d\n", __func__,
				__FILE__, __LINE__);
		goto error;
	}

	writer_stream_class = lookup_stream_class(writer_component, stream_class);
	if (!writer_stream_class || !bt_get(writer_stream_class)) {
		fprintf(writer_component->err, "[error] %s in %s:%d\n", __func__,
				__FILE__, __LINE__);
		goto error;
	}

	writer_event_class = get_event_class(writer_component,
			writer_stream_class, event_class);
	if (!writer_event_class) {
		writer_event_class = ctf_copy_event_class(writer_component->err,
				event_class);
		if (!writer_event_class) {
			fprintf(writer_component->err, "[error] %s in %s:%d\n",
					__func__, __FILE__, __LINE__);
			goto error;
		}
		int_ret = bt_ctf_stream_class_add_event_class(
				writer_stream_class, writer_event_class);
		if (int_ret) {
			fprintf(writer_component->err, "[error] %s in %s:%d\n",
					__func__, __FILE__, __LINE__);
			goto error;
		}
	}

	writer_event = ctf_copy_event(writer_component->err, event,
			writer_event_class, true);
	if (!writer_event) {
		fprintf(writer_component->err, "[error] %s in %s:%d\n", __func__,
				__FILE__, __LINE__);
		fprintf(writer_component->err, "[error] Failed to copy event %s\n",
				bt_ctf_event_class_get_name(writer_event_class));
		goto error;
	}

	int_ret = bt_ctf_stream_append_event(writer_stream, writer_event);
	if (int_ret < 0) {
		fprintf(writer_component->err, "[error] %s in %s:%d\n", __func__,
				__FILE__, __LINE__);
		fprintf(writer_component->err, "[error] Failed to append event %s\n",
				bt_ctf_event_class_get_name(writer_event_class));
		goto error;
	}

	ret = BT_COMPONENT_STATUS_OK;
	goto end;

error:
	ret = BT_COMPONENT_STATUS_ERROR;
end:
	bt_put(writer_event);
	bt_put(writer_event_class);
	bt_put(writer_stream_class);
	bt_put(stream_class);
	bt_put(writer_stream);
	bt_put(stream);
	bt_put(event_class);
	return ret;
}
