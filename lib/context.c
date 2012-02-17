/*
 * context.c
 *
 * Babeltrace Library
 *
 * Copyright 2011-2012 EfficiOS Inc. and Linux Foundation
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

#include <babeltrace/babeltrace.h>
#include <babeltrace/context.h>
#include <babeltrace/context-internal.h>
#include <babeltrace/trace-handle.h>
#include <babeltrace/trace-handle-internal.h>
#include <babeltrace/trace-collection.h>
#include <babeltrace/format.h>
#include <stdlib.h>

#include <fts.h>
#include <fcntl.h> /* For O_RDONLY */

/* TODO ybrosseau: should be hidden in the CTF format */
#include <babeltrace/ctf/types.h> /* for ctf_packet_seek */

#include <glib.h>

struct bt_context *bt_context_create(void)
{
	struct bt_context *ctx;

	ctx = g_new0(struct bt_context, 1);
	ctx->refcount = 1;
	/* Negative handle id are errors. */
	ctx->last_trace_handle_id = 0;

	/* Instanciate the trace handle container */
	ctx->trace_handles = g_hash_table_new_full(g_direct_hash,
				g_direct_equal, NULL,
				(GDestroyNotify) bt_trace_handle_destroy);

	ctx->tc = g_new0(struct trace_collection, 1);
	init_trace_collection(ctx->tc);

	return ctx;
}

int bt_context_add_trace(struct bt_context *ctx, const char *path,
		const char *format_name)
{
	struct trace_descriptor *td;
	struct format *fmt;
	struct bt_trace_handle *handle;
	int ret;

	fmt = bt_lookup_format(g_quark_from_string(format_name));
	if (!fmt) {
		fprintf(stderr, "[error] [Context] Format \"%s\" unknown.\n\n",
			format_name);
		ret = -1;
		goto end;
	}
	td = fmt->open_trace(path, O_RDONLY,
			     ctf_packet_seek, NULL);
	if (!td) {
		fprintf(stderr, "[error] [Context] Cannot open_trace of the format %s .\n\n",
				path);
		ret = -1;
		goto end;
	}

	/* Create an handle for the trace */
	handle = bt_trace_handle_create(ctx);
	if (handle < 0) {
		fprintf(stderr, "[error] [Context] Creating trace handle %s .\n\n",
				path);
		ret = -1;
		goto end;
	}
	handle->format = fmt;
	handle->td = td;
	strncpy(handle->path, path, PATH_MAX);
	handle->path[PATH_MAX - 1] = '\0';

	/* Add new handle to container */
	g_hash_table_insert(ctx->trace_handles,
		(gpointer) (unsigned long) handle->id,
		handle);
	ret = trace_collection_add(ctx->tc, td);
	if (ret == 0)
		return handle->id;
end:
	return ret;
}

void bt_context_remove_trace(struct bt_context *ctx, int handle_id)
{
	struct bt_trace_handle *handle;

	handle = g_hash_table_lookup(ctx->trace_handles,
		(gpointer) (unsigned long) handle_id);
	assert(handle != NULL);

	/* Remove from containers */
	trace_collection_remove(ctx->tc, handle->td);
	g_hash_table_remove(ctx->trace_handles,
		(gpointer) (unsigned long) handle_id);

	/* Close the trace */
	handle->format->close_trace(handle->td);

	/* Destory the handle */
	bt_trace_handle_destroy(handle);
}

static
void bt_context_destroy(struct bt_context *ctx)
{
	finalize_trace_collection(ctx->tc);

	/* Remote all traces. The g_hash_table_destroy will call
	 * bt_trace_handle_destroy on each elements.
	 */
	g_hash_table_destroy(ctx->trace_handles);

	/* ctx->tc should always be valid */
	assert(ctx->tc != NULL);
	g_free(ctx->tc);
	g_free(ctx);
}

void bt_context_get(struct bt_context *ctx)
{
	ctx->refcount++;
}

void bt_context_put(struct bt_context *ctx)
{
	ctx->refcount--;
	if (ctx->refcount == 0)
		bt_context_destroy(ctx);
}
