#ifndef BABELTRACE_GRAPH_MESSAGE_DISCARDED_EVENTS_H
#define BABELTRACE_GRAPH_MESSAGE_DISCARDED_EVENTS_H

/*
 * Copyright 2019 Philippe Proulx <pproulx@efficios.com>
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

#include <stdint.h>

/* For bt_message, bt_self_message_iterator, bt_stream */
#include <babeltrace/types.h>

#ifdef __cplusplus
extern "C" {
#endif

extern bt_message *bt_message_discarded_events_create(
		bt_self_message_iterator *message_iterator,
		bt_stream *stream);

extern bt_message *bt_message_discarded_events_create_with_default_clock_snapshots(
		bt_self_message_iterator *message_iterator,
		bt_stream *stream, uint64_t beginning_raw_value,
		uint64_t end_raw_value);

extern bt_stream *bt_message_discarded_events_borrow_stream(
		bt_message *message);

extern void bt_message_discarded_events_set_count(bt_message *message,
		uint64_t count);

#ifdef __cplusplus
}
#endif

#endif /* BABELTRACE_GRAPH_MESSAGE_DISCARDED_EVENTS_H */
