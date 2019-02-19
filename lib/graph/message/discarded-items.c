/*
 * Copyright 2019 Philippe Proulx <pproulx@efficios.com>
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

#define BT_LOG_TAG "MSG-DISCARDED-ITEMS"
#include <babeltrace/lib-logging-internal.h>

#include <babeltrace/assert-pre-internal.h>
#include <babeltrace/object-internal.h>
#include <babeltrace/compiler-internal.h>
#include <babeltrace/trace-ir/clock-class.h>
#include <babeltrace/trace-ir/clock-snapshot-internal.h>
#include <babeltrace/trace-ir/stream-class-internal.h>
#include <babeltrace/trace-ir/stream-internal.h>
#include <babeltrace/property-internal.h>
#include <babeltrace/graph/message-internal.h>
#include <babeltrace/graph/message-discarded-items-internal.h>
#include <babeltrace/graph/message-discarded-events.h>
#include <babeltrace/graph/message-discarded-events-const.h>
#include <babeltrace/graph/message-discarded-packets.h>
#include <babeltrace/graph/message-discarded-packets-const.h>

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

	BT_ASSERT_PRE_NON_NULL(self_msg_iter, "Message iterator");
	BT_ASSERT_PRE_NON_NULL(stream, "Stream");
	stream_class = bt_stream_borrow_class(stream);
	BT_ASSERT(stream_class);
	BT_ASSERT_PRE((with_cs && stream_class->default_clock_class) ||
		(!with_cs && !stream_class->default_clock_class),
		"Creating a message with a default clock snapshot, but without "
		"a default clock class, or without a default clock snapshot, "
		"but with a default clock class: ",
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
		BT_LOGE_STR("Failed to allocate one discarded items message.");
		goto error;
	}

	bt_message_init(&message->parent, type,
		destroy_discarded_items_message, NULL);
	message->stream = stream;
	bt_object_get_no_null_check(message->stream);

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

	BT_ASSERT(message);
	return disc_items_msg->stream;
}

static inline
void set_discarded_items_message_count(struct bt_message *message,
		uint64_t count)
{
	struct bt_message_discarded_items *disc_items_msg = (void *) message;

	BT_ASSERT(message);
	BT_ASSERT_PRE_HOT(message, "Message", ": %!+n", message);
	bt_property_uint_set(&disc_items_msg->count, count);
}

static inline
enum bt_property_availability get_discarded_items_message_count(
		const struct bt_message *message, uint64_t *count)
{
	struct bt_message_discarded_items *disc_items_msg = (void *) message;

	BT_ASSERT_PRE_NON_NULL(count, "Count (output)");
	BT_ASSERT(message);
	*count = disc_items_msg->count.value;
	return disc_items_msg->count.base.avail;
}

static inline
enum bt_clock_snapshot_state
borrow_discarded_items_message_default_beginning_clock_snapshot_const(
		const struct bt_message *message,
		const struct bt_clock_snapshot **snapshot)
{
	struct bt_message_discarded_items *disc_items_msg = (void *) message;

	BT_ASSERT(message);
	BT_ASSERT_PRE(disc_items_msg->stream->class->default_clock_class,
		"Message's stream's class has no default clock class: "
		"%![msg-]+n, %![sc-]+S",
		message, disc_items_msg->stream->class);
	BT_ASSERT_PRE_NON_NULL(snapshot, "Clock snapshot (output)");
	*snapshot = disc_items_msg->default_begin_cs;
	return BT_CLOCK_SNAPSHOT_STATE_KNOWN;
}

static inline
enum bt_clock_snapshot_state
borrow_discarded_items_message_default_end_clock_snapshot_const(
		const struct bt_message *message,
		const struct bt_clock_snapshot **snapshot)
{
	struct bt_message_discarded_items *disc_items_msg = (void *) message;

	BT_ASSERT(message);
	BT_ASSERT_PRE(disc_items_msg->stream->class->default_clock_class,
		"Message's stream's class has no default clock class: "
		"%![msg-]+n, %![sc-]+S",
		message, disc_items_msg->stream->class);
	BT_ASSERT_PRE_NON_NULL(snapshot, "Clock snapshot (output)");
	*snapshot = disc_items_msg->default_end_cs;
	return BT_CLOCK_SNAPSHOT_STATE_KNOWN;
}

struct bt_message *bt_message_discarded_events_create(
		struct bt_self_message_iterator *message_iterator,
		const struct bt_stream *stream)
{
	return create_discarded_items_message(message_iterator,
		BT_MESSAGE_TYPE_DISCARDED_EVENTS, (void *) stream,
		false, 0, 0);
}

struct bt_message *bt_message_discarded_events_create_with_default_clock_snapshots(
		struct bt_self_message_iterator *message_iterator,
		const struct bt_stream *stream, uint64_t beginning_raw_value,
		uint64_t end_raw_value)
{
	return create_discarded_items_message(message_iterator,
		BT_MESSAGE_TYPE_DISCARDED_EVENTS, (void *) stream,
		true, beginning_raw_value, end_raw_value);
}

struct bt_stream *bt_message_discarded_events_borrow_stream(
		struct bt_message *message)
{
	BT_ASSERT_PRE_NON_NULL(message, "Message");
	BT_ASSERT_PRE_MSG_IS_TYPE(message, BT_MESSAGE_TYPE_DISCARDED_EVENTS);
	return borrow_discarded_items_message_stream(message);
}

void bt_message_discarded_events_set_count(struct bt_message *message,
		uint64_t count)
{
	BT_ASSERT_PRE_NON_NULL(message, "Message");
	BT_ASSERT_PRE_MSG_IS_TYPE(message, BT_MESSAGE_TYPE_DISCARDED_EVENTS);
	set_discarded_items_message_count(message, count);
}

enum bt_clock_snapshot_state
bt_message_discarded_events_borrow_default_beginning_clock_snapshot_const(
		const struct bt_message *msg,
		const struct bt_clock_snapshot **snapshot)
{
	BT_ASSERT_PRE_NON_NULL(msg, "Message");
	BT_ASSERT_PRE_MSG_IS_TYPE(msg, BT_MESSAGE_TYPE_DISCARDED_EVENTS);
	return borrow_discarded_items_message_default_beginning_clock_snapshot_const(
		msg, snapshot);
}

enum bt_clock_snapshot_state
bt_message_discarded_events_borrow_default_end_clock_snapshot_const(
		const struct bt_message *msg,
		const struct bt_clock_snapshot **snapshot)
{
	BT_ASSERT_PRE_NON_NULL(msg, "Message");
	BT_ASSERT_PRE_MSG_IS_TYPE(msg, BT_MESSAGE_TYPE_DISCARDED_EVENTS);
	return borrow_discarded_items_message_default_end_clock_snapshot_const(
		msg, snapshot);
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
	BT_ASSERT_PRE_NON_NULL(message, "Message");
	BT_ASSERT_PRE_MSG_IS_TYPE(message, BT_MESSAGE_TYPE_DISCARDED_EVENTS);
	return get_discarded_items_message_count(message, count);
}

struct bt_message *bt_message_discarded_packets_create(
		struct bt_self_message_iterator *message_iterator,
		const struct bt_stream *stream)
{
	return create_discarded_items_message(message_iterator,
		BT_MESSAGE_TYPE_DISCARDED_PACKETS, (void *) stream,
		false, 0, 0);
}

struct bt_message *bt_message_discarded_packets_create_with_default_clock_snapshots(
		struct bt_self_message_iterator *message_iterator,
		const struct bt_stream *stream, uint64_t beginning_raw_value,
		uint64_t end_raw_value)
{
	return create_discarded_items_message(message_iterator,
		BT_MESSAGE_TYPE_DISCARDED_PACKETS, (void *) stream,
		true, beginning_raw_value, end_raw_value);
}

struct bt_stream *bt_message_discarded_packets_borrow_stream(
		struct bt_message *message)
{
	BT_ASSERT_PRE_NON_NULL(message, "Message");
	BT_ASSERT_PRE_MSG_IS_TYPE(message, BT_MESSAGE_TYPE_DISCARDED_PACKETS);
	return borrow_discarded_items_message_stream(message);
}

void bt_message_discarded_packets_set_count(struct bt_message *message,
		uint64_t count)
{
	BT_ASSERT_PRE_NON_NULL(message, "Message");
	BT_ASSERT_PRE_MSG_IS_TYPE(message, BT_MESSAGE_TYPE_DISCARDED_PACKETS);
	set_discarded_items_message_count(message, count);
}

enum bt_clock_snapshot_state
bt_message_discarded_packets_borrow_default_beginning_clock_snapshot_const(
		const struct bt_message *msg,
		const struct bt_clock_snapshot **snapshot)
{
	BT_ASSERT_PRE_NON_NULL(msg, "Message");
	BT_ASSERT_PRE_MSG_IS_TYPE(msg, BT_MESSAGE_TYPE_DISCARDED_PACKETS);
	return borrow_discarded_items_message_default_beginning_clock_snapshot_const(
		msg, snapshot);
}

enum bt_clock_snapshot_state
bt_message_discarded_packets_borrow_default_end_clock_snapshot_const(
		const struct bt_message *msg,
		const struct bt_clock_snapshot **snapshot)
{
	BT_ASSERT_PRE_NON_NULL(msg, "Message");
	BT_ASSERT_PRE_MSG_IS_TYPE(msg, BT_MESSAGE_TYPE_DISCARDED_PACKETS);
	return borrow_discarded_items_message_default_end_clock_snapshot_const(
		msg, snapshot);
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
	BT_ASSERT_PRE_NON_NULL(message, "Message");
	BT_ASSERT_PRE_MSG_IS_TYPE(message, BT_MESSAGE_TYPE_DISCARDED_PACKETS);
	return get_discarded_items_message_count(message, count);
}

static inline
const struct bt_clock_class *
borrow_discarded_items_message_stream_class_default_clock_class(
		const struct bt_message *msg)
{
	struct bt_message_discarded_items *disc_items_msg = (void *) msg;

	BT_ASSERT(msg);
	return disc_items_msg->stream->class->default_clock_class;
}

const struct bt_clock_class *
bt_message_discarded_events_borrow_stream_class_default_clock_class_const(
		const struct bt_message *msg)
{
	BT_ASSERT_PRE_NON_NULL(msg, "Message");
	BT_ASSERT_PRE_MSG_IS_TYPE(msg, BT_MESSAGE_TYPE_DISCARDED_EVENTS);
	return borrow_discarded_items_message_stream_class_default_clock_class(
		msg);
}

const struct bt_clock_class *
bt_message_discarded_packets_borrow_stream_class_default_clock_class_const(
		const struct bt_message *msg)
{
	BT_ASSERT_PRE_NON_NULL(msg, "Message");
	BT_ASSERT_PRE_MSG_IS_TYPE(msg, BT_MESSAGE_TYPE_DISCARDED_PACKETS);
	return borrow_discarded_items_message_stream_class_default_clock_class(
		msg);
}
