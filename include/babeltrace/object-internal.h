#ifndef BABELTRACE_OBJECT_INTERNAL_H
#define BABELTRACE_OBJECT_INTERNAL_H

/*
 * Babeltrace - Base object
 *
 * Copyright 2015 Jérémie Galarneau <jeremie.galarneau@efficios.com>
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

#include <babeltrace/ref-internal.h>
#include <babeltrace/ref.h>

/**
 * All objects publicly exposed by Babeltrace APIs must contain this structure
 * as their first member. This allows the unification of all ref counting
 * mechanism and may be used to provide more base functionality to all
 * objects.
 */
struct bt_object {
	struct bt_ref ref_count;
	/* Class-specific release function. */
	bt_object_release_func release;
	/* @see doc/ref-counting.md */
	struct bt_object *parent;
};

static inline
long bt_object_get_ref_count(const void *);
static inline
void bt_object_set_parent(void *, void *);

static
void bt_object_release(void *ptr)
{
	struct bt_object *obj = ptr;

	if (obj && obj->release && !bt_object_get_ref_count(obj)) {
		obj->release(obj);
	}
}

static
void generic_release(struct bt_object *obj)
{
	if (obj->parent) {
		/* The release function will be invoked by the parent. */
		bt_put(obj->parent);
	} else {
		bt_object_release(obj);
	}
}

static inline
struct bt_object *bt_object_get_parent(void *ptr)
{
	struct bt_object *obj = ptr;

	return ptr ? bt_get(obj->parent) : NULL;
}

static inline
void bt_object_set_parent(void *child_ptr, void *parent)
{
	struct bt_object *child = child_ptr;

	if (!child) {
		return;
	}

	/*
	 * It is assumed that a "child" being "parented" is publicly reachable.
	 * Therefore, a reference to its parent must be taken. The reference
	 * to the parent will be released once the object's reference count
	 * falls to zero.
	 */
	child->parent = bt_get(parent);
}

static inline
void bt_object_init(void *ptr, bt_object_release_func release)
{
	struct bt_object *obj = ptr;

	obj->release = release;
	obj->parent = NULL;
	bt_ref_init(&obj->ref_count, generic_release);
}

static inline
long bt_object_get_ref_count(const void *ptr)
{
	const struct bt_object *obj = ptr;

	return obj->ref_count.count;
}

#endif /* BABELTRACE_OBJECT_INTERNAL_H */
