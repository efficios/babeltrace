#ifndef BABELTRACE_TRACE_IR_PRIVATE_EVENT_H
#define BABELTRACE_TRACE_IR_PRIVATE_EVENT_H

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

#ifdef __cplusplus
extern "C" {
#endif

struct bt_private_event;
struct bt_private_event_header_field;
struct bt_private_event_class;
struct bt_private_field;
struct bt_private_packet;

static inline
struct bt_event *bt_private_event_borrow_event(
		struct bt_private_event *priv_event)
{
	return (void *) priv_event;
}

extern struct bt_private_event_class *bt_private_event_borrow_class(
		struct bt_private_event *event);

extern struct bt_private_packet *bt_private_event_borrow_packet(
		struct bt_private_event *event);

extern struct bt_private_stream *bt_private_event_borrow_stream(
		struct bt_private_event *event);

extern struct bt_private_field *bt_private_event_borrow_header_field(
		struct bt_private_event *event);

extern int bt_private_event_move_header_field(
		struct bt_private_event *event,
		struct bt_private_event_header_field *header);

extern struct bt_private_field *
bt_private_event_borrow_common_context_field(
		struct bt_private_event *event);

extern struct bt_private_field *
bt_private_event_borrow_specific_context_field(
		struct bt_private_event *event);

extern struct bt_private_field *bt_private_event_borrow_payload_field(
		struct bt_private_event *event);

extern int bt_private_event_set_default_clock_value(
		struct bt_private_event *event, uint64_t value_cycles);

#ifdef __cplusplus
}
#endif

#endif /* BABELTRACE_TRACE_IR_PRIVATE_EVENT_H */
