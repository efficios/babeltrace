#ifndef CTF_FS_DATA_STREAM_H
#define CTF_FS_DATA_STREAM_H

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
#include <glib.h>
#include <babeltrace/babeltrace-internal.h>
#include <babeltrace/ctf-ir/trace.h>

#include "../common/notif-iter/notif-iter.h"
#include "lttng-index.h"

struct ctf_fs_component;
struct ctf_fs_file;
struct ctf_fs_stream;

struct index_entry {
	uint64_t offset; /* in bytes. */
	uint64_t packet_size; /* in bytes. */
	/* relative to the packet context field's mapped clock. */
	uint64_t timestamp_begin, timestamp_end;
};

struct index {
	GArray *entries; /* Array of struct index_entry. */
};

BT_HIDDEN
struct ctf_fs_stream *ctf_fs_stream_create(
		struct ctf_fs_component *ctf_fs, const char *path);

BT_HIDDEN
void ctf_fs_stream_destroy(struct ctf_fs_stream *stream);

BT_HIDDEN
int ctf_fs_data_stream_open_streams(struct ctf_fs_component *ctf_fs);

BT_HIDDEN
struct bt_notification_iterator_next_return ctf_fs_stream_next(
		struct ctf_fs_stream *stream);

#endif /* CTF_FS_DATA_STREAM_H */
