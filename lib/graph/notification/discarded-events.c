/*
 * Copyright 2017 Philippe Proulx <pproulx@efficios.com>
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

#include <babeltrace/graph/notification.h>
#include <babeltrace/graph/notification-discarded-events.h>
#include <babeltrace/graph/notification-discarded-elements-internal.h>
#include <stdint.h>

struct bt_clock_value *
bt_notification_discarded_events_borrow_begin_clock_value(
		struct bt_notification *notification)
{
	return bt_notification_discarded_elements_borrow_begin_clock_value(
		BT_NOTIFICATION_TYPE_DISCARDED_EVENTS, notification);
}

struct bt_clock_value *
bt_notification_discarded_events_borrow_end_clock_value(
		struct bt_notification *notification)
{
	return bt_notification_discarded_elements_borrow_end_clock_value(
		BT_NOTIFICATION_TYPE_DISCARDED_EVENTS, notification);
}

int64_t bt_notification_discarded_events_get_count(
		struct bt_notification *notification)
{
	return bt_notification_discarded_elements_get_count(
		BT_NOTIFICATION_TYPE_DISCARDED_EVENTS, notification);
}

struct bt_stream *bt_notification_discarded_events_borrow_stream(
		struct bt_notification *notification)
{
	return bt_notification_discarded_elements_borrow_stream(
		BT_NOTIFICATION_TYPE_DISCARDED_EVENTS, notification);
}
