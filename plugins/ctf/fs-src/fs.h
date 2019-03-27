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
#include <babeltrace/babeltrace.h>
#include "data-stream-file.h"
#include "metadata.h"
#include "../common/metadata/decoder.h"

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
	struct ctf_metadata_decoder *decoder;

	/* Owned by this */
	bt_trace_class *trace_class;

	/* Weak (owned by `decoder` above) */
	struct ctf_trace_class *tc;

	/* Owned by this */

	/* Owned by this */
	char *text;

	uint8_t uuid[16];
	bool is_uuid_set;
	int bo;
};

struct ctf_fs_component {
	/* Weak, guaranteed to exist */
	bt_self_component_source *self_comp;

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
	bt_trace *trace;

	/* Array of struct ctf_fs_ds_file_group *, owned by this */
	GPtrArray *ds_file_groups;

	/* Owned by this */
	GString *path;

	/* Owned by this */
	GString *name;

	/* Next automatic stream ID when not provided by packet header */
	uint64_t next_stream_id;
};

struct ctf_fs_ds_file_group {
	/*
	 * Array of struct ctf_fs_ds_file_info, owned by this.
	 *
	 * This is an _ordered_ array of data stream file infos which
	 * belong to this group (a single stream instance).
	 *
	 * You can call ctf_fs_ds_file_create() with one of those paths
	 * and the trace IR stream below.
	 */
	GPtrArray *ds_file_infos;

	/* Owned by this */
	struct ctf_stream_class *sc;

	/* Owned by this */
	bt_stream *stream;

	/* Stream (instance) ID; -1ULL means none */
	uint64_t stream_id;

	/* Weak, belongs to component */
	struct ctf_fs_trace *ctf_fs_trace;
};

struct ctf_fs_port_data {
	/* Weak, belongs to ctf_fs_trace */
	struct ctf_fs_ds_file_group *ds_file_group;

	/* Weak */
	struct ctf_fs_component *ctf_fs;
};

struct ctf_fs_msg_iter_data {
	/* Weak */
	bt_self_message_iterator *pc_msg_iter;

	/* Weak, belongs to ctf_fs_trace */
	struct ctf_fs_ds_file_group *ds_file_group;

	/* Owned by this */
	struct ctf_fs_ds_file *ds_file;

	/* Which file the iterator is _currently_ operating on */
	size_t ds_file_info_index;

	/* Owned by this */
	struct bt_msg_iter *msg_iter;
};

BT_HIDDEN
bt_self_component_status ctf_fs_init(
		bt_self_component_source *source,
		const bt_value *params, void *init_method_data);

BT_HIDDEN
void ctf_fs_finalize(bt_self_component_source *component);

BT_HIDDEN
bt_query_status ctf_fs_query(
		bt_self_component_class_source *comp_class,
		const bt_query_executor *query_exec,
		const char *object, const bt_value *params,
		const bt_value **result);

BT_HIDDEN
struct ctf_fs_trace *ctf_fs_trace_create(bt_self_component_source *self_comp,
		const char *path, const char *name,
		struct ctf_fs_metadata_config *config);

BT_HIDDEN
void ctf_fs_trace_destroy(struct ctf_fs_trace *trace);

BT_HIDDEN
int ctf_fs_find_traces(GList **trace_paths, const char *start_path);

BT_HIDDEN
GList *ctf_fs_create_trace_names(GList *trace_paths, const char *base_path);

BT_HIDDEN
bt_self_message_iterator_status ctf_fs_iterator_init(
		bt_self_message_iterator *self_msg_iter,
		bt_self_component_source *self_comp,
		bt_self_component_port_output *self_port);

BT_HIDDEN
void ctf_fs_iterator_finalize(bt_self_message_iterator *it);

BT_HIDDEN
bt_self_message_iterator_status ctf_fs_iterator_next(
		bt_self_message_iterator *iterator,
		bt_message_array_const msgs, uint64_t capacity,
		uint64_t *count);

BT_HIDDEN
bt_self_message_iterator_status ctf_fs_iterator_seek_beginning(
		bt_self_message_iterator *message_iterator);

#endif /* BABELTRACE_PLUGIN_CTF_FS_H */
