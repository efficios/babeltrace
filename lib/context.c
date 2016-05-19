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
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <babeltrace/babeltrace.h>
#include <babeltrace/context.h>
#include <babeltrace/context-internal.h>
#include <babeltrace/trace-handle.h>
#include <babeltrace/trace-handle-internal.h>
#include <babeltrace/trace-collection.h>
#include <babeltrace/format.h>
#include <babeltrace/format-internal.h>
#include <babeltrace/babeltrace-internal.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <errno.h>

#include <fcntl.h> /* For O_RDONLY */

#include <glib.h>

static
void remove_trace_handle(struct bt_trace_handle *handle);

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
				(GDestroyNotify) remove_trace_handle);

	ctx->current_iterator = NULL;
	ctx->tc = g_new0(struct trace_collection, 1);
	bt_init_trace_collection(ctx->tc);

	return ctx;
}

int bt_context_add_trace(struct bt_context *ctx, const char *path,
		const char *format_name,
		void (*packet_seek)(struct bt_stream_pos *pos, size_t index,
			int whence),
		struct bt_mmap_stream_list *stream_list,
		FILE *metadata)
{
	struct bt_trace_descriptor *td;
	struct bt_format *fmt;
	struct bt_trace_handle *handle;
	int ret, closeret;

	if (!ctx || !format_name || (!path && !stream_list))
		return -EINVAL;

	fmt = bt_lookup_format(g_quark_from_string(format_name));
	if (!fmt) {
		fprintf(stderr, "[error] [Context] Format \"%s\" unknown.\n\n",
			format_name);
		ret = -1;
		goto end;
	}
	if (path) {
		td = fmt->open_trace(path, O_RDONLY, packet_seek, NULL);
		if (!td) {
			fprintf(stderr, "[warning] [Context] Cannot open_trace of format %s at path %s.\n",
					format_name, path);
			ret = -1;
			goto end;
		}
	} else {
		td = fmt->open_mmap_trace(stream_list, packet_seek, metadata);
		if (!td) {
			fprintf(stderr, "[error] [Context] Cannot open_mmap_trace of format %s.\n\n",
					format_name);
			ret = -1;
			goto end;
		}
	}

	/* Create an handle for the trace */
	handle = bt_trace_handle_create(ctx);
	if (!handle) {
		fprintf(stderr, "[error] [Context] Creating trace handle %s .\n\n",
				path);
		ret = -1;
		goto error_close;
	}
	handle->format = fmt;
	handle->td = td;
	if (path) {
		strncpy(handle->path, path, PATH_MAX);
		handle->path[PATH_MAX - 1] = '\0';
	}

	ret = bt_trace_collection_add(ctx->tc, td);
	if (ret != 0)
		goto error_destroy_handle;

	if (fmt->set_handle)
		fmt->set_handle(td, handle);
	if (fmt->set_context)
		fmt->set_context(td, ctx);

	if (fmt->convert_index_timestamp) {
		ret = fmt->convert_index_timestamp(td);
		if (ret < 0)
			goto error_collection_del;
	}

	if (fmt->timestamp_begin)
		handle->real_timestamp_begin = fmt->timestamp_begin(td,
				handle, BT_CLOCK_REAL);
	if (fmt->timestamp_end)
		handle->real_timestamp_end = fmt->timestamp_end(td, handle,
				BT_CLOCK_REAL);
	if (fmt->timestamp_begin)
		handle->cycles_timestamp_begin = fmt->timestamp_begin(td,
				handle, BT_CLOCK_CYCLES);
	if (fmt->timestamp_end)
		handle->cycles_timestamp_end = fmt->timestamp_end(td, handle,
				BT_CLOCK_CYCLES);

	/* Add new handle to container */
	g_hash_table_insert(ctx->trace_handles,
		(gpointer) (unsigned long) handle->id,
		handle);

	return handle->id;

error_collection_del:
	/* Remove from containers */
	bt_trace_collection_remove(handle->td->ctx->tc, handle->td);
error_destroy_handle:
	bt_trace_handle_destroy(handle);
error_close:
	closeret = fmt->close_trace(td);
	if (closeret) {
		fprintf(stderr, "Error in close_trace callback\n");
	}
end:
	return ret;
}

int bt_context_remove_trace(struct bt_context *ctx, int handle_id)
{
	int ret = 0;

	if (!ctx) {
		ret = -EINVAL;
		goto end;
	}

	/*
	 * Remove the handle. remove_trace_handle will be called
	 * automatically.
	 */
	if (!g_hash_table_remove(ctx->trace_handles,
		(gpointer) (unsigned long) handle_id)) {
		ret = -ENOENT;
		goto end;
	}
end:
	return ret;
}

static
void bt_context_destroy(struct bt_context *ctx)
{
	assert(ctx);

	/*
	 * Remove all traces. The g_hash_table_destroy will call
	 * remove_trace_handle on each element.
	 */
	g_hash_table_destroy(ctx->trace_handles);

	bt_finalize_trace_collection(ctx->tc);

	/* ctx->tc should always be valid */
	assert(ctx->tc != NULL);
	g_free(ctx->tc);
	g_free(ctx);
}

void bt_context_get(struct bt_context *ctx)
{
	assert(ctx);
	ctx->refcount++;
}

void bt_context_put(struct bt_context *ctx)
{
	assert(ctx);
	ctx->refcount--;
	if (ctx->refcount == 0)
		bt_context_destroy(ctx);
}

static
void remove_trace_handle(struct bt_trace_handle *handle)
{
	int ret;

	if (!handle->td->ctx)
		return;
	/* Remove from containers */
	bt_trace_collection_remove(handle->td->ctx->tc, handle->td);
	/* Close the trace */
	ret = handle->format->close_trace(handle->td);
	if (ret) {
		fprintf(stderr, "Error in close_trace callback\n");
	}

	bt_trace_handle_destroy(handle);
}
