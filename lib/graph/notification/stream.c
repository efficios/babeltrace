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
#include <babeltrace/graph/notification-stream-internal.h>
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
		struct bt_ctf_stream *stream)
{
	struct bt_notification_stream_end *notification;
	struct bt_ctf_stream_class *stream_class;

	if (!stream) {
		BT_LOGW_STR("Invalid parameter: stream is NULL.");
		goto error;
	}

	stream_class = bt_ctf_stream_borrow_stream_class(stream);
	assert(stream_class);

	if (stream->pos.fd >= 0) {
		BT_LOGW("Invalid parameter: stream is a CTF writer stream: "
			"stream-addr=%p, stream-name=\"%s\", "
			"stream-class-addr=%p, stream-class-name\"%s\", "
			"stream-class-id=%" PRId64,
			stream, bt_ctf_stream_get_name(stream), stream_class,
			bt_ctf_stream_class_get_name(stream_class),
			bt_ctf_stream_class_get_id(stream_class));
		goto error;
	}

	BT_LOGD("Creating stream end notification object: "
		"stream-addr=%p, stream-name=\"%s\", "
		"stream-class-addr=%p, stream-class-name=\"%s\", "
		"stream-class-id=%" PRId64,
		stream, bt_ctf_stream_get_name(stream),
		stream_class,
		bt_ctf_stream_class_get_name(stream_class),
		bt_ctf_stream_class_get_id(stream_class));
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
		stream, bt_ctf_stream_get_name(stream),
		stream_class,
		bt_ctf_stream_class_get_name(stream_class),
		bt_ctf_stream_class_get_id(stream_class), notification);
	return &notification->parent;
error:
	return NULL;
}

struct bt_ctf_stream *bt_notification_stream_end_get_stream(
		struct bt_notification *notification)
{
	struct bt_notification_stream_end *stream_end;
	struct bt_ctf_stream *stream = NULL;

	if (!notification) {
		BT_LOGW_STR("Invalid parameter: notification is NULL.");
		goto end;
	}

	if (notification->type != BT_NOTIFICATION_TYPE_STREAM_END) {
		BT_LOGW("Invalid parameter: notification is not a stream end notification: "
			"addr%p, notif-type=%s",
			notification, bt_notification_type_string(
				bt_notification_get_type(notification)));
		goto end;
	}

	stream_end = container_of(notification,
			struct bt_notification_stream_end, parent);
	stream = bt_get(stream_end->stream);

end:
	return stream;
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
		struct bt_ctf_stream *stream)
{
	struct bt_notification_stream_begin *notification;
	struct bt_ctf_stream_class *stream_class;

	if (!stream) {
		BT_LOGW_STR("Invalid parameter: stream is NULL.");
		goto error;
	}

	stream_class = bt_ctf_stream_borrow_stream_class(stream);
	assert(stream_class);

	if (stream->pos.fd >= 0) {
		BT_LOGW("Invalid parameter: stream is a CTF writer stream: "
			"stream-addr=%p, stream-name=\"%s\", "
			"stream-class-addr=%p, stream-class-name\"%s\", "
			"stream-class-id=%" PRId64,
			stream, bt_ctf_stream_get_name(stream), stream_class,
			bt_ctf_stream_class_get_name(stream_class),
			bt_ctf_stream_class_get_id(stream_class));
		goto error;
	}

	BT_LOGD("Creating stream beginning notification object: "
		"stream-addr=%p, stream-name=\"%s\", "
		"stream-class-addr=%p, stream-class-name=\"%s\", "
		"stream-class-id=%" PRId64,
		stream, bt_ctf_stream_get_name(stream),
		stream_class,
		bt_ctf_stream_class_get_name(stream_class),
		bt_ctf_stream_class_get_id(stream_class));
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
		stream, bt_ctf_stream_get_name(stream),
		stream_class,
		bt_ctf_stream_class_get_name(stream_class),
		bt_ctf_stream_class_get_id(stream_class), notification);
	return &notification->parent;
error:
	return NULL;
}

struct bt_ctf_stream *bt_notification_stream_begin_get_stream(
		struct bt_notification *notification)
{
	struct bt_notification_stream_begin *stream_begin;
	struct bt_ctf_stream *stream = NULL;

	if (!notification) {
		BT_LOGW_STR("Invalid parameter: notification is NULL.");
		goto end;
	}

	if (notification->type != BT_NOTIFICATION_TYPE_STREAM_BEGIN) {
		BT_LOGW("Invalid parameter: notification is not a stream beginning notification: "
			"addr%p, notif-type=%s",
			notification, bt_notification_type_string(
				bt_notification_get_type(notification)));
		goto end;
	}

	stream_begin = container_of(notification,
			struct bt_notification_stream_begin, parent);
	stream = bt_get(stream_begin->stream);

end:
	return stream;
}
