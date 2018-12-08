#ifndef BABELTRACE_TRACE_IR_EVENT_HEADER_FIELD_H
#define BABELTRACE_TRACE_IR_EVENT_HEADER_FIELD_H

/*
 * Copyright 2018 Philippe Proulx <pproulx@efficios.com>
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

/* For bt_stream_class, bt_event_header_field, bt_field */
#include <babeltrace/types.h>

#ifdef __cplusplus
extern "C" {
#endif

extern
bt_event_header_field *bt_event_header_field_create(
		bt_stream_class *stream_class);

extern
bt_field *bt_event_header_field_borrow_field(
		bt_event_header_field *field);

extern
void bt_event_header_field_release(bt_event_header_field *field);

#ifdef __cplusplus
}
#endif

#endif /* BABELTRACE_TRACE_IR_EVENT_HEADER_FIELD_H */
