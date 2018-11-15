#ifndef BABELTRACE_TRACE_IR_FIELD_PATH_INTERNAL
#define BABELTRACE_TRACE_IR_FIELD_PATH_INTERNAL

/*
 * BabelTrace - Trace IR: Field path
 *
 * Copyright 2016 Philippe Proulx <pproulx@efficios.com>
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
 *
 * The Common Trace Format (CTF) Specification is available at
 * http://www.efficios.com/ctf
 */

#include <babeltrace/object-internal.h>
#include <babeltrace/trace-ir/field-path.h>
#include <babeltrace/assert-internal.h>
#include <glib.h>

struct bt_field_path {
	struct bt_object base;
	enum bt_scope root;

	/* Array of `uint64_t` (indexes) */
	GArray *indexes;
};

BT_HIDDEN
struct bt_field_path *bt_field_path_create(void);

static inline
uint64_t bt_field_path_get_index_by_index_inline(
		struct bt_field_path *field_path, uint64_t index)
{
	BT_ASSERT(field_path);
	BT_ASSERT(index < field_path->indexes->len);
	return g_array_index(field_path->indexes, uint64_t, index);
}

#endif /* BABELTRACE_TRACE_IR_FIELD_PATH_INTERNAL */
