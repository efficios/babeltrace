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

#define BT_LOG_TAG "NOTIF-STREAM"
#include <babeltrace/lib-logging-internal.h>

#include <babeltrace/compiler-internal.h>
#include <babeltrace/ctf-ir/stream-internal.h>
#include <babeltrace/ctf-ir/stream-class.h>
#include <babeltrace/graph/notification-stream-internal.h>
#include <babeltrace/assert-internal.h>
#include <babeltrace/assert-pre-internal.h>
#include <inttypes.h>

static
void bt_notification_stream_end_destroy(struct bt_object *obj)
{
	struct bt_notification_stream_end *notification =
			(struct bt_notification_stream_end *) obj;

	BT_LOGD("Destroying stream end notification: addr=%p",
		notification);
	BT_LOGD_STR("Putting stream.");
	BT_PUT(notification->stream);
	g_free(notification);
}

struct bt_notification *bt_notification_stream_end_create(
		struct bt_stream *stream)
{
	struct bt_notification_stream_end *notification;
	struct bt_stream_class *stream_class;

	BT_ASSERT_PRE_NON_NULL(stream, "Stream");
	BT_ASSERT_PRE(stream->pos.fd < 0,
		"Stream is a CTF writer stream: %!+s", stream);
	stream_class = bt_stream_borrow_stream_class(stream);
	BT_ASSERT(stream_class);
	BT_LOGD("Creating stream end notification object: "
		"stream-addr=%p, stream-name=\"%s\", "
		"stream-class-addr=%p, stream-class-name=\"%s\", "
		"stream-class-id=%" PRId64,
		stream, bt_stream_get_name(stream),
		stream_class,
		bt_stream_class_get_name(stream_class),
		bt_stream_class_get_id(stream_class));
	notification = g_new0(struct bt_notification_stream_end, 1);
	if (!notification) {
		BT_LOGE_STR("Failed to allocate one stream end notification.");
		goto error;
	}

	bt_notification_init(&notification->parent,
			BT_NOTIFICATION_TYPE_STREAM_END,
			bt_notification_stream_end_destroy);
	notification->stream = bt_get(stream);
	BT_LOGD("Created stream end notification object: "
		"stream-addr=%p, stream-name=\"%s\", "
		"stream-class-addr=%p, stream-class-name=\"%s\", "
		"stream-class-id=%" PRId64 ", addr=%p",
		stream, bt_stream_get_name(stream),
		stream_class,
		bt_stream_class_get_name(stream_class),
		bt_stream_class_get_id(stream_class), notification);
	return &notification->parent;
error:
	return NULL;
}

struct bt_stream *bt_notification_stream_end_get_stream(
		struct bt_notification *notification)
{
	struct bt_notification_stream_end *stream_end;

	BT_ASSERT_PRE_NON_NULL(notification, "Notification");
	BT_ASSERT_PRE_NOTIF_IS_TYPE(notification,
		BT_NOTIFICATION_TYPE_STREAM_END);
	stream_end = container_of(notification,
			struct bt_notification_stream_end, parent);
	return bt_get(stream_end->stream);
}

static
void bt_notification_stream_begin_destroy(struct bt_object *obj)
{
	struct bt_notification_stream_begin *notification =
			(struct bt_notification_stream_begin *) obj;

	BT_LOGD("Destroying stream beginning notification: addr=%p",
		notification);
	BT_LOGD_STR("Putting stream.");
	BT_PUT(notification->stream);
	g_free(notification);
}

struct bt_notification *bt_notification_stream_begin_create(
		struct bt_stream *stream)
{
	struct bt_notification_stream_begin *notification;
	struct bt_stream_class *stream_class;

	BT_ASSERT_PRE_NON_NULL(stream, "Stream");
	BT_ASSERT_PRE(stream->pos.fd < 0,
		"Stream is a CTF writer stream: %!+s", stream);
	stream_class = bt_stream_borrow_stream_class(stream);
	BT_ASSERT(stream_class);
	BT_LOGD("Creating stream beginning notification object: "
		"stream-addr=%p, stream-name=\"%s\", "
		"stream-class-addr=%p, stream-class-name=\"%s\", "
		"stream-class-id=%" PRId64,
		stream, bt_stream_get_name(stream),
		stream_class,
		bt_stream_class_get_name(stream_class),
		bt_stream_class_get_id(stream_class));
	notification = g_new0(struct bt_notification_stream_begin, 1);
	if (!notification) {
		BT_LOGE_STR("Failed to allocate one stream beginning notification.");
		goto error;
	}

	bt_notification_init(&notification->parent,
			BT_NOTIFICATION_TYPE_STREAM_BEGIN,
			bt_notification_stream_begin_destroy);
	notification->stream = bt_get(stream);
	BT_LOGD("Created stream beginning notification object: "
		"stream-addr=%p, stream-name=\"%s\", "
		"stream-class-addr=%p, stream-class-name=\"%s\", "
		"stream-class-id=%" PRId64 ", addr=%p",
		stream, bt_stream_get_name(stream),
		stream_class,
		bt_stream_class_get_name(stream_class),
		bt_stream_class_get_id(stream_class), notification);
	return &notification->parent;
error:
	return NULL;
}

struct bt_stream *bt_notification_stream_begin_get_stream(
		struct bt_notification *notification)
{
	struct bt_notification_stream_begin *stream_begin;

	BT_ASSERT_PRE_NON_NULL(notification, "Notification");
	BT_ASSERT_PRE_NOTIF_IS_TYPE(notification,
		BT_NOTIFICATION_TYPE_STREAM_BEGIN);
	stream_begin = container_of(notification,
			struct bt_notification_stream_begin, parent);
	return bt_get(stream_begin->stream);
}
