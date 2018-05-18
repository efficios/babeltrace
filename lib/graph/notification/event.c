/*
 * Babeltrace Plug-in Event Notification
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

#define BT_LOG_TAG "NOTIF-EVENT"
#include <babeltrace/lib-logging-internal.h>

#include <babeltrace/compiler-internal.h>
#include <babeltrace/ctf-ir/event.h>
#include <babeltrace/ctf-ir/event-internal.h>
#include <babeltrace/ctf-ir/event-class-internal.h>
#include <babeltrace/ctf-ir/stream-class-internal.h>
#include <babeltrace/ctf-ir/trace.h>
#include <babeltrace/graph/clock-class-priority-map.h>
#include <babeltrace/graph/clock-class-priority-map-internal.h>
#include <babeltrace/graph/notification-event-internal.h>
#include <babeltrace/types.h>
#include <babeltrace/assert-internal.h>
#include <babeltrace/assert-pre-internal.h>
#include <stdbool.h>
#include <inttypes.h>

static
void bt_notification_event_destroy(struct bt_object *obj)
{
	struct bt_notification_event *notification =
			(struct bt_notification_event *) obj;

	BT_LOGD("Destroying event notification: addr=%p", notification);
	BT_LOGD_STR("Recycling event.");
	bt_event_recycle(notification->event);
	notification->event = NULL;
	BT_LOGD_STR("Putting clock class priority map.");
	BT_PUT(notification->cc_prio_map);
	g_free(notification);
}

BT_ASSERT_PRE_FUNC
static inline bool event_class_has_trace(struct bt_event_class *event_class)
{
	struct bt_stream_class *stream_class;

	stream_class = bt_event_class_borrow_stream_class(event_class);
	BT_ASSERT(stream_class);
	return bt_stream_class_borrow_trace(stream_class) != NULL;
}

struct bt_notification *bt_notification_event_create(
		struct bt_event_class *event_class,
		struct bt_packet *packet,
		struct bt_clock_class_priority_map *cc_prio_map)
{
	struct bt_notification_event *notification = NULL;

	BT_ASSERT_PRE_NON_NULL(event_class, "Event class");
	BT_ASSERT_PRE_NON_NULL(packet, "Packet");

	if (cc_prio_map) {
		/* Function's reference, released at the end */
		bt_get(cc_prio_map);
	} else {
		cc_prio_map = bt_clock_class_priority_map_create();
		if (!cc_prio_map) {
			BT_LOGE_STR("Cannot create empty clock class priority map.");
			goto error;
		}
	}

	BT_ASSERT(cc_prio_map);
	BT_LOGD("Creating event notification object: "
		"event-class-addr=%p, "
		"event-class-name=\"%s\", event-class-id=%" PRId64 ", "
		"cc-prio-map-addr=%p",
		event_class,
		bt_event_class_get_name(event_class),
		bt_event_class_get_id(event_class), cc_prio_map);

	BT_ASSERT_PRE(event_class_has_trace(event_class),
		"Event class is not part of a trace: %!+E", event_class);
	notification = g_new0(struct bt_notification_event, 1);
	if (!notification) {
		BT_LOGE_STR("Failed to allocate one event notification.");
		goto error;
	}

	bt_notification_init(&notification->parent, BT_NOTIFICATION_TYPE_EVENT,
		bt_notification_event_destroy);
	notification->event = bt_event_create(event_class, packet);
	if (!notification->event) {
		BT_LIB_LOGE("Cannot create event from event class: "
			"%![event-class-]+E", event_class);
		goto error;
	}

	notification->cc_prio_map = bt_get(cc_prio_map);
	BT_LOGD_STR("Freezing event notification's clock class priority map.");
	bt_clock_class_priority_map_freeze(notification->cc_prio_map);
	BT_LOGD("Created event notification object: "
		"event-addr=%p, event-class-addr=%p, "
		"event-class-name=\"%s\", event-class-id=%" PRId64 ", "
		"cc-prio-map-addr=%p, notif-addr=%p",
		notification->event, event_class,
		bt_event_class_get_name(event_class),
		bt_event_class_get_id(event_class), cc_prio_map,
		notification);
	goto end;

error:
	BT_PUT(notification);

end:
	bt_put(cc_prio_map);
	return &notification->parent;
}

struct bt_event *bt_notification_event_borrow_event(
		struct bt_notification *notification)
{
	struct bt_notification_event *event_notification;

	BT_ASSERT_PRE_NON_NULL(notification, "Notification");
	BT_ASSERT_PRE_NOTIF_IS_TYPE(notification, BT_NOTIFICATION_TYPE_EVENT);
	event_notification = container_of(notification,
			struct bt_notification_event, parent);
	return event_notification->event;
}

extern struct bt_clock_class_priority_map *
bt_notification_event_borrow_clock_class_priority_map(
		struct bt_notification *notification)
{
	struct bt_notification_event *event_notification;

	BT_ASSERT_PRE_NON_NULL(notification, "Notification");
	BT_ASSERT_PRE_NOTIF_IS_TYPE(notification, BT_NOTIFICATION_TYPE_EVENT);
	event_notification = container_of(notification,
			struct bt_notification_event, parent);
	return event_notification->cc_prio_map;
}
