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

#define BT_LOG_TAG "PLUGIN-CTF-FS-SINK-WRITE"
#include "logging.h"

#include <babeltrace/babeltrace.h>
#include <babeltrace/assert-internal.h>
#include <glib.h>

#include <ctfcopytrace.h>

#include "writer.h"

static
void unref_stream_class(const bt_stream_class *writer_stream_class)
{
	bt_stream_class_put_ref(writer_stream_class);
}

static
void unref_stream(const bt_stream_class *writer_stream)
{
	bt_stream_class_put_ref(writer_stream);
}

static
gboolean empty_ht(gpointer key, gpointer value, gpointer user_data)
{
	return TRUE;
}

static
gboolean empty_streams_ht(gpointer key, gpointer value, gpointer user_data)
{
	int ret;
	const bt_stream *writer_stream = value;

	ret = bt_stream_flush(writer_stream);
	if (ret) {
		BT_LOGD_STR("Failed to flush stream while emptying hash table.");
	}
	return TRUE;
}

static
void destroy_stream_state_key(gpointer key)
{
	g_free((enum fs_writer_stream_state *) key);
}

static
void check_completed_trace(gpointer key, gpointer value, gpointer user_data)
{
	enum fs_writer_stream_state *state = value;
	int *trace_completed = user_data;

	if (*state != FS_WRITER_COMPLETED_STREAM) {
		*trace_completed = 0;
	}
}

static
void trace_is_static_listener(const bt_trace *trace, void *data)
{
	struct fs_writer *fs_writer = data;
	int trace_completed = 1;

	fs_writer->trace_static = 1;

	g_hash_table_foreach(fs_writer->stream_states,
			check_completed_trace, &trace_completed);
	if (trace_completed) {
		writer_close(fs_writer->writer_component, fs_writer);
		g_hash_table_remove(fs_writer->writer_component->trace_map,
				fs_writer->trace);
	}
}

static
const bt_stream_class *insert_new_stream_class(
		struct writer_component *writer_component,
		struct fs_writer *fs_writer,
		const bt_stream_class *stream_class)
{
	const bt_stream_class *writer_stream_class = NULL;
	const bt_trace *trace = NULL, *writer_trace = NULL;
	struct bt_ctf_writer *ctf_writer = fs_writer->writer;
	bt_component_status ret;

	trace = bt_stream_class_get_trace(stream_class);
	BT_ASSERT(trace);

	writer_trace = bt_ctf_writer_get_trace(ctf_writer);
	BT_ASSERT(writer_trace);

	ret = ctf_copy_clock_classes(writer_component->err, writer_trace,
			writer_stream_class, trace);
	if (ret != BT_COMPONENT_STATUS_OK) {
		BT_LOGE_STR("Failed to copy clock classes.");
		goto error;
	}

	writer_stream_class = ctf_copy_stream_class(writer_component->err,
			stream_class, writer_trace, true);
	if (!writer_stream_class) {
		BT_LOGE_STR("Failed to copy stream class.");
		goto error;
	}

	ret = bt_trace_add_stream_class(writer_trace, writer_stream_class);
	if (ret) {
		BT_LOGE_STR("Failed to add stream_class.");
		goto error;
	}

	g_hash_table_insert(fs_writer->stream_class_map,
			(gpointer) stream_class, writer_stream_class);

	goto end;

error:
	BT_STREAM_CLASS_PUT_REF_AND_RESET(writer_stream_class);
end:
	bt_trace_put_ref(writer_trace);
	bt_trace_put_ref(trace);
	return writer_stream_class;
}

static
enum fs_writer_stream_state *insert_new_stream_state(
		struct writer_component *writer_component,
		struct fs_writer *fs_writer, const bt_stream *stream)
{
	enum fs_writer_stream_state *v = NULL;

	v = g_new0(enum fs_writer_stream_state, 1);
	if (!v) {
		BT_LOGE_STR("Failed to allocate fs_writer_stream_state.");
		goto end;
	}
	*v = FS_WRITER_UNKNOWN_STREAM;

	g_hash_table_insert(fs_writer->stream_states, stream, v);
end:
	return v;
}

/*
 * Make sure the output path is valid for a single trace: either it does
 * not exists or it is empty.
 *
 * Return 0 if the path is valid, -1 otherwise.
 */
static
bool valid_single_trace_path(const char *path)
{
	GError *error = NULL;
	GDir *dir = NULL;
	int ret = 0;

	dir = g_dir_open(path, 0, &error);

	/* Non-existent directory. */
	if (!dir) {
		/* For any other error, return an error */
		if (error->code != G_FILE_ERROR_NOENT) {
			ret = -1;
		}
		goto end;
	}

	/* g_dir_read_name skips "." and "..", error out on first result */
	while (g_dir_read_name(dir) != NULL) {
		ret = -1;
		break;
	}

end:
	if (dir) {
		g_dir_close(dir);
	}
	if (error) {
		g_error_free(error);
	}

	return ret;
}

static
int make_trace_path(struct writer_component *writer_component,
		const bt_trace *trace, char *trace_path)
{
	int ret;
	const char *trace_name;

	if (writer_component->single_trace) {
		trace_name = "\0";
	} else {
		trace_name = bt_trace_get_name(trace);
		if (!trace_name) {
			trace_name = writer_component->trace_name_base->str;
		}
	}

	/* Sanitize the trace name. */
	if (strlen(trace_name) == 2 && !strcmp(trace_name, "..")) {
		BT_LOGE_STR("Trace name cannot be \"..\".");
		goto error;
	}

	if (strstr(trace_name, "../")) {
		BT_LOGE_STR("Trace name cannot contain \"../\".");
		goto error;

	}

	snprintf(trace_path, PATH_MAX, "%s" G_DIR_SEPARATOR_S "%s",
			writer_component->base_path->str,
			trace_name);
	/*
	 * Append a suffix if the trace_path exists and we are not in
	 * single-trace mode.
	 */
	if (writer_component->single_trace) {
		if (valid_single_trace_path(trace_path) != 0) {
			BT_LOGE_STR("Invalid output directory.");
			goto error;
		}
	} else {
		if (g_file_test(trace_path, G_FILE_TEST_EXISTS)) {
			int i = 0;

			do {
				snprintf(trace_path, PATH_MAX, "%s" G_DIR_SEPARATOR_S "%s-%d",
						writer_component->base_path->str,
						trace_name, ++i);
			} while (g_file_test(trace_path, G_FILE_TEST_EXISTS) && i < INT_MAX);
			if (i == INT_MAX) {
				BT_LOGE_STR("Unable to find a unique trace path.");
				goto error;
			}
		}
	}

	ret = 0;
	goto end;

error:
	ret = -1;
end:
	return ret;
}

static
struct fs_writer *insert_new_writer(
		struct writer_component *writer_component,
		const bt_trace *trace)
{
	struct bt_ctf_writer *ctf_writer = NULL;
	const bt_trace *writer_trace = NULL;
	char trace_path[PATH_MAX];
	bt_component_status ret;
	const bt_stream *stream = NULL;
	struct fs_writer *fs_writer = NULL;
	int nr_stream, i;

	if (writer_component->single_trace && writer_component->nr_traces > 0) {
		BT_LOGE_STR("Trying to process more than one trace but single trace mode enabled.");
		goto error;
	}

	ret = make_trace_path(writer_component, trace, trace_path);
	if (ret) {
		BT_LOGE_STR("Failed to make trace path.");
		goto error;
	}

	printf("ctf.fs sink creating trace in %s\n", trace_path);

	ctf_writer = bt_ctf_writer_create(trace_path);
	if (!ctf_writer) {
		BT_LOGE_STR("Failed to create CTF writer.");
		goto error;
	}

	writer_trace = bt_ctf_writer_get_trace(ctf_writer);
	BT_ASSERT(writer_trace);

	ret = ctf_copy_trace(writer_component->err, trace, writer_trace);
	if (ret != BT_COMPONENT_STATUS_OK) {
		BT_LOGE_STR("Failed to copy trace.");
		BT_OBJECT_PUT_REF_AND_RESET(ctf_writer);
		goto error;
	}

	fs_writer = g_new0(struct fs_writer, 1);
	if (!fs_writer) {
		BT_LOGE_STR("Failed to allocate fs_writer.");
		goto error;
	}
	fs_writer->writer = ctf_writer;
	fs_writer->trace = trace;
	fs_writer->writer_trace = writer_trace;
	fs_writer->writer_component = writer_component;
	BT_TRACE_PUT_REF_AND_RESET(writer_trace);
	fs_writer->stream_class_map = g_hash_table_new_full(g_direct_hash,
			g_direct_equal, NULL, (GDestroyNotify) unref_stream_class);
	fs_writer->stream_map = g_hash_table_new_full(g_direct_hash,
			g_direct_equal, NULL, (GDestroyNotify) unref_stream);
	fs_writer->stream_states = g_hash_table_new_full(g_direct_hash,
			g_direct_equal, NULL, destroy_stream_state_key);

	/* Set all the existing streams in the unknown state. */
	nr_stream = bt_trace_get_stream_count(trace);
	for (i = 0; i < nr_stream; i++) {
		stream = bt_trace_get_stream_by_index(trace, i);
		BT_ASSERT(stream);

		insert_new_stream_state(writer_component, fs_writer, stream);
		BT_STREAM_PUT_REF_AND_RESET(stream);
	}

	/* Check if the trace is already static or register a listener. */
	if (bt_trace_is_static(trace)) {
		fs_writer->trace_static = 1;
		fs_writer->static_listener_id = -1;
	} else {
		ret = bt_trace_add_is_static_listener(trace,
				trace_is_static_listener, NULL, fs_writer);
		BT_ASSERT(ret >= 0);
		fs_writer->static_listener_id = ret;
	}

	writer_component->nr_traces++;
	g_hash_table_insert(writer_component->trace_map, (gpointer) trace,
			fs_writer);

	goto end;

error:
	g_free(fs_writer);
	fs_writer = NULL;
	bt_trace_put_ref(writer_trace);
	bt_stream_put_ref(stream);
	BT_OBJECT_PUT_REF_AND_RESET(ctf_writer);
end:
	return fs_writer;
}

static
struct fs_writer *get_fs_writer(struct writer_component *writer_component,
		const bt_stream_class *stream_class)
{
	const bt_trace *trace = NULL;
	struct fs_writer *fs_writer;

	trace = bt_stream_class_get_trace(stream_class);
	BT_ASSERT(trace);

	fs_writer = g_hash_table_lookup(writer_component->trace_map,
			(gpointer) trace);
	if (!fs_writer) {
		fs_writer = insert_new_writer(writer_component, trace);
	}
	BT_TRACE_PUT_REF_AND_RESET(trace);

	return fs_writer;
}

static
struct fs_writer *get_fs_writer_from_stream(
		struct writer_component *writer_component,
		const bt_stream *stream)
{
	const bt_stream_class *stream_class = NULL;
	struct fs_writer *fs_writer;

	stream_class = bt_stream_get_class(stream);
	BT_ASSERT(stream_class);

	fs_writer = get_fs_writer(writer_component, stream_class);

	bt_stream_class_put_ref(stream_class);
	return fs_writer;
}

static
const bt_stream_class *lookup_stream_class(
		struct writer_component *writer_component,
		const bt_stream_class *stream_class)
{
	struct fs_writer *fs_writer = get_fs_writer(
			writer_component, stream_class);
	BT_ASSERT(fs_writer);
	return (const bt_stream_class *) g_hash_table_lookup(
			fs_writer->stream_class_map, (gpointer) stream_class);
}

static
const bt_stream *lookup_stream(struct writer_component *writer_component,
		const bt_stream *stream)
{
	struct fs_writer *fs_writer = get_fs_writer_from_stream(
			writer_component, stream);
	BT_ASSERT(fs_writer);
	return (const bt_stream *) g_hash_table_lookup(
			fs_writer->stream_map, (gpointer) stream);
}

static
const bt_stream *insert_new_stream(
		struct writer_component *writer_component,
		struct fs_writer *fs_writer,
		const bt_stream_class *stream_class,
		const bt_stream *stream)
{
	const bt_stream *writer_stream = NULL;
	const bt_stream_class *writer_stream_class = NULL;
	struct bt_ctf_writer *ctf_writer = bt_object_get_ref(fs_writer->writer);

	writer_stream_class = lookup_stream_class(writer_component,
			stream_class);
	if (!writer_stream_class) {
		writer_stream_class = insert_new_stream_class(
				writer_component, fs_writer, stream_class);
		if (!writer_stream_class) {
			BT_LOGE_STR("Failed to insert a new stream_class.");
			goto error;
		}
	}
	bt_stream_class_get_ref(writer_stream_class);

	writer_stream = bt_stream_create(writer_stream_class,
		bt_stream_get_name(stream));
	BT_ASSERT(writer_stream);

	g_hash_table_insert(fs_writer->stream_map, (gpointer) stream,
			writer_stream);

	goto end;

error:
	BT_STREAM_PUT_REF_AND_RESET(writer_stream);
end:
	bt_object_put_ref(ctf_writer);
	bt_stream_class_put_ref(writer_stream_class);
	return writer_stream;
}

static
const bt_event_class *get_event_class(struct writer_component *writer_component,
		const bt_stream_class *writer_stream_class,
		const bt_event_class *event_class)
{
	return bt_stream_class_get_event_class_by_id(writer_stream_class,
			bt_event_class_get_id(event_class));
}

static
const bt_stream *get_writer_stream(
		struct writer_component *writer_component,
		const bt_packet *packet, const bt_stream *stream)
{
	const bt_stream *writer_stream = NULL;

	writer_stream = lookup_stream(writer_component, stream);
	if (!writer_stream) {
		BT_LOGE_STR("Failed to find existing stream.");
		goto error;
	}
	bt_stream_get_ref(writer_stream);

	goto end;

error:
	BT_STREAM_PUT_REF_AND_RESET(writer_stream);
end:
	return writer_stream;
}

BT_HIDDEN
void writer_close(struct writer_component *writer_component,
		struct fs_writer *fs_writer)
{
	if (fs_writer->static_listener_id >= 0) {
		bt_trace_remove_is_static_listener(fs_writer->trace,
				fs_writer->static_listener_id);
	}

	/* Empty the stream class HT. */
	g_hash_table_foreach_remove(fs_writer->stream_class_map,
			empty_ht, NULL);
	g_hash_table_destroy(fs_writer->stream_class_map);

	/* Empty the stream HT. */
	g_hash_table_foreach_remove(fs_writer->stream_map,
			empty_streams_ht, NULL);
	g_hash_table_destroy(fs_writer->stream_map);

	/* Empty the stream state HT. */
	g_hash_table_foreach_remove(fs_writer->stream_states,
			empty_ht, NULL);
	g_hash_table_destroy(fs_writer->stream_states);
}

BT_HIDDEN
bt_component_status writer_stream_begin(
		struct writer_component *writer_component,
		const bt_stream *stream)
{
	const bt_stream_class *stream_class = NULL;
	struct fs_writer *fs_writer;
	const bt_stream *writer_stream = NULL;
	bt_component_status ret = BT_COMPONENT_STATUS_OK;
	enum fs_writer_stream_state *state;

	stream_class = bt_stream_get_class(stream);
	BT_ASSERT(stream_class);

	fs_writer = get_fs_writer(writer_component, stream_class);
	if (!fs_writer) {
		BT_LOGE_STR("Failed to get fs_writer.");
		goto error;
	}

	/* Set the stream as active */
	state = g_hash_table_lookup(fs_writer->stream_states, stream);
	if (!state) {
		if (fs_writer->trace_static) {
			BT_LOGE_STR("Cannot add new stream on a static trace.");
			goto error;
		}
		state = insert_new_stream_state(writer_component, fs_writer,
				stream);
	}
	if (*state != FS_WRITER_UNKNOWN_STREAM) {
		BT_LOGE("Unexpected stream state: state=%d", *state);
		goto error;
	}
	*state = FS_WRITER_ACTIVE_STREAM;

	writer_stream = insert_new_stream(writer_component, fs_writer,
			stream_class, stream);
	if (!writer_stream) {
		BT_LOGE_STR("Failed to insert new stream.");
		goto error;
	}

	goto end;

error:
	ret = BT_COMPONENT_STATUS_ERROR;
end:
	bt_object_put_ref(stream_class);
	return ret;
}

BT_HIDDEN
bt_component_status writer_stream_end(
		struct writer_component *writer_component,
		const bt_stream *stream)
{
	const bt_stream_class *stream_class = NULL;
	struct fs_writer *fs_writer;
	const bt_trace *trace = NULL;
	bt_component_status ret = BT_COMPONENT_STATUS_OK;
	enum fs_writer_stream_state *state;

	stream_class = bt_stream_get_class(stream);
	BT_ASSERT(stream_class);

	fs_writer = get_fs_writer(writer_component, stream_class);
	if (!fs_writer) {
		BT_LOGE_STR("Failed to get fs_writer.");
		goto error;
	}

	state = g_hash_table_lookup(fs_writer->stream_states, stream);
	if (*state != FS_WRITER_ACTIVE_STREAM) {
		BT_LOGE("Unexpected stream state: state=%d", *state);
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
	BT_OBJECT_PUT_REF_AND_RESET(trace);
	BT_OBJECT_PUT_REF_AND_RESET(stream_class);
	return ret;
}

BT_HIDDEN
bt_component_status writer_new_packet(
		struct writer_component *writer_component,
		const bt_packet *packet)
{
	const bt_stream *stream = NULL, *writer_stream = NULL;
	bt_component_status ret = BT_COMPONENT_STATUS_OK;
	int int_ret;

	stream = bt_packet_get_stream(packet);
	BT_ASSERT(stream);

	writer_stream = get_writer_stream(writer_component, packet, stream);
	if (!writer_stream) {
		BT_LOGE_STR("Failed to get writer_stream.");
		goto error;
	}
	BT_OBJECT_PUT_REF_AND_RESET(stream);

	int_ret = ctf_stream_copy_packet_context(
			writer_component->err, packet, writer_stream);
	if (int_ret < 0) {
		BT_LOGE_STR("Failed to copy packet_context.");
		goto error;
	}

	ret = ctf_stream_copy_packet_header(writer_component->err,
			packet, writer_stream);
	if (ret != 0) {
		BT_LOGE_STR("Failed to copy packet_header.");
		goto error;
	}

	goto end;

error:
	ret = BT_COMPONENT_STATUS_ERROR;
end:
	bt_object_put_ref(writer_stream);
	bt_object_put_ref(stream);
	return ret;
}

BT_HIDDEN
bt_component_status writer_close_packet(
		struct writer_component *writer_component,
		const bt_packet *packet)
{
	const bt_stream *stream = NULL, *writer_stream = NULL;
	bt_component_status ret;

	stream = bt_packet_get_stream(packet);
	BT_ASSERT(stream);

	writer_stream = lookup_stream(writer_component, stream);
	if (!writer_stream) {
		BT_LOGE_STR("Failed to find existing stream.");
		goto error;
	}
	BT_OBJECT_PUT_REF_AND_RESET(stream);

	bt_object_get_ref(writer_stream);

	ret = bt_stream_flush(writer_stream);
	if (ret) {
		BT_LOGE_STR("Failed to flush stream.");
		goto error;
	}

	BT_OBJECT_PUT_REF_AND_RESET(writer_stream);

	ret = BT_COMPONENT_STATUS_OK;
	goto end;

error:
	ret = BT_COMPONENT_STATUS_ERROR;
end:
	bt_object_put_ref(writer_stream);
	bt_object_put_ref(stream);
	return ret;
}

BT_HIDDEN
bt_component_status writer_output_event(
		struct writer_component *writer_component,
		const bt_event *event)
{
	bt_component_status ret;
	const bt_event_class *event_class = NULL, *writer_event_class = NULL;
	const bt_stream *stream = NULL, *writer_stream = NULL;
	const bt_stream_class *stream_class = NULL, *writer_stream_class = NULL;
	const bt_event *writer_event = NULL;
	int int_ret;
	const bt_trace *writer_trace = NULL;

	event_class = bt_event_get_class(event);
	BT_ASSERT(event_class);

	stream = bt_event_get_stream(event);
	BT_ASSERT(stream);

	writer_stream = lookup_stream(writer_component, stream);
	if (!writer_stream || !bt_object_get_ref(writer_stream)) {
		BT_LOGE_STR("Failed for find existing stream.");
		goto error;
	}

	stream_class = bt_event_class_get_stream_class(event_class);
	BT_ASSERT(stream_class);

	writer_stream_class = lookup_stream_class(writer_component, stream_class);
	if (!writer_stream_class || !bt_object_get_ref(writer_stream_class)) {
		BT_LOGE_STR("Failed to find existing stream_class.");
		goto error;
	}

	writer_trace = bt_stream_class_get_trace(writer_stream_class);
	BT_ASSERT(writer_trace);

	writer_event_class = get_event_class(writer_component,
			writer_stream_class, event_class);
	if (!writer_event_class) {
		writer_event_class = ctf_copy_event_class(writer_component->err,
				writer_trace, event_class);
		if (!writer_event_class) {
			BT_LOGE_STR("Failed to copy event_class.");
			goto error;
		}
		int_ret = bt_stream_class_add_event_class(
				writer_stream_class, writer_event_class);
		if (int_ret) {
			BT_LOGE("Failed to add event_class: event_name=\"%s\"",
					bt_event_class_get_name(event_class));
			goto error;
		}
	}

	writer_event = ctf_copy_event(writer_component->err, event,
			writer_event_class, true);
	if (!writer_event) {
		BT_LOGE("Failed to copy event: event_class=\"%s\"",
				bt_event_class_get_name(writer_event_class));
		goto error;
	}

	int_ret = bt_stream_append_event(writer_stream, writer_event);
	if (int_ret < 0) {
		BT_LOGE("Failed to append event: event_class=\"%s\"",
				bt_event_class_get_name(writer_event_class));
		goto error;
	}

	ret = BT_COMPONENT_STATUS_OK;
	goto end;

error:
	ret = BT_COMPONENT_STATUS_ERROR;
end:
	bt_object_put_ref(writer_trace);
	bt_object_put_ref(writer_event);
	bt_object_put_ref(writer_event_class);
	bt_object_put_ref(writer_stream_class);
	bt_object_put_ref(stream_class);
	bt_object_put_ref(writer_stream);
	bt_object_put_ref(stream);
	bt_object_put_ref(event_class);
	return ret;
}
