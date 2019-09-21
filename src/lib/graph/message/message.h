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

/* Protection: this file uses BT_LIB_LOG*() macros directly */
#ifndef BT_LIB_LOG_SUPPORTED
# error Please include "lib/logging.h" before including this file.
#endif

#include "common/macros.h"
#include "lib/object.h"
#include "common/assert.h"
#include <babeltrace2/graph/graph.h>
#include <babeltrace2/graph/message.h>
#include <babeltrace2/trace-ir/stream.h>
#include "lib/object-pool.h"
#include <babeltrace2/types.h>

/* Protection: this file uses BT_LIB_LOG*() macros directly */
#ifndef BT_LIB_LOG_SUPPORTED
# error Please include "lib/logging.h" before including this file.
#endif

typedef struct bt_stream *(*get_stream_func)(
		struct bt_message *message);

struct bt_message {
	struct bt_object base;
	enum bt_message_type type;
	bt_bool frozen;

	/* Owned by this; keeps the graph alive while the msg. is alive */
	struct bt_graph *graph;
};

#define _BT_ASSERT_PRE_MSG_IS_TYPE_COND(_msg, _type)			\
	(((struct bt_message *) (_msg))->type == (_type))

#define _BT_ASSERT_PRE_MSG_IS_TYPE_FMT					\
	"Message has the wrong type: expected-type=%s, %![msg-]+n"

#define BT_ASSERT_PRE_MSG_IS_TYPE(_msg, _type)				\
	BT_ASSERT_PRE(							\
		_BT_ASSERT_PRE_MSG_IS_TYPE_COND((_msg), (_type)),	\
		_BT_ASSERT_PRE_MSG_IS_TYPE_FMT,				\
		bt_message_type_string(_type), (_msg))

#define BT_ASSERT_PRE_DEV_MSG_IS_TYPE(_msg, _type)			\
	BT_ASSERT_PRE_DEV(						\
		_BT_ASSERT_PRE_MSG_IS_TYPE_COND((_msg), (_type)),	\
		_BT_ASSERT_PRE_MSG_IS_TYPE_FMT,				\
		bt_message_type_string(_type), (_msg))

BT_HIDDEN
void bt_message_init(struct bt_message *message,
		enum bt_message_type type,
		bt_object_release_func release,
		struct bt_graph *graph);

static inline
void bt_message_reset(struct bt_message *message)
{
	BT_ASSERT_DBG(message);

#ifdef BT_DEV_MODE
	message->frozen = BT_FALSE;
#endif
}

static inline
struct bt_message *bt_message_create_from_pool(
		struct bt_object_pool *pool, struct bt_graph *graph)
{
	struct bt_message *msg = bt_object_pool_create_object(pool);

	if (G_UNLIKELY(!msg)) {
		BT_LIB_LOGE_APPEND_CAUSE(
			"Cannot allocate one message from message pool: "
			"%![pool-]+o, %![graph-]+g", pool, graph);
		goto error;
	}

	if (G_LIKELY(!msg->graph)) {
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
		return "EVENT";
	case BT_MESSAGE_TYPE_MESSAGE_ITERATOR_INACTIVITY:
		return "MESSAGE_ITERATOR_INACTIVITY";
	case BT_MESSAGE_TYPE_STREAM_BEGINNING:
		return "STREAM_BEGINNING";
	case BT_MESSAGE_TYPE_STREAM_END:
		return "STREAM_END";
	case BT_MESSAGE_TYPE_PACKET_BEGINNING:
		return "PACKET_BEGINNING";
	case BT_MESSAGE_TYPE_PACKET_END:
		return "PACKET_END";
	case BT_MESSAGE_TYPE_DISCARDED_EVENTS:
		return "DISCARDED_EVENTS";
	default:
		return "(unknown)";
	}
}

#endif /* BABELTRACE_GRAPH_MESSAGE_MESSAGE_INTERNAL_H */
