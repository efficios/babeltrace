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
#include <babeltrace/babeltrace.h>
#include <plugins-common.h>
#include <glib.h>
#include <babeltrace/assert-internal.h>
#include <inttypes.h>
#include <stdbool.h>
#include "fs.h"
#include "metadata.h"
#include "data-stream-file.h"
#include "file.h"
#include "../common/metadata/decoder.h"
#include "../common/notif-iter/notif-iter.h"
#include "../common/utils/utils.h"
#include "query.h"

#define BT_LOG_TAG "PLUGIN-CTF-FS-SRC"
#include "logging.h"

static
int notif_iter_data_set_current_ds_file(struct ctf_fs_notif_iter_data *notif_iter_data)
{
	struct ctf_fs_ds_file_info *ds_file_info;
	int ret = 0;

	BT_ASSERT(notif_iter_data->ds_file_info_index <
		notif_iter_data->ds_file_group->ds_file_infos->len);
	ds_file_info = g_ptr_array_index(
		notif_iter_data->ds_file_group->ds_file_infos,
		notif_iter_data->ds_file_info_index);

	ctf_fs_ds_file_destroy(notif_iter_data->ds_file);
	notif_iter_data->ds_file = ctf_fs_ds_file_create(
		notif_iter_data->ds_file_group->ctf_fs_trace,
		notif_iter_data->pc_notif_iter,
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
		bt_notif_iter_destroy(notif_iter_data->notif_iter);
	}

	g_free(notif_iter_data);
}

static
enum bt_self_notification_iterator_status ctf_fs_iterator_next_one(
		struct ctf_fs_notif_iter_data *notif_iter_data,
		const bt_notification **notif)
{
	enum bt_self_notification_iterator_status status;
	bt_notification *priv_notif;
	int ret;

	BT_ASSERT(notif_iter_data->ds_file);
	status = ctf_fs_ds_file_next(notif_iter_data->ds_file, &priv_notif);
	*notif = priv_notif;

	if (status == BT_SELF_NOTIFICATION_ITERATOR_STATUS_OK &&
			bt_notification_get_type(*notif) ==
			BT_NOTIFICATION_TYPE_STREAM_BEGINNING) {
		if (notif_iter_data->skip_stream_begin_notifs) {
			/*
			 * We already emitted a
			 * BT_NOTIFICATION_TYPE_STREAM_BEGINNING
			 * notification: skip this one, get a new one.
			 */
			BT_NOTIFICATION_PUT_REF_AND_RESET(*notif);
			status = ctf_fs_ds_file_next(notif_iter_data->ds_file,
				&priv_notif);
			*notif = priv_notif;
			BT_ASSERT(status != BT_SELF_NOTIFICATION_ITERATOR_STATUS_END);
			goto end;
		} else {
			/*
			 * First BT_NOTIFICATION_TYPE_STREAM_BEGINNING
			 * notification: skip all following.
			 */
			notif_iter_data->skip_stream_begin_notifs = true;
			goto end;
		}
	}

	if (status == BT_SELF_NOTIFICATION_ITERATOR_STATUS_OK &&
			bt_notification_get_type(*notif) ==
			BT_NOTIFICATION_TYPE_STREAM_END) {
		notif_iter_data->ds_file_info_index++;

		if (notif_iter_data->ds_file_info_index ==
				notif_iter_data->ds_file_group->ds_file_infos->len) {
			/*
			 * No more stream files to read: we reached the
			 * real end. Emit this
			 * BT_NOTIFICATION_TYPE_STREAM_END notification.
			 * The next time ctf_fs_iterator_next() is
			 * called for this notification iterator,
			 * ctf_fs_ds_file_next() will return
			 * BT_SELF_NOTIFICATION_ITERATOR_STATUS_END().
			 */
			goto end;
		}

		BT_NOTIFICATION_PUT_REF_AND_RESET(*notif);
		bt_notif_iter_reset(notif_iter_data->notif_iter);

		/*
		 * Open and start reading the next stream file within
		 * our stream file group.
		 */
		ret = notif_iter_data_set_current_ds_file(notif_iter_data);
		if (ret) {
			status = BT_SELF_NOTIFICATION_ITERATOR_STATUS_ERROR;
			goto end;
		}

		status = ctf_fs_ds_file_next(notif_iter_data->ds_file, &priv_notif);
		*notif = priv_notif;

		/*
		 * If we get a notification, we expect to get a
		 * BT_NOTIFICATION_TYPE_STREAM_BEGINNING notification
		 * because the iterator's state machine emits one before
		 * even requesting the first block of data from the
		 * medium. Skip this notification because we're not
		 * really starting a new stream here, and try getting a
		 * new notification (which, if it works, is a
		 * BT_NOTIFICATION_TYPE_PACKET_BEGINNING one). We're sure to
		 * get at least one pair of
		 * BT_NOTIFICATION_TYPE_PACKET_BEGINNING and
		 * BT_NOTIFICATION_TYPE_PACKET_END notifications in the
		 * case of a single, empty packet. We know there's at
		 * least one packet because the stream file group does
		 * not contain empty stream files.
		 */
		BT_ASSERT(notif_iter_data->skip_stream_begin_notifs);

		if (status == BT_SELF_NOTIFICATION_ITERATOR_STATUS_OK) {
			BT_ASSERT(bt_notification_get_type(*notif) ==
				BT_NOTIFICATION_TYPE_STREAM_BEGINNING);
			BT_NOTIFICATION_PUT_REF_AND_RESET(*notif);
			status = ctf_fs_ds_file_next(notif_iter_data->ds_file,
				&priv_notif);
			*notif = priv_notif;
			BT_ASSERT(status != BT_SELF_NOTIFICATION_ITERATOR_STATUS_END);
		}
	}

end:
	return status;
}

BT_HIDDEN
enum bt_self_notification_iterator_status ctf_fs_iterator_next(
		bt_self_notification_iterator *iterator,
		bt_notification_array_const notifs, uint64_t capacity,
		uint64_t *count)
{
	enum bt_self_notification_iterator_status status =
		BT_SELF_NOTIFICATION_ITERATOR_STATUS_OK;
	struct ctf_fs_notif_iter_data *notif_iter_data =
		bt_self_notification_iterator_get_data(iterator);
	uint64_t i = 0;

	while (i < capacity && status == BT_SELF_NOTIFICATION_ITERATOR_STATUS_OK) {
		status = ctf_fs_iterator_next_one(notif_iter_data, &notifs[i]);
		if (status == BT_SELF_NOTIFICATION_ITERATOR_STATUS_OK) {
			i++;
		}
	}

	if (i > 0) {
		/*
		 * Even if ctf_fs_iterator_next_one() returned something
		 * else than BT_SELF_NOTIFICATION_ITERATOR_STATUS_OK, we
		 * accumulated notification objects in the output
		 * notification array, so we need to return
		 * BT_SELF_NOTIFICATION_ITERATOR_STATUS_OK so that they are
		 * transfered to downstream. This other status occurs
		 * again the next time muxer_notif_iter_do_next() is
		 * called, possibly without any accumulated
		 * notification, in which case we'll return it.
		 */
		*count = i;
		status = BT_SELF_NOTIFICATION_ITERATOR_STATUS_OK;
	}

	return status;
}

void ctf_fs_iterator_finalize(bt_self_notification_iterator *it)
{
	ctf_fs_notif_iter_data_destroy(
		bt_self_notification_iterator_get_data(it));
}

enum bt_self_notification_iterator_status ctf_fs_iterator_init(
		bt_self_notification_iterator *self_notif_iter,
		bt_self_component_source *self_comp,
		bt_self_component_port_output *self_port)
{
	struct ctf_fs_port_data *port_data;
	struct ctf_fs_notif_iter_data *notif_iter_data = NULL;
	enum bt_self_notification_iterator_status ret =
		BT_SELF_NOTIFICATION_ITERATOR_STATUS_OK;
	int iret;

	port_data = bt_self_component_port_get_data(
		bt_self_component_port_output_as_self_component_port(
			self_port));
	BT_ASSERT(port_data);
	notif_iter_data = g_new0(struct ctf_fs_notif_iter_data, 1);
	if (!notif_iter_data) {
		ret = BT_SELF_NOTIFICATION_ITERATOR_STATUS_NOMEM;
		goto error;
	}

	notif_iter_data->pc_notif_iter = self_notif_iter;
	notif_iter_data->notif_iter = bt_notif_iter_create(
		port_data->ds_file_group->ctf_fs_trace->metadata->tc,
		bt_common_get_page_size() * 8,
		ctf_fs_ds_file_medops, NULL);
	if (!notif_iter_data->notif_iter) {
		BT_LOGE_STR("Cannot create a CTF notification iterator.");
		ret = BT_SELF_NOTIFICATION_ITERATOR_STATUS_NOMEM;
		goto error;
	}

	notif_iter_data->ds_file_group = port_data->ds_file_group;
	iret = notif_iter_data_set_current_ds_file(notif_iter_data);
	if (iret) {
		ret = BT_SELF_NOTIFICATION_ITERATOR_STATUS_ERROR;
		goto error;
	}

	bt_self_notification_iterator_set_data(self_notif_iter,
		notif_iter_data);
	if (ret != BT_SELF_NOTIFICATION_ITERATOR_STATUS_OK) {
		goto error;
	}

	notif_iter_data = NULL;
	goto end;

error:
	bt_self_notification_iterator_set_data(self_notif_iter, NULL);

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

	BT_TRACE_PUT_REF_AND_RESET(ctf_fs_trace->trace);

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

	g_free(ctf_fs_trace);
}

static
void ctf_fs_trace_destroy_notifier(void *data)
{
	struct ctf_fs_trace *trace = data;
	ctf_fs_trace_destroy(trace);
}

void ctf_fs_finalize(bt_self_component_source *component)
{
	ctf_fs_destroy(bt_self_component_get_data(
		bt_self_component_source_as_self_component(component)));
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
	BT_ASSERT(ds_file_group->ds_file_infos->len > 0);
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

	port_data->ctf_fs = ctf_fs;
	port_data->ds_file_group = ds_file_group;
	ret = bt_self_component_source_add_output_port(
		ctf_fs->self_comp, port_name->str, port_data, NULL);
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
		int64_t begin_ns, struct ctf_fs_ds_index *index)
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

	bt_stream_put_ref(ds_file_group->stream);
	bt_stream_class_put_ref(ds_file_group->stream_class);
	g_free(ds_file_group);
}

static
struct ctf_fs_ds_file_group *ctf_fs_ds_file_group_create(
		struct ctf_fs_trace *ctf_fs_trace,
		bt_stream_class *stream_class,
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
	BT_ASSERT(stream_class);
	ds_file_group->stream_class = stream_class;
	bt_stream_class_get_ref(ds_file_group->stream_class);
	ds_file_group->ctf_fs_trace = ctf_fs_trace;
	goto end;

error:
	ctf_fs_ds_file_group_destroy(ds_file_group);
	ds_file_group = NULL;

end:
	return ds_file_group;
}

/* Replace by g_ptr_array_insert when we depend on glib >= 2.40. */
static
void array_insert(GPtrArray *array, gpointer element, size_t pos)
{
	size_t original_array_len = array->len;

	/* Allocate an unused element at the end of the array. */
	g_ptr_array_add(array, NULL);

	/* If we are not inserting at the end, move the elements by one. */
	if (pos < original_array_len) {
		memmove(&(array->pdata[pos + 1]),
			&(array->pdata[pos]),
			(original_array_len - pos) * sizeof(gpointer));
	}

	/* Insert the value and bump the array len */
	array->pdata[pos] = element;
}

static
int ctf_fs_ds_file_group_add_ds_file_info(
		struct ctf_fs_ds_file_group *ds_file_group,
		const char *path, int64_t begin_ns,
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

	array_insert(ds_file_group->ds_file_infos, ds_file_info, i);
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
	bt_stream_class *stream_class = NULL;
	int64_t stream_instance_id = -1;
	int64_t begin_ns = -1;
	struct ctf_fs_ds_file_group *ds_file_group = NULL;
	bool add_group = false;
	int ret;
	size_t i;
	struct ctf_fs_ds_file *ds_file = NULL;
	struct ctf_fs_ds_index *index = NULL;
	struct bt_notif_iter *notif_iter = NULL;
	struct ctf_stream_class *sc = NULL;
	struct bt_notif_iter_packet_properties props;

	notif_iter = bt_notif_iter_create(ctf_fs_trace->metadata->tc,
		bt_common_get_page_size() * 8, ctf_fs_ds_file_medops, NULL);
	if (!notif_iter) {
		BT_LOGE_STR("Cannot create a CTF notification iterator.");
		goto error;
	}

	ds_file = ctf_fs_ds_file_create(ctf_fs_trace, NULL, notif_iter,
		NULL, path);
	if (!ds_file) {
		goto error;
	}

	ret = ctf_fs_ds_file_borrow_packet_header_context_fields(ds_file,
		NULL, NULL);
	if (ret) {
		BT_LOGE("Cannot get stream file's first packet's header and context fields (`%s`).",
			path);
		goto error;
	}

	ret = bt_notif_iter_get_packet_properties(ds_file->notif_iter, &props);
	BT_ASSERT(ret == 0);
	sc = ctf_trace_class_borrow_stream_class_by_id(ds_file->metadata->tc,
		props.stream_class_id);
	BT_ASSERT(sc);
	stream_class = sc->ir_sc;
	BT_ASSERT(stream_class);
	stream_instance_id = props.data_stream_id;

	if (props.snapshots.beginning_clock != UINT64_C(-1)) {
		BT_ASSERT(sc->default_clock_class);
		ret = bt_clock_class_cycles_to_ns_from_origin(
			sc->default_clock_class,
			props.snapshots.beginning_clock, &begin_ns);
		if (ret) {
			BT_LOGE("Cannot convert clock cycles to nanoseconds from origin (`%s`).",
				path);
			goto error;
		}
	}

	index = ctf_fs_ds_file_build_index(ds_file);
	if (!index) {
		BT_LOGW("Failed to index CTF stream file \'%s\'",
			ds_file->file->path->str);
	}

	if (begin_ns == -1) {
		/*
		 * No beggining timestamp to sort the stream files
		 * within a stream file group, so consider that this
		 * file must be the only one within its group.
		 */
		stream_instance_id = -1;
	}

	if (stream_instance_id == -1) {
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

	BT_ASSERT(stream_instance_id != -1);
	BT_ASSERT(begin_ns != -1);

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
		bt_notif_iter_destroy(notif_iter);
	}

	ctf_fs_ds_index_destroy(index);
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
			BT_LOGD("Ignoring metadata file `%s" G_DIR_SEPARATOR_S "%s`",
				ctf_fs_trace->path->str, basename);
			continue;
		}

		if (basename[0] == '.') {
			BT_LOGD("Ignoring hidden file `%s" G_DIR_SEPARATOR_S "%s`",
				ctf_fs_trace->path->str, basename);
			continue;
		}

		/* Create the file. */
		file = ctf_fs_file_create();
		if (!file) {
			BT_LOGE("Cannot create stream file object for file `%s" G_DIR_SEPARATOR_S "%s`",
				ctf_fs_trace->path->str, basename);
			goto error;
		}

		/* Create full path string. */
		g_string_append_printf(file->path, "%s" G_DIR_SEPARATOR_S "%s",
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

		if (ds_file_group->stream_id == UINT64_C(-1)) {
			/* No stream ID: use 0 */
			ds_file_group->stream = bt_stream_create_with_id(
				ds_file_group->stream_class,
				ctf_fs_trace->trace,
				ctf_fs_trace->next_stream_id);
			ctf_fs_trace->next_stream_id++;
		} else {
			/* Specific stream ID */
			ds_file_group->stream = bt_stream_create_with_id(
				ds_file_group->stream_class,
				ctf_fs_trace->trace,
				(uint64_t) ds_file_group->stream_id);
		}

		if (!ds_file_group->stream) {
			BT_LOGE("Cannot create stream for DS file group: "
				"addr=%p, stream-name=\"%s\"",
				ds_file_group, name->str);
			g_string_free(name, TRUE);
			goto error;
		}

		ret = bt_stream_set_name(ds_file_group->stream,
			name->str);
		if (ret) {
			BT_LOGE("Cannot set stream's name: "
				"addr=%p, stream-name=\"%s\"",
				ds_file_group->stream, name->str);
			g_string_free(name, TRUE);
			goto error;
		}

		g_string_free(name, TRUE);
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
int set_trace_name(bt_trace *trace, const char *name_suffix)
{
	int ret = 0;
	const bt_trace_class *tc = bt_trace_borrow_class_const(trace);
	const bt_value *val;
	GString *name;

	name = g_string_new(NULL);
	if (!name) {
		BT_LOGE_STR("Failed to allocate a GString.");
		ret = -1;
		goto end;
	}

	/*
	 * Check if we have a trace environment string value named `hostname`.
	 * If so, use it as the trace name's prefix.
	 */
	val = bt_trace_class_borrow_environment_entry_value_by_name_const(
		tc, "hostname");
	if (val && bt_value_is_string(val)) {
		g_string_append(name, bt_value_string_get(val));

		if (name_suffix) {
			g_string_append_c(name, G_DIR_SEPARATOR);
		}
	}

	if (name_suffix) {
		g_string_append(name, name_suffix);
	}

	ret = bt_trace_set_name(trace, name->str);
	if (ret) {
		goto end;
	}

	goto end;

end:
	if (name) {
		g_string_free(name, TRUE);
	}

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

	ctf_fs_metadata_init(ctf_fs_trace->metadata);
	ctf_fs_trace->ds_file_groups = g_ptr_array_new_with_free_func(
		(GDestroyNotify) ctf_fs_ds_file_group_destroy);
	if (!ctf_fs_trace->ds_file_groups) {
		goto error;
	}

	ret = ctf_fs_metadata_set_trace_class(ctf_fs_trace, metadata_config);
	if (ret) {
		goto error;
	}

	ctf_fs_trace->trace =
		bt_trace_create(ctf_fs_trace->metadata->trace_class);
	if (!ctf_fs_trace->trace) {
		goto error;
	}

	ret = set_trace_name(ctf_fs_trace->trace, name);
	if (ret) {
		goto error;
	}

	ret = create_ds_file_groups(ctf_fs_trace);
	if (ret) {
		goto error;
	}

	/*
	 * create_ds_file_groups() created all the streams that this
	 * trace needs. There won't be any more. Therefore it is safe to
	 * make this trace static.
	 */
	(void) bt_trace_make_static(ctf_fs_trace->trace);

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

	g_string_printf(metadata_path, "%s" G_DIR_SEPARATOR_S "%s", path, CTF_FS_METADATA_FILENAME);

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

	// FIXME: Remove or ifdef for __MINGW32__
	if (strcmp(norm_path->str, "/") == 0) {
		BT_LOGE("Opening a trace in `/` is not supported.");
		ret = -1;
		goto end;
	}

	*trace_paths = g_list_prepend(*trace_paths, norm_path);
	BT_ASSERT(*trace_paths);
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

		g_string_printf(sub_path, "%s" G_DIR_SEPARATOR_S "%s", start_path, basename);
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
	BT_ASSERT(last_sep);

	/* Distance to base */
	base_dist = last_sep - base_path + 1;

	/* Create the trace names */
	for (node = trace_paths; node; node = g_list_next(node)) {
		GString *trace_name = g_string_new(NULL);
		GString *trace_path = node->data;

		BT_ASSERT(trace_name);
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
struct ctf_fs_component *ctf_fs_create(
		bt_self_component_source *self_comp,
		const bt_value *params)
{
	struct ctf_fs_component *ctf_fs;
	const bt_value *value = NULL;
	const char *path_param;

	ctf_fs = g_new0(struct ctf_fs_component, 1);
	if (!ctf_fs) {
		goto end;
	}

	bt_self_component_set_data(
		bt_self_component_source_as_self_component(self_comp),
		ctf_fs);

	/*
	 * We don't need to get a new reference here because as long as
	 * our private ctf_fs_component object exists, the containing
	 * private component should also exist.
	 */
	ctf_fs->self_comp = self_comp;
	value = bt_value_map_borrow_entry_value_const(params, "path");
	if (value && !bt_value_is_string(value)) {
		goto error;
	}

	path_param = bt_value_string_get(value);
	value = bt_value_map_borrow_entry_value_const(params,
		"clock-class-offset-s");
	if (value) {
		if (!bt_value_is_integer(value)) {
			BT_LOGE("clock-class-offset-s should be an integer");
			goto error;
		}
		ctf_fs->metadata_config.clock_class_offset_s = bt_value_integer_get(value);
	}

	value = bt_value_map_borrow_entry_value_const(params,
		"clock-class-offset-ns");
	if (value) {
		if (!bt_value_is_integer(value)) {
			BT_LOGE("clock-class-offset-ns should be an integer");
			goto error;
		}
		ctf_fs->metadata_config.clock_class_offset_ns = bt_value_integer_get(value);
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

	if (create_ctf_fs_traces(ctf_fs, path_param)) {
		goto error;
	}

	goto end;

error:
	ctf_fs_destroy(ctf_fs);
	ctf_fs = NULL;
	bt_self_component_set_data(
		bt_self_component_source_as_self_component(self_comp),
		NULL);

end:
	return ctf_fs;
}

BT_HIDDEN
enum bt_self_component_status ctf_fs_init(
		bt_self_component_source *self_comp,
		const bt_value *params, UNUSED_VAR void *init_method_data)
{
	struct ctf_fs_component *ctf_fs;
	enum bt_self_component_status ret = BT_SELF_COMPONENT_STATUS_OK;

	ctf_fs = ctf_fs_create(self_comp, params);
	if (!ctf_fs) {
		ret = BT_SELF_COMPONENT_STATUS_ERROR;
	}

	return ret;
}

BT_HIDDEN
enum bt_query_status ctf_fs_query(
		bt_self_component_class_source *comp_class,
		const bt_query_executor *query_exec,
		const char *object, const bt_value *params,
		const bt_value **result)
{
	enum bt_query_status status = BT_QUERY_STATUS_OK;

	if (!strcmp(object, "metadata-info")) {
		status = metadata_info_query(comp_class, params, result);
	} else if (!strcmp(object, "trace-info")) {
		status = trace_info_query(comp_class, params, result);
	} else {
		BT_LOGE("Unknown query object `%s`", object);
		status = BT_QUERY_STATUS_INVALID_OBJECT;
		goto end;
	}
end:
	return status;
}
