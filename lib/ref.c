/*
 * ref.c: reference counting
 *
 * Babeltrace Library
 *
 * Copyright (c) 2015 Jérémie Galarneau <jeremie.galarneau@efficios.com>
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

#define BT_LOG_TAG "REF"
#include <babeltrace/lib-logging-internal.h>

#include <babeltrace/assert-pre-internal.h>
#include <babeltrace/ref-internal.h>
#include <babeltrace/object-internal.h>

void *bt_get(void *ptr)
{
	struct bt_object *obj = ptr;

	if (unlikely(!obj)) {
		goto end;
	}

	BT_ASSERT_PRE(obj->is_shared, "Object is not shared: addr=%p", obj);

	if (unlikely(!obj->ref_count.release)) {
		goto end;
	}

	if (unlikely(obj->parent && bt_object_get_ref_count(obj) == 0)) {
		BT_LOGV("Incrementing object's parent's reference count: "
			"addr=%p, parent-addr=%p", ptr, obj->parent);
		bt_get(obj->parent);
	}
	BT_LOGV("Incrementing object's reference count: %lu -> %lu: "
		"addr=%p, cur-count=%lu, new-count=%lu",
		obj->ref_count.count, obj->ref_count.count + 1,
		ptr,
		obj->ref_count.count, obj->ref_count.count + 1);
	bt_ref_get(&obj->ref_count);

end:
	return obj;
}

void bt_put(void *ptr)
{
	struct bt_object *obj = ptr;

	if (unlikely(!obj)) {
		return;
	}

	BT_ASSERT_PRE(obj->is_shared, "Object is not shared: addr=%p", obj);

	if (unlikely(!obj->ref_count.release)) {
		return;
	}

	if (BT_LOG_ON_WARN && unlikely(bt_object_get_ref_count(obj) == 0)) {
		BT_LOGW("Decrementing a reference count set to 0: addr=%p",
			ptr);
	}

	BT_LOGV("Decrementing object's reference count: %lu -> %lu: "
		"addr=%p, cur-count=%lu, new-count=%lu",
		obj->ref_count.count, obj->ref_count.count - 1,
		ptr,
		obj->ref_count.count, obj->ref_count.count - 1);
	bt_ref_put(&obj->ref_count);
}
