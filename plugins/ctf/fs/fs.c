/*
 * fs.c
 *
 * Babeltrace CTF file system Reader Component
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
#include <babeltrace/ctf-ir/clock-class.h>
#include <babeltrace/graph/private-component.h>
#include <babeltrace/graph/component.h>
#include <babeltrace/graph/notification-iterator.h>
#include <babeltrace/graph/private-notification-iterator.h>
#include <babeltrace/graph/notification-stream.h>
#include <babeltrace/graph/notification-event.h>
#include <babeltrace/graph/notification-packet.h>
#include <babeltrace/graph/notification-heap.h>
#include <plugins-common.h>
#include <glib.h>
#include <assert.h>
#include <unistd.h>
#include "fs.h"
#include "metadata.h"
#include "data-stream.h"
#include "file.h"

#define PRINT_ERR_STREAM	ctf_fs->error_fp
#define PRINT_PREFIX		"ctf-fs"
#include "print.h"
#define METADATA_TEXT_SIG	"/* CTF 1.8"

BT_HIDDEN
bool ctf_fs_debug;

struct bt_notification_iterator_next_return ctf_fs_iterator_next(
		struct bt_private_notification_iterator *iterator);

static
enum bt_notification_iterator_status ctf_fs_iterator_get_next_notification(
		struct ctf_fs_iterator *it,
		struct ctf_fs_stream *stream,
		struct bt_notification **notification)
{
	enum bt_ctf_notif_iter_status status;
	enum bt_notification_iterator_status ret;

	if (stream->end_reached) {
		status = BT_CTF_NOTIF_ITER_STATUS_EOF;
		goto end;
	}

	status = bt_ctf_notif_iter_get_next_notification(stream->notif_iter,
			notification);
	if (status != BT_CTF_NOTIF_ITER_STATUS_OK &&
			status != BT_CTF_NOTIF_ITER_STATUS_EOF) {
		goto end;
	}

	/* Should be handled in bt_ctf_notif_iter_get_next_notification. */
	if (status == BT_CTF_NOTIF_ITER_STATUS_EOF) {
		*notification = bt_notification_stream_end_create(
				stream->stream);
		if (!*notification) {
			status = BT_CTF_NOTIF_ITER_STATUS_ERROR;
		}
		status = BT_CTF_NOTIF_ITER_STATUS_OK;
		stream->end_reached = true;
	}
end:
	switch (status) {
	case BT_CTF_NOTIF_ITER_STATUS_EOF:
		ret = BT_NOTIFICATION_ITERATOR_STATUS_END;
		break;
	case BT_CTF_NOTIF_ITER_STATUS_OK:
		ret = BT_NOTIFICATION_ITERATOR_STATUS_OK;
		break;
	case BT_CTF_NOTIF_ITER_STATUS_AGAIN:
		/*
		 * Should not make it this far as this is medium-specific;
		 * there is nothing for the user to do and it should have been
		 * handled upstream.
		 */
		assert(0);
	case BT_CTF_NOTIF_ITER_STATUS_INVAL:
		/* No argument provided by the user, so don't return INVAL. */
	case BT_CTF_NOTIF_ITER_STATUS_ERROR:
	default:
		ret = BT_NOTIFICATION_ITERATOR_STATUS_ERROR;
		break;
	}
	return ret;
}

/*
 * Remove me. This is a temporary work-around due to our inhability to use
 * libbabeltrace-ctf from libbabeltrace-plugin.
 */
static
struct bt_ctf_stream *internal_bt_notification_get_stream(
		struct bt_notification *notification)
{
	struct bt_ctf_stream *stream = NULL;

	assert(notification);
	switch (bt_notification_get_type(notification)) {
	case BT_NOTIFICATION_TYPE_EVENT:
	{
		struct bt_ctf_event *event;

		event = bt_notification_event_get_event(notification);
		stream = bt_ctf_event_get_stream(event);
		bt_put(event);
		break;
	}
	case BT_NOTIFICATION_TYPE_PACKET_BEGIN:
	{
		struct bt_ctf_packet *packet;

		packet = bt_notification_packet_begin_get_packet(notification);
		stream = bt_ctf_packet_get_stream(packet);
		bt_put(packet);
		break;
	}
	case BT_NOTIFICATION_TYPE_PACKET_END:
	{
		struct bt_ctf_packet *packet;

		packet = bt_notification_packet_end_get_packet(notification);
		stream = bt_ctf_packet_get_stream(packet);
		bt_put(packet);
		break;
	}
	case BT_NOTIFICATION_TYPE_STREAM_END:
		stream = bt_notification_stream_end_get_stream(notification);
		break;
	default:
		goto end;
	}
end:
	return stream;
}

static
enum bt_notification_iterator_status populate_heap(struct ctf_fs_iterator *it)
{
	size_t i, pending_streams_count = it->pending_streams->len;
	enum bt_notification_iterator_status ret =
			BT_NOTIFICATION_ITERATOR_STATUS_OK;

	/* Insert one stream-associated notification for each stream. */
	for (i = 0; i < pending_streams_count; i++) {
		struct bt_notification *notification;
		struct ctf_fs_stream *fs_stream;
		struct bt_ctf_stream *stream;
		size_t pending_stream_index = pending_streams_count - 1 - i;

		fs_stream = g_ptr_array_index(it->pending_streams,
				pending_stream_index);

		do {
			int heap_ret;

			ret = ctf_fs_iterator_get_next_notification(
					it, fs_stream, &notification);
			if (ret && ret != BT_NOTIFICATION_ITERATOR_STATUS_END) {
				printf_debug("Failed to populate heap at stream %zu\n",
						pending_stream_index);
				goto end;
			}

			stream = internal_bt_notification_get_stream(
					notification);
			if (stream) {
				gboolean inserted;

				/*
				 * Associate pending ctf_fs_stream to
				 * bt_ctf_stream. Ownership of stream
				 * is passed to the stream ht.
				 */
				inserted = g_hash_table_insert(it->stream_ht,
						stream, fs_stream);
				if (!inserted) {
					ret = BT_NOTIFICATION_ITERATOR_STATUS_NOMEM;
					printf_debug("Failed to associate fs stream to ctf stream\n");
					goto end;
				}
			}

			heap_ret = bt_notification_heap_insert(
					it->pending_notifications,
					notification);
			bt_put(notification);
			if (heap_ret) {
				ret = BT_NOTIFICATION_ITERATOR_STATUS_NOMEM;
				printf_debug("Failed to insert notification in heap\n");
				goto end;
			}
		} while (!stream && ret != BT_NOTIFICATION_ITERATOR_STATUS_END);
		/*
		 * Set NULL so the destruction callback registered with the
		 * array is not invoked on the stream (its ownership was
		 * transferred to the streams hashtable).
		 */
		g_ptr_array_index(it->pending_streams,
				pending_stream_index) = NULL;
		g_ptr_array_remove_index(it->pending_streams,
				pending_stream_index);
	}

	g_ptr_array_free(it->pending_streams, TRUE);
	it->pending_streams = NULL;
end:
	return ret;
}

struct bt_notification_iterator_next_return ctf_fs_iterator_next(
		struct bt_private_notification_iterator *iterator)
{
	int heap_ret;
	struct bt_ctf_stream *stream = NULL;
	struct ctf_fs_stream *fs_stream;
	struct bt_notification *next_stream_notification;
	struct ctf_fs_iterator *ctf_it =
			bt_private_notification_iterator_get_user_data(
				iterator);
	struct bt_notification_iterator_next_return ret = {
		.status = BT_NOTIFICATION_ITERATOR_STATUS_OK,
		.notification = NULL,
	};

	ret.notification =
		bt_notification_heap_pop(ctf_it->pending_notifications);
	if (!ret.notification && !ctf_it->pending_streams) {
		ret.status = BT_NOTIFICATION_ITERATOR_STATUS_END;
		goto end;
	}

	if (!ret.notification && ctf_it->pending_streams) {
		/*
		 * Insert at one notification per stream in the heap and pop
		 * one.
		 */
		ret.status = populate_heap(ctf_it);
		if (ret.status) {
			goto end;
		}

		ret.notification = bt_notification_heap_pop(
				ctf_it->pending_notifications);
		if (!ret.notification) {
			ret.status = BT_NOTIFICATION_ITERATOR_STATUS_END;
			goto end;
		}
	}

	/* notification is set from here. */

	stream = internal_bt_notification_get_stream(ret.notification);
	if (!stream) {
		/*
		 * The current notification is not associated to a particular
		 * stream, there is no need to insert a new notification from
		 * a stream in the heap.
		 */
		goto end;
	}

	fs_stream = g_hash_table_lookup(ctf_it->stream_ht, stream);
	if (!fs_stream) {
		/* We have reached this stream's end. */
		goto end;
	}

	ret.status = ctf_fs_iterator_get_next_notification(ctf_it, fs_stream,
			&next_stream_notification);
	if ((ret.status && ret.status != BT_NOTIFICATION_ITERATOR_STATUS_END)) {
		heap_ret = bt_notification_heap_insert(
				ctf_it->pending_notifications,
				ret.notification);

		assert(!next_stream_notification);
		if (heap_ret) {
			/*
			 * We're dropping the most recent notification, but at
			 * this point, something is seriously wrong...
			 */
			ret.status = BT_NOTIFICATION_ITERATOR_STATUS_NOMEM;
		}
		BT_PUT(ret.notification);
		goto end;
	}

	if (ret.status == BT_NOTIFICATION_ITERATOR_STATUS_END) {
		gboolean success;

		/* Remove stream. */
		success = g_hash_table_remove(ctf_it->stream_ht, stream);
		assert(success);
		ret.status = BT_NOTIFICATION_ITERATOR_STATUS_OK;
	} else {
		heap_ret = bt_notification_heap_insert(ctf_it->pending_notifications,
						       next_stream_notification);
		BT_PUT(next_stream_notification);
		if (heap_ret) {
			/*
			 * We're dropping the most recent notification...
			 */
			ret.status = BT_NOTIFICATION_ITERATOR_STATUS_NOMEM;
		}
	}

	/*
	 * Ensure that the stream is removed from both pending_streams and
	 * the streams hashtable on reception of the "end of stream"
	 * notification.
	 */
end:
	bt_put(stream);
	return ret;
}

static
void ctf_fs_iterator_destroy_data(struct ctf_fs_iterator *ctf_it)
{
	if (!ctf_it) {
		return;
	}
	bt_put(ctf_it->pending_notifications);
	if (ctf_it->pending_streams) {
		g_ptr_array_free(ctf_it->pending_streams, TRUE);
	}
	if (ctf_it->stream_ht) {
		g_hash_table_destroy(ctf_it->stream_ht);
	}
	g_free(ctf_it);
}

void ctf_fs_iterator_finalize(struct bt_private_notification_iterator *it)
{
	void *data = bt_private_notification_iterator_get_user_data(it);

	ctf_fs_iterator_destroy_data(data);
}

static
bool compare_event_notifications(struct bt_notification *a,
		struct bt_notification *b)
{
	int ret;
	struct bt_ctf_clock_class *clock_class;
	struct bt_ctf_clock_value *a_clock_value, *b_clock_value;
	struct bt_ctf_stream_class *a_stream_class;
	struct bt_ctf_stream *a_stream;
	struct bt_ctf_event *a_event, *b_event;
	struct bt_ctf_trace *trace;
	int64_t a_ts, b_ts;

	// FIXME - assumes only one clock
	a_event = bt_notification_event_get_event(a);
	b_event = bt_notification_event_get_event(b);
	assert(a_event);
	assert(b_event);

	a_stream = bt_ctf_event_get_stream(a_event);
	assert(a_stream);
	a_stream_class = bt_ctf_stream_get_class(a_stream);
	assert(a_stream_class);
	trace = bt_ctf_stream_class_get_trace(a_stream_class);
	assert(trace);

	clock_class = bt_ctf_trace_get_clock_class(trace, 0);
	a_clock_value = bt_ctf_event_get_clock_value(a_event, clock_class);
	b_clock_value = bt_ctf_event_get_clock_value(b_event, clock_class);
	assert(a_clock_value);
	assert(b_clock_value);

	ret = bt_ctf_clock_value_get_value_ns_from_epoch(a_clock_value, &a_ts);
	assert(!ret);
	ret = bt_ctf_clock_value_get_value_ns_from_epoch(b_clock_value, &b_ts);
	assert(!ret);

	bt_put(a_event);
	bt_put(b_event);
	bt_put(a_clock_value);
	bt_put(b_clock_value);
	bt_put(a_stream);
	bt_put(a_stream_class);
	bt_put(clock_class);
	bt_put(trace);
	return a_ts < b_ts;
}

static
bool compare_notifications(struct bt_notification *a, struct bt_notification *b,
		void *unused)
{
	static int notification_priorities[] = {
		[BT_NOTIFICATION_TYPE_NEW_TRACE] = 0,
		[BT_NOTIFICATION_TYPE_NEW_STREAM_CLASS] = 1,
		[BT_NOTIFICATION_TYPE_NEW_EVENT_CLASS] = 2,
		[BT_NOTIFICATION_TYPE_PACKET_BEGIN] = 3,
		[BT_NOTIFICATION_TYPE_PACKET_END] = 4,
		[BT_NOTIFICATION_TYPE_EVENT] = 5,
		[BT_NOTIFICATION_TYPE_END_OF_TRACE] = 6,
	};
	int a_prio, b_prio;
	enum bt_notification_type a_type, b_type;

	assert(a && b);
	a_type = bt_notification_get_type(a);
	b_type = bt_notification_get_type(b);
	assert(a_type > BT_NOTIFICATION_TYPE_ALL);
	assert(a_type < BT_NOTIFICATION_TYPE_NR);
	assert(b_type > BT_NOTIFICATION_TYPE_ALL);
	assert(b_type < BT_NOTIFICATION_TYPE_NR);

	a_prio = notification_priorities[a_type];
	b_prio = notification_priorities[b_type];

	if (likely((a_type == b_type) && a_type == BT_NOTIFICATION_TYPE_EVENT)) {
		return compare_event_notifications(a, b);
	}

	if (unlikely(a_prio != b_prio)) {
		return a_prio < b_prio;
	}

	/* Notification types are equal, but not of type "event". */
	switch (a_type) {
	case BT_NOTIFICATION_TYPE_PACKET_BEGIN:
	case BT_NOTIFICATION_TYPE_PACKET_END:
	case BT_NOTIFICATION_TYPE_STREAM_END:
	{
		int64_t a_sc_id, b_sc_id;
		struct bt_ctf_stream *a_stream, *b_stream;
		struct bt_ctf_stream_class *a_sc, *b_sc;

		a_stream = internal_bt_notification_get_stream(a);
		b_stream = internal_bt_notification_get_stream(b);
		assert(a_stream && b_stream);

		a_sc = bt_ctf_stream_get_class(a_stream);
		b_sc = bt_ctf_stream_get_class(b_stream);
		assert(a_sc && b_sc);

		a_sc_id = bt_ctf_stream_class_get_id(a_sc);
		b_sc_id = bt_ctf_stream_class_get_id(b_sc);
		assert(a_sc_id >= 0 && b_sc_id >= 0);
		bt_put(a_sc);
		bt_put(a_stream);
		bt_put(b_sc);
		bt_put(b_stream);
		return a_sc_id < b_sc_id;
	}
	case BT_NOTIFICATION_TYPE_NEW_TRACE:
	case BT_NOTIFICATION_TYPE_END_OF_TRACE:
		/* Impossible to have two separate traces. */
	default:
		assert(0);
	}

	assert(0);
	return a < b;
}

static
void stream_destroy(void *stream)
{
	ctf_fs_stream_destroy((struct ctf_fs_stream *) stream);
}

static
int open_trace_streams(struct ctf_fs_component *ctf_fs,
		struct ctf_fs_iterator *ctf_it)
{
	int ret = 0;
	const char *name;
	GError *error = NULL;
	GDir *dir = g_dir_open(ctf_fs->trace_path->str, 0, &error);

	if (!dir) {
		PERR("Cannot open directory \"%s\": %s (code %d)\n",
				ctf_fs->trace_path->str, error->message,
				error->code);
		goto error;
	}

	while ((name = g_dir_read_name(dir))) {
		struct ctf_fs_file *file = NULL;
		struct ctf_fs_stream *stream = NULL;

		if (!strcmp(name, CTF_FS_METADATA_FILENAME)) {
			/* Ignore the metadata stream. */
			PDBG("Ignoring metadata file \"%s\"\n",
					name);
			continue;
		}

		if (name[0] == '.') {
			PDBG("Ignoring hidden file \"%s\"\n",
					name);
			continue;
		}

		/* Create the file. */
		file = ctf_fs_file_create(ctf_fs);
		if (!file) {
			PERR("Cannot create stream file object\n");
			goto error;
		}

		/* Create full path string. */
		g_string_append_printf(file->path, "%s/%s",
				ctf_fs->trace_path->str, name);
		if (!g_file_test(file->path->str, G_FILE_TEST_IS_REGULAR)) {
			PDBG("Ignoring non-regular file \"%s\"\n", name);
			ctf_fs_file_destroy(file);
			continue;
		}

		/* Open the file. */
		if (ctf_fs_file_open(ctf_fs, file, "rb")) {
			ctf_fs_file_destroy(file);
			goto error;
		}

		if (file->size == 0) {
			/* Skip empty stream. */
			ctf_fs_file_destroy(file);
			continue;
		}

		/* Create a private stream; file ownership is passed to it. */
		stream = ctf_fs_stream_create(ctf_fs, file);
		if (!stream) {
			ctf_fs_file_destroy(file);
			goto error;
		}

		g_ptr_array_add(ctf_it->pending_streams, stream);
	}

	goto end;
error:
	ret = -1;
end:
	if (dir) {
		g_dir_close(dir);
		dir = NULL;
	}
	if (error) {
		g_error_free(error);
	}
	return ret;
}

enum bt_notification_iterator_status ctf_fs_iterator_init(
		struct bt_private_notification_iterator *it,
		struct bt_private_port *port)
{
	struct ctf_fs_iterator *ctf_it;
	struct ctf_fs_component *ctf_fs;
	struct bt_private_component *source =
		bt_private_notification_iterator_get_private_component(it);
	enum bt_notification_iterator_status ret = BT_NOTIFICATION_ITERATOR_STATUS_OK;

	assert(source && it);

	ctf_fs = bt_private_component_get_user_data(source);
	if (!ctf_fs) {
		ret = BT_NOTIFICATION_ITERATOR_STATUS_INVAL;
		goto end;
	}

	ctf_it = g_new0(struct ctf_fs_iterator, 1);
	if (!ctf_it) {
		ret = BT_NOTIFICATION_ITERATOR_STATUS_NOMEM;
		goto end;
	}

	ctf_it->stream_ht = g_hash_table_new_full(g_direct_hash,
			g_direct_equal, bt_put, stream_destroy);
	if (!ctf_it->stream_ht) {
		goto error;
	}
	ctf_it->pending_streams = g_ptr_array_new_with_free_func(
			stream_destroy);
	if (!ctf_it->pending_streams) {
		goto error;
	}
	ctf_it->pending_notifications = bt_notification_heap_create(
			compare_notifications, NULL);
	if (!ctf_it->pending_notifications) {
		goto error;
	}

	ret = open_trace_streams(ctf_fs, ctf_it);
	if (ret) {
		goto error;
	}

	ret = bt_private_notification_iterator_set_user_data(it, ctf_it);
	if (ret) {
		goto error;
	}

	goto end;

error:
	(void) bt_private_notification_iterator_set_user_data(it, NULL);
	ctf_fs_iterator_destroy_data(ctf_it);

end:
	bt_put(source);
	return ret;
}

static
void ctf_fs_destroy_data(struct ctf_fs_component *ctf_fs)
{
	if (!ctf_fs) {
		return;
	}
	if (ctf_fs->trace_path) {
		g_string_free(ctf_fs->trace_path, TRUE);
	}
	if (ctf_fs->metadata) {
		ctf_fs_metadata_fini(ctf_fs->metadata);
		g_free(ctf_fs->metadata);
	}
	g_free(ctf_fs);
}

void ctf_fs_finalize(struct bt_private_component *component)
{
	void *data = bt_private_component_get_user_data(component);

	ctf_fs_destroy_data(data);
}

static
struct ctf_fs_component *ctf_fs_create(struct bt_value *params)
{
	struct ctf_fs_component *ctf_fs;
	struct bt_value *value = NULL;
	const char *path;
	enum bt_value_status ret;

	ctf_fs = g_new0(struct ctf_fs_component, 1);
	if (!ctf_fs) {
		goto end;
	}

	/* FIXME: should probably look for a source URI */
	value = bt_value_map_get(params, "path");
	if (!value || bt_value_is_null(value) || !bt_value_is_string(value)) {
		goto error;
	}

	ret = bt_value_string_get(value, &path);
	if (ret != BT_VALUE_STATUS_OK) {
		goto error;
	}

	ctf_fs->trace_path = g_string_new(path);
	if (!ctf_fs->trace_path) {
		goto error;
	}
	ctf_fs->error_fp = stderr;
	ctf_fs->page_size = (size_t) getpagesize();

	// FIXME: check error.
	ctf_fs->metadata = g_new0(struct ctf_fs_metadata, 1);
	if (!ctf_fs->metadata) {
		goto error;
	}
	ctf_fs_metadata_set_trace(ctf_fs);
	goto end;

error:
	ctf_fs_destroy_data(ctf_fs);
	ctf_fs = NULL;
end:
	BT_PUT(value);
	return ctf_fs;
}

BT_HIDDEN
enum bt_component_status ctf_fs_init(struct bt_private_component *source,
		struct bt_value *params, UNUSED_VAR void *init_method_data)
{
	struct ctf_fs_component *ctf_fs;
	enum bt_component_status ret = BT_COMPONENT_STATUS_OK;

	assert(source);
	ctf_fs_debug = g_strcmp0(getenv("CTF_FS_DEBUG"), "1") == 0;
	ctf_fs = ctf_fs_create(params);
	if (!ctf_fs) {
		ret = BT_COMPONENT_STATUS_NOMEM;
		goto end;
	}

	ret = bt_private_component_set_user_data(source, ctf_fs);
	if (ret != BT_COMPONENT_STATUS_OK) {
		goto error;
	}
end:
	return ret;
error:
	(void) bt_private_component_set_user_data(source, NULL);
        ctf_fs_destroy_data(ctf_fs);
	return ret;
}

BT_HIDDEN
struct bt_value *ctf_fs_query(struct bt_component_class *comp_class,
		const char *object, struct bt_value *params)
{
	struct bt_value *results = NULL;
	struct bt_value *path_value = NULL;
	char *metadata_text = NULL;
	FILE *metadata_fp = NULL;
	GString *g_metadata_text = NULL;

	if (strcmp(object, "metadata-info") == 0) {
		int ret;
		int bo;
		const char *path;
		bool is_packetized;

		results = bt_value_map_create();
		if (!results) {
			goto error;
		}

		if (!bt_value_is_map(params)) {
			fprintf(stderr,
				"Query parameters is not a map value object\n");
			goto error;
		}

		path_value = bt_value_map_get(params, "path");
		ret = bt_value_string_get(path_value, &path);
		if (ret) {
			fprintf(stderr,
				"Cannot get `path` string parameter\n");
			goto error;
		}

		assert(path);
		metadata_fp = ctf_fs_metadata_open_file(path);
		if (!metadata_fp) {
			fprintf(stderr,
				"Cannot open trace at path `%s`\n", path);
			goto error;
		}

		is_packetized = ctf_metadata_is_packetized(metadata_fp, &bo);

		if (is_packetized) {
			ret = ctf_metadata_packetized_file_to_buf(NULL,
				metadata_fp, (uint8_t **) &metadata_text, bo);
			if (ret) {
				fprintf(stderr,
					"Cannot decode packetized metadata file\n");
				goto error;
			}
		} else {
			long filesize;

			fseek(metadata_fp, 0, SEEK_END);
			filesize = ftell(metadata_fp);
			rewind(metadata_fp);
			metadata_text = malloc(filesize + 1);
			if (!metadata_text) {
				fprintf(stderr,
					"Cannot allocate buffer for metadata text\n");
				goto error;
			}

			if (fread(metadata_text, filesize, 1, metadata_fp) !=
					1) {
				fprintf(stderr,
					"Cannot read metadata file\n");
				goto error;
			}

			metadata_text[filesize] = '\0';
		}

		g_metadata_text = g_string_new(NULL);
		if (!g_metadata_text) {
			goto error;
		}

		if (strncmp(metadata_text, METADATA_TEXT_SIG,
				sizeof(METADATA_TEXT_SIG) - 1) != 0) {
			g_string_assign(g_metadata_text, METADATA_TEXT_SIG);
			g_string_append(g_metadata_text, " */\n\n");
		}

		g_string_append(g_metadata_text, metadata_text);

		ret = bt_value_map_insert_string(results, "text",
			g_metadata_text->str);
		if (ret) {
			fprintf(stderr, "Cannot insert metadata text into results\n");
			goto error;
		}

		ret = bt_value_map_insert_bool(results, "is-packetized",
			is_packetized);
		if (ret) {
			fprintf(stderr, "Cannot insert is packetized into results\n");
			goto error;
		}
	} else {
		fprintf(stderr, "Unknown query object `%s`\n", object);
		goto error;
	}

	goto end;

error:
	BT_PUT(results);

end:
	bt_put(path_value);
	free(metadata_text);

	if (g_metadata_text) {
		g_string_free(g_metadata_text, TRUE);
	}

	if (metadata_fp) {
		fclose(metadata_fp);
	}
	return results;
}
