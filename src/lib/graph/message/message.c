/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright 2017-2018 Philippe Proulx <pproulx@efficios.com>
 * Copyright 2016 Jérémie Galarneau <jeremie.galarneau@efficios.com>
 */

#define BT_LOG_TAG "LIB/MSG"
#include "lib/logging.h"

#include "common/assert.h"
#include "lib/assert-cond.h"
#include <babeltrace2/graph/message.h>
#include "lib/graph/message/message.h"
#include "lib/graph/graph.h"

BT_HIDDEN
void bt_message_init(struct bt_message *message,
		enum bt_message_type type,
		bt_object_release_func release,
		struct bt_graph *graph)
{
	message->type = type;
	bt_object_init_shared(&message->base, release);
	message->graph = graph;

	if (graph) {
		bt_graph_add_message(graph, message);
	}
}

enum bt_message_type bt_message_get_type(
		const struct bt_message *message)
{
	BT_ASSERT_PRE_DEV_NON_NULL(message, "Message");
	return message->type;
}

BT_HIDDEN
void bt_message_unlink_graph(struct bt_message *msg)
{
	BT_ASSERT(msg);
	msg->graph = NULL;
}

void bt_message_get_ref(const struct bt_message *message)
{
	bt_object_get_ref(message);
}

void bt_message_put_ref(const struct bt_message *message)
{
	bt_object_put_ref(message);
}
