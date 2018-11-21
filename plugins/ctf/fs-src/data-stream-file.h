#ifndef CTF_FS_DS_FILE_H
#define CTF_FS_DS_FILE_H

/*
 * Copyright 2016 - Philippe Proulx <pproulx@efficios.com>
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
#include <stdbool.h>
#include <glib.h>
#include <babeltrace/babeltrace-internal.h>
#include <babeltrace/babeltrace.h>

#include "../common/notif-iter/notif-iter.h"
#include "lttng-index.h"

struct ctf_fs_component;
struct ctf_fs_file;
struct ctf_fs_trace;
struct ctf_fs_ds_file;

struct ctf_fs_ds_index_entry {
	/* Position, in bytes, of the packet from the beginning of the file. */
	uint64_t offset;

	/* Size of the packet, in bytes. */
	uint64_t packet_size;

	/*
	 * Extracted from the packet context, relative to the respective fields'
	 * mapped clock classes (in cycles).
	 */
	uint64_t timestamp_begin, timestamp_end;

	/*
	 * Converted from the packet context, relative to the trace's EPOCH
	 * (in ns since EPOCH).
	 */
	int64_t timestamp_begin_ns, timestamp_end_ns;
};

struct ctf_fs_ds_index {
	/* Array of struct ctf_fs_fd_index_entry. */
	GArray *entries;
};

struct ctf_fs_ds_file_info {
	/*
	 * Owned by this. May be NULL.
	 *
	 * A stream cannot be assumed to be indexed as the indexing might have
	 * been skipped. Moreover, the index's fields may not all be available
	 * depending on the producer (e.g. timestamp_begin/end are not
	 * mandatory).
	 *
	 * FIXME In such cases (missing fields), the indexing is aborted as
	 * no the index entries don't have a concept of fields being present
	 * or not.
	 */
	struct ctf_fs_ds_index *index;

	/* Owned by this. */
	GString *path;

	/* Guaranteed to be set, as opposed to the index. */
	int64_t begin_ns;
};

struct ctf_fs_metadata;

struct ctf_fs_ds_file {
	/* Weak */
	struct ctf_fs_metadata *metadata;

	/* Weak */
	struct bt_self_notification_iterator *pc_notif_iter;

	/* Owned by this */
	struct ctf_fs_file *file;

	/* Owned by this */
	struct bt_private_stream *stream;

	/* Weak */
	struct bt_notif_iter *notif_iter;

	void *mmap_addr;

	/*
	 * Max length of chunk to mmap() when updating the current mapping.
	 * This value must be page-aligned.
	 */
	size_t mmap_max_len;

	/* Length of the current mapping. Never exceeds the file's length. */
	size_t mmap_len;

	/* Offset in the file where the current mapping starts. */
	off_t mmap_offset;

	/*
	 * Offset, in the current mapping, of the address to return on the next
	 * request.
	 */
	off_t request_offset;

	bool end_reached;
};

BT_HIDDEN
struct ctf_fs_ds_file *ctf_fs_ds_file_create(
		struct ctf_fs_trace *ctf_fs_trace,
		struct bt_self_notification_iterator *pc_notif_iter,
		struct bt_notif_iter *notif_iter,
		struct bt_private_stream *stream, const char *path);

BT_HIDDEN
int ctf_fs_ds_file_borrow_packet_header_context_fields(
		struct ctf_fs_ds_file *ds_file,
		struct bt_private_field **packet_header_field,
		struct bt_private_field **packet_context_field);

BT_HIDDEN
void ctf_fs_ds_file_destroy(struct ctf_fs_ds_file *stream);

BT_HIDDEN
enum bt_notification_iterator_status ctf_fs_ds_file_next(
		struct ctf_fs_ds_file *ds_file,
		struct bt_private_notification **notif);

BT_HIDDEN
struct ctf_fs_ds_index *ctf_fs_ds_file_build_index(
		struct ctf_fs_ds_file *ds_file);

BT_HIDDEN
void ctf_fs_ds_index_destroy(struct ctf_fs_ds_index *index);

extern struct bt_notif_iter_medium_ops ctf_fs_ds_file_medops;

#endif /* CTF_FS_DS_FILE_H */
