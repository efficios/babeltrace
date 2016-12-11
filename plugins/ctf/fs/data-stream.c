/*
 * Copyright 2016 - Philippe Proulx <pproulx@efficios.com>
 * Copyright 2016 - Jérémie Galarneau <jeremie.galarneau@efficios.com>
 * Copyright 2010-2011 - EfficiOS Inc. and Linux Foundation
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
#include <glib.h>
#include <inttypes.h>
#include <sys/mman.h>
#include <babeltrace/ctf-ir/stream.h>
#include <babeltrace/plugin/notification/iterator.h>
#include "file.h"
#include "metadata.h"
#include "../common/notif-iter/notif-iter.h"
#include <assert.h>
#include "data-stream.h"

#define PRINT_ERR_STREAM	ctf_fs->error_fp
#define PRINT_PREFIX		"ctf-fs-data-stream"
#include "print.h"

static
size_t remaining_mmap_bytes(struct ctf_fs_stream *stream)
{
	return stream->mmap_valid_len - stream->request_offset;
}

static
int stream_munmap(struct ctf_fs_stream *stream)
{
	int ret = 0;
	struct ctf_fs_component *ctf_fs = stream->file->ctf_fs;

	if (munmap(stream->mmap_addr, stream->mmap_len)) {
		PERR("Cannot memory-unmap address %p (size %zu) of file \"%s\" (%p): %s\n",
			stream->mmap_addr, stream->mmap_len,
			stream->file->path->str, stream->file->fp,
			strerror(errno));
		ret = -1;
		goto end;
	}
end:
	return ret;
}

static
int mmap_next(struct ctf_fs_stream *stream)
{
	int ret = 0;
	struct ctf_fs_component *ctf_fs = stream->file->ctf_fs;

	/* Unmap old region */
	if (stream->mmap_addr) {
		if (stream_munmap(stream)) {
			goto error;
		}

		stream->mmap_offset += stream->mmap_valid_len;
		stream->request_offset = 0;
	}

	stream->mmap_valid_len = MIN(stream->file->size - stream->mmap_offset,
			stream->mmap_max_len);
	/* Round up to next page, assuming page size being a power of 2. */
	stream->mmap_len = (stream->mmap_valid_len + ctf_fs->page_size - 1)
			& ~(ctf_fs->page_size - 1);
	/* Map new region */
	stream->mmap_addr = mmap((void *) 0, stream->mmap_len,
			PROT_READ, MAP_PRIVATE, fileno(stream->file->fp),
			stream->mmap_offset);
	if (stream->mmap_addr == MAP_FAILED) {
		PERR("Cannot memory-map address (size %zu) of file \"%s\" (%p) at offset %zu: %s\n",
				stream->mmap_len, stream->file->path->str,
				stream->file->fp, stream->mmap_offset,
				strerror(errno));
		goto error;
	}

	goto end;
error:
	stream_munmap(stream);
	ret = -1;
end:
	return ret;
}

static
enum bt_ctf_notif_iter_medium_status medop_request_bytes(
		size_t request_sz, uint8_t **buffer_addr,
		size_t *buffer_sz, void *data)
{
	enum bt_ctf_notif_iter_medium_status status =
		BT_CTF_NOTIF_ITER_MEDIUM_STATUS_OK;
	struct ctf_fs_stream *stream = data;
	struct ctf_fs_component *ctf_fs = stream->file->ctf_fs;

	if (request_sz == 0) {
		goto end;
	}

	/* Check if we have at least one memory-mapped byte left */
	if (remaining_mmap_bytes(stream) == 0) {
		/* Are we at the end of the file? */
		if (stream->request_offset >= stream->file->size) {
			PDBG("Reached end of file \"%s\" (%p)\n",
				stream->file->path->str, stream->file->fp);
			status = BT_CTF_NOTIF_ITER_MEDIUM_STATUS_EOF;
			goto end;
		}

		if (mmap_next(stream)) {
			PERR("Cannot memory-map next region of file \"%s\" (%p)\n",
				stream->file->path->str, stream->file->fp);
			goto error;
		}
	}

	*buffer_sz = MIN(remaining_mmap_bytes(stream), request_sz);
	*buffer_addr = ((uint8_t *) stream->mmap_addr) + stream->request_offset;
	stream->request_offset += *buffer_sz;
	goto end;

error:
	status = BT_CTF_NOTIF_ITER_MEDIUM_STATUS_ERROR;

end:
	return status;
}

static
struct bt_ctf_stream *medop_get_stream(
		struct bt_ctf_stream_class *stream_class, void *data)
{
	struct ctf_fs_stream *fs_stream = data;
	struct ctf_fs_component *ctf_fs = fs_stream->file->ctf_fs;

	if (!fs_stream->stream) {
		int64_t id = bt_ctf_stream_class_get_id(stream_class);

		PDBG("Creating stream out of stream class %" PRId64 "\n", id);
		fs_stream->stream = bt_ctf_stream_create(stream_class,
				fs_stream->file->path->str);
		if (!fs_stream->stream) {
			PERR("Cannot create stream (stream class %" PRId64 ")\n",
					id);
		}
	}

	return fs_stream->stream;
}

static struct bt_ctf_notif_iter_medium_ops medops = {
	.request_bytes = medop_request_bytes,
	.get_stream = medop_get_stream,
};

BT_HIDDEN
struct ctf_fs_stream *ctf_fs_stream_create(
		struct ctf_fs_component *ctf_fs, struct ctf_fs_file *file)
{
	struct ctf_fs_stream *stream = g_new0(struct ctf_fs_stream, 1);

	if (!stream) {
		goto error;
	}

	stream->file = file;
	stream->notif_iter = bt_ctf_notif_iter_create(ctf_fs->metadata->trace,
			ctf_fs->page_size, medops, stream, ctf_fs->error_fp);
	if (!stream->notif_iter) {
		goto error;
	}

	stream->mmap_max_len = ctf_fs->page_size * 2048;
	goto end;
error:
	/* Do not touch "borrowed" file. */
	stream->file = NULL;
	ctf_fs_stream_destroy(stream);
	stream = NULL;
end:
	return stream;
}

BT_HIDDEN
void ctf_fs_stream_destroy(struct ctf_fs_stream *stream)
{
	if (!stream) {
		return;
	}

	if (stream->file) {
		ctf_fs_file_destroy(stream->file);
	}

	if (stream->stream) {
		BT_PUT(stream->stream);
	}

	if (stream->notif_iter) {
		bt_ctf_notif_iter_destroy(stream->notif_iter);
	}

	g_free(stream);
}
