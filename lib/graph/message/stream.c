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

#define BT_LOG_TAG "MSG-STREAM"
#include <babeltrace/lib-logging-internal.h>

#include <babeltrace/assert-pre-internal.h>
#include <babeltrace/compiler-internal.h>
#include <babeltrace/trace-ir/clock-snapshot-const.h>
#include <babeltrace/trace-ir/stream-internal.h>
#include <babeltrace/trace-ir/stream-class.h>
#include <babeltrace/trace-ir/stream-class-internal.h>
#include <babeltrace/graph/message-stream.h>
#include <babeltrace/graph/message-stream-const.h>
#include <babeltrace/graph/message-stream-internal.h>
#include <babeltrace/assert-internal.h>
#include <inttypes.h>

static
void bt_message_stream_end_destroy(struct bt_object *obj)
{
	struct bt_message_stream_end *message =
			(struct bt_message_stream_end *) obj;

	BT_LIB_LOGD("Destroying stream end message: %!+n",
		message);
	BT_LIB_LOGD("Putting stream: %!+s", message->stream);
	BT_OBJECT_PUT_REF_AND_RESET(message->stream);
	g_free(message);
}

struct bt_message *bt_message_stream_end_create(
		struct bt_self_message_iterator *self_msg_iter,
		struct bt_stream *stream)
{
	struct bt_message_stream_end *message;
	struct bt_stream_class *stream_class;

	BT_ASSERT_PRE_NON_NULL(self_msg_iter, "Message iterator");
	BT_ASSERT_PRE_NON_NULL(stream, "Stream");
	stream_class = bt_stream_borrow_class(stream);
	BT_ASSERT(stream_class);
	BT_LIB_LOGD("Creating stream end message object: "
		"%![stream-]+s, %![sc-]+S", stream, stream_class);
	message = g_new0(struct bt_message_stream_end, 1);
	if (!message) {
		BT_LOGE_STR("Failed to allocate one stream end message.");
		goto error;
	}

	bt_message_init(&message->parent,
			BT_MESSAGE_TYPE_STREAM_END,
			bt_message_stream_end_destroy, NULL);
	message->stream = stream;
	bt_object_get_no_null_check(message->stream);
	BT_LIB_LOGD("Created stream end message object: "
		"%![msg-]+n, %![stream-]+s, %![sc-]+S", message,
		stream, stream_class);

	return (void *) &message->parent;
error:
	return NULL;
}

struct bt_stream *bt_message_stream_end_borrow_stream(
		struct bt_message *message)
{
	struct bt_message_stream_end *stream_end;

	BT_ASSERT_PRE_NON_NULL(message, "Message");
	BT_ASSERT_PRE_MSG_IS_TYPE(message,
		BT_MESSAGE_TYPE_STREAM_END);
	stream_end = (void *) message;
	return stream_end->stream;
}

const struct bt_stream *bt_message_stream_end_borrow_stream_const(
		const struct bt_message *message)
{
	return bt_message_stream_end_borrow_stream(
		(void *) message);
}

static
void bt_message_stream_beginning_destroy(struct bt_object *obj)
{
	struct bt_message_stream_beginning *message =
			(struct bt_message_stream_beginning *) obj;

	BT_LIB_LOGD("Destroying stream beginning message: %!+n",
		message);
	BT_LIB_LOGD("Putting stream: %!+s", message->stream);
	BT_OBJECT_PUT_REF_AND_RESET(message->stream);
	g_free(message);
}

struct bt_message *bt_message_stream_beginning_create(
		struct bt_self_message_iterator *self_msg_iter,
		struct bt_stream *stream)
{
	struct bt_message_stream_beginning *message;
	struct bt_stream_class *stream_class;

	BT_ASSERT_PRE_NON_NULL(self_msg_iter, "Message iterator");
	BT_ASSERT_PRE_NON_NULL(stream, "Stream");
	stream_class = bt_stream_borrow_class(stream);
	BT_ASSERT(stream_class);
	BT_LIB_LOGD("Creating stream beginning message object: "
		"%![stream-]+s, %![sc-]+S", stream, stream_class);
	message = g_new0(struct bt_message_stream_beginning, 1);
	if (!message) {
		BT_LOGE_STR("Failed to allocate one stream beginning message.");
		goto error;
	}

	bt_message_init(&message->parent,
			BT_MESSAGE_TYPE_STREAM_BEGINNING,
			bt_message_stream_beginning_destroy, NULL);
	message->stream = stream;
	bt_object_get_no_null_check(message->stream);
	BT_LIB_LOGD("Created stream beginning message object: "
		"%![msg-]+n, %![stream-]+s, %![sc-]+S", message,
		stream, stream_class);
	return (void *) &message->parent;
error:
	return NULL;
}

struct bt_stream *bt_message_stream_beginning_borrow_stream(
		struct bt_message *message)
{
	struct bt_message_stream_beginning *stream_begin;

	BT_ASSERT_PRE_NON_NULL(message, "Message");
	BT_ASSERT_PRE_MSG_IS_TYPE(message,
		BT_MESSAGE_TYPE_STREAM_BEGINNING);
	stream_begin = (void *) message;
	return stream_begin->stream;
}

const struct bt_stream *bt_message_stream_beginning_borrow_stream_const(
		const struct bt_message *message)
{
	return bt_message_stream_beginning_borrow_stream(
		(void *) message);
}
