#ifndef BABELTRACE_TRACE_IR_STREAM_CLASS_H
#define BABELTRACE_TRACE_IR_STREAM_CLASS_H

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

#include <stdint.h>

/*
 * For bt_bool, bt_trace_class, bt_stream_class, bt_event_class,
 * bt_clock_class
 */
#include <babeltrace/types.h>

/* For bt_stream_class_status */
#include <babeltrace/trace-ir/stream-class-const.h>

#ifdef __cplusplus
extern "C" {
#endif

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

extern bt_stream_class_status
bt_stream_class_set_event_header_field_class(
		bt_stream_class *stream_class,
		bt_field_class *field_class);

extern bt_stream_class_status
bt_stream_class_set_event_common_context_field_class(
		bt_stream_class *stream_class,
		bt_field_class *field_class);

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

extern void
bt_stream_class_set_packets_have_discarded_event_counter_snapshot(
		bt_stream_class *stream_class, bt_bool value);

extern void
bt_stream_class_set_packets_have_packet_counter_snapshot(
		bt_stream_class *stream_class, bt_bool value);

extern void
bt_stream_class_set_packets_have_default_beginning_clock_snapshot(
		bt_stream_class *stream_class, bt_bool value);

extern void
bt_stream_class_set_packets_have_default_end_clock_snapshot(
		bt_stream_class *stream_class, bt_bool value);

#ifdef __cplusplus
}
#endif

#endif /* BABELTRACE_TRACE_IR_STREAM_CLASS_H */
