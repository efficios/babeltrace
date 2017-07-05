/*
 * fs.c
 *
 * Babeltrace CTF file system Reader Component
 *
 * Copyright 2015-2017 Philippe Proulx <pproulx@efficios.com>
 * Copyright 2016 Jérémie Galarneau <jeremie.galarneau@efficios.com>
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

#include <babeltrace/common-internal.h>
#include <babeltrace/ctf-ir/packet.h>
#include <babeltrace/ctf-ir/clock-class.h>
#include <babeltrace/ctf-ir/stream.h>
#include <babeltrace/ctf-ir/fields.h>
#include <babeltrace/graph/private-port.h>
#include <babeltrace/graph/private-component.h>
#include <babeltrace/graph/private-component-source.h>
#include <babeltrace/graph/private-notification-iterator.h>
#include <babeltrace/graph/component.h>
#include <babeltrace/graph/notification-iterator.h>
#include <babeltrace/graph/clock-class-priority-map.h>
#include <plugins-common.h>
#include <glib.h>
#include <assert.h>
#include <inttypes.h>
#include <stdbool.h>
#include "fs.h"
#include "metadata.h"
#include "data-stream-file.h"
#include "file.h"
#include "../common/metadata/decoder.h"
#include "../common/notif-iter/notif-iter.h"
#include "query.h"

#define BT_LOG_TAG "PLUGIN-CTF-FS-SRC"
#include "logging.h"

static
int notif_iter_data_set_current_ds_file(struct ctf_fs_notif_iter_data *notif_iter_data)
{
	struct ctf_fs_ds_file_info *ds_file_info;
	int ret = 0;

	assert(notif_iter_data->ds_file_info_index <
		notif_iter_data->ds_file_group->ds_file_infos->len);
	ds_file_info = g_ptr_array_index(
		notif_iter_data->ds_file_group->ds_file_infos,
		notif_iter_data->ds_file_info_index);

	ctf_fs_ds_file_destroy(notif_iter_data->ds_file);
	notif_iter_data->ds_file = ctf_fs_ds_file_create(
		notif_iter_data->ds_file_group->ctf_fs_trace,
		notif_iter_data->notif_iter,
		notif_iter_data->ds_file_group->stream,
		ds_file_info->path->str);
	if (!notif_iter_data->ds_file) {
		ret = -1;
	}

	return ret;
}

static
void ctf_fs_notif_iter_data_destroy(
		struct ctf_fs_notif_iter_data *notif_iter_data)
{
	if (!notif_iter_data) {
		return;
	}

	ctf_fs_ds_file_destroy(notif_iter_data->ds_file);

	if (notif_iter_data->notif_iter) {
		bt_ctf_notif_iter_destroy(notif_iter_data->notif_iter);
	}

	g_free(notif_iter_data);
}

struct bt_notification_iterator_next_return ctf_fs_iterator_next(
		struct bt_private_notification_iterator *iterator)
{
	struct bt_notification_iterator_next_return next_ret;
	struct ctf_fs_notif_iter_data *notif_iter_data =
		bt_private_notification_iterator_get_user_data(iterator);
	int ret;

	assert(notif_iter_data->ds_file);
	next_ret = ctf_fs_ds_file_next(notif_iter_data->ds_file);
	if (next_ret.status == BT_NOTIFICATION_ITERATOR_STATUS_END) {
		assert(!next_ret.notification);
		notif_iter_data->ds_file_info_index++;

		if (notif_iter_data->ds_file_info_index ==
				notif_iter_data->ds_file_group->ds_file_infos->len) {
			/*
			 * No more stream files to read: we reached the
			 * real end.
			 */
			goto end;
		}

		/*
		 * Open and start reading the next stream file within
		 * our stream file group.
		 */
		ret = notif_iter_data_set_current_ds_file(notif_iter_data);
		if (ret) {
			next_ret.status = BT_NOTIFICATION_ITERATOR_STATUS_ERROR;
			goto end;
		}

		next_ret = ctf_fs_ds_file_next(notif_iter_data->ds_file);

		/*
		 * We should not get BT_NOTIFICATION_ITERATOR_STATUS_END
		 * with a brand new stream file because empty stream
		 * files are not even part of stream file groups, which
		 * means we're sure to get at least one pair of "packet
		 * begin" and "packet end" notifications in the case of
		 * a single, empty packet.
		 */
		assert(next_ret.status != BT_NOTIFICATION_ITERATOR_STATUS_END);
	}

end:
	return next_ret;
}

void ctf_fs_iterator_finalize(struct bt_private_notification_iterator *it)
{
	void *notif_iter_data =
		bt_private_notification_iterator_get_user_data(it);

	ctf_fs_notif_iter_data_destroy(notif_iter_data);
}

enum bt_notification_iterator_status ctf_fs_iterator_init(
		struct bt_private_notification_iterator *it,
		struct bt_private_port *port)
{
	struct ctf_fs_port_data *port_data;
	struct ctf_fs_notif_iter_data *notif_iter_data = NULL;
	enum bt_notification_iterator_status ret =
		BT_NOTIFICATION_ITERATOR_STATUS_OK;
	int iret;

	port_data = bt_private_port_get_user_data(port);
	if (!port_data) {
		ret = BT_NOTIFICATION_ITERATOR_STATUS_INVALID;
		goto error;
	}

	notif_iter_data = g_new0(struct ctf_fs_notif_iter_data, 1);
	if (!notif_iter_data) {
		ret = BT_NOTIFICATION_ITERATOR_STATUS_NOMEM;
		goto error;
	}

	notif_iter_data->notif_iter = bt_ctf_notif_iter_create(
		port_data->ds_file_group->ctf_fs_trace->metadata->trace,
		bt_common_get_page_size() * 8,
		ctf_fs_ds_file_medops, NULL);
	if (!notif_iter_data->notif_iter) {
		BT_LOGE_STR("Cannot create a CTF notification iterator.");
		ret = BT_NOTIFICATION_ITERATOR_STATUS_NOMEM;
		goto error;
	}

	notif_iter_data->ds_file_group = port_data->ds_file_group;
	iret = notif_iter_data_set_current_ds_file(notif_iter_data);
	if (iret) {
		ret = BT_NOTIFICATION_ITERATOR_STATUS_ERROR;
		goto error;
	}

	ret = bt_private_notification_iterator_set_user_data(it, notif_iter_data);
	if (ret) {
		goto error;
	}

	notif_iter_data = NULL;
	goto end;

error:
	(void) bt_private_notification_iterator_set_user_data(it, NULL);

	if (ret == BT_NOTIFICATION_ITERATOR_STATUS_OK) {
		ret = BT_NOTIFICATION_ITERATOR_STATUS_ERROR;
	}

end:
	ctf_fs_notif_iter_data_destroy(notif_iter_data);
	return ret;
}

static
void ctf_fs_destroy(struct ctf_fs_component *ctf_fs)
{
	if (!ctf_fs) {
		return;
	}

	if (ctf_fs->traces) {
		g_ptr_array_free(ctf_fs->traces, TRUE);
	}

	if (ctf_fs->port_data) {
		g_ptr_array_free(ctf_fs->port_data, TRUE);
	}

	g_free(ctf_fs);
}

BT_HIDDEN
void ctf_fs_trace_destroy(struct ctf_fs_trace *ctf_fs_trace)
{
	if (!ctf_fs_trace) {
		return;
	}

	if (ctf_fs_trace->ds_file_groups) {
		g_ptr_array_free(ctf_fs_trace->ds_file_groups, TRUE);
	}

	if (ctf_fs_trace->path) {
		g_string_free(ctf_fs_trace->path, TRUE);
	}

	if (ctf_fs_trace->name) {
		g_string_free(ctf_fs_trace->name, TRUE);
	}

	if (ctf_fs_trace->metadata) {
		ctf_fs_metadata_fini(ctf_fs_trace->metadata);
		g_free(ctf_fs_trace->metadata);
	}

	bt_put(ctf_fs_trace->cc_prio_map);
	g_free(ctf_fs_trace);
}

static
void ctf_fs_trace_destroy_notifier(void *data)
{
	struct ctf_fs_trace *trace = data;
	ctf_fs_trace_destroy(trace);
}

void ctf_fs_finalize(struct bt_private_component *component)
{
	void *data = bt_private_component_get_user_data(component);

	ctf_fs_destroy(data);
}

static
void port_data_destroy(void *data) {
	struct ctf_fs_port_data *port_data = data;

	if (!port_data) {
		return;
	}

	g_free(port_data);
}

static
GString *get_stream_instance_unique_name(
		struct ctf_fs_ds_file_group *ds_file_group)
{
	GString *name;
	struct ctf_fs_ds_file_info *ds_file_info;

	name = g_string_new(NULL);
	if (!name) {
		goto end;
	}

	/*
	 * If there's more than one stream file in the stream file
	 * group, the first (earliest) stream file's path is used as
	 * the stream's unique name.
	 */
	assert(ds_file_group->ds_file_infos->len > 0);
	ds_file_info = g_ptr_array_index(ds_file_group->ds_file_infos, 0);
	g_string_assign(name, ds_file_info->path->str);

end:
	return name;
}

static
int create_one_port_for_trace(struct ctf_fs_component *ctf_fs,
		struct ctf_fs_trace *ctf_fs_trace,
		struct ctf_fs_ds_file_group *ds_file_group)
{
	int ret = 0;
	struct ctf_fs_port_data *port_data = NULL;
	GString *port_name = NULL;

	port_name = get_stream_instance_unique_name(ds_file_group);
	if (!port_name) {
		goto error;
	}

	BT_LOGD("Creating one port named `%s`", port_name->str);

	/* Create output port for this file */
	port_data = g_new0(struct ctf_fs_port_data, 1);
	if (!port_data) {
		goto error;
	}

	port_data->ds_file_group = ds_file_group;
	ret = bt_private_component_source_add_output_private_port(
		ctf_fs->priv_comp, port_name->str, port_data, NULL);
	if (ret) {
		goto error;
	}

	g_ptr_array_add(ctf_fs->port_data, port_data);
	port_data = NULL;
	goto end;

error:
	ret = -1;

end:
	if (port_name) {
		g_string_free(port_name, TRUE);
	}

	port_data_destroy(port_data);
	return ret;
}

static
int create_ports_for_trace(struct ctf_fs_component *ctf_fs,
		struct ctf_fs_trace *ctf_fs_trace)
{
	int ret = 0;
	size_t i;

	/* Create one output port for each stream file group */
	for (i = 0; i < ctf_fs_trace->ds_file_groups->len; i++) {
		struct ctf_fs_ds_file_group *ds_file_group =
			g_ptr_array_index(ctf_fs_trace->ds_file_groups, i);

		ret = create_one_port_for_trace(ctf_fs, ctf_fs_trace,
			ds_file_group);
		if (ret) {
			BT_LOGE("Cannot create output port.");
			goto end;
		}
	}

end:
	return ret;
}

static
uint64_t get_packet_header_stream_instance_id(struct ctf_fs_trace *ctf_fs_trace,
		struct bt_ctf_field *packet_header_field)
{
	struct bt_ctf_field *stream_instance_id_field = NULL;
	uint64_t stream_instance_id = -1ULL;
	int ret;

	if (!packet_header_field) {
		goto end;
	}

	stream_instance_id_field = bt_ctf_field_structure_get_field_by_name(
		packet_header_field, "stream_instance_id");
	if (!stream_instance_id_field) {
		goto end;
	}

	ret = bt_ctf_field_unsigned_integer_get_value(stream_instance_id_field,
		&stream_instance_id);
	if (ret) {
		stream_instance_id = -1ULL;
		goto end;
	}

end:
	bt_put(stream_instance_id_field);
	return stream_instance_id;
}

struct bt_ctf_stream_class *stream_class_from_packet_header(
		struct ctf_fs_trace *ctf_fs_trace,
		struct bt_ctf_field *packet_header_field)
{
	struct bt_ctf_field *stream_id_field = NULL;
	struct bt_ctf_stream_class *stream_class = NULL;
	uint64_t stream_id = -1ULL;
	int ret;

	if (!packet_header_field) {
		goto single_stream_class;
	}

	stream_id_field = bt_ctf_field_structure_get_field_by_name(
		packet_header_field, "stream_id");
	if (!stream_id_field) {
		goto single_stream_class;
	}

	ret = bt_ctf_field_unsigned_integer_get_value(stream_id_field,
		&stream_id);
	if (ret) {
		stream_id = -1ULL;
	}

	if (stream_id == -1ULL) {
single_stream_class:
		/* Single stream class */
		if (bt_ctf_trace_get_stream_class_count(
				ctf_fs_trace->metadata->trace) == 0) {
			goto end;
		}

		stream_class = bt_ctf_trace_get_stream_class_by_index(
			ctf_fs_trace->metadata->trace, 0);
	} else {
		stream_class = bt_ctf_trace_get_stream_class_by_id(
			ctf_fs_trace->metadata->trace, stream_id);
	}

end:
	bt_put(stream_id_field);
	return stream_class;
}

uint64_t get_packet_context_timestamp_begin_ns(
		struct ctf_fs_trace *ctf_fs_trace,
		struct bt_ctf_field *packet_context_field)
{
	int ret;
	struct bt_ctf_field *timestamp_begin_field = NULL;
	struct bt_ctf_field_type *timestamp_begin_ft = NULL;
	uint64_t timestamp_begin_raw_value = -1ULL;
	uint64_t timestamp_begin_ns = -1ULL;
	int64_t timestamp_begin_ns_signed;
	struct bt_ctf_clock_class *timestamp_begin_clock_class = NULL;
	struct bt_ctf_clock_value *clock_value = NULL;

	if (!packet_context_field) {
		goto end;
	}

	timestamp_begin_field = bt_ctf_field_structure_get_field_by_name(
		packet_context_field, "timestamp_begin");
	if (!timestamp_begin_field) {
		goto end;
	}

	timestamp_begin_ft = bt_ctf_field_get_type(timestamp_begin_field);
	assert(timestamp_begin_ft);
	timestamp_begin_clock_class =
		bt_ctf_field_type_integer_get_mapped_clock_class(timestamp_begin_ft);
	if (!timestamp_begin_clock_class) {
		goto end;
	}

	ret = bt_ctf_field_unsigned_integer_get_value(timestamp_begin_field,
		&timestamp_begin_raw_value);
	if (ret) {
		goto end;
	}

	clock_value = bt_ctf_clock_value_create(timestamp_begin_clock_class,
		timestamp_begin_raw_value);
	if (!clock_value) {
		goto end;
	}

	ret = bt_ctf_clock_value_get_value_ns_from_epoch(clock_value,
		&timestamp_begin_ns_signed);
	if (ret) {
		goto end;
	}

	timestamp_begin_ns = (uint64_t) timestamp_begin_ns_signed;

end:
	bt_put(timestamp_begin_field);
	bt_put(timestamp_begin_ft);
	bt_put(timestamp_begin_clock_class);
	bt_put(clock_value);
	return timestamp_begin_ns;
}

static
void ctf_fs_ds_file_info_destroy(struct ctf_fs_ds_file_info *ds_file_info)
{
	if (!ds_file_info) {
		return;
	}

	if (ds_file_info->path) {
		g_string_free(ds_file_info->path, TRUE);
	}

	ctf_fs_ds_index_destroy(ds_file_info->index);
	g_free(ds_file_info);
}

static
struct ctf_fs_ds_file_info *ctf_fs_ds_file_info_create(const char *path,
		uint64_t begin_ns, struct ctf_fs_ds_index *index)
{
	struct ctf_fs_ds_file_info *ds_file_info;

	ds_file_info = g_new0(struct ctf_fs_ds_file_info, 1);
	if (!ds_file_info) {
		goto end;
	}

	ds_file_info->path = g_string_new(path);
	if (!ds_file_info->path) {
		ctf_fs_ds_file_info_destroy(ds_file_info);
		ds_file_info = NULL;
		goto end;
	}

	ds_file_info->begin_ns = begin_ns;
	ds_file_info->index = index;
	index = NULL;

end:
	ctf_fs_ds_index_destroy(index);
	return ds_file_info;
}

static
void ctf_fs_ds_file_group_destroy(struct ctf_fs_ds_file_group *ds_file_group)
{
	if (!ds_file_group) {
		return;
	}

	if (ds_file_group->ds_file_infos) {
		g_ptr_array_free(ds_file_group->ds_file_infos, TRUE);
	}

	bt_put(ds_file_group->stream);
	bt_put(ds_file_group->stream_class);
	g_free(ds_file_group);
}

static
struct ctf_fs_ds_file_group *ctf_fs_ds_file_group_create(
		struct ctf_fs_trace *ctf_fs_trace,
		struct bt_ctf_stream_class *stream_class,
		uint64_t stream_instance_id)
{
	struct ctf_fs_ds_file_group *ds_file_group;

	ds_file_group = g_new0(struct ctf_fs_ds_file_group, 1);
	if (!ds_file_group) {
		goto error;
	}

	ds_file_group->ds_file_infos = g_ptr_array_new_with_free_func(
		(GDestroyNotify) ctf_fs_ds_file_info_destroy);
	if (!ds_file_group->ds_file_infos) {
		goto error;
	}

	ds_file_group->stream_id = stream_instance_id;
	assert(stream_class);
	ds_file_group->stream_class = bt_get(stream_class);
	ds_file_group->ctf_fs_trace = ctf_fs_trace;
	goto end;

error:
	ctf_fs_ds_file_group_destroy(ds_file_group);
	ds_file_group = NULL;

end:
	return ds_file_group;
}

static
int ctf_fs_ds_file_group_add_ds_file_info(
		struct ctf_fs_ds_file_group *ds_file_group,
		const char *path, uint64_t begin_ns,
		struct ctf_fs_ds_index *index)
{
	struct ctf_fs_ds_file_info *ds_file_info;
	gint i = 0;
	int ret = 0;

	/* Onwership of index is transferred. */
	ds_file_info = ctf_fs_ds_file_info_create(path, begin_ns, index);
	index = NULL;
	if (!ds_file_info) {
		goto error;
	}

	/* Find a spot to insert this one */
	for (i = 0; i < ds_file_group->ds_file_infos->len; i++) {
		struct ctf_fs_ds_file_info *other_ds_file_info =
			g_ptr_array_index(ds_file_group->ds_file_infos, i);

		if (begin_ns < other_ds_file_info->begin_ns) {
			break;
		}
	}

	if (i == ds_file_group->ds_file_infos->len) {
		/* Append instead */
		i = -1;
	}

	g_ptr_array_insert(ds_file_group->ds_file_infos, i, ds_file_info);
	ds_file_info = NULL;
	goto end;

error:
	ctf_fs_ds_file_info_destroy(ds_file_info);
	ctf_fs_ds_index_destroy(index);
	ret = -1;
end:
	return ret;
}

static
int add_ds_file_to_ds_file_group(struct ctf_fs_trace *ctf_fs_trace,
		const char *path)
{
	struct bt_ctf_field *packet_header_field = NULL;
	struct bt_ctf_field *packet_context_field = NULL;
	struct bt_ctf_stream_class *stream_class = NULL;
	uint64_t stream_instance_id = -1ULL;
	uint64_t begin_ns = -1ULL;
	struct ctf_fs_ds_file_group *ds_file_group = NULL;
	bool add_group = false;
	int ret;
	size_t i;
	struct ctf_fs_ds_file *ds_file = NULL;
	struct ctf_fs_ds_index *index = NULL;
	struct bt_ctf_notif_iter *notif_iter = NULL;

	notif_iter = bt_ctf_notif_iter_create(ctf_fs_trace->metadata->trace,
		bt_common_get_page_size() * 8, ctf_fs_ds_file_medops, NULL);
	if (!notif_iter) {
		BT_LOGE_STR("Cannot create a CTF notification iterator.");
		goto error;
	}

	ds_file = ctf_fs_ds_file_create(ctf_fs_trace, notif_iter, NULL, path);
	if (!ds_file) {
		goto error;
	}

	ret = ctf_fs_ds_file_get_packet_header_context_fields(ds_file,
		&packet_header_field, &packet_context_field);
	if (ret) {
		BT_LOGE("Cannot get stream file's first packet's header and context fields (`%s`).",
			path);
		goto error;
	}

	stream_instance_id = get_packet_header_stream_instance_id(ctf_fs_trace,
		packet_header_field);
	begin_ns = get_packet_context_timestamp_begin_ns(ctf_fs_trace,
		packet_context_field);
	stream_class = stream_class_from_packet_header(ctf_fs_trace,
		packet_header_field);
	if (!stream_class) {
		goto error;
	}

	index = ctf_fs_ds_file_build_index(ds_file);
	if (!index) {
		BT_LOGW("Failed to index CTF stream file \'%s\'",
			ds_file->file->path->str);
	}

	if (begin_ns == -1ULL) {
		/*
		 * No beggining timestamp to sort the stream files
		 * within a stream file group, so consider that this
		 * file must be the only one within its group.
		 */
		stream_instance_id = -1ULL;
	}

	if (stream_instance_id == -1ULL) {
		/*
		 * No stream instance ID or no beginning timestamp:
		 * create a unique stream file group for this stream
		 * file because, even if there's a stream instance ID,
		 * there's no timestamp to order the file within its
		 * group.
		 */
		ds_file_group = ctf_fs_ds_file_group_create(ctf_fs_trace,
			stream_class, stream_instance_id);
		if (!ds_file_group) {
			goto error;
		}

		ret = ctf_fs_ds_file_group_add_ds_file_info(ds_file_group,
			path, begin_ns, index);
		/* Ownership of index is transferred. */
		index = NULL;
		if (ret) {
			goto error;
		}

		add_group = true;
		goto end;
	}

	assert(stream_instance_id != -1ULL);
	assert(begin_ns != -1ULL);

	/* Find an existing stream file group with this ID */
	for (i = 0; i < ctf_fs_trace->ds_file_groups->len; i++) {
		ds_file_group = g_ptr_array_index(
			ctf_fs_trace->ds_file_groups, i);

		if (ds_file_group->stream_class == stream_class &&
				ds_file_group->stream_id ==
				stream_instance_id) {
			break;
		}

		ds_file_group = NULL;
	}

	if (!ds_file_group) {
		ds_file_group = ctf_fs_ds_file_group_create(ctf_fs_trace,
			stream_class, stream_instance_id);
		if (!ds_file_group) {
			goto error;
		}

		add_group = true;
	}

	ret = ctf_fs_ds_file_group_add_ds_file_info(ds_file_group, path,
		begin_ns, index);
	index = NULL;
	if (ret) {
		goto error;
	}

	goto end;

error:
	ctf_fs_ds_file_group_destroy(ds_file_group);
	ret = -1;

end:
	if (add_group && ds_file_group) {
		g_ptr_array_add(ctf_fs_trace->ds_file_groups, ds_file_group);
	}

	ctf_fs_ds_file_destroy(ds_file);

	if (notif_iter) {
		bt_ctf_notif_iter_destroy(notif_iter);
	}

	ctf_fs_ds_index_destroy(index);
	bt_put(packet_header_field);
	bt_put(packet_context_field);
	bt_put(stream_class);
	return ret;
}

static
int create_ds_file_groups(struct ctf_fs_trace *ctf_fs_trace)
{
	int ret = 0;
	const char *basename;
	GError *error = NULL;
	GDir *dir = NULL;
	size_t i;

	/* Check each file in the path directory, except specific ones */
	dir = g_dir_open(ctf_fs_trace->path->str, 0, &error);
	if (!dir) {
		BT_LOGE("Cannot open directory `%s`: %s (code %d)",
			ctf_fs_trace->path->str, error->message,
			error->code);
		goto error;
	}

	while ((basename = g_dir_read_name(dir))) {
		struct ctf_fs_file *file;

		if (!strcmp(basename, CTF_FS_METADATA_FILENAME)) {
			/* Ignore the metadata stream. */
			BT_LOGD("Ignoring metadata file `%s/%s`",
				ctf_fs_trace->path->str, basename);
			continue;
		}

		if (basename[0] == '.') {
			BT_LOGD("Ignoring hidden file `%s/%s`",
				ctf_fs_trace->path->str, basename);
			continue;
		}

		/* Create the file. */
		file = ctf_fs_file_create();
		if (!file) {
			BT_LOGE("Cannot create stream file object for file `%s/%s`",
				ctf_fs_trace->path->str, basename);
			goto error;
		}

		/* Create full path string. */
		g_string_append_printf(file->path, "%s/%s",
				ctf_fs_trace->path->str, basename);
		if (!g_file_test(file->path->str, G_FILE_TEST_IS_REGULAR)) {
			BT_LOGD("Ignoring non-regular file `%s`",
				file->path->str);
			ctf_fs_file_destroy(file);
			file = NULL;
			continue;
		}

		ret = ctf_fs_file_open(file, "rb");
		if (ret) {
			BT_LOGE("Cannot open stream file `%s`", file->path->str);
			goto error;
		}

		if (file->size == 0) {
			/* Skip empty stream. */
			BT_LOGD("Ignoring empty file `%s`", file->path->str);
			ctf_fs_file_destroy(file);
			continue;
		}

		ret = add_ds_file_to_ds_file_group(ctf_fs_trace,
			file->path->str);
		if (ret) {
			BT_LOGE("Cannot add stream file `%s` to stream file group",
				file->path->str);
			ctf_fs_file_destroy(file);
			goto error;
		}

		ctf_fs_file_destroy(file);
	}

	/*
	 * At this point, DS file groupes are created, but their
	 * associated stream objects do not exist yet. This is because
	 * we need to name the created stream object with the data
	 * stream file's path. We have everything we need here to do
	 * this.
	 */
	for (i = 0; i < ctf_fs_trace->ds_file_groups->len; i++) {
		struct ctf_fs_ds_file_group *ds_file_group =
			g_ptr_array_index(ctf_fs_trace->ds_file_groups, i);
		GString *name = get_stream_instance_unique_name(ds_file_group);

		if (!name) {
			goto error;
		}

		if (ds_file_group->stream_id == -1ULL) {
			/* No stream ID */
			ds_file_group->stream = bt_ctf_stream_create(
				ds_file_group->stream_class, name->str);
		} else {
			/* Specific stream ID */
			ds_file_group->stream = bt_ctf_stream_create_with_id(
				ds_file_group->stream_class, name->str,
				ds_file_group->stream_id);
		}

		g_string_free(name, TRUE);

		if (!ds_file_group) {
			BT_LOGE("Cannot create stream for DS file group: "
				"addr=%p, stream-name=\"%s\"",
				ds_file_group, name->str);
			goto error;
		}
	}

	goto end;

error:
	ret = -1;

end:
	if (dir) {
		g_dir_close(dir);
		dir = NULL;
	}

	if (error) {
		g_error_free(error);
	}

	return ret;
}

static
int create_cc_prio_map(struct ctf_fs_trace *ctf_fs_trace)
{
	int ret = 0;
	size_t i;
	int count;

	assert(ctf_fs_trace);
	ctf_fs_trace->cc_prio_map = bt_clock_class_priority_map_create();
	if (!ctf_fs_trace->cc_prio_map) {
		ret = -1;
		goto end;
	}

	count = bt_ctf_trace_get_clock_class_count(
		ctf_fs_trace->metadata->trace);
	assert(count >= 0);

	for (i = 0; i < count; i++) {
		struct bt_ctf_clock_class *clock_class =
			bt_ctf_trace_get_clock_class_by_index(
				ctf_fs_trace->metadata->trace, i);

		assert(clock_class);
		ret = bt_clock_class_priority_map_add_clock_class(
			ctf_fs_trace->cc_prio_map, clock_class, 0);
		BT_PUT(clock_class);

		if (ret) {
			goto end;
		}
	}

end:
	return ret;
}

BT_HIDDEN
struct ctf_fs_trace *ctf_fs_trace_create(const char *path, const char *name,
		struct ctf_fs_metadata_config *metadata_config)
{
	struct ctf_fs_trace *ctf_fs_trace;
	int ret;

	ctf_fs_trace = g_new0(struct ctf_fs_trace, 1);
	if (!ctf_fs_trace) {
		goto end;
	}

	ctf_fs_trace->path = g_string_new(path);
	if (!ctf_fs_trace->path) {
		goto error;
	}

	ctf_fs_trace->name = g_string_new(name);
	if (!ctf_fs_trace->name) {
		goto error;
	}

	ctf_fs_trace->metadata = g_new0(struct ctf_fs_metadata, 1);
	if (!ctf_fs_trace->metadata) {
		goto error;
	}

	ctf_fs_trace->ds_file_groups = g_ptr_array_new_with_free_func(
		(GDestroyNotify) ctf_fs_ds_file_group_destroy);
	if (!ctf_fs_trace->ds_file_groups) {
		goto error;
	}

	ret = ctf_fs_metadata_set_trace(ctf_fs_trace, metadata_config);
	if (ret) {
		goto error;
	}

	ret = create_ds_file_groups(ctf_fs_trace);
	if (ret) {
		goto error;
	}

	ret = create_cc_prio_map(ctf_fs_trace);
	if (ret) {
		goto error;
	}

	/*
	 * create_ds_file_groups() created all the streams that this
	 * trace needs. There won't be any more. Therefore it is safe to
	 * make this trace static.
	 */
	(void) bt_ctf_trace_set_is_static(ctf_fs_trace->metadata->trace);

	goto end;

error:
	ctf_fs_trace_destroy(ctf_fs_trace);
	ctf_fs_trace = NULL;
end:
	return ctf_fs_trace;
}

static
int path_is_ctf_trace(const char *path)
{
	GString *metadata_path = g_string_new(NULL);
	int ret = 0;

	if (!metadata_path) {
		ret = -1;
		goto end;
	}

	g_string_printf(metadata_path, "%s/%s", path, CTF_FS_METADATA_FILENAME);

	if (g_file_test(metadata_path->str, G_FILE_TEST_IS_REGULAR)) {
		ret = 1;
		goto end;
	}

end:
	g_string_free(metadata_path, TRUE);
	return ret;
}

static
int add_trace_path(GList **trace_paths, const char *path)
{
	GString *norm_path = NULL;
	int ret = 0;

	norm_path = bt_common_normalize_path(path, NULL);
	if (!norm_path) {
		BT_LOGE("Failed to normalize path `%s`.", path);
		ret = -1;
		goto end;
	}

	if (strcmp(norm_path->str, "/") == 0) {
		BT_LOGE("Opening a trace in `/` is not supported.");
		ret = -1;
		goto end;
	}

	*trace_paths = g_list_prepend(*trace_paths, norm_path);
	assert(*trace_paths);
	norm_path = NULL;

end:
	if (norm_path) {
		g_string_free(norm_path, TRUE);
	}

	return ret;
}

BT_HIDDEN
int ctf_fs_find_traces(GList **trace_paths, const char *start_path)
{
	int ret;
	GError *error = NULL;
	GDir *dir = NULL;
	const char *basename = NULL;

	/* Check if the starting path is a CTF trace itself */
	ret = path_is_ctf_trace(start_path);
	if (ret < 0) {
		goto end;
	}

	if (ret) {
		/*
		 * Stop recursion: a CTF trace cannot contain another
		 * CTF trace.
		 */
		ret = add_trace_path(trace_paths, start_path);
		goto end;
	}

	/* Look for subdirectories */
	if (!g_file_test(start_path, G_FILE_TEST_IS_DIR)) {
		/* Starting path is not a directory: end of recursion */
		goto end;
	}

	dir = g_dir_open(start_path, 0, &error);
	if (!dir) {
		if (error->code == G_FILE_ERROR_ACCES) {
			BT_LOGD("Cannot open directory `%s`: %s (code %d): continuing",
				start_path, error->message, error->code);
			goto end;
		}

		BT_LOGE("Cannot open directory `%s`: %s (code %d)",
			start_path, error->message, error->code);
		ret = -1;
		goto end;
	}

	while ((basename = g_dir_read_name(dir))) {
		GString *sub_path = g_string_new(NULL);

		if (!sub_path) {
			ret = -1;
			goto end;
		}

		g_string_printf(sub_path, "%s/%s", start_path, basename);
		ret = ctf_fs_find_traces(trace_paths, sub_path->str);
		g_string_free(sub_path, TRUE);
		if (ret) {
			goto end;
		}
	}

end:
	if (dir) {
		g_dir_close(dir);
	}

	if (error) {
		g_error_free(error);
	}

	return ret;
}

BT_HIDDEN
GList *ctf_fs_create_trace_names(GList *trace_paths, const char *base_path) {
	GList *trace_names = NULL;
	GList *node;
	const char *last_sep;
	size_t base_dist;

	/*
	 * At this point we know that all the trace paths are
	 * normalized, and so is the base path. This means that
	 * they are absolute and they don't end with a separator.
	 * We can simply find the location of the last separator
	 * in the base path, which gives us the name of the actual
	 * directory to look into, and use this location as the
	 * start of each trace name within each trace path.
	 *
	 * For example:
	 *
	 *     Base path: /home/user/my-traces/some-trace
	 *     Trace paths:
	 *       - /home/user/my-traces/some-trace/host1/trace1
	 *       - /home/user/my-traces/some-trace/host1/trace2
	 *       - /home/user/my-traces/some-trace/host2/trace
	 *       - /home/user/my-traces/some-trace/other-trace
	 *
	 * In this case the trace names are:
	 *
	 *       - some-trace/host1/trace1
	 *       - some-trace/host1/trace2
	 *       - some-trace/host2/trace
	 *       - some-trace/other-trace
	 */
	last_sep = strrchr(base_path, G_DIR_SEPARATOR);

	/* We know there's at least one separator */
	assert(last_sep);

	/* Distance to base */
	base_dist = last_sep - base_path + 1;

	/* Create the trace names */
	for (node = trace_paths; node; node = g_list_next(node)) {
		GString *trace_name = g_string_new(NULL);
		GString *trace_path = node->data;

		assert(trace_name);
		g_string_assign(trace_name, &trace_path->str[base_dist]);
		trace_names = g_list_append(trace_names, trace_name);
	}

	return trace_names;
}

static
int create_ctf_fs_traces(struct ctf_fs_component *ctf_fs,
		const char *path_param)
{
	struct ctf_fs_trace *ctf_fs_trace = NULL;
	int ret = 0;
	GString *norm_path = NULL;
	GList *trace_paths = NULL;
	GList *trace_names = NULL;
	GList *tp_node;
	GList *tn_node;

	norm_path = bt_common_normalize_path(path_param, NULL);
	if (!norm_path) {
		BT_LOGE("Failed to normalize path: `%s`.",
			path_param);
		goto error;
	}

	ret = ctf_fs_find_traces(&trace_paths, norm_path->str);
	if (ret) {
		goto error;
	}

	if (!trace_paths) {
		BT_LOGE("No CTF traces recursively found in `%s`.",
			path_param);
		goto error;
	}

	trace_names = ctf_fs_create_trace_names(trace_paths, norm_path->str);
	if (!trace_names) {
		BT_LOGE("Cannot create trace names from trace paths.");
		goto error;
	}

	for (tp_node = trace_paths, tn_node = trace_names; tp_node;
			tp_node = g_list_next(tp_node),
			tn_node = g_list_next(tn_node)) {
		GString *trace_path = tp_node->data;
		GString *trace_name = tn_node->data;

		ctf_fs_trace = ctf_fs_trace_create(trace_path->str,
				trace_name->str, &ctf_fs->metadata_config);
		if (!ctf_fs_trace) {
			BT_LOGE("Cannot create trace for `%s`.",
				trace_path->str);
			goto error;
		}

		ret = create_ports_for_trace(ctf_fs, ctf_fs_trace);
		if (ret) {
			goto error;
		}

		g_ptr_array_add(ctf_fs->traces, ctf_fs_trace);
		ctf_fs_trace = NULL;
	}

	goto end;

error:
	ret = -1;
	ctf_fs_trace_destroy(ctf_fs_trace);

end:
	for (tp_node = trace_paths; tp_node; tp_node = g_list_next(tp_node)) {
		if (tp_node->data) {
			g_string_free(tp_node->data, TRUE);
		}
	}

	for (tn_node = trace_names; tn_node; tn_node = g_list_next(tn_node)) {
		if (tn_node->data) {
			g_string_free(tn_node->data, TRUE);
		}
	}

	if (trace_paths) {
		g_list_free(trace_paths);
	}

	if (trace_names) {
		g_list_free(trace_names);
	}

	if (norm_path) {
		g_string_free(norm_path, TRUE);
	}

	return ret;
}

static
struct ctf_fs_component *ctf_fs_create(struct bt_private_component *priv_comp,
		struct bt_value *params)
{
	struct ctf_fs_component *ctf_fs;
	struct bt_value *value = NULL;
	const char *path_param;
	int ret;

	ctf_fs = g_new0(struct ctf_fs_component, 1);
	if (!ctf_fs) {
		goto end;
	}

	ret = bt_private_component_set_user_data(priv_comp, ctf_fs);
	assert(ret == 0);

	/*
	 * We don't need to get a new reference here because as long as
	 * our private ctf_fs_component object exists, the containing
	 * private component should also exist.
	 */
	ctf_fs->priv_comp = priv_comp;
	value = bt_value_map_get(params, "path");
	if (!bt_value_is_string(value)) {
		goto error;
	}

	ret = bt_value_string_get(value, &path_param);
	assert(ret == 0);
	BT_PUT(value);
	value = bt_value_map_get(params, "clock-class-offset-s");
	if (value) {
		if (!bt_value_is_integer(value)) {
			BT_LOGE("clock-class-offset-s should be an integer");
			goto error;
		}
		ret = bt_value_integer_get(value,
			&ctf_fs->metadata_config.clock_class_offset_s);
		assert(ret == 0);
		BT_PUT(value);
	}

	value = bt_value_map_get(params, "clock-class-offset-ns");
	if (value) {
		if (!bt_value_is_integer(value)) {
			BT_LOGE("clock-class-offset-ns should be an integer");
			goto error;
		}
		ret = bt_value_integer_get(value,
			&ctf_fs->metadata_config.clock_class_offset_ns);
		assert(ret == 0);
		BT_PUT(value);
	}

	ctf_fs->port_data = g_ptr_array_new_with_free_func(port_data_destroy);
	if (!ctf_fs->port_data) {
		goto error;
	}

	ctf_fs->traces = g_ptr_array_new_with_free_func(
			ctf_fs_trace_destroy_notifier);
	if (!ctf_fs->traces) {
		goto error;
	}

	ret = create_ctf_fs_traces(ctf_fs, path_param);
	if (ret) {
		goto error;
	}

	goto end;

error:
	ctf_fs_destroy(ctf_fs);
	ctf_fs = NULL;
	ret = bt_private_component_set_user_data(priv_comp, NULL);
	assert(ret == 0);

end:
	bt_put(value);
	return ctf_fs;
}

BT_HIDDEN
enum bt_component_status ctf_fs_init(struct bt_private_component *priv_comp,
		struct bt_value *params, UNUSED_VAR void *init_method_data)
{
	struct ctf_fs_component *ctf_fs;
	enum bt_component_status ret = BT_COMPONENT_STATUS_OK;

	ctf_fs = ctf_fs_create(priv_comp, params);
	if (!ctf_fs) {
		ret = BT_COMPONENT_STATUS_ERROR;
	}

	return ret;
}

BT_HIDDEN
struct bt_value *ctf_fs_query(struct bt_component_class *comp_class,
		const char *object, struct bt_value *params)
{
	struct bt_value *result = NULL;

	if (!strcmp(object, "metadata-info")) {
		result = metadata_info_query(comp_class, params);
	} else if (!strcmp(object, "trace-info")) {
		result = trace_info_query(comp_class, params);
	} else {
		BT_LOGE("Unknown query object `%s`", object);
		goto end;
	}
end:
	return result;
}
