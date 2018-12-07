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

#define BT_LOG_TAG "NOTIF-STREAM"
#include <babeltrace/lib-logging-internal.h>

#include <babeltrace/assert-pre-internal.h>
#include <babeltrace/compiler-internal.h>
#include <babeltrace/trace-ir/stream-internal.h>
#include <babeltrace/trace-ir/stream-class.h>
#include <babeltrace/trace-ir/stream-class-internal.h>
#include <babeltrace/graph/notification-stream.h>
#include <babeltrace/graph/notification-stream-const.h>
#include <babeltrace/graph/notification-stream-internal.h>
#include <babeltrace/assert-internal.h>
#include <inttypes.h>

static
void bt_notification_stream_end_destroy(struct bt_object *obj)
{
	struct bt_notification_stream_end *notification =
			(struct bt_notification_stream_end *) obj;

	BT_LIB_LOGD("Destroying stream end notification: %!+n",
		notification);
	BT_LIB_LOGD("Putting stream: %!+s", notification->stream);
	BT_OBJECT_PUT_REF_AND_RESET(notification->stream);

	if (notification->default_cv) {
		bt_clock_value_recycle(notification->default_cv);
		notification->default_cv = NULL;
	}

	g_free(notification);
}

struct bt_notification *bt_notification_stream_end_create(
		struct bt_self_notification_iterator *self_notif_iter,
		struct bt_stream *stream)
{
	struct bt_notification_stream_end *notification;
	struct bt_stream_class *stream_class;

	BT_ASSERT_PRE_NON_NULL(self_notif_iter, "Notification iterator");
	BT_ASSERT_PRE_NON_NULL(stream, "Stream");
	stream_class = bt_stream_borrow_class(stream);
	BT_ASSERT(stream_class);
	BT_LIB_LOGD("Creating stream end notification object: "
		"%![stream-]+s, %![sc-]+S", stream, stream_class);
	notification = g_new0(struct bt_notification_stream_end, 1);
	if (!notification) {
		BT_LOGE_STR("Failed to allocate one stream end notification.");
		goto error;
	}

	bt_notification_init(&notification->parent,
			BT_NOTIFICATION_TYPE_STREAM_END,
			bt_notification_stream_end_destroy, NULL);
	notification->stream = stream;
	bt_object_get_no_null_check(notification->stream);
	BT_LIB_LOGD("Created stream end notification object: "
		"%![notif-]+n, %![stream-]+s, %![sc-]+S", notification,
		stream, stream_class);

	return (void *) &notification->parent;
error:
	return NULL;
}

struct bt_stream *bt_notification_stream_end_borrow_stream(
		struct bt_notification *notification)
{
	struct bt_notification_stream_end *stream_end;

	BT_ASSERT_PRE_NON_NULL(notification, "Notification");
	BT_ASSERT_PRE_NOTIF_IS_TYPE(notification,
		BT_NOTIFICATION_TYPE_STREAM_END);
	stream_end = (void *) notification;
	return stream_end->stream;
}

const struct bt_stream *bt_notification_stream_end_borrow_stream_const(
		const struct bt_notification *notification)
{
	return bt_notification_stream_end_borrow_stream(
		(void *) notification);
}

void bt_notification_stream_end_set_default_clock_value(
		struct bt_notification *notif, uint64_t value_cycles)
{
	struct bt_notification_stream_end *se_notif = (void *) notif;

	BT_ASSERT_PRE_NON_NULL(notif, "Notification");
	BT_ASSERT_PRE_HOT(notif, "Notification", ": %!+n", notif);
	BT_ASSERT_PRE_NOTIF_IS_TYPE(notif, BT_NOTIFICATION_TYPE_STREAM_END);
	BT_ASSERT_PRE(se_notif->stream->class->default_clock_class,
		"Notification's stream class has no default clock class: "
		"%![notif-]+n, %![sc-]+S", notif, se_notif->stream->class);

	/* TODO: have the object already created */
	se_notif->default_cv = bt_clock_value_create(
		se_notif->stream->class->default_clock_class);
	BT_ASSERT(se_notif->default_cv);
	bt_clock_value_set_value_inline(se_notif->default_cv, value_cycles);
	BT_LIB_LOGV("Set notification's default clock value: %![notif-]+n, "
		"value=%" PRIu64, value_cycles);
}

struct bt_clock_value *bt_notification_stream_end_borrow_default_clock_value(
		struct bt_notification *notif)
{
	struct bt_notification_stream_end *stream_end = (void *) notif;

	BT_ASSERT_PRE_NON_NULL(notif, "Notification");
	BT_ASSERT_PRE_NOTIF_IS_TYPE(notif, BT_NOTIFICATION_TYPE_STREAM_END);
	return stream_end->default_cv;
}

static
void bt_notification_stream_beginning_destroy(struct bt_object *obj)
{
	struct bt_notification_stream_beginning *notification =
			(struct bt_notification_stream_beginning *) obj;

	BT_LIB_LOGD("Destroying stream beginning notification: %!+n",
		notification);
	BT_LIB_LOGD("Putting stream: %!+s", notification->stream);
	BT_OBJECT_PUT_REF_AND_RESET(notification->stream);

	if (notification->default_cv) {
		bt_clock_value_recycle(notification->default_cv);
		notification->default_cv = NULL;
	}

	g_free(notification);
}

struct bt_notification *bt_notification_stream_beginning_create(
		struct bt_self_notification_iterator *self_notif_iter,
		struct bt_stream *stream)
{
	struct bt_notification_stream_beginning *notification;
	struct bt_stream_class *stream_class;

	BT_ASSERT_PRE_NON_NULL(self_notif_iter, "Notification iterator");
	BT_ASSERT_PRE_NON_NULL(stream, "Stream");
	stream_class = bt_stream_borrow_class(stream);
	BT_ASSERT(stream_class);
	BT_LIB_LOGD("Creating stream beginning notification object: "
		"%![stream-]+s, %![sc-]+S", stream, stream_class);
	notification = g_new0(struct bt_notification_stream_beginning, 1);
	if (!notification) {
		BT_LOGE_STR("Failed to allocate one stream beginning notification.");
		goto error;
	}

	bt_notification_init(&notification->parent,
			BT_NOTIFICATION_TYPE_STREAM_BEGINNING,
			bt_notification_stream_beginning_destroy, NULL);
	notification->stream = stream;
	bt_object_get_no_null_check(notification->stream);
	BT_LIB_LOGD("Created stream beginning notification object: "
		"%![notif-]+n, %![stream-]+s, %![sc-]+S", notification,
		stream, stream_class);
	return (void *) &notification->parent;
error:
	return NULL;
}

struct bt_stream *bt_notification_stream_beginning_borrow_stream(
		struct bt_notification *notification)
{
	struct bt_notification_stream_beginning *stream_begin;

	BT_ASSERT_PRE_NON_NULL(notification, "Notification");
	BT_ASSERT_PRE_NOTIF_IS_TYPE(notification,
		BT_NOTIFICATION_TYPE_STREAM_BEGINNING);
	stream_begin = (void *) notification;
	return stream_begin->stream;
}

const struct bt_stream *bt_notification_stream_beginning_borrow_stream_const(
		const struct bt_notification *notification)
{
	return bt_notification_stream_beginning_borrow_stream(
		(void *) notification);
}

void bt_notification_stream_beginning_set_default_clock_value(
		struct bt_notification *notif,
		uint64_t value_cycles)
{
	struct bt_notification_stream_beginning *sb_notif = (void *) notif;

	BT_ASSERT_PRE_NON_NULL(notif, "Notification");
	BT_ASSERT_PRE_HOT(notif, "Notification", ": %!+n", notif);
	BT_ASSERT_PRE_NOTIF_IS_TYPE(notif, BT_NOTIFICATION_TYPE_STREAM_BEGINNING);
	BT_ASSERT_PRE(sb_notif->stream->class->default_clock_class,
		"Notification's stream class has no default clock class: "
		"%![notif-]+n, %![sc-]+S", notif, sb_notif->stream->class);

	/* TODO: have the object already created */
	sb_notif->default_cv = bt_clock_value_create(
		sb_notif->stream->class->default_clock_class);
	BT_ASSERT(sb_notif->default_cv);
	bt_clock_value_set_value_inline(sb_notif->default_cv, value_cycles);
	BT_LIB_LOGV("Set notification's default clock value: %![notif-]+n, "
		"value=%" PRIu64, value_cycles);
}

struct bt_clock_value *bt_notification_stream_beginning_borrow_default_clock_value(
		struct bt_notification *notif)
{
	struct bt_notification_stream_beginning *stream_begin = (void *) notif;

	BT_ASSERT_PRE_NON_NULL(notif, "Notification");
	BT_ASSERT_PRE_NOTIF_IS_TYPE(notif, BT_NOTIFICATION_TYPE_STREAM_BEGINNING);
	return stream_begin->default_cv;
}
