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

#define BT_LOG_TAG "MSG-PACKET"
#include <babeltrace/lib-logging-internal.h>

#include <babeltrace/compiler-internal.h>
#include <babeltrace/trace-ir/packet.h>
#include <babeltrace/trace-ir/packet-internal.h>
#include <babeltrace/trace-ir/stream-class.h>
#include <babeltrace/trace-ir/stream.h>
#include <babeltrace/trace-ir/stream-internal.h>
#include <babeltrace/graph/graph-internal.h>
#include <babeltrace/graph/message-packet-const.h>
#include <babeltrace/graph/message-packet.h>
#include <babeltrace/graph/message-packet-internal.h>
#include <babeltrace/assert-internal.h>
#include <babeltrace/assert-pre-internal.h>
#include <babeltrace/object-internal.h>
#include <inttypes.h>

BT_HIDDEN
struct bt_message *bt_message_packet_beginning_new(struct bt_graph *graph)
{
	struct bt_message_packet_beginning *message;

	message = g_new0(struct bt_message_packet_beginning, 1);
	if (!message) {
		BT_LOGE_STR("Failed to allocate one packet beginning message.");
		goto error;
	}

	bt_message_init(&message->parent,
			BT_MESSAGE_TYPE_PACKET_BEGINNING,
			(bt_object_release_func) bt_message_packet_beginning_recycle,
			graph);
	goto end;

error:
	BT_OBJECT_PUT_REF_AND_RESET(message);

end:
	return (void *) message;
}

struct bt_message *bt_message_packet_beginning_create(
		struct bt_self_message_iterator *self_msg_iter,
		struct bt_packet *packet)
{
	struct bt_self_component_port_input_message_iterator *msg_iter =
		(void *) self_msg_iter;
	struct bt_message_packet_beginning *message = NULL;
	struct bt_stream *stream;
	struct bt_stream_class *stream_class;

	BT_ASSERT_PRE_NON_NULL(msg_iter, "Message iterator");
	BT_ASSERT_PRE_NON_NULL(packet, "Packet");
	stream = bt_packet_borrow_stream(packet);
	BT_ASSERT(stream);
	stream_class = bt_stream_borrow_class(stream);
	BT_ASSERT(stream_class);
	BT_LIB_LOGD("Creating packet beginning message object: "
		"%![packet-]+a, %![stream-]+s, %![sc-]+S",
		packet, stream, stream_class);
	message = (void *) bt_message_create_from_pool(
		&msg_iter->graph->packet_begin_msg_pool, msg_iter->graph);
	if (!message) {
		/* bt_message_create_from_pool() logs errors */
		goto end;
	}

	BT_ASSERT(!message->packet);
	message->packet = packet;
	bt_object_get_no_null_check_no_parent_check(
		&message->packet->base);
	bt_packet_set_is_frozen(packet, true);
	BT_LIB_LOGD("Created packet beginning message object: "
		"%![msg-]+n, %![packet-]+a, %![stream-]+s, %![sc-]+S",
		message, packet, stream, stream_class);
	goto end;

end:
	return (void *) message;
}

BT_HIDDEN
void bt_message_packet_beginning_destroy(struct bt_message *msg)
{
	struct bt_message_packet_beginning *packet_begin_msg = (void *) msg;

	BT_LIB_LOGD("Destroying packet beginning message: %!+n", msg);
	BT_LIB_LOGD("Putting packet: %!+a", packet_begin_msg->packet);
	BT_OBJECT_PUT_REF_AND_RESET(packet_begin_msg->packet);
	g_free(msg);
}

BT_HIDDEN
void bt_message_packet_beginning_recycle(struct bt_message *msg)
{
	struct bt_message_packet_beginning *packet_begin_msg = (void *) msg;
	struct bt_graph *graph;

	BT_ASSERT(packet_begin_msg);

	if (unlikely(!msg->graph)) {
		bt_message_packet_beginning_destroy(msg);
		return;
	}

	BT_LIB_LOGD("Recycling packet beginning message: %!+n", msg);
	bt_message_reset(msg);
	bt_object_put_no_null_check(&packet_begin_msg->packet->base);
	packet_begin_msg->packet = NULL;
	graph = msg->graph;
	msg->graph = NULL;
	bt_object_pool_recycle_object(&graph->packet_begin_msg_pool, msg);
}

struct bt_packet *bt_message_packet_beginning_borrow_packet(
		struct bt_message *message)
{
	struct bt_message_packet_beginning *packet_begin;

	BT_ASSERT_PRE_NON_NULL(message, "Message");
	BT_ASSERT_PRE_MSG_IS_TYPE(message,
		BT_MESSAGE_TYPE_PACKET_BEGINNING);
	packet_begin = (void *) message;
	return packet_begin->packet;
}

const struct bt_packet *bt_message_packet_beginning_borrow_packet_const(
		const struct bt_message *message)
{
	return bt_message_packet_beginning_borrow_packet(
		(void *) message);
}

BT_HIDDEN
struct bt_message *bt_message_packet_end_new(struct bt_graph *graph)
{
	struct bt_message_packet_end *message;

	message = g_new0(struct bt_message_packet_end, 1);
	if (!message) {
		BT_LOGE_STR("Failed to allocate one packet end message.");
		goto error;
	}

	bt_message_init(&message->parent,
			BT_MESSAGE_TYPE_PACKET_END,
			(bt_object_release_func) bt_message_packet_end_recycle,
			graph);
	goto end;

error:
	BT_OBJECT_PUT_REF_AND_RESET(message);

end:
	return (void *) message;
}

struct bt_message *bt_message_packet_end_create(
		struct bt_self_message_iterator *self_msg_iter,
		struct bt_packet *packet)
{
	struct bt_self_component_port_input_message_iterator *msg_iter =
		(void *) self_msg_iter;
	struct bt_message_packet_end *message = NULL;
	struct bt_stream *stream;
	struct bt_stream_class *stream_class;

	BT_ASSERT_PRE_NON_NULL(msg_iter, "Message iterator");
	BT_ASSERT_PRE_NON_NULL(packet, "Packet");
	stream = bt_packet_borrow_stream(packet);
	BT_ASSERT(stream);
	stream_class = bt_stream_borrow_class(stream);
	BT_ASSERT(stream_class);
	BT_LIB_LOGD("Creating packet end message object: "
		"%![packet-]+a, %![stream-]+s, %![sc-]+S",
		packet, stream, stream_class);
	message = (void *) bt_message_create_from_pool(
		&msg_iter->graph->packet_end_msg_pool, msg_iter->graph);
	if (!message) {
		/* bt_message_create_from_pool() logs errors */
		goto end;
	}

	BT_ASSERT(!message->packet);
	message->packet = packet;
	bt_object_get_no_null_check_no_parent_check(
		&message->packet->base);
	bt_packet_set_is_frozen(packet, true);
	BT_LIB_LOGD("Created packet end message object: "
		"%![msg-]+n, %![packet-]+a, %![stream-]+s, %![sc-]+S",
		message, packet, stream, stream_class);
	goto end;

end:
	return (void *) message;
}

BT_HIDDEN
void bt_message_packet_end_destroy(struct bt_message *msg)
{
	struct bt_message_packet_end *packet_end_msg = (void *) msg;

	BT_LIB_LOGD("Destroying packet end message: %!+n", msg);
	BT_LIB_LOGD("Putting packet: %!+a", packet_end_msg->packet);
	BT_OBJECT_PUT_REF_AND_RESET(packet_end_msg->packet);
	g_free(msg);
}

BT_HIDDEN
void bt_message_packet_end_recycle(struct bt_message *msg)
{
	struct bt_message_packet_end *packet_end_msg = (void *) msg;
	struct bt_graph *graph;

	BT_ASSERT(packet_end_msg);

	if (!msg->graph) {
		bt_message_packet_end_destroy(msg);
		return;
	}

	BT_LIB_LOGD("Recycling packet end message: %!+n", msg);
	bt_message_reset(msg);
	BT_OBJECT_PUT_REF_AND_RESET(packet_end_msg->packet);
	graph = msg->graph;
	msg->graph = NULL;
	bt_object_pool_recycle_object(&graph->packet_end_msg_pool, msg);
}

struct bt_packet *bt_message_packet_end_borrow_packet(
		struct bt_message *message)
{
	struct bt_message_packet_end *packet_end;

	BT_ASSERT_PRE_NON_NULL(message, "Message");
	BT_ASSERT_PRE_MSG_IS_TYPE(message,
		BT_MESSAGE_TYPE_PACKET_END);
	packet_end = (void *) message;
	return packet_end->packet;
}

const struct bt_packet *bt_message_packet_end_borrow_packet_const(
		const struct bt_message *message)
{
	return bt_message_packet_end_borrow_packet(
		(void *) message);
}
