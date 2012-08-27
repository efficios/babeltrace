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

#ifdef __cplusplus
extern "C" {
#endif

/* struct bt_context is opaque to the user */
struct bt_context;
struct stream_pos;
struct bt_ctf_event;

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
 * path is the path to the trace, it is not recursive. If "path" is NULL,
 * stream_list is used instead as a list of mmap streams to open for the trace.
 *
 * format is a string containing the format name in which the trace was
 * produced.
 *
 * packet_seek can be NULL to use the default packet_seek handler provided by
 * the trace format. If non-NULL, it is used as an override of the handler for
 * seeks across packets. It takes as parameter a stream position, the packet
 * index it needs to seek to (for SEEK_SET), and a "whence" parameter (either
 * SEEK_CUR: seek to next packet, or SEEK_SET: seek to packet at packet index).
 *
 * stream_list is a linked list of streams, it is used to open a trace where
 * the trace data is located in memory mapped areas instead of trace files,
 * this argument should be set to NULL when path is NULL.
 *
 * The metadata parameter acts as a metadata override when not NULL, otherwise
 * the format handles the metadata opening.
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
 * Effectively closing the trace. Return negative error value if trace
 * is not in context.
 */
int bt_context_remove_trace(struct bt_context *ctx, int trace_id);

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

/*
 * bt_ctf_get_context : get the context associated with an event
 *
 * Returns NULL on error
 */
struct bt_context *bt_ctf_event_get_context(const struct bt_ctf_event *event);

#ifdef __cplusplus
}
#endif

#endif /* _BABELTRACE_CONTEXT_H */
