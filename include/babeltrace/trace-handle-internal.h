#ifndef _BABELTRACE_TRACE_HANDLE_INTERNAL_H
#define _BABELTRACE_TRACE_HANDLE_INTERNAL_H

/*
 * BabelTrace
 *
 * Internal trace handle header
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
#include <babeltrace/context.h>
#include <babeltrace/format.h>

/*
 * trace_handle : unique identifier of a trace
 *
 * The trace_handle allows the user to manipulate a trace file directly.
 * It is a unique identifier representing a trace file.
 */
struct bt_trace_handle {
	int id;
	struct trace_descriptor *td;
	struct format *format;
	char path[PATH_MAX];
	uint64_t real_timestamp_begin;
	uint64_t real_timestamp_end;
	uint64_t cycles_timestamp_begin;
	uint64_t cycles_timestamp_end;
};

/*
 * bt_trace_handle_create : allocates a trace_handle
 *
 * Returns a newly allocated trace_handle or NULL on error
 */
struct bt_trace_handle *bt_trace_handle_create(struct bt_context *ctx);

/*
 * bt_trace_handle_destroy : free a trace_handle
 */
void bt_trace_handle_destroy(struct bt_trace_handle *bt);

#endif /* _BABELTRACE_TRACE_HANDLE_INTERNAL_H */
