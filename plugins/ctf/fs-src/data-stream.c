/*
 * Copyright 2016 - Philippe Proulx <pproulx@efficios.com>
 * Copyright 2016 - Jérémie Galarneau <jeremie.galarneau@efficios.com>
 * Copyright 2010-2011 - EfficiOS Inc. and Linux Foundation
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

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <glib.h>
#include <inttypes.h>
#include <sys/mman.h>
#include <babeltrace/ctf-ir/stream.h>
#include <babeltrace/graph/notification-iterator.h>
#include <babeltrace/graph/notification-stream.h>
#include <babeltrace/graph/notification-event.h>
#include <babeltrace/graph/notification-packet.h>
#include "file.h"
#include "metadata.h"
#include "../common/notif-iter/notif-iter.h"
#include <assert.h>
#include "data-stream.h"

#define PRINT_ERR_STREAM	ctf_fs->error_fp
#define PRINT_PREFIX		"ctf-fs-data-stream"
#define PRINT_DBG_CHECK		ctf_fs_debug
#include "../print.h"

static inline
size_t remaining_mmap_bytes(struct ctf_fs_stream *stream)
{
	return stream->mmap_valid_len - stream->request_offset;
}

static
int stream_munmap(struct ctf_fs_stream *stream)
{
	int ret = 0;
	struct ctf_fs_component *ctf_fs = stream->file->ctf_fs;

	if (munmap(stream->mmap_addr, stream->mmap_len)) {
		PERR("Cannot memory-unmap address %p (size %zu) of file \"%s\" (%p): %s\n",
			stream->mmap_addr, stream->mmap_len,
			stream->file->path->str, stream->file->fp,
			strerror(errno));
		ret = -1;
		goto end;
	}
end:
	return ret;
}

static
enum bt_ctf_notif_iter_medium_status mmap_next(struct ctf_fs_stream *stream)
{
	enum bt_ctf_notif_iter_medium_status ret =
			BT_CTF_NOTIF_ITER_MEDIUM_STATUS_OK;
	struct ctf_fs_component *ctf_fs = stream->file->ctf_fs;

	/* Unmap old region */
	if (stream->mmap_addr) {
		if (stream_munmap(stream)) {
			goto error;
		}

		stream->mmap_offset += stream->mmap_valid_len;
		stream->request_offset = 0;
	}

	stream->mmap_valid_len = MIN(stream->file->size - stream->mmap_offset,
			stream->mmap_max_len);
	if (stream->mmap_valid_len == 0) {
		ret = BT_CTF_NOTIF_ITER_MEDIUM_STATUS_EOF;
		goto end;
	}
	/* Round up to next page, assuming page size being a power of 2. */
	stream->mmap_len = (stream->mmap_valid_len + ctf_fs->page_size - 1)
			& ~(ctf_fs->page_size - 1);
	/* Map new region */
	assert(stream->mmap_len);
	stream->mmap_addr = mmap((void *) 0, stream->mmap_len,
			PROT_READ, MAP_PRIVATE, fileno(stream->file->fp),
			stream->mmap_offset);
	if (stream->mmap_addr == MAP_FAILED) {
		PERR("Cannot memory-map address (size %zu) of file \"%s\" (%p) at offset %zu: %s\n",
				stream->mmap_len, stream->file->path->str,
				stream->file->fp, stream->mmap_offset,
				strerror(errno));
		goto error;
	}

	goto end;
error:
	stream_munmap(stream);
	ret = BT_CTF_NOTIF_ITER_MEDIUM_STATUS_ERROR;
end:
	return ret;
}

static
enum bt_ctf_notif_iter_medium_status medop_request_bytes(
		size_t request_sz, uint8_t **buffer_addr,
		size_t *buffer_sz, void *data)
{
	enum bt_ctf_notif_iter_medium_status status =
		BT_CTF_NOTIF_ITER_MEDIUM_STATUS_OK;
	struct ctf_fs_stream *stream = data;
	struct ctf_fs_component *ctf_fs = stream->file->ctf_fs;

	if (request_sz == 0) {
		goto end;
	}

	/* Check if we have at least one memory-mapped byte left */
	if (remaining_mmap_bytes(stream) == 0) {
		/* Are we at the end of the file? */
		if (stream->mmap_offset >= stream->file->size) {
			PDBG("Reached end of file \"%s\" (%p)\n",
				stream->file->path->str, stream->file->fp);
			status = BT_CTF_NOTIF_ITER_MEDIUM_STATUS_EOF;
			goto end;
		}

		status = mmap_next(stream);
		switch (status) {
		case BT_CTF_NOTIF_ITER_MEDIUM_STATUS_OK:
			break;
		case BT_CTF_NOTIF_ITER_MEDIUM_STATUS_EOF:
			goto end;
		default:
			PERR("Cannot memory-map next region of file \"%s\" (%p)\n",
					stream->file->path->str,
					stream->file->fp);
			goto error;
		}
	}

	*buffer_sz = MIN(remaining_mmap_bytes(stream), request_sz);
	*buffer_addr = ((uint8_t *) stream->mmap_addr) + stream->request_offset;
	stream->request_offset += *buffer_sz;
	goto end;

error:
	status = BT_CTF_NOTIF_ITER_MEDIUM_STATUS_ERROR;

end:
	return status;
}

static
struct bt_ctf_stream *medop_get_stream(
		struct bt_ctf_stream_class *stream_class, void *data)
{
	struct ctf_fs_stream *fs_stream = data;
	struct ctf_fs_component *ctf_fs = fs_stream->file->ctf_fs;

	if (!fs_stream->stream) {
		int64_t id = bt_ctf_stream_class_get_id(stream_class);

		PDBG("Creating stream out of stream class %" PRId64 "\n", id);
		fs_stream->stream = bt_ctf_stream_create(stream_class,
				fs_stream->file->path->str);
		if (!fs_stream->stream) {
			PERR("Cannot create stream (stream class %" PRId64 ")\n",
					id);
		}
	}

	return fs_stream->stream;
}

static struct bt_ctf_notif_iter_medium_ops medops = {
	.request_bytes = medop_request_bytes,
	.get_stream = medop_get_stream,
};

static
int build_index_from_idx_file(struct ctf_fs_stream *stream)
{
	int ret = 0;
	gchar *directory = NULL;
	gchar *basename = NULL;
	GString *index_basename = NULL;
	gchar *index_file_path = NULL;
	GMappedFile *mapped_file = NULL;
	gsize filesize;
	const struct ctf_packet_index_file_hdr *header;
	const char *mmap_begin, *file_pos;
	struct index_entry *index;
	uint64_t total_packets_size = 0;
	size_t file_index_entry_size;
	size_t file_entry_count;
	size_t i;

	/* Look for index file in relative path index/name.idx. */
	basename = g_path_get_basename(stream->file->path->str);
	if (!basename) {
		ret = -1;
		goto end;
	}

	directory = g_path_get_dirname(stream->file->path->str);
	if (!directory) {
		ret = -1;
		goto end;
	}

	index_basename = g_string_new(basename);
	if (!index_basename) {
		ret = -1;
		goto end;
	}

	g_string_append(index_basename, ".idx");
	index_file_path = g_build_filename(directory, "index",
			index_basename->str, NULL);
	mapped_file = g_mapped_file_new(index_file_path, FALSE, NULL);
	if (!mapped_file) {
		ret = -1;
		goto end;
	}
	filesize = g_mapped_file_get_length(mapped_file);
	if (filesize < sizeof(*header)) {
		printf_error("Invalid LTTng trace index: file size < header size");
		ret = -1;
		goto end;
	}

	mmap_begin = g_mapped_file_get_contents(mapped_file);
	header = (struct ctf_packet_index_file_hdr *) mmap_begin;

	file_pos = g_mapped_file_get_contents(mapped_file) + sizeof(*header);
	if (be32toh(header->magic) != CTF_INDEX_MAGIC) {
		printf_error("Invalid LTTng trace index: \"magic\" validation failed");
		ret = -1;
		goto end;
	}

	file_index_entry_size = be32toh(header->packet_index_len);
	file_entry_count = (filesize - sizeof(*header)) / file_index_entry_size;
	if ((filesize - sizeof(*header)) % (file_entry_count * file_index_entry_size)) {
		printf_error("Invalid index file size; not a multiple of index entry size");
		ret = -1;
		goto end;
	}

	stream->index.entries = g_array_sized_new(FALSE, TRUE,
			sizeof(struct index_entry), file_entry_count);
	if (!stream->index.entries) {
		ret = -1;
		goto end;
	}
	index = (struct index_entry *) stream->index.entries->data;
	for (i = 0; i < file_entry_count; i++) {
		struct ctf_packet_index *file_index =
				(struct ctf_packet_index *) file_pos;
		uint64_t packet_size = be64toh(file_index->packet_size);

		if (packet_size % CHAR_BIT) {
			ret = -1;
			printf_error("Invalid packet size encountered in index file");
			goto invalid_index;
		}

		/* Convert size in bits to bytes. */
		packet_size /= CHAR_BIT;
		index->packet_size = packet_size;

		index->offset = be64toh(file_index->offset);
		if (i != 0 && index->offset < (index - 1)->offset) {
			printf_error("Invalid, non-monotonic, packet offset encountered in index file");
			ret = -1;
			goto invalid_index;
		}

		index->timestamp_begin = be64toh(file_index->timestamp_begin);
		index->timestamp_end = be64toh(file_index->timestamp_end);
		if (index->timestamp_end < index->timestamp_begin) {
			printf_error("Invalid packet time bounds encountered in index file");
			ret = -1;
			goto invalid_index;
		}

		total_packets_size += packet_size;
		file_pos += file_index_entry_size;
		index++;
	}

	/* Validate that the index addresses the complete stream. */
	if (stream->file->size != total_packets_size) {
		printf_error("Invalid index; indexed size != stream file size");
		ret = -1;
		goto invalid_index;
	}
end:
	g_free(directory);
	g_free(basename);
	g_free(index_file_path);
	if (index_basename) {
		g_string_free(index_basename, TRUE);
	}
	if (mapped_file) {
		g_mapped_file_unref(mapped_file);
	}
	return ret;
invalid_index:
	g_array_free(stream->index.entries, TRUE);
	goto end;
}

static
int build_index_from_stream(struct ctf_fs_stream *stream)
{
	return 0;
}

static
int init_stream_index(struct ctf_fs_stream *stream)
{
	int ret;

	ret = build_index_from_idx_file(stream);
	if (!ret) {
		goto end;
	}

	ret = build_index_from_stream(stream);
end:
	return ret;
}

BT_HIDDEN
struct ctf_fs_stream *ctf_fs_stream_create(
		struct ctf_fs_trace *ctf_fs_trace, const char *path)
{
	int ret;
	struct ctf_fs_stream *ctf_fs_stream = g_new0(struct ctf_fs_stream, 1);

	if (!ctf_fs_stream) {
		goto error;
	}

	ctf_fs_stream->file = ctf_fs_file_create(ctf_fs_trace->ctf_fs);
	if (!ctf_fs_stream->file) {
		goto error;
	}

	ctf_fs_stream->cc_prio_map = bt_get(ctf_fs_trace->cc_prio_map);
	g_string_assign(ctf_fs_stream->file->path, path);
	ret = ctf_fs_file_open(ctf_fs_trace->ctf_fs, ctf_fs_stream->file, "rb");
	if (ret) {
		goto error;
	}

	ctf_fs_stream->notif_iter = bt_ctf_notif_iter_create(
		ctf_fs_trace->metadata->trace,
		ctf_fs_trace->ctf_fs->page_size, medops, ctf_fs_stream,
		ctf_fs_trace->ctf_fs->error_fp);
	if (!ctf_fs_stream->notif_iter) {
		goto error;
	}

	ctf_fs_stream->mmap_max_len = ctf_fs_trace->ctf_fs->page_size * 2048;
	ret = init_stream_index(ctf_fs_stream);
	if (ret) {
		goto error;
	}

	goto end;

error:
	/* Do not touch "borrowed" file. */
	ctf_fs_stream_destroy(ctf_fs_stream);
	ctf_fs_stream = NULL;

end:
	return ctf_fs_stream;
}

BT_HIDDEN
void ctf_fs_stream_destroy(struct ctf_fs_stream *ctf_fs_stream)
{
	if (!ctf_fs_stream) {
		return;
	}

	bt_put(ctf_fs_stream->cc_prio_map);

	if (ctf_fs_stream->file) {
		ctf_fs_file_destroy(ctf_fs_stream->file);
	}

	if (ctf_fs_stream->stream) {
		BT_PUT(ctf_fs_stream->stream);
	}

	if (ctf_fs_stream->notif_iter) {
		bt_ctf_notif_iter_destroy(ctf_fs_stream->notif_iter);
	}

	if (ctf_fs_stream->index.entries) {
		g_array_free(ctf_fs_stream->index.entries, TRUE);
	}

	g_free(ctf_fs_stream);
}

BT_HIDDEN
struct bt_notification_iterator_next_return ctf_fs_stream_next(
		struct ctf_fs_stream *stream)
{
	enum bt_ctf_notif_iter_status notif_iter_status;
	struct bt_notification_iterator_next_return ret = {
		.status = BT_NOTIFICATION_ITERATOR_STATUS_ERROR,
		.notification = NULL,
	};

	if (stream->end_reached) {
		notif_iter_status = BT_CTF_NOTIF_ITER_STATUS_EOF;
		goto translate_status;
	}

	notif_iter_status = bt_ctf_notif_iter_get_next_notification(
		stream->notif_iter, stream->cc_prio_map, &ret.notification);
	if (notif_iter_status != BT_CTF_NOTIF_ITER_STATUS_OK &&
			notif_iter_status != BT_CTF_NOTIF_ITER_STATUS_EOF) {
		goto translate_status;
	}

	/* Should be handled in bt_ctf_notif_iter_get_next_notification. */
	if (notif_iter_status == BT_CTF_NOTIF_ITER_STATUS_EOF) {
		ret.notification = bt_notification_stream_end_create(
			stream->stream);
		if (!ret.notification) {
			notif_iter_status = BT_CTF_NOTIF_ITER_STATUS_ERROR;
			goto translate_status;
		}

		notif_iter_status = BT_CTF_NOTIF_ITER_STATUS_OK;
		stream->end_reached = true;
	}

translate_status:
	switch (notif_iter_status) {
	case BT_CTF_NOTIF_ITER_STATUS_EOF:
		ret.status = BT_NOTIFICATION_ITERATOR_STATUS_END;
		break;
	case BT_CTF_NOTIF_ITER_STATUS_OK:
		ret.status = BT_NOTIFICATION_ITERATOR_STATUS_OK;
		break;
	case BT_CTF_NOTIF_ITER_STATUS_AGAIN:
		/*
		 * Should not make it this far as this is
		 * medium-specific; there is nothing for the user to do
		 * and it should have been handled upstream.
		 */
		assert(false);
	case BT_CTF_NOTIF_ITER_STATUS_INVAL:
	case BT_CTF_NOTIF_ITER_STATUS_ERROR:
	default:
		ret.status = BT_NOTIFICATION_ITERATOR_STATUS_ERROR;
		break;
	}

	return ret;
}
