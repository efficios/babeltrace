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

#include <babeltrace/compiler.h>
#include <babeltrace/ctf-ir/event.h>
#include <babeltrace/ctf-ir/event-internal.h>
#include <babeltrace/ctf-ir/event-class.h>
#include <babeltrace/ctf-ir/stream-class.h>
#include <babeltrace/ctf-ir/trace.h>
#include <babeltrace/graph/clock-class-priority-map.h>
#include <babeltrace/graph/notification-event-internal.h>

static
void bt_notification_event_destroy(struct bt_object *obj)
{
	struct bt_notification_event *notification =
			(struct bt_notification_event *) obj;

	BT_PUT(notification->event);
	BT_PUT(notification->cc_prio_map);
	g_free(notification);
}

static
bool validate_clock_classes(struct bt_notification_event *notif)
{
	/*
	 * For each clock class found in the event's trace, get the
	 * event's clock value for this clock class, and if it exists,
	 * make sure that this clock class has a priority in the
	 * notification's clock class priority map.
	 */
	bool is_valid = true;
	int ret;
	int count;
	size_t i;
	struct bt_ctf_event_class *event_class = NULL;
	struct bt_ctf_stream_class *stream_class = NULL;
	struct bt_ctf_trace *trace = NULL;
	uint64_t prio;

	event_class = bt_ctf_event_get_class(notif->event);
	assert(event_class);
	stream_class = bt_ctf_event_class_get_stream_class(event_class);
	assert(stream_class);
	trace = bt_ctf_stream_class_get_trace(stream_class);
	assert(trace);
	count = bt_ctf_trace_get_clock_class_count(trace);
	assert(count >= 0);

	for (i = 0; i < count; i++) {
		struct bt_ctf_clock_class *clock_class =
			bt_ctf_trace_get_clock_class(trace, i);

		assert(clock_class);
		ret = bt_clock_class_priority_map_get_clock_class_priority(
			notif->cc_prio_map, clock_class, &prio);
		bt_put(clock_class);
		if (ret) {
			is_valid = false;
			goto end;
		}
	}

end:
	bt_put(trace);
	bt_put(stream_class);
	bt_put(event_class);
	return is_valid;
}

struct bt_notification *bt_notification_event_create(struct bt_ctf_event *event,
		struct bt_clock_class_priority_map *cc_prio_map)
{
	struct bt_notification_event *notification = NULL;

	if (!event || !cc_prio_map) {
		goto error;
	}

	if (!bt_ctf_event_borrow_packet(event)) {
		goto error;
	}

	notification = g_new0(struct bt_notification_event, 1);
	if (!notification) {
		goto error;
	}
	bt_notification_init(&notification->parent,
			BT_NOTIFICATION_TYPE_EVENT,
			bt_notification_event_destroy);
	notification->event = bt_get(event);
	notification->cc_prio_map = bt_get(cc_prio_map);
	if (!validate_clock_classes(notification)) {
		goto error;
	}

	bt_ctf_event_freeze(notification->event);
	return &notification->parent;
error:
	bt_put(notification);
	return NULL;
}

struct bt_ctf_event *bt_notification_event_get_event(
		struct bt_notification *notification)
{
	struct bt_ctf_event *event = NULL;
	struct bt_notification_event *event_notification;

	if (bt_notification_get_type(notification) !=
			BT_NOTIFICATION_TYPE_EVENT) {
		goto end;
	}
	event_notification = container_of(notification,
			struct bt_notification_event, parent);
	event = bt_get(event_notification->event);
end:
	return event;
}

extern struct bt_clock_class_priority_map *
bt_notification_event_get_clock_class_priority_map(
		struct bt_notification *notification)
{
	struct bt_clock_class_priority_map *cc_prio_map = NULL;
	struct bt_notification_event *event_notification;

	if (bt_notification_get_type(notification) !=
			BT_NOTIFICATION_TYPE_EVENT) {
		goto end;
	}

	event_notification = container_of(notification,
			struct bt_notification_event, parent);
	cc_prio_map = bt_get(event_notification->cc_prio_map);
end:
	return cc_prio_map;
}
