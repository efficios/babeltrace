#ifndef BABELTRACE_GRAPH_NOTIFICATION_ITERATOR_H
#define BABELTRACE_GRAPH_NOTIFICATION_ITERATOR_H

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
struct bt_notification_iterator;

/**
 * Status code. Errors are always negative.
 */
enum bt_notification_iterator_status {
	/** Canceled. */
	BT_NOTIFICATION_ITERATOR_STATUS_CANCELED = 125,
	/** No notifications available for now. Try again later. */
	BT_NOTIFICATION_ITERATOR_STATUS_AGAIN = 11,
	/** No more notifications to be delivered. */
	BT_NOTIFICATION_ITERATOR_STATUS_END = 1,
	/** No error, okay. */
	BT_NOTIFICATION_ITERATOR_STATUS_OK = 0,
	/** Invalid arguments. */
	BT_NOTIFICATION_ITERATOR_STATUS_INVALID = -22,
	/** General error. */
	BT_NOTIFICATION_ITERATOR_STATUS_ERROR = -1,
	/** Out of memory. */
	BT_NOTIFICATION_ITERATOR_STATUS_NOMEM = -12,
	/** Unsupported iterator feature. */
	BT_NOTIFICATION_ITERATOR_STATUS_UNSUPPORTED = -2,
};

#ifdef __cplusplus
}
#endif

#endif /* BABELTRACE_GRAPH_NOTIFICATION_ITERATOR_H */
