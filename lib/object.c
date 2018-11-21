/*
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

#define BT_LOG_TAG "OBJECT"
#include <babeltrace/lib-logging-internal.h>

#include <babeltrace/assert-pre-internal.h>
#include <babeltrace/object-internal.h>

void *bt_object_get_ref(void *ptr)
{
	struct bt_object *obj = ptr;

	if (unlikely(!obj)) {
		goto end;
	}

	BT_ASSERT_PRE(obj->is_shared, "Object is not shared: addr=%p", obj);
	bt_object_get_no_null_check(obj);

end:
	return ptr;
}

void bt_object_put_ref(void *ptr)
{
	struct bt_object *obj = ptr;

	if (unlikely(!obj)) {
		return;
	}

	BT_ASSERT_PRE(obj->is_shared, "Object is not shared: addr=%p", obj);
	BT_ASSERT_PRE(bt_object_get_ref_count(obj) > 0,
		"Decrementing a reference count set to 0: addr=%p", ptr);
	bt_object_put_no_null_check(obj);
}
