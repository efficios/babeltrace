/*
 * Babeltrace Plug-in Packet-related Notifications
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

#define BT_LOG_TAG "NOTIF-PACKET"
#include <babeltrace/lib-logging-internal.h>

#include <babeltrace/compiler-internal.h>
#include <babeltrace/ctf-ir/packet.h>
#include <babeltrace/ctf-ir/packet-internal.h>
#include <babeltrace/ctf-ir/stream-class.h>
#include <babeltrace/ctf-ir/stream.h>
#include <babeltrace/ctf-ir/stream-internal.h>
#include <babeltrace/graph/notification-packet-internal.h>
#include <babeltrace/assert-internal.h>
#include <babeltrace/assert-pre-internal.h>
#include <inttypes.h>

static
void bt_notification_packet_begin_destroy(struct bt_object *obj)
{
	struct bt_notification_packet_begin *notification =
			(struct bt_notification_packet_begin *) obj;

	BT_LOGD("Destroying packet beginning notification: addr=%p",
		notification);
	BT_LOGD_STR("Putting packet.");
	BT_PUT(notification->packet);
	g_free(notification);
}

static
void bt_notification_packet_end_destroy(struct bt_object *obj)
{
	struct bt_notification_packet_end *notification =
			(struct bt_notification_packet_end *) obj;

	BT_LOGD("Destroying packet end notification: addr=%p",
		notification);
	BT_LOGD_STR("Putting packet.");
	BT_PUT(notification->packet);
	g_free(notification);
}

struct bt_notification *bt_notification_packet_begin_create(
		struct bt_packet *packet)
{
	struct bt_notification_packet_begin *notification;
	struct bt_stream *stream;
	struct bt_stream_class *stream_class;

	BT_ASSERT_PRE_NON_NULL(packet, "Packet");
	stream = bt_packet_borrow_stream(packet);
	BT_ASSERT(stream);
	stream_class = bt_stream_borrow_stream_class(stream);
	BT_ASSERT(stream_class);
	BT_LOGD("Creating packet beginning notification object: "
		"packet-addr=%p, stream-addr=%p, stream-name=\"%s\", "
		"stream-class-addr=%p, stream-class-name=\"%s\", "
		"stream-class-id=%" PRId64,
		packet, stream, bt_stream_get_name(stream),
		stream_class,
		bt_stream_class_get_name(stream_class),
		bt_stream_class_get_id(stream_class));
	notification = g_new0(struct bt_notification_packet_begin, 1);
	if (!notification) {
		BT_LOGE_STR("Failed to allocate one packet beginning notification.");
		goto error;
	}

	bt_notification_init(&notification->parent,
			BT_NOTIFICATION_TYPE_PACKET_BEGIN,
			bt_notification_packet_begin_destroy);
	notification->packet = bt_get(packet);
	BT_LOGD("Created packet beginning notification object: "
		"packet-addr=%p, stream-addr=%p, stream-name=\"%s\", "
		"stream-class-addr=%p, stream-class-name=\"%s\", "
		"stream-class-id=%" PRId64 ", addr=%p",
		packet, stream, bt_stream_get_name(stream),
		stream_class,
		bt_stream_class_get_name(stream_class),
		bt_stream_class_get_id(stream_class), notification);
	return &notification->parent;
error:
	return NULL;
}

struct bt_packet *bt_notification_packet_begin_get_packet(
		struct bt_notification *notification)
{
	struct bt_notification_packet_begin *packet_begin;

	BT_ASSERT_PRE_NON_NULL(notification, "Notification");
	BT_ASSERT_PRE_NOTIF_IS_TYPE(notification,
		BT_NOTIFICATION_TYPE_PACKET_BEGIN);
	packet_begin = container_of(notification,
			struct bt_notification_packet_begin, parent);
	return bt_get(packet_begin->packet);
}

struct bt_notification *bt_notification_packet_end_create(
		struct bt_packet *packet)
{
	struct bt_notification_packet_end *notification;
	struct bt_stream *stream;
	struct bt_stream_class *stream_class;

	BT_ASSERT_PRE_NON_NULL(packet, "Packet");
	stream = bt_packet_borrow_stream(packet);
	BT_ASSERT(stream);
	stream_class = bt_stream_borrow_stream_class(stream);
	BT_ASSERT(stream_class);
	BT_LOGD("Creating packet end notification object: "
		"packet-addr=%p, stream-addr=%p, stream-name=\"%s\", "
		"stream-class-addr=%p, stream-class-name=\"%s\", "
		"stream-class-id=%" PRId64,
		packet, stream, bt_stream_get_name(stream),
		stream_class,
		bt_stream_class_get_name(stream_class),
		bt_stream_class_get_id(stream_class));
	notification = g_new0(struct bt_notification_packet_end, 1);
	if (!notification) {
		BT_LOGE_STR("Failed to allocate one packet end notification.");
		goto error;
	}

	bt_notification_init(&notification->parent,
			BT_NOTIFICATION_TYPE_PACKET_END,
			bt_notification_packet_end_destroy);
	notification->packet = bt_get(packet);
	BT_LOGD("Created packet end notification object: "
		"packet-addr=%p, stream-addr=%p, stream-name=\"%s\", "
		"stream-class-addr=%p, stream-class-name=\"%s\", "
		"stream-class-id=%" PRId64 ", addr=%p",
		packet, stream, bt_stream_get_name(stream),
		stream_class,
		bt_stream_class_get_name(stream_class),
		bt_stream_class_get_id(stream_class), notification);
	return &notification->parent;
error:
	return NULL;
}

struct bt_packet *bt_notification_packet_end_get_packet(
		struct bt_notification *notification)
{
	struct bt_notification_packet_end *packet_end;

	BT_ASSERT_PRE_NON_NULL(notification, "Notification");
	BT_ASSERT_PRE_NOTIF_IS_TYPE(notification,
		BT_NOTIFICATION_TYPE_PACKET_END);
	packet_end = container_of(notification,
			struct bt_notification_packet_end, parent);
	return bt_get(packet_end->packet);
}
