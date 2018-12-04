#ifndef BABELTRACE_TRACE_IR_STREAM_H
#define BABELTRACE_TRACE_IR_STREAM_H

/*
 * Copyright 2013, 2014 Jérémie Galarneau <jeremie.galarneau@efficios.com>
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
 *
 * The Common Trace Format (CTF) Specification is available at
 * http://www.efficios.com/ctf
 */

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct bt_stream;
struct bt_stream_class;

extern struct bt_stream *bt_stream_create(struct bt_stream_class *stream_class);

extern struct bt_stream *bt_stream_create_with_id(
		struct bt_stream_class *stream_class, uint64_t id);

extern struct bt_stream_class *bt_stream_borrow_class(struct bt_stream *stream);

extern int bt_stream_set_name(struct bt_stream *stream, const char *name);

#ifdef __cplusplus
}
#endif

#endif /* BABELTRACE_TRACE_IR_STREAM_H */
