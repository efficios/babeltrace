#ifndef BABELTRACE_TRACE_IR_STREAM_INTERNAL_H
#define BABELTRACE_TRACE_IR_STREAM_INTERNAL_H

/*
 * Copyright 2017-2018 Philippe Proulx <pproulx@efficios.com>
 * Copyright 2013, 2014 Jérémie Galarneau <jeremie.galarneau@efficios.com>
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

#include <babeltrace/trace-ir/stream.h>
#include <babeltrace/trace-ir/utils-internal.h>
#include <babeltrace/object-internal.h>
#include <babeltrace/object-pool-internal.h>
#include <babeltrace/babeltrace-internal.h>
#include <glib.h>

struct bt_stream_class;
struct bt_stream;

struct bt_stream {
	struct bt_object base;

	/* Owned by this */
	struct bt_stream_class *class;

	struct {
		GString *str;

		/* NULL or `str->str` above */
		const char *value;
	} name;

	uint64_t id;

	/* Pool of `struct bt_packet *` */
	struct bt_object_pool packet_pool;

	bool frozen;
};

BT_HIDDEN
void _bt_stream_freeze(const struct bt_stream *stream);

#ifdef BT_DEV_MODE
# define bt_stream_freeze		_bt_stream_freeze
#else
# define bt_stream_freeze(_stream)
#endif

static inline
struct bt_trace *bt_stream_borrow_trace_inline(const struct bt_stream *stream)
{
	BT_ASSERT(stream);
	return (void *) bt_object_borrow_parent(&stream->base);
}

#endif /* BABELTRACE_TRACE_IR_STREAM_INTERNAL_H */
