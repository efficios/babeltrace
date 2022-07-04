/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright 2016 Jérémie Galarneau <jeremie.galarneau@efficios.com>
 */

#ifndef BABELTRACE_CTF_WRITER_VISITOR_INTERNAL_H
#define BABELTRACE_CTF_WRITER_VISITOR_INTERNAL_H

#include <stdlib.h>
#include <stdint.h>
#include <babeltrace2-ctf-writer/visitor.h>
#include "common/macros.h"

typedef void *(*bt_ctf_child_accessor)(void *object, int index);
typedef int64_t (*bt_ctf_child_count_accessor)(void *object);
typedef int (*bt_ctf_child_visitor)(void *object, bt_ctf_visitor visitor,
		void *data);

struct bt_ctf_visitor_object {
	enum bt_ctf_visitor_object_type type;
	void *object;
};

int bt_ctf_visitor_helper(struct bt_ctf_visitor_object *root,
		bt_ctf_child_count_accessor child_counter,
		bt_ctf_child_accessor child_accessor,
		bt_ctf_child_visitor child_visitor,
		bt_ctf_visitor visitor,
		void *data);

#endif /* BABELTRACE_CTF_WRITER_VISITOR_INTERNAL_H */
