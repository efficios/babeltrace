#ifndef _BABELTRACE_CONTEXT_H
#define _BABELTRACE_CONTEXT_H

/*
 * BabelTrace
 *
 * context header
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
 */

#include <unistd.h>
#include <babeltrace/format.h>

/* struct bt_context is opaque to the user */
struct bt_context;
struct stream_pos;

/*
 * bt_context_create : create a Babeltrace context
 *
 * Allocate a new context. The creation of the context sets the refcount
 * to 1.
 *
 * Returns an allocated context on success and NULL on error
 */
struct bt_context *bt_context_create(void);

/*
 * bt_context_add_trace : Add a trace by path to the context
 *
 * Open a trace.
 *
 * packet_seek can be NULL to use the default packet_seek handler
 * provided by the trace format. If non-NULL, it is used as an override
 * of the handler for seeks across packets. It takes as parameter a
 * stream position, the packet index it needs to seek to (for SEEK_SET),
 * and a "whence" parameter (either SEEK_CUR: seek to next packet, or
 * SEEK_SET: seek to packet at packet index).
 *
 * If "path" is NULL, stream_list is used instread as a list of streams
 * to open for the trace.

 * The metadata parameter acts as a metadata override when not NULL.
 *
 * Return: the trace handle id (>= 0) on success, a negative
 * value on error.
 */
int bt_context_add_trace(struct bt_context *ctx, const char *path,
		const char *format,
		void (*packet_seek)(struct stream_pos *pos,
			size_t index, int whence),
		struct mmap_stream_list *stream_list,
		FILE *metadata);

/*
 * bt_context_remove_trace: Remove a trace from the context.
 *
 * Effectively closing the trace.
 */
void bt_context_remove_trace(struct bt_context *ctx, int trace_id);

/*
 * bt_context_get and bt_context_put : increments and decrement the
 * refcount of the context
 *
 * These functions ensures that the context won't be destroyed when it
 * is in use. The same number of get and put (plus one extra put to
 * release the initial reference done at creation) has to be done to
 * destroy a context.
 *
 * When the context refcount is decremented to 0 by a bt_context_put,
 * the context is freed.
 */
void bt_context_get(struct bt_context *ctx);
void bt_context_put(struct bt_context *ctx);

#endif /* _BABELTRACE_CONTEXT_H */
