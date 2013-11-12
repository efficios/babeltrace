#ifndef BABELTRACE_CTF_WRITER_REF_INTERNAL_H
#define BABELTRACE_CTF_WRITER_REF_INTERNAL_H

/*
 * BabelTrace - CTF Writer: Reference count
 *
 * Copyright 2013 EfficiOS Inc.
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

#include <assert.h>

struct bt_ctf_ref {
	long refcount;
};

static inline
void bt_ctf_ref_init(struct bt_ctf_ref *ref)
{
	assert(ref);
	ref->refcount = 1;
}

static inline
void bt_ctf_ref_get(struct bt_ctf_ref *ref)
{
	assert(ref);
	ref->refcount++;
}

static inline
void bt_ctf_ref_put(struct bt_ctf_ref *ref,
		void (*release)(struct bt_ctf_ref *))
{
	assert(ref);
	assert(release);
	if ((--ref->refcount) == 0) {
		release(ref);
	}
}

#endif /* BABELTRACE_CTF_WRITER_REF_INTERNAL_H */
