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

#include <stdbool.h>
#include <babeltrace/babeltrace-internal.h>
#include <babeltrace/graph/component.h>
#include <babeltrace/graph/clock-class-priority-map.h>
#include "data-stream.h"

BT_HIDDEN
extern bool ctf_fs_debug;

struct ctf_fs_file {
	/* Weak */
	struct ctf_fs_component *ctf_fs;

	/* Owned by this */
	GString *path;

	/* Owned by this */
	FILE *fp;

	off_t size;
};

struct ctf_fs_metadata {
	/* Owned by this */
	struct bt_ctf_trace *trace;

	/* Owned by this */
	char *text;

	uint8_t uuid[16];
	bool is_uuid_set;
	int bo;
};

struct ctf_fs_stream {
	/* Owned by this */
	struct ctf_fs_file *file;

	/* Owned by this */
	struct bt_ctf_stream *stream;

	/* Owned by this */
	struct bt_clock_class_priority_map *cc_prio_map;

	/* Owned by this */
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
	uint64_t clock_offset;
	uint64_t clock_offset_ns;
};

struct ctf_fs_component {
	/* Weak */
	struct bt_private_component *priv_comp;

	/* Array of struct ctf_fs_port_data *, owned by this */
	GPtrArray *port_data;

	/* Array of struct ctf_fs_trace *, owned by this */
	GPtrArray *traces;

	struct ctf_fs_component_options options;
	FILE *error_fp;
	size_t page_size;
};

struct ctf_fs_trace {
	/* Weak */
	struct ctf_fs_component *ctf_fs;

	/* Owned by this */
	struct ctf_fs_metadata *metadata;

	/* Owned by this */
	struct bt_clock_class_priority_map *cc_prio_map;

	/* Owned by this */
	GString *path;

	/* Owned by this */
	GString *name;
};

struct ctf_fs_port_data {
	/* Owned by this */
	GString *path;

	/* Weak */
	struct ctf_fs_trace *ctf_fs_trace;
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
