#ifndef BABELTRACE_OBJECT_POOL_INTERNAL_H
#define BABELTRACE_OBJECT_POOL_INTERNAL_H

/*
 * Copyright (c) 2018 EfficiOS Inc. and Linux Foundation
 * Copyright (c) 2018 Philippe Proulx <pproulx@efficios.com>
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

/*
 * This is a generic object pool to avoid memory allocation/deallocation
 * for objects of which the lifespan is typically short, but which are
 * created a lot.
 *
 * The object pool, thanks to two user functions, knows how to allocate
 * a brand new object in memory when the pool is empty and how to
 * destroy an object when we destroy the pool.
 *
 * The object pool's user is responsible for:
 *
 * * Setting whatever references the object needs to keep and reset some
 *   properties _after_ calling bt_object_pool_create_object(). This is
 *   typically done in the bt_*_create() function which calls
 *   bt_object_pool_create_object() (which could call the user-provided
 *   allocation function if the pool is empty) and then sets the
 *   appropriate properties on the possibly recycled object.
 *
 * * Releasing whatever references the object keeps _before_ calling
 *   bt_object_pool_recycle_object(). This is typically done in a custom
 *   bt_*_recycle() function which does the necessary before calling
 *   bt_object_pool_recycle_object() with an object ready to be reused
 *   at any time.
 */

#include <glib.h>
#include <babeltrace/object-internal.h>

typedef void *(*bt_object_pool_new_object_func)(void *data);
typedef void *(*bt_object_pool_destroy_object_func)(void *obj, void *data);

struct bt_object_pool {
	/*
	 * Container of recycled objects, owned by this. The array's size
	 * is the pool's capacity.
	 */
	GPtrArray *objects;

	/*
	 * Pool's size, that is, number of elements in the array above,
	 * starting at index 0, which exist as recycled objects.
	 */
	size_t size;

	/* User functions */
	struct {
		/* Allocate a new object in memory */
		bt_object_pool_new_object_func new_object;

		/* Free direct and indirect memory occupied by object */
		bt_object_pool_destroy_object_func destroy_object;
	} funcs;

	/* User data passed to user functions */
	void *data;
};

/*
 * Initializes an object pool which is already allocated.
 */
int bt_object_pool_initialize(struct bt_object_pool *pool,
		bt_object_pool_new_object_func new_object_func,
		bt_object_pool_destroy_object_func destroy_object_func,
		void *data);

/*
 * Finalizes an object pool without deallocating it.
 */
void bt_object_pool_finalize(struct bt_object_pool *pool);

/*
 * Creates an object from an object pool. If the pool is empty, this
 * function calls the "new" user function to allocate a new object
 * before returning it. Otherwise this function returns a recycled
 * object, removing it from the pool.
 *
 * The returned object is owned by the caller.
 */
static inline
void *bt_object_pool_create_object(struct bt_object_pool *pool)
{
	struct bt_object *obj;

	BT_ASSERT(pool);

#ifdef BT_LOGV
	BT_LOGV("Creating object from pool: pool-addr=%p, pool-size=%zu, pool-cap=%u",
		pool, pool->size, pool->objects->len);
#endif

	if (pool->size > 0) {
		/* Pick one from the pool */
		pool->size--;
		obj = pool->objects->pdata[pool->size];
		pool->objects->pdata[pool->size] = NULL;
		goto end;
	}

	/* Pool is empty: create a brand new object */
#ifdef BT_LOGV
	BT_LOGV("Pool is empty: allocating new object: pool-addr=%p",
		pool);
#endif

	obj = pool->funcs.new_object(pool->data);

end:
#ifdef BT_LOGV
	BT_LOGV("Created one object from pool: pool-addr=%p, obj-addr=%p",
		pool, obj);
#endif

	return obj;
}

/*
 * Recycles an object, that is, puts it back into the pool.
 *
 * The pool becomes the sole owner of the object to recycle.
 */
static inline
void bt_object_pool_recycle_object(struct bt_object_pool *pool, void *obj)
{
	struct bt_object *bt_obj = obj;

	BT_ASSERT(pool);
	BT_ASSERT(obj);

#ifdef BT_LOGV
	BT_LOGV("Recycling object: pool-addr=%p, pool-size=%zu, pool-cap=%u, obj-addr=%p",
		pool, pool->size, pool->objects->len, obj);
#endif

	if (pool->size == pool->objects->len) {
		/* Backing array is full: make place for recycled object */
#ifdef BT_LOGV
		BT_LOGV("Object pool is full: increasing object pool capacity: "
			"pool-addr=%p, old-pool-cap=%u, new-pool-cap=%u",
			pool, pool->objects->len, pool->objects->len + 1);
#endif
		g_ptr_array_set_size(pool->objects, pool->size + 1);
	}

	/* Reset reference count to 1 since it could be 0 now */
	bt_obj->ref_count.count = 1;

	/* Back to the pool */
	pool->objects->pdata[pool->size] = obj;
	pool->size++;

#ifdef BT_LOGV
	BT_LOGV("Recycled object: pool-addr=%p, pool-size=%zu, pool-cap=%u, obj-addr=%p",
		pool, pool->size, pool->objects->len, obj);
#endif
}

#endif /* BABELTRACE_OBJECT_POOL_INTERNAL_H */
