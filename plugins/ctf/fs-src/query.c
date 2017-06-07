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
#include <assert.h>
#include "metadata.h"
#include "../common/metadata/decoder.h"
#include <babeltrace/common-internal.h>

#define BT_LOG_TAG "PLUGIN-CTF-FS-QUERY-SRC"
#include "logging.h"

#define METADATA_TEXT_SIG	"/* CTF 1.8"

BT_HIDDEN
struct bt_value *metadata_info_query(struct bt_component_class *comp_class,
		struct bt_value *params)
{
	struct bt_value *results = NULL;
	struct bt_value *path_value = NULL;
	char *metadata_text = NULL;
	FILE *metadata_fp = NULL;
	GString *g_metadata_text = NULL;
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

		if (fread(metadata_text, filesize, 1, metadata_fp) != 1) {
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
