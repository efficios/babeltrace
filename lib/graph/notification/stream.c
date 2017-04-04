/*
 * Babeltrace Plug-in Stream-related Notifications
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

#include <babeltrace/compiler-internal.h>
#include <babeltrace/graph/notification-stream-internal.h>

static
void bt_notification_stream_end_destroy(struct bt_object *obj)
{
	struct bt_notification_stream_end *notification =
			(struct bt_notification_stream_end *) obj;

	BT_PUT(notification->stream);
	g_free(notification);
}

struct bt_notification *bt_notification_stream_end_create(
		struct bt_ctf_stream *stream)
{
	struct bt_notification_stream_end *notification;

	if (!stream) {
		goto error;
	}

	notification = g_new0(struct bt_notification_stream_end, 1);
	bt_notification_init(&notification->parent,
			BT_NOTIFICATION_TYPE_STREAM_END,
			bt_notification_stream_end_destroy);
	notification->stream = bt_get(stream);
	return &notification->parent;
error:
	return NULL;
}

struct bt_ctf_packet *bt_notification_stream_end_get_stream(
		struct bt_notification *notification)
{
	struct bt_notification_stream_end *stream_end;

	stream_end = container_of(notification,
			struct bt_notification_stream_end, parent);
	return bt_get(stream_end->stream);
}
