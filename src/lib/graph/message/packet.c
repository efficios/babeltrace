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

#define BT_LOG_TAG "LIB/MSG-PACKET"
#include "lib/logging.h"

#include <stdbool.h>

#include "lib/assert-pre.h"
#include "lib/assert-post.h"
#include "compat/compiler.h"
#include <babeltrace2/trace-ir/packet.h>
#include "lib/trace-ir/packet.h"
#include <babeltrace2/trace-ir/stream-class.h>
#include <babeltrace2/trace-ir/stream.h>
#include "lib/trace-ir/stream.h"
#include "lib/trace-ir/stream-class.h"
#include "lib/graph/graph.h"
#include <babeltrace2/graph/message.h>
#include "common/assert.h"
#include "lib/object.h"
#include <inttypes.h>

#include "packet.h"

static inline
struct bt_message *new_packet_message(struct bt_graph *graph,
		enum bt_message_type type, bt_object_release_func recycle_func)
{
	struct bt_message_packet *message;

	message = g_new0(struct bt_message_packet, 1);
	if (!message) {
		BT_LIB_LOGE_APPEND_CAUSE(
			"Failed to allocate one packet message.");
		goto error;
	}

	bt_message_init(&message->parent, type, recycle_func, graph);
	goto end;

error:
	BT_OBJECT_PUT_REF_AND_RESET(message);

end:
	return (void *) message;
}

BT_HIDDEN
struct bt_message *bt_message_packet_beginning_new(struct bt_graph *graph)
{
	return new_packet_message(graph, BT_MESSAGE_TYPE_PACKET_BEGINNING,
		(bt_object_release_func) bt_message_packet_beginning_recycle);
}

BT_HIDDEN
struct bt_message *bt_message_packet_end_new(struct bt_graph *graph)
{
	return new_packet_message(graph, BT_MESSAGE_TYPE_PACKET_END,
		(bt_object_release_func) bt_message_packet_end_recycle);
}

static inline
struct bt_message *create_packet_message(
		struct bt_message_iterator *msg_iter,
		struct bt_packet *packet, struct bt_object_pool *pool,
		bool with_cs, uint64_t raw_value)
{
	struct bt_message_packet *message = NULL;
	struct bt_stream *stream;
	struct bt_stream_class *stream_class;
	bool need_cs;

	BT_ASSERT(msg_iter);
	BT_ASSERT_PRE_NON_NULL(packet, "Packet");
	stream = bt_packet_borrow_stream(packet);
	BT_ASSERT(stream);
	stream_class = bt_stream_borrow_class(stream);
	BT_ASSERT(stream_class);

	/*
	 * It's not possible to create a packet from a stream of which
	 * the class indicates that packets are not supported.
	 */
	BT_ASSERT(stream_class->supports_packets);

	if (pool == &msg_iter->graph->packet_begin_msg_pool) {
		need_cs = stream_class->packets_have_beginning_default_clock_snapshot;
	} else {
		need_cs = stream_class->packets_have_end_default_clock_snapshot;
	}

	/*
	 * `packet_has_default_clock_snapshot` implies that the stream
	 * class has a default clock class (precondition).
	 */
	BT_ASSERT_PRE(need_cs ? with_cs : true,
		"Unexpected stream class configuration when creating "
		"a packet beginning or end message: "
		"a default clock snapshot is needed, but none was provided: "
		"%![stream-]+s, %![sc-]+S, with-cs=%d, "
		"cs-val=%" PRIu64,
		stream, stream_class, with_cs, raw_value);
	BT_ASSERT_PRE(!need_cs ? !with_cs : true,
		"Unexpected stream class configuration when creating "
		"a packet beginning or end message: "
		"no default clock snapshot is needed, but one was provided: "
		"%![stream-]+s, %![sc-]+S, with-cs=%d, "
		"cs-val=%" PRIu64,
		stream, stream_class, with_cs, raw_value);
	BT_LIB_LOGD("Creating packet message object: "
		"%![packet-]+a, %![stream-]+s, %![sc-]+S",
		packet, stream, stream_class);
	message = (void *) bt_message_create_from_pool(pool, msg_iter->graph);
	if (!message) {
		/* bt_message_create_from_pool() logs errors */
		goto end;
	}

	if (with_cs) {
		BT_ASSERT(stream_class->default_clock_class);
		message->default_cs = bt_clock_snapshot_create(
			stream_class->default_clock_class);
		if (!message->default_cs) {
			bt_object_put_ref_no_null_check(message);
			message = NULL;
			goto end;
		}

		bt_clock_snapshot_set_raw_value(message->default_cs, raw_value);
	}

	BT_ASSERT(!message->packet);
	message->packet = packet;
	bt_object_get_ref_no_null_check_no_parent_check(
		&message->packet->base);
	bt_packet_set_is_frozen(packet, true);
	BT_LIB_LOGD("Created packet message object: "
		"%![msg-]+n, %![packet-]+a, %![stream-]+s, %![sc-]+S",
		message, packet, stream, stream_class);
	goto end;

end:
	return (void *) message;
}

struct bt_message *bt_message_packet_beginning_create(
		struct bt_self_message_iterator *self_msg_iter,
		const struct bt_packet *packet)
{
	struct bt_message_iterator *msg_iter =
		(void *) self_msg_iter;

	BT_ASSERT_PRE_DEV_NO_ERROR();
	BT_ASSERT_PRE_NON_NULL(msg_iter, "Message iterator");
	return create_packet_message(msg_iter, (void *) packet,
		&msg_iter->graph->packet_begin_msg_pool, false, 0);
}

struct bt_message *bt_message_packet_beginning_create_with_default_clock_snapshot(
		struct bt_self_message_iterator *self_msg_iter,
		const struct bt_packet *packet, uint64_t raw_value)
{
	struct bt_message_iterator *msg_iter =
		(void *) self_msg_iter;

	BT_ASSERT_PRE_DEV_NO_ERROR();
	BT_ASSERT_PRE_NON_NULL(msg_iter, "Message iterator");
	return create_packet_message(msg_iter, (void *) packet,
		&msg_iter->graph->packet_begin_msg_pool, true, raw_value);
}

struct bt_message *bt_message_packet_end_create(
		struct bt_self_message_iterator *self_msg_iter,
		const struct bt_packet *packet)
{
	struct bt_message_iterator *msg_iter =
		(void *) self_msg_iter;

	BT_ASSERT_PRE_DEV_NO_ERROR();
	BT_ASSERT_PRE_NON_NULL(msg_iter, "Message iterator");
	return create_packet_message(msg_iter, (void *) packet,
		&msg_iter->graph->packet_end_msg_pool, false, 0);
}

struct bt_message *bt_message_packet_end_create_with_default_clock_snapshot(
		struct bt_self_message_iterator *self_msg_iter,
		const struct bt_packet *packet, uint64_t raw_value)
{
	struct bt_message_iterator *msg_iter =
		(void *) self_msg_iter;

	BT_ASSERT_PRE_DEV_NO_ERROR();
	BT_ASSERT_PRE_NON_NULL(msg_iter, "Message iterator");
	return create_packet_message(msg_iter, (void *) packet,
		&msg_iter->graph->packet_end_msg_pool, true, raw_value);
}

BT_HIDDEN
void bt_message_packet_destroy(struct bt_message *msg)
{
	struct bt_message_packet *packet_msg = (void *) msg;

	BT_LIB_LOGD("Destroying packet message: %!+n", msg);
	BT_LIB_LOGD("Putting packet: %!+a", packet_msg->packet);
	BT_OBJECT_PUT_REF_AND_RESET(packet_msg->packet);

	if (packet_msg->default_cs) {
		bt_clock_snapshot_recycle(packet_msg->default_cs);
		packet_msg->default_cs = NULL;
	}

	g_free(msg);
}

static inline
void recycle_packet_message(struct bt_message *msg, struct bt_object_pool *pool)
{
	struct bt_message_packet *packet_msg = (void *) msg;

	BT_LIB_LOGD("Recycling packet message: %!+n", msg);
	bt_message_reset(msg);
	bt_object_put_ref_no_null_check(&packet_msg->packet->base);

	if (packet_msg->default_cs) {
		bt_clock_snapshot_recycle(packet_msg->default_cs);
		packet_msg->default_cs = NULL;
	}

	packet_msg->packet = NULL;
	msg->graph = NULL;
	bt_object_pool_recycle_object(pool, msg);
}

BT_HIDDEN
void bt_message_packet_beginning_recycle(struct bt_message *msg)
{
	BT_ASSERT(msg);

	if (G_UNLIKELY(!msg->graph)) {
		bt_message_packet_destroy(msg);
		return;
	}

	recycle_packet_message(msg, &msg->graph->packet_begin_msg_pool);
}

BT_HIDDEN
void bt_message_packet_end_recycle(struct bt_message *msg)
{
	BT_ASSERT(msg);

	if (G_UNLIKELY(!msg->graph)) {
		bt_message_packet_destroy(msg);
		return;
	}

	recycle_packet_message(msg, &msg->graph->packet_end_msg_pool);
}

struct bt_packet *bt_message_packet_beginning_borrow_packet(
		struct bt_message *message)
{
	struct bt_message_packet *packet_msg = (void *) message;

	BT_ASSERT_PRE_DEV_NON_NULL(message, "Message");
	BT_ASSERT_PRE_DEV_MSG_IS_TYPE(message,
		BT_MESSAGE_TYPE_PACKET_BEGINNING);
	return packet_msg->packet;
}

const struct bt_packet *bt_message_packet_beginning_borrow_packet_const(
		const struct bt_message *message)
{
	return bt_message_packet_beginning_borrow_packet(
		(void *) message);
}

struct bt_packet *bt_message_packet_end_borrow_packet(
		struct bt_message *message)
{
	struct bt_message_packet *packet_msg = (void *) message;

	BT_ASSERT_PRE_DEV_NON_NULL(message, "Message");
	BT_ASSERT_PRE_DEV_MSG_IS_TYPE(message,
		BT_MESSAGE_TYPE_PACKET_END);
	return packet_msg->packet;
}

const struct bt_packet *bt_message_packet_end_borrow_packet_const(
		const struct bt_message *message)
{
	return bt_message_packet_end_borrow_packet(
		(void *) message);
}

static inline
const struct bt_clock_snapshot *
borrow_packet_message_default_clock_snapshot_const(
		const struct bt_message *message)
{
	struct bt_message_packet *packet_msg = (void *) message;

	BT_ASSERT_DBG(message);
	BT_ASSERT_PRE_DEV(
		packet_msg->packet->stream->class->default_clock_class,
		"Message's stream's class has no default clock class: "
		"%![msg-]+n, %![sc-]+S",
		message, packet_msg->packet->stream->class);
	return packet_msg->default_cs;
}

const struct bt_clock_snapshot *
bt_message_packet_beginning_borrow_default_clock_snapshot_const(
		const struct bt_message *msg)
{
	BT_ASSERT_PRE_DEV_NON_NULL(msg, "Message");
	BT_ASSERT_PRE_DEV_MSG_IS_TYPE(msg, BT_MESSAGE_TYPE_PACKET_BEGINNING);
	return borrow_packet_message_default_clock_snapshot_const(msg);
}

const struct bt_clock_snapshot *
bt_message_packet_end_borrow_default_clock_snapshot_const(
		const struct bt_message *msg)
{
	BT_ASSERT_PRE_DEV_NON_NULL(msg, "Message");
	BT_ASSERT_PRE_DEV_MSG_IS_TYPE(msg, BT_MESSAGE_TYPE_PACKET_END);
	return borrow_packet_message_default_clock_snapshot_const(msg);
}

static inline
const struct bt_clock_class *
borrow_packet_message_stream_class_default_clock_class(
		const struct bt_message *msg)
{
	struct bt_message_packet *packet_msg = (void *) msg;

	BT_ASSERT_DBG(msg);
	return packet_msg->packet->stream->class->default_clock_class;
}

const struct bt_clock_class *
bt_message_packet_beginning_borrow_stream_class_default_clock_class_const(
		const struct bt_message *msg)
{
	BT_ASSERT_PRE_DEV_NON_NULL(msg, "Message");
	BT_ASSERT_PRE_DEV_MSG_IS_TYPE(msg, BT_MESSAGE_TYPE_PACKET_BEGINNING);
	return borrow_packet_message_stream_class_default_clock_class(msg);
}

const struct bt_clock_class *
bt_message_packet_end_borrow_stream_class_default_clock_class_const(
		const struct bt_message *msg)
{
	BT_ASSERT_PRE_DEV_NON_NULL(msg, "Message");
	BT_ASSERT_PRE_DEV_MSG_IS_TYPE(msg, BT_MESSAGE_TYPE_PACKET_END);
	return borrow_packet_message_stream_class_default_clock_class(msg);
}
