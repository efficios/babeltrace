#ifndef BABELTRACE_GRAPH_NOTIFICATION_STREAM_INTERNAL_H
#define BABELTRACE_GRAPH_NOTIFICATION_STREAM_INTERNAL_H

/*
 * Copyright 2016 Jérémie Galarneau <jeremie.galarneau@efficios.com>
 *
 * Author: Jérémie Galarneau <jeremie.galarneau@efficios.com>
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

#include <babeltrace/compiler-internal.h>
#include <babeltrace/trace-ir/packet.h>
#include <babeltrace/graph/notification-internal.h>
#include <babeltrace/trace-ir/clock-value-internal.h>
#include <babeltrace/assert-internal.h>

struct bt_notification_stream_begin {
	struct bt_notification parent;
	struct bt_stream *stream;
	struct bt_clock_value *default_cv;
};

struct bt_notification_stream_end {
	struct bt_notification parent;
	struct bt_stream *stream;
	struct bt_clock_value *default_cv;
};

#endif /* BABELTRACE_GRAPH_NOTIFICATION_STREAM_INTERNAL_H */
