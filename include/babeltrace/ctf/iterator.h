#ifndef _BABELTRACE_CTF_ITERATOR_H
#define _BABELTRACE_CTF_ITERATOR_H

/*
 * BabelTrace
 *
 * CTF iterator API
 *
 * Copyright 2011-2012 EfficiOS Inc. and Linux Foundation
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

#include <babeltrace/iterator.h>

#ifdef __cplusplus
extern "C" {
#endif

struct bt_ctf_iter;
struct bt_ctf_event;

/*
 * bt_ctf_iter_create - Allocate a CTF trace collection iterator.
 *
 * begin_pos and end_pos are optional parameters to specify the position
 * at which the trace collection should be seeked upon iterator
 * creation, and the position at which iteration will start returning
 * "EOF".
 *
 * By default, if begin_pos is NULL, a BT_SEEK_CUR is performed at
 * creation. By default, if end_pos is NULL, a BT_SEEK_END (end of
 * trace) is the EOF criterion.
 *
 * Return a pointer to the newly allocated iterator.
 *
 * Only one iterator can be created against a context. If more than one
 * iterator is being created for the same context, the second creation
 * will return NULL. The previous iterator must be destroyed before
 * creation of the new iterator for this function to succeed.
 */
struct bt_ctf_iter *bt_ctf_iter_create(struct bt_context *ctx,
		const struct bt_iter_pos *begin_pos,
		const struct bt_iter_pos *end_pos);

 /*
 * bt_ctf_iter_create_intersect - Allocate a CTF trace collection
 * iterator corresponding to the timerange when all streams are active
 * simultaneously.
 *
 * On success, return a pointer to the newly allocated iterator. The
 * out parameters inter_begin_pos and inter_end_pos are also set to
 * correspond to the beginning and end of the intersection,
 * respectively.
 *
 * On failure, return NULL.
 */
struct bt_ctf_iter *bt_ctf_iter_create_intersect(struct bt_context *ctx,
		struct bt_iter_pos **inter_begin_pos,
		struct bt_iter_pos **inter_end_pos);

/*
 * bt_ctf_get_iter - get iterator from ctf iterator.
 */
struct bt_iter *bt_ctf_get_iter(struct bt_ctf_iter *iter);

/*
 * bt_ctf_iter_destroy - Free a CTF trace collection iterator.
 */
void bt_ctf_iter_destroy(struct bt_ctf_iter *iter);

/*
 * bt_ctf_iter_read_event: Read the iterator's current event data.
 *
 * @iter: trace collection iterator (input). Should NOT be NULL.
 *
 * Return current event on success, NULL on end of trace.
 */
struct bt_ctf_event *bt_ctf_iter_read_event(struct bt_ctf_iter *iter);

/*
 * bt_ctf_iter_read_event_flags: Read the iterator's current event data.
 *
 * @iter: trace collection iterator (input). Should NOT be NULL.
 * @flags: pointer passed by the user, in which the trace reader populates
 * flags on special condition (BT_ITER_FLAG_*).
 *
 * Return current event on success, NULL on end of trace.
 */
struct bt_ctf_event *bt_ctf_iter_read_event_flags(struct bt_ctf_iter *iter,
		int *flags);

/*
 * bt_ctf_get_lost_events_count: returns the number of events discarded
 * immediately prior to the last event read
 *
 * @iter: trace collection iterator (input). Should NOT be NULL.
 *
 * Return the number of lost events or -1ULL on error.
 */
uint64_t bt_ctf_get_lost_events_count(struct bt_ctf_iter *iter);

#ifdef __cplusplus
}
#endif

#endif /* _BABELTRACE_CTF_ITERATOR_H */
