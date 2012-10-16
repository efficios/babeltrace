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

	if (!ctx)
		return NULL;

	th = g_new0(struct bt_trace_handle, 1);
	th->id = ctx->last_trace_handle_id++;
	return th;
}

void bt_trace_handle_destroy(struct bt_trace_handle *th)
{
	th->format->close_trace(th->td);
	g_free(th);
}

int bt_trace_handle_get_id(struct bt_trace_handle *th)
{
	if (!th)
		return -1;

	return th->id;
}

const char *bt_trace_handle_get_path(struct bt_context *ctx, int handle_id)
{
	struct bt_trace_handle *handle;

	if (!ctx)
		return NULL;

	handle = g_hash_table_lookup(ctx->trace_handles,
			(gpointer) (unsigned long) handle_id);
	if (!handle)
		return NULL;
	return handle->path;
}

uint64_t bt_trace_handle_get_timestamp_begin(struct bt_context *ctx,
		int handle_id, enum bt_clock_type type)
{
	struct bt_trace_handle *handle;
	uint64_t ret;

	if (!ctx)
		return -1ULL;

	handle = g_hash_table_lookup(ctx->trace_handles,
			(gpointer) (unsigned long) handle_id);
	if (!handle) {
		ret = -1ULL;
		goto end;
	}
	if (type == BT_CLOCK_REAL) {
		ret = handle->real_timestamp_begin;
	} else if (type == BT_CLOCK_CYCLES) {
		ret = handle->cycles_timestamp_begin;
	} else {
		ret = -1ULL;
	}

end:
	return ret;
}

uint64_t bt_trace_handle_get_timestamp_end(struct bt_context *ctx,
		int handle_id, enum bt_clock_type type)
{
	struct bt_trace_handle *handle;
	uint64_t ret;

	if (!ctx)
		return -1ULL;

	handle = g_hash_table_lookup(ctx->trace_handles,
			(gpointer) (unsigned long) handle_id);
	if (!handle) {
		ret = -1ULL;
		goto end;
	}
	if (type == BT_CLOCK_REAL) {
		ret = handle->real_timestamp_end;
	} else if (type == BT_CLOCK_CYCLES) {
		ret = handle->cycles_timestamp_end;
	} else {
		ret = -1ULL;
	}

end:
	return ret;
}
