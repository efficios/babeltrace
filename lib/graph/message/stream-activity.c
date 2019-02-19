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

#define BT_LOG_TAG "MSG-STREAM-ACTIVITY"
#include <babeltrace/lib-logging-internal.h>

#include <babeltrace/assert-pre-internal.h>
#include <babeltrace/object-internal.h>
#include <babeltrace/compiler-internal.h>
#include <babeltrace/trace-ir/clock-class.h>
#include <babeltrace/trace-ir/clock-snapshot-internal.h>
#include <babeltrace/trace-ir/stream-class-internal.h>
#include <babeltrace/trace-ir/stream-internal.h>
#include <babeltrace/graph/message-internal.h>
#include <babeltrace/graph/message-stream-activity-beginning-const.h>
#include <babeltrace/graph/message-stream-activity-end-const.h>
#include <babeltrace/graph/message-stream-activity-beginning.h>
#include <babeltrace/graph/message-stream-activity-end.h>
#include <babeltrace/graph/message-stream-activity-internal.h>

static
void destroy_stream_activity_message(struct bt_object *obj)
{
	struct bt_message_stream_activity *message = (void *) obj;

	BT_LIB_LOGD("Destroying stream activity message: %!+n", message);
	BT_LIB_LOGD("Putting stream: %!+s", message->stream);
	BT_OBJECT_PUT_REF_AND_RESET(message->stream);

	if (message->default_cs) {
		bt_clock_snapshot_recycle(message->default_cs);
		message->default_cs = NULL;
	}

	g_free(message);
}

static inline
struct bt_message *create_stream_activity_message(
		struct bt_self_message_iterator *self_msg_iter,
		struct bt_stream *stream, enum bt_message_type type)
{
	struct bt_message_stream_activity *message;
	struct bt_stream_class *stream_class;

	BT_ASSERT_PRE_NON_NULL(self_msg_iter, "Message iterator");
	BT_ASSERT_PRE_NON_NULL(stream, "Stream");
	stream_class = bt_stream_borrow_class(stream);
	BT_ASSERT(stream_class);
	BT_LIB_LOGD("Creating stream activity message object: "
		"type=%s, %![stream-]+s, %![sc-]+S",
		bt_message_type_string(type), stream, stream_class);
	message = g_new0(struct bt_message_stream_activity, 1);
	if (!message) {
		BT_LOGE_STR("Failed to allocate one stream activity message.");
		goto error;
	}

	bt_message_init(&message->parent, type,
		destroy_stream_activity_message, NULL);
	message->stream = stream;
	bt_object_get_no_null_check(message->stream);

	if (stream_class->default_clock_class) {
		message->default_cs = bt_clock_snapshot_create(
			stream_class->default_clock_class);
		if (!message->default_cs) {
			goto error;
		}
	}

	message->default_cs_state =
		BT_MESSAGE_STREAM_ACTIVITY_CLOCK_SNAPSHOT_STATE_UNKNOWN;
	BT_LIB_LOGD("Created stream activity message object: "
		"%![msg-]+n, %![stream-]+s, %![sc-]+S", message,
		stream, stream_class);

	return (void *) &message->parent;

error:
	return NULL;
}

struct bt_message *bt_message_stream_activity_beginning_create(
		struct bt_self_message_iterator *self_msg_iter,
		const struct bt_stream *stream)
{
	return create_stream_activity_message(self_msg_iter, (void *) stream,
		BT_MESSAGE_TYPE_STREAM_ACTIVITY_BEGINNING);
}

struct bt_message *bt_message_stream_activity_end_create(
		struct bt_self_message_iterator *self_msg_iter,
		const struct bt_stream *stream)
{
	return create_stream_activity_message(self_msg_iter, (void *) stream,
		BT_MESSAGE_TYPE_STREAM_ACTIVITY_END);
}

static inline
struct bt_stream *borrow_stream_activity_message_stream(
		struct bt_message *message)
{
	struct bt_message_stream_activity *stream_act_msg = (void *) message;

	BT_ASSERT(message);
	return stream_act_msg->stream;
}

struct bt_stream *bt_message_stream_activity_beginning_borrow_stream(
		struct bt_message *message)
{
	BT_ASSERT_PRE_NON_NULL(message, "Message");
	BT_ASSERT_PRE_MSG_IS_TYPE(message,
		BT_MESSAGE_TYPE_STREAM_ACTIVITY_BEGINNING);
	return borrow_stream_activity_message_stream(message);
}

struct bt_stream *bt_message_stream_activity_end_borrow_stream(
		struct bt_message *message)
{
	BT_ASSERT_PRE_NON_NULL(message, "Message");
	BT_ASSERT_PRE_MSG_IS_TYPE(message,
		BT_MESSAGE_TYPE_STREAM_ACTIVITY_END);
	return borrow_stream_activity_message_stream(message);
}

const struct bt_stream *bt_message_stream_activity_beginning_borrow_stream_const(
		const struct bt_message *message)
{
	return bt_message_stream_activity_beginning_borrow_stream(
		(void *) message);
}

const struct bt_stream *bt_message_stream_activity_end_borrow_stream_const(
		const struct bt_message *message)
{
	return bt_message_stream_activity_end_borrow_stream((void *) message);
}

static inline
void set_stream_activity_message_default_clock_snapshot(
		struct bt_message *msg, uint64_t value_cycles)
{
	struct bt_message_stream_activity *stream_act_msg = (void *) msg;
	struct bt_stream_class *sc;

	BT_ASSERT(msg);
	BT_ASSERT_PRE_HOT(msg, "Message", ": %!+n", msg);
	sc = stream_act_msg->stream->class;
	BT_ASSERT(sc);
	BT_ASSERT_PRE(sc->default_clock_class,
		"Message's stream's class has no default clock class: "
		"%![msg-]+n, %![sc-]+S", msg, sc);
	bt_clock_snapshot_set_raw_value(stream_act_msg->default_cs,
		value_cycles);
	stream_act_msg->default_cs_state =
		BT_MESSAGE_STREAM_ACTIVITY_CLOCK_SNAPSHOT_STATE_KNOWN;
	BT_LIB_LOGV("Set stream activity message's default clock snapshot: "
		"%![msg-]+n, value=%" PRIu64, msg, value_cycles);
}

void bt_message_stream_activity_beginning_set_default_clock_snapshot(
		struct bt_message *msg, uint64_t raw_value)
{
	BT_ASSERT_PRE_NON_NULL(msg, "Message");
	BT_ASSERT_PRE_MSG_IS_TYPE(msg,
		BT_MESSAGE_TYPE_STREAM_ACTIVITY_BEGINNING);
	set_stream_activity_message_default_clock_snapshot(msg, raw_value);
}

void bt_message_stream_activity_end_set_default_clock_snapshot(
		struct bt_message *msg, uint64_t raw_value)
{
	BT_ASSERT_PRE_NON_NULL(msg, "Message");
	BT_ASSERT_PRE_MSG_IS_TYPE(msg,
		BT_MESSAGE_TYPE_STREAM_ACTIVITY_END);
	set_stream_activity_message_default_clock_snapshot(msg, raw_value);
}

static inline
enum bt_message_stream_activity_clock_snapshot_state
borrow_stream_activity_message_default_clock_snapshot_const(
		const bt_message *msg, const bt_clock_snapshot **snapshot)
{
	const struct bt_message_stream_activity *stream_act_msg =
		(const void *) msg;

	BT_ASSERT_PRE_NON_NULL(snapshot, "Clock snapshot (output)");
	*snapshot = stream_act_msg->default_cs;
	return stream_act_msg->default_cs_state;
}

enum bt_message_stream_activity_clock_snapshot_state
bt_message_stream_activity_beginning_borrow_default_clock_snapshot_const(
		const bt_message *msg, const bt_clock_snapshot **snapshot)
{
	BT_ASSERT_PRE_NON_NULL(msg, "Message");
	BT_ASSERT_PRE_MSG_IS_TYPE(msg,
		BT_MESSAGE_TYPE_STREAM_ACTIVITY_BEGINNING);
	return borrow_stream_activity_message_default_clock_snapshot_const(msg,
		snapshot);
}

enum bt_message_stream_activity_clock_snapshot_state
bt_message_stream_activity_end_borrow_default_clock_snapshot_const(
		const bt_message *msg, const bt_clock_snapshot **snapshot)
{
	BT_ASSERT_PRE_NON_NULL(msg, "Message");
	BT_ASSERT_PRE_MSG_IS_TYPE(msg, BT_MESSAGE_TYPE_STREAM_ACTIVITY_END);
	return borrow_stream_activity_message_default_clock_snapshot_const(msg,
		snapshot);
}

static inline
void set_stream_activity_message_default_clock_snapshot_state(
		struct bt_message *msg,
		enum bt_message_stream_activity_clock_snapshot_state state)
{
	struct bt_message_stream_activity *stream_act_msg = (void *) msg;

	BT_ASSERT(msg);
	BT_ASSERT_PRE_HOT(msg, "Message", ": %!+n", msg);
	BT_ASSERT_PRE(state != BT_MESSAGE_STREAM_ACTIVITY_CLOCK_SNAPSHOT_STATE_KNOWN,
		"Invalid clock snapshot state: %![msg-]+n, state=%s",
		msg,
		bt_message_stream_activity_clock_snapshot_state_string(state));
	stream_act_msg->default_cs_state = state;
	BT_LIB_LOGV("Set stream activity message's default clock snapshot state: "
		"%![msg-]+n, state=%s", msg,
		bt_message_stream_activity_clock_snapshot_state_string(state));
}

void bt_message_stream_activity_beginning_set_default_clock_snapshot_state(
		struct bt_message *msg,
		enum bt_message_stream_activity_clock_snapshot_state state)
{
	BT_ASSERT_PRE_NON_NULL(msg, "Message");
	BT_ASSERT_PRE_MSG_IS_TYPE(msg,
		BT_MESSAGE_TYPE_STREAM_ACTIVITY_BEGINNING);
	set_stream_activity_message_default_clock_snapshot_state(msg, state);
}

void bt_message_stream_activity_end_set_default_clock_snapshot_state(
		struct bt_message *msg,
		enum bt_message_stream_activity_clock_snapshot_state state)
{
	BT_ASSERT_PRE_NON_NULL(msg, "Message");
	BT_ASSERT_PRE_MSG_IS_TYPE(msg,
		BT_MESSAGE_TYPE_STREAM_ACTIVITY_END);
	set_stream_activity_message_default_clock_snapshot_state(msg, state);
}

static inline
const struct bt_clock_class *
borrow_stream_activity_message_stream_class_default_clock_class(
		const struct bt_message *msg)
{
	struct bt_message_stream_activity *stream_act_msg = (void *) msg;

	BT_ASSERT(msg);
	return stream_act_msg->stream->class->default_clock_class;
}

const struct bt_clock_class *
bt_message_stream_activity_beginning_borrow_stream_class_default_clock_class_const(
		const struct bt_message *msg)
{
	BT_ASSERT_PRE_NON_NULL(msg, "Message");
	BT_ASSERT_PRE_MSG_IS_TYPE(msg,
		BT_MESSAGE_TYPE_STREAM_ACTIVITY_BEGINNING);
	return borrow_stream_activity_message_stream_class_default_clock_class(
		msg);
}

const struct bt_clock_class *
bt_message_stream_activity_end_borrow_stream_class_default_clock_class_const(
		const struct bt_message *msg)
{
	BT_ASSERT_PRE_NON_NULL(msg, "Message");
	BT_ASSERT_PRE_MSG_IS_TYPE(msg, BT_MESSAGE_TYPE_STREAM_ACTIVITY_END);
	return borrow_stream_activity_message_stream_class_default_clock_class(
		msg);
}
