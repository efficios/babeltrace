/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright 2016 Philippe Proulx <pproulx@efficios.com>
 *
 * The Common Trace Format (CTF) Specification is available at
 * http://www.efficios.com/ctf
 */

#ifndef BABELTRACE_CTF_WRITER_FIELD_PATH_INTERNAL
#define BABELTRACE_CTF_WRITER_FIELD_PATH_INTERNAL

#include "common/common.h"
#include "common/assert.h"
#include "common/macros.h"
#include <babeltrace2-ctf-writer/field-types.h>
#include <glib.h>

#include "object.h"

struct bt_ctf_field_path {
	struct bt_ctf_object base;
	enum bt_ctf_scope root;

	/*
	 * Array of integers (int) indicating the index in either
	 * structures, variants, arrays, or sequences that make up
	 * the path to a field type. -1 means the "current element
	 * of an array or sequence type".
	 */
	GArray *indexes;
};

struct bt_ctf_field_path *bt_ctf_field_path_create(void);

void bt_ctf_field_path_clear(struct bt_ctf_field_path *field_path);

struct bt_ctf_field_path *bt_ctf_field_path_copy(
		struct bt_ctf_field_path *path);

enum bt_ctf_scope bt_ctf_field_path_get_root_scope(
		const struct bt_ctf_field_path *field_path);

int64_t bt_ctf_field_path_get_index_count(
		const struct bt_ctf_field_path *field_path);

int bt_ctf_field_path_get_index(
		const struct bt_ctf_field_path *field_path, uint64_t index);

#endif /* BABELTRACE_CTF_WRITER_FIELD_PATH_INTERNAL */
