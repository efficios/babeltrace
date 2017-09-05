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
		struct bt_ctf_packet *packet)
{
	struct bt_notification_packet_begin *notification;
	struct bt_ctf_stream *stream;
	struct bt_ctf_stream_class *stream_class;

	if (!packet) {
		BT_LOGW_STR("Invalid parameter: packet is NULL.");
		goto error;
	}

	stream = bt_ctf_packet_borrow_stream(packet);
	assert(stream);
	stream_class = bt_ctf_stream_borrow_stream_class(stream);
	assert(stream_class);
	BT_LOGD("Creating packet beginning notification object: "
		"packet-addr=%p, stream-addr=%p, stream-name=\"%s\", "
		"stream-class-addr=%p, stream-class-name=\"%s\", "
		"stream-class-id=%" PRId64,
		packet, stream, bt_ctf_stream_get_name(stream),
		stream_class,
		bt_ctf_stream_class_get_name(stream_class),
		bt_ctf_stream_class_get_id(stream_class));
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
		packet, stream, bt_ctf_stream_get_name(stream),
		stream_class,
		bt_ctf_stream_class_get_name(stream_class),
		bt_ctf_stream_class_get_id(stream_class), notification);
	return &notification->parent;
error:
	return NULL;
}

struct bt_ctf_packet *bt_notification_packet_begin_get_packet(
		struct bt_notification *notification)
{
	struct bt_ctf_packet *ret = NULL;
	struct bt_notification_packet_begin *packet_begin;

	if (!notification) {
		BT_LOGW_STR("Invalid parameter: notification is NULL.");
		goto end;
	}

	if (notification->type != BT_NOTIFICATION_TYPE_PACKET_BEGIN) {
		BT_LOGW("Invalid parameter: notification is not a packet beginning notification: "
			"addr%p, notif-type=%s",
			notification, bt_notification_type_string(
				bt_notification_get_type(notification)));
		goto end;
	}

	packet_begin = container_of(notification,
			struct bt_notification_packet_begin, parent);
	ret = bt_get(packet_begin->packet);
end:
	return ret;
}

struct bt_notification *bt_notification_packet_end_create(
		struct bt_ctf_packet *packet)
{
	struct bt_notification_packet_end *notification;
	struct bt_ctf_stream *stream;
	struct bt_ctf_stream_class *stream_class;

	if (!packet) {
		BT_LOGW_STR("Invalid parameter: packet is NULL.");
		goto error;
	}

	stream = bt_ctf_packet_borrow_stream(packet);
	assert(stream);
	stream_class = bt_ctf_stream_borrow_stream_class(stream);
	assert(stream_class);
	BT_LOGD("Creating packet end notification object: "
		"packet-addr=%p, stream-addr=%p, stream-name=\"%s\", "
		"stream-class-addr=%p, stream-class-name=\"%s\", "
		"stream-class-id=%" PRId64,
		packet, stream, bt_ctf_stream_get_name(stream),
		stream_class,
		bt_ctf_stream_class_get_name(stream_class),
		bt_ctf_stream_class_get_id(stream_class));
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
		packet, stream, bt_ctf_stream_get_name(stream),
		stream_class,
		bt_ctf_stream_class_get_name(stream_class),
		bt_ctf_stream_class_get_id(stream_class), notification);
	return &notification->parent;
error:
	return NULL;
}

struct bt_ctf_packet *bt_notification_packet_end_get_packet(
		struct bt_notification *notification)
{
	struct bt_ctf_packet *ret = NULL;
	struct bt_notification_packet_end *packet_end;

	if (!notification) {
		BT_LOGW_STR("Invalid parameter: notification is NULL.");
		goto end;
	}

	if (notification->type != BT_NOTIFICATION_TYPE_PACKET_END) {
		BT_LOGW("Invalid parameter: notification is not a packet end notification: "
			"addr%p, notif-type=%s",
			notification, bt_notification_type_string(
				bt_notification_get_type(notification)));
		goto end;
	}

	packet_end = container_of(notification,
			struct bt_notification_packet_end, parent);
	ret = bt_get(packet_end->packet);
end:
	return ret;
}
