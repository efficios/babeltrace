/*
 * query.c
 *
 * Babeltrace CTF file system Reader Component queries
 *
 * Copyright 2017 Jérémie Galarneau <jeremie.galarneau@efficios.com>
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

#define BT_LOG_OUTPUT_LEVEL log_level
#define BT_LOG_TAG "PLUGIN/SRC.CTF.FS/QUERY"
#include "logging/log.h"

#include "query.h"
#include <stdbool.h>
#include <glib.h>
#include <glib/gstdio.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "common/assert.h"
#include "metadata.h"
#include "../common/metadata/decoder.h"
#include "common/common.h"
#include "common/macros.h"
#include <babeltrace2/babeltrace.h>
#include "fs.h"
#include "logging/comp-logging.h"

#define METADATA_TEXT_SIG	"/* CTF 1.8"

struct range {
	int64_t begin_ns;
	int64_t end_ns;
	bool set;
};

BT_HIDDEN
bt_component_class_query_method_status metadata_info_query(
		bt_self_component_class_source *self_comp_class_src,
		const bt_value *params, bt_logging_level log_level,
		const bt_value **user_result)
{
	bt_component_class_query_method_status status =
		BT_COMPONENT_CLASS_QUERY_METHOD_STATUS_OK;
	bt_self_component_class *self_comp_class =
		bt_self_component_class_source_as_self_component_class(self_comp_class_src);
	bt_value *result = NULL;
	const bt_value *path_value = NULL;
	FILE *metadata_fp = NULL;
	int ret;
	int bo;
	const char *path;
	bool is_packetized;
	struct ctf_metadata_decoder *decoder = NULL;
	struct ctf_metadata_decoder_config decoder_cfg = { 0 };
	enum ctf_metadata_decoder_status decoder_status;

	result = bt_value_map_create();
	if (!result) {
		status = BT_COMPONENT_CLASS_QUERY_METHOD_STATUS_MEMORY_ERROR;
		goto error;
	}

	BT_ASSERT(params);

	if (!bt_value_is_map(params)) {
		BT_COMP_CLASS_LOGE_APPEND_CAUSE(self_comp_class,
			"Query parameters is not a map value object.");
		status = BT_COMPONENT_CLASS_QUERY_METHOD_STATUS_ERROR;
		goto error;
	}

	path_value = bt_value_map_borrow_entry_value_const(params, "path");
	if (!path_value) {
		BT_COMP_CLASS_LOGE_APPEND_CAUSE(self_comp_class,
			"Mandatory `path` parameter missing");
		status = BT_COMPONENT_CLASS_QUERY_METHOD_STATUS_ERROR;
		goto error;
	}

	if (!bt_value_is_string(path_value)) {
		BT_COMP_CLASS_LOGE_APPEND_CAUSE(self_comp_class,
			"`path` parameter is required to be a string value");
		status = BT_COMPONENT_CLASS_QUERY_METHOD_STATUS_ERROR;
		goto error;
	}

	path = bt_value_string_get(path_value);

	BT_ASSERT(path);
	metadata_fp = ctf_fs_metadata_open_file(path);
	if (!metadata_fp) {
		BT_COMP_CLASS_LOGE_APPEND_CAUSE(self_comp_class,
			"Cannot open trace metadata: path=\"%s\".", path);
		goto error;
	}

	ret = ctf_metadata_decoder_is_packetized(metadata_fp, &is_packetized,
		&bo, log_level, NULL);
	if (ret) {
		BT_COMP_CLASS_LOGE_APPEND_CAUSE(self_comp_class,
			"Cannot check whether or not the metadata stream is packetized: path=\"%s\".",
			path);
		goto error;
	}

	decoder_cfg.log_level = log_level;
	decoder_cfg.keep_plain_text = true;
	decoder = ctf_metadata_decoder_create(&decoder_cfg);
	if (!decoder) {
		BT_COMP_CLASS_LOGE_APPEND_CAUSE(self_comp_class,
			"Cannot create metadata decoder: path=\"%s\".", path);
		goto error;
	}

	rewind(metadata_fp);
	decoder_status = ctf_metadata_decoder_append_content(decoder,
		metadata_fp);
	if (decoder_status) {
		BT_COMP_CLASS_LOGE_APPEND_CAUSE(self_comp_class,
			"Cannot update metadata decoder's content: path=\"%s\".",
			path);
		goto error;
	}

	ret = bt_value_map_insert_string_entry(result, "text",
		ctf_metadata_decoder_get_text(decoder));
	if (ret) {
		BT_COMP_CLASS_LOGE_APPEND_CAUSE(self_comp_class,
			"Cannot insert metadata text into query result.");
		goto error;
	}

	ret = bt_value_map_insert_bool_entry(result, "is-packetized",
		is_packetized);
	if (ret) {
		BT_COMP_CLASS_LOGE_APPEND_CAUSE(self_comp_class,
			"Cannot insert \"is-packetized\" attribute into query result.");
		goto error;
	}

	goto end;

error:
	BT_VALUE_PUT_REF_AND_RESET(result);
	result = NULL;

	if (status >= 0) {
		status = BT_COMPONENT_CLASS_QUERY_METHOD_STATUS_ERROR;
	}

end:
	ctf_metadata_decoder_destroy(decoder);

	if (metadata_fp) {
		ret = fclose(metadata_fp);
		if (ret) {
			BT_LOGE_ERRNO("Cannot close metatada file stream",
				": path=\"%s\"", path);
		}
	}

	*user_result = result;
	return status;
}

static
int add_range(bt_value *info, struct range *range,
		const char *range_name)
{
	int ret = 0;
	bt_value_map_insert_entry_status status;
	bt_value *range_map;

	if (!range->set) {
		/* Not an error. */
		goto end;
	}

	status = bt_value_map_insert_empty_map_entry(info, range_name,
		&range_map);
	if (status != BT_VALUE_MAP_INSERT_ENTRY_STATUS_OK) {
		ret = -1;
		goto end;
	}

	status = bt_value_map_insert_signed_integer_entry(range_map, "begin",
			range->begin_ns);
	if (status != BT_VALUE_MAP_INSERT_ENTRY_STATUS_OK) {
		ret = -1;
		goto end;
	}

	status = bt_value_map_insert_signed_integer_entry(range_map, "end",
			range->end_ns);
	if (status != BT_VALUE_MAP_INSERT_ENTRY_STATUS_OK) {
		ret = -1;
		goto end;
	}

end:
	return ret;
}

static
int populate_stream_info(struct ctf_fs_ds_file_group *group,
		bt_value *group_info, struct range *stream_range)
{
	int ret = 0;
	bt_value_map_insert_entry_status insert_status;
	struct ctf_fs_ds_index_entry *first_ds_index_entry, *last_ds_index_entry;
	gchar *port_name = NULL;

	/*
	 * Since each `struct ctf_fs_ds_file_group` has a sorted array of
	 * `struct ctf_fs_ds_index_entry`, we can compute the stream range from
	 * the timestamp_begin of the first index entry and the timestamp_end
	 * of the last index entry.
	 */
	BT_ASSERT(group->index);
	BT_ASSERT(group->index->entries);
	BT_ASSERT(group->index->entries->len > 0);

	/* First entry. */
	first_ds_index_entry = (struct ctf_fs_ds_index_entry *) g_ptr_array_index(
		group->index->entries, 0);

	/* Last entry. */
	last_ds_index_entry = (struct ctf_fs_ds_index_entry *) g_ptr_array_index(
		group->index->entries, group->index->entries->len - 1);

	stream_range->begin_ns = first_ds_index_entry->timestamp_begin_ns;
	stream_range->end_ns = last_ds_index_entry->timestamp_end_ns;

	/*
	 * If any of the begin and end timestamps is not set it means that
	 * packets don't include `timestamp_begin` _and_ `timestamp_end` fields
	 * in their packet context so we can't set the range.
	 */
	stream_range->set = stream_range->begin_ns != UINT64_C(-1) &&
		stream_range->end_ns != UINT64_C(-1);

	ret = add_range(group_info, stream_range, "range-ns");
	if (ret) {
		goto end;
	}

	port_name = ctf_fs_make_port_name(group);
	if (!port_name) {
		ret = -1;
		goto end;
	}

	insert_status = bt_value_map_insert_string_entry(group_info,
		"port-name", port_name);
	if (insert_status != BT_VALUE_MAP_INSERT_ENTRY_STATUS_OK) {
		ret = -1;
		goto end;
	}

end:
	g_free(port_name);
	return ret;
}

static
int populate_trace_info(const struct ctf_fs_trace *trace, bt_value *trace_info,
		bt_logging_level log_level,
		bt_self_component_class *self_comp_class)
{
	int ret = 0;
	size_t group_idx;
	bt_value_map_insert_entry_status insert_status;
	bt_value_array_append_element_status append_status;
	bt_value *file_groups = NULL;

	BT_ASSERT(trace->ds_file_groups);
	/* Add trace range info only if it contains streams. */
	if (trace->ds_file_groups->len == 0) {
		ret = -1;
		BT_COMP_CLASS_LOGE_APPEND_CAUSE(self_comp_class,
			"Trace has no streams: trace-path=%s", trace->path->str);
		goto end;
	}

	insert_status = bt_value_map_insert_empty_array_entry(trace_info,
		"stream-infos", &file_groups);
	if (insert_status != BT_VALUE_MAP_INSERT_ENTRY_STATUS_OK) {
		ret = -1;
		goto end;
	}

	/* Find range of all stream groups, and of the trace. */
	for (group_idx = 0; group_idx < trace->ds_file_groups->len;
			group_idx++) {
		bt_value *group_info;
		struct range group_range = { .set = false };
		struct ctf_fs_ds_file_group *group = g_ptr_array_index(
				trace->ds_file_groups, group_idx);

		append_status = bt_value_array_append_empty_map_element(
			file_groups, &group_info);
		if (append_status != BT_VALUE_ARRAY_APPEND_ELEMENT_STATUS_OK) {
			ret = -1;
			goto end;
		}

		ret = populate_stream_info(group, group_info, &group_range);
		if (ret) {
			goto end;
		}
	}

end:
	return ret;
}

BT_HIDDEN
bt_component_class_query_method_status trace_infos_query(
		bt_self_component_class_source *self_comp_class_src,
		const bt_value *params, bt_logging_level log_level,
		const bt_value **user_result)
{
	struct ctf_fs_component *ctf_fs = NULL;
	bt_component_class_query_method_status status =
		BT_COMPONENT_CLASS_QUERY_METHOD_STATUS_OK;
	bt_self_component_class *self_comp_class =
		bt_self_component_class_source_as_self_component_class(
			self_comp_class_src);
	bt_value *result = NULL;
	const bt_value *inputs_value = NULL;
	const bt_value *trace_name_value;
	int ret = 0;
	bt_value *trace_info = NULL;
	bt_value_array_append_element_status append_status;

	BT_ASSERT(params);

	if (!bt_value_is_map(params)) {
		BT_COMP_CLASS_LOGE_APPEND_CAUSE(self_comp_class,
			"Query parameters is not a map value object.");
		status = BT_COMPONENT_CLASS_QUERY_METHOD_STATUS_ERROR;
		goto error;
	}

	ctf_fs = ctf_fs_component_create(log_level, NULL);
	if (!ctf_fs) {
		goto error;
	}

	if (!read_src_fs_parameters(params, &inputs_value, &trace_name_value,
			ctf_fs, NULL, self_comp_class)) {
		status = BT_COMPONENT_CLASS_QUERY_METHOD_STATUS_ERROR;
		goto error;
	}

	if (ctf_fs_component_create_ctf_fs_trace(ctf_fs, inputs_value,
			trace_name_value, NULL, self_comp_class)) {
		goto error;
	}

	result = bt_value_array_create();
	if (!result) {
		status = BT_COMPONENT_CLASS_QUERY_METHOD_STATUS_MEMORY_ERROR;
		goto error;
	}

	append_status = bt_value_array_append_empty_map_element(result,
		&trace_info);
	if (append_status != BT_VALUE_ARRAY_APPEND_ELEMENT_STATUS_OK) {
		BT_COMP_CLASS_LOGE_APPEND_CAUSE(self_comp_class,
			"Failed to create trace info map.");
		goto error;
	}

	ret = populate_trace_info(ctf_fs->trace, trace_info, log_level,
		self_comp_class);
	if (ret) {
		goto error;
	}

	goto end;

error:
	BT_VALUE_PUT_REF_AND_RESET(result);

	if (status >= 0) {
		status = BT_COMPONENT_CLASS_QUERY_METHOD_STATUS_ERROR;
	}

end:
	if (ctf_fs) {
		ctf_fs_destroy(ctf_fs);
		ctf_fs = NULL;
	}

	*user_result = result;
	return status;
}

BT_HIDDEN
bt_component_class_query_method_status support_info_query(
		bt_self_component_class_source *comp_class,
		const bt_value *params, bt_logging_level log_level,
		const bt_value **user_result)
{
	const bt_value *input_type_value;
	const char *input_type;
	bt_component_class_query_method_status status;
	bt_value_map_insert_entry_status insert_entry_status;
	double weight = 0;
	gchar *metadata_path = NULL;
	bt_value *result = NULL;
	struct ctf_metadata_decoder *metadata_decoder = NULL;
	FILE *metadata_file = NULL;
	char uuid_str[BT_UUID_STR_LEN + 1];
	bool has_uuid = false;
	const bt_value *input_value;
	const char *input;

	input_type_value = bt_value_map_borrow_entry_value_const(params, "type");
	BT_ASSERT(input_type_value);
	BT_ASSERT(bt_value_get_type(input_type_value) == BT_VALUE_TYPE_STRING);
	input_type = bt_value_string_get(input_type_value);

	if (strcmp(input_type, "directory") != 0) {
		goto create_result;
	}

	input_value = bt_value_map_borrow_entry_value_const(params, "input");
	BT_ASSERT(input_value);
	BT_ASSERT(bt_value_get_type(input_value) == BT_VALUE_TYPE_STRING);
	input = bt_value_string_get(input_value);

	metadata_path = g_build_filename(input, CTF_FS_METADATA_FILENAME, NULL);
	if (!metadata_path) {
		status = BT_COMPONENT_CLASS_QUERY_METHOD_STATUS_MEMORY_ERROR;
		goto end;
	}

	metadata_file = g_fopen(metadata_path, "rb");
	if (metadata_file) {
		struct ctf_metadata_decoder_config metadata_decoder_config = { 0 };
		enum ctf_metadata_decoder_status decoder_status;
		bt_uuid_t uuid;

		metadata_decoder_config.log_level = log_level;

		metadata_decoder = ctf_metadata_decoder_create(
			&metadata_decoder_config);
		if (!metadata_decoder) {
			status = BT_COMPONENT_CLASS_QUERY_METHOD_STATUS_ERROR;
			goto end;
		}

		decoder_status = ctf_metadata_decoder_append_content(
			metadata_decoder, metadata_file);
		if (decoder_status != CTF_METADATA_DECODER_STATUS_OK) {
			BT_LOGW("cannot append metadata content: metadata-decoder-status=%d",
				decoder_status);
			status = BT_COMPONENT_CLASS_QUERY_METHOD_STATUS_ERROR;
			goto end;
		}

		/*
		 * We were able to parse the metadata file, so we are
		 * confident it's a CTF trace.
		 */
		weight = 0.75;

		/* If the trace has a UUID, return the stringified UUID as the group. */
		if (ctf_metadata_decoder_get_trace_class_uuid(metadata_decoder, uuid) == 0) {
			bt_uuid_to_str(uuid, uuid_str);
			has_uuid = true;
		}
	}

create_result:
	result = bt_value_map_create();
	if (!result) {
		status = BT_COMPONENT_CLASS_QUERY_METHOD_STATUS_MEMORY_ERROR;
		goto end;
	}

	insert_entry_status = bt_value_map_insert_real_entry(result, "weight", weight);
	if (insert_entry_status != BT_VALUE_MAP_INSERT_ENTRY_STATUS_OK) {
		status = (int) insert_entry_status;
		goto end;
	}

	/* We are not supposed to have weight == 0 and a UUID. */
	BT_ASSERT(weight > 0 || !has_uuid);

	if (weight > 0 && has_uuid) {
		insert_entry_status = bt_value_map_insert_string_entry(result, "group", uuid_str);
		if (insert_entry_status != BT_VALUE_MAP_INSERT_ENTRY_STATUS_OK) {
			status = (int) insert_entry_status;
			goto end;
		}
	}

	*user_result = result;
	result = NULL;
	status = BT_COMPONENT_CLASS_QUERY_METHOD_STATUS_OK;

end:
	g_free(metadata_path);
	bt_value_put_ref(result);
	ctf_metadata_decoder_destroy(metadata_decoder);

	return status;
}
