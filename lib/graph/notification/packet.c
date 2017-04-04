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

#include <babeltrace/compiler-internal.h>
#include <babeltrace/graph/notification-packet-internal.h>

static
void bt_notification_packet_begin_destroy(struct bt_object *obj)
{
	struct bt_notification_packet_begin *notification =
			(struct bt_notification_packet_begin *) obj;

	BT_PUT(notification->packet);
	g_free(notification);
}

static
void bt_notification_packet_end_destroy(struct bt_object *obj)
{
	struct bt_notification_packet_end *notification =
			(struct bt_notification_packet_end *) obj;

	BT_PUT(notification->packet);
	g_free(notification);
}

struct bt_notification *bt_notification_packet_begin_create(
		struct bt_ctf_packet *packet)
{
	struct bt_notification_packet_begin *notification;

	if (!packet) {
		goto error;
	}

	notification = g_new0(struct bt_notification_packet_begin, 1);
	bt_notification_init(&notification->parent,
			BT_NOTIFICATION_TYPE_PACKET_BEGIN,
			bt_notification_packet_begin_destroy);
	notification->packet = bt_get(packet);
	return &notification->parent;
error:
	return NULL;
}

struct bt_ctf_packet *bt_notification_packet_begin_get_packet(
		struct bt_notification *notification)
{
	struct bt_ctf_packet *ret = NULL;
	struct bt_notification_packet_begin *packet_begin;

	if (notification->type != BT_NOTIFICATION_TYPE_PACKET_BEGIN) {
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

	if (!packet) {
		goto error;
	}

	notification = g_new0(struct bt_notification_packet_end, 1);
	bt_notification_init(&notification->parent,
			BT_NOTIFICATION_TYPE_PACKET_END,
			bt_notification_packet_end_destroy);
	notification->packet = bt_get(packet);
	return &notification->parent;
error:
	return NULL;
}

struct bt_ctf_packet *bt_notification_packet_end_get_packet(
		struct bt_notification *notification)
{
	struct bt_ctf_packet *ret = NULL;
	struct bt_notification_packet_end *packet_end;

	if (notification->type != BT_NOTIFICATION_TYPE_PACKET_END) {
		goto end;
	}

	packet_end = container_of(notification,
			struct bt_notification_packet_end, parent);
	ret = bt_get(packet_end->packet);
end:
	return ret;
}
