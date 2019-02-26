#ifndef BABELTRACE_TRACE_IR_TRACE_H
#define BABELTRACE_TRACE_IR_TRACE_H

/*
 * Copyright 2017-2018 Philippe Proulx <pproulx@efficios.com>
 * Copyright 2014 Jérémie Galarneau <jeremie.galarneau@efficios.com>
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

/* For bt_bool, bt_trace, bt_trace_class, bt_stream */
#include <babeltrace/types.h>

/* For bt_trace_status */
#include <babeltrace/trace-ir/trace-const.h>

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

extern bt_trace_class *bt_trace_borrow_class(bt_trace *trace);

extern bt_trace *bt_trace_create(bt_trace_class *trace_class);

extern bt_trace_status bt_trace_set_name(bt_trace *trace,
		const char *name);

extern bt_stream *bt_trace_borrow_stream_by_index(bt_trace *trace,
		uint64_t index);

extern bt_stream *bt_trace_borrow_stream_by_id(bt_trace *trace,
		uint64_t id);

#ifdef __cplusplus
}
#endif

#endif /* BABELTRACE_TRACE_IR_TRACE_H */
