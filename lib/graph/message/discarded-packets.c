/*
 * Copyright 2017-2018 Philippe Proulx <pproulx@efficios.com>
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

#include <babeltrace/graph/message.h>
#include <babeltrace/graph/message-discarded-packets.h>
#include <babeltrace/graph/message-discarded-elements-internal.h>
#include <stdint.h>

struct bt_clock_value *
bt_message_discarded_packets_borrow_begin_clock_value(
		struct bt_message *message)
{
	return bt_message_discarded_elements_borrow_begin_clock_value(
		BT_MESSAGE_TYPE_DISCARDED_PACKETS, message);
}

struct bt_clock_value *
bt_message_discarded_packets_borrow_end_clock_value(
		struct bt_message *message)
{
	return bt_message_discarded_elements_borrow_end_clock_value(
		BT_MESSAGE_TYPE_DISCARDED_PACKETS, message);
}

int64_t bt_message_discarded_packets_get_count(
		struct bt_message *message)
{
	return bt_message_discarded_elements_get_count(
		BT_MESSAGE_TYPE_DISCARDED_PACKETS, message);
}

struct bt_stream *bt_message_discarded_packets_borrow_stream(
		struct bt_message *message)
{
	return bt_message_discarded_elements_borrow_stream(
		BT_MESSAGE_TYPE_DISCARDED_PACKETS, message);
}
