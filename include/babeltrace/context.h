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

/* struct bt_context is opaque to the user */
struct bt_context;

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
 * Open a trace
 *
 * Return: 0 on success, nonzero on failure.
 */
int bt_context_add_trace(struct bt_context *ctx, const char *path,
		const char *format);

/*
 * bt_context_add_traces_recursive: Open a trace recursively
 *
 * Find each trace present in the subdirectory starting from the given
 * path, and add them to the context.
 *
 * Return: 0 on success, nonzero on failure.
 */
int bt_context_add_traces_recursive(struct bt_context *ctx, const char *path,
		const char *format);

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
