/*
 * Babeltrace Plug-in Notification
 *
 * Copyright 2016 Jérémie Galarneau <jeremie.galarneau@efficios.com>
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
 */

#include <babeltrace/graph/notification-internal.h>

BT_HIDDEN
void bt_notification_init(struct bt_notification *notification,
		enum bt_notification_type type,
		bt_object_release_func release)
{
	assert(type > BT_NOTIFICATION_TYPE_ALL &&
			type < BT_NOTIFICATION_TYPE_NR);
	notification->type = type;
	bt_object_init(&notification->base, release);
}

enum bt_notification_type bt_notification_get_type(
		struct bt_notification *notification)
{
	return notification ? notification->type : BT_NOTIFICATION_TYPE_UNKNOWN;
}

struct bt_notification *bt_notification_from_private(
		struct bt_private_notification *private_notification)
{
	return bt_get(private_notification);
}

struct bt_notification *bt_notification_borrow_from_private(
		struct bt_private_notification *private_notification)
{
	return (void *) private_notification;
}
