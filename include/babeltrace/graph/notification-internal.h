#ifndef BABELTRACE_GRAPH_NOTIFICATION_NOTIFICATION_INTERNAL_H
#define BABELTRACE_GRAPH_NOTIFICATION_NOTIFICATION_INTERNAL_H

/*
 * BabelTrace - Plug-in Notification internal
 *
 * Copyright 2015 Jérémie Galarneau <jeremie.galarneau@efficios.com>
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

#include <babeltrace/babeltrace-internal.h>
#include <babeltrace/object-internal.h>
#include <babeltrace/assert-internal.h>
#include <babeltrace/graph/graph.h>
#include <babeltrace/graph/notification.h>
#include <babeltrace/trace-ir/stream.h>
#include <babeltrace/object-pool-internal.h>
#include <babeltrace/types.h>

typedef struct bt_stream *(*get_stream_func)(
		struct bt_notification *notification);

struct bt_notification {
	struct bt_object base;
	enum bt_notification_type type;
	uint64_t seq_num;
	bt_bool frozen;

	/* Owned by this; keeps the graph alive while the notif. is alive */
	struct bt_graph *graph;
};

#define BT_ASSERT_PRE_NOTIF_IS_TYPE(_notif, _type)			\
	BT_ASSERT_PRE((_notif)->type == (_type),			\
		"Notification has the wrong type: expected-type=%s, "	\
		"%![notif-]+n", bt_notification_type_string(_type),	\
		(_notif))

BT_HIDDEN
void bt_notification_init(struct bt_notification *notification,
		enum bt_notification_type type,
		bt_object_release_func release,
		struct bt_graph *graph);

static inline
void bt_notification_reset(struct bt_notification *notification)
{
	BT_ASSERT(notification);

#ifdef BT_DEV_MODE
	notification->frozen = BT_FALSE;
	notification->seq_num = UINT64_C(-1);
#endif
}

static inline
struct bt_notification *bt_notification_create_from_pool(
		struct bt_object_pool *pool, struct bt_graph *graph)
{
	struct bt_notification *notif = bt_object_pool_create_object(pool);

	if (unlikely(!notif)) {
#ifdef BT_LIB_LOGE
		BT_LIB_LOGE("Cannot allocate one notification from notification pool: "
			"%![pool-]+o, %![graph-]+g", pool, graph);
#endif
		goto error;
	}

	if (likely(!notif->graph)) {
		notif->graph = graph;
	}

	goto end;

error:
	BT_ASSERT(!notif);

end:
	return notif;
}

static inline void _bt_notification_freeze(struct bt_notification *notification)
{
	notification->frozen = BT_TRUE;
}

BT_HIDDEN
void bt_notification_unlink_graph(struct bt_notification *notif);

#ifdef BT_DEV_MODE
# define bt_notification_freeze		_bt_notification_freeze
#else
# define bt_notification_freeze(_x)
#endif /* BT_DEV_MODE */

static inline
const char *bt_notification_type_string(enum bt_notification_type type)
{
	switch (type) {
	case BT_NOTIFICATION_TYPE_UNKNOWN:
		return "BT_NOTIFICATION_TYPE_UNKNOWN";
	case BT_NOTIFICATION_TYPE_EVENT:
		return "BT_NOTIFICATION_TYPE_EVENT";
	case BT_NOTIFICATION_TYPE_INACTIVITY:
		return "BT_NOTIFICATION_TYPE_INACTIVITY";
	case BT_NOTIFICATION_TYPE_STREAM_BEGIN:
		return "BT_NOTIFICATION_TYPE_STREAM_BEGIN";
	case BT_NOTIFICATION_TYPE_STREAM_END:
		return "BT_NOTIFICATION_TYPE_STREAM_END";
	case BT_NOTIFICATION_TYPE_PACKET_BEGIN:
		return "BT_NOTIFICATION_TYPE_PACKET_BEGIN";
	case BT_NOTIFICATION_TYPE_PACKET_END:
		return "BT_NOTIFICATION_TYPE_PACKET_END";
	default:
		return "(unknown)";
	}
}

#endif /* BABELTRACE_GRAPH_NOTIFICATION_NOTIFICATION_INTERNAL_H */
