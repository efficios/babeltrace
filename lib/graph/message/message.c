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

#define BT_LOG_TAG "MSG"
#include <babeltrace/lib-logging-internal.h>

#include <babeltrace/assert-internal.h>
#include <babeltrace/assert-pre-internal.h>
#include <babeltrace/graph/message-const.h>
#include <babeltrace/graph/message-internal.h>
#include <babeltrace/graph/graph-internal.h>

BT_HIDDEN
void bt_message_init(struct bt_message *message,
		enum bt_message_type type,
		bt_object_release_func release,
		struct bt_graph *graph)
{
	BT_ASSERT(type >= 0 && type <= BT_MESSAGE_TYPE_DISCARDED_EVENTS);
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
	BT_ASSERT_PRE_NON_NULL(message, "Message");
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
