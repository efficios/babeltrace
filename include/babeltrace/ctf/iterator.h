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
 */

#include <babeltrace/iterator.h>

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

#endif /* _BABELTRACE_CTF_ITERATOR_H */
