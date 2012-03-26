/*
 * trace_handle.c
 *
 * Babeltrace Library
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

#include <stdint.h>
#include <stdlib.h>
#include <babeltrace/babeltrace.h>
#include <babeltrace/context.h>
#include <babeltrace/context-internal.h>
#include <babeltrace/trace-handle.h>
#include <babeltrace/trace-handle-internal.h>

struct bt_trace_handle *bt_trace_handle_create(struct bt_context *ctx)
{
	struct bt_trace_handle *th;

	th = g_new0(struct bt_trace_handle, 1);
	th->id = ctx->last_trace_handle_id++;
	return th;
}

void bt_trace_handle_destroy(struct bt_trace_handle *th)
{
	g_free(th);
}

int bt_trace_handle_get_id(struct bt_trace_handle *th)
{
	return th->id;
}

const char *bt_trace_handle_get_path(struct bt_context *ctx, int handle_id)
{
	struct bt_trace_handle *handle;

	handle = g_hash_table_lookup(ctx->trace_handles,
			(gpointer) (unsigned long) handle_id);
	return handle->path;
}

uint64_t bt_trace_handle_get_timestamp_begin(struct bt_context *ctx, int handle_id)
{
	struct bt_trace_handle *handle;

	handle = g_hash_table_lookup(ctx->trace_handles,
			(gpointer) (unsigned long) handle_id);
	return handle->timestamp_begin;
}

uint64_t bt_trace_handle_get_timestamp_end(struct bt_context *ctx, int handle_id)
{
	struct bt_trace_handle *handle;

	handle = g_hash_table_lookup(ctx->trace_handles,
			(gpointer) (unsigned long) handle_id);
	return handle->timestamp_end;
}
