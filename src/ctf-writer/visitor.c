/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright 2016 Jérémie Galarneau <jeremie.galarneau@efficios.com>
 *
 * Babeltrace CTF writer - Visitor
 */

#include <babeltrace2-ctf-writer/object.h>

#include "common/macros.h"

#include "visitor.h"

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
