/*
 * fd-cache.c
 *
 * Babeltrace - File descriptor cache
 *
 * Copyright 2019 Francis Deslauriers <francis.deslauriers@efficios.com>
 *
 * Author: Francis Deslauriers <francis.deslauriers@efficios.com>
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

#define BT_LOG_TAG "FD-CACHE"
#include "logging.h"

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <glib.h>

#include <babeltrace/assert-internal.h>
#include <babeltrace/fd-cache-internal.h>

struct file_key {
	uint64_t dev;
	uint64_t ino;
};

struct fd_handle_internal {
	struct bt_fd_cache_handle fd_handle;
	uint64_t ref_count;
	struct file_key *key;
};

static
void fd_cache_handle_internal_destroy(
		struct fd_handle_internal *internal_fd)
{
	if (!internal_fd) {
		goto end;
	}

	if (internal_fd->fd_handle.fd >= 0) {
		close(internal_fd->fd_handle.fd);
		internal_fd->fd_handle.fd = -1;
	}

end:
	g_free(internal_fd);
}

/*
 * Using simple hash algorithm found on stackoverflow:
 * https://stackoverflow.com/questions/664014/
 */
static inline
uint64_t hash_uint64_t(uint64_t x) {
	x = (x ^ (x >> 30)) * UINT64_C(0xbf58476d1ce4e5b9);
	x = (x ^ (x >> 27)) * UINT64_C(0x94d049bb133111eb);
	x = x ^ (x >> 31);
	return x;
}

static
guint file_key_hash(gconstpointer v)
{
	const struct file_key *fk = v;
	return hash_uint64_t(fk->dev) ^ hash_uint64_t(fk->ino);
}

static
gboolean file_key_equal(gconstpointer v1, gconstpointer v2)
{
	const struct file_key *fk1 = v1;
	const struct file_key *fk2 = v2;

	return (fk1->dev == fk2->dev) && (fk1->ino == fk2->ino);
}

static
void file_key_destroy(gpointer data)
{
	struct file_key *fk = data;
	g_free(fk);
}

BT_HIDDEN
int bt_fd_cache_init(struct bt_fd_cache *fdc)
{
	int ret = 0;

	fdc->cache = g_hash_table_new_full(file_key_hash, file_key_equal,
		file_key_destroy, (GDestroyNotify) fd_cache_handle_internal_destroy);
	if (!fdc->cache) {
		ret = -1;
	}

	return ret;
}

BT_HIDDEN
void bt_fd_cache_fini(struct bt_fd_cache *fdc)
{
	BT_ASSERT(fdc->cache);
	/*
	 * All handle should have been removed for the hashtable at this point.
	 */
	BT_ASSERT(g_hash_table_size(fdc->cache) == 0);
	g_hash_table_destroy(fdc->cache);

	return;
}

BT_HIDDEN
struct bt_fd_cache_handle *bt_fd_cache_get_handle(struct bt_fd_cache *fdc,
		const char *path)
{
	struct fd_handle_internal *fd_internal = NULL;
	struct stat statbuf;
	struct file_key fk;
	int ret;

	ret = stat(path, &statbuf);
	if (ret < 0) {
		BT_LOGE_ERRNO("Failed to stat file", ": path=%s", path);
		goto end;
	}

	/*
	 * Use the device number and inode number to uniquely identify a file.
	 * Even if the file as the same path, it may have been replaced so we
	 * must open a new FD for it. This replacement of file is more likely
	 * to happen with a lttng-live source component.
	 */
	fk.dev = statbuf.st_dev;
	fk.ino = statbuf.st_ino;

	fd_internal = g_hash_table_lookup(fdc->cache, &fk);
	if (!fd_internal) {
		gboolean ret;
		struct file_key *file_key;

		int fd = open(path, O_RDONLY);
		if (fd < 0) {
			BT_LOGE_ERRNO("Failed to open file", "path=%s", path);
			goto error;
		}

		fd_internal = g_new0(struct fd_handle_internal, 1);
		if (!fd_internal) {
			BT_LOGE("Failed to allocate fd internal handle");
			goto error;
		}

		file_key = g_new0(struct file_key, 1);
		if (!fd_internal) {
			BT_LOGE("Failed to allocate file key");
			goto error;
		}

		*file_key = fk;

		fd_internal->fd_handle.fd = fd;
		fd_internal->ref_count = 0;
		fd_internal->key = file_key;

		/* Insert the newly created fd handle. */
		ret = g_hash_table_insert(fdc->cache, fd_internal->key,
				fd_internal);
		BT_ASSERT(ret);
	}

	BT_ASSERT(fd_internal->ref_count >= 0);

	fd_internal->ref_count++;
	goto end;

error:
	fd_cache_handle_internal_destroy(fd_internal);
	fd_internal = NULL;
end:
	return (struct bt_fd_cache_handle *) fd_internal;
}

BT_HIDDEN
void bt_fd_cache_put_handle(struct bt_fd_cache *fdc,
		struct bt_fd_cache_handle *handle)
{
	struct fd_handle_internal *fd_internal;

	if (!handle) {
		goto end;
	}

	fd_internal = (struct fd_handle_internal *) handle;

	BT_ASSERT(fd_internal->ref_count > 0);

	if (fd_internal->ref_count > 1) {
		fd_internal->ref_count--;
	} else {
		gboolean ret;
		int close_ret;

		close_ret = close(fd_internal->fd_handle.fd);
		if (close_ret == -1) {
			BT_LOGW_ERRNO("Failed to close file descriptor",
				": fd=%d", fd_internal->fd_handle.fd);
		}
		ret = g_hash_table_remove(fdc->cache, fd_internal->key);
		BT_ASSERT(ret);
	}

end:
	return;
}
