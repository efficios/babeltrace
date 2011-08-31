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

/*
 * babeltrace_iter_create - Allocate a trace collection iterator.
 */
struct babeltrace_iter *babeltrace_iter_create(struct trace_collection *tc);

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
 * babeltrace_iter_get_pos - Get the current trace collection position.
 *
 * The position returned by this function needs to be freed by
 * babeltrace_iter_free_pos after use.
 */
struct babeltrace_iter_pos *
	babeltrace_iter_get_pos(struct babeltrace_iter *iter);

/*
 * babeltrace_iter_free_pos - Free the position.
 */
void babeltrace_iter_free_pos(struct babeltrace_iter_pos *pos);

/*
 * babeltrace_iter_seek_pos - Seek the trace collection to the position.
 */
int babeltrace_iter_seek_pos(struct babeltrace_iter *iter,
		struct babeltrace_iter_pos *pos);

/*
 * babeltrace_iter_seek_time: Seek the trace collection to the given timestamp.
 *
 * Return EOF if timestamp is after the last event of the trace collection.
 * Return other negative value for other errors.
 * Return 0 for success.
 */
int babeltrace_iter_seek_time(struct babeltrace_iter *iter,
		uint64_t timestamp);

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
