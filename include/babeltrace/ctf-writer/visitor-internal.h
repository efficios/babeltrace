#ifndef BABELTRACE_CTF_WRITER_VISITOR_INTERNAL_H
#define BABELTRACE_CTF_WRITER_VISITOR_INTERNAL_H

/*
 * BabelTrace - CTF writer: Visitor internal
 *
 * Copyright 2016 Jérémie Galarneau <jeremie.galarneau@efficios.com>
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

#include <babeltrace/ctf-writer/visitor.h>
#include <babeltrace/babeltrace-internal.h>

typedef void *(*bt_ctf_child_accessor)(void *object, int index);
typedef int64_t (*bt_ctf_child_count_accessor)(void *object);
typedef int (*bt_ctf_child_visitor)(void *object, bt_ctf_visitor visitor,
		void *data);

struct bt_ctf_visitor_object {
	enum bt_ctf_visitor_object_type type;
	void *object;
};

BT_HIDDEN
int bt_ctf_visitor_helper(struct bt_ctf_visitor_object *root,
		bt_ctf_child_count_accessor child_counter,
		bt_ctf_child_accessor child_accessor,
		bt_ctf_child_visitor child_visitor,
		bt_ctf_visitor visitor,
		void *data);

#endif /* BABELTRACE_CTF_WRITER_VISITOR_INTERNAL_H */
