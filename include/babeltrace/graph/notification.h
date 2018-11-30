#ifndef BABELTRACE_GRAPH_NOTIFICATION_H
#define BABELTRACE_GRAPH_NOTIFICATION_H

/*
 * Copyright 2015 Jérémie Galarneau <jeremie.galarneau@efficios.com>
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

#ifdef __cplusplus
extern "C" {
#endif

struct bt_notification;

typedef struct bt_notification **bt_notification_array;

/**
 * Notification types. Unhandled notification types should be ignored.
 */
enum bt_notification_type {
	BT_NOTIFICATION_TYPE_EVENT =			0,
	BT_NOTIFICATION_TYPE_INACTIVITY =		1,
	BT_NOTIFICATION_TYPE_STREAM_BEGIN =		2,
	BT_NOTIFICATION_TYPE_STREAM_END =		3,
	BT_NOTIFICATION_TYPE_PACKET_BEGIN =		4,
	BT_NOTIFICATION_TYPE_PACKET_END =		5,
};

/**
 * Get a notification's type.
 *
 * @param notification	Notification instance
 * @returns		One of #bt_notification_type
 */
extern enum bt_notification_type bt_notification_get_type(
		struct bt_notification *notification);

#ifdef __cplusplus
}
#endif

#endif /* BABELTRACE_GRAPH_NOTIFICATION_H */
