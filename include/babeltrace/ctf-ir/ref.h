#ifndef BABELTRACE_CTF_IR_REF_H
#define BABELTRACE_CTF_IR_REF_H

/*
 * BabelTrace - CTF IR: common reference counting
 *
 * Copyright (c) 2015 EfficiOS Inc. and Linux Foundation
 * Copyright (c) 2015 Philippe Proulx <pproulx@efficios.com>
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

#include <assert.h>
#include <babeltrace/ref-internal.h>
#include <babeltrace/ctf-ir/common-internal.h>

/*
 * BT_CTF_PUT: calls bt_ctf_put() with a variable, then sets this
 * 	variable to NULL.
 *
 * A common action with CTF IR objects is to create or get one, do
 * something with it, and then put it. To avoid putting it a second time
 * later (if an error occurs, for example), the variable is often reset
 * to NULL after putting the object it points to. Since this is so
 * common, you can use the BT_CTF_PUT() macro, which does just that.
 *
 * It is safe to call this function with a NULL object.
 *
 * @param obj CTF IR object.
 */
#define BT_CTF_PUT(_obj)		\
	do {				\
		bt_ctf_put(_obj);	\
		(_obj) = NULL;		\
	} while (0)

/*
 * bt_ctf_get: increments the reference count of a CTF IR object.
 *
 * The same number of bt_ctf_get() and bt_ctf_put() (plus one extra
 * bt_ctf_put() to release the initial reference done at creation) have
 * to be done to destroy a CTF IR object.
 *
 * It is safe to call this function with a NULL object.
 *
 * @param obj CTF IR object.
 */
static inline
void bt_ctf_get(void *obj)
{
	if (obj) {
		struct bt_ctf_base *base = obj;

		bt_ref_get(&base->ref_count);
	}
}

/*
 * bt_ctf_put: decrements the reference count of a CTF IR object.
 *
 * The same number of bt_ctf_get() and bt_ctf_put() (plus one extra
 * bt_ctf_put() to release the initial reference done at creation) have
 * to be done to destroy a CTF IR object.
 *
 * When the object's reference count is decremented to 0 by a call to
 * bt_ctf_put(), the object is freed.
 *
 * It is safe to call this function with a NULL object.
 *
 * @param obj CTF IR object.
 */
static inline
void bt_ctf_put(void *obj)
{
	if (obj) {
		struct bt_ctf_base *base = obj;

		bt_ref_put(&base->ref_count);
	}
}

#endif /* BABELTRACE_CTF_IR_REF_H */
