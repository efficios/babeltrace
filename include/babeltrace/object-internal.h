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

#include <babeltrace/babeltrace-internal.h>
#include <babeltrace/assert-internal.h>
#include <stdbool.h>

struct bt_object;

typedef void (*bt_object_release_func)(struct bt_object *);
typedef void (*bt_object_parent_is_owner_listener_func)(
		struct bt_object *);

static inline
void bt_object_get_no_null_check(struct bt_object *obj);

static inline
void bt_object_put_no_null_check(struct bt_object *obj);

/*
 * Babeltrace object base.
 *
 * All objects publicly exposed by Babeltrace APIs must contain this
 * object as their first member.
 */
struct bt_object {
	/*
	 * True if this object is shared, that is, it has a reference
	 * count.
	 */
	bool is_shared;

	/*
	 * Current reference count.
	 */
	unsigned long long ref_count;

	/*
	 * Release function called when the object's reference count
	 * falls to zero. For an object with a parent, this function is
	 * bt_object_with_parent_release_func(), which calls
	 * `spec_release_func` below if there's no current parent.
	 */
	bt_object_release_func release_func;

	/*
	 * Specific release function called by
	 * bt_object_with_parent_release_func() or directly by a
	 * parent object.
	 */
	bt_object_release_func spec_release_func;

	/*
	 * Optional callback for an object with a parent, called by
	 * bt_object_with_parent_release_func() to indicate to the
	 * object that its parent is its owner.
	 */
	bt_object_parent_is_owner_listener_func
		parent_is_owner_listener_func;

	/*
	 * Optional parent object.
	 */
	struct bt_object *parent;
};

static inline
unsigned long long bt_object_get_ref_count(struct bt_object *obj)
{
	BT_ASSERT(obj);
	BT_ASSERT(obj->is_shared);
	return obj->ref_count;
}

static inline
struct bt_object *bt_object_borrow_parent(struct bt_object *obj)
{
	BT_ASSERT(obj);
	BT_ASSERT(obj->is_shared);
	return obj->parent;
}

static inline
struct bt_object *bt_object_get_parent(struct bt_object *obj)
{
	struct bt_object *parent = bt_object_borrow_parent(obj);

	if (parent) {
		bt_object_get_no_null_check(parent);
	}

	return parent;
}

static inline
void bt_object_set_parent(struct bt_object *child, struct bt_object *parent)
{
	BT_ASSERT(child);
	BT_ASSERT(child->is_shared);

#ifdef BT_LOGV
	BT_LOGV("Setting object's parent: addr=%p, parent-addr=%p",
		child, parent);
#endif

	/*
	 * It is assumed that a "child" having a parent is publicly
	 * reachable. Therefore, a reference to its parent must be
	 * taken. The reference to the parent will be released once the
	 * object's reference count falls to zero.
	 */
	if (parent) {
		BT_ASSERT(!child->parent);
		child->parent = parent;
		bt_object_get_no_null_check(parent);
	} else {
		if (child->parent) {
			bt_object_put_no_null_check(child->parent);
		}

		child->parent = NULL;
	}
}

static inline
void bt_object_try_spec_release(struct bt_object *obj)
{
	BT_ASSERT(obj);
	BT_ASSERT(obj->is_shared);
	BT_ASSERT(obj->spec_release_func);

	if (bt_object_get_ref_count(obj) == 0) {
		obj->spec_release_func(obj);
	}
}

static inline
void bt_object_with_parent_release_func(struct bt_object *obj)
{
	if (obj->parent) {
		/*
		 * Keep our own copy of the parent address because `obj`
		 * could be destroyed in
		 * obj->parent_is_owner_listener_func().
		 */
		struct bt_object *parent = obj->parent;

#ifdef BT_LOGV
		BT_LOGV("Releasing parented object: addr=%p, ref-count=%llu, "
			"parent-addr=%p, parent-ref-count=%llu",
			obj, obj->ref_count,
			parent, parent->ref_count);
#endif

		if (obj->parent_is_owner_listener_func) {
			/*
			 * Object has a chance to destroy itself here
			 * under certain conditions and notify its
			 * parent. At this point the parent is
			 * guaranteed to exist because it's not put yet.
			 */
			obj->parent_is_owner_listener_func(obj);
		}

		/* The release function will be invoked by the parent. */
		bt_object_put_no_null_check(parent);
	} else {
		bt_object_try_spec_release(obj);
	}
}

static inline
void bt_object_init(struct bt_object *obj, bool is_shared,
		bt_object_release_func release_func)
{
	BT_ASSERT(obj);
	BT_ASSERT(!is_shared || release_func);
	obj->is_shared = is_shared;
	obj->release_func = release_func;
	obj->parent_is_owner_listener_func = NULL;
	obj->spec_release_func = NULL;
	obj->parent = NULL;
	obj->ref_count = 1;
}

static inline
void bt_object_init_shared(struct bt_object *obj,
		bt_object_release_func release_func)
{
	bt_object_init(obj, true, release_func);
}

static inline
void bt_object_init_unique(struct bt_object *obj)
{
	bt_object_init(obj, false, NULL);
}

static inline
void bt_object_init_shared_with_parent(struct bt_object *obj,
		bt_object_release_func spec_release_func)
{
	BT_ASSERT(obj);
	BT_ASSERT(spec_release_func);
	bt_object_init_shared(obj, bt_object_with_parent_release_func);
	obj->spec_release_func = spec_release_func;
}

static inline
void bt_object_set_parent_is_owner_listener_func(struct bt_object *obj,
		bt_object_parent_is_owner_listener_func func)
{
	BT_ASSERT(obj);
	BT_ASSERT(obj->is_shared);
	BT_ASSERT(obj->spec_release_func);
	((struct bt_object *) obj)->parent_is_owner_listener_func = func;
}

static inline
void bt_object_inc_ref_count(struct bt_object *obj)
{
	BT_ASSERT(obj);
	BT_ASSERT(obj->is_shared);
	obj->ref_count++;
	BT_ASSERT(obj->ref_count != 0);
}

static inline
void bt_object_get_no_null_check(struct bt_object *obj)
{
	BT_ASSERT(obj);
	BT_ASSERT(obj->is_shared);

	if (unlikely(obj->parent && bt_object_get_ref_count(obj) == 0)) {
#ifdef BT_LOGV
		BT_LOGV("Incrementing object's parent's reference count: "
			"addr=%p, parent-addr=%p", obj, obj->parent);
#endif

		bt_object_get_no_null_check(obj->parent);
	}

#ifdef BT_LOGV
	BT_LOGV("Incrementing object's reference count: %llu -> %llu: "
		"addr=%p, cur-count=%llu, new-count=%llu",
		obj->ref_count, obj->ref_count + 1,
		obj, obj->ref_count, obj->ref_count + 1);
#endif

	bt_object_inc_ref_count(obj);
}

static inline
void bt_object_put_no_null_check(struct bt_object *obj)
{
	BT_ASSERT(obj);
	BT_ASSERT(obj->is_shared);
	BT_ASSERT(obj->ref_count > 0);

#ifdef BT_LOGV
	BT_LOGV("Decrementing object's reference count: %llu -> %llu: "
		"addr=%p, cur-count=%llu, new-count=%llu",
		obj->ref_count, obj->ref_count - 1,
		obj, obj->ref_count, obj->ref_count - 1);
#endif

	obj->ref_count--;

	if (obj->ref_count == 0) {
		BT_ASSERT(obj->release_func);
		obj->release_func(obj);
	}
}

#endif /* BABELTRACE_OBJECT_INTERNAL_H */
