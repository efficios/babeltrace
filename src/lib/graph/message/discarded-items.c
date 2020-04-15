/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright 2019 Philippe Proulx <pproulx@efficios.com>
 */

#define BT_LOG_TAG "LIB/MSG-DISCARDED-ITEMS"
#include "lib/logging.h"

#include <stdbool.h>

#include "lib/assert-cond.h"
#include "lib/object.h"
#include "compat/compiler.h"
#include <babeltrace2/trace-ir/clock-class.h>
#include "lib/trace-ir/clock-snapshot.h"
#include "lib/trace-ir/stream-class.h"
#include "lib/trace-ir/stream.h"
#include "lib/property.h"
#include "lib/graph/message/message.h"

#include "discarded-items.h"

static
void destroy_discarded_items_message(struct bt_object *obj)
{
	struct bt_message_discarded_items *message = (void *) obj;

	BT_LIB_LOGD("Destroying discarded items message: %!+n", message);
	BT_LIB_LOGD("Putting stream: %!+s", message->stream);
	BT_OBJECT_PUT_REF_AND_RESET(message->stream);

	if (message->default_begin_cs) {
		bt_clock_snapshot_recycle(message->default_begin_cs);
		message->default_begin_cs = NULL;
	}

	if (message->default_end_cs) {
		bt_clock_snapshot_recycle(message->default_end_cs);
		message->default_end_cs = NULL;
	}

	g_free(message);
}

static inline
struct bt_message *create_discarded_items_message(
		struct bt_self_message_iterator *self_msg_iter,
		enum bt_message_type type, struct bt_stream *stream,
		bool with_cs,
		uint64_t beginning_raw_value, uint64_t end_raw_value)
{
	struct bt_message_discarded_items *message;
	struct bt_stream_class *stream_class;
	bool has_support;
	bool need_cs;

	BT_ASSERT_PRE_NON_NULL(self_msg_iter, "Message iterator");
	BT_ASSERT_PRE_NON_NULL(stream, "Stream");
	stream_class = bt_stream_borrow_class(stream);
	BT_ASSERT(stream_class);

	if (type == BT_MESSAGE_TYPE_DISCARDED_EVENTS) {
		has_support = stream_class->supports_discarded_events;
		need_cs = stream_class->discarded_events_have_default_clock_snapshots;
	} else {
		has_support = stream_class->supports_discarded_packets;
		need_cs = stream_class->discarded_packets_have_default_clock_snapshots;
	}

	BT_ASSERT_PRE(has_support,
		"Stream class does not support discarded events or packets: "
		"type=%s, %![stream-]+s, %![sc-]+S",
		bt_message_type_string(type), stream, stream_class);
	BT_ASSERT_PRE(need_cs ? with_cs : true,
		"Unexpected stream class configuration when creating "
		"a discarded events or discarded packets message: "
		"default clock snapshots are needed, but none was provided: "
		"type=%s, %![stream-]+s, %![sc-]+S, with-cs=%d, "
		"cs-begin-val=%" PRIu64 ", cs-end-val=%" PRIu64,
		bt_message_type_string(type), stream, stream_class,
		with_cs, beginning_raw_value, end_raw_value);
	BT_ASSERT_PRE(!need_cs ? !with_cs : true,
		"Unexpected stream class configuration when creating "
		"a discarded events or discarded packets message: "
		"no default clock snapshots are needed, but two were provided: "
		"type=%s, %![stream-]+s, %![sc-]+S, with-cs=%d, "
		"cs-begin-val=%" PRIu64 ", cs-end-val=%" PRIu64,
		bt_message_type_string(type), stream, stream_class,
		with_cs, beginning_raw_value, end_raw_value);
	BT_LIB_LOGD("Creating discarded items message object: "
		"type=%s, %![stream-]+s, %![sc-]+S, with-cs=%d, "
		"cs-begin-val=%" PRIu64 ", cs-end-val=%" PRIu64,
		bt_message_type_string(type), stream, stream_class,
		with_cs, beginning_raw_value, end_raw_value);
	message = g_new0(struct bt_message_discarded_items, 1);
	if (!message) {
		BT_LIB_LOGE_APPEND_CAUSE(
			"Failed to allocate one discarded items message.");
		goto error;
	}

	bt_message_init(&message->parent, type,
		destroy_discarded_items_message, NULL);
	message->stream = stream;
	bt_object_get_ref_no_null_check(message->stream);

	if (with_cs) {
		BT_ASSERT(stream_class->default_clock_class);
		message->default_begin_cs = bt_clock_snapshot_create(
			stream_class->default_clock_class);
		if (!message->default_begin_cs) {
			goto error;
		}

		bt_clock_snapshot_set_raw_value(message->default_begin_cs,
			beginning_raw_value);

		message->default_end_cs = bt_clock_snapshot_create(
			stream_class->default_clock_class);
		if (!message->default_end_cs) {
			goto error;
		}

		bt_clock_snapshot_set_raw_value(message->default_end_cs,
			end_raw_value);
	}

	bt_property_uint_init(&message->count,
		BT_PROPERTY_AVAILABILITY_NOT_AVAILABLE, 0);
	BT_LIB_LOGD("Created discarded items message object: "
		"%![msg-]+n, %![stream-]+s, %![sc-]+S", message,
		stream, stream_class);

	return (void *) &message->parent;

error:
	return NULL;
}

static inline
struct bt_stream *borrow_discarded_items_message_stream(
		struct bt_message *message)
{
	struct bt_message_discarded_items *disc_items_msg = (void *) message;

	BT_ASSERT_DBG(message);
	return disc_items_msg->stream;
}

static inline
void set_discarded_items_message_count(struct bt_message *message,
		uint64_t count)
{
	struct bt_message_discarded_items *disc_items_msg = (void *) message;

	BT_ASSERT(message);
	BT_ASSERT_PRE_DEV_HOT(message, "Message", ": %!+n", message);
	bt_property_uint_set(&disc_items_msg->count, count);
}

static inline
enum bt_property_availability get_discarded_items_message_count(
		const struct bt_message *message, uint64_t *count)
{
	struct bt_message_discarded_items *disc_items_msg = (void *) message;

	BT_ASSERT_PRE_DEV_NON_NULL(count, "Count (output)");
	BT_ASSERT_DBG(message);
	*count = disc_items_msg->count.value;
	return disc_items_msg->count.base.avail;
}

static inline
const struct bt_clock_snapshot *
borrow_discarded_items_message_beginning_default_clock_snapshot_const(
		const struct bt_message *message)
{
	struct bt_message_discarded_items *disc_items_msg = (void *) message;

	BT_ASSERT_DBG(message);
	BT_ASSERT_PRE_DEV(disc_items_msg->stream->class->default_clock_class,
		"Message's stream's class has no default clock class: "
		"%![msg-]+n, %![sc-]+S",
		message, disc_items_msg->stream->class);
	return disc_items_msg->default_begin_cs;
}

static inline
const struct bt_clock_snapshot *
borrow_discarded_items_message_end_default_clock_snapshot_const(
		const struct bt_message *message)
{
	struct bt_message_discarded_items *disc_items_msg = (void *) message;

	BT_ASSERT_DBG(message);
	BT_ASSERT_PRE_DEV(disc_items_msg->stream->class->default_clock_class,
		"Message's stream's class has no default clock class: "
		"%![msg-]+n, %![sc-]+S",
		message, disc_items_msg->stream->class);
	return disc_items_msg->default_end_cs;
}

struct bt_message *bt_message_discarded_events_create(
		struct bt_self_message_iterator *message_iterator,
		const struct bt_stream *stream)
{
	BT_ASSERT_PRE_DEV_NO_ERROR();

	return create_discarded_items_message(message_iterator,
		BT_MESSAGE_TYPE_DISCARDED_EVENTS, (void *) stream,
		false, 0, 0);
}

struct bt_message *bt_message_discarded_events_create_with_default_clock_snapshots(
		struct bt_self_message_iterator *message_iterator,
		const struct bt_stream *stream, uint64_t beginning_raw_value,
		uint64_t end_raw_value)
{
	BT_ASSERT_PRE_DEV_NO_ERROR();
	BT_ASSERT_PRE_BEGIN_LE_END(message_iterator, beginning_raw_value, end_raw_value);

	return create_discarded_items_message(message_iterator,
		BT_MESSAGE_TYPE_DISCARDED_EVENTS, (void *) stream,
		true, beginning_raw_value, end_raw_value);
}

struct bt_stream *bt_message_discarded_events_borrow_stream(
		struct bt_message *message)
{
	BT_ASSERT_PRE_DEV_NON_NULL(message, "Message");
	BT_ASSERT_PRE_DEV_MSG_IS_TYPE(message, BT_MESSAGE_TYPE_DISCARDED_EVENTS);
	return borrow_discarded_items_message_stream(message);
}

void bt_message_discarded_events_set_count(struct bt_message *message,
		uint64_t count)
{
	BT_ASSERT_PRE_NON_NULL(message, "Message");
	BT_ASSERT_PRE_MSG_IS_TYPE(message, BT_MESSAGE_TYPE_DISCARDED_EVENTS);
	BT_ASSERT_PRE(count > 0, "Discarded event count is 0.");
	set_discarded_items_message_count(message, count);
}

const struct bt_clock_snapshot *
bt_message_discarded_events_borrow_beginning_default_clock_snapshot_const(
		const struct bt_message *msg)
{
	BT_ASSERT_PRE_DEV_NON_NULL(msg, "Message");
	BT_ASSERT_PRE_DEV_MSG_IS_TYPE(msg, BT_MESSAGE_TYPE_DISCARDED_EVENTS);
	return borrow_discarded_items_message_beginning_default_clock_snapshot_const(
		msg);
}

const struct bt_clock_snapshot *
bt_message_discarded_events_borrow_end_default_clock_snapshot_const(
		const struct bt_message *msg)
{
	BT_ASSERT_PRE_DEV_NON_NULL(msg, "Message");
	BT_ASSERT_PRE_DEV_MSG_IS_TYPE(msg, BT_MESSAGE_TYPE_DISCARDED_EVENTS);
	return borrow_discarded_items_message_end_default_clock_snapshot_const(
		msg);
}

const struct bt_stream *
bt_message_discarded_events_borrow_stream_const(const struct bt_message *message)
{
	return (void *) bt_message_discarded_events_borrow_stream(
		(void *) message);
}

enum bt_property_availability bt_message_discarded_events_get_count(
		const struct bt_message *message, uint64_t *count)
{
	BT_ASSERT_PRE_DEV_NON_NULL(message, "Message");
	BT_ASSERT_PRE_DEV_MSG_IS_TYPE(message,
		BT_MESSAGE_TYPE_DISCARDED_EVENTS);
	return get_discarded_items_message_count(message, count);
}

struct bt_message *bt_message_discarded_packets_create(
		struct bt_self_message_iterator *message_iterator,
		const struct bt_stream *stream)
{
	BT_ASSERT_PRE_DEV_NO_ERROR();

	return create_discarded_items_message(message_iterator,
		BT_MESSAGE_TYPE_DISCARDED_PACKETS, (void *) stream,
		false, 0, 0);
}

struct bt_message *bt_message_discarded_packets_create_with_default_clock_snapshots(
		struct bt_self_message_iterator *message_iterator,
		const struct bt_stream *stream, uint64_t beginning_raw_value,
		uint64_t end_raw_value)
{
	BT_ASSERT_PRE_DEV_NO_ERROR();
	BT_ASSERT_PRE_BEGIN_LE_END(message_iterator, beginning_raw_value, end_raw_value);

	return create_discarded_items_message(message_iterator,
		BT_MESSAGE_TYPE_DISCARDED_PACKETS, (void *) stream,
		true, beginning_raw_value, end_raw_value);
}

struct bt_stream *bt_message_discarded_packets_borrow_stream(
		struct bt_message *message)
{
	BT_ASSERT_PRE_DEV_NON_NULL(message, "Message");
	BT_ASSERT_PRE_DEV_MSG_IS_TYPE(message,
		BT_MESSAGE_TYPE_DISCARDED_PACKETS);
	return borrow_discarded_items_message_stream(message);
}

void bt_message_discarded_packets_set_count(struct bt_message *message,
		uint64_t count)
{
	BT_ASSERT_PRE_NON_NULL(message, "Message");
	BT_ASSERT_PRE_MSG_IS_TYPE(message, BT_MESSAGE_TYPE_DISCARDED_PACKETS);
	BT_ASSERT_PRE(count > 0, "Discarded packet count is 0.");
	set_discarded_items_message_count(message, count);
}

const struct bt_clock_snapshot *
bt_message_discarded_packets_borrow_beginning_default_clock_snapshot_const(
		const struct bt_message *msg)
{
	BT_ASSERT_PRE_DEV_NON_NULL(msg, "Message");
	BT_ASSERT_PRE_DEV_MSG_IS_TYPE(msg, BT_MESSAGE_TYPE_DISCARDED_PACKETS);
	return borrow_discarded_items_message_beginning_default_clock_snapshot_const(
		msg);
}

const struct bt_clock_snapshot *
bt_message_discarded_packets_borrow_end_default_clock_snapshot_const(
		const struct bt_message *msg)
{
	BT_ASSERT_PRE_DEV_NON_NULL(msg, "Message");
	BT_ASSERT_PRE_DEV_MSG_IS_TYPE(msg, BT_MESSAGE_TYPE_DISCARDED_PACKETS);
	return borrow_discarded_items_message_end_default_clock_snapshot_const(
		msg);
}

const struct bt_stream *
bt_message_discarded_packets_borrow_stream_const(const struct bt_message *message)
{
	return (void *) bt_message_discarded_packets_borrow_stream(
		(void *) message);
}

enum bt_property_availability bt_message_discarded_packets_get_count(
		const struct bt_message *message, uint64_t *count)
{
	BT_ASSERT_PRE_DEV_NON_NULL(message, "Message");
	BT_ASSERT_PRE_DEV_MSG_IS_TYPE(message,
		BT_MESSAGE_TYPE_DISCARDED_PACKETS);
	return get_discarded_items_message_count(message, count);
}

static inline
const struct bt_clock_class *
borrow_discarded_items_message_stream_class_default_clock_class(
		const struct bt_message *msg)
{
	struct bt_message_discarded_items *disc_items_msg = (void *) msg;

	BT_ASSERT_DBG(msg);
	return disc_items_msg->stream->class->default_clock_class;
}

const struct bt_clock_class *
bt_message_discarded_events_borrow_stream_class_default_clock_class_const(
		const struct bt_message *msg)
{
	BT_ASSERT_PRE_DEV_NON_NULL(msg, "Message");
	BT_ASSERT_PRE_DEV_MSG_IS_TYPE(msg, BT_MESSAGE_TYPE_DISCARDED_EVENTS);
	return borrow_discarded_items_message_stream_class_default_clock_class(
		msg);
}

const struct bt_clock_class *
bt_message_discarded_packets_borrow_stream_class_default_clock_class_const(
		const struct bt_message *msg)
{
	BT_ASSERT_PRE_DEV_NON_NULL(msg, "Message");
	BT_ASSERT_PRE_DEV_MSG_IS_TYPE(msg, BT_MESSAGE_TYPE_DISCARDED_PACKETS);
	return borrow_discarded_items_message_stream_class_default_clock_class(
		msg);
}
