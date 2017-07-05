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
#include <stdbool.h>
#include <inttypes.h>

static
void bt_notification_event_destroy(struct bt_object *obj)
{
	struct bt_notification_event *notification =
			(struct bt_notification_event *) obj;

	BT_LOGD("Destroying event notification: addr=%p", notification);
	BT_LOGD_STR("Putting event.");
	BT_PUT(notification->event);
	BT_LOGD_STR("Putting clock class priority map.");
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
	struct bt_ctf_clock_value *clock_value = NULL;
	struct bt_ctf_clock_class *clock_class = NULL;
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
		bt_bool found_in_trace = BT_FALSE;

		clock_class =
			bt_clock_class_priority_map_get_clock_class_by_index(
				notif->cc_prio_map, cc_prio_map_cc_i);
		assert(clock_class);
		clock_value = bt_ctf_event_get_clock_value(notif->event,
			clock_class);
		if (!clock_value) {
			BT_LOGW("Event has no clock value for a clock class which exists in the notification's clock class priority map: "
				"notif-addr=%p, event-addr=%p, "
				"event-class-addr=%p, event-class-name=\"%s\", "
				"event-class-id=%" PRId64 ", "
				"cc-prio-map-addr=%p, "
				"clock-class-addr=%p, clock-class-name=\"%s\"",
				notif, notif->event, event_class,
				bt_ctf_event_class_get_name(event_class),
				bt_ctf_event_class_get_id(event_class),
				notif->cc_prio_map, clock_class,
				bt_ctf_clock_class_get_name(clock_class));
			is_valid = BT_FALSE;
			goto end;
		}

		for (trace_cc_i = 0; trace_cc_i < trace_cc_count;
				trace_cc_i++) {
			struct bt_ctf_clock_class *trace_clock_class =
				bt_ctf_trace_get_clock_class_by_index(trace,
					trace_cc_i);

			assert(trace_clock_class);
			bt_put(trace_clock_class);

			if (trace_clock_class == clock_class) {
				found_in_trace = BT_TRUE;
				break;
			}
		}

		if (!found_in_trace) {
			BT_LOGW("A clock class found in the event notification's clock class priority map does not exist in the notification's event's trace: "
				"notif-addr=%p, trace-addr=%p, "
				"trace-name=\"%s\", cc-prio-map-addr=%p, "
				"clock-class-addr=%p, clock-class-name=\"%s\"",
				notif, trace, bt_ctf_trace_get_name(trace),
				notif->cc_prio_map, clock_class,
				bt_ctf_clock_class_get_name(clock_class));
			is_valid = BT_FALSE;
			goto end;
		}

		BT_PUT(clock_value);
		BT_PUT(clock_class);
	}

end:
	bt_put(clock_value);
	bt_put(clock_class);
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
	struct bt_ctf_event_class *event_class;

	if (!event) {
		BT_LOGW_STR("Invalid parameter: event is NULL.");
		goto error;
	}

	if (!cc_prio_map) {
		BT_LOGW_STR("Invalid parameter: clock class priority map is NULL.");
		goto error;
	}

	event_class = bt_ctf_event_borrow_event_class(event);
	assert(event_class);
	BT_LOGD("Creating event notification object: "
		"event-addr=%p, event-class-addr=%p, "
		"event-class-name=\"%s\", event-class-id=%" PRId64 ", "
		"cc-prio-map-addr=%p",
		event, event_class,
		bt_ctf_event_class_get_name(event_class),
		bt_ctf_event_class_get_id(event_class), cc_prio_map);

	if (!bt_ctf_event_borrow_packet(event)) {
		BT_LOGW("Invalid parameter: event has no packet: "
			"event-addr=%p, event-class-addr=%p, "
			"event-class-name=\"%s\", "
			"event-class-id=%" PRId64,
			event, event_class,
			bt_ctf_event_class_get_name(event_class),
			bt_ctf_event_class_get_id(event_class));
		goto error;
	}

	if (!event_has_trace(event)) {
		BT_LOGW("Invalid parameter: event has no trace: "
			"event-addr=%p, event-class-addr=%p, "
			"event-class-name=\"%s\", "
			"event-class-id=%" PRId64,
			event, event_class,
			bt_ctf_event_class_get_name(event_class),
			bt_ctf_event_class_get_id(event_class));
		goto error;
	}

	notification = g_new0(struct bt_notification_event, 1);
	if (!notification) {
		BT_LOGE_STR("Failed to allocate one event notification.");
		goto error;
	}

	bt_notification_init(&notification->parent, BT_NOTIFICATION_TYPE_EVENT,
		bt_notification_event_destroy);
	notification->event = bt_get(event);
	notification->cc_prio_map = bt_get(cc_prio_map);
	if (!validate_clock_classes(notification)) {
		BT_LOGW("Invalid event: invalid clock class: "
			"event-addr=%p, event-class-addr=%p, "
			"event-class-name=\"%s\", "
			"event-class-id=%" PRId64,
			event, event_class,
			bt_ctf_event_class_get_name(event_class),
			bt_ctf_event_class_get_id(event_class));
		goto error;
	}

	BT_LOGD_STR("Freezing event notification's event.");
	bt_ctf_event_freeze(notification->event);
	BT_LOGD_STR("Freezing event notification's clock class priority map.");
	bt_clock_class_priority_map_freeze(notification->cc_prio_map);
	BT_LOGD("Created event notification object: "
		"event-addr=%p, event-class-addr=%p, "
		"event-class-name=\"%s\", event-class-id=%" PRId64 ", "
		"cc-prio-map-addr=%p, notif-addr=%p",
		event, event_class,
		bt_ctf_event_class_get_name(event_class),
		bt_ctf_event_class_get_id(event_class), cc_prio_map,
		notification);
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

	if (!notification) {
		BT_LOGW_STR("Invalid parameter: notification is NULL.");
		goto end;
	}

	if (bt_notification_get_type(notification) !=
			BT_NOTIFICATION_TYPE_EVENT) {
		BT_LOGW("Invalid parameter: notification is not an event notification: "
			"addr%p, notif-type=%s",
			notification, bt_notification_type_string(
				bt_notification_get_type(notification)));
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

	if (!notification) {
		BT_LOGW_STR("Invalid parameter: notification is NULL.");
		goto end;
	}

	if (bt_notification_get_type(notification) !=
			BT_NOTIFICATION_TYPE_EVENT) {
		BT_LOGW("Invalid parameter: notification is not an event notification: "
			"addr%p, notif-type=%s",
			notification, bt_notification_type_string(
				bt_notification_get_type(notification)));
		goto end;
	}

	event_notification = container_of(notification,
			struct bt_notification_event, parent);
	cc_prio_map = bt_get(event_notification->cc_prio_map);

end:
	return cc_prio_map;
}
