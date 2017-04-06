/*
 * fs.c
 *
 * Babeltrace CTF file system Reader Component
 *
 * Copyright 2016 Jérémie Galarneau <jeremie.galarneau@efficios.com>
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

#include <babeltrace/ctf-ir/packet.h>
#include <babeltrace/ctf-ir/clock-class.h>
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
#include <unistd.h>
#include "fs.h"
#include "metadata.h"
#include "data-stream.h"
#include "file.h"
#include "../common/metadata/decoder.h"

#define PRINT_ERR_STREAM	ctf_fs->error_fp
#define PRINT_PREFIX		"ctf-fs"
#include "print.h"
#define METADATA_TEXT_SIG	"/* CTF 1.8"

BT_HIDDEN
bool ctf_fs_debug;

struct bt_notification_iterator_next_return ctf_fs_iterator_next(
		struct bt_private_notification_iterator *iterator)
{
	struct ctf_fs_stream *fs_stream =
		bt_private_notification_iterator_get_user_data(iterator);

	return ctf_fs_stream_next(fs_stream);
}

void ctf_fs_iterator_finalize(struct bt_private_notification_iterator *it)
{
	void *ctf_fs_stream =
		bt_private_notification_iterator_get_user_data(it);

	ctf_fs_stream_destroy(ctf_fs_stream);
}

enum bt_notification_iterator_status ctf_fs_iterator_init(
		struct bt_private_notification_iterator *it,
		struct bt_private_port *port)
{
	struct ctf_fs_stream *stream = NULL;
	struct ctf_fs_component *ctf_fs;
	struct ctf_fs_port_data *port_data;
	struct bt_private_component *priv_comp =
		bt_private_notification_iterator_get_private_component(it);
	enum bt_notification_iterator_status ret =
		BT_NOTIFICATION_ITERATOR_STATUS_OK;

	assert(priv_comp);

	ctf_fs = bt_private_component_get_user_data(priv_comp);
	if (!ctf_fs) {
		ret = BT_NOTIFICATION_ITERATOR_STATUS_INVALID;
		goto error;
	}

	port_data = bt_private_port_get_user_data(port);
	if (!port_data) {
		ret = BT_NOTIFICATION_ITERATOR_STATUS_INVALID;
		goto error;
	}

	stream = ctf_fs_stream_create(ctf_fs, port_data->path->str);
	if (!stream) {
		goto error;
	}

	ret = bt_private_notification_iterator_set_user_data(it, stream);
	if (ret) {
		goto error;
	}

	stream = NULL;
	goto end;

error:
	(void) bt_private_notification_iterator_set_user_data(it, NULL);

	if (ret == BT_NOTIFICATION_ITERATOR_STATUS_OK) {
		ret = BT_NOTIFICATION_ITERATOR_STATUS_ERROR;
	}

end:
	ctf_fs_stream_destroy(stream);
	bt_put(priv_comp);
	return ret;
}

static
void ctf_fs_destroy_data(struct ctf_fs_component *ctf_fs)
{
	if (!ctf_fs) {
		return;
	}

	if (ctf_fs->trace_path) {
		g_string_free(ctf_fs->trace_path, TRUE);
	}

	if (ctf_fs->port_data) {
		g_ptr_array_free(ctf_fs->port_data, TRUE);
	}

	if (ctf_fs->metadata) {
		ctf_fs_metadata_fini(ctf_fs->metadata);
		g_free(ctf_fs->metadata);
	}

	bt_put(ctf_fs->cc_prio_map);
	g_free(ctf_fs);
}

void ctf_fs_finalize(struct bt_private_component *component)
{
	void *data = bt_private_component_get_user_data(component);

	ctf_fs_destroy_data(data);
}

static
void port_data_destroy(void *data) {
	struct ctf_fs_port_data *port_data = data;

	if (!port_data) {
		return;
	}

	if (port_data->path) {
		g_string_free(port_data->path, TRUE);
	}

	g_free(port_data);
}

static
int create_one_port(struct ctf_fs_component *ctf_fs,
		const char *stream_basename, const char *stream_path)
{
	int ret = 0;
	struct bt_private_port *port = NULL;
	struct ctf_fs_port_data *port_data = NULL;
	GString *port_name = NULL;

	port_name = g_string_new(NULL);
	if (!port_name) {
		goto error;
	}

	/* Assign the name for the new output port */
	g_string_assign(port_name, "");
	g_string_printf(port_name, "trace0-stream-%s", stream_basename);
	PDBG("Creating one port named `%s` associated with path `%s`\n",
		port_name->str, stream_path);

	/* Create output port for this file */
	port = bt_private_component_source_add_output_private_port(
		ctf_fs->priv_comp, port_name->str);
	if (!port) {
		goto error;
	}

	port_data = g_new0(struct ctf_fs_port_data, 1);
	if (!port_data) {
		goto error;
	}

	port_data->path = g_string_new(stream_path);
	if (!port_data->path) {
		goto error;
	}

	ret = bt_private_port_set_user_data(port, port_data);
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

	bt_put(port);
	port_data_destroy(port_data);
	return ret;
}

static
int create_ports(struct ctf_fs_component *ctf_fs)
{
	int ret = 0;
	const char *basename;
	GError *error = NULL;
	GDir *dir = NULL;
	struct bt_private_port *def_port;
	struct ctf_fs_file *file = NULL;

	/* Remove default port if needed */
	def_port = bt_private_component_source_get_default_output_private_port(
		ctf_fs->priv_comp);
	if (def_port) {
		bt_private_port_remove_from_component(def_port);
		bt_put(def_port);
	}

	/* Create one output port for each stream file */
	dir = g_dir_open(ctf_fs->trace_path->str, 0, &error);
	if (!dir) {
		PERR("Cannot open directory `%s`: %s (code %d)\n",
			ctf_fs->trace_path->str, error->message,
			error->code);
		goto error;
	}

	while ((basename = g_dir_read_name(dir))) {
		if (!strcmp(basename, CTF_FS_METADATA_FILENAME)) {
			/* Ignore the metadata stream. */
			PDBG("Ignoring metadata file `%s`\n", basename);
			continue;
		}

		if (basename[0] == '.') {
			PDBG("Ignoring hidden file `%s`\n", basename);
			continue;
		}

		/* Create the file. */
		file = ctf_fs_file_create(ctf_fs);
		if (!file) {
			PERR("Cannot create stream file object for file `%s`\n",
				basename);
			goto error;
		}

		/* Create full path string. */
		g_string_append_printf(file->path, "%s/%s",
				ctf_fs->trace_path->str, basename);
		if (!g_file_test(file->path->str, G_FILE_TEST_IS_REGULAR)) {
			PDBG("Ignoring non-regular file `%s`\n", basename);
			ctf_fs_file_destroy(file);
			file = NULL;
			continue;
		}

		ret = ctf_fs_file_open(ctf_fs, file, "rb");
		if (ret) {
			PERR("Cannot open stream file `%s`\n", basename);
			goto error;
		}

		if (file->size == 0) {
			/* Skip empty stream. */
			PDBG("Ignoring empty file `%s`\n", basename);
			ctf_fs_file_destroy(file);
			file = NULL;
			continue;
		}

		ret = create_one_port(ctf_fs, basename, file->path->str);
		if (ret) {
			PERR("Cannot create output port for file `%s`\n",
				basename);
			goto error;
		}

		ctf_fs_file_destroy(file);
		file = NULL;
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

	ctf_fs_file_destroy(file);
	return ret;
}

static
int create_cc_prio_map(struct ctf_fs_component *ctf_fs)
{
	int ret = 0;
	size_t i;
	int count;

	assert(ctf_fs);
	ctf_fs->cc_prio_map = bt_clock_class_priority_map_create();
	if (!ctf_fs->cc_prio_map) {
		ret = -1;
		goto end;
	}

	count = bt_ctf_trace_get_clock_class_count(ctf_fs->metadata->trace);
	assert(count >= 0);

	for (i = 0; i < count; i++) {
		struct bt_ctf_clock_class *clock_class =
			bt_ctf_trace_get_clock_class(ctf_fs->metadata->trace,
				i);

		assert(clock_class);
		ret = bt_clock_class_priority_map_add_clock_class(
			ctf_fs->cc_prio_map, clock_class, 0);
		BT_PUT(clock_class);

		if (ret) {
			goto end;
		}
	}

end:
	return ret;
}

static
struct ctf_fs_component *ctf_fs_create(struct bt_private_component *priv_comp,
		struct bt_value *params)
{
	struct ctf_fs_component *ctf_fs;
	struct bt_value *value = NULL;
	const char *path;
	int ret;

	ctf_fs = g_new0(struct ctf_fs_component, 1);
	if (!ctf_fs) {
		goto end;
	}

	/*
	 * We don't need to get a new reference here because as long as
	 * our private ctf_fs_component object exists, the containing
	 * private component should also exist.
	 */
	ctf_fs->priv_comp = priv_comp;

	/* FIXME: should probably look for a source URI */
	value = bt_value_map_get(params, "path");
	if (!value || bt_value_is_null(value) || !bt_value_is_string(value)) {
		goto error;
	}

	ret = bt_value_string_get(value, &path);
	if (ret) {
		goto error;
	}

	ctf_fs->port_data = g_ptr_array_new_with_free_func(port_data_destroy);
	if (!ctf_fs->port_data) {
		goto error;
	}

	ctf_fs->trace_path = g_string_new(path);
	if (!ctf_fs->trace_path) {
		BT_PUT(value);
		goto error;
	}
	bt_put(value);

	value = bt_value_map_get(params, "offset-s");
	if (value) {
		int64_t offset;

		if (!bt_value_is_integer(value)) {
			fprintf(stderr,
				"offset-s should be an integer\n");
			goto error;
		}
		ret = bt_value_integer_get(value, &offset);
		if (ret != BT_VALUE_STATUS_OK) {
			fprintf(stderr,
				"Failed to get offset-s value\n");
			goto error;
		}
		ctf_fs->options.clock_offset = offset;
		bt_put(value);
	}

	value = bt_value_map_get(params, "offset-ns");
	if (value) {
		int64_t offset;

		if (!bt_value_is_integer(value)) {
			fprintf(stderr,
				"offset-ns should be an integer\n");
			goto error;
		}
		ret = bt_value_integer_get(value, &offset);
		if (ret != BT_VALUE_STATUS_OK) {
			fprintf(stderr,
				"Failed to get offset-ns value\n");
			goto error;
		}
		ctf_fs->options.clock_offset_ns = offset;
		bt_put(value);
	}

	ctf_fs->error_fp = stderr;
	ctf_fs->page_size = (size_t) getpagesize();

	// FIXME: check error.
	ctf_fs->metadata = g_new0(struct ctf_fs_metadata, 1);
	if (!ctf_fs->metadata) {
		goto error;
	}

	ret = ctf_fs_metadata_set_trace(ctf_fs);
	if (ret) {
		goto error;
	}

	ret = create_cc_prio_map(ctf_fs);
	if (ret) {
		goto error;
	}

	ret = create_ports(ctf_fs);
	if (ret) {
		goto error;
	}

	goto end;

error:
	ctf_fs_destroy_data(ctf_fs);
	ctf_fs = NULL;
end:
	return ctf_fs;
}

BT_HIDDEN
enum bt_component_status ctf_fs_init(struct bt_private_component *priv_comp,
		struct bt_value *params, UNUSED_VAR void *init_method_data)
{
	struct ctf_fs_component *ctf_fs;
	enum bt_component_status ret = BT_COMPONENT_STATUS_OK;

	assert(priv_comp);
	ctf_fs_debug = g_strcmp0(getenv("CTF_FS_DEBUG"), "1") == 0;
	ctf_fs = ctf_fs_create(priv_comp, params);
	if (!ctf_fs) {
		ret = BT_COMPONENT_STATUS_NOMEM;
		goto end;
	}

	ret = bt_private_component_set_user_data(priv_comp, ctf_fs);
	if (ret != BT_COMPONENT_STATUS_OK) {
		goto error;
	}
end:
	return ret;
error:
	(void) bt_private_component_set_user_data(priv_comp, NULL);
        ctf_fs_destroy_data(ctf_fs);
	return ret;
}

BT_HIDDEN
struct bt_value *ctf_fs_query(struct bt_component_class *comp_class,
		const char *object, struct bt_value *params)
{
	struct bt_value *results = NULL;
	struct bt_value *path_value = NULL;
	char *metadata_text = NULL;
	FILE *metadata_fp = NULL;
	GString *g_metadata_text = NULL;

	if (strcmp(object, "metadata-info") == 0) {
		int ret;
		int bo;
		const char *path;
		bool is_packetized;

		results = bt_value_map_create();
		if (!results) {
			goto error;
		}

		if (!bt_value_is_map(params)) {
			fprintf(stderr,
				"Query parameters is not a map value object\n");
			goto error;
		}

		path_value = bt_value_map_get(params, "path");
		ret = bt_value_string_get(path_value, &path);
		if (ret) {
			fprintf(stderr,
				"Cannot get `path` string parameter\n");
			goto error;
		}

		assert(path);
		metadata_fp = ctf_fs_metadata_open_file(path);
		if (!metadata_fp) {
			fprintf(stderr,
				"Cannot open trace at path `%s`\n", path);
			goto error;
		}

		is_packetized = ctf_metadata_decoder_is_packetized(metadata_fp,
			&bo);

		if (is_packetized) {
			ret = ctf_metadata_decoder_packetized_file_stream_to_buf(
				metadata_fp, &metadata_text, bo);
			if (ret) {
				fprintf(stderr,
					"Cannot decode packetized metadata file\n");
				goto error;
			}
		} else {
			long filesize;

			fseek(metadata_fp, 0, SEEK_END);
			filesize = ftell(metadata_fp);
			rewind(metadata_fp);
			metadata_text = malloc(filesize + 1);
			if (!metadata_text) {
				fprintf(stderr,
					"Cannot allocate buffer for metadata text\n");
				goto error;
			}

			if (fread(metadata_text, filesize, 1, metadata_fp) !=
					1) {
				fprintf(stderr,
					"Cannot read metadata file\n");
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

		ret = bt_value_map_insert_string(results, "text",
			g_metadata_text->str);
		if (ret) {
			fprintf(stderr, "Cannot insert metadata text into results\n");
			goto error;
		}

		ret = bt_value_map_insert_bool(results, "is-packetized",
			is_packetized);
		if (ret) {
			fprintf(stderr, "Cannot insert is packetized into results\n");
			goto error;
		}
	} else {
		fprintf(stderr, "Unknown query object `%s`\n", object);
		goto error;
	}

	goto end;

error:
	BT_PUT(results);

end:
	bt_put(path_value);
	free(metadata_text);

	if (g_metadata_text) {
		g_string_free(g_metadata_text, TRUE);
	}

	if (metadata_fp) {
		fclose(metadata_fp);
	}
	return results;
}
