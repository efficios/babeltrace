#ifndef _BABELTRACE_H
#define _BABELTRACE_H

/*
 * BabelTrace API
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
 */

/* Forward declarations */
struct babeltrace_iter;
struct trace_collection;
struct ctf_stream_event;
struct ctf_stream;
struct babeltrace_saved_pos;

struct trace_collection_pos {
	enum {
		BT_SEEK_TIME,		/* uses u.seek_time */
		BT_SEEK_RESTORE,	/* uses u.restore */
		BT_SEEK_CUR,
		BT_SEEK_BEGIN,
		BT_SEEK_END,
	} type;
	union {
		uint64_t seek_time;
		struct babeltrace_saved_pos *restore;
	} u;
};

/*
 * babeltrace_iter_create - Allocate a trace collection iterator.
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
struct babeltrace_iter *babeltrace_iter_create(struct trace_collection *tc,
		struct trace_collection_pos *begin_pos,
		struct trace_collection_pos *end_pos);

/*
 * babeltrace_iter_destroy - Free a trace collection iterator.
 */
void babeltrace_iter_destroy(struct babeltrace_iter *iter);

/*
 * babeltrace_iter_next: Move trace collection position to the next event.
 *
 * Returns 0 on success, a negative value on error
 */
int babeltrace_iter_next(struct babeltrace_iter *iter);

/*
 * babeltrace_iter_save_pos - Save the current trace collection position.
 *
 * The position returned by this function needs to be freed by
 * babeltrace_iter_free_pos after use.
 */
struct trace_collection_pos *
	babeltrace_iter_save_pos(struct babeltrace_iter *iter);

/*
 * babeltrace_iter_free_pos - Free the position.
 */
void babeltrace_iter_free_pos(struct trace_collection_pos *pos);

/*
 * babeltrace_iter_seek: seek iterator to given position.
 *
 * Return EOF if position is after the last event of the trace collection.
 * Return other negative value for other errors.
 * Return 0 for success.
 */
int babeltrace_iter_seek(struct babeltrace_iter *iter,
		const struct trace_collection_pos *pos);

/*
 * babeltrace_iter_read_event: Read the iterator's current event data.
 *
 * @iter: trace collection iterator (input)
 * @stream: stream containing event at current position (output)
 * @event: current event (output)
 * Return 0 on success, negative error value on error.
 */
int babeltrace_iter_read_event(struct babeltrace_iter *iter,
		struct ctf_stream **stream,
		struct ctf_stream_event **event);

#endif /* _BABELTRACE_H */
