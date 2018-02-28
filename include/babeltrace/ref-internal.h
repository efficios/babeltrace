#ifndef BABELTRACE_REF_INTERNAL_H
#define BABELTRACE_REF_INTERNAL_H

/*
 * Babeltrace - Reference Counting
 *
 * Copyright 2013, 2014 Jérémie Galarneau <jeremie.galarneau@efficios.com>
 *
 * Author: Jérémie Galarneau <jeremie.galarneau@efficios.com>
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
#include <babeltrace/assert-internal.h>

struct bt_object;
typedef void (*bt_object_release_func)(struct bt_object *);

struct bt_ref {
	unsigned long count;
	bt_object_release_func release;
};

static inline
void bt_ref_init(struct bt_ref *ref, bt_object_release_func release)
{
	BT_ASSERT(ref);
	ref->count = 1;
	ref->release = release;
}

static inline
void bt_ref_get(struct bt_ref *ref)
{
	BT_ASSERT(ref);

	if (unlikely(!ref->release)) {
		return;
	}

	ref->count++;
	/* Overflow check. */
	BT_ASSERT(ref->count);
}

static inline
void bt_ref_put(struct bt_ref *ref)
{
	BT_ASSERT(ref);
	/* Only assert if the object has opted-in for reference counting. */
	if (unlikely((--ref->count) == 0 && ref->release)) {
		ref->release((struct bt_object *) ref);
	}
}

#endif /* BABELTRACE_REF_INTERNAL_H */
