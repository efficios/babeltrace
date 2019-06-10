/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2016-2017 Philippe Proulx <pproulx@efficios.com>
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
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

/* From stream-const.h */

typedef enum bt_stream_status {
	BT_STREAM_STATUS_OK = 0,
	BT_STREAM_STATUS_NOMEM = -12,
} bt_stream_status;

extern const bt_stream_class *bt_stream_borrow_class_const(
		const bt_stream *stream);

extern const bt_trace *bt_stream_borrow_trace_const(
		const bt_stream *stream);

extern const char *bt_stream_get_name(const bt_stream *stream);

extern uint64_t bt_stream_get_id(const bt_stream *stream);

extern void bt_stream_get_ref(const bt_stream *stream);

extern void bt_stream_put_ref(const bt_stream *stream);

/* From stream.h */

extern bt_stream *bt_stream_create(bt_stream_class *stream_class,
		bt_trace *trace);

extern bt_stream *bt_stream_create_with_id(
		bt_stream_class *stream_class,
		bt_trace *trace, uint64_t id);

extern bt_trace *bt_stream_borrow_trace(bt_stream *stream);

extern bt_stream_class *bt_stream_borrow_class(bt_stream *stream);

extern bt_stream_status bt_stream_set_name(bt_stream *stream,
		const char *name);
