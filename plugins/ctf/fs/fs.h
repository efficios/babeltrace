#ifndef BABELTRACE_PLUGIN_CTF_FS_H
#define BABELTRACE_PLUGIN_CTF_FS_H

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
#include <babeltrace/graph/component.h>
#include "data-stream.h"

#define CTF_FS_COMPONENT_DESCRIPTION \
	"Component used to read a CTF trace located on a file system."

BT_HIDDEN
extern bool ctf_fs_debug;

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
	/* A stream is assumed to be indexed. */
	struct index index;
	void *mmap_addr;
	/* Max length of chunk to mmap() when updating the current mapping. */
	size_t mmap_max_len;
	/* Length of the current mapping. */
	size_t mmap_len;
	/* Length of the current mapping which *exists* in the backing file. */
	size_t mmap_valid_len;
	/* Offset in the file where the current mapping starts. */
	off_t mmap_offset;
	/*
	 * Offset, in the current mapping, of the address to return on the next
	 * request.
	 */
	off_t request_offset;
	bool end_reached;
};

struct ctf_fs_component_options {
};

struct ctf_fs_port_data {
	GString *path;
};

struct ctf_fs_component {
	struct bt_private_component *priv_comp;
	GString *trace_path;
	FILE *error_fp;
	size_t page_size;
	struct ctf_fs_component_options options;
	struct ctf_fs_metadata *metadata;

	/* Array of struct ctf_fs_port_data *, owned by this */
	GPtrArray *port_data;
};

BT_HIDDEN
enum bt_component_status ctf_fs_init(struct bt_private_component *source,
		struct bt_value *params, void *init_method_data);

BT_HIDDEN
void ctf_fs_finalize(struct bt_private_component *component);

BT_HIDDEN
enum bt_notification_iterator_status ctf_fs_iterator_init(
		struct bt_private_notification_iterator *it,
		struct bt_private_port *port);

void ctf_fs_iterator_finalize(struct bt_private_notification_iterator *it);

struct bt_notification_iterator_next_return ctf_fs_iterator_next(
		struct bt_private_notification_iterator *iterator);

BT_HIDDEN
struct bt_value *ctf_fs_query(struct bt_component_class *comp_class,
		const char *object, struct bt_value *params);

#endif /* BABELTRACE_PLUGIN_CTF_FS_H */
