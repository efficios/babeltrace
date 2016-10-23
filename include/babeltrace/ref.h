#ifndef BABELTRACE_REF_H
#define BABELTRACE_REF_H

/*
 * BabelTrace: common reference counting
 *
 * Copyright (c) 2015 EfficiOS Inc. and Linux Foundation
 * Copyright (c) 2015 Philippe Proulx <pproulx@efficios.com>
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

#include <babeltrace/babeltrace-internal.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * BT_PUT: calls bt_put() with a variable, then sets this variable to NULL.
 *
 * A common action with Babeltrace objects is to create or get one, perform
 * an action with it, and then put it. To avoid putting it a second time
 * later (if an error occurs, for example), the variable is often reset
 * to NULL after putting the object it points to. Since this is so
 * common, the BT_PUT() macro can be used to do just that.
 *
 * It is safe to call this function with a variable containing NULL.
 *
 * @param obj Variable pointing to a Babeltrace object.
 */
#define BT_PUT(_obj)		\
	do {			\
		bt_put(_obj);	\
		(_obj) = NULL;	\
	} while (0)

/*
 * BT_MOVE: transfers the ownership of an object, setting the old owner to NULL.
 *
 * This macro sets the variable _dst to the value of the variable _src,
 * then sets _src to NULL, effectively moving the ownership of an
 * object from one variable to the other.
 *
 * Before assigning _src to _dst, it puts _dst. Therefore it is not safe to
 * call this function with an uninitialized value of _dst.
 *
 * @param _dst Destination variable pointing to a Babeltrace object.
 * @param _src Source variable pointing to a Babeltrace object.
 */
#define BT_MOVE(_dst, _src)	\
	do {			\
		bt_put(_dst);	\
		(_dst) = (_src);\
		(_src) = NULL;	\
	} while (0)

/*
 * bt_get: increments the reference count of a Babeltrace object.
 *
 * The same number of bt_get() and bt_put() (plus one extra bt_put() to release
 * the initial reference acquired at creation) have to be performed to destroy a
 * Babeltrace object.
 *
 * It is safe to call this function with a NULL object.
 *
 * @param obj Babeltrace object.
 *
 * Returns obj.
 */
BT_HIDDEN
void *bt_get(void *obj);

/*
 * bt_put: decrements the reference count of a Babeltrace object.
 *
 * The same number of bt_get() and bt_put() (plus one extra bt_put() to release
 * bt_put() to release the initial reference done at creation) have to be
 * performed to destroy a Babeltrace object.
 *
 * The object is freed when its reference count is decremented to 0 by a call to
 * bt_put().
 *
 * It is safe to call this function with a NULL object.
 *
 * @param obj Babeltrace object.
 */
BT_HIDDEN
void bt_put(void *obj);

#ifdef __cplusplus
}
#endif

#endif /* BABELTRACE_REF_H */
