#ifndef _BABELTRACE_FORMAT_INTERNAL_H
#define _BABELTRACE_FORMAT_INTERNAL_H

/*
 * BabelTrace
 *
 * Trace Format Internal Header
 *
 * Copyright 2010-2013 EfficiOS Inc. and Linux Foundation
 *
 * Author: Mathieu Desnoyers <mathieu.desnoyers@efficios.com>
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

#include <limits.h>
#include <babeltrace/context-internal.h>
#include <babeltrace/babeltrace-internal.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Parent trace descriptor */
struct bt_trace_descriptor {
	char path[PATH_MAX];		/* trace path */
	struct bt_context *ctx;
	struct bt_trace_handle *handle;
	struct trace_collection *collection;	/* Container of this trace */
	GHashTable *clocks;
	struct ctf_clock *single_clock;		/* currently supports only one clock */
};

#ifdef __cplusplus
}
#endif

#endif /* _BABELTRACE_FORMAT_INTERNAL_H */
