/*
 * Copyright 2017-2018 Philippe Proulx <pproulx@efficios.com>
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

#define BT_LOG_TAG "MSG-INACTIVITY"
#include <babeltrace/lib-logging-internal.h>

#include <babeltrace/assert-pre-internal.h>
#include <babeltrace/object-internal.h>
#include <babeltrace/compiler-internal.h>
#include <babeltrace/trace-ir/clock-class.h>
#include <babeltrace/trace-ir/clock-value-internal.h>
#include <babeltrace/graph/message-internal.h>
#include <babeltrace/graph/message-inactivity-const.h>
#include <babeltrace/graph/message-inactivity.h>
#include <babeltrace/graph/message-inactivity-internal.h>

static
void bt_message_inactivity_destroy(struct bt_object *obj)
{
	struct bt_message_inactivity *message =
			(struct bt_message_inactivity *) obj;

	BT_LIB_LOGD("Destroying inactivity message: %!+n", message);

	if (message->default_cv) {
		bt_clock_value_recycle(message->default_cv);
	}

	g_free(message);
}

struct bt_message *bt_message_inactivity_create(
		struct bt_self_message_iterator *self_msg_iter,
		struct bt_clock_class *default_clock_class)
{
	struct bt_self_component_port_input_message_iterator *msg_iter =
		(void *) self_msg_iter;
	struct bt_message_inactivity *message;
	struct bt_message *ret_msg = NULL;

	BT_ASSERT_PRE_NON_NULL(msg_iter, "Message iterator");
	BT_ASSERT_PRE_NON_NULL(default_clock_class, "Default clock class");
	BT_LIB_LOGD("Creating inactivity message object: "
		"%![iter-]+i, %![default-cc-]+K", msg_iter,
		default_clock_class);
	message = g_new0(struct bt_message_inactivity, 1);
	if (!message) {
		BT_LOGE_STR("Failed to allocate one inactivity message.");
		goto error;
	}
	bt_message_init(&message->parent,
		BT_MESSAGE_TYPE_INACTIVITY,
		bt_message_inactivity_destroy, NULL);
	ret_msg = &message->parent;
	message->default_cv = bt_clock_value_create(default_clock_class);
	if (!message->default_cv) {
		goto error;
	}

	BT_LIB_LOGD("Created inactivity message object: %!+n", ret_msg);
	goto end;

error:
	BT_OBJECT_PUT_REF_AND_RESET(ret_msg);

end:
	return (void *) ret_msg;
}

void bt_message_inactivity_set_default_clock_value(
		struct bt_message *msg, uint64_t value_cycles)
{
	struct bt_message_inactivity *inactivity = (void *) msg;

	BT_ASSERT_PRE_NON_NULL(msg, "Message");
	BT_ASSERT_PRE_MSG_IS_TYPE(msg, BT_MESSAGE_TYPE_INACTIVITY);
	BT_ASSERT_PRE_HOT(msg, "Message", ": %!+n", msg);
	bt_clock_value_set_value_inline(inactivity->default_cv, value_cycles);
	BT_LIB_LOGV("Set inactivity message's default clock value: "
		"%![msg-]+n, value=%" PRIu64, msg, value_cycles);
}

const struct bt_clock_value *
bt_message_inactivity_borrow_default_clock_value_const(
		const struct bt_message *msg)
{
	struct bt_message_inactivity *inactivity = (void *) msg;

	BT_ASSERT_PRE_NON_NULL(msg, "Message");
	BT_ASSERT_PRE_MSG_IS_TYPE(msg, BT_MESSAGE_TYPE_INACTIVITY);
	return inactivity->default_cv;
}
