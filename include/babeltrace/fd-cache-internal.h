#ifndef BABELTRACE_FD_CACHE_INTERNAL_H
#define BABELTRACE_FD_CACHE_INTERNAL_H
/*
 * fd-cache-internal.h
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

#include <babeltrace/babeltrace-internal.h>

struct bt_fd_cache_handle {
	int fd;
};

struct bt_fd_cache {
	GHashTable *cache;
};

static inline
int bt_fd_cache_handle_get_fd(struct bt_fd_cache_handle *handle)
{
	return handle->fd;
}

BT_HIDDEN
int bt_fd_cache_init(struct bt_fd_cache *fdc);

BT_HIDDEN
void bt_fd_cache_fini(struct bt_fd_cache *fdc);

BT_HIDDEN
struct bt_fd_cache_handle *bt_fd_cache_get_handle(struct bt_fd_cache *fdc,
		const char *path);

BT_HIDDEN
void bt_fd_cache_put_handle(struct bt_fd_cache *fdc,
		struct bt_fd_cache_handle *handle);

#endif /* BABELTRACE_FD_CACHE_INTERNAL_H */
