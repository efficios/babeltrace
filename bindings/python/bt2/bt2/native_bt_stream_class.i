/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2016 Philippe Proulx <pproulx@efficios.com>
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

/* From stream-class-const.h */

typedef enum bt_stream_class_status {
	BT_STREAM_CLASS_STATUS_OK = 0,
	BT_STREAM_CLASS_STATUS_NOMEM = -12,
} bt_stream_class_status;

extern const bt_trace_class *bt_stream_class_borrow_trace_class_const(
		const bt_stream_class *stream_class);

extern const char *bt_stream_class_get_name(
		const bt_stream_class *stream_class);

extern bt_bool bt_stream_class_assigns_automatic_event_class_id(
		const bt_stream_class *stream_class);

extern bt_bool bt_stream_class_assigns_automatic_stream_id(
		const bt_stream_class *stream_class);

extern uint64_t bt_stream_class_get_id(
		const bt_stream_class *stream_class);

extern const bt_field_class *
bt_stream_class_borrow_packet_context_field_class_const(
		const bt_stream_class *stream_class);

extern const bt_field_class *
bt_stream_class_borrow_event_common_context_field_class_const(
		const bt_stream_class *stream_class);

extern uint64_t bt_stream_class_get_event_class_count(
		const bt_stream_class *stream_class);

extern const bt_event_class *
bt_stream_class_borrow_event_class_by_index_const(
		const bt_stream_class *stream_class, uint64_t index);

extern const bt_event_class *
bt_stream_class_borrow_event_class_by_id_const(
		const bt_stream_class *stream_class, uint64_t id);

extern const bt_clock_class *
bt_stream_class_borrow_default_clock_class_const(
		const bt_stream_class *stream_class);

extern bt_bool bt_stream_class_default_clock_is_always_known(
		const bt_stream_class *stream_class);

extern void bt_stream_class_get_ref(const bt_stream_class *stream_class);

extern void bt_stream_class_put_ref(const bt_stream_class *stream_class);

/* From stream-class-const.h */

extern bt_stream_class *bt_stream_class_create(
		bt_trace_class *trace_class);

extern bt_stream_class *bt_stream_class_create_with_id(
		bt_trace_class *trace_class, uint64_t id);

extern bt_trace_class *bt_stream_class_borrow_trace_class(
		bt_stream_class *stream_class);

extern bt_stream_class_status bt_stream_class_set_name(
		bt_stream_class *stream_class, const char *name);

extern void bt_stream_class_set_assigns_automatic_event_class_id(
		bt_stream_class *stream_class, bt_bool value);

extern void bt_stream_class_set_assigns_automatic_stream_id(
		bt_stream_class *stream_class, bt_bool value);

extern bt_stream_class_status
bt_stream_class_set_packet_context_field_class(
		bt_stream_class *stream_class,
		bt_field_class *field_class);

extern bt_field_class *
bt_stream_class_borrow_packet_context_field_class(
		bt_stream_class *stream_class);

extern bt_stream_class_status
bt_stream_class_set_event_common_context_field_class(
		bt_stream_class *stream_class,
		bt_field_class *field_class);

extern bt_field_class *
bt_stream_class_borrow_event_common_context_field_class(
		bt_stream_class *stream_class);

extern bt_event_class *
bt_stream_class_borrow_event_class_by_index(
		bt_stream_class *stream_class, uint64_t index);

extern bt_event_class *
bt_stream_class_borrow_event_class_by_id(
		bt_stream_class *stream_class, uint64_t id);

extern bt_clock_class *bt_stream_class_borrow_default_clock_class(
		bt_stream_class *stream_class);

extern bt_stream_class_status bt_stream_class_set_default_clock_class(
		bt_stream_class *stream_class,
		bt_clock_class *clock_class);
