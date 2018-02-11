/*
 * Copyright 2017 Philippe Proulx <pproulx@efficios.com>
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

#define BT_LOG_TAG "NOTIF-INACTIVITY"
#include <babeltrace/lib-logging-internal.h>

#include <babeltrace/object-internal.h>
#include <babeltrace/compiler-internal.h>
#include <babeltrace/ctf-ir/clock-class.h>
#include <babeltrace/graph/clock-class-priority-map.h>
#include <babeltrace/graph/clock-class-priority-map-internal.h>
#include <babeltrace/graph/notification-internal.h>
#include <babeltrace/graph/notification-inactivity-internal.h>

static
void bt_notification_inactivity_destroy(struct bt_object *obj)
{
	struct bt_notification_inactivity *notification =
			(struct bt_notification_inactivity *) obj;

	BT_LOGD("Destroying inactivity notification: addr=%p", notification);
	BT_LOGD_STR("Putting clock class priority map.");
	bt_put(notification->cc_prio_map);

	if (notification->clock_values) {
		BT_LOGD_STR("Putting clock values.");
		g_hash_table_destroy(notification->clock_values);
	}

	g_free(notification);
}

struct bt_private_notification *bt_private_notification_inactivity_create(
		struct bt_clock_class_priority_map *cc_prio_map)
{
	struct bt_notification_inactivity *notification;
	struct bt_notification *ret_notif = NULL;

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

	BT_LOGD("Creating inactivity notification object: "
		"cc-prio-map-addr=%p",
		cc_prio_map);
	notification = g_new0(struct bt_notification_inactivity, 1);
	if (!notification) {
		BT_LOGE_STR("Failed to allocate one inactivity notification.");
		goto error;
	}
	bt_notification_init(&notification->parent,
		BT_NOTIFICATION_TYPE_INACTIVITY,
		bt_notification_inactivity_destroy);
	ret_notif = &notification->parent;
	notification->clock_values = g_hash_table_new_full(g_direct_hash,
		g_direct_equal, bt_put, bt_put);
	if (!notification->clock_values) {
		BT_LOGE_STR("Failed to allocate a GHashTable.");
		goto error;
	}

	notification->cc_prio_map = bt_get(cc_prio_map);
	BT_LOGD_STR("Freezing inactivity notification's clock class priority map.");
	bt_clock_class_priority_map_freeze(cc_prio_map);
	BT_LOGD("Created inactivity notification object: "
		"cc-prio-map-addr=%p, notif-addr=%p",
		cc_prio_map, ret_notif);
	goto end;

error:
	BT_PUT(ret_notif);

end:
	bt_put(cc_prio_map);
	return bt_private_notification_from_notification(ret_notif);
}

extern struct bt_clock_class_priority_map *
bt_notification_inactivity_get_clock_class_priority_map(
		struct bt_notification *notification)
{
	struct bt_clock_class_priority_map *cc_prio_map = NULL;
	struct bt_notification_inactivity *inactivity_notification;

	if (!notification) {
		BT_LOGW_STR("Invalid parameter: notification is NULL.");
		goto end;
	}

	if (bt_notification_get_type(notification) !=
			BT_NOTIFICATION_TYPE_INACTIVITY) {
		BT_LOGW("Invalid parameter: notification is not an inactivity notification: "
			"addr%p, notif-type=%s",
			notification, bt_notification_type_string(
				bt_notification_get_type(notification)));
		goto end;
	}

	inactivity_notification = container_of(notification,
			struct bt_notification_inactivity, parent);
	cc_prio_map = bt_get(inactivity_notification->cc_prio_map);
end:
	return cc_prio_map;
}

struct bt_clock_value *bt_notification_inactivity_get_clock_value(
		struct bt_notification *notification,
		struct bt_clock_class *clock_class)
{
	struct bt_clock_value *clock_value = NULL;
	struct bt_notification_inactivity *inactivity_notification;

	if (!notification) {
		BT_LOGW_STR("Invalid parameter: notification is NULL.");
		goto end;
	}

	if (!clock_class) {
		BT_LOGW_STR("Invalid parameter: clock class is NULL.");
		goto end;
	}

	if (bt_notification_get_type(notification) !=
			BT_NOTIFICATION_TYPE_INACTIVITY) {
		BT_LOGW("Invalid parameter: notification is not an inactivity notification: "
			"addr%p, notif-type=%s",
			notification, bt_notification_type_string(
				bt_notification_get_type(notification)));
		goto end;
	}

	inactivity_notification = container_of(notification,
		struct bt_notification_inactivity, parent);
	clock_value = g_hash_table_lookup(inactivity_notification->clock_values,
		clock_class);
	bt_get(clock_value);

end:
	return clock_value;
}

int bt_private_notification_inactivity_set_clock_value(
		struct bt_private_notification *priv_notif,
		struct bt_clock_value *clock_value)
{
	int ret = 0;
	uint64_t prio;
	struct bt_clock_class *clock_class = NULL;
	struct bt_notification_inactivity *inactivity_notification;
	struct bt_notification *notification =
		bt_notification_borrow_from_private(priv_notif);

	if (!notification) {
		BT_LOGW_STR("Invalid parameter: notification is NULL.");
		ret = -1;
		goto end;
	}

	if (!clock_value) {
		BT_LOGW_STR("Invalid parameter: clock value is NULL.");
		ret = -1;
		goto end;
	}

	if (notification->frozen) {
		BT_LOGW_STR("Invalid parameter: notification is frozen.");
		ret = -1;
		goto end;
	}

	if (bt_notification_get_type(notification) !=
			BT_NOTIFICATION_TYPE_INACTIVITY) {
		BT_LOGW("Invalid parameter: notification is not an inactivity notification: "
			"addr%p, notif-type=%s",
			notification, bt_notification_type_string(
				bt_notification_get_type(notification)));
		ret = -1;
		goto end;
	}

	inactivity_notification = container_of(notification,
			struct bt_notification_inactivity, parent);
	clock_class = bt_clock_value_get_class(clock_value);
	ret = bt_clock_class_priority_map_get_clock_class_priority(
		inactivity_notification->cc_prio_map, clock_class, &prio);
	if (ret) {
		BT_LOGW("Clock value's class is not mapped to a priority within the scope of the inactivity notification: "
			"notif-addr=%p, cc-prio-map-addr=%p, "
			"clock-class-addr=%p, clock-class-name=\"%s\", "
			"clock-value-addr=%p",
			inactivity_notification,
			inactivity_notification->cc_prio_map,
			clock_class, bt_clock_class_get_name(clock_class),
			clock_value);
		ret = -1;
		goto end;
	}

	g_hash_table_insert(inactivity_notification->clock_values,
		clock_class, bt_get(clock_value));
	clock_class = NULL;
	BT_LOGV("Set inactivity notification's clock value: "
		"notif-addr=%p, cc-prio-map-addr=%p, "
		"clock-class-addr=%p, clock-class-name=\"%s\", "
		"clock-value-addr=%p",
		inactivity_notification,
		inactivity_notification->cc_prio_map,
		clock_class, bt_clock_class_get_name(clock_class),
		clock_value);

end:
	bt_put(clock_class);
	return ret;
}
