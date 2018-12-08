#ifndef BABELTRACE_GRAPH_MESSAGE_MESSAGE_INTERNAL_H
#define BABELTRACE_GRAPH_MESSAGE_MESSAGE_INTERNAL_H

/*
 * Copyright 2017-2018 Philippe Proulx <pproulx@efficios.com>
 * Copyright 2015 Jérémie Galarneau <jeremie.galarneau@efficios.com>
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
#include <babeltrace/graph/message-const.h>
#include <babeltrace/trace-ir/stream.h>
#include <babeltrace/object-pool-internal.h>
#include <babeltrace/types.h>

typedef struct bt_stream *(*get_stream_func)(
		struct bt_message *message);

struct bt_message {
	struct bt_object base;
	enum bt_message_type type;
	uint64_t seq_num;
	bt_bool frozen;

	/* Owned by this; keeps the graph alive while the msg. is alive */
	struct bt_graph *graph;
};

#define BT_ASSERT_PRE_MSG_IS_TYPE(_msg, _type)			\
	BT_ASSERT_PRE(((struct bt_message *) (_msg))->type == (_type), \
		"Message has the wrong type: expected-type=%s, "	\
		"%![msg-]+n", bt_message_type_string(_type),	\
		(_msg))

BT_HIDDEN
void bt_message_init(struct bt_message *message,
		enum bt_message_type type,
		bt_object_release_func release,
		struct bt_graph *graph);

static inline
void bt_message_reset(struct bt_message *message)
{
	BT_ASSERT(message);

#ifdef BT_DEV_MODE
	message->frozen = BT_FALSE;
	message->seq_num = UINT64_C(-1);
#endif
}

static inline
struct bt_message *bt_message_create_from_pool(
		struct bt_object_pool *pool, struct bt_graph *graph)
{
	struct bt_message *msg = bt_object_pool_create_object(pool);

	if (unlikely(!msg)) {
#ifdef BT_LIB_LOGE
		BT_LIB_LOGE("Cannot allocate one message from message pool: "
			"%![pool-]+o, %![graph-]+g", pool, graph);
#endif
		goto error;
	}

	if (likely(!msg->graph)) {
		msg->graph = graph;
	}

	goto end;

error:
	BT_ASSERT(!msg);

end:
	return msg;
}

static inline void _bt_message_freeze(struct bt_message *message)
{
	message->frozen = BT_TRUE;
}

BT_HIDDEN
void bt_message_unlink_graph(struct bt_message *msg);

#ifdef BT_DEV_MODE
# define bt_message_freeze		_bt_message_freeze
#else
# define bt_message_freeze(_x)
#endif /* BT_DEV_MODE */

static inline
const char *bt_message_type_string(enum bt_message_type type)
{
	switch (type) {
	case BT_MESSAGE_TYPE_EVENT:
		return "BT_MESSAGE_TYPE_EVENT";
	case BT_MESSAGE_TYPE_INACTIVITY:
		return "BT_MESSAGE_TYPE_INACTIVITY";
	case BT_MESSAGE_TYPE_STREAM_BEGINNING:
		return "BT_MESSAGE_TYPE_STREAM_BEGINNING";
	case BT_MESSAGE_TYPE_STREAM_END:
		return "BT_MESSAGE_TYPE_STREAM_END";
	case BT_MESSAGE_TYPE_PACKET_BEGINNING:
		return "BT_MESSAGE_TYPE_PACKET_BEGINNING";
	case BT_MESSAGE_TYPE_PACKET_END:
		return "BT_MESSAGE_TYPE_PACKET_END";
	default:
		return "(unknown)";
	}
}

#endif /* BABELTRACE_GRAPH_MESSAGE_MESSAGE_INTERNAL_H */
