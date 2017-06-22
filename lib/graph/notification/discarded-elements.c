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

#define BT_LOG_TAG "NOTIF-DISCARDED"
#include <babeltrace/lib-logging-internal.h>

#include <babeltrace/object-internal.h>
#include <babeltrace/compiler-internal.h>
#include <babeltrace/ctf-ir/clock-class.h>
#include <babeltrace/graph/clock-class-priority-map.h>
#include <babeltrace/graph/clock-class-priority-map-internal.h>
#include <babeltrace/graph/notification-internal.h>
#include <babeltrace/graph/notification-discarded-elements-internal.h>
#include <stdint.h>
#include <inttypes.h>

static
void bt_notification_discarded_elements_destroy(struct bt_object *obj)
{
	struct bt_notification_discarded_elements *notification =
		(struct bt_notification_discarded_elements *) obj;

	BT_LOGD("Destroying discarded elements notification: addr=%p",
		notification);
	BT_LOGD_STR("Putting stream.");
	bt_put(notification->stream);
	BT_LOGD_STR("Putting beginning clock value.");
	bt_put(notification->begin_clock_value);
	BT_LOGD_STR("Putting end clock value.");
	bt_put(notification->end_clock_value);
	g_free(notification);
}

BT_HIDDEN
struct bt_notification *bt_notification_discarded_elements_create(
		enum bt_notification_type type,
		struct bt_ctf_stream *stream,
		struct bt_ctf_clock_value *begin_clock_value,
		struct bt_ctf_clock_value *end_clock_value,
		uint64_t count)
{
	struct bt_notification_discarded_elements *notification;
	struct bt_notification *ret_notif = NULL;

	if (!stream) {
		BT_LOGW_STR("Invalid parameter: stream is NULL.");
	}

	BT_LOGD("Creating discarded elements notification object: "
		"type=%s, stream-addr=%p, stream-name=\"%s\", "
		"begin-clock-value-addr=%p, end-clock-value-addr=%p, "
		"count=%" PRIu64,
		bt_notification_type_string(type), stream,
		bt_ctf_stream_get_name(stream), begin_clock_value,
		end_clock_value, count);
	notification = g_new0(struct bt_notification_discarded_elements, 1);
	if (!notification) {
		BT_LOGE_STR("Failed to allocate one discarded elements notification.");
		goto error;
	}

	bt_notification_init(&notification->parent, type,
		bt_notification_discarded_elements_destroy);
	ret_notif = &notification->parent;
	notification->stream = bt_get(stream);
	notification->begin_clock_value = bt_get(begin_clock_value);
	notification->end_clock_value = bt_get(end_clock_value);
	notification->count = (int64_t) count;
	BT_LOGD("Created discarded elements notification object: "
		"type=%s, stream-addr=%p, stream-name=\"%s\", "
		"begin-clock-value-addr=%p, end-clock-value-addr=%p, "
		"count=%" PRIu64 ", addr=%p",
		bt_notification_type_string(type), stream,
		bt_ctf_stream_get_name(stream), begin_clock_value,
		end_clock_value, count, ret_notif);
	goto end;

error:
	BT_PUT(ret_notif);

end:
	return ret_notif;
}

BT_HIDDEN
struct bt_ctf_clock_value *
bt_notification_discarded_elements_get_begin_clock_value(
		enum bt_notification_type type,
		struct bt_notification *notification)
{
	struct bt_ctf_clock_value *clock_value = NULL;
	struct bt_notification_discarded_elements *discarded_elems_notif;

	if (!notification) {
		BT_LOGW_STR("Invalid parameter: notification is NULL.");
		goto end;
	}

	if (bt_notification_get_type(notification) !=
			BT_NOTIFICATION_TYPE_DISCARDED_EVENTS) {
		BT_LOGW("Invalid parameter: notification has not the expected type: "
			"addr%p, expected-type=%s, notif-type=%s",
			notification, bt_notification_type_string(type),
			bt_notification_type_string(
				bt_notification_get_type(notification)));
		goto end;
	}

	discarded_elems_notif = container_of(notification,
			struct bt_notification_discarded_elements, parent);
	clock_value = bt_get(discarded_elems_notif->begin_clock_value);

end:
	return clock_value;
}

BT_HIDDEN
struct bt_ctf_clock_value *
bt_notification_discarded_elements_get_end_clock_value(
		enum bt_notification_type type,
		struct bt_notification *notification)
{
	struct bt_ctf_clock_value *clock_value = NULL;
	struct bt_notification_discarded_elements *discarded_elems_notif;

	if (!notification) {
		BT_LOGW_STR("Invalid parameter: notification is NULL.");
		goto end;
	}

	if (bt_notification_get_type(notification) !=
			BT_NOTIFICATION_TYPE_DISCARDED_EVENTS) {
		BT_LOGW("Invalid parameter: notification has not the expected type: "
			"addr%p, expected-type=%s, notif-type=%s",
			notification, bt_notification_type_string(type),
			bt_notification_type_string(
				bt_notification_get_type(notification)));
		goto end;
	}

	discarded_elems_notif = container_of(notification,
			struct bt_notification_discarded_elements, parent);
	clock_value = bt_get(discarded_elems_notif->end_clock_value);

end:
	return clock_value;
}

BT_HIDDEN
int64_t bt_notification_discarded_elements_get_count(
		enum bt_notification_type type,
		struct bt_notification *notification)
{
	int64_t count = (int64_t) -1;
	struct bt_notification_discarded_elements *discarded_elems_notif;

	if (!notification) {
		BT_LOGW_STR("Invalid parameter: notification is NULL.");
		goto end;
	}

	if (bt_notification_get_type(notification) !=
			BT_NOTIFICATION_TYPE_DISCARDED_EVENTS) {
		BT_LOGW("Invalid parameter: notification has not the expected type: "
			"addr%p, expected-type=%s, notif-type=%s",
			notification, bt_notification_type_string(type),
			bt_notification_type_string(
				bt_notification_get_type(notification)));
		goto end;
	}

	discarded_elems_notif = container_of(notification,
			struct bt_notification_discarded_elements, parent);
	count = discarded_elems_notif->count;

end:
	return count;
}

BT_HIDDEN
struct bt_ctf_stream *bt_notification_discarded_elements_get_stream(
		enum bt_notification_type type,
		struct bt_notification *notification)
{
	struct bt_ctf_stream *stream = NULL;
	struct bt_notification_discarded_elements *discarded_elems_notif;

	if (!notification) {
		BT_LOGW_STR("Invalid parameter: notification is NULL.");
		goto end;
	}

	if (bt_notification_get_type(notification) !=
			BT_NOTIFICATION_TYPE_DISCARDED_EVENTS) {
		BT_LOGW("Invalid parameter: notification has not the expected type: "
			"addr%p, expected-type=%s, notif-type=%s",
			notification, bt_notification_type_string(type),
			bt_notification_type_string(
				bt_notification_get_type(notification)));
		goto end;
	}

	discarded_elems_notif = container_of(notification,
			struct bt_notification_discarded_elements, parent);
	stream = bt_get(discarded_elems_notif->stream);

end:
	return stream;
}
