#ifndef BABELTRACE_TRACE_IR_EVENT_CONST_H
#define BABELTRACE_TRACE_IR_EVENT_CONST_H

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

/* For enum bt_clock_value_status */
#include <babeltrace/trace-ir/clock-value-const.h>

#ifdef __cplusplus
extern "C" {
#endif

struct bt_event;
struct bt_clock_value;
struct bt_event_class;
struct bt_field;
struct bt_packet;

extern const struct bt_event_class *bt_event_borrow_class_const(
		const struct bt_event *event);

extern const struct bt_packet *bt_event_borrow_packet_const(
		const struct bt_event *event);

extern const struct bt_stream *bt_event_borrow_stream_const(
		const struct bt_event *event);

extern const struct bt_field *bt_event_borrow_header_field_const(
		const struct bt_event *event);

extern const struct bt_field *bt_event_borrow_common_context_field_const(
		const struct bt_event *event);

extern const struct bt_field *bt_event_borrow_specific_context_field_const(
		const struct bt_event *event);

extern const struct bt_field *bt_event_borrow_payload_field_const(
		const struct bt_event *event);

extern enum bt_clock_value_status bt_event_borrow_default_clock_value_const(
		const struct bt_event *event,
		const struct bt_clock_value **clock_value);

#ifdef __cplusplus
}
#endif

#endif /* BABELTRACE_TRACE_IR_EVENT_CONST_H */
