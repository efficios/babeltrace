/*
 * Copyright 2019 Philippe Proulx <pproulx@efficios.com>
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

#define BT_LOG_TAG "CTFSER"
#include "logging.h"

#include <unistd.h>
#include <string.h>
#include <inttypes.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <babeltrace/assert-internal.h>
#include <stdarg.h>
#include <ctype.h>
#include <glib.h>
#include <stdlib.h>
#include <stdio.h>
#include <wchar.h>
#include <stdbool.h>
#include <babeltrace/babeltrace-internal.h>
#include <babeltrace/common-internal.h>
#include <babeltrace/ctfser-internal.h>
#include <babeltrace/compat/unistd-internal.h>
#include <babeltrace/compat/fcntl-internal.h>

static inline
uint64_t get_packet_size_increment_bytes(void)
{
	return bt_common_get_page_size() * 8;
}

static inline
void mmap_align_ctfser(struct bt_ctfser *ctfser)
{
	ctfser->base_mma = mmap_align(ctfser->cur_packet_size_bytes, PROT_WRITE,
		MAP_SHARED, ctfser->fd, ctfser->mmap_offset);
}

BT_HIDDEN
int _bt_ctfser_increase_cur_packet_size(struct bt_ctfser *ctfser)
{
	int ret;

	BT_ASSERT(ctfser);
	BT_LOGV("Increasing stream file's current packet size: "
		"path=\"%s\", fd=%d, "
		"offset-in-cur-packet-bits=%" PRIu64 ", "
		"cur-packet-size-bytes=%" PRIu64,
		ctfser->path->str, ctfser->fd,
		ctfser->offset_in_cur_packet_bits,
		ctfser->cur_packet_size_bytes);
	ret = munmap_align(ctfser->base_mma);
	if (ret) {
		BT_LOGE_ERRNO("Failed to perform an aligned memory unmapping",
			": ret=%d", ret);
		goto end;
	}

	ctfser->cur_packet_size_bytes += get_packet_size_increment_bytes();

	do {
		ret = bt_posix_fallocate(ctfser->fd, ctfser->mmap_offset,
			ctfser->cur_packet_size_bytes);
	} while (ret == EINTR);

	if (ret) {
		BT_LOGE("Failed to preallocate memory space: ret=%d", ret);
		goto end;
	}

	mmap_align_ctfser(ctfser);
	if (ctfser->base_mma == MAP_FAILED) {
		BT_LOGE_ERRNO("Failed to perform an aligned memory mapping",
			": ret=%d", ret);
		ret = -1;
		goto end;
	}

	BT_LOGV("Increased packet size: "
		"path=\"%s\", fd=%d, "
		"offset-in-cur-packet-bits=%" PRIu64 ", "
		"new-packet-size-bytes=%" PRIu64,
		ctfser->path->str, ctfser->fd,
		ctfser->offset_in_cur_packet_bits,
		ctfser->cur_packet_size_bytes);

end:
	return ret;
}

BT_HIDDEN
int bt_ctfser_init(struct bt_ctfser *ctfser, const char *path)
{
	int ret = 0;

	BT_ASSERT(ctfser);
	memset(ctfser, 0, sizeof(*ctfser));
	ctfser->fd = open(path, O_RDWR | O_CREAT | O_TRUNC,
		S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);
	if (ctfser->fd < 0) {
		BT_LOGW_ERRNO("Failed to open stream file for writing",
			": path=\"%s\", ret=%d",
			path, ctfser->fd);
		ret = -1;
		goto end;
	}

	ctfser->path = g_string_new(path);

end:
	return ret;
}

BT_HIDDEN
int bt_ctfser_fini(struct bt_ctfser *ctfser)
{
	int ret = 0;

	if (ctfser->fd == -1) {
		goto free_path;
	}

	/*
	 * Truncate the stream file's size to the minimum required to
	 * fit the last packet as we might have grown it too much during
	 * the last memory map.
	 */
	do {
		ret = ftruncate(ctfser->fd, ctfser->stream_size_bytes);
	} while (ret == -1 && errno == EINTR);

	if (ret) {
		BT_LOGE_ERRNO("Failed to truncate stream file",
			": ret=%d, size-bytes=%" PRIu64,
			ret, ctfser->stream_size_bytes);
		goto end;
	}

	if (ctfser->base_mma) {
		/* Unmap old base */
		ret = munmap_align(ctfser->base_mma);
		if (ret) {
			BT_LOGE_ERRNO("Failed to unmap stream file",
				": ret=%d, size-bytes=%" PRIu64,
				ret, ctfser->stream_size_bytes);
			goto end;
		}

		ctfser->base_mma = NULL;
	}

	ret = close(ctfser->fd);
	if (ret) {
		BT_LOGE_ERRNO("Failed to close stream file",
			": ret=%d", ret);
		goto end;
	}

	ctfser->fd = -1;

free_path:
	if (ctfser->path) {
		g_string_free(ctfser->path, TRUE);
		ctfser->path = NULL;
	}

end:
	return ret;
}

BT_HIDDEN
int bt_ctfser_open_packet(struct bt_ctfser *ctfser)
{
	int ret = 0;

	BT_LOGV("Opening packet: path=\"%s\", fd=%d, "
		"prev-packet-size-bytes=%" PRIu64,
		ctfser->path->str, ctfser->fd,
		ctfser->prev_packet_size_bytes);

	if (ctfser->base_mma) {
		/* Unmap old base (previous packet) */
		ret = munmap_align(ctfser->base_mma);
		if (ret) {
			BT_LOGE_ERRNO("Failed to unmap stream file",
				": ret=%d, size-bytes=%" PRIu64,
				ret, ctfser->stream_size_bytes);
			goto end;
		}

		ctfser->base_mma = NULL;
	}

	/*
	 * Add the previous packet's size to the memory map address
	 * offset to start writing immediately after it.
	 */
	ctfser->mmap_offset += ctfser->prev_packet_size_bytes;
	ctfser->prev_packet_size_bytes = 0;

	/* Make initial space for the current packet */
	ctfser->cur_packet_size_bytes = get_packet_size_increment_bytes();

	do {
		ret = bt_posix_fallocate(ctfser->fd, ctfser->mmap_offset,
			ctfser->cur_packet_size_bytes);
	} while (ret == EINTR);

	if (ret) {
		BT_LOGE("Failed to preallocate memory space: ret=%d", ret);
		goto end;
	}

	/* Start writing at the beginning of the current packet */
	ctfser->offset_in_cur_packet_bits = 0;

	/* Get new base address */
	mmap_align_ctfser(ctfser);
	if (ctfser->base_mma == MAP_FAILED) {
		BT_LOGE_ERRNO("Failed to perform an aligned memory mapping",
			": ret=%d", ret);
		ret = -1;
		goto end;
	}

	BT_LOGV("Opened packet: path=\"%s\", fd=%d, "
		"cur-packet-size-bytes=%" PRIu64,
		ctfser->path->str, ctfser->fd,
		ctfser->cur_packet_size_bytes);

end:
	return ret;
}

BT_HIDDEN
void bt_ctfser_close_current_packet(struct bt_ctfser *ctfser,
		uint64_t packet_size_bytes)
{
	BT_LOGV("Closing packet: path=\"%s\", fd=%d, "
		"offset-in-cur-packet-bits=%" PRIu64
		"cur-packet-size-bytes=%" PRIu64,
		ctfser->path->str, ctfser->fd,
		ctfser->offset_in_cur_packet_bits,
		ctfser->cur_packet_size_bytes);

	/*
	 * This will be used during the next call to
	 * bt_ctfser_open_packet(): we add
	 * `ctfser->prev_packet_size_bytes` to the current memory map
	 * address offset (first byte of _this_ packet), effectively
	 * making _this_ packet the required size.
	 */
	ctfser->prev_packet_size_bytes = packet_size_bytes;
	ctfser->stream_size_bytes += packet_size_bytes;
	BT_LOGV("Closed packet: path=\"%s\", fd=%d, "
		"stream-file-size-bytes=%" PRIu64,
		ctfser->path->str, ctfser->fd,
		ctfser->stream_size_bytes);
}
