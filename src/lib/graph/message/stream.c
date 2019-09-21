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

#define BT_LOG_TAG "LIB/MSG-STREAM"
#include "lib/logging.h"

#include "lib/assert-pre.h"
#include "compat/compiler.h"
#include <babeltrace2/trace-ir/clock-snapshot.h>
#include "lib/trace-ir/stream.h"
#include <babeltrace2/trace-ir/stream-class.h>
#include "lib/trace-ir/stream-class.h"
#include <babeltrace2/graph/message.h>
#include "common/assert.h"
#include <inttypes.h>

#include "stream.h"

static
void destroy_stream_message(struct bt_object *obj)
{
	struct bt_message_stream *message = (void *) obj;

	BT_LIB_LOGD("Destroying stream message: %!+n", message);

	if (message->default_cs) {
		BT_LIB_LOGD("Putting default clock snapshot: %!+k",
			message->default_cs);
		bt_clock_snapshot_destroy(message->default_cs);
		message->default_cs = NULL;
	}

	BT_LIB_LOGD("Putting stream: %!+s", message->stream);
	BT_OBJECT_PUT_REF_AND_RESET(message->stream);
	g_free(message);
}

static inline
struct bt_message *create_stream_message(
		struct bt_self_message_iterator *self_msg_iter,
		struct bt_stream *stream, enum bt_message_type type)
{
	struct bt_message_stream *message;
	struct bt_stream_class *stream_class;

	BT_ASSERT_PRE_NON_NULL(self_msg_iter, "Message iterator");
	BT_ASSERT_PRE_NON_NULL(stream, "Stream");
	stream_class = bt_stream_borrow_class(stream);
	BT_ASSERT(stream_class);
	BT_LIB_LOGD("Creating stream message object: "
		"type=%s, %![stream-]+s, %![sc-]+S",
		bt_message_type_string(type), stream, stream_class);
	message = g_new0(struct bt_message_stream, 1);
	if (!message) {
		BT_LIB_LOGE_APPEND_CAUSE(
			"Failed to allocate one stream message.");
		goto error;
	}

	bt_message_init(&message->parent, type,
		destroy_stream_message, NULL);
	message->stream = stream;
	bt_object_get_ref_no_null_check(message->stream);

	if (stream_class->default_clock_class) {
		message->default_cs = bt_clock_snapshot_create(
			stream_class->default_clock_class);
		if (!message->default_cs) {
			goto error;
		}
	}

	BT_LIB_LOGD("Created stream message object: "
		"%![msg-]+n, %![stream-]+s, %![sc-]+S", message,
		stream, stream_class);

	goto end;

error:
	if (message) {
		g_free(message);
		message = NULL;
	}

end:
	return &message->parent;
}

struct bt_message *bt_message_stream_beginning_create(
		struct bt_self_message_iterator *self_msg_iter,
		const struct bt_stream *stream)
{
	BT_ASSERT_PRE_DEV_NO_ERROR();

	return create_stream_message(self_msg_iter, (void *) stream,
		BT_MESSAGE_TYPE_STREAM_BEGINNING);
}

struct bt_message *bt_message_stream_end_create(
		struct bt_self_message_iterator *self_msg_iter,
		const struct bt_stream *stream)
{
	BT_ASSERT_PRE_DEV_NO_ERROR();

	return create_stream_message(self_msg_iter, (void *) stream,
		BT_MESSAGE_TYPE_STREAM_END);
}

static inline
struct bt_stream *borrow_stream_message_stream(struct bt_message *message)
{
	struct bt_message_stream *stream_msg;

	BT_ASSERT_DBG(message);
	stream_msg = (void *) message;
	return stream_msg->stream;
}

struct bt_stream *bt_message_stream_beginning_borrow_stream(
		struct bt_message *message)
{
	BT_ASSERT_PRE_DEV_NON_NULL(message, "Message");
	BT_ASSERT_PRE_DEV_MSG_IS_TYPE(message,
		BT_MESSAGE_TYPE_STREAM_BEGINNING);
	return borrow_stream_message_stream(message);
}

struct bt_stream *bt_message_stream_end_borrow_stream(
		struct bt_message *message)
{
	BT_ASSERT_PRE_DEV_NON_NULL(message, "Message");
	BT_ASSERT_PRE_DEV_MSG_IS_TYPE(message,
		BT_MESSAGE_TYPE_STREAM_END);
	return borrow_stream_message_stream(message);
}

const struct bt_stream *bt_message_stream_beginning_borrow_stream_const(
		const struct bt_message *message)
{
	return bt_message_stream_beginning_borrow_stream(
		(void *) message);
}

const struct bt_stream *bt_message_stream_end_borrow_stream_const(
		const struct bt_message *message)
{
	return bt_message_stream_end_borrow_stream(
		(void *) message);
}

static
void bt_message_stream_set_default_clock_snapshot(
		struct bt_message *msg, uint64_t raw_value)
{
	struct bt_message_stream *stream_msg = (void *) msg;
	struct bt_stream_class *sc;

	BT_ASSERT(msg);
	BT_ASSERT_PRE_DEV_HOT(msg, "Message", ": %!+n", msg);
	sc = stream_msg->stream->class;
	BT_ASSERT(sc);
	BT_ASSERT_PRE(sc->default_clock_class,
		"Message's stream's class has no default clock class: "
		"%![msg-]+n, %![sc-]+S", msg, sc);
	BT_ASSERT(stream_msg->default_cs);
	bt_clock_snapshot_set_raw_value(stream_msg->default_cs, raw_value);
	stream_msg->default_cs_state = BT_MESSAGE_STREAM_CLOCK_SNAPSHOT_STATE_KNOWN;
	BT_LIB_LOGD("Set stream message's default clock snapshot: "
		"%![msg-]+n, value=%" PRIu64, msg, raw_value);
}

void bt_message_stream_beginning_set_default_clock_snapshot(
		struct bt_message *message, uint64_t raw_value)
{
	BT_ASSERT_PRE_NON_NULL(message, "Message");
	BT_ASSERT_PRE_MSG_IS_TYPE(message, BT_MESSAGE_TYPE_STREAM_BEGINNING);

	bt_message_stream_set_default_clock_snapshot(message, raw_value);
}

void bt_message_stream_end_set_default_clock_snapshot(
		struct bt_message *message, uint64_t raw_value)
{
	BT_ASSERT_PRE_NON_NULL(message, "Message");
	BT_ASSERT_PRE_MSG_IS_TYPE(message, BT_MESSAGE_TYPE_STREAM_END);

	return bt_message_stream_set_default_clock_snapshot(message, raw_value);
}

static enum bt_message_stream_clock_snapshot_state
bt_message_stream_borrow_default_clock_snapshot_const(
		const bt_message *msg, const bt_clock_snapshot **snapshot)
{
	struct bt_message_stream *stream_msg = (void *) msg;
	struct bt_stream_class *sc;

	BT_ASSERT_DBG(msg);
	sc = stream_msg->stream->class;
	BT_ASSERT_DBG(sc);
	BT_ASSERT_PRE_DEV(sc->default_clock_class,
		"Message's stream's class has no default clock class: "
		"%![msg-]+n, %![sc-]+S", msg, sc);
	BT_ASSERT_DBG(stream_msg->default_cs);

	*snapshot = stream_msg->default_cs;

	return stream_msg->default_cs_state;
}

enum bt_message_stream_clock_snapshot_state
bt_message_stream_beginning_borrow_default_clock_snapshot_const(
		const bt_message *message, const bt_clock_snapshot **snapshot)
{
	BT_ASSERT_PRE_DEV_NON_NULL(message, "Message");
	BT_ASSERT_PRE_DEV_MSG_IS_TYPE(message,
		BT_MESSAGE_TYPE_STREAM_BEGINNING);
	return bt_message_stream_borrow_default_clock_snapshot_const(
		message, snapshot);
}

enum bt_message_stream_clock_snapshot_state
bt_message_stream_end_borrow_default_clock_snapshot_const(
		const bt_message *message, const bt_clock_snapshot **snapshot)
{
	BT_ASSERT_PRE_DEV_NON_NULL(message, "Message");
	BT_ASSERT_PRE_DEV_MSG_IS_TYPE(message, BT_MESSAGE_TYPE_STREAM_END);
	return bt_message_stream_borrow_default_clock_snapshot_const(
		message, snapshot);
}

static inline
const struct bt_clock_class *
borrow_stream_message_stream_class_default_clock_class(
		const struct bt_message *msg)
{
	struct bt_message_stream *stream_msg = (void *) msg;

	BT_ASSERT_DBG(msg);
	return stream_msg->stream->class->default_clock_class;
}

const struct bt_clock_class *
bt_message_stream_beginning_borrow_stream_class_default_clock_class_const(
		const struct bt_message *msg)
{
	BT_ASSERT_PRE_DEV_NON_NULL(msg, "Message");
	BT_ASSERT_PRE_DEV_MSG_IS_TYPE(msg,
		BT_MESSAGE_TYPE_STREAM_BEGINNING);
	return borrow_stream_message_stream_class_default_clock_class(msg);
}

const struct bt_clock_class *
bt_message_stream_end_borrow_stream_class_default_clock_class_const(
		const struct bt_message *msg)
{
	BT_ASSERT_PRE_DEV_NON_NULL(msg, "Message");
	BT_ASSERT_PRE_DEV_MSG_IS_TYPE(msg, BT_MESSAGE_TYPE_STREAM_END);
	return borrow_stream_message_stream_class_default_clock_class(msg);
}
