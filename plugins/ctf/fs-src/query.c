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

#include "query.h"
#include <stdbool.h>
#include <babeltrace/assert-internal.h>
#include "metadata.h"
#include "../common/metadata/decoder.h"
#include <babeltrace/common-internal.h>
#include <babeltrace/babeltrace-internal.h>
#include <babeltrace/babeltrace.h>
#include "fs.h"

#define BT_LOG_TAG "PLUGIN-CTF-FS-QUERY-SRC"
#include "logging.h"

#define METADATA_TEXT_SIG	"/* CTF 1.8"

struct range {
	int64_t begin_ns;
	int64_t end_ns;
	bool set;
};

BT_HIDDEN
struct bt_component_class_query_method_return metadata_info_query(
		struct bt_component_class *comp_class,
		struct bt_value *params)
{
	struct bt_component_class_query_method_return query_ret = {
		.result = NULL,
		.status = BT_QUERY_STATUS_OK,
	};

	struct bt_private_value *result = NULL;
	struct bt_value *path_value = NULL;
	char *metadata_text = NULL;
	FILE *metadata_fp = NULL;
	GString *g_metadata_text = NULL;
	int ret;
	int bo;
	const char *path;
	bool is_packetized;

	result = bt_private_value_map_create();
	if (!result) {
		query_ret.status = BT_QUERY_STATUS_NOMEM;
		goto error;
	}

	query_ret.result = bt_value_borrow_from_private(result);
	BT_ASSERT(params);

	if (!bt_value_is_map(params)) {
		BT_LOGE_STR("Query parameters is not a map value object.");
		query_ret.status = BT_QUERY_STATUS_INVALID_PARAMS;
		goto error;
	}

	path_value = bt_value_map_borrow_entry_value(params, "path");
	path = bt_value_string_get(path_value);

	BT_ASSERT(path);
	metadata_fp = ctf_fs_metadata_open_file(path);
	if (!metadata_fp) {
		BT_LOGE("Cannot open trace metadata: path=\"%s\".", path);
		goto error;
	}

	is_packetized = ctf_metadata_decoder_is_packetized(metadata_fp,
		&bo);

	if (is_packetized) {
		ret = ctf_metadata_decoder_packetized_file_stream_to_buf(
			metadata_fp, &metadata_text, bo);
		if (ret) {
			BT_LOGE("Cannot decode packetized metadata file: path=\"%s\"",
				path);
			goto error;
		}
	} else {
		long filesize;

		ret = fseek(metadata_fp, 0, SEEK_END);
		if (ret) {
			BT_LOGE_ERRNO("Failed to seek to the end of the metadata file",
				": path=\"%s\"", path);
			goto error;
		}
		filesize = ftell(metadata_fp);
		if (filesize < 0) {
			BT_LOGE_ERRNO("Failed to get the current position in the metadata file",
				": path=\"%s\"", path);
			goto error;
		}
		rewind(metadata_fp);
		metadata_text = malloc(filesize + 1);
		if (!metadata_text) {
			BT_LOGE_STR("Cannot allocate buffer for metadata text.");
			goto error;
		}

		if (fread(metadata_text, filesize, 1, metadata_fp) != 1) {
			BT_LOGE_ERRNO("Cannot read metadata file", ": path=\"%s\"",
				path);
			goto error;
		}

		metadata_text[filesize] = '\0';
	}

	g_metadata_text = g_string_new(NULL);
	if (!g_metadata_text) {
		goto error;
	}

	if (strncmp(metadata_text, METADATA_TEXT_SIG,
			sizeof(METADATA_TEXT_SIG) - 1) != 0) {
		g_string_assign(g_metadata_text, METADATA_TEXT_SIG);
		g_string_append(g_metadata_text, " */\n\n");
	}

	g_string_append(g_metadata_text, metadata_text);

	ret = bt_private_value_map_insert_string_entry(result, "text",
		g_metadata_text->str);
	if (ret) {
		BT_LOGE_STR("Cannot insert metadata text into query result.");
		goto error;
	}

	ret = bt_private_value_map_insert_bool_entry(result, "is-packetized",
		is_packetized);
	if (ret) {
		BT_LOGE_STR("Cannot insert \"is-packetized\" attribute into query result.");
		goto error;
	}

	goto end;

error:
	BT_OBJECT_PUT_REF_AND_RESET(result);
	query_ret.result = NULL;

	if (query_ret.status >= 0) {
		query_ret.status = BT_QUERY_STATUS_ERROR;
	}

end:
	free(metadata_text);

	if (g_metadata_text) {
		g_string_free(g_metadata_text, TRUE);
	}

	if (metadata_fp) {
		fclose(metadata_fp);
	}

	return query_ret;
}

static
int add_range(struct bt_private_value *info, struct range *range,
		const char *range_name)
{
	int ret = 0;
	enum bt_value_status status;
	struct bt_private_value *range_map = NULL;

	if (!range->set) {
		/* Not an error. */
		goto end;
	}

	range_map = bt_private_value_map_create();
	if (!range_map) {
		ret = -1;
		goto end;
	}

	status = bt_private_value_map_insert_integer_entry(range_map, "begin",
			range->begin_ns);
	if (status != BT_VALUE_STATUS_OK) {
		ret = -1;
		goto end;
	}

	status = bt_private_value_map_insert_integer_entry(range_map, "end",
			range->end_ns);
	if (status != BT_VALUE_STATUS_OK) {
		ret = -1;
		goto end;
	}

	status = bt_private_value_map_insert_entry(info, range_name,
		bt_value_borrow_from_private(range_map));
	if (status != BT_VALUE_STATUS_OK) {
		ret = -1;
		goto end;
	}

end:
	bt_object_put_ref(range_map);
	return ret;
}

static
int add_stream_ids(struct bt_private_value *info,
		struct bt_stream *stream)
{
	int ret = 0;
	int64_t stream_class_id, stream_instance_id;
	enum bt_value_status status;
	struct bt_stream_class *stream_class = NULL;

	stream_instance_id = bt_stream_get_id(stream);
	if (stream_instance_id != -1) {
		status = bt_private_value_map_insert_integer_entry(info, "id",
				stream_instance_id);
		if (status != BT_VALUE_STATUS_OK) {
			ret = -1;
			goto end;
		}
	}

	stream_class = bt_stream_borrow_class(stream);
	if (!stream_class) {
		ret = -1;
		goto end;
	}

	stream_class_id = bt_stream_class_get_id(stream_class);
	if (stream_class_id == -1) {
		ret = -1;
		goto end;
	}

	status = bt_private_value_map_insert_integer_entry(info, "class-id", stream_class_id);
	if (status != BT_VALUE_STATUS_OK) {
		ret = -1;
		goto end;
	}

end:
	return ret;
}

static
int populate_stream_info(struct ctf_fs_ds_file_group *group,
		struct bt_private_value *group_info,
		struct range *stream_range)
{
	int ret = 0;
	size_t file_idx;
	enum bt_value_status status;
	struct bt_private_value *file_paths;

	stream_range->begin_ns = INT64_MAX;
	stream_range->end_ns = 0;

	file_paths = bt_private_value_array_create();
	if (!file_paths) {
		ret = -1;
		goto end;
	}

	for (file_idx = 0; file_idx < group->ds_file_infos->len; file_idx++) {
		int64_t file_begin_epoch, file_end_epoch;
		struct ctf_fs_ds_file_info *info =
				g_ptr_array_index(group->ds_file_infos,
					file_idx);

		if (!info->index || info->index->entries->len == 0) {
			BT_LOGW("Cannot determine range of unindexed stream file \'%s\'",
				info->path->str);
			ret = -1;
			goto end;
		}

		status = bt_private_value_array_append_string_element(file_paths,
				info->path->str);
		if (status != BT_VALUE_STATUS_OK) {
			ret = -1;
			goto end;
		}

		/*
		 * file range is from timestamp_begin of the first entry to the
		 * timestamp_end of the last entry.
		 */
		file_begin_epoch = ((struct ctf_fs_ds_index_entry *) &g_array_index(info->index->entries,
				struct ctf_fs_ds_index_entry, 0))->timestamp_begin_ns;
		file_end_epoch = ((struct ctf_fs_ds_index_entry *) &g_array_index(info->index->entries,
				struct ctf_fs_ds_index_entry, info->index->entries->len - 1))->timestamp_end_ns;

		stream_range->begin_ns = min(stream_range->begin_ns, file_begin_epoch);
		stream_range->end_ns = max(stream_range->end_ns, file_end_epoch);
		stream_range->set = true;
	}

	if (stream_range->set) {
		ret = add_range(group_info, stream_range, "range-ns");
		if (ret) {
			goto end;
		}
	}

	status = bt_private_value_map_insert_entry(group_info, "paths",
		bt_value_borrow_from_private(file_paths));
	if (status != BT_VALUE_STATUS_OK) {
		ret = -1;
		goto end;
	}

	ret = add_stream_ids(group_info,
		bt_stream_borrow_from_private(group->stream));
	if (ret) {
		goto end;
	}
end:
	bt_object_put_ref(file_paths);
	return ret;
}

static
int populate_trace_info(const char *trace_path, const char *trace_name,
		struct bt_private_value *trace_info)
{
	int ret = 0;
	size_t group_idx;
	struct ctf_fs_trace *trace = NULL;
	enum bt_value_status status;
	struct bt_private_value *file_groups;
	struct range trace_range = {
		.begin_ns = INT64_MAX,
		.end_ns = 0,
		.set = false,
	};
	struct range trace_intersection = {
		.begin_ns = 0,
		.end_ns = INT64_MAX,
		.set = false,
	};

	file_groups = bt_private_value_array_create();
	if (!file_groups) {
		goto end;
	}

	status = bt_private_value_map_insert_string_entry(trace_info, "name",
		trace_name);
	if (status != BT_VALUE_STATUS_OK) {
		ret = -1;
		goto end;
	}
	status = bt_private_value_map_insert_string_entry(trace_info, "path",
			trace_path);
	if (status != BT_VALUE_STATUS_OK) {
		ret = -1;
		goto end;
	}

	trace = ctf_fs_trace_create(trace_path, trace_name, NULL);
	if (!trace) {
		BT_LOGE("Failed to create fs trace at \'%s\'", trace_path);
		ret = -1;
		goto end;
	}

	BT_ASSERT(trace->ds_file_groups);
	/* Add trace range info only if it contains streams. */
	if (trace->ds_file_groups->len == 0) {
		ret = -1;
		goto end;
	}

	/* Find range of all stream groups, and of the trace. */
	for (group_idx = 0; group_idx < trace->ds_file_groups->len;
			group_idx++) {
		struct bt_private_value *group_info;
		struct range group_range = { .set = false };
		struct ctf_fs_ds_file_group *group = g_ptr_array_index(
				trace->ds_file_groups, group_idx);

		group_info = bt_private_value_map_create();
		if (!group_info) {
			ret = -1;
			goto end;
		}

		ret = populate_stream_info(group, group_info, &group_range);
		if (ret) {
			bt_object_put_ref(group_info);
			goto end;
		}

		if (group_range.set) {
			trace_range.begin_ns = min(trace_range.begin_ns,
					group_range.begin_ns);
			trace_range.end_ns = max(trace_range.end_ns,
					group_range.end_ns);
			trace_range.set = true;

			trace_intersection.begin_ns = max(trace_intersection.begin_ns,
					group_range.begin_ns);
			trace_intersection.end_ns = min(trace_intersection.end_ns,
					group_range.end_ns);
			trace_intersection.set = true;
			status = bt_private_value_array_append_element(
				file_groups,
				bt_value_borrow_from_private(group_info));
			bt_object_put_ref(group_info);
			if (status != BT_VALUE_STATUS_OK) {
				goto end;
			}
		}
	}

	ret = add_range(trace_info, &trace_range, "range-ns");
	if (ret) {
		goto end;
	}

	if (trace_intersection.begin_ns < trace_intersection.end_ns) {
		ret = add_range(trace_info, &trace_intersection,
				"intersection-range-ns");
		if (ret) {
			goto end;
		}
	}

	status = bt_private_value_map_insert_entry(trace_info, "streams",
		bt_value_borrow_from_private(file_groups));
	BT_OBJECT_PUT_REF_AND_RESET(file_groups);
	if (status != BT_VALUE_STATUS_OK) {
		ret = -1;
		goto end;
	}

end:
	bt_object_put_ref(file_groups);
	ctf_fs_trace_destroy(trace);
	return ret;
}

BT_HIDDEN
struct bt_component_class_query_method_return trace_info_query(
		struct bt_component_class *comp_class,
		struct bt_value *params)
{
	struct bt_component_class_query_method_return query_ret = {
		.result = NULL,
		.status = BT_QUERY_STATUS_OK,
	};

	struct bt_private_value *result = NULL;
	struct bt_value *path_value = NULL;
	int ret = 0;
	const char *path = NULL;
	GList *trace_paths = NULL;
	GList *trace_names = NULL;
	GList *tp_node = NULL;
	GList *tn_node = NULL;
	GString *normalized_path = NULL;

	BT_ASSERT(params);

	if (!bt_value_is_map(params)) {
		BT_LOGE("Query parameters is not a map value object.");
		query_ret.status = BT_QUERY_STATUS_INVALID_PARAMS;
		goto error;
	}

	path_value = bt_value_map_borrow_entry_value(params, "path");
	path = bt_value_string_get(path_value);

	normalized_path = bt_common_normalize_path(path, NULL);
	if (!normalized_path) {
		BT_LOGE("Failed to normalize path: `%s`.", path);
		goto error;
	}
	BT_ASSERT(path);

	ret = ctf_fs_find_traces(&trace_paths, normalized_path->str);
	if (ret) {
		goto error;
	}

	trace_names = ctf_fs_create_trace_names(trace_paths,
		normalized_path->str);
	if (!trace_names) {
		BT_LOGE("Cannot create trace names from trace paths.");
		goto error;
	}

	result = bt_private_value_array_create();
	if (!result) {
		query_ret.status = BT_QUERY_STATUS_NOMEM;
		goto error;
	}

	query_ret.result = bt_value_borrow_from_private(result);

	/* Iterates over both trace paths and names simultaneously. */
	for (tp_node = trace_paths, tn_node = trace_names; tp_node;
			tp_node = g_list_next(tp_node),
			tn_node = g_list_next(tn_node)) {
		GString *trace_path = tp_node->data;
		GString *trace_name = tn_node->data;
		enum bt_value_status status;
		struct bt_private_value *trace_info;

		trace_info = bt_private_value_map_create();
		if (!trace_info) {
			BT_LOGE("Failed to create trace info map.");
			goto error;
		}

		ret = populate_trace_info(trace_path->str, trace_name->str,
			trace_info);
		if (ret) {
			bt_object_put_ref(trace_info);
			goto error;
		}

		status = bt_private_value_array_append_element(result,
			bt_value_borrow_from_private(trace_info));
		bt_object_put_ref(trace_info);
		if (status != BT_VALUE_STATUS_OK) {
			goto error;
		}
	}

	goto end;

error:
	BT_OBJECT_PUT_REF_AND_RESET(result);
	query_ret.result = NULL;

	if (query_ret.status >= 0) {
		query_ret.status = BT_QUERY_STATUS_ERROR;
	}

end:
	if (normalized_path) {
		g_string_free(normalized_path, TRUE);
	}
	if (trace_paths) {
		for (tp_node = trace_paths; tp_node; tp_node = g_list_next(tp_node)) {
			if (tp_node->data) {
				g_string_free(tp_node->data, TRUE);
			}
		}
		g_list_free(trace_paths);
	}
	if (trace_names) {
		for (tn_node = trace_names; tn_node; tn_node = g_list_next(tn_node)) {
			if (tn_node->data) {
				g_string_free(tn_node->data, TRUE);
			}
		}
		g_list_free(trace_names);
	}
	/* "path" becomes invalid with the release of path_value. */
	return query_ret;
}
