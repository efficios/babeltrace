#ifndef LTTNG_LIVE_DATA_STREAM_H
#define LTTNG_LIVE_DATA_STREAM_H

/*
 * Copyright 2016 - Philippe Proulx <pproulx@efficios.com>
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

#include <stdio.h>
#include <glib.h>
#include <babeltrace/babeltrace-internal.h>
#include <babeltrace/babeltrace.h>

#include "lttng-live-internal.h"
#include "../common/notif-iter/notif-iter.h"

enum bt_ctf_lttng_live_iterator_status lttng_live_lazy_notif_init(
		struct lttng_live_session *session);

struct lttng_live_stream_iterator *lttng_live_stream_iterator_create(
		struct lttng_live_session *session,
		uint64_t ctf_trace_id,
		uint64_t stream_id);

void lttng_live_stream_iterator_destroy(struct lttng_live_stream_iterator *stream);

#endif /* LTTNG_LIVE_DATA_STREAM_H */
