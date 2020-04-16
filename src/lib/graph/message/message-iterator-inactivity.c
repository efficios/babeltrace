/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright 2017-2018 Philippe Proulx <pproulx@efficios.com>
 */

#define BT_LOG_TAG "LIB/MSG-MSG-ITER-INACTIVITY"
#include "lib/logging.h"

#include "lib/assert-cond.h"
#include "lib/object.h"
#include "compat/compiler.h"
#include <babeltrace2/trace-ir/clock-class.h>
#include "lib/trace-ir/clock-snapshot.h"
#include "lib/graph/message/message.h"
#include <babeltrace2/graph/message.h>

#include "message-iterator-inactivity.h"

static
void bt_message_message_iterator_inactivity_destroy(struct bt_object *obj)
{
	struct bt_message_message_iterator_inactivity *message =
			(struct bt_message_message_iterator_inactivity *) obj;

	BT_LIB_LOGD("Destroying message iterator inactivity message: %!+n",
		message);

	if (message->cs) {
		bt_clock_snapshot_recycle(message->cs);
		message->cs = NULL;
	}

	g_free(message);
}

struct bt_message *bt_message_message_iterator_inactivity_create(
		struct bt_self_message_iterator *self_msg_iter,
		const struct bt_clock_class *clock_class,
		uint64_t value_cycles)
{
	struct bt_message_iterator *msg_iter =
		(void *) self_msg_iter;
	struct bt_message_message_iterator_inactivity *message;
	struct bt_message *ret_msg = NULL;

	BT_ASSERT_PRE_DEV_NO_ERROR();
	BT_ASSERT_PRE_MSG_ITER_NON_NULL(msg_iter);
	BT_ASSERT_PRE_DEF_CLK_CLS_NON_NULL(clock_class);
	BT_LIB_LOGD("Creating message iterator inactivity message object: "
		"%![iter-]+i, %![cc-]+K, value=%" PRIu64, msg_iter,
		clock_class, value_cycles);
	message = g_new0(struct bt_message_message_iterator_inactivity, 1);
	if (!message) {
		BT_LIB_LOGE_APPEND_CAUSE(
			"Failed to allocate one message iterator "
			"inactivity message.");
		goto error;
	}
	bt_message_init(&message->parent,
		BT_MESSAGE_TYPE_MESSAGE_ITERATOR_INACTIVITY,
		bt_message_message_iterator_inactivity_destroy, NULL);
	ret_msg = &message->parent;
	message->cs = bt_clock_snapshot_create(
		(void *) clock_class);
	if (!message->cs) {
		goto error;
	}
	bt_clock_snapshot_set_raw_value(message->cs, value_cycles);

	BT_LIB_LOGD("Created message iterator inactivity message object: %!+n",
		ret_msg);
	goto end;

error:
	BT_OBJECT_PUT_REF_AND_RESET(ret_msg);

end:
	return (void *) ret_msg;
}

extern const struct bt_clock_snapshot *
bt_message_message_iterator_inactivity_borrow_clock_snapshot_const(
		const bt_message *msg)
{
	struct bt_message_message_iterator_inactivity *inactivity = (void *) msg;

	BT_ASSERT_PRE_DEV_MSG_NON_NULL(msg);
	BT_ASSERT_PRE_DEV_MSG_HAS_TYPE(msg,
		BT_MESSAGE_TYPE_MESSAGE_ITERATOR_INACTIVITY);
	return inactivity->cs;
}
