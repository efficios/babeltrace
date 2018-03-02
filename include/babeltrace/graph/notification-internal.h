#ifndef BABELTRACE_COMPONENT_NOTIFICATION_NOTIFICATION_INTERNAL_H
#define BABELTRACE_COMPONENT_NOTIFICATION_NOTIFICATION_INTERNAL_H

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

#include <babeltrace/ref-internal.h>
#include <babeltrace/babeltrace-internal.h>
#include <babeltrace/object-internal.h>
#include <babeltrace/graph/notification.h>
#include <babeltrace/ctf-ir/stream.h>
#include <babeltrace/types.h>

typedef struct bt_stream *(*get_stream_func)(
		struct bt_notification *notification);

struct bt_notification {
	struct bt_object base;
	enum bt_notification_type type;
	get_stream_func get_stream;
	uint64_t seq_num;
	bt_bool frozen;
};

#define BT_ASSERT_PRE_NOTIF_IS_TYPE(_notif, _type)			\
	BT_ASSERT_PRE((_notif)->type == (_type),			\
		"Notification has the wrong type: expected-type=%s, "	\
		"%![notif-]+n", bt_notification_type_string(_type),	\
		(_notif))

BT_HIDDEN
void bt_notification_init(struct bt_notification *notification,
		enum bt_notification_type type,
		bt_object_release_func release);

static inline void bt_notification_freeze(struct bt_notification *notification)
{
	notification->frozen = BT_TRUE;
}

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
	case BT_NOTIFICATION_TYPE_DISCARDED_EVENTS:
		return "BT_NOTIFICATION_TYPE_DISCARDED_EVENTS";
	case BT_NOTIFICATION_TYPE_DISCARDED_PACKETS:
		return "BT_NOTIFICATION_TYPE_DISCARDED_PACKETS";
	default:
		return "(unknown)";
	}
}

#endif /* BABELTRACE_COMPONENT_NOTIFICATION_NOTIFICATION_INTERNAL_H */
