#ifndef _BABELTRACE_ITERATOR_INTERNAL_H
#define _BABELTRACE_ITERATOR_INTERNAL_H

/*
 * BabelTrace
 *
 * Internal iterator header
 *
 * Copyright 2010-2011 EfficiOS Inc. and Linux Foundation
 *
 * Author: Mathieu Desnoyers <mathieu.desnoyers@efficios.com>
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

#include <babeltrace/ctf/events.h>

/*
 * struct bt_iter: data structure representing an iterator on a trace
 * collection.
 */
struct bt_iter {
	struct ptr_heap *stream_heap;
	struct bt_context *ctx;
	const struct bt_iter_pos *end_pos;
};

/*
 * bt_iter_create - Allocate a trace collection iterator.
 *
 * begin_pos and end_pos are optional parameters to specify the position
 * at which the trace collection should be seeked upon iterator
 * creation, and the position at which iteration will start returning
 * "EOF".
 *
 * By default, if begin_pos is NULL, a BT_SEEK_CUR is performed at
 * creation. By default, if end_pos is NULL, a BT_SEEK_END (end of
 * trace) is the EOF criterion.
 */
struct bt_iter *bt_iter_create(struct bt_context *ctx,
		const struct bt_iter_pos *begin_pos,
		const struct bt_iter_pos *end_pos);

/*
 * bt_iter_destroy - Free a trace collection iterator.
 */
void bt_iter_destroy(struct bt_iter *iter);

int bt_iter_init(struct bt_iter *iter,
		struct bt_context *ctx,
		const struct bt_iter_pos *begin_pos,
		const struct bt_iter_pos *end_pos);
void bt_iter_fini(struct bt_iter *iter);

#endif /* _BABELTRACE_ITERATOR_INTERNAL_H */
