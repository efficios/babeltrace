#ifndef BABELTRACE_COMPONENT_NOTIFICATION_DISCARDED_ELEMENTS_INTERNAL_H
#define BABELTRACE_COMPONENT_NOTIFICATION_DISCARDED_ELEMENTS_INTERNAL_H

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

#include <glib.h>
#include <stdint.h>
#include <babeltrace/assert-internal.h>
#include <babeltrace/compiler-internal.h>
#include <babeltrace/graph/notification.h>
#include <babeltrace/graph/notification-internal.h>

struct bt_clock_class_priority_map;
struct bt_stream;

struct bt_notification_discarded_elements {
	struct bt_notification parent;
	struct bt_stream *stream;
	struct bt_clock_value *begin_clock_value;
	struct bt_clock_value *end_clock_value;
	int64_t count;
};

BT_HIDDEN
struct bt_notification *bt_notification_discarded_elements_create(
		enum bt_notification_type type,
		struct bt_stream *stream,
		struct bt_clock_value *begin_clock_value,
		struct bt_clock_value *end_clock_value,
		uint64_t count);

BT_HIDDEN
struct bt_stream *bt_notification_discarded_elements_get_stream(
		enum bt_notification_type type,
		struct bt_notification *notification);

BT_HIDDEN
struct bt_clock_value *
bt_notification_discarded_elements_get_begin_clock_value(
		enum bt_notification_type type,
		struct bt_notification *notification);

BT_HIDDEN
struct bt_clock_value *
bt_notification_discarded_elements_get_end_clock_value(
		enum bt_notification_type type,
		struct bt_notification *notification);

BT_HIDDEN
int64_t bt_notification_discarded_elements_get_count(
		enum bt_notification_type type,
		struct bt_notification *notification);

static inline
struct bt_stream *bt_notification_discarded_elements_borrow_stream(
		struct bt_notification *notification)
{
	struct bt_notification_discarded_elements *discarded_elems_notif;

	BT_ASSERT(notification);
	discarded_elems_notif = container_of(notification,
			struct bt_notification_discarded_elements, parent);
	return discarded_elems_notif->stream;
}

#endif /* BABELTRACE_COMPONENT_NOTIFICATION_DISCARDED_ELEMENTS_INTERNAL_H */
