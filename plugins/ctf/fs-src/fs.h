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
#include "data-stream-file.h"
#include "metadata.h"

BT_HIDDEN
extern bool ctf_fs_debug;

struct ctf_fs_file {
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

struct ctf_fs_component {
	/* Weak, guaranteed to exist */
	struct bt_private_component *priv_comp;

	/* Array of struct ctf_fs_port_data *, owned by this */
	GPtrArray *port_data;

	/* Array of struct ctf_fs_trace *, owned by this */
	GPtrArray *traces;

	struct ctf_fs_metadata_config metadata_config;
};

struct ctf_fs_trace {
	/* Owned by this */
	struct ctf_fs_metadata *metadata;

	/* Owned by this */
	struct bt_clock_class_priority_map *cc_prio_map;

	/* Array of struct ctf_fs_ds_file_group *, owned by this */
	GPtrArray *ds_file_groups;

	/* Owned by this */
	GString *path;

	/* Owned by this */
	GString *name;
};

struct ctf_fs_ds_file_group {
	/*
	 * Array of struct ctf_fs_ds_file_info, owned by this.
	 *
	 * This is an _ordered_ array of data stream file infos which
	 * belong to this group (a single stream instance).
	 *
	 * You can call ctf_fs_ds_file_create() with one of those paths
	 * and the CTF IR stream below.
	 */
	GPtrArray *ds_file_infos;

	/* Owned by this */
	struct bt_ctf_stream_class *stream_class;

	/* Owned by this */
	struct bt_ctf_stream *stream;

	/* Stream (instance) ID; -1ULL means none */
	uint64_t stream_id;

	/* Weak, belongs to component */
	struct ctf_fs_trace *ctf_fs_trace;
};

struct ctf_fs_port_data {
	/* Weak, belongs to ctf_fs_trace */
	struct ctf_fs_ds_file_group *ds_file_group;
};

struct ctf_fs_notif_iter_data {
	/* Weak, belongs to ctf_fs_trace */
	struct ctf_fs_ds_file_group *ds_file_group;

	/* Owned by this */
	struct ctf_fs_ds_file *ds_file;

	/* Which file the iterator is _currently_ operating on */
	size_t ds_file_info_index;

	/* Owned by this */
	struct bt_ctf_notif_iter *notif_iter;
};

BT_HIDDEN
enum bt_component_status ctf_fs_init(struct bt_private_component *source,
		struct bt_value *params, void *init_method_data);

BT_HIDDEN
void ctf_fs_finalize(struct bt_private_component *component);

BT_HIDDEN
struct bt_value *ctf_fs_query(struct bt_component_class *comp_class,
		const char *object, struct bt_value *params);

BT_HIDDEN
struct ctf_fs_trace *ctf_fs_trace_create(const char *path, const char *name,
		struct ctf_fs_metadata_config *config);

BT_HIDDEN
void ctf_fs_trace_destroy(struct ctf_fs_trace *trace);

BT_HIDDEN
int ctf_fs_find_traces(GList **trace_paths, const char *start_path);

BT_HIDDEN
GList *ctf_fs_create_trace_names(GList *trace_paths, const char *base_path);

BT_HIDDEN
enum bt_notification_iterator_status ctf_fs_iterator_init(
		struct bt_private_notification_iterator *it,
		struct bt_private_port *port);
BT_HIDDEN
void ctf_fs_iterator_finalize(struct bt_private_notification_iterator *it);

BT_HIDDEN
struct bt_notification_iterator_next_return ctf_fs_iterator_next(
		struct bt_private_notification_iterator *iterator);

#endif /* BABELTRACE_PLUGIN_CTF_FS_H */
