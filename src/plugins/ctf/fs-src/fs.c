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

#define BT_COMP_LOG_SELF_COMP self_comp
#define BT_LOG_OUTPUT_LEVEL log_level
#define BT_LOG_TAG "PLUGIN/SRC.CTF.FS"
#include "logging/comp-logging.h"

#include "common/common.h"
#include <babeltrace2/babeltrace.h>
#include "common/uuid.h"
#include <glib.h>
#include "common/assert.h"
#include <inttypes.h>
#include <stdbool.h>
#include "fs.h"
#include "metadata.h"
#include "data-stream-file.h"
#include "file.h"
#include "../common/metadata/decoder.h"
#include "../common/metadata/ctf-meta-configure-ir-trace.h"
#include "../common/msg-iter/msg-iter.h"
#include "query.h"
#include "plugins/common/param-validation/param-validation.h"

struct tracer_info {
	const char *name;
	int64_t major;
	int64_t minor;
	int64_t patch;
};

static
void ctf_fs_msg_iter_data_destroy(
		struct ctf_fs_msg_iter_data *msg_iter_data)
{
	if (!msg_iter_data) {
		return;
	}

	if (msg_iter_data->msg_iter) {
		ctf_msg_iter_destroy(msg_iter_data->msg_iter);
	}

	if (msg_iter_data->msg_iter_medops_data) {
		ctf_fs_ds_group_medops_data_destroy(
			msg_iter_data->msg_iter_medops_data);
	}

	g_free(msg_iter_data);
}

static
bt_message_iterator_class_next_method_status ctf_fs_iterator_next_one(
		struct ctf_fs_msg_iter_data *msg_iter_data,
		const bt_message **out_msg)
{
	bt_message_iterator_class_next_method_status status;
	enum ctf_msg_iter_status msg_iter_status;
	bt_logging_level log_level = msg_iter_data->log_level;

	msg_iter_status = ctf_msg_iter_get_next_message(
		msg_iter_data->msg_iter, out_msg);

	switch (msg_iter_status) {
	case CTF_MSG_ITER_STATUS_OK:
		/* Cool, message has been written to *out_msg. */
		status = BT_MESSAGE_ITERATOR_CLASS_NEXT_METHOD_STATUS_OK;
		break;

	case CTF_MSG_ITER_STATUS_EOF:
		status = BT_MESSAGE_ITERATOR_CLASS_NEXT_METHOD_STATUS_END;
		break;

	case CTF_MSG_ITER_STATUS_AGAIN:
		/*
		 * Should not make it this far as this is
		 * medium-specific; there is nothing for the user to do
		 * and it should have been handled upstream.
		 */
		bt_common_abort();

	case CTF_MSG_ITER_MEDIUM_STATUS_ERROR:
		BT_MSG_ITER_LOGE_APPEND_CAUSE(msg_iter_data->self_msg_iter,
			"Failed to get next message from CTF message iterator.");
		status = BT_MESSAGE_ITERATOR_CLASS_NEXT_METHOD_STATUS_ERROR;
		break;

	case CTF_MSG_ITER_MEDIUM_STATUS_MEMORY_ERROR:
		BT_MSG_ITER_LOGE_APPEND_CAUSE(msg_iter_data->self_msg_iter,
			"Failed to get next message from CTF message iterator.");
		status = BT_MESSAGE_ITERATOR_CLASS_NEXT_METHOD_STATUS_MEMORY_ERROR;
		break;

	default:
		bt_common_abort();
	}

	return status;
}

BT_HIDDEN
bt_message_iterator_class_next_method_status ctf_fs_iterator_next(
		bt_self_message_iterator *iterator,
		bt_message_array_const msgs, uint64_t capacity,
		uint64_t *count)
{
	bt_message_iterator_class_next_method_status status;
	struct ctf_fs_msg_iter_data *msg_iter_data =
		bt_self_message_iterator_get_data(iterator);
	uint64_t i = 0;

	if (G_UNLIKELY(msg_iter_data->next_saved_error)) {
		/*
		 * Last time we were called, we hit an error but had some
		 * messages to deliver, so we stashed the error here.  Return
		 * it now.
		 */
		BT_CURRENT_THREAD_MOVE_ERROR_AND_RESET(msg_iter_data->next_saved_error);
		status = msg_iter_data->next_saved_status;
		goto end;
	}

	do {
		status = ctf_fs_iterator_next_one(msg_iter_data, &msgs[i]);
		if (status == BT_MESSAGE_ITERATOR_CLASS_NEXT_METHOD_STATUS_OK) {
			i++;
		}
	} while (i < capacity &&
			status == BT_MESSAGE_ITERATOR_CLASS_NEXT_METHOD_STATUS_OK);

	if (i > 0) {
		/*
		 * Even if ctf_fs_iterator_next_one() returned something
		 * else than BT_MESSAGE_ITERATOR_NEXT_METHOD_STATUS_OK, we
		 * accumulated message objects in the output
		 * message array, so we need to return
		 * BT_MESSAGE_ITERATOR_NEXT_METHOD_STATUS_OK so that they are
		 * transfered to downstream. This other status occurs
		 * again the next time muxer_msg_iter_do_next() is
		 * called, possibly without any accumulated
		 * message, in which case we'll return it.
		 */
		if (status < 0) {
			/*
			 * Save this error for the next _next call.  Assume that
			 * this component always appends error causes when
			 * returning an error status code, which will cause the
			 * current thread error to be non-NULL.
			 */
			msg_iter_data->next_saved_error = bt_current_thread_take_error();
			BT_ASSERT(msg_iter_data->next_saved_error);
			msg_iter_data->next_saved_status = status;
		}

		*count = i;
		status = BT_MESSAGE_ITERATOR_CLASS_NEXT_METHOD_STATUS_OK;
	}

end:
	return status;
}

BT_HIDDEN
bt_message_iterator_class_seek_beginning_method_status
ctf_fs_iterator_seek_beginning(bt_self_message_iterator *it)
{
	struct ctf_fs_msg_iter_data *msg_iter_data =
		bt_self_message_iterator_get_data(it);

	BT_ASSERT(msg_iter_data);

	ctf_msg_iter_reset(msg_iter_data->msg_iter);
	ctf_fs_ds_group_medops_data_reset(msg_iter_data->msg_iter_medops_data);

	return BT_MESSAGE_ITERATOR_CLASS_SEEK_BEGINNING_METHOD_STATUS_OK;
}

BT_HIDDEN
void ctf_fs_iterator_finalize(bt_self_message_iterator *it)
{
	ctf_fs_msg_iter_data_destroy(
		bt_self_message_iterator_get_data(it));
}

BT_HIDDEN
bt_message_iterator_class_initialize_method_status ctf_fs_iterator_init(
		bt_self_message_iterator *self_msg_iter,
		bt_self_message_iterator_configuration *config,
		bt_self_component_port_output *self_port)
{
	struct ctf_fs_port_data *port_data;
	struct ctf_fs_msg_iter_data *msg_iter_data = NULL;
	bt_message_iterator_class_initialize_method_status status;
	bt_logging_level log_level;
	enum ctf_msg_iter_medium_status medium_status;
	bt_self_component *self_comp =
		bt_self_message_iterator_borrow_component(self_msg_iter);

	port_data = bt_self_component_port_get_data(
		bt_self_component_port_output_as_self_component_port(
			self_port));
	BT_ASSERT(port_data);
	log_level = port_data->ctf_fs->log_level;
	msg_iter_data = g_new0(struct ctf_fs_msg_iter_data, 1);
	if (!msg_iter_data) {
		status = BT_MESSAGE_ITERATOR_CLASS_INITIALIZE_METHOD_STATUS_MEMORY_ERROR;
		goto error;
	}

	msg_iter_data->log_level = log_level;
	msg_iter_data->self_comp = self_comp;
	msg_iter_data->self_msg_iter = self_msg_iter;
	msg_iter_data->ds_file_group = port_data->ds_file_group;

	medium_status = ctf_fs_ds_group_medops_data_create(
		msg_iter_data->ds_file_group, self_msg_iter, log_level,
		&msg_iter_data->msg_iter_medops_data);
	BT_ASSERT(
		medium_status == CTF_MSG_ITER_MEDIUM_STATUS_OK ||
		medium_status == CTF_MSG_ITER_MEDIUM_STATUS_ERROR ||
		medium_status == CTF_MSG_ITER_MEDIUM_STATUS_MEMORY_ERROR);
	if (medium_status != CTF_MSG_ITER_MEDIUM_STATUS_OK) {
		BT_MSG_ITER_LOGE_APPEND_CAUSE(self_msg_iter,
			"Failed to create ctf_fs_ds_group_medops");
		status = (int) medium_status;
		goto error;
	}

	msg_iter_data->msg_iter = ctf_msg_iter_create(
		msg_iter_data->ds_file_group->ctf_fs_trace->metadata->tc,
		bt_common_get_page_size(msg_iter_data->log_level) * 8,
		ctf_fs_ds_group_medops,
		msg_iter_data->msg_iter_medops_data,
		msg_iter_data->log_level,
		self_comp, self_msg_iter);
	if (!msg_iter_data->msg_iter) {
		BT_COMP_LOGE_APPEND_CAUSE(self_comp, "Cannot create a CTF message iterator.");
		status = BT_MESSAGE_ITERATOR_CLASS_INITIALIZE_METHOD_STATUS_MEMORY_ERROR;
		goto error;
	}

	/*
	 * This iterator can seek forward if its stream class has a default
	 * clock class.
	 */
	if (msg_iter_data->ds_file_group->sc->default_clock_class) {
		bt_self_message_iterator_configuration_set_can_seek_forward(
			config, true);
	}

	bt_self_message_iterator_set_data(self_msg_iter,
		msg_iter_data);
	msg_iter_data = NULL;

	status = BT_MESSAGE_ITERATOR_CLASS_INITIALIZE_METHOD_STATUS_OK;
	goto end;

error:
	bt_self_message_iterator_set_data(self_msg_iter, NULL);

end:
	ctf_fs_msg_iter_data_destroy(msg_iter_data);
	return status;
}

static
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

	if (ctf_fs_trace->metadata) {
		ctf_fs_metadata_fini(ctf_fs_trace->metadata);
		g_free(ctf_fs_trace->metadata);
	}

	g_free(ctf_fs_trace);
}

BT_HIDDEN
void ctf_fs_destroy(struct ctf_fs_component *ctf_fs)
{
	if (!ctf_fs) {
		return;
	}

	ctf_fs_trace_destroy(ctf_fs->trace);

	if (ctf_fs->port_data) {
		g_ptr_array_free(ctf_fs->port_data, TRUE);
	}

	g_free(ctf_fs);
}

static
void port_data_destroy(struct ctf_fs_port_data *port_data)
{
	if (!port_data) {
		return;
	}

	g_free(port_data);
}

static
void port_data_destroy_notifier(void *data) {
	port_data_destroy(data);
}

static
void ctf_fs_trace_destroy_notifier(void *data)
{
	struct ctf_fs_trace *trace = data;
	ctf_fs_trace_destroy(trace);
}

struct ctf_fs_component *ctf_fs_component_create(bt_logging_level log_level,
		bt_self_component *self_comp)
{
	struct ctf_fs_component *ctf_fs;

	ctf_fs = g_new0(struct ctf_fs_component, 1);
	if (!ctf_fs) {
		goto error;
	}

	ctf_fs->log_level = log_level;
	ctf_fs->port_data =
		g_ptr_array_new_with_free_func(port_data_destroy_notifier);
	if (!ctf_fs->port_data) {
		goto error;
	}

	goto end;

error:
	ctf_fs_destroy(ctf_fs);
	ctf_fs = NULL;

end:
	return ctf_fs;
}

void ctf_fs_finalize(bt_self_component_source *component)
{
	ctf_fs_destroy(bt_self_component_get_data(
		bt_self_component_source_as_self_component(component)));
}

gchar *ctf_fs_make_port_name(struct ctf_fs_ds_file_group *ds_file_group)
{
	GString *name = g_string_new(NULL);

	/*
	 * The unique port name is generated by concatenating unique identifiers
	 * for:
	 *
	 *   - the trace
	 *   - the stream class
	 *   - the stream
	 */

	/* For the trace, use the uuid if present, else the path. */
	if (ds_file_group->ctf_fs_trace->metadata->tc->is_uuid_set) {
		char uuid_str[BT_UUID_STR_LEN + 1];

		bt_uuid_to_str(ds_file_group->ctf_fs_trace->metadata->tc->uuid, uuid_str);
		g_string_assign(name, uuid_str);
	} else {
		g_string_assign(name, ds_file_group->ctf_fs_trace->path->str);
	}

	/*
	 * For the stream class, use the id if present.  We can omit this field
	 * otherwise, as there will only be a single stream class.
	 */
	if (ds_file_group->sc->id != UINT64_C(-1)) {
		g_string_append_printf(name, " | %" PRIu64, ds_file_group->sc->id);
	}

	/* For the stream, use the id if present, else, use the path. */
	if (ds_file_group->stream_id != UINT64_C(-1)) {
		g_string_append_printf(name, " | %" PRIu64, ds_file_group->stream_id);
	} else {
		BT_ASSERT(ds_file_group->ds_file_infos->len == 1);
		struct ctf_fs_ds_file_info *ds_file_info =
			g_ptr_array_index(ds_file_group->ds_file_infos, 0);
		g_string_append_printf(name, " | %s", ds_file_info->path->str);
	}

	return g_string_free(name, FALSE);
}

static
int create_one_port_for_trace(struct ctf_fs_component *ctf_fs,
		struct ctf_fs_trace *ctf_fs_trace,
		struct ctf_fs_ds_file_group *ds_file_group,
		bt_self_component_source *self_comp_src)
{
	int ret = 0;
	struct ctf_fs_port_data *port_data = NULL;
	gchar *port_name;
	bt_logging_level log_level = ctf_fs->log_level;
	bt_self_component *self_comp =
		bt_self_component_source_as_self_component(self_comp_src);

	port_name = ctf_fs_make_port_name(ds_file_group);
	if (!port_name) {
		goto error;
	}

	BT_COMP_LOGI("Creating one port named `%s`", port_name);

	/* Create output port for this file */
	port_data = g_new0(struct ctf_fs_port_data, 1);
	if (!port_data) {
		goto error;
	}

	port_data->ctf_fs = ctf_fs;
	port_data->ds_file_group = ds_file_group;
	ret = bt_self_component_source_add_output_port(
		self_comp_src, port_name, port_data, NULL);
	if (ret) {
		goto error;
	}

	g_ptr_array_add(ctf_fs->port_data, port_data);
	port_data = NULL;
	goto end;

error:
	ret = -1;

end:
	g_free(port_name);

	port_data_destroy(port_data);
	return ret;
}

static
int create_ports_for_trace(struct ctf_fs_component *ctf_fs,
		struct ctf_fs_trace *ctf_fs_trace,
		bt_self_component_source *self_comp_src)
{
	int ret = 0;
	size_t i;
	bt_logging_level log_level = ctf_fs_trace->log_level;
	bt_self_component *self_comp =
		bt_self_component_source_as_self_component(self_comp_src);

	/* Create one output port for each stream file group */
	for (i = 0; i < ctf_fs_trace->ds_file_groups->len; i++) {
		struct ctf_fs_ds_file_group *ds_file_group =
			g_ptr_array_index(ctf_fs_trace->ds_file_groups, i);

		ret = create_one_port_for_trace(ctf_fs, ctf_fs_trace,
			ds_file_group, self_comp_src);
		if (ret) {
			BT_COMP_LOGE_APPEND_CAUSE(self_comp, "Cannot create output port.");
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

	g_free(ds_file_info);
}

static
struct ctf_fs_ds_file_info *ctf_fs_ds_file_info_create(const char *path,
		int64_t begin_ns)
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

end:
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

	ctf_fs_ds_index_destroy(ds_file_group->index);

	bt_stream_put_ref(ds_file_group->stream);
	g_free(ds_file_group);
}

static
struct ctf_fs_ds_file_group *ctf_fs_ds_file_group_create(
		struct ctf_fs_trace *ctf_fs_trace,
		struct ctf_stream_class *sc,
		uint64_t stream_instance_id,
		struct ctf_fs_ds_index *index)
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

	ds_file_group->index = index;

	ds_file_group->stream_id = stream_instance_id;
	BT_ASSERT(sc);
	ds_file_group->sc = sc;
	ds_file_group->ctf_fs_trace = ctf_fs_trace;
	goto end;

error:
	ctf_fs_ds_file_group_destroy(ds_file_group);
	ctf_fs_ds_index_destroy(index);
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

	/* Insert the value. */
	array->pdata[pos] = element;
}

/*
 * Insert ds_file_info in ds_file_group's list of ds_file_infos at the right
 * place to keep it sorted.
 */

static
void ds_file_group_insert_ds_file_info_sorted(
		struct ctf_fs_ds_file_group *ds_file_group,
		struct ctf_fs_ds_file_info *ds_file_info)
{
	guint i;

	/* Find the spot where to insert this ds_file_info. */
	for (i = 0; i < ds_file_group->ds_file_infos->len; i++) {
		struct ctf_fs_ds_file_info *other_ds_file_info =
			g_ptr_array_index(ds_file_group->ds_file_infos, i);

		if (ds_file_info->begin_ns < other_ds_file_info->begin_ns) {
			break;
		}
	}

	array_insert(ds_file_group->ds_file_infos, ds_file_info, i);
}

static
bool ds_index_entries_equal(
	const struct ctf_fs_ds_index_entry *left,
	const struct ctf_fs_ds_index_entry *right)
{
	if (left->packet_size != right->packet_size) {
		return false;
	}

	if (left->timestamp_begin != right->timestamp_begin) {
		return false;
	}

	if (left->timestamp_end != right->timestamp_end) {
		return false;
	}

	if (left->packet_seq_num != right->packet_seq_num) {
		return false;
	}

	return true;
}

/*
 * Insert `entry` into `index`, without duplication.
 *
 * The entry is inserted only if there isn't an identical entry already.
 *
 * In any case, the ownership of `entry` is transferred to this function.  So if
 * the entry is not inserted, it is freed.
 */

static
void ds_index_insert_ds_index_entry_sorted(
	struct ctf_fs_ds_index *index,
	struct ctf_fs_ds_index_entry *entry)
{
	guint i;
	struct ctf_fs_ds_index_entry *other_entry = NULL;

	/* Find the spot where to insert this index entry. */
	for (i = 0; i < index->entries->len; i++) {
		other_entry = g_ptr_array_index(index->entries, i);

		if (entry->timestamp_begin_ns <= other_entry->timestamp_begin_ns) {
			break;
		}
	}

	/*
	 * Insert the entry only if a duplicate doesn't already exist.
	 *
	 * There can be duplicate packets if reading multiple overlapping
	 * snapshots of the same trace.  We then want the index to contain
	 * a reference to only one copy of that packet.
	 */
	if (i == index->entries->len ||
			!ds_index_entries_equal(entry, other_entry)) {
		array_insert(index->entries, entry, i);
	} else {
		g_free(entry);
	}
}

static
void merge_ctf_fs_ds_indexes(struct ctf_fs_ds_index *dest, struct ctf_fs_ds_index *src)
{
	guint i;

	for (i = 0; i < src->entries->len; i++) {
		struct ctf_fs_ds_index_entry *entry =
			g_ptr_array_index(src->entries, i);

		/*
		* Ownership of the ctf_fs_ds_index_entry is transferred to
		* ds_index_insert_ds_index_entry_sorted.
		*/
		g_ptr_array_index(src->entries, i) = NULL;
		ds_index_insert_ds_index_entry_sorted(dest, entry);
	}
}

static
int add_ds_file_to_ds_file_group(struct ctf_fs_trace *ctf_fs_trace,
		const char *path)
{
	int64_t stream_instance_id = -1;
	int64_t begin_ns = -1;
	struct ctf_fs_ds_file_group *ds_file_group = NULL;
	bool add_group = false;
	int ret;
	size_t i;
	struct ctf_fs_ds_file *ds_file = NULL;
	struct ctf_fs_ds_file_info *ds_file_info = NULL;
	struct ctf_fs_ds_index *index = NULL;
	struct ctf_msg_iter *msg_iter = NULL;
	struct ctf_stream_class *sc = NULL;
	struct ctf_msg_iter_packet_properties props;
	bt_logging_level log_level = ctf_fs_trace->log_level;
	bt_self_component *self_comp = ctf_fs_trace->self_comp;
	bt_self_component_class *self_comp_class = ctf_fs_trace->self_comp_class;

	/*
	 * Create a temporary ds_file to read some properties about the data
	 * stream file.
	 */
	ds_file = ctf_fs_ds_file_create(ctf_fs_trace, NULL, NULL, path,
		log_level);
	if (!ds_file) {
		goto error;
	}

	/* Create a temporary iterator to read the ds_file. */
	msg_iter = ctf_msg_iter_create(ctf_fs_trace->metadata->tc,
		bt_common_get_page_size(log_level) * 8,
		ctf_fs_ds_file_medops, ds_file, log_level, self_comp, NULL);
	if (!msg_iter) {
		BT_COMP_LOGE_STR("Cannot create a CTF message iterator.");
		goto error;
	}

	ctf_msg_iter_set_dry_run(msg_iter, true);

	ret = ctf_msg_iter_get_packet_properties(msg_iter, &props);
	if (ret) {
		BT_COMP_OR_COMP_CLASS_LOGE_APPEND_CAUSE(self_comp, self_comp_class,
			"Cannot get stream file's first packet's header and context fields (`%s`).",
			path);
		goto error;
	}

	sc = ctf_trace_class_borrow_stream_class_by_id(ds_file->metadata->tc,
		props.stream_class_id);
	BT_ASSERT(sc);
	stream_instance_id = props.data_stream_id;

	if (props.snapshots.beginning_clock != UINT64_C(-1)) {
		BT_ASSERT(sc->default_clock_class);
		ret = bt_util_clock_cycles_to_ns_from_origin(
			props.snapshots.beginning_clock,
			sc->default_clock_class->frequency,
			sc->default_clock_class->offset_seconds,
			sc->default_clock_class->offset_cycles, &begin_ns);
		if (ret) {
			BT_COMP_OR_COMP_CLASS_LOGE_APPEND_CAUSE(self_comp, self_comp_class,
				"Cannot convert clock cycles to nanoseconds from origin (`%s`).",
				path);
			goto error;
		}
	}

	ds_file_info = ctf_fs_ds_file_info_create(path, begin_ns);
	if (!ds_file_info) {
		goto error;
	}

	index = ctf_fs_ds_file_build_index(ds_file, ds_file_info, msg_iter);
	if (!index) {
		BT_COMP_OR_COMP_CLASS_LOGE_APPEND_CAUSE(
			self_comp, self_comp_class,
			"Failed to index CTF stream file \'%s\'",
			ds_file->file->path->str);
		goto error;
	}

	if (begin_ns == -1) {
		/*
		 * No beginning timestamp to sort the stream files
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
			sc, UINT64_C(-1), index);
		/* Ownership of index is transferred. */
		index = NULL;

		if (!ds_file_group) {
			goto error;
		}

		ds_file_group_insert_ds_file_info_sorted(ds_file_group,
			BT_MOVE_REF(ds_file_info));

		add_group = true;
		goto end;
	}

	BT_ASSERT(stream_instance_id != -1);
	BT_ASSERT(begin_ns != -1);

	/* Find an existing stream file group with this ID */
	for (i = 0; i < ctf_fs_trace->ds_file_groups->len; i++) {
		ds_file_group = g_ptr_array_index(
			ctf_fs_trace->ds_file_groups, i);

		if (ds_file_group->sc == sc &&
				ds_file_group->stream_id ==
				stream_instance_id) {
			break;
		}

		ds_file_group = NULL;
	}

	if (!ds_file_group) {
		ds_file_group = ctf_fs_ds_file_group_create(ctf_fs_trace,
			sc, stream_instance_id, index);
		/* Ownership of index is transferred. */
		index = NULL;
		if (!ds_file_group) {
			goto error;
		}

		add_group = true;
	} else {
		merge_ctf_fs_ds_indexes(ds_file_group->index, index);
	}

	ds_file_group_insert_ds_file_info_sorted(ds_file_group,
		BT_MOVE_REF(ds_file_info));

	goto end;

error:
	ctf_fs_ds_file_group_destroy(ds_file_group);
	ds_file_group = NULL;
	ret = -1;

end:
	if (add_group && ds_file_group) {
		g_ptr_array_add(ctf_fs_trace->ds_file_groups, ds_file_group);
	}

	ctf_fs_ds_file_destroy(ds_file);
	ctf_fs_ds_file_info_destroy(ds_file_info);

	if (msg_iter) {
		ctf_msg_iter_destroy(msg_iter);
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
	bt_logging_level log_level = ctf_fs_trace->log_level;
	bt_self_component *self_comp = ctf_fs_trace->self_comp;
	bt_self_component_class *self_comp_class = ctf_fs_trace->self_comp_class;

	/* Check each file in the path directory, except specific ones */
	dir = g_dir_open(ctf_fs_trace->path->str, 0, &error);
	if (!dir) {
		BT_COMP_OR_COMP_CLASS_LOGE_APPEND_CAUSE(self_comp, self_comp_class,
			"Cannot open directory `%s`: %s (code %d)",
			ctf_fs_trace->path->str, error->message,
			error->code);
		goto error;
	}

	while ((basename = g_dir_read_name(dir))) {
		struct ctf_fs_file *file;

		if (strcmp(basename, CTF_FS_METADATA_FILENAME) == 0) {
			/* Ignore the metadata stream. */
			BT_COMP_LOGI("Ignoring metadata file `%s" G_DIR_SEPARATOR_S "%s`",
				ctf_fs_trace->path->str, basename);
			continue;
		}

		if (basename[0] == '.') {
			BT_COMP_LOGI("Ignoring hidden file `%s" G_DIR_SEPARATOR_S "%s`",
				ctf_fs_trace->path->str, basename);
			continue;
		}

		/* Create the file. */
		file = ctf_fs_file_create(log_level, self_comp);
		if (!file) {
			BT_COMP_OR_COMP_CLASS_LOGE_APPEND_CAUSE(self_comp, self_comp_class,
				"Cannot create stream file object for file `%s" G_DIR_SEPARATOR_S "%s`",
				ctf_fs_trace->path->str, basename);
			goto error;
		}

		/* Create full path string. */
		g_string_append_printf(file->path, "%s" G_DIR_SEPARATOR_S "%s",
				ctf_fs_trace->path->str, basename);
		if (!g_file_test(file->path->str, G_FILE_TEST_IS_REGULAR)) {
			BT_COMP_LOGI("Ignoring non-regular file `%s`",
				file->path->str);
			ctf_fs_file_destroy(file);
			file = NULL;
			continue;
		}

		ret = ctf_fs_file_open(file, "rb");
		if (ret) {
			BT_COMP_OR_COMP_CLASS_LOGE_APPEND_CAUSE(self_comp, self_comp_class,
				"Cannot open stream file `%s`",
				file->path->str);
			goto error;
		}

		if (file->size == 0) {
			/* Skip empty stream. */
			BT_COMP_LOGI("Ignoring empty file `%s`", file->path->str);
			ctf_fs_file_destroy(file);
			continue;
		}

		ret = add_ds_file_to_ds_file_group(ctf_fs_trace,
			file->path->str);
		if (ret) {
			BT_COMP_OR_COMP_CLASS_LOGE_APPEND_CAUSE(self_comp, self_comp_class,
				"Cannot add stream file `%s` to stream file group",
				file->path->str);
			ctf_fs_file_destroy(file);
			goto error;
		}

		ctf_fs_file_destroy(file);
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
int set_trace_name(bt_trace *trace, const char *name_suffix,
		bt_logging_level log_level, bt_self_component *self_comp)
{
	int ret = 0;
	const bt_value *val;
	GString *name;

	name = g_string_new(NULL);
	if (!name) {
		BT_COMP_LOGE_STR("Failed to allocate a GString.");
		ret = -1;
		goto end;
	}

	/*
	 * Check if we have a trace environment string value named `hostname`.
	 * If so, use it as the trace name's prefix.
	 */
	val = bt_trace_borrow_environment_entry_value_by_name_const(
		trace, "hostname");
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

static
struct ctf_fs_trace *ctf_fs_trace_create(
		bt_self_component *self_comp,
		bt_self_component_class *self_comp_class,
		const char *path, const char *name,
		struct ctf_fs_metadata_config *metadata_config,
		bt_logging_level log_level)
{
	struct ctf_fs_trace *ctf_fs_trace;
	int ret;

	/* Only one of them must be set. */
	BT_ASSERT(!self_comp != !self_comp_class);

	ctf_fs_trace = g_new0(struct ctf_fs_trace, 1);
	if (!ctf_fs_trace) {
		goto end;
	}

	ctf_fs_trace->log_level = log_level;
	ctf_fs_trace->self_comp = self_comp;
	ctf_fs_trace->self_comp_class = self_comp_class;
	ctf_fs_trace->path = g_string_new(path);
	if (!ctf_fs_trace->path) {
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

	ret = ctf_fs_metadata_set_trace_class(self_comp, ctf_fs_trace,
		metadata_config);
	if (ret) {
		goto error;
	}

	if (ctf_fs_trace->metadata->trace_class) {
		ctf_fs_trace->trace =
			bt_trace_create(ctf_fs_trace->metadata->trace_class);
		if (!ctf_fs_trace->trace) {
			goto error;
		}
	}

	if (ctf_fs_trace->trace) {
		ret = ctf_trace_class_configure_ir_trace(
			ctf_fs_trace->metadata->tc, ctf_fs_trace->trace);
		if (ret) {
			goto error;
		}

		ret = set_trace_name(ctf_fs_trace->trace, name, log_level,
			self_comp);
		if (ret) {
			goto error;
		}
	}

	ret = create_ds_file_groups(ctf_fs_trace);
	if (ret) {
		goto error;
	}

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

/* Helper for ctf_fs_component_create_ctf_fs_trace, to handle a single path. */

static
int ctf_fs_component_create_ctf_fs_trace_one_path(
		struct ctf_fs_component *ctf_fs,
		const char *path_param,
		const char *trace_name,
		GPtrArray *traces,
		bt_self_component *self_comp,
		bt_self_component_class *self_comp_class)
{
	struct ctf_fs_trace *ctf_fs_trace;
	int ret;
	GString *norm_path;
	bt_logging_level log_level = ctf_fs->log_level;

	norm_path = bt_common_normalize_path(path_param, NULL);
	if (!norm_path) {
		BT_COMP_OR_COMP_CLASS_LOGE_APPEND_CAUSE(self_comp, self_comp_class,
			"Failed to normalize path: `%s`.",
			path_param);
		goto error;
	}

	ret = path_is_ctf_trace(norm_path->str);
	if (ret < 0) {
		BT_COMP_OR_COMP_CLASS_LOGE_APPEND_CAUSE(self_comp, self_comp_class,
			"Failed to check if path is a CTF trace: path=%s",
			norm_path->str);
		goto error;
	} else if (ret == 0) {
		BT_COMP_OR_COMP_CLASS_LOGE_APPEND_CAUSE(self_comp, self_comp_class,
			"Path is not a CTF trace (does not contain a metadata file): `%s`.",
			norm_path->str);
		goto error;
	}

	// FIXME: Remove or ifdef for __MINGW32__
	if (strcmp(norm_path->str, "/") == 0) {
		BT_COMP_OR_COMP_CLASS_LOGE_APPEND_CAUSE(self_comp, self_comp_class,
			"Opening a trace in `/` is not supported.");
		ret = -1;
		goto end;
	}

	ctf_fs_trace = ctf_fs_trace_create(self_comp, self_comp_class, norm_path->str,
		trace_name, &ctf_fs->metadata_config, log_level);
	if (!ctf_fs_trace) {
		BT_COMP_OR_COMP_CLASS_LOGE_APPEND_CAUSE(self_comp, self_comp_class,
			"Cannot create trace for `%s`.",
			norm_path->str);
		goto error;
	}

	g_ptr_array_add(traces, ctf_fs_trace);
	ctf_fs_trace = NULL;

	ret = 0;
	goto end;

error:
	ret = -1;

end:
	if (norm_path) {
		g_string_free(norm_path, TRUE);
	}

	return ret;
}

/*
 * Count the number of stream and event classes defined by this trace's metadata.
 *
 * This is used to determine which metadata is the "latest", out of multiple
 * traces sharing the same UUID.  It is assumed that amongst all these metadatas,
 * a bigger metadata is a superset of a smaller metadata.  Therefore, it is
 * enough to just count the classes.
 */

static
unsigned int metadata_count_stream_and_event_classes(struct ctf_fs_trace *trace)
{
	unsigned int num = trace->metadata->tc->stream_classes->len;
	guint i;

	for (i = 0; i < trace->metadata->tc->stream_classes->len; i++) {
		struct ctf_stream_class *sc = trace->metadata->tc->stream_classes->pdata[i];
		num += sc->event_classes->len;
	}

	return num;
}

/*
 * Merge the src ds_file_group into dest.  This consists of merging their
 * ds_file_infos, making sure to keep the result sorted.
 */

static
void merge_ctf_fs_ds_file_groups(struct ctf_fs_ds_file_group *dest, struct ctf_fs_ds_file_group *src)
{
	guint i;

	for (i = 0; i < src->ds_file_infos->len; i++) {
		struct ctf_fs_ds_file_info *ds_file_info =
			g_ptr_array_index(src->ds_file_infos, i);

		/* Ownership of the ds_file_info is transferred to dest. */
		g_ptr_array_index(src->ds_file_infos, i) = NULL;

		ds_file_group_insert_ds_file_info_sorted(dest, ds_file_info);
	}

	/* Merge both indexes. */
	merge_ctf_fs_ds_indexes(dest->index, src->index);
}
/* Merge src_trace's data stream file groups into dest_trace's. */

static
int merge_matching_ctf_fs_ds_file_groups(
		struct ctf_fs_trace *dest_trace,
		struct ctf_fs_trace *src_trace)
{

	GPtrArray *dest = dest_trace->ds_file_groups;
	GPtrArray *src = src_trace->ds_file_groups;
	guint s_i;
	int ret = 0;

	/*
	 * Save the initial length of dest: we only want to check against the
	 * original elements in the inner loop.
	 */
	const guint dest_len = dest->len;

	for (s_i = 0; s_i < src->len; s_i++) {
		struct ctf_fs_ds_file_group *src_group = g_ptr_array_index(src, s_i);
		struct ctf_fs_ds_file_group *dest_group = NULL;

		/* A stream instance without ID can't match a stream in the other trace.  */
		if (src_group->stream_id != -1) {
			guint d_i;

			/* Let's search for a matching ds_file_group in the destination.  */
			for (d_i = 0; d_i < dest_len; d_i++) {
				struct ctf_fs_ds_file_group *candidate_dest = g_ptr_array_index(dest, d_i);

				/* Can't match a stream instance without ID.  */
				if (candidate_dest->stream_id == -1) {
					continue;
				}

				/*
				 * If the two groups have the same stream instance id
				 * and belong to the same stream class (stream instance
				 * ids are per-stream class), they represent the same
				 * stream instance.
				 */
				if (candidate_dest->stream_id != src_group->stream_id ||
						candidate_dest->sc->id != src_group->sc->id) {
					continue;
				}

				dest_group = candidate_dest;
				break;
			}
		}

		/*
		 * Didn't find a friend in dest to merge our src_group into?
		 * Create a new empty one. This can happen if a stream was
		 * active in the source trace chunk but not in the destination
		 * trace chunk.
		 */
		if (!dest_group) {
			struct ctf_stream_class *sc;
			struct ctf_fs_ds_index *index;

			sc = ctf_trace_class_borrow_stream_class_by_id(
				dest_trace->metadata->tc, src_group->sc->id);
			BT_ASSERT(sc);

			index = ctf_fs_ds_index_create(dest_trace->log_level,
				dest_trace->self_comp);
			if (!index) {
				ret = -1;
				goto end;
			}

			dest_group = ctf_fs_ds_file_group_create(dest_trace, sc,
				src_group->stream_id, index);
			/* Ownership of index is transferred. */
			index = NULL;
			if (!dest_group) {
				ret = -1;
				goto end;
			}

			g_ptr_array_add(dest_trace->ds_file_groups, dest_group);
		}

		BT_ASSERT(dest_group);
		merge_ctf_fs_ds_file_groups(dest_group, src_group);
	}

end:
	return ret;
}

/*
 * Collapse the given traces, which must all share the same UUID, in a single
 * one.
 *
 * The trace with the most expansive metadata is chosen and all other traces
 * are merged into that one.  The array slots of all the traces that get merged
 * in the chosen one are set to NULL, so only the slot of the chosen trace
 * remains non-NULL.
 */

static
int merge_ctf_fs_traces(struct ctf_fs_trace **traces, unsigned int num_traces,
		struct ctf_fs_trace **out_trace)
{
	unsigned int winner_count;
	struct ctf_fs_trace *winner;
	guint i, winner_i;
	int ret = 0;

	BT_ASSERT(num_traces >= 2);

	winner_count = metadata_count_stream_and_event_classes(traces[0]);
	winner = traces[0];
	winner_i = 0;

	/* Find the trace with the largest metadata. */
	for (i = 1; i < num_traces; i++) {
		struct ctf_fs_trace *candidate;
		unsigned int candidate_count;

		candidate = traces[i];

		/* A bit of sanity check. */
		BT_ASSERT(bt_uuid_compare(winner->metadata->tc->uuid, candidate->metadata->tc->uuid) == 0);

		candidate_count = metadata_count_stream_and_event_classes(candidate);

		if (candidate_count > winner_count) {
			winner_count = candidate_count;
			winner = candidate;
			winner_i = i;
		}
	}

	/* Merge all the other traces in the winning trace. */
	for (i = 0; i < num_traces; i++) {
		struct ctf_fs_trace *trace = traces[i];

		/* Don't merge the winner into itself. */
		if (trace == winner) {
			continue;
		}

		/* Merge trace's data stream file groups into winner's. */
		ret = merge_matching_ctf_fs_ds_file_groups(winner, trace);
		if (ret) {
			goto end;
		}
	}

	/*
	 * Move the winner out of the array, into `*out_trace`.
	 */
	*out_trace = winner;
	traces[winner_i] = NULL;

end:
	return ret;
}

enum target_event {
	FIRST_EVENT,
	LAST_EVENT,
};

static
int decode_clock_snapshot_after_event(struct ctf_fs_trace *ctf_fs_trace,
	struct ctf_clock_class *default_cc,
	struct ctf_fs_ds_index_entry *index_entry,
	enum target_event target_event, uint64_t *cs, int64_t *ts_ns)
{
	enum ctf_msg_iter_status iter_status = CTF_MSG_ITER_STATUS_OK;
	struct ctf_fs_ds_file *ds_file = NULL;
	struct ctf_msg_iter *msg_iter = NULL;
	bt_logging_level log_level = ctf_fs_trace->log_level;
	bt_self_component *self_comp = ctf_fs_trace->self_comp;
	int ret = 0;

	BT_ASSERT(ctf_fs_trace);
	BT_ASSERT(index_entry);
	BT_ASSERT(index_entry->path);

	ds_file = ctf_fs_ds_file_create(ctf_fs_trace, NULL,
		NULL, index_entry->path, log_level);
	if (!ds_file) {
		BT_COMP_LOGE_APPEND_CAUSE(self_comp, "Failed to create a ctf_fs_ds_file");
		ret = -1;
		goto end;
	}

	BT_ASSERT(ctf_fs_trace->metadata);
	BT_ASSERT(ctf_fs_trace->metadata->tc);

	msg_iter = ctf_msg_iter_create(ctf_fs_trace->metadata->tc,
		bt_common_get_page_size(log_level) * 8, ctf_fs_ds_file_medops,
		ds_file, log_level, self_comp, NULL);
	if (!msg_iter) {
		/* ctf_msg_iter_create() logs errors. */
		ret = -1;
		goto end;
	}

	/*
	 * Turn on dry run mode to prevent the creation and usage of Babeltrace
	 * library objects (bt_field, bt_message_*, etc.).
	 */
	ctf_msg_iter_set_dry_run(msg_iter, true);

	/* Seek to the beginning of the target packet. */
	iter_status = ctf_msg_iter_seek(msg_iter, index_entry->offset);
	if (iter_status) {
		/* ctf_msg_iter_seek() logs errors. */
		ret = -1;
		goto end;
	}

	switch (target_event) {
	case FIRST_EVENT:
		/*
		 * Start to decode the packet until we reach the end of
		 * the first event. To extract the first event's clock
		 * snapshot.
		 */
		iter_status = ctf_msg_iter_curr_packet_first_event_clock_snapshot(
			msg_iter, cs);
		break;
	case LAST_EVENT:
		/* Decode the packet to extract the last event's clock snapshot. */
		iter_status = ctf_msg_iter_curr_packet_last_event_clock_snapshot(
			msg_iter, cs);
		break;
	default:
		bt_common_abort();
	}
	if (iter_status) {
		ret = -1;
		goto end;
	}

	/* Convert clock snapshot to timestamp. */
	ret = bt_util_clock_cycles_to_ns_from_origin(*cs,
		default_cc->frequency, default_cc->offset_seconds,
		default_cc->offset_cycles, ts_ns);
	if (ret) {
		BT_COMP_LOGE_APPEND_CAUSE(self_comp,
			"Failed to convert clock snapshot to timestamp");
		goto end;
	}

end:
	if (ds_file) {
		ctf_fs_ds_file_destroy(ds_file);
	}
	if (msg_iter) {
		ctf_msg_iter_destroy(msg_iter);
	}

	return ret;
}

static
int decode_packet_first_event_timestamp(struct ctf_fs_trace *ctf_fs_trace,
	struct ctf_clock_class *default_cc,
	struct ctf_fs_ds_index_entry *index_entry, uint64_t *cs, int64_t *ts_ns)
{
	return decode_clock_snapshot_after_event(ctf_fs_trace, default_cc,
		index_entry, FIRST_EVENT, cs, ts_ns);
}

static
int decode_packet_last_event_timestamp(struct ctf_fs_trace *ctf_fs_trace,
	struct ctf_clock_class *default_cc,
	struct ctf_fs_ds_index_entry *index_entry, uint64_t *cs, int64_t *ts_ns)
{
	return decode_clock_snapshot_after_event(ctf_fs_trace, default_cc,
		index_entry, LAST_EVENT, cs, ts_ns);
}

/*
 * Fix up packet index entries for lttng's "event-after-packet" bug.
 * Some buggy lttng tracer versions may emit events with a timestamp that is
 * larger (after) than the timestamp_end of the their packets.
 *
 * To fix up this erroneous data we do the following:
 *  1. If it's not the stream file's last packet: set the packet index entry's
 *	end time to the next packet's beginning time.
 *  2. If it's the stream file's last packet, set the packet index entry's end
 *	time to the packet's last event's time, if any, or to the packet's
 *  	beginning time otherwise.
 *
 * Known buggy tracer versions:
 *  - before lttng-ust 2.11.0
 *  - before lttng-module 2.11.0
 *  - before lttng-module 2.10.10
 *  - before lttng-module 2.9.13
 */
static
int fix_index_lttng_event_after_packet_bug(struct ctf_fs_trace *trace)
{
	int ret = 0;
	guint ds_file_group_i;
	GPtrArray *ds_file_groups = trace->ds_file_groups;
	bt_logging_level log_level = trace->log_level;

	for (ds_file_group_i = 0; ds_file_group_i < ds_file_groups->len;
			ds_file_group_i++) {
		guint entry_i;
		struct ctf_clock_class *default_cc;
		struct ctf_fs_ds_index_entry *last_entry;
		struct ctf_fs_ds_index *index;

		struct ctf_fs_ds_file_group *ds_file_group =
			g_ptr_array_index(ds_file_groups, ds_file_group_i);

		BT_ASSERT(ds_file_group);
		index = ds_file_group->index;

		BT_ASSERT(index);
		BT_ASSERT(index->entries);
		BT_ASSERT(index->entries->len > 0);

		/*
		 * Iterate over all entries but the last one. The last one is
		 * fixed differently after.
		 */
		for (entry_i = 0; entry_i < index->entries->len - 1;
				entry_i++) {
			struct ctf_fs_ds_index_entry *curr_entry, *next_entry;

			curr_entry = g_ptr_array_index(index->entries, entry_i);
			next_entry = g_ptr_array_index(index->entries, entry_i + 1);

			/*
			 * 1. Set the current index entry `end` timestamp to
			 * the next index entry `begin` timestamp.
			 */
			curr_entry->timestamp_end = next_entry->timestamp_begin;
			curr_entry->timestamp_end_ns = next_entry->timestamp_begin_ns;
		}

		/*
		 * 2. Fix the last entry by decoding the last event of the last
		 * packet.
		 */
		last_entry = g_ptr_array_index(index->entries,
			index->entries->len - 1);
		BT_ASSERT(last_entry);

		BT_ASSERT(ds_file_group->sc->default_clock_class);
		default_cc = ds_file_group->sc->default_clock_class;

		/*
		 * Decode packet to read the timestamp of the last event of the
		 * entry.
		 */
		ret = decode_packet_last_event_timestamp(trace, default_cc,
			last_entry, &last_entry->timestamp_end,
			&last_entry->timestamp_end_ns);
		if (ret) {
			BT_COMP_LOGE_APPEND_CAUSE(trace->self_comp,
				"Failed to decode stream's last packet to get its last event's clock snapshot.");
			goto end;
		}
	}

end:
	return ret;
}

/*
 * Fix up packet index entries for barectf's "event-before-packet" bug.
 * Some buggy barectf tracer versions may emit events with a timestamp that is
 * less than the timestamp_begin of the their packets.
 *
 * To fix up this erroneous data we do the following:
 *  1. Starting at the second index entry, set the timestamp_begin of the
 *     current entry to the timestamp of the first event of the packet.
 *  2. Set the previous entry's timestamp_end to the timestamp_begin of the
 *     current packet.
 *
 * Known buggy tracer versions:
 *  - before barectf 2.3.1
 */
static
int fix_index_barectf_event_before_packet_bug(struct ctf_fs_trace *trace)
{
	int ret = 0;
	guint ds_file_group_i;
	GPtrArray *ds_file_groups = trace->ds_file_groups;
	bt_logging_level log_level = trace->log_level;

	for (ds_file_group_i = 0; ds_file_group_i < ds_file_groups->len;
			ds_file_group_i++) {
		guint entry_i;
		struct ctf_clock_class *default_cc;
		struct ctf_fs_ds_file_group *ds_file_group =
			g_ptr_array_index(ds_file_groups, ds_file_group_i);

		struct ctf_fs_ds_index *index = ds_file_group->index;

		BT_ASSERT(index);
		BT_ASSERT(index->entries);
		BT_ASSERT(index->entries->len > 0);

		BT_ASSERT(ds_file_group->sc->default_clock_class);
		default_cc = ds_file_group->sc->default_clock_class;

		/*
		 * 1. Iterate over the index, starting from the second entry
		 * (index = 1).
		 */
		for (entry_i = 1; entry_i < index->entries->len;
				entry_i++) {
			struct ctf_fs_ds_index_entry *curr_entry, *prev_entry;
			prev_entry = g_ptr_array_index(index->entries, entry_i - 1);
			curr_entry = g_ptr_array_index(index->entries, entry_i);
			/*
			 * 2. Set the current entry `begin` timestamp to the
			 * timestamp of the first event of the current packet.
			 */
			ret = decode_packet_first_event_timestamp(trace, default_cc,
				curr_entry, &curr_entry->timestamp_begin,
				&curr_entry->timestamp_begin_ns);
			if (ret) {
				BT_COMP_LOGE_APPEND_CAUSE(trace->self_comp,
					"Failed to decode first event's clock snapshot");
				goto end;
			}

			/*
			 * 3. Set the previous entry `end` timestamp to the
			 * timestamp of the first event of the current packet.
			 */
			prev_entry->timestamp_end = curr_entry->timestamp_begin;
			prev_entry->timestamp_end_ns = curr_entry->timestamp_begin_ns;
		}
	}
end:
	return ret;
}

/*
 * When using the lttng-crash feature it's likely that the last packets of each
 * stream have their timestamp_end set to zero. This is caused by the fact that
 * the tracer crashed and was not able to properly close the packets.
 *
 * To fix up this erroneous data we do the following:
 * For each index entry, if the entry's timestamp_end is 0 and the
 * timestamp_begin is not 0:
 *  - If it's the stream file's last packet: set the packet index entry's end
 *    time to the packet's last event's time, if any, or to the packet's
 *    beginning time otherwise.
 *  - If it's not the stream file's last packet: set the packet index
 *    entry's end time to the next packet's beginning time.
 *
 * Affected versions:
 * - All current and future lttng-ust and lttng-modules versions.
 */
static
int fix_index_lttng_crash_quirk(struct ctf_fs_trace *trace)
{
	int ret = 0;
	guint ds_file_group_idx;
	GPtrArray *ds_file_groups = trace->ds_file_groups;
	bt_logging_level log_level = trace->log_level;

	for (ds_file_group_idx = 0; ds_file_group_idx < ds_file_groups->len;
			ds_file_group_idx++) {
		guint entry_idx;
		struct ctf_clock_class *default_cc;
		struct ctf_fs_ds_index_entry *last_entry;
		struct ctf_fs_ds_index *index;

		struct ctf_fs_ds_file_group *ds_file_group =
			g_ptr_array_index(ds_file_groups, ds_file_group_idx);

		BT_ASSERT(ds_file_group);
		index = ds_file_group->index;

		BT_ASSERT(ds_file_group->sc->default_clock_class);
		default_cc = ds_file_group->sc->default_clock_class;

		BT_ASSERT(index);
		BT_ASSERT(index->entries);
		BT_ASSERT(index->entries->len > 0);

		last_entry = g_ptr_array_index(index->entries,
			index->entries->len - 1);
		BT_ASSERT(last_entry);


		/* 1. Fix the last entry first. */
		if (last_entry->timestamp_end == 0 &&
				last_entry->timestamp_begin != 0) {
			/*
			 * Decode packet to read the timestamp of the
			 * last event of the stream file.
			 */
			ret = decode_packet_last_event_timestamp(trace,
				default_cc, last_entry,
				&last_entry->timestamp_end,
				&last_entry->timestamp_end_ns);
			if (ret) {
				BT_COMP_LOGE_APPEND_CAUSE(trace->self_comp,
					"Failed to decode last event's clock snapshot");
				goto end;
			}
		}

		/* Iterate over all entries but the last one. */
		for (entry_idx = 0; entry_idx < index->entries->len - 1;
				entry_idx++) {
			struct ctf_fs_ds_index_entry *curr_entry, *next_entry;
			curr_entry = g_ptr_array_index(index->entries, entry_idx);
			next_entry = g_ptr_array_index(index->entries, entry_idx + 1);

			if (curr_entry->timestamp_end == 0 &&
					curr_entry->timestamp_begin != 0) {
				/*
				 * 2. Set the current index entry `end` timestamp to
				 * the next index entry `begin` timestamp.
				 */
				curr_entry->timestamp_end = next_entry->timestamp_begin;
				curr_entry->timestamp_end_ns = next_entry->timestamp_begin_ns;
			}
		}
	}

end:
	return ret;
}

/*
 * Extract the tracer information necessary to compare versions.
 * Returns 0 on success, and -1 if the extraction is not successful because the
 * necessary fields are absents in the trace metadata.
 */
static
int extract_tracer_info(struct ctf_fs_trace *trace,
		struct tracer_info *current_tracer_info)
{
	int ret = 0;
	struct ctf_trace_class_env_entry *entry;

	/* Clear the current_tracer_info struct */
	memset(current_tracer_info, 0, sizeof(*current_tracer_info));

	/*
	 * To compare 2 tracer versions, at least the tracer name and it's
	 * major version are needed. If one of these is missing, consider it an
	 * extraction failure.
	 */
	entry = ctf_trace_class_borrow_env_entry_by_name(
		trace->metadata->tc, "tracer_name");
	if (!entry || entry->type != CTF_TRACE_CLASS_ENV_ENTRY_TYPE_STR) {
		goto missing_bare_minimum;
	}

	/* Set tracer name. */
	current_tracer_info->name = entry->value.str->str;

	entry = ctf_trace_class_borrow_env_entry_by_name(
		trace->metadata->tc, "tracer_major");
	if (!entry || entry->type != CTF_TRACE_CLASS_ENV_ENTRY_TYPE_INT) {
		goto missing_bare_minimum;
	}

	/* Set major version number. */
	current_tracer_info->major = entry->value.i;

	entry = ctf_trace_class_borrow_env_entry_by_name(
		trace->metadata->tc, "tracer_minor");
	if (!entry || entry->type != CTF_TRACE_CLASS_ENV_ENTRY_TYPE_INT) {
		goto end;
	}

	/* Set minor version number. */
	current_tracer_info->minor = entry->value.i;

	entry = ctf_trace_class_borrow_env_entry_by_name(
		trace->metadata->tc, "tracer_patch");
	if (!entry) {
		/*
		 * If `tracer_patch` doesn't exist `tracer_patchlevel` might.
		 * For example, `lttng-modules` uses entry name
		 * `tracer_patchlevel`.
		 */
		entry = ctf_trace_class_borrow_env_entry_by_name(
			trace->metadata->tc, "tracer_patchlevel");
	}

	if (!entry || entry->type != CTF_TRACE_CLASS_ENV_ENTRY_TYPE_INT) {
		goto end;
	}

	/* Set patch version number. */
	current_tracer_info->patch = entry->value.i;

	goto end;

missing_bare_minimum:
	ret = -1;
end:
	return ret;
}

static
bool is_tracer_affected_by_lttng_event_after_packet_bug(
		struct tracer_info *curr_tracer_info)
{
	bool is_affected = false;

	if (strcmp(curr_tracer_info->name, "lttng-ust") == 0) {
		if (curr_tracer_info->major < 2) {
			is_affected = true;
		} else if (curr_tracer_info->major == 2) {
			/* fixed in lttng-ust 2.11.0 */
			if (curr_tracer_info->minor < 11) {
				is_affected = true;
			}
		}
	} else if (strcmp(curr_tracer_info->name, "lttng-modules") == 0) {
		if (curr_tracer_info->major < 2) {
			is_affected = true;
		} else if (curr_tracer_info->major == 2) {
			/* fixed in lttng-modules 2.11.0 */
			if (curr_tracer_info->minor == 10) {
				/* fixed in lttng-modules 2.10.10 */
				if (curr_tracer_info->patch < 10) {
					is_affected = true;
				}
			} else if (curr_tracer_info->minor == 9) {
				/* fixed in lttng-modules 2.9.13 */
				if (curr_tracer_info->patch < 13) {
					is_affected = true;
				}
			} else if (curr_tracer_info->minor < 9) {
				is_affected = true;
			}
		}
	}

	return is_affected;
}

static
bool is_tracer_affected_by_barectf_event_before_packet_bug(
		struct tracer_info *curr_tracer_info)
{
	bool is_affected = false;

	if (strcmp(curr_tracer_info->name, "barectf") == 0) {
		if (curr_tracer_info->major < 2) {
			is_affected = true;
		} else if (curr_tracer_info->major == 2) {
			if (curr_tracer_info->minor < 3) {
				is_affected = true;
			} else if (curr_tracer_info->minor == 3) {
				/* fixed in barectf 2.3.1 */
				if (curr_tracer_info->patch < 1) {
					is_affected = true;
				}
			}
		}
	}

	return is_affected;
}

static
bool is_tracer_affected_by_lttng_crash_quirk(
		struct tracer_info *curr_tracer_info)
{
	bool is_affected = false;

	/* All LTTng tracer may be affected by this lttng crash quirk. */
	if (strcmp(curr_tracer_info->name, "lttng-ust") == 0) {
		is_affected = true;
	} else if (strcmp(curr_tracer_info->name, "lttng-modules") == 0) {
		is_affected = true;
	}

	return is_affected;
}

/*
 * Looks for trace produced by known buggy tracers and fix up the index
 * produced earlier.
 */
static
int fix_packet_index_tracer_bugs(struct ctf_fs_component *ctf_fs,
		bt_self_component *self_comp,
		bt_self_component_class *self_comp_class)
{
	int ret = 0;
	struct tracer_info current_tracer_info;
	bt_logging_level log_level = ctf_fs->log_level;

	ret = extract_tracer_info(ctf_fs->trace, &current_tracer_info);
	if (ret) {
		/*
		 * A trace may not have all the necessary environment
		 * entries to do the tracer version comparison.
		 * At least, the tracer name and major version number
		 * are needed. Failing to extract these entries is not
		 * an error.
		 */
		ret = 0;
		BT_LOGI_STR("Cannot extract tracer information necessary to compare with buggy versions.");
		goto end;;
	}

	/* Check if the trace may be affected by old tracer bugs. */
	if (is_tracer_affected_by_lttng_event_after_packet_bug(
			&current_tracer_info)) {
		BT_LOGI_STR("Trace may be affected by LTTng tracer packet timestamp bug. Fixing up.");
		ret = fix_index_lttng_event_after_packet_bug(ctf_fs->trace);
		if (ret) {
			BT_COMP_OR_COMP_CLASS_LOGE_APPEND_CAUSE(
				self_comp, self_comp_class,
				"Failed to fix LTTng event-after-packet bug.");
			goto end;
		}
		ctf_fs->trace->metadata->tc->quirks.lttng_event_after_packet = true;
	}

	if (is_tracer_affected_by_barectf_event_before_packet_bug(
			&current_tracer_info)) {
		BT_LOGI_STR("Trace may be affected by barectf tracer packet timestamp bug. Fixing up.");
		ret = fix_index_barectf_event_before_packet_bug(ctf_fs->trace);
		if (ret) {
			BT_COMP_OR_COMP_CLASS_LOGE_APPEND_CAUSE(
				self_comp, self_comp_class,
				"Failed to fix barectf event-before-packet bug.");
			goto end;
		}
		ctf_fs->trace->metadata->tc->quirks.barectf_event_before_packet = true;
	}

	if (is_tracer_affected_by_lttng_crash_quirk(
			&current_tracer_info)) {
		ret = fix_index_lttng_crash_quirk(ctf_fs->trace);
		if (ret) {
			BT_COMP_OR_COMP_CLASS_LOGE_APPEND_CAUSE(
				self_comp, self_comp_class,
				"Failed to fix lttng-crash timestamp quirks.");
			goto end;
		}
		ctf_fs->trace->metadata->tc->quirks.lttng_crash = true;
	}

end:
	return ret;
}

static
gint compare_ds_file_groups_by_first_path(gconstpointer a, gconstpointer b)
{
	struct ctf_fs_ds_file_group * const *ds_file_group_a = a;
	struct ctf_fs_ds_file_group * const *ds_file_group_b = b;
	const struct ctf_fs_ds_file_info *first_ds_file_info_a;
	const struct ctf_fs_ds_file_info *first_ds_file_info_b;

	BT_ASSERT((*ds_file_group_a)->ds_file_infos->len > 0);
	BT_ASSERT((*ds_file_group_b)->ds_file_infos->len > 0);
	first_ds_file_info_a = (*ds_file_group_a)->ds_file_infos->pdata[0];
	first_ds_file_info_b = (*ds_file_group_b)->ds_file_infos->pdata[0];
	return strcmp(first_ds_file_info_a->path->str,
		first_ds_file_info_b->path->str);
}

static
gint compare_strings(gconstpointer p_a, gconstpointer p_b)
{
	const char *a = *((const char **) p_a);
	const char *b = *((const char **) p_b);

	return strcmp(a, b);
}

int ctf_fs_component_create_ctf_fs_trace(
		struct ctf_fs_component *ctf_fs,
		const bt_value *paths_value,
		const bt_value *trace_name_value,
		bt_self_component *self_comp,
		bt_self_component_class *self_comp_class)
{
	int ret = 0;
	uint64_t i;
	bt_logging_level log_level = ctf_fs->log_level;
	GPtrArray *paths = NULL;
	GPtrArray *traces;
	const char *trace_name;

	BT_ASSERT(bt_value_get_type(paths_value) == BT_VALUE_TYPE_ARRAY);
	BT_ASSERT(!bt_value_array_is_empty(paths_value));

	traces = g_ptr_array_new_with_free_func(ctf_fs_trace_destroy_notifier);
	if (!traces) {
		BT_COMP_OR_COMP_CLASS_LOGE_APPEND_CAUSE(self_comp, self_comp_class,
			"Failed to allocate a GPtrArray.");
		goto error;
	}

	paths = g_ptr_array_new_with_free_func(g_free);
	if (!paths) {
		BT_COMP_OR_COMP_CLASS_LOGE_APPEND_CAUSE(self_comp, self_comp_class,
			"Failed to allocate a GPtrArray.");
		goto error;
	}

	trace_name = trace_name_value ? bt_value_string_get(trace_name_value) : NULL;

	/*
	 * Create a sorted array of the paths, to make the execution of this
	 * component deterministic.
	 */
	for (i = 0; i < bt_value_array_get_length(paths_value); i++) {
		const bt_value *path_value =
			bt_value_array_borrow_element_by_index_const(paths_value, i);
		const char *input = bt_value_string_get(path_value);
		gchar *input_copy;

		input_copy = g_strdup(input);
		if (!input_copy) {
			BT_COMP_OR_COMP_CLASS_LOGE_APPEND_CAUSE(self_comp, self_comp_class,
				"Failed to copy a string.");
			goto error;
		}

		g_ptr_array_add(paths, input_copy);
	}

	g_ptr_array_sort(paths, compare_strings);

	/* Create a separate ctf_fs_trace object for each path. */
	for (i = 0; i < paths->len; i++) {
		const char *path = g_ptr_array_index(paths, i);

		ret = ctf_fs_component_create_ctf_fs_trace_one_path(ctf_fs,
			path, trace_name, traces, self_comp, self_comp_class);
		if (ret) {
			goto end;
		}
	}

	if (traces->len > 1) {
		struct ctf_fs_trace *first_trace = (struct ctf_fs_trace *) traces->pdata[0];
		const uint8_t *first_trace_uuid = first_trace->metadata->tc->uuid;
		struct ctf_fs_trace *trace;

		/*
		 * We have more than one trace, they must all share the same
		 * UUID, verify that.
		 */
		for (i = 0; i < traces->len; i++) {
			struct ctf_fs_trace *this_trace =
				(struct ctf_fs_trace *) traces->pdata[i];
			const uint8_t *this_trace_uuid = this_trace->metadata->tc->uuid;

			if (!this_trace->metadata->tc->is_uuid_set) {
				BT_COMP_OR_COMP_CLASS_LOGE_APPEND_CAUSE(self_comp, self_comp_class,
					"Multiple traces given, but a trace does not have a UUID: path=%s",
					this_trace->path->str);
				goto error;
			}

			if (bt_uuid_compare(first_trace_uuid, this_trace_uuid) != 0) {
				char first_trace_uuid_str[BT_UUID_STR_LEN + 1];
				char this_trace_uuid_str[BT_UUID_STR_LEN + 1];

				bt_uuid_to_str(first_trace_uuid, first_trace_uuid_str);
				bt_uuid_to_str(this_trace_uuid, this_trace_uuid_str);

				BT_COMP_OR_COMP_CLASS_LOGE_APPEND_CAUSE(self_comp, self_comp_class,
					"Multiple traces given, but UUIDs don't match: "
					"first-trace-uuid=%s, first-trace-path=%s, "
					"trace-uuid=%s, trace-path=%s",
					first_trace_uuid_str, first_trace->path->str,
					this_trace_uuid_str, this_trace->path->str);
				goto error;
			}
		}

		ret = merge_ctf_fs_traces((struct ctf_fs_trace **) traces->pdata,
			traces->len, &trace);
		if (ret) {
			BT_COMP_OR_COMP_CLASS_LOGE_APPEND_CAUSE(self_comp, self_comp_class,
				"Failed to merge traces with the same UUID.");
			goto error;
		}

		ctf_fs->trace = trace;
	} else {
		/* Just one trace, it may or may not have a UUID, both are fine. */
		ctf_fs->trace = traces->pdata[0];
		traces->pdata[0] = NULL;
	}

	ret = fix_packet_index_tracer_bugs(ctf_fs, self_comp, self_comp_class);
	if (ret) {
		BT_COMP_OR_COMP_CLASS_LOGE_APPEND_CAUSE(self_comp, self_comp_class,
			"Failed to fix packet index tracer bugs.");
	}

	/*
	 * Sort data stream file groups by first data stream file info
	 * path to get a deterministic order. This order influences the
	 * order of the output ports. It also influences the order of
	 * the automatic stream IDs if the trace's packet headers do not
	 * contain a `stream_instance_id` field, in which case the data
	 * stream file to stream ID association is always the same,
	 * whatever the build and the system.
	 *
	 * Having a deterministic order here can help debugging and
	 * testing.
	 */
	g_ptr_array_sort(ctf_fs->trace->ds_file_groups,
		compare_ds_file_groups_by_first_path);
	goto end;
error:
	ret = -1;

end:
	if (traces) {
		g_ptr_array_free(traces, TRUE);
	}

	if (paths) {
		g_ptr_array_free(paths, TRUE);
	}

	return ret;
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

/* Create the IR stream objects for ctf_fs_trace. */

static
int create_streams_for_trace(struct ctf_fs_trace *ctf_fs_trace)
{
	int ret;
	GString *name = NULL;
	guint i;
	bt_logging_level log_level = ctf_fs_trace->log_level;
	bt_self_component *self_comp = ctf_fs_trace->self_comp;

	for (i = 0; i < ctf_fs_trace->ds_file_groups->len; i++) {
		struct ctf_fs_ds_file_group *ds_file_group =
			g_ptr_array_index(ctf_fs_trace->ds_file_groups, i);
		name = get_stream_instance_unique_name(ds_file_group);

		if (!name) {
			goto error;
		}

		if (ds_file_group->sc->ir_sc) {
			BT_ASSERT(ctf_fs_trace->trace);

			if (ds_file_group->stream_id == UINT64_C(-1)) {
				/* No stream ID: use 0 */
				ds_file_group->stream = bt_stream_create_with_id(
					ds_file_group->sc->ir_sc,
					ctf_fs_trace->trace,
					ctf_fs_trace->next_stream_id);
				ctf_fs_trace->next_stream_id++;
			} else {
				/* Specific stream ID */
				ds_file_group->stream = bt_stream_create_with_id(
					ds_file_group->sc->ir_sc,
					ctf_fs_trace->trace,
					(uint64_t) ds_file_group->stream_id);
			}
		} else {
			ds_file_group->stream = NULL;
		}

		if (!ds_file_group->stream) {
			BT_COMP_LOGE_APPEND_CAUSE(self_comp,
				"Cannot create stream for DS file group: "
				"addr=%p, stream-name=\"%s\"",
				ds_file_group, name->str);
			goto error;
		}

		ret = bt_stream_set_name(ds_file_group->stream,
			name->str);
		if (ret) {
			BT_COMP_LOGE_APPEND_CAUSE(self_comp, "Cannot set stream's name: "
				"addr=%p, stream-name=\"%s\"",
				ds_file_group->stream, name->str);
			goto error;
		}

		g_string_free(name, TRUE);
		name = NULL;
	}

	ret = 0;
	goto end;

error:
	ret = -1;

end:

	if (name) {
		g_string_free(name, TRUE);
	}
	return ret;
}

static const struct bt_param_validation_value_descr inputs_elem_descr = {
	.type = BT_VALUE_TYPE_STRING,
};

static const struct bt_param_validation_map_value_entry_descr fs_params_entries_descr[] = {
	{ "inputs", BT_PARAM_VALIDATION_MAP_VALUE_ENTRY_MANDATORY, {
		BT_VALUE_TYPE_ARRAY,
		.array = {
			.min_length = 1,
			.max_length = BT_PARAM_VALIDATION_INFINITE,
			.element_type = &inputs_elem_descr,
		}
	}},
	{ "trace-name", BT_PARAM_VALIDATION_MAP_VALUE_ENTRY_OPTIONAL, { .type = BT_VALUE_TYPE_STRING } },
	{ "clock-class-offset-s", BT_PARAM_VALIDATION_MAP_VALUE_ENTRY_OPTIONAL, { .type = BT_VALUE_TYPE_SIGNED_INTEGER } },
	{ "clock-class-offset-ns", BT_PARAM_VALIDATION_MAP_VALUE_ENTRY_OPTIONAL, { .type = BT_VALUE_TYPE_SIGNED_INTEGER } },
	{ "force-clock-class-origin-unix-epoch", BT_PARAM_VALIDATION_MAP_VALUE_ENTRY_OPTIONAL, { .type = BT_VALUE_TYPE_BOOL } },
	BT_PARAM_VALIDATION_MAP_VALUE_ENTRY_END
};


bool read_src_fs_parameters(const bt_value *params,
		const bt_value **inputs,
		const bt_value **trace_name,
		struct ctf_fs_component *ctf_fs,
		bt_self_component *self_comp,
		bt_self_component_class *self_comp_class) {
	bool ret;
	const bt_value *value;
	bt_logging_level log_level = ctf_fs->log_level;
	enum bt_param_validation_status validate_value_status;
	gchar *error = NULL;

	validate_value_status = bt_param_validation_validate(params,
		fs_params_entries_descr, &error);
	if (validate_value_status != BT_PARAM_VALIDATION_STATUS_OK) {
		BT_COMP_OR_COMP_CLASS_LOGE_APPEND_CAUSE(self_comp, self_comp_class,
			"%s", error);
		ret = false;
		goto end;
	}

	/* inputs parameter */
	*inputs = bt_value_map_borrow_entry_value_const(params, "inputs");

	/* clock-class-offset-s parameter */
	value = bt_value_map_borrow_entry_value_const(params,
		"clock-class-offset-s");
	if (value) {
		ctf_fs->metadata_config.clock_class_offset_s =
			bt_value_integer_signed_get(value);
	}

	/* clock-class-offset-ns parameter */
	value = bt_value_map_borrow_entry_value_const(params,
		"clock-class-offset-ns");
	if (value) {
		ctf_fs->metadata_config.clock_class_offset_ns =
			bt_value_integer_signed_get(value);
	}

	/* force-clock-class-origin-unix-epoch parameter */
	value = bt_value_map_borrow_entry_value_const(params,
		"force-clock-class-origin-unix-epoch");
	if (value) {
		ctf_fs->metadata_config.force_clock_class_origin_unix_epoch =
			bt_value_bool_get(value);
	}

	/* trace-name parameter */
	*trace_name = bt_value_map_borrow_entry_value_const(params, "trace-name");

	ret = true;

end:
	g_free(error);
	return ret;
}

static
struct ctf_fs_component *ctf_fs_create(
	const bt_value *params,
	bt_self_component_source *self_comp_src)
{
	struct ctf_fs_component *ctf_fs = NULL;
	const bt_value *inputs_value;
	const bt_value *trace_name_value;
	bt_self_component *self_comp =
		bt_self_component_source_as_self_component(self_comp_src);

	ctf_fs = ctf_fs_component_create(bt_component_get_logging_level(
		bt_self_component_as_component(self_comp)), self_comp);
	if (!ctf_fs) {
		goto error;
	}

	if (!read_src_fs_parameters(params, &inputs_value, &trace_name_value,
			ctf_fs, self_comp, NULL)) {
		goto error;
	}

	bt_self_component_set_data(self_comp, ctf_fs);

	if (ctf_fs_component_create_ctf_fs_trace(ctf_fs, inputs_value,
			trace_name_value, self_comp, NULL)) {
		goto error;
	}

	if (create_streams_for_trace(ctf_fs->trace)) {
		goto error;
	}

	if (create_ports_for_trace(ctf_fs, ctf_fs->trace, self_comp_src)) {
		goto error;
	}

	goto end;

error:
	ctf_fs_destroy(ctf_fs);
	ctf_fs = NULL;
	bt_self_component_set_data(self_comp, NULL);

end:
	return ctf_fs;
}

BT_HIDDEN
bt_component_class_initialize_method_status ctf_fs_init(
		bt_self_component_source *self_comp_src,
		bt_self_component_source_configuration *config,
		const bt_value *params, __attribute__((unused)) void *init_method_data)
{
	struct ctf_fs_component *ctf_fs;
	bt_component_class_initialize_method_status ret =
		BT_COMPONENT_CLASS_INITIALIZE_METHOD_STATUS_OK;

	ctf_fs = ctf_fs_create(params, self_comp_src);
	if (!ctf_fs) {
		ret = BT_COMPONENT_CLASS_INITIALIZE_METHOD_STATUS_ERROR;
	}

	return ret;
}

BT_HIDDEN
bt_component_class_query_method_status ctf_fs_query(
		bt_self_component_class_source *comp_class,
		bt_private_query_executor *priv_query_exec,
		const char *object, const bt_value *params,
		__attribute__((unused)) void *method_data,
		const bt_value **result)
{
	bt_component_class_query_method_status status =
		BT_COMPONENT_CLASS_QUERY_METHOD_STATUS_OK;
	bt_logging_level log_level = bt_query_executor_get_logging_level(
		bt_private_query_executor_as_query_executor_const(
			priv_query_exec));

	if (strcmp(object, "metadata-info") == 0) {
		status = metadata_info_query(comp_class, params, log_level,
			result);
	} else if (strcmp(object, "babeltrace.trace-infos") == 0) {
		status = trace_infos_query(comp_class, params, log_level,
			result);
	} else if (!strcmp(object, "babeltrace.support-info")) {
		status = support_info_query(comp_class, params, log_level, result);
	} else {
		BT_LOGE("Unknown query object `%s`", object);
		status = BT_COMPONENT_CLASS_QUERY_METHOD_STATUS_UNKNOWN_OBJECT;
		goto end;
	}
end:
	return status;
}
