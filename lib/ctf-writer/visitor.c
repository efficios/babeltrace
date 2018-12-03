/*
 * visitor.c
 *
 * Babeltrace CTF writer - Visitor
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

#include <babeltrace/ctf-writer/visitor-internal.h>
#include <babeltrace/ctf-writer/object.h>

BT_HIDDEN
int bt_ctf_visitor_helper(struct bt_ctf_visitor_object *root,
		bt_ctf_child_count_accessor child_counter,
		bt_ctf_child_accessor child_accessor,
		bt_ctf_child_visitor child_visitor,
		bt_ctf_visitor visitor,
		void *data)
{
	int ret, child_count, i;

	ret = visitor(root, data);
	if (ret) {
		goto end;
	}

	child_count = child_counter(root->object);
	if (child_count < 0) {
		ret = child_count;
		goto end;
	}

	for (i = 0; i < child_count; i++) {
		void *child;

		child = child_accessor(root->object, i);
		if (!child) {
			ret = -1;
			goto end;
		}
		ret = child_visitor(child, visitor, data);
		BT_CTF_OBJECT_PUT_REF_AND_RESET(child);
	        if (ret) {
			goto end;
		}
	}
end:
	return ret;
}

enum bt_ctf_visitor_object_type bt_ctf_visitor_object_get_type(
		struct bt_ctf_visitor_object *object)
{
	enum bt_ctf_visitor_object_type ret = BT_CTF_VISITOR_OBJECT_TYPE_UNKNOWN;

	if (!object) {
		goto end;
	}

	ret = object->type;
end:
	return ret;
}

void *bt_ctf_visitor_object_get_object(struct bt_ctf_visitor_object *object)
{
	void *ret = NULL;

	if (!object) {
		goto end;
	}

	ret = object->object;
end:
	return ret;
}
