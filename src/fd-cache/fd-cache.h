/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright 2019 Francis Deslauriers <francis.deslauriers@efficios.com>
 *
 * Babeltrace - File descriptor cache
 */

#ifndef BABELTRACE_FD_CACHE_INTERNAL_H
#define BABELTRACE_FD_CACHE_INTERNAL_H

#include "common/macros.h"

struct bt_fd_cache_handle {
	int fd;
};

struct bt_fd_cache {
	int log_level;
	GHashTable *cache;
};

static inline
int bt_fd_cache_handle_get_fd(struct bt_fd_cache_handle *handle)
{
	return handle->fd;
}

int bt_fd_cache_init(struct bt_fd_cache *fdc, int log_level);

void bt_fd_cache_fini(struct bt_fd_cache *fdc);

struct bt_fd_cache_handle *bt_fd_cache_get_handle(struct bt_fd_cache *fdc,
		const char *path);

void bt_fd_cache_put_handle(struct bt_fd_cache *fdc,
		struct bt_fd_cache_handle *handle);

#endif /* BABELTRACE_FD_CACHE_INTERNAL_H */
