#ifndef BABELTRACE_PLUGIN_CTF_FS_INTERNAL_H
#define BABELTRACE_PLUGIN_CTF_FS_INTERNAL_H

/*
 * BabelTrace - CTF on File System Component
 *
 * Copyright 2016 Jérémie Galarneau <jeremie.galarneau@efficios.com>
 * Copyright 2016 Philippe Proulx <pproulx@efficios.com>
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

#include <babeltrace/babeltrace-internal.h>
#include <babeltrace/plugin/component.h>

#define CTF_FS_COMPONENT_NAME "fs"
#define CTF_FS_COMPONENT_DESCRIPTION \
	"Component used to read a CTF trace located on a file system."

static bool ctf_fs_debug;

struct ctf_fs_file {
	struct ctf_fs_component *ctf_fs;
	GString *path;
	FILE *fp;
	off_t size;
};

struct ctf_fs_metadata {
	struct bt_ctf_trace *trace;
	uint8_t uuid[16];
	bool is_uuid_set;
	int bo;
	char *text;
};

struct ctf_fs_stream {
	struct ctf_fs_file *file;
	struct bt_ctf_stream *stream;
	struct bt_ctf_notif_iter *notif_iter;
	void *mmap_addr;
	size_t mmap_len;
	off_t mmap_offset;
	off_t request_offset;
};

struct ctf_fs_data_stream {
	GPtrArray *streams;
};

struct ctf_fs_iterator {
	int dummy;
};

struct ctf_fs_component_options {
	bool opt_dummy : 1;
};

struct ctf_fs_component {
	GString *trace_path;
	FILE *error_fp;
	size_t page_size;
	struct bt_notification *last_notif;
	struct ctf_fs_metadata metadata;
	struct ctf_fs_data_stream data_stream;
	struct ctf_fs_component_options options;
};

BT_HIDDEN
enum bt_component_status ctf_fs_init(struct bt_component *source,
		struct bt_value *params);

#endif /* BABELTRACE_PLUGIN_CTF_FS_INTERNAL_H */
