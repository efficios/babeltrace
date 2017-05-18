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
#include <stdbool.h>

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
bt_bool validate_clock_classes(struct bt_notification_event *notif)
{
	/*
	 * For each clock class found in the notification's clock class
	 * priority map, make sure the event has a clock value set for
	 * this clock class. Also make sure that those clock classes
	 * are part of the trace to which the event belongs.
	 */
	bt_bool is_valid = BT_TRUE;
	int trace_cc_count;
	int cc_prio_map_cc_count;
	size_t cc_prio_map_cc_i, trace_cc_i;
	struct bt_ctf_event_class *event_class = NULL;
	struct bt_ctf_stream_class *stream_class = NULL;
	struct bt_ctf_trace *trace = NULL;

	event_class = bt_ctf_event_borrow_event_class(notif->event);
	assert(event_class);
	stream_class = bt_ctf_event_class_borrow_stream_class(event_class);
	assert(stream_class);
	trace = bt_ctf_stream_class_borrow_trace(stream_class);
	assert(trace);
	trace_cc_count = bt_ctf_trace_get_clock_class_count(trace);
	assert(trace_cc_count >= 0);
	cc_prio_map_cc_count =
		bt_clock_class_priority_map_get_clock_class_count(
			notif->cc_prio_map);
	assert(cc_prio_map_cc_count >= 0);

	for (cc_prio_map_cc_i = 0; cc_prio_map_cc_i < cc_prio_map_cc_count;
			cc_prio_map_cc_i++) {
		struct bt_ctf_clock_class *clock_class =
			bt_clock_class_priority_map_get_clock_class_by_index(
				notif->cc_prio_map, cc_prio_map_cc_i);
		struct bt_ctf_clock_value *clock_value;
		bt_bool found_in_trace = BT_FALSE;

		assert(clock_class);
		clock_value = bt_ctf_event_get_clock_value(notif->event,
			clock_class);
		if (!clock_value) {
			is_valid = BT_FALSE;
			goto end;
		}

		bt_put(clock_value);

		for (trace_cc_i = 0; trace_cc_i < trace_cc_count;
				trace_cc_i++) {
			struct bt_ctf_clock_class *trace_clock_class =
				bt_ctf_trace_get_clock_class_by_index(trace,
					trace_cc_i);

			assert(trace_clock_class);

			if (trace_clock_class == clock_class) {
				found_in_trace = BT_TRUE;
				break;
			}
		}

		bt_put(clock_class);

		if (!found_in_trace) {
			is_valid = BT_FALSE;
			goto end;
		}
	}

end:
	return is_valid;
}

static
bool event_has_trace(struct bt_ctf_event *event)
{
	struct bt_ctf_event_class *event_class;
	struct bt_ctf_stream_class *stream_class;

	event_class = bt_ctf_event_borrow_event_class(event);
	assert(event_class);
	stream_class = bt_ctf_event_class_borrow_stream_class(event_class);
	assert(stream_class);
	return bt_ctf_stream_class_borrow_trace(stream_class) != NULL;
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

	if (!event_has_trace(event)) {
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
	bt_clock_class_priority_map_freeze(notification->cc_prio_map);
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
