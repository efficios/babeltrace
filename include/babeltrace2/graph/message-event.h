#ifndef BABELTRACE2_GRAPH_MESSAGE_EVENT_H
#define BABELTRACE2_GRAPH_MESSAGE_EVENT_H

/*
 * Copyright (c) 2010-2019 EfficiOS Inc. and Linux Foundation
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

#ifndef __BT_IN_BABELTRACE_H
# error "Please include <babeltrace2/babeltrace.h> instead."
#endif

#include <stdint.h>

#include <babeltrace2/types.h>

#ifdef __cplusplus
extern "C" {
#endif

extern
bt_message *bt_message_event_create(
		bt_self_message_iterator *message_iterator,
		const bt_event_class *event_class,
		const bt_stream *stream);

extern
bt_message *bt_message_event_create_with_packet(
		bt_self_message_iterator *message_iterator,
		const bt_event_class *event_class,
		const bt_packet *packet);

extern
bt_message *bt_message_event_create_with_default_clock_snapshot(
		bt_self_message_iterator *message_iterator,
		const bt_event_class *event_class,
		const bt_stream *stream, uint64_t raw_clock_value);

extern
bt_message *bt_message_event_create_with_packet_and_default_clock_snapshot(
		bt_self_message_iterator *message_iterator,
		const bt_event_class *event_class,
		const bt_packet *packet, uint64_t raw_clock_value);

extern bt_event *bt_message_event_borrow_event(
		bt_message *message);

#ifdef __cplusplus
}
#endif

#endif /* BABELTRACE2_GRAPH_MESSAGE_EVENT_H */
