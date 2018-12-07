/*
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
#include <babeltrace/trace-ir/packet.h>
#include <babeltrace/trace-ir/packet-internal.h>
#include <babeltrace/trace-ir/stream-class.h>
#include <babeltrace/trace-ir/stream.h>
#include <babeltrace/trace-ir/stream-internal.h>
#include <babeltrace/graph/graph-internal.h>
#include <babeltrace/graph/notification-packet-const.h>
#include <babeltrace/graph/notification-packet.h>
#include <babeltrace/graph/notification-packet-internal.h>
#include <babeltrace/object.h>
#include <babeltrace/assert-internal.h>
#include <babeltrace/assert-pre-internal.h>
#include <babeltrace/object-internal.h>
#include <inttypes.h>

BT_HIDDEN
struct bt_notification *bt_notification_packet_beginning_new(struct bt_graph *graph)
{
	struct bt_notification_packet_beginning *notification;

	notification = g_new0(struct bt_notification_packet_beginning, 1);
	if (!notification) {
		BT_LOGE_STR("Failed to allocate one packet beginning notification.");
		goto error;
	}

	bt_notification_init(&notification->parent,
			BT_NOTIFICATION_TYPE_PACKET_BEGINNING,
			(bt_object_release_func) bt_notification_packet_beginning_recycle,
			graph);
	goto end;

error:
	BT_OBJECT_PUT_REF_AND_RESET(notification);

end:
	return (void *) notification;
}

struct bt_notification *bt_notification_packet_beginning_create(
		struct bt_self_notification_iterator *self_notif_iter,
		struct bt_packet *packet)
{
	struct bt_self_component_port_input_notification_iterator *notif_iter =
		(void *) self_notif_iter;
	struct bt_notification_packet_beginning *notification = NULL;
	struct bt_stream *stream;
	struct bt_stream_class *stream_class;

	BT_ASSERT_PRE_NON_NULL(notif_iter, "Notification iterator");
	BT_ASSERT_PRE_NON_NULL(packet, "Packet");
	stream = bt_packet_borrow_stream(packet);
	BT_ASSERT(stream);
	stream_class = bt_stream_borrow_class(stream);
	BT_ASSERT(stream_class);
	BT_LIB_LOGD("Creating packet beginning notification object: "
		"%![packet-]+a, %![stream-]+s, %![sc-]+S",
		packet, stream, stream_class);
	notification = (void *) bt_notification_create_from_pool(
		&notif_iter->graph->packet_begin_notif_pool, notif_iter->graph);
	if (!notification) {
		/* bt_notification_create_from_pool() logs errors */
		goto end;
	}

	BT_ASSERT(!notification->packet);
	notification->packet = packet;
	bt_object_get_no_null_check_no_parent_check(
		&notification->packet->base);
	bt_packet_set_is_frozen(packet, true);
	BT_LIB_LOGD("Created packet beginning notification object: "
		"%![notif-]+n, %![packet-]+a, %![stream-]+s, %![sc-]+S",
		notification, packet, stream, stream_class);
	goto end;

end:
	return (void *) notification;
}

BT_HIDDEN
void bt_notification_packet_beginning_destroy(struct bt_notification *notif)
{
	struct bt_notification_packet_beginning *packet_begin_notif = (void *) notif;

	BT_LIB_LOGD("Destroying packet beginning notification: %!+n", notif);
	BT_LIB_LOGD("Putting packet: %!+a", packet_begin_notif->packet);
	BT_OBJECT_PUT_REF_AND_RESET(packet_begin_notif->packet);
	g_free(notif);
}

BT_HIDDEN
void bt_notification_packet_beginning_recycle(struct bt_notification *notif)
{
	struct bt_notification_packet_beginning *packet_begin_notif = (void *) notif;
	struct bt_graph *graph;

	BT_ASSERT(packet_begin_notif);

	if (unlikely(!notif->graph)) {
		bt_notification_packet_beginning_destroy(notif);
		return;
	}

	BT_LIB_LOGD("Recycling packet beginning notification: %!+n", notif);
	bt_notification_reset(notif);
	bt_object_put_no_null_check(&packet_begin_notif->packet->base);
	packet_begin_notif->packet = NULL;
	graph = notif->graph;
	notif->graph = NULL;
	bt_object_pool_recycle_object(&graph->packet_begin_notif_pool, notif);
}

struct bt_packet *bt_notification_packet_beginning_borrow_packet(
		struct bt_notification *notification)
{
	struct bt_notification_packet_beginning *packet_begin;

	BT_ASSERT_PRE_NON_NULL(notification, "Notification");
	BT_ASSERT_PRE_NOTIF_IS_TYPE(notification,
		BT_NOTIFICATION_TYPE_PACKET_BEGINNING);
	packet_begin = (void *) notification;
	return packet_begin->packet;
}

const struct bt_packet *bt_notification_packet_beginning_borrow_packet_const(
		const struct bt_notification *notification)
{
	return bt_notification_packet_beginning_borrow_packet(
		(void *) notification);
}

BT_HIDDEN
struct bt_notification *bt_notification_packet_end_new(struct bt_graph *graph)
{
	struct bt_notification_packet_end *notification;

	notification = g_new0(struct bt_notification_packet_end, 1);
	if (!notification) {
		BT_LOGE_STR("Failed to allocate one packet end notification.");
		goto error;
	}

	bt_notification_init(&notification->parent,
			BT_NOTIFICATION_TYPE_PACKET_END,
			(bt_object_release_func) bt_notification_packet_end_recycle,
			graph);
	goto end;

error:
	BT_OBJECT_PUT_REF_AND_RESET(notification);

end:
	return (void *) notification;
}

struct bt_notification *bt_notification_packet_end_create(
		struct bt_self_notification_iterator *self_notif_iter,
		struct bt_packet *packet)
{
	struct bt_self_component_port_input_notification_iterator *notif_iter =
		(void *) self_notif_iter;
	struct bt_notification_packet_end *notification = NULL;
	struct bt_stream *stream;
	struct bt_stream_class *stream_class;

	BT_ASSERT_PRE_NON_NULL(notif_iter, "Notification iterator");
	BT_ASSERT_PRE_NON_NULL(packet, "Packet");
	stream = bt_packet_borrow_stream(packet);
	BT_ASSERT(stream);
	stream_class = bt_stream_borrow_class(stream);
	BT_ASSERT(stream_class);
	BT_LIB_LOGD("Creating packet end notification object: "
		"%![packet-]+a, %![stream-]+s, %![sc-]+S",
		packet, stream, stream_class);
	notification = (void *) bt_notification_create_from_pool(
		&notif_iter->graph->packet_end_notif_pool, notif_iter->graph);
	if (!notification) {
		/* bt_notification_create_from_pool() logs errors */
		goto end;
	}

	BT_ASSERT(!notification->packet);
	notification->packet = packet;
	bt_object_get_no_null_check_no_parent_check(
		&notification->packet->base);
	bt_packet_set_is_frozen(packet, true);
	BT_LIB_LOGD("Created packet end notification object: "
		"%![notif-]+n, %![packet-]+a, %![stream-]+s, %![sc-]+S",
		notification, packet, stream, stream_class);
	goto end;

end:
	return (void *) notification;
}

BT_HIDDEN
void bt_notification_packet_end_destroy(struct bt_notification *notif)
{
	struct bt_notification_packet_end *packet_end_notif = (void *) notif;

	BT_LIB_LOGD("Destroying packet end notification: %!+n", notif);
	BT_LIB_LOGD("Putting packet: %!+a", packet_end_notif->packet);
	BT_OBJECT_PUT_REF_AND_RESET(packet_end_notif->packet);
	g_free(notif);
}

BT_HIDDEN
void bt_notification_packet_end_recycle(struct bt_notification *notif)
{
	struct bt_notification_packet_end *packet_end_notif = (void *) notif;
	struct bt_graph *graph;

	BT_ASSERT(packet_end_notif);

	if (!notif->graph) {
		bt_notification_packet_end_destroy(notif);
		return;
	}

	BT_LIB_LOGD("Recycling packet end notification: %!+n", notif);
	bt_notification_reset(notif);
	BT_OBJECT_PUT_REF_AND_RESET(packet_end_notif->packet);
	graph = notif->graph;
	notif->graph = NULL;
	bt_object_pool_recycle_object(&graph->packet_end_notif_pool, notif);
}

struct bt_packet *bt_notification_packet_end_borrow_packet(
		struct bt_notification *notification)
{
	struct bt_notification_packet_end *packet_end;

	BT_ASSERT_PRE_NON_NULL(notification, "Notification");
	BT_ASSERT_PRE_NOTIF_IS_TYPE(notification,
		BT_NOTIFICATION_TYPE_PACKET_END);
	packet_end = (void *) notification;
	return packet_end->packet;
}

const struct bt_packet *bt_notification_packet_end_borrow_packet_const(
		const struct bt_notification *notification)
{
	return bt_notification_packet_end_borrow_packet(
		(void *) notification);
}
