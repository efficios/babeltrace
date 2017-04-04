/*
 * Copyright 2016 - Philippe Proulx <pproulx@efficios.com>
 * Copyright 2010-2011 - EfficiOS Inc. and Linux Foundation
 *
 * Some functions are based on older functions written by Mathieu Desnoyers.
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

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <assert.h>
#include <glib.h>
#include <babeltrace/compat/uuid-internal.h>
#include <babeltrace/compat/memstream-internal.h>

#define PRINT_ERR_STREAM	ctf_fs->error_fp
#define PRINT_PREFIX		"ctf-fs-metadata"
#include "print.h"

#include "fs.h"
#include "file.h"
#include "metadata.h"
#include "../common/metadata/decoder.h"

#define NSEC_PER_SEC 1000000000LL

BT_HIDDEN
FILE *ctf_fs_metadata_open_file(const char *trace_path)
{
	GString *metadata_path = g_string_new(trace_path);
	FILE *fp = NULL;

	if (!metadata_path) {
		goto error;
	}

	g_string_append(metadata_path, "/" CTF_FS_METADATA_FILENAME);
	fp = fopen(metadata_path->str, "rb");
	if (!fp) {
		goto error;
	}

	goto end;

error:
	if (fp) {
		fclose(fp);
		fp = NULL;
	}

end:
	g_string_free(metadata_path, TRUE);
	return fp;
}

static struct ctf_fs_file *get_file(struct ctf_fs_component *ctf_fs,
		const char *trace_path)
{
	struct ctf_fs_file *file = ctf_fs_file_create(ctf_fs);

	if (!file) {
		goto error;
	}

	g_string_append(file->path, trace_path);
	g_string_append(file->path, "/" CTF_FS_METADATA_FILENAME);

	if (ctf_fs_file_open(ctf_fs, file, "rb")) {
		goto error;
	}

	goto end;

error:
	if (file) {
		ctf_fs_file_destroy(file);
		file = NULL;
	}

end:
	return file;
}

int ctf_fs_metadata_set_trace(struct ctf_fs_component *ctf_fs)
{
	int ret = 0;
	struct ctf_fs_file *file = NULL;
	struct ctf_metadata_decoder *metadata_decoder = NULL;

	file = get_file(ctf_fs, ctf_fs->trace_path->str);
	if (!file) {
		PERR("Cannot create metadata file object\n");
		goto end;
	}

	metadata_decoder = ctf_metadata_decoder_create(ctf_fs->error_fp,
		ctf_fs->options.clock_offset * NSEC_PER_SEC +
		ctf_fs->options.clock_offset_ns);
	if (!metadata_decoder) {
		PERR("Cannot create metadata decoder object\n");
		goto end;
	}

	ret = ctf_metadata_decoder_decode(metadata_decoder, file->fp);
	if (ret) {
		PERR("Cannot decode metadata file\n");
		goto end;
	}

	ctf_fs->metadata->trace = ctf_metadata_decoder_get_trace(
		metadata_decoder);
	assert(ctf_fs->metadata->trace);
	ret = -1;

end:
	ctf_fs_file_destroy(file);
	ctf_metadata_decoder_destroy(metadata_decoder);
	return ret;
}

int ctf_fs_metadata_init(struct ctf_fs_metadata *metadata)
{
	/* Nothing to initialize for the moment. */
	return 0;
}

void ctf_fs_metadata_fini(struct ctf_fs_metadata *metadata)
{
	if (metadata->text) {
		free(metadata->text);
	}

	if (metadata->trace) {
		BT_PUT(metadata->trace);
	}
}
