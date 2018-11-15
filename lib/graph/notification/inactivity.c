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
#include <babeltrace/trace-ir/clock-class.h>
#include <babeltrace/trace-ir/clock-value-internal.h>
#include <babeltrace/graph/notification-internal.h>
#include <babeltrace/graph/notification-inactivity-internal.h>
#include <babeltrace/graph/private-connection-private-notification-iterator.h>
#include <babeltrace/assert-pre-internal.h>

static
void bt_notification_inactivity_destroy(struct bt_object *obj)
{
	struct bt_notification_inactivity *notification =
			(struct bt_notification_inactivity *) obj;

	BT_LOGD("Destroying inactivity notification: addr=%p", notification);

	if (notification->default_cv) {
		bt_clock_value_recycle(notification->default_cv);
	}

	g_free(notification);
}

struct bt_notification *bt_notification_inactivity_create(
		struct bt_private_connection_private_notification_iterator *notif_iter,
		struct bt_clock_class *default_clock_class)
{
	struct bt_notification_inactivity *notification;
	struct bt_notification *ret_notif = NULL;

	BT_ASSERT_PRE_NON_NULL(notif_iter, "Notification iterator");
	BT_ASSERT_PRE_NON_NULL(default_clock_class, "Default clock class");
	BT_LOGD_STR("Creating inactivity notification object.");
	notification = g_new0(struct bt_notification_inactivity, 1);
	if (!notification) {
		BT_LOGE_STR("Failed to allocate one inactivity notification.");
		goto error;
	}
	bt_notification_init(&notification->parent,
		BT_NOTIFICATION_TYPE_INACTIVITY,
		bt_notification_inactivity_destroy, NULL);
	ret_notif = &notification->parent;
	notification->default_cv = bt_clock_value_create(default_clock_class);
	if (!notification->default_cv) {
		goto error;
	}

	BT_LOGD("Created inactivity notification object: addr=%p",
		ret_notif);
	goto end;

error:
	BT_PUT(ret_notif);

end:
	return ret_notif;
}

int bt_notification_inactivity_set_default_clock_value(
		struct bt_notification *notif, uint64_t value_cycles)
{
	struct bt_notification_inactivity *inactivity = (void *) notif;

	BT_ASSERT_PRE_NON_NULL(notif, "Notification");
	BT_ASSERT_PRE_NOTIF_IS_TYPE(notif, BT_NOTIFICATION_TYPE_INACTIVITY);
	BT_ASSERT_PRE_HOT(notif, "Notification", ": %!+n", notif);
	bt_clock_value_set_value_inline(inactivity->default_cv, value_cycles);
	BT_LIB_LOGV("Set inactivity notification's default clock value: "
		"%![notif-]+n, value=%" PRIu64, notif, value_cycles);
	return 0;
}

struct bt_clock_value *bt_notification_inactivity_borrow_default_clock_value(
		struct bt_notification *notif)
{
	struct bt_notification_inactivity *inactivity = (void *) notif;

	BT_ASSERT_PRE_NON_NULL(notif, "Notification");
	BT_ASSERT_PRE_NOTIF_IS_TYPE(notif, BT_NOTIFICATION_TYPE_INACTIVITY);
	return inactivity->default_cv;
}
