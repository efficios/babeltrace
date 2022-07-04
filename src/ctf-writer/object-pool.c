/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2018 EfficiOS Inc. and Linux Foundation
 * Copyright (c) 2018 Philippe Proulx <pproulx@efficios.com>
 */

#define BT_LOG_TAG "CTF-WRITER/OBJECT-POOL"
#include "logging.h"

#include <stdint.h>

#include "common/assert.h"

#include "object-pool.h"

BT_EXPORT
int bt_ctf_object_pool_initialize(struct bt_ctf_object_pool *pool,
		bt_ctf_object_pool_new_object_func new_object_func,
		bt_ctf_object_pool_destroy_object_func destroy_object_func,
		void *data)
{
	int ret = 0;

	BT_ASSERT_DBG(new_object_func);
	BT_ASSERT_DBG(destroy_object_func);
	BT_LOGD("Initializing object pool: addr=%p, data-addr=%p",
		pool, data);
	pool->objects = g_ptr_array_new();
	if (!pool->objects) {
		BT_LOGE_STR("Failed to allocate a GPtrArray.");
		goto error;
	}

	pool->funcs.new_object = new_object_func;
	pool->funcs.destroy_object = destroy_object_func;
	pool->data = data;
	pool->size = 0;
	BT_LOGD("Initialized object pool.");
	goto end;

error:
	if (pool) {
		bt_ctf_object_pool_finalize(pool);
	}

	ret = -1;

end:
	return ret;
}

BT_EXPORT
void bt_ctf_object_pool_finalize(struct bt_ctf_object_pool *pool)
{
	uint64_t i;

	BT_ASSERT_DBG(pool);
	BT_LOGD("Finalizing object pool.");

	if (pool->objects) {
		for (i = 0; i < pool->size; i++) {
			void *obj = pool->objects->pdata[i];

			if (obj) {
				pool->funcs.destroy_object(obj, pool->data);
			}
		}

		g_ptr_array_free(pool->objects, TRUE);
		pool->objects = NULL;
	}
}
