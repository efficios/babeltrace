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
#include "common/macros.h"
#include <babeltrace2/babeltrace.h>
#include "data-stream-file.h"
#include "metadata.h"
#include "../common/metadata/decoder.h"

BT_HIDDEN
extern bool ctf_fs_debug;

struct ctf_fs_file {
	bt_logging_level log_level;

	/* Weak */
	bt_self_component *self_comp;

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
	char *text;

	int bo;
};

struct ctf_fs_component {
	bt_logging_level log_level;

	/* Array of struct ctf_fs_port_data *, owned by this */
	GPtrArray *port_data;

	/* Owned by this */
	struct ctf_fs_trace *trace;

	struct ctf_fs_metadata_config metadata_config;
};

struct ctf_fs_trace {
	bt_logging_level log_level;

	/*
	 * Weak. These are mostly used to generate log messages or to append
	 * error causes. They are mutually exclusive, only one of them must be
	 * set.
	 */
	bt_self_component *self_comp;
	bt_self_component_class *self_comp_class;

	/* Owned by this */
	struct ctf_fs_metadata *metadata;

	/* Owned by this */
	bt_trace *trace;

	/* Array of struct ctf_fs_ds_file_group *, owned by this */
	GPtrArray *ds_file_groups;

	/* Owned by this */
	GString *path;

	/* Next automatic stream ID when not provided by packet header */
	uint64_t next_stream_id;
};

struct ctf_fs_ds_index_entry {
	/* Weak, belongs to ctf_fs_ds_file_info. */
	const char *path;

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

	/*
	 * Packet sequence number, or UINT64_MAX if not present in the index.
	 */
	uint64_t packet_seq_num;
};

struct ctf_fs_ds_index {
	/* Array of pointer to struct ctf_fs_ds_index_entry. */
	GPtrArray *entries;
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

	/*
	 * Owned by this.
	 */
	struct ctf_fs_ds_index *index;
};

struct ctf_fs_port_data {
	/* Weak, belongs to ctf_fs_trace */
	struct ctf_fs_ds_file_group *ds_file_group;

	/* Weak */
	struct ctf_fs_component *ctf_fs;
};

struct ctf_fs_msg_iter_data {
	bt_logging_level log_level;

	/* Weak */
	bt_self_component *self_comp;

	/* Weak */
	bt_self_message_iterator *self_msg_iter;

	/* Weak, belongs to ctf_fs_trace */
	struct ctf_fs_ds_file_group *ds_file_group;

	/* Owned by this */
	struct ctf_msg_iter *msg_iter;

	/*
	 * Saved error.  If we hit an error in the _next method, but have some
	 * messages ready to return, we save the error here and return it on
	 * the next _next call.
	 */
	bt_message_iterator_class_next_method_status next_saved_status;
	const struct bt_error *next_saved_error;

	struct ctf_fs_ds_group_medops_data *msg_iter_medops_data;
};

BT_HIDDEN
bt_component_class_initialize_method_status ctf_fs_init(
		bt_self_component_source *source,
		bt_self_component_source_configuration *config,
		const bt_value *params, void *init_method_data);

BT_HIDDEN
void ctf_fs_finalize(bt_self_component_source *component);

BT_HIDDEN
bt_component_class_query_method_status ctf_fs_query(
		bt_self_component_class_source *comp_class,
		bt_private_query_executor *priv_query_exec,
		const char *object, const bt_value *params,
		void *method_data, const bt_value **result);

BT_HIDDEN
bt_message_iterator_class_initialize_method_status ctf_fs_iterator_init(
		bt_self_message_iterator *self_msg_iter,
		bt_self_message_iterator_configuration *config,
		bt_self_component_port_output *self_port);

BT_HIDDEN
void ctf_fs_iterator_finalize(bt_self_message_iterator *it);

BT_HIDDEN
bt_message_iterator_class_next_method_status ctf_fs_iterator_next(
		bt_self_message_iterator *iterator,
		bt_message_array_const msgs, uint64_t capacity,
		uint64_t *count);

BT_HIDDEN
bt_message_iterator_class_seek_beginning_method_status ctf_fs_iterator_seek_beginning(
		bt_self_message_iterator *message_iterator);

/* Create and initialize a new, empty ctf_fs_component. */

BT_HIDDEN
struct ctf_fs_component *ctf_fs_component_create(bt_logging_level log_level,
		bt_self_component *self_comp);

/*
 * Create one `struct ctf_fs_trace` from one trace, or multiple traces sharing
 * the same UUID.
 *
 * `paths_value` must be an array of strings,
 *
 * The created `struct ctf_fs_trace` is assigned to `ctf_fs->trace`.
 *
 * `self_comp` and `self_comp_class` are used for logging, only one of them
 * should be set.
 */

BT_HIDDEN
int ctf_fs_component_create_ctf_fs_trace(
		struct ctf_fs_component *ctf_fs,
		const bt_value *paths_value,
		const bt_value *trace_name_value,
		bt_self_component *self_comp,
		bt_self_component_class *self_comp_class);

/* Free `ctf_fs` and everything it owns. */

BT_HIDDEN
void ctf_fs_destroy(struct ctf_fs_component *ctf_fs);

/*
 * Read and validate parameters taken by the src.ctf.fs plugin.
 *
 *  - The mandatory `paths` parameter is returned in `*paths`.
 *  - The optional `clock-class-offset-s` and `clock-class-offset-ns`, if
 *    present, are recorded in the `ctf_fs` structure.
 *  - The optional `trace-name` parameter is returned in `*trace_name` if
 *    present, else `*trace_name` is set to NULL.
 *
 * `self_comp` and `self_comp_class` are used for logging, only one of them
 * should be set.
 *
 * Return true on success, false if any parameter didn't pass validation.
 */

BT_HIDDEN
bool read_src_fs_parameters(const bt_value *params,
		const bt_value **paths,
		const bt_value **trace_name,
		struct ctf_fs_component *ctf_fs,
		bt_self_component *self_comp,
		bt_self_component_class *self_comp_class);

/*
 * Generate the port name to be used for a given data stream file group.
 *
 * The result must be freed using g_free by the caller.
 */

BT_HIDDEN
gchar *ctf_fs_make_port_name(struct ctf_fs_ds_file_group *ds_file_group);

#endif /* BABELTRACE_PLUGIN_CTF_FS_H */
