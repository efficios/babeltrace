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

struct trace_collection;

/*
 * The context represents the object in which a trace_collection is open. As
 * long as this structure is allocated, the trace_collection is open and the
 * traces it contains can be read and seeked by the iterators and callbacks.
 *
 * It has to be created with the bt_context_create() function and destroyed by
 * bt_context_destroy()
 */
struct bt_context {
	struct trace_collection *tc;
	int refcount;
	int last_trace_handle_id;
};

/*
 * bt_context_create : create a Babeltrace context
 *
 * Allocate a new context and assign the trace_collection passed in
 * parameter as the context trace_collection. The trace_collection
 * must contain an array of valid trace_descriptors. The creation of
 * the context sets the refcount to 1.
 *
 * Returns an allocated context on success and NULL on error
 */
struct bt_context *bt_context_create(struct trace_collection *tc);

/*
 * bt_context_destroy : destroy a context
 *
 * If the context is still in use (by an iterator or a callback), the
 * destroy fails and returns -1, on success : return 0.
 */
int bt_context_destroy(struct bt_context *ctx);

/*
 * bt_context_get and bt_context_put : increments and decrement the refcount of
 * the context
 *
 * These functions ensures that the context won't be destroyed when it is in
 * use. The same number of get and put has to be done to be able to destroy a
 * context.
 *
 * Return 0 on success, -1 if the context pointer is invalid.  When the context
 * refcount is decremented to 0 by a bt_context_put, it calls
 * bt_context_destroy to free the context. In this case the return code of
 * bt_context_destroy is returned.
 */
int bt_context_get(struct bt_context *ctx);
int bt_context_put(struct bt_context *ctx);

#endif /* _BABELTRACE_CONTEXT_H */
