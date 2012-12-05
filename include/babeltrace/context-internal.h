#ifndef _BABELTRACE_CONTEXT_INTERNAL_H
#define _BABELTRACE_CONTEXT_INTERNAL_H

/*
 * BabelTrace
 *
 * context header (internal)
 *
 * Copyright 2012 EfficiOS Inc. and Linux Foundation
 *
 * Author: Mathieu Desnoyers <mathieu.desnoyers@efficios.com>
 *         Julien Desfossez <julien.desfossez@efficios.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <glib.h>

struct trace_collection;
struct GHashTable;

/*
 * The context represents the object in which a trace_collection is
 * open. As long as this structure is allocated, the trace_collection is
 * open and the traces it contains can be read and seeked by the
 * iterators and callbacks.
 *
 * It has to be created with the bt_context_create() function and
 * destroyed by calling one more bt_context_put() than bt_context_get()
 */
struct bt_context {
	struct trace_collection *tc;
	GHashTable *trace_handles;
	int refcount;
	int last_trace_handle_id;
	struct bt_iter *current_iterator;
};

#endif /* _BABELTRACE_CONTEXT_INTERNAL_H */
