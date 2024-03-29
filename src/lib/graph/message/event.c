/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright 2017-2018 Philippe Proulx <pproulx@efficios.com>
 * Copyright 2016 Jérémie Galarneau <jeremie.galarneau@efficios.com>
 */

#define BT_LOG_TAG "LIB/MSG-EVENT"
#include "lib/logging.h"

#include "common/assert.h"
#include "lib/assert-cond.h"
#include "compat/compiler.h"
#include "lib/object.h"
#include <babeltrace2/trace-ir/event.h>
#include "lib/trace-ir/event.h"
#include "lib/trace-ir/event-class.h"
#include "lib/trace-ir/stream-class.h"
#include <babeltrace2/trace-ir/trace.h>
#include "lib/trace-ir/clock-snapshot.h"
#include "lib/graph/graph.h"
#include <babeltrace2/graph/message.h>
#include <babeltrace2/types.h>
#include <stdbool.h>

#include "event.h"

#define BT_ASSERT_PRE_DEV_MSG_IS_EVENT(_msg)				\
	BT_ASSERT_PRE_DEV_MSG_HAS_TYPE("message", (_msg), "event",	\
		BT_MESSAGE_TYPE_EVENT)

struct bt_message *bt_message_event_new(
		struct bt_graph *graph)
{
	struct bt_message_event *message = NULL;

	message = g_new0(struct bt_message_event, 1);
	if (!message) {
		BT_LIB_LOGE_APPEND_CAUSE(
			"Failed to allocate one event message.");
		goto error;
	}

	bt_message_init(&message->parent, BT_MESSAGE_TYPE_EVENT,
		(bt_object_release_func) bt_message_event_recycle, graph);
	goto end;

error:
	BT_OBJECT_PUT_REF_AND_RESET(message);

end:
	return (void *) message;
}

static
struct bt_event *create_event(struct bt_event_class *event_class,
		struct bt_packet *packet, struct bt_stream *stream,
		const char *api_func)
{
	struct bt_event *event = NULL;

	BT_ASSERT_DBG(event_class);
	BT_ASSERT_DBG(stream);
	event = bt_object_pool_create_object(&event_class->event_pool);
	if (G_UNLIKELY(!event)) {
		BT_LIB_LOGE_APPEND_CAUSE(
			"Cannot allocate one event from event class's event pool: "
			"%![ec-]+E", event_class);
		goto end;
	}

	if (G_LIKELY(!event->class)) {
		event->class = event_class;
		bt_object_get_ref_no_null_check(&event_class->base);
	}

	BT_ASSERT_DBG(!event->stream);
	event->stream = stream;
	bt_object_get_ref_no_null_check_no_parent_check(&event->stream->base);
	BT_LIB_LOGD("Set event's stream: %![event-]+e, %![stream-]+s",
		event, stream);

	if (packet) {
		BT_ASSERT_PRE_DEV_FROM_FUNC(api_func,
			"packet-stream-class-is-event-class-stream-class",
			bt_event_class_borrow_stream_class(
				event_class) == packet->stream->class,
			"Packet's stream class and event class's stream class differ: "
			"%![ec-]+E, %![packet-]+a", event, packet);
		BT_ASSERT_DBG(event->stream->class->supports_packets);
		BT_ASSERT_DBG(!event->packet);
		event->packet = packet;
		bt_object_get_ref_no_null_check_no_parent_check(&event->packet->base);
		BT_LIB_LOGD("Set event's packet: %![event-]+e, %![packet-]+a",
			event, packet);
	}

end:
	return event;
}

static inline
struct bt_message *create_event_message(
		struct bt_self_message_iterator *self_msg_iter,
		const struct bt_event_class *c_event_class,
		const struct bt_packet *c_packet,
		const struct bt_stream *c_stream, bool with_cs,
		uint64_t raw_value, const char *api_func)
{
	struct bt_message_iterator *msg_iter =
		(void *) self_msg_iter;
	struct bt_message_event *message = NULL;
	struct bt_event_class *event_class = (void *) c_event_class;
	struct bt_stream_class *stream_class;
	struct bt_packet *packet = (void *) c_packet;
	struct bt_stream *stream = (void *) c_stream;
	struct bt_event *event;

	BT_ASSERT_DBG(stream);
	BT_ASSERT_PRE_MSG_ITER_NON_NULL_FROM_FUNC(api_func, msg_iter);
	BT_ASSERT_PRE_EC_NON_NULL_FROM_FUNC(api_func, event_class);
	stream_class = bt_event_class_borrow_stream_class_inline(event_class);
	BT_ASSERT_PRE_FROM_FUNC(api_func,
		"stream-class-is-event-class-stream-class",
		bt_event_class_borrow_stream_class(event_class) ==
			stream->class,
		"Stream's class and event's stream class differ: "
		"%![ec-]+E, %![stream-]+s", event_class, stream);
	BT_ASSERT_DBG(stream_class);
	BT_ASSERT_PRE_FROM_FUNC(api_func,
		"with-default-clock-snapshot-if-stream-class-has-default-clock-class",
		(with_cs && stream_class->default_clock_class) ||
		(!with_cs && !stream_class->default_clock_class),
		"Creating an event message with a default clock snapshot, but without "
		"a default clock class, or without a default clock snapshot, "
		"but with a default clock class: ",
		"%![ec-]+E, %![sc-]+S, with-cs=%d, "
		"cs-val=%" PRIu64,
		event_class, stream_class, with_cs, raw_value);
	BT_LIB_LOGD("Creating event message object: %![ec-]+E", event_class);
	event = create_event(event_class, packet, stream, api_func);
	if (G_UNLIKELY(!event)) {
		BT_LIB_LOGE_APPEND_CAUSE(
			"Cannot create event from event class: "
			"%![ec-]+E", event_class);
		goto error;
	}

	/*
	 * Create message from pool _after_ we have everything
	 * (in this case, a valid event object) so that we never have an
	 * error condition with a non-NULL message object.
	 * Otherwise:
	 *
	 * * We cannot recycle the message on error because
	 *   bt_message_event_recycle() expects a complete
	 *   message (and the event or clock class priority map
	 *   object could be unset).
	 *
	 * * We cannot destroy the message because we would need
	 *   to notify the graph (pool owner) so that it removes the
	 *   message from its message array.
	 */
	message = (void *) bt_message_create_from_pool(
		&msg_iter->graph->event_msg_pool, msg_iter->graph);
	if (G_UNLIKELY(!message)) {
		/* bt_message_create_from_pool() logs errors */
		goto error;
	}

	if (with_cs) {
		BT_ASSERT_DBG(stream_class->default_clock_class);
		message->default_cs = bt_clock_snapshot_create(
			stream_class->default_clock_class);
		if (!message->default_cs) {
			goto error;
		}

		bt_clock_snapshot_set_raw_value(message->default_cs, raw_value);
	}

	BT_ASSERT_DBG(!message->event);
	message->event = event;

	if (packet) {
		bt_packet_set_is_frozen(packet, true);
	}

	bt_stream_freeze(stream);
	bt_event_class_freeze(event_class);
	BT_LIB_LOGD("Created event message object: "
		"%![msg-]+n, %![event-]+e", message, event);
	goto end;

error:
	BT_ASSERT(!message);
	bt_event_destroy(event);

end:
	return (void *) message;
}

BT_EXPORT
struct bt_message *bt_message_event_create(
		struct bt_self_message_iterator *msg_iter,
		const struct bt_event_class *event_class,
		const struct bt_stream *stream)
{
	BT_ASSERT_PRE_DEV_NO_ERROR();
	BT_ASSERT_PRE_STREAM_NON_NULL(stream);
	return create_event_message(msg_iter, event_class, NULL, stream,
		false, 0, __func__);
}

BT_EXPORT
struct bt_message *bt_message_event_create_with_packet(
		struct bt_self_message_iterator *msg_iter,
		const struct bt_event_class *event_class,
		const struct bt_packet *packet)
{
	BT_ASSERT_PRE_DEV_NO_ERROR();
	BT_ASSERT_PRE_PACKET_NON_NULL(packet);
	return create_event_message(msg_iter, event_class, packet,
		packet->stream, false, 0, __func__);
}

BT_EXPORT
struct bt_message *bt_message_event_create_with_default_clock_snapshot(
		struct bt_self_message_iterator *msg_iter,
		const struct bt_event_class *event_class,
		const struct bt_stream *stream,
		uint64_t raw_value)
{
	BT_ASSERT_PRE_DEV_NO_ERROR();
	BT_ASSERT_PRE_STREAM_NON_NULL(stream);
	return create_event_message(msg_iter, event_class, NULL, stream,
		true, raw_value, __func__);
}

BT_EXPORT
struct bt_message *
bt_message_event_create_with_packet_and_default_clock_snapshot(
		struct bt_self_message_iterator *msg_iter,
		const struct bt_event_class *event_class,
		const struct bt_packet *packet,
		uint64_t raw_value)
{
	BT_ASSERT_PRE_DEV_NO_ERROR();
	BT_ASSERT_PRE_PACKET_NON_NULL(packet);
	return create_event_message(msg_iter, event_class, packet,
		packet->stream, true, raw_value, __func__);
}

void bt_message_event_destroy(struct bt_message *msg)
{
	struct bt_message_event *event_msg = (void *) msg;

	BT_LIB_LOGD("Destroying event message: %!+n", msg);

	if (event_msg->event) {
		BT_LIB_LOGD("Recycling event: %!+e", event_msg->event);
		bt_event_recycle(event_msg->event);
		event_msg->event = NULL;
	}

	if (event_msg->default_cs) {
		bt_clock_snapshot_recycle(event_msg->default_cs);
		event_msg->default_cs = NULL;
	}

	g_free(msg);
}

void bt_message_event_recycle(struct bt_message *msg)
{
	struct bt_message_event *event_msg = (void *) msg;
	struct bt_graph *graph;

	BT_ASSERT_DBG(event_msg);

	if (G_UNLIKELY(!msg->graph)) {
		bt_message_event_destroy(msg);
		return;
	}

	BT_LIB_LOGD("Recycling event message: %![msg-]+n, %![event-]+e",
		msg, event_msg->event);
	bt_message_reset(msg);
	BT_ASSERT_DBG(event_msg->event);
	bt_event_recycle(event_msg->event);
	event_msg->event = NULL;

	if (event_msg->default_cs) {
		bt_clock_snapshot_recycle(event_msg->default_cs);
		event_msg->default_cs = NULL;
	}

	graph = msg->graph;
	msg->graph = NULL;
	bt_object_pool_recycle_object(&graph->event_msg_pool, msg);
}

#define BT_ASSERT_PRE_DEV_FOR_BORROW_EVENTS(_msg)			\
	do {								\
		BT_ASSERT_PRE_DEV_MSG_NON_NULL(_msg);			\
		BT_ASSERT_PRE_DEV_MSG_IS_EVENT(_msg);			\
	} while (0)

static inline
struct bt_event *borrow_event(struct bt_message *message)
{
	struct bt_message_event *event_message;

	event_message = container_of(message,
			struct bt_message_event, parent);
	return event_message->event;
}

BT_EXPORT
struct bt_event *bt_message_event_borrow_event(
		struct bt_message *message)
{
	BT_ASSERT_PRE_DEV_FOR_BORROW_EVENTS(message);
	return borrow_event(message);
}

BT_EXPORT
const struct bt_event *bt_message_event_borrow_event_const(
		const struct bt_message *message)
{
	BT_ASSERT_PRE_DEV_FOR_BORROW_EVENTS(message);
	return borrow_event((void *) message);
}

BT_EXPORT
const struct bt_clock_snapshot *
bt_message_event_borrow_default_clock_snapshot_const(
		const struct bt_message *msg)
{
	struct bt_message_event *event_msg = (void *) msg;
	struct bt_stream_class *stream_class;

	BT_ASSERT_PRE_DEV_MSG_NON_NULL(msg);
	BT_ASSERT_PRE_DEV_MSG_IS_EVENT(msg);
	stream_class = bt_event_class_borrow_stream_class_inline(
		event_msg->event->class);
	BT_ASSERT_DBG(stream_class);
	BT_ASSERT_PRE_DEV_MSG_SC_DEF_CLK_CLS(msg, stream_class);
	return event_msg->default_cs;
}

BT_EXPORT
const bt_clock_class *
bt_message_event_borrow_stream_class_default_clock_class_const(
		const bt_message *msg)
{
	struct bt_message_event *event_msg = (void *) msg;
	struct bt_stream_class *stream_class;

	BT_ASSERT_PRE_DEV_MSG_NON_NULL(msg);
	BT_ASSERT_PRE_DEV_MSG_IS_EVENT(msg);
	stream_class = bt_event_class_borrow_stream_class_inline(
		event_msg->event->class);
	BT_ASSERT_DBG(stream_class);
	return stream_class->default_clock_class;
}
