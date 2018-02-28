#ifndef BABELTRACE_COMPONENT_NOTIFICATION_EVENT_INTERNAL_H
#define BABELTRACE_COMPONENT_NOTIFICATION_EVENT_INTERNAL_H

/*
 * BabelTrace - Plug-in Event Notification internal
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
#include <babeltrace/graph/notification-internal.h>
#include <babeltrace/graph/clock-class-priority-map.h>
#include <babeltrace/assert-internal.h>

#ifdef __cplusplus
extern "C" {
#endif

struct bt_notification_event {
	struct bt_notification parent;
	struct bt_event *event;
	struct bt_clock_class_priority_map *cc_prio_map;
};

static inline
struct bt_event *bt_notification_event_borrow_event(
		struct bt_notification *notif)
{
	struct bt_notification_event *notif_event = container_of(notif,
			struct bt_notification_event, parent);

	BT_ASSERT(notif_event);
	return notif_event->event;
}

static inline
struct bt_clock_class_priority_map *
bt_notification_event_borrow_clock_class_priority_map(
		struct bt_notification *notif)
{
	struct bt_notification_event *notif_event = container_of(notif,
			struct bt_notification_event, parent);

	BT_ASSERT(notif_event);
	return notif_event->cc_prio_map;
}


#ifdef __cplusplus
}
#endif

#endif /* BABELTRACECOMPONENTPLUGIN_NOTIFICATION_EVENT_INTERNAL_H */
