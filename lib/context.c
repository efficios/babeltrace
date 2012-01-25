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
#include <stdlib.h>

struct bt_context *bt_context_create(struct trace_collection *tc)
{
	struct bt_context *ctx;

	ctx = calloc(1, sizeof(struct bt_context));
	if (ctx == NULL) {
		perror("allocating context");
		goto error;
	}

	ctx->tc = tc;
	ctx->refcount = 1;

	return ctx;

error:
	return NULL;
}

int bt_context_destroy(struct bt_context *ctx)
{
	if (ctx) {
		if (ctx->refcount >= 1)
			goto ctx_used;

		free(ctx);
	}
	return 0;

ctx_used:
	return -1;
}

int bt_context_get(struct bt_context *ctx)
{
	if (!ctx)
		return -1;
	ctx->refcount++;
	return 0;
}

int bt_context_put(struct bt_context *ctx)
{
	if (!ctx)
		return -1;

	ctx->refcount--;
	if (ctx->refcount == 0)
		return bt_context_destroy(ctx);
	return 0;
}
