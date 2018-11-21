/*
 * Babeltrace Plug-in Notification
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

#define BT_LOG_TAG "NOTIF"
#include <babeltrace/lib-logging-internal.h>

#include <babeltrace/graph/private-notification.h>
#include <babeltrace/graph/notification-internal.h>
#include <babeltrace/graph/graph-internal.h>
#include <babeltrace/assert-internal.h>
#include <babeltrace/assert-pre-internal.h>

BT_ASSERT_PRE_FUNC
static inline void _init_seq_num(struct bt_notification *notification)
{
	notification->seq_num = UINT64_C(-1);
}

#ifdef BT_DEV_MODE
# define init_seq_num	_init_seq_num
#else
# define init_seq_num(_notif)
#endif /* BT_DEV_MODE */

BT_HIDDEN
void bt_notification_init(struct bt_notification *notification,
		enum bt_notification_type type,
		bt_object_release_func release,
		struct bt_graph *graph)
{
	BT_ASSERT(type >= 0 && type < BT_NOTIFICATION_TYPE_NR);
	notification->type = type;
	init_seq_num(notification);
	bt_object_init_shared(&notification->base, release);
	notification->graph = graph;

	if (graph) {
		bt_graph_add_notification(graph, notification);
	}
}

enum bt_notification_type bt_notification_get_type(
		struct bt_notification *notification)
{
	BT_ASSERT_PRE_NON_NULL(notification, "Notification");
	return notification->type;
}

BT_HIDDEN
void bt_notification_unlink_graph(struct bt_notification *notif)
{
	BT_ASSERT(notif);
	notif->graph = NULL;
}

struct bt_notification *bt_notification_borrow_from_private(
		struct bt_private_notification *priv_notif)
{
	return (void *) priv_notif;
}
