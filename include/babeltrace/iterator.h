#ifndef _BABELTRACE_ITERATOR_H
#define _BABELTRACE_ITERATOR_H

/*
 * BabelTrace API Iterators
 *
 * Copyright 2010-2011 - Mathieu Desnoyers <mathieu.desnoyers@efficios.com>
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

#include <babeltrace/format.h>
#include <babeltrace/context.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Flags for the iterator read_event */
enum {
	BT_ITER_FLAG_LOST_EVENTS	= (1 << 0),
	BT_ITER_FLAG_RETRY		= (1 << 1),
};

/* Forward declarations */
struct bt_iter;
struct bt_saved_pos;

/*
 * bt_iter is an abstract class, each format has to implement its own
 * iterator derived from this parent class.
 */

/*
 * bt_iter_pos
 *
 * This structure represents the position where to set an iterator.
 *
 * type represents the type of seek to use.
 * u is the argument of the seek if necessary :
 * - seek_time is the real timestamp to seek to when using BT_SEEK_TIME, it
 *   is expressed in nanoseconds
 * - restore is a position saved with bt_iter_get_pos, it is used with
 *   BT_SEEK_RESTORE.
 *
 * Note about BT_SEEK_LAST: if many events happen to be at the last
 * timestamp, it is implementation-defined which event will be the last,
 * and the order of events with the same timestamp may not be the same
 * as normal iteration on the trace. Therefore, it is recommended to
 * only use BT_SEEK_LAST to get the timestamp of the last event(s) in
 * the trace.
 */
enum bt_iter_pos_type {
	BT_SEEK_TIME,		/* uses u.seek_time */
	BT_SEEK_RESTORE,	/* uses u.restore */
	BT_SEEK_CUR,
	BT_SEEK_BEGIN,
	BT_SEEK_LAST,
};

struct bt_iter_pos {
	enum bt_iter_pos_type type;
	union {
		uint64_t seek_time;
		struct bt_saved_pos *restore;
	} u;
};

/*
 * bt_iter_next: Move trace collection position to the next event.
 *
 * Returns 0 on success, a negative value on error
 */
int bt_iter_next(struct bt_iter *iter);

/*
 * bt_iter_get_pos - Get the current iterator position.
 *
 * The position returned by this function needs to be freed by
 * bt_iter_free_pos after use.
 */
struct bt_iter_pos *bt_iter_get_pos(struct bt_iter *iter);

/*
 * bt_iter_free_pos - Free the position.
 */
void bt_iter_free_pos(struct bt_iter_pos *pos);

/*
 * bt_iter_set_pos: move the iterator to a given position.
 *
 * On error, the stream_heap is reinitialized and returned empty.
 *
 * Return 0 for success.
 *
 * Return EOF if the position requested is after the last event of the
 * trace collection.
 * Return -EINVAL when called with invalid parameter.
 * Return -ENOMEM if the stream_heap could not be properly initialized.
 */
int bt_iter_set_pos(struct bt_iter *iter, const struct bt_iter_pos *pos);

/*
 * bt_iter_create_time_pos: create a position based on time
 *
 * This function allocates and returns a new bt_iter_pos (which must be freed
 * with bt_iter_free_pos) to be able to restore an iterator position based on a
 * real timestamp.
 */
struct bt_iter_pos *bt_iter_create_time_pos(struct bt_iter *iter,
		uint64_t timestamp);

#ifdef __cplusplus
}
#endif

#endif /* _BABELTRACE_ITERATOR_H */
