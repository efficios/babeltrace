#ifndef BABELTRACE_CTF_IR_COMMON_INTERNAL_H
#define BABELTRACE_CTF_IR_COMMON_INTERNAL_H

/*
 * Babeltrace - CTF IR: common data structures
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

#include <babeltrace/ref-internal.h>

/* this must be the base class of _all_ CTF IR classes */
struct bt_ctf_base {
	struct bt_ref ref_count;
};

static inline
void bt_ctf_base_init(void *obj, bt_ref_release_func_t release_func)
{
	struct bt_ctf_base *base = obj;

	bt_ref_init(&base->ref_count, release_func);
}

#endif /* BABELTRACE_CTF_IR_COMMON_INTERNAL_H */
