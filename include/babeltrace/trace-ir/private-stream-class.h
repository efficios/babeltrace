#ifndef BABELTRACE_TRACE_IR_PRIVATE_STREAM_CLASS_H
#define BABELTRACE_TRACE_IR_PRIVATE_STREAM_CLASS_H

/*
 * Copyright 2014 Jérémie Galarneau <jeremie.galarneau@efficios.com>
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

/* For bt_bool */
#include <babeltrace/types.h>

#ifdef __cplusplus
extern "C" {
#endif

struct bt_stream_class;
struct bt_private_stream_class;
struct bt_private_trace;
struct bt_private_event_class;
struct bt_private_clock_class;

extern struct bt_stream_class *bt_stream_class_borrow_from_private(
		struct bt_private_stream_class *priv_stream_class);

extern struct bt_private_stream_class *bt_private_stream_class_create(
		struct bt_private_trace *trace);

extern struct bt_private_stream_class *bt_private_stream_class_create_with_id(
		struct bt_private_trace *trace, uint64_t id);

extern struct bt_private_trace *bt_private_stream_class_borrow_private_trace(
		struct bt_private_stream_class *stream_class);

extern int bt_private_stream_class_set_name(
		struct bt_private_stream_class *stream_class,
		const char *name);

extern int bt_private_stream_class_set_assigns_automatic_event_class_id(
		struct bt_private_stream_class *stream_class, bt_bool value);

extern int bt_private_stream_class_set_assigns_automatic_stream_id(
		struct bt_private_stream_class *stream_class, bt_bool value);

extern struct bt_private_field_class *
bt_private_stream_class_borrow_packet_context_private_field_class(
		struct bt_private_stream_class *stream_class);

extern int bt_private_stream_class_set_packet_context_private_field_class(
		struct bt_private_stream_class *stream_class,
		struct bt_private_field_class *field_class);

extern struct bt_private_field_class *
bt_private_stream_class_borrow_event_header_private_field_class(
		struct bt_private_stream_class *stream_class);

extern int bt_private_stream_class_set_event_header_private_field_class(
		struct bt_private_stream_class *stream_class,
		struct bt_private_field_class *field_class);

extern struct bt_private_field_class *
bt_private_stream_class_borrow_event_common_context_private_field_class(
		struct bt_private_stream_class *stream_class);

extern int
bt_private_stream_class_set_event_common_context_private_field_class(
		struct bt_private_stream_class *stream_class,
		struct bt_private_field_class *field_class);

extern struct bt_private_event_class *
bt_private_stream_class_borrow_private_event_class_by_index(
		struct bt_private_stream_class *stream_class, uint64_t index);

extern struct bt_private_event_class *
bt_private_stream_class_borrow_private_event_class_by_id(
		struct bt_private_stream_class *stream_class, uint64_t id);

extern int bt_private_stream_class_set_default_clock_class(
		struct bt_private_stream_class *stream_class,
		struct bt_clock_class *clock_class);

extern bt_bool bt_private_stream_class_default_clock_is_always_known(
		struct bt_private_stream_class *stream_class);

extern int
bt_private_stream_class_set_packets_have_discarded_event_counter_snapshot(
		struct bt_private_stream_class *stream_class, bt_bool value);

extern int
bt_private_stream_class_set_packets_have_packet_counter_snapshot(
		struct bt_private_stream_class *stream_class, bt_bool value);

extern int
bt_private_stream_class_set_packets_have_default_beginning_clock_value(
		struct bt_private_stream_class *stream_class, bt_bool value);

extern int
bt_private_stream_class_set_packets_have_default_end_clock_value(
		struct bt_private_stream_class *stream_class, bt_bool value);

#ifdef __cplusplus
}
#endif

#endif /* BABELTRACE_TRACE_IR_PRIVATE_STREAM_CLASS_H */
