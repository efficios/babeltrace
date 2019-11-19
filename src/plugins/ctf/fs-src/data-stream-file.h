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
#include "common/macros.h"
#include <babeltrace2/babeltrace.h>

#include "../common/msg-iter/msg-iter.h"
#include "lttng-index.h"

struct ctf_fs_component;
struct ctf_fs_file;
struct ctf_fs_trace;
struct ctf_fs_ds_file;
struct ctf_fs_ds_file_group;
struct ctf_fs_ds_group_medops_data;

struct ctf_fs_ds_file_info {
	/* Owned by this. */
	GString *path;

	/* Guaranteed to be set, as opposed to the index. */
	int64_t begin_ns;
};

struct ctf_fs_metadata;

struct ctf_fs_ds_file {
	bt_logging_level log_level;

	/* Weak */
	bt_self_component *self_comp;

	/* Weak */
	bt_self_message_iterator *self_msg_iter;

	/* Weak */
	struct ctf_fs_metadata *metadata;

	/* Owned by this */
	struct ctf_fs_file *file;

	/* Owned by this */
	bt_stream *stream;

	void *mmap_addr;

	/*
	 * Max length of chunk to mmap() when updating the current mapping.
	 * This value must be page-aligned.
	 */
	size_t mmap_max_len;

	/* Length of the current mapping. Never exceeds the file's length. */
	size_t mmap_len;

	/* Offset in the file where the current mapping starts. */
	off_t mmap_offset_in_file;

	/*
	 * Offset, in the current mapping, of the address to return on the next
	 * request.
	 */
	off_t request_offset_in_mapping;
};

BT_HIDDEN
struct ctf_fs_ds_file *ctf_fs_ds_file_create(
		struct ctf_fs_trace *ctf_fs_trace,
		bt_self_message_iterator *self_msg_iter,
		bt_stream *stream, const char *path,
		bt_logging_level log_level);

BT_HIDDEN
void ctf_fs_ds_file_destroy(struct ctf_fs_ds_file *stream);

BT_HIDDEN
struct ctf_fs_ds_index *ctf_fs_ds_file_build_index(
		struct ctf_fs_ds_file *ds_file,
		struct ctf_fs_ds_file_info *ds_file_info,
		struct ctf_msg_iter *msg_iter);

BT_HIDDEN
struct ctf_fs_ds_index *ctf_fs_ds_index_create(bt_logging_level log_level,
		bt_self_component *self_comp);

BT_HIDDEN
void ctf_fs_ds_index_destroy(struct ctf_fs_ds_index *index);

/*
 * Medium operations to iterate on a single ctf_fs_ds_file.
 *
 * The data pointer when using this must be a pointer to the ctf_fs_ds_file.
 */
extern struct ctf_msg_iter_medium_ops ctf_fs_ds_file_medops;

/*
 * Medium operations to iterate on the packet of a ctf_fs_ds_group.
 *
 * The iteration is done based on the index of the group.
 *
 * The data pointer when using these medops must be a pointer to a ctf_fs_ds
 * group_medops_data structure.
 */
BT_HIDDEN
extern struct ctf_msg_iter_medium_ops ctf_fs_ds_group_medops;

BT_HIDDEN
enum ctf_msg_iter_medium_status ctf_fs_ds_group_medops_data_create(
		struct ctf_fs_ds_file_group *ds_file_group,
		bt_self_message_iterator *self_msg_iter,
		bt_logging_level log_level,
		struct ctf_fs_ds_group_medops_data **out);

BT_HIDDEN
void ctf_fs_ds_group_medops_data_reset(struct ctf_fs_ds_group_medops_data *data);

BT_HIDDEN
void ctf_fs_ds_group_medops_data_destroy(
		struct ctf_fs_ds_group_medops_data *data);

#endif /* CTF_FS_DS_FILE_H */
