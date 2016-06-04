/*
 * visitor.c
 *
 * Babeltrace CTF IR - Visitor
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

#include <babeltrace/ctf-ir/visitor-internal.h>
#include <babeltrace/ref.h>

BT_HIDDEN
int visitor_helper(struct bt_ctf_ir_element *root,
		bt_child_count_accessor child_counter,
		bt_child_accessor child_accessor,
		bt_child_visitor child_visitor,
		bt_ctf_ir_visitor visitor,
		void *data)
{
	int ret, child_count, i;

	ret = visitor(root, data);
	if (ret) {
		goto end;
	}

	child_count = child_counter(root->element);
	if (child_count < 0) {
		ret = child_count;
		goto end;
	}

	for (i = 0; i < child_count; i++) {
		void *child;

		child = child_accessor(root->element, i);
		if (!child) {
			ret = -1;
			goto end;
		}
		ret = child_visitor(child, visitor, data);
		BT_PUT(child);
	        if (ret) {
			goto end;
		}
	}
end:
	return ret;
}

enum bt_ctf_ir_type bt_ctf_ir_element_get_type(
		struct bt_ctf_ir_element *element)
{
	enum bt_ctf_ir_type ret = BT_CTF_IR_TYPE_UNKNOWN;
	
	if (!element) {
		goto end;
	}

	ret = element->type;
end:
	return ret;
}

void *bt_ctf_ir_element_get_element(struct bt_ctf_ir_element *element)
{
	void *ret = NULL;

	if (!element) {
		goto end;
	}

	ret = element->element;
end:
	return ret;
}
