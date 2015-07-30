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

/**
 * All objects publicly exposed by Babeltrace APIs must contain this structure
 * as their first member. This allows the unification of all ref counting
 * mechanism and may be used to provide more base functionality to all
 * objects.
 */
struct bt_object {
	struct bt_ref ref_count;
};

static inline
void bt_object_init(void *obj, bt_object_release_func release)
{
	bt_ref_init(&((struct bt_object *) obj)->ref_count, release);
}

#endif /* BABELTRACE_OBJECT_INTERNAL_H */
