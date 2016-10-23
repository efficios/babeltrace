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

#include <babeltrace/ref-internal.h>
#include <babeltrace/object-internal.h>

BT_HIDDEN
void *bt_get(void *ptr)
{
	struct bt_object *obj = ptr;

	if (!obj) {
		goto end;
	}

	if (obj->parent && bt_object_get_ref_count(obj) == 0) {
		bt_get(obj->parent);
	}
	bt_ref_get(&obj->ref_count);
end:
	return obj;
}

BT_HIDDEN
void bt_put(void *ptr)
{
	struct bt_object *obj = ptr;

	if (!obj) {
		return;
	}

	bt_ref_put(&obj->ref_count);
}
