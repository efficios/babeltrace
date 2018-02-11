/*
 * Copyright 2016-2017 - Philippe Proulx <pproulx@efficios.com>
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

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <glib.h>
#include <inttypes.h>
#include <babeltrace/compat/mman-internal.h>
#include <babeltrace/endian-internal.h>
#include <babeltrace/babeltrace.h>
#include <babeltrace/common-internal.h>
#include "file.h"
#include "metadata.h"
#include "../common/notif-iter/notif-iter.h"
#include <assert.h>
#include "data-stream-file.h"
#include <string.h>

#define BT_LOG_TAG "PLUGIN-CTF-FS-SRC-DS"
#include "logging.h"

static inline
size_t remaining_mmap_bytes(struct ctf_fs_ds_file *ds_file)
{
	return ds_file->mmap_len - ds_file->request_offset;
}

static
int ds_file_munmap(struct ctf_fs_ds_file *ds_file)
{
	int ret = 0;

	if (!ds_file || !ds_file->mmap_addr) {
		goto end;
	}

	if (bt_munmap(ds_file->mmap_addr, ds_file->mmap_len)) {
		BT_LOGE_ERRNO("Cannot memory-unmap file",
			": address=%p, size=%zu, file_path=\"%s\", file=%p",
			ds_file->mmap_addr, ds_file->mmap_len,
			ds_file->file ? ds_file->file->path->str : "NULL",
			ds_file->file ? ds_file->file->fp : NULL);
		ret = -1;
		goto end;
	}

	ds_file->mmap_addr = NULL;

end:
	return ret;
}

static
enum bt_notif_iter_medium_status ds_file_mmap_next(
		struct ctf_fs_ds_file *ds_file)
{
	enum bt_notif_iter_medium_status ret =
			BT_NOTIF_ITER_MEDIUM_STATUS_OK;

	/* Unmap old region */
	if (ds_file->mmap_addr) {
		if (ds_file_munmap(ds_file)) {
			goto error;
		}

		/*
		 * mmap_len is guaranteed to be page-aligned except on the
		 * last mapping where it may not be possible (since the file's
		 * size itself may not be a page multiple).
		 */
		ds_file->mmap_offset += ds_file->mmap_len;
		ds_file->request_offset = 0;
	}

	ds_file->mmap_len = MIN(ds_file->file->size - ds_file->mmap_offset,
			ds_file->mmap_max_len);
	if (ds_file->mmap_len == 0) {
		ret = BT_NOTIF_ITER_MEDIUM_STATUS_EOF;
		goto end;
	}
	/* Map new region */
	assert(ds_file->mmap_len);
	ds_file->mmap_addr = bt_mmap((void *) 0, ds_file->mmap_len,
			PROT_READ, MAP_PRIVATE, fileno(ds_file->file->fp),
			ds_file->mmap_offset);
	if (ds_file->mmap_addr == MAP_FAILED) {
		BT_LOGE("Cannot memory-map address (size %zu) of file \"%s\" (%p) at offset %jd: %s",
				ds_file->mmap_len, ds_file->file->path->str,
				ds_file->file->fp, (intmax_t) ds_file->mmap_offset,
				strerror(errno));
		goto error;
	}

	goto end;
error:
	ds_file_munmap(ds_file);
	ret = BT_NOTIF_ITER_MEDIUM_STATUS_ERROR;
end:
	return ret;
}

static
enum bt_notif_iter_medium_status medop_request_bytes(
		size_t request_sz, uint8_t **buffer_addr,
		size_t *buffer_sz, void *data)
{
	enum bt_notif_iter_medium_status status =
		BT_NOTIF_ITER_MEDIUM_STATUS_OK;
	struct ctf_fs_ds_file *ds_file = data;

	if (request_sz == 0) {
		goto end;
	}

	/* Check if we have at least one memory-mapped byte left */
	if (remaining_mmap_bytes(ds_file) == 0) {
		/* Are we at the end of the file? */
		if (ds_file->mmap_offset >= ds_file->file->size) {
			BT_LOGD("Reached end of file \"%s\" (%p)",
				ds_file->file->path->str, ds_file->file->fp);
			status = BT_NOTIF_ITER_MEDIUM_STATUS_EOF;
			goto end;
		}

		status = ds_file_mmap_next(ds_file);
		switch (status) {
		case BT_NOTIF_ITER_MEDIUM_STATUS_OK:
			break;
		case BT_NOTIF_ITER_MEDIUM_STATUS_EOF:
			goto end;
		default:
			BT_LOGE("Cannot memory-map next region of file \"%s\" (%p)",
					ds_file->file->path->str,
					ds_file->file->fp);
			goto error;
		}
	}

	*buffer_sz = MIN(remaining_mmap_bytes(ds_file), request_sz);
	*buffer_addr = ((uint8_t *) ds_file->mmap_addr) + ds_file->request_offset;
	ds_file->request_offset += *buffer_sz;
	goto end;

error:
	status = BT_NOTIF_ITER_MEDIUM_STATUS_ERROR;

end:
	return status;
}

static
struct bt_private_stream *medop_get_stream(
		struct bt_private_stream_class *stream_class,
		uint64_t stream_id, void *data)
{
	struct ctf_fs_ds_file *ds_file = data;
	struct bt_private_stream_class *ds_file_stream_class;
	struct bt_private_stream *stream = NULL;

	ds_file_stream_class = bt_private_stream_get_private_class(
		ds_file->stream);
	bt_put(ds_file_stream_class);

	if (stream_class != ds_file_stream_class) {
		/*
		 * Not supported: two packets described by two different
		 * stream classes within the same data stream file.
		 */
		goto end;
	}

	stream = ds_file->stream;

end:
	return stream;
}

static
enum bt_notif_iter_medium_status medop_seek(
		enum bt_notif_iter_seek_whence whence, off_t offset,
		void *data)
{
	enum bt_notif_iter_medium_status ret =
			BT_NOTIF_ITER_MEDIUM_STATUS_OK;
	struct ctf_fs_ds_file *ds_file = data;
	off_t file_size = ds_file->file->size;

	if (whence != BT_NOTIF_ITER_SEEK_WHENCE_SET ||
		offset < 0 || offset > file_size) {
		BT_LOGE("Invalid medium seek request: whence=%d, offset=%jd, "
				"file-size=%jd", (int) whence, offset,
				file_size);
		ret = BT_NOTIF_ITER_MEDIUM_STATUS_INVAL;
		goto end;
	}

	/*
	 * Determine whether or not the destination is contained within the
	 * current mapping.
	 */
	if (ds_file->mmap_addr && (offset < ds_file->mmap_offset ||
			offset >= ds_file->mmap_offset + ds_file->mmap_len)) {
		int unmap_ret;
		off_t offset_in_mapping = offset % bt_common_get_page_size();

		BT_LOGD("Medium seek request cannot be accomodated by the current "
				"file mapping: offset=%jd, mmap-offset=%jd, "
				"mmap-len=%zu", offset, ds_file->mmap_offset,
				ds_file->mmap_len);
		unmap_ret = ds_file_munmap(ds_file);
		if (unmap_ret) {
			ret = BT_NOTIF_ITER_MEDIUM_STATUS_ERROR;
			goto end;
		}

		ds_file->mmap_offset = offset - offset_in_mapping;
		ds_file->request_offset = offset_in_mapping;
		ret = ds_file_mmap_next(ds_file);
		if (ret != BT_NOTIF_ITER_MEDIUM_STATUS_OK) {
			goto end;
		}
	} else {
		ds_file->request_offset = offset - ds_file->mmap_offset;
	}

	ds_file->end_reached = (offset == file_size);
end:
	return ret;
}

BT_HIDDEN
struct bt_notif_iter_medium_ops ctf_fs_ds_file_medops = {
	.request_bytes = medop_request_bytes,
	.get_stream = medop_get_stream,
	.seek = medop_seek,
};

static
struct ctf_fs_ds_index *ctf_fs_ds_index_create(size_t length)
{
	struct ctf_fs_ds_index *index = g_new0(struct ctf_fs_ds_index, 1);

	if (!index) {
		BT_LOGE_STR("Failed to allocate index");
		goto error;
	}

	index->entries = g_array_sized_new(FALSE, TRUE,
			sizeof(struct ctf_fs_ds_index_entry), length);
	if (!index->entries) {
		BT_LOGE("Failed to allocate %zu index entries.", length);
		goto error;
	}
	g_array_set_size(index->entries, length);
end:
	return index;
error:
	ctf_fs_ds_index_destroy(index);
	goto end;
}

/* Returns a new, zeroed, index entry. */
static
struct ctf_fs_ds_index_entry *ctf_fs_ds_index_add_new_entry(
		struct ctf_fs_ds_index *index)
{
	g_array_set_size(index->entries, index->entries->len + 1);
	return &g_array_index(index->entries, struct ctf_fs_ds_index_entry,
			index->entries->len - 1);
}

static
struct bt_clock_class *get_field_mapped_clock_class(
		struct bt_field *field)
{
	struct bt_field_type *field_type;
	struct bt_clock_class *clock_class = NULL;

	field_type = bt_field_get_type(field);
	if (!field_type) {
		goto end;
	}

	clock_class = bt_field_type_integer_get_mapped_clock_class(
			field_type);
	if (!clock_class) {
		goto end;
	}
end:
	bt_put(field_type);
	return clock_class;
}

static
int get_packet_bounds_from_packet_context(
		struct bt_field *packet_context,
		struct bt_clock_class **_timestamp_begin_cc,
		struct bt_field **_timestamp_begin,
		struct bt_clock_class **_timestamp_end_cc,
		struct bt_field **_timestamp_end)
{
	int ret = 0;
	struct bt_clock_class *timestamp_begin_cc = NULL;
	struct bt_clock_class *timestamp_end_cc = NULL;
	struct bt_field *timestamp_begin = NULL;
	struct bt_field *timestamp_end = NULL;

	timestamp_begin = bt_field_structure_get_field_by_name(
			packet_context, "timestamp_begin");
	if (!timestamp_begin) {
		BT_LOGD_STR("Cannot retrieve timestamp_begin field in packet context.");
		ret = -1;
		goto end;
	}

	timestamp_begin_cc = get_field_mapped_clock_class(timestamp_begin);
	if (!timestamp_begin_cc) {
		BT_LOGD_STR("Cannot retrieve the clock mapped to timestamp_begin.");
	}

	timestamp_end = bt_field_structure_get_field_by_name(
			packet_context, "timestamp_end");
	if (!timestamp_end) {
		BT_LOGD_STR("Cannot retrieve timestamp_end field in packet context.");
		ret = -1;
		goto end;
	}

	timestamp_end_cc = get_field_mapped_clock_class(timestamp_end);
	if (!timestamp_end_cc) {
		BT_LOGD_STR("Cannot retrieve the clock mapped to timestamp_end.");
	}

	if (_timestamp_begin_cc) {
		*_timestamp_begin_cc = bt_get(timestamp_begin_cc);
	}
	if (_timestamp_begin) {
		*_timestamp_begin = bt_get(timestamp_begin);
	}
	if (_timestamp_end_cc) {
		*_timestamp_end_cc = bt_get(timestamp_end_cc);
	}
	if (_timestamp_end) {
		*_timestamp_end = bt_get(timestamp_end);
	}
end:
	bt_put(timestamp_begin_cc);
	bt_put(timestamp_end_cc);
	bt_put(timestamp_begin);
	bt_put(timestamp_end);
	return ret;
}

static
int get_ds_file_packet_bounds_clock_classes(struct ctf_fs_ds_file *ds_file,
		struct bt_clock_class **timestamp_begin_cc,
		struct bt_clock_class **timestamp_end_cc)
{
	struct bt_field *packet_context = NULL;
	int ret = ctf_fs_ds_file_get_packet_header_context_fields(ds_file,
			NULL, &packet_context);

	if (ret || !packet_context) {
		BT_LOGD("Cannot retrieve packet context field of stream \'%s\'",
			ds_file->file->path->str);
		ret = -1;
		goto end;
	}

	ret = get_packet_bounds_from_packet_context(packet_context,
			timestamp_begin_cc, NULL,
			timestamp_end_cc, NULL);
end:
	bt_put(packet_context);
	return ret;
}

static
int convert_cycles_to_ns(struct bt_clock_class *clock_class,
		uint64_t cycles, int64_t *ns)
{
	int ret = 0;
	struct bt_clock_value *clock_value;

	assert(ns);
	clock_value = bt_clock_value_create(clock_class, cycles);
	if (!clock_value) {
		ret = -1;
		goto end;
	}

	ret = bt_clock_value_get_value_ns_from_epoch(clock_value, ns);
	if (ret) {
		goto end;
	}
end:
	bt_put(clock_value);
	return ret;
}

static
struct ctf_fs_ds_index *build_index_from_idx_file(
		struct ctf_fs_ds_file *ds_file)
{
	int ret;
	gchar *directory = NULL;
	gchar *basename = NULL;
	GString *index_basename = NULL;
	gchar *index_file_path = NULL;
	GMappedFile *mapped_file = NULL;
	gsize filesize;
	const char *mmap_begin = NULL, *file_pos = NULL;
	const struct ctf_packet_index_file_hdr *header = NULL;
	struct ctf_fs_ds_index *index = NULL;
	struct ctf_fs_ds_index_entry *index_entry = NULL;
	uint64_t total_packets_size = 0;
	size_t file_index_entry_size;
	size_t file_entry_count;
	size_t i;
	struct bt_clock_class *timestamp_begin_cc = NULL;
	struct bt_clock_class *timestamp_end_cc = NULL;

	BT_LOGD("Building index from .idx file of stream file %s",
			ds_file->file->path->str);

	ret = get_ds_file_packet_bounds_clock_classes(ds_file,
			&timestamp_begin_cc, &timestamp_end_cc);
	if (ret) {
		BT_LOGD_STR("Cannot get clock classes of \"timestamp_begin\" "
				"and \"timestamp_end\" fields");
		goto error;
	}

	/* Look for index file in relative path index/name.idx. */
	basename = g_path_get_basename(ds_file->file->path->str);
	if (!basename) {
		BT_LOGE("Cannot get the basename of datastream file %s",
				ds_file->file->path->str);
		goto error;
	}

	directory = g_path_get_dirname(ds_file->file->path->str);
	if (!directory) {
		BT_LOGE("Cannot get dirname of datastream file %s",
				ds_file->file->path->str);
		goto error;
	}

	index_basename = g_string_new(basename);
	if (!index_basename) {
		BT_LOGE_STR("Cannot allocate index file basename string");
		goto error;
	}

	g_string_append(index_basename, ".idx");
	index_file_path = g_build_filename(directory, "index",
			index_basename->str, NULL);
	mapped_file = g_mapped_file_new(index_file_path, FALSE, NULL);
	if (!mapped_file) {
		BT_LOGD("Cannot create new mapped file %s",
				index_file_path);
		goto error;
	}

	/*
	 * The g_mapped_file API limits us to 4GB files on 32-bit.
	 * Traces with such large indexes have never been seen in the wild,
	 * but this would need to be adjusted to support them.
	 */
	filesize = g_mapped_file_get_length(mapped_file);
	if (filesize < sizeof(*header)) {
		BT_LOGW("Invalid LTTng trace index file: "
			"file size (%zu bytes) < header size (%zu bytes)",
			filesize, sizeof(*header));
		goto error;
	}

	mmap_begin = g_mapped_file_get_contents(mapped_file);
	header = (struct ctf_packet_index_file_hdr *) mmap_begin;

	file_pos = g_mapped_file_get_contents(mapped_file) + sizeof(*header);
	if (be32toh(header->magic) != CTF_INDEX_MAGIC) {
		BT_LOGW_STR("Invalid LTTng trace index: \"magic\" field validation failed");
		goto error;
	}

	file_index_entry_size = be32toh(header->packet_index_len);
	file_entry_count = (filesize - sizeof(*header)) / file_index_entry_size;
	if ((filesize - sizeof(*header)) % file_index_entry_size) {
		BT_LOGW("Invalid LTTng trace index: the index's size after the header "
			"(%zu bytes) is not a multiple of the index entry size "
			"(%zu bytes)", (filesize - sizeof(*header)),
			sizeof(*header));
		goto error;
	}

	index = ctf_fs_ds_index_create(file_entry_count);
	if (!index) {
		goto error;
	}

	index_entry = (struct ctf_fs_ds_index_entry *) &g_array_index(
			index->entries, struct ctf_fs_ds_index_entry, 0);
	for (i = 0; i < file_entry_count; i++) {
		struct ctf_packet_index *file_index =
				(struct ctf_packet_index *) file_pos;
		uint64_t packet_size = be64toh(file_index->packet_size);

		if (packet_size % CHAR_BIT) {
			BT_LOGW("Invalid packet size encountered in LTTng trace index file");
			goto error;
		}

		/* Convert size in bits to bytes. */
		packet_size /= CHAR_BIT;
		index_entry->packet_size = packet_size;

		index_entry->offset = be64toh(file_index->offset);
		if (i != 0 && index_entry->offset < (index_entry - 1)->offset) {
			BT_LOGW("Invalid, non-monotonic, packet offset encountered in LTTng trace index file: "
				"previous offset=%" PRIu64 ", current offset=%" PRIu64,
				(index_entry - 1)->offset, index_entry->offset);
			goto error;
		}

		index_entry->timestamp_begin = be64toh(file_index->timestamp_begin);
		index_entry->timestamp_end = be64toh(file_index->timestamp_end);
		if (index_entry->timestamp_end < index_entry->timestamp_begin) {
			BT_LOGW("Invalid packet time bounds encountered in LTTng trace index file (begin > end): "
				"timestamp_begin=%" PRIu64 "timestamp_end=%" PRIu64,
				index_entry->timestamp_begin,
				index_entry->timestamp_end);
			goto error;
		}

		/* Convert the packet's bound to nanoseconds since Epoch. */
		ret = convert_cycles_to_ns(timestamp_begin_cc,
				index_entry->timestamp_begin,
				&index_entry->timestamp_begin_ns);
		if (ret) {
			BT_LOGD_STR("Failed to convert raw timestamp to nanoseconds since Epoch during index parsing");
			goto error;
		}
		ret = convert_cycles_to_ns(timestamp_end_cc,
				index_entry->timestamp_end,
				&index_entry->timestamp_end_ns);
		if (ret) {
			BT_LOGD_STR("Failed to convert raw timestamp to nanoseconds since Epoch during LTTng trace index parsing");
			goto error;
		}

		total_packets_size += packet_size;
		file_pos += file_index_entry_size;
		index_entry++;
	}

	/* Validate that the index addresses the complete stream. */
	if (ds_file->file->size != total_packets_size) {
		BT_LOGW("Invalid LTTng trace index file; indexed size != stream file size: "
			"file-size=%" PRIu64 ", total-packets-size=%" PRIu64,
			ds_file->file->size, total_packets_size);
		goto error;
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
	bt_put(timestamp_begin_cc);
	bt_put(timestamp_end_cc);
	return index;
error:
	ctf_fs_ds_index_destroy(index);
	index = NULL;
	goto end;
}

static
int init_index_entry(struct ctf_fs_ds_index_entry *entry,
		struct bt_field *packet_context, off_t packet_size,
		off_t packet_offset)
{
	int ret;
	struct bt_field *timestamp_begin = NULL;
	struct bt_field *timestamp_end = NULL;
	struct bt_clock_class *timestamp_begin_cc = NULL;
	struct bt_clock_class *timestamp_end_cc = NULL;

	ret = get_packet_bounds_from_packet_context(packet_context,
			&timestamp_begin_cc, &timestamp_begin,
			&timestamp_end_cc, &timestamp_end);
	if (ret || !timestamp_begin_cc || !timestamp_begin ||
			!timestamp_end_cc || ! timestamp_end) {
		BT_LOGD_STR("Failed to determine time bound fields of packet.");
		goto end;
	}

	assert(packet_offset >= 0);
	entry->offset = packet_offset;

	assert(packet_size >= 0);
	entry->packet_size = packet_size;

	ret = bt_field_unsigned_integer_get_value(timestamp_begin,
			&entry->timestamp_begin);
	if (ret) {
		goto end;
	}
	ret = bt_field_unsigned_integer_get_value(timestamp_end,
			&entry->timestamp_end);
	if (ret) {
		goto end;
	}

	/* Convert the packet's bound to nanoseconds since Epoch. */
	ret = convert_cycles_to_ns(timestamp_begin_cc,
				   entry->timestamp_begin,
				   &entry->timestamp_begin_ns);
	if (ret) {
		BT_LOGD_STR("Failed to convert raw timestamp to nanoseconds since Epoch.");
		goto end;
	}

	ret = convert_cycles_to_ns(timestamp_end_cc,
				   entry->timestamp_end,
				   &entry->timestamp_end_ns);
	if (ret) {
		BT_LOGD_STR("Failed to convert raw timestamp to nanoseconds since Epoch.");
		goto end;
	}
end:
	bt_put(timestamp_begin);
	bt_put(timestamp_begin_cc);
	bt_put(timestamp_end);
	bt_put(timestamp_end_cc);
	return ret;
}

static
struct ctf_fs_ds_index *build_index_from_stream_file(
		struct ctf_fs_ds_file *ds_file)
{
	int ret;
	struct ctf_fs_ds_index *index = NULL;
	enum bt_notif_iter_status iter_status;
	struct bt_field *packet_context = NULL;

	BT_LOGD("Indexing stream file %s", ds_file->file->path->str);

	index = ctf_fs_ds_index_create(0);
	if (!index) {
		goto error;
	}

	do {
		off_t current_packet_offset;
		off_t next_packet_offset;
		off_t current_packet_size, current_packet_size_bytes;
		struct ctf_fs_ds_index_entry *entry;

		iter_status = bt_notif_iter_get_packet_header_context_fields(
				ds_file->notif_iter, NULL, &packet_context);
		if (iter_status != BT_NOTIF_ITER_STATUS_OK) {
			if (iter_status == BT_NOTIF_ITER_STATUS_EOF) {
				break;
			}
			goto error;
		}
		current_packet_offset =
				bt_notif_iter_get_current_packet_offset(
				ds_file->notif_iter);
		if (current_packet_offset < 0) {
			BT_LOGE_STR("Cannot get the current packet's offset.");
			goto error;
		}

		current_packet_size = bt_notif_iter_get_current_packet_size(
				ds_file->notif_iter);
		if (current_packet_size < 0) {
			BT_LOGE("Cannot get packet size: packet-offset=%jd",
					current_packet_offset);
			goto error;
		}
		current_packet_size_bytes =
				((current_packet_size + 7) & ~7) / CHAR_BIT;

		if (current_packet_offset + current_packet_size_bytes >
				ds_file->file->size) {
			BT_LOGW("Invalid packet size reported in file: stream=\"%s\", "
					"packet-offset=%jd, packet-size-bytes=%jd, "
					"file-size=%jd",
					ds_file->file->path->str,
					current_packet_offset,
					current_packet_size_bytes,
					ds_file->file->size);
			goto error;
		}

		next_packet_offset = current_packet_offset +
				current_packet_size_bytes;
		BT_LOGD("Seeking to next packet: current-packet-offset=%jd, "
				"next-packet-offset=%jd", current_packet_offset,
				next_packet_offset);

		entry = ctf_fs_ds_index_add_new_entry(index);
		if (!entry) {
			BT_LOGE_STR("Failed to allocate a new index entry.");
			goto error;
		}

		ret = init_index_entry(entry, packet_context,
				current_packet_size_bytes,
				current_packet_offset);
		if (ret) {
			goto error;
		}

		iter_status = bt_notif_iter_seek(ds_file->notif_iter,
				next_packet_offset);
		BT_PUT(packet_context);
	} while (iter_status == BT_NOTIF_ITER_STATUS_OK);

	if (iter_status != BT_NOTIF_ITER_STATUS_EOF) {
		goto error;
	}
end:
	bt_put(packet_context);
	return index;
error:
	ctf_fs_ds_index_destroy(index);
	index = NULL;
	goto end;
}

BT_HIDDEN
struct ctf_fs_ds_file *ctf_fs_ds_file_create(
		struct ctf_fs_trace *ctf_fs_trace,
		struct bt_notif_iter *notif_iter,
		struct bt_private_stream *stream, const char *path)
{
	int ret;
	const size_t page_size = bt_common_get_page_size();
	struct ctf_fs_ds_file *ds_file = g_new0(struct ctf_fs_ds_file, 1);

	if (!ds_file) {
		goto error;
	}

	ds_file->file = ctf_fs_file_create();
	if (!ds_file->file) {
		goto error;
	}

	ds_file->stream = bt_get(stream);
	ds_file->cc_prio_map = bt_get(ctf_fs_trace->cc_prio_map);
	g_string_assign(ds_file->file->path, path);
	ret = ctf_fs_file_open(ds_file->file, "rb");
	if (ret) {
		goto error;
	}

	ds_file->notif_iter = notif_iter;
	bt_notif_iter_set_medops_data(ds_file->notif_iter, ds_file);
	if (!ds_file->notif_iter) {
		goto error;
	}

	ds_file->mmap_max_len = page_size * 2048;

	goto end;

error:
	/* Do not touch "borrowed" file. */
	ctf_fs_ds_file_destroy(ds_file);
	ds_file = NULL;

end:
	return ds_file;
}

BT_HIDDEN
struct ctf_fs_ds_index *ctf_fs_ds_file_build_index(
		struct ctf_fs_ds_file *ds_file)
{
	struct ctf_fs_ds_index *index;

	index = build_index_from_idx_file(ds_file);
	if (index) {
		goto end;
	}

	BT_LOGD("Failed to build index from .index file; "
		"falling back to stream indexing.");
	index = build_index_from_stream_file(ds_file);
end:
	return index;
}

BT_HIDDEN
void ctf_fs_ds_file_destroy(struct ctf_fs_ds_file *ds_file)
{
	if (!ds_file) {
		return;
	}

	bt_put(ds_file->cc_prio_map);
	bt_put(ds_file->stream);
	(void) ds_file_munmap(ds_file);

	if (ds_file->file) {
		ctf_fs_file_destroy(ds_file->file);
	}

	g_free(ds_file);
}

BT_HIDDEN
struct bt_notification_iterator_next_method_return ctf_fs_ds_file_next(
		struct ctf_fs_ds_file *ds_file)
{
	enum bt_notif_iter_status notif_iter_status;
	struct bt_notification_iterator_next_method_return ret = {
		.status = BT_NOTIFICATION_ITERATOR_STATUS_ERROR,
		.notification = NULL,
	};

	notif_iter_status = bt_notif_iter_get_next_notification(
		ds_file->notif_iter, ds_file->cc_prio_map, &ret.notification);

	switch (notif_iter_status) {
	case BT_NOTIF_ITER_STATUS_EOF:
		ret.status = BT_NOTIFICATION_ITERATOR_STATUS_END;
		break;
	case BT_NOTIF_ITER_STATUS_OK:
		ret.status = BT_NOTIFICATION_ITERATOR_STATUS_OK;
		break;
	case BT_NOTIF_ITER_STATUS_AGAIN:
		/*
		 * Should not make it this far as this is
		 * medium-specific; there is nothing for the user to do
		 * and it should have been handled upstream.
		 */
		abort();
	case BT_NOTIF_ITER_STATUS_INVAL:
	case BT_NOTIF_ITER_STATUS_ERROR:
	default:
		ret.status = BT_NOTIFICATION_ITERATOR_STATUS_ERROR;
		break;
	}

	return ret;
}

BT_HIDDEN
int ctf_fs_ds_file_get_packet_header_context_fields(
		struct ctf_fs_ds_file *ds_file,
		struct bt_field **packet_header_field,
		struct bt_field **packet_context_field)
{
	enum bt_notif_iter_status notif_iter_status;
	int ret = 0;

	assert(ds_file);
	notif_iter_status = bt_notif_iter_get_packet_header_context_fields(
		ds_file->notif_iter, packet_header_field, packet_context_field);
	switch (notif_iter_status) {
	case BT_NOTIF_ITER_STATUS_EOF:
	case BT_NOTIF_ITER_STATUS_OK:
		break;
	case BT_NOTIF_ITER_STATUS_AGAIN:
		abort();
	case BT_NOTIF_ITER_STATUS_INVAL:
	case BT_NOTIF_ITER_STATUS_ERROR:
	default:
		goto error;
		break;
	}

	goto end;

error:
	ret = -1;

	if (packet_header_field) {
		bt_put(*packet_header_field);
	}

	if (packet_context_field) {
		bt_put(*packet_context_field);
	}

end:
	return ret;
}

BT_HIDDEN
void ctf_fs_ds_index_destroy(struct ctf_fs_ds_index *index)
{
	if (!index) {
		return;
	}

	if (index->entries) {
		g_array_free(index->entries, TRUE);
	}
	g_free(index);
}
