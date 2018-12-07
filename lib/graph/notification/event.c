/*
 * Copyright 2017-2018 Philippe Proulx <pproulx@efficios.com>
 * Copyright 2016 Jérémie Galarneau <jeremie.galarneau@efficios.com>
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

#define BT_LOG_TAG "NOTIF-EVENT"
#include <babeltrace/lib-logging-internal.h>

#include <babeltrace/assert-internal.h>
#include <babeltrace/assert-pre-internal.h>
#include <babeltrace/compiler-internal.h>
#include <babeltrace/object-internal.h>
#include <babeltrace/trace-ir/event.h>
#include <babeltrace/trace-ir/event-internal.h>
#include <babeltrace/trace-ir/event-class-internal.h>
#include <babeltrace/trace-ir/stream-class-internal.h>
#include <babeltrace/trace-ir/trace.h>
#include <babeltrace/graph/graph-internal.h>
#include <babeltrace/graph/notification-event-const.h>
#include <babeltrace/graph/notification-event.h>
#include <babeltrace/graph/notification-event-internal.h>
#include <babeltrace/types.h>
#include <stdbool.h>
#include <inttypes.h>

BT_ASSERT_PRE_FUNC
static inline bool event_class_has_trace(struct bt_event_class *event_class)
{
	struct bt_stream_class *stream_class;

	stream_class = bt_event_class_borrow_stream_class(event_class);
	BT_ASSERT(stream_class);
	return bt_stream_class_borrow_trace_class(stream_class) != NULL;
}

BT_HIDDEN
struct bt_notification *bt_notification_event_new(
		struct bt_graph *graph)
{
	struct bt_notification_event *notification = NULL;

	notification = g_new0(struct bt_notification_event, 1);
	if (!notification) {
		BT_LOGE_STR("Failed to allocate one event notification.");
		goto error;
	}

	bt_notification_init(&notification->parent, BT_NOTIFICATION_TYPE_EVENT,
		(bt_object_release_func) bt_notification_event_recycle, graph);
	goto end;

error:
	BT_OBJECT_PUT_REF_AND_RESET(notification);

end:
	return (void *) notification;
}

struct bt_notification *bt_notification_event_create(
		struct bt_self_notification_iterator *self_notif_iter,
		struct bt_event_class *event_class,
		struct bt_packet *packet)
{
	struct bt_self_component_port_input_notification_iterator *notif_iter =
		(void *) self_notif_iter;
	struct bt_notification_event *notification = NULL;
	struct bt_event *event;

	BT_ASSERT_PRE_NON_NULL(notif_iter, "Notification iterator");
	BT_ASSERT_PRE_NON_NULL(event_class, "Event class");
	BT_ASSERT_PRE_NON_NULL(packet, "Packet");
	BT_ASSERT_PRE(event_class_has_trace(event_class),
		"Event class is not part of a trace: %!+E", event_class);
	BT_LIB_LOGD("Creating event notification object: %![ec-]+E",
		event_class);
	event = bt_event_create(event_class, packet);
	if (unlikely(!event)) {
		BT_LIB_LOGE("Cannot create event from event class: "
			"%![ec-]+E", event_class);
		goto error;
	}

	/*
	 * Create notification from pool _after_ we have everything
	 * (in this case, a valid event object) so that we never have an
	 * error condition with a non-NULL notification object.
	 * Otherwise:
	 *
	 * * We cannot recycle the notification on error because
	 *   bt_notification_event_recycle() expects a complete
	 *   notification (and the event or clock class priority map
	 *   object could be unset).
	 *
	 * * We cannot destroy the notification because we would need
	 *   to notify the graph (pool owner) so that it removes the
	 *   notification from its notification array.
	 */
	notification = (void *) bt_notification_create_from_pool(
		&notif_iter->graph->event_notif_pool, notif_iter->graph);
	if (unlikely(!notification)) {
		/* bt_notification_create_from_pool() logs errors */
		goto error;
	}

	BT_ASSERT(!notification->event);
	notification->event = event;
	bt_packet_set_is_frozen(packet, true);
	bt_event_class_freeze(event_class);
	BT_LIB_LOGD("Created event notification object: "
		"%![notif-]+n, %![event-]+e", notification, event);
	goto end;

error:
	BT_ASSERT(!notification);
	bt_event_destroy(event);

end:
	return (void *) notification;
}

BT_HIDDEN
void bt_notification_event_destroy(struct bt_notification *notif)
{
	struct bt_notification_event *event_notif = (void *) notif;

	BT_LIB_LOGD("Destroying event notification: %!+n", notif);

	if (event_notif->event) {
		BT_LIB_LOGD("Recycling event: %!+e", event_notif->event);
		bt_event_recycle(event_notif->event);
		event_notif->event = NULL;
	}

	g_free(notif);
}

BT_HIDDEN
void bt_notification_event_recycle(struct bt_notification *notif)
{
	struct bt_notification_event *event_notif = (void *) notif;
	struct bt_graph *graph;

	BT_ASSERT(event_notif);

	if (unlikely(!notif->graph)) {
		bt_notification_event_destroy(notif);
		return;
	}

	BT_LIB_LOGD("Recycling event notification: %![notif-]+n, %![event-]+e",
		notif, event_notif->event);
	bt_notification_reset(notif);
	BT_ASSERT(event_notif->event);
	bt_event_recycle(event_notif->event);
	event_notif->event = NULL;
	graph = notif->graph;
	notif->graph = NULL;
	bt_object_pool_recycle_object(&graph->event_notif_pool, notif);
}

static inline
struct bt_event *borrow_event(struct bt_notification *notification)
{
	struct bt_notification_event *event_notification;

	BT_ASSERT_PRE_NON_NULL(notification, "Notification");
	BT_ASSERT_PRE_NOTIF_IS_TYPE(notification, BT_NOTIFICATION_TYPE_EVENT);
	event_notification = container_of(notification,
			struct bt_notification_event, parent);
	return event_notification->event;
}

struct bt_event *bt_notification_event_borrow_event(
		struct bt_notification *notification)
{
	return borrow_event(notification);
}

const struct bt_event *bt_notification_event_borrow_event_const(
		const struct bt_notification *notification)
{
	return borrow_event((void *) notification);
}
