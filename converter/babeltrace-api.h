#ifndef _BABELTRACE_LIB_H
#define _BABELTRACE_LIB_H

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

#include <babeltrace/types.h>
#include <babeltrace/format.h>
#include <babeltrace/ctf/types.h>
#include <babeltrace/ctf-ir/metadata.h>

/*
 * struct babeltrace_iter: data structure representing an iterator on a trace
 * collection.
 */
struct babeltrace_iter {
	struct ptr_heap *stream_heap;
	struct trace_collection *tc;
};

struct babeltrace_iter_pos {
	GPtrArray *pos; /* struct babeltrace_iter_stream_pos */
};

struct babeltrace_iter_stream_pos {
	struct stream_pos parent;
	ssize_t offset;
	size_t cur_index;
};

/*
 * Initialization/teardown.
 */
struct babeltrace_iter *babeltrace_iter_create(struct trace_collection *tc);
void babeltrace_iter_destroy(struct babeltrace_iter *iter);

/*
 * Move within the trace.
 */
/*
 * babeltrace_iter_next: Move stream position to the next event.
 *
 * Returns 0 on success, a negative value on error
 */
int babeltrace_iter_next(struct babeltrace_iter *iter);

/* Get the current position for each stream of the trace */
struct babeltrace_iter_pos *
babeltrace_iter_get_pos(struct babeltrace_iter *iter);

/* The position needs to be freed after use */
void babeltrace_iter_free_pos(struct babeltrace_iter_pos *pos);

/* Seek the trace to the position */
int babeltrace_iter_seek_pos(struct babeltrace_iter *iter,
		struct babeltrace_iter_pos *pos);

/*
 * babeltrace_iter_seek_time: Seek the trace to the given timestamp.
 *
 * Return EOF if timestamp is after the last event of the trace.
 * Return other negative value for other errors.
 * Return 0 for success.
 */
int babeltrace_iter_seek_time(struct babeltrace_iter *iter,
		uint64_t timestamp);

/*
 * babeltrace_iter_read_event: Read the current event data.
 *
 * @iter: trace iterator (input)
 * @stream: stream containing event at current position (output)
 * @event: current event (output)
 * Return 0 on success, negative error value on error.
 */
int babeltrace_iter_read_event(struct babeltrace_iter *iter,
		struct ctf_stream **stream,
		struct ctf_stream_event **event);

#endif /* _BABELTRACE_LIB_H */
