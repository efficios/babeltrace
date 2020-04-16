/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright 2019 EfficiOS, Inc.
 */

#define BT_LOG_TAG "LIB/MESSAGE-ITERATOR-CLASS"
#include "lib/logging.h"

#include "message-iterator-class.h"

#include "compat/compiler.h"
#include "lib/assert-cond.h"
#include "lib/func-status.h"

#define BT_ASSERT_COND_DEV_MSG_ITER_CLS_HOT(_msg_iter_cls) \
	BT_ASSERT_PRE_DEV_HOT((_msg_iter_cls), \
		"Message iterator class", ": %!+I", (_msg_iter_cls))

BT_HIDDEN
void _bt_message_iterator_class_freeze(
		const struct bt_message_iterator_class *msg_iter_cls)
{
	BT_ASSERT(msg_iter_cls);
	BT_LIB_LOGD("Freezing message iterator class: %!+I", msg_iter_cls);
	((struct bt_message_iterator_class *) msg_iter_cls)->frozen = true;
}

void bt_message_iterator_class_get_ref(
		const bt_message_iterator_class *message_iterator_class)
{
	bt_object_get_ref(message_iterator_class);
}

void bt_message_iterator_class_put_ref(
		const bt_message_iterator_class *message_iterator_class)
{
	bt_object_put_ref(message_iterator_class);
}

static
void destroy_iterator_class(struct bt_object *obj)
{
	struct bt_message_iterator_class *class;

	BT_ASSERT(obj);
	class = container_of(obj, struct bt_message_iterator_class, base);

	BT_LIB_LOGI("Destroying message iterator class: %!+I", class);

	g_free(class);
}

struct bt_message_iterator_class *bt_message_iterator_class_create(
		bt_message_iterator_class_next_method next_method)
{
	struct bt_message_iterator_class *message_iterator_class;

	BT_ASSERT_PRE_NO_ERROR();
	BT_ASSERT_PRE_NON_NULL(next_method, "Next method");
	BT_LOGI("Creating message iterator class: next-method-addr=%p",
		next_method);

	message_iterator_class = g_new0(struct bt_message_iterator_class, 1);
	if (!message_iterator_class) {
		BT_LIB_LOGE_APPEND_CAUSE(
			"Failed to allocate one message iterator class.");
		goto end;
	}

	bt_object_init_shared(&message_iterator_class->base, destroy_iterator_class);

	message_iterator_class->methods.next = next_method;

end:
	return message_iterator_class;
}

bt_message_iterator_class_set_method_status
bt_message_iterator_class_set_initialize_method(
	bt_message_iterator_class *message_iterator_class,
	bt_message_iterator_class_initialize_method method)
{
	BT_ASSERT_PRE_NO_ERROR();
	BT_ASSERT_PRE_MSG_ITER_CLS_NON_NULL(message_iterator_class);
	BT_ASSERT_PRE_METHOD_NON_NULL(method);
	BT_ASSERT_COND_DEV_MSG_ITER_CLS_HOT(message_iterator_class);
	message_iterator_class->methods.initialize = method;
	BT_LIB_LOGD("Set message iterator class's iterator initialization method"
		": %!+I", message_iterator_class);
	return BT_FUNC_STATUS_OK;
}

bt_message_iterator_class_set_method_status
bt_message_iterator_class_set_finalize_method(
		bt_message_iterator_class *message_iterator_class,
		bt_message_iterator_class_finalize_method method)
{
	BT_ASSERT_PRE_NO_ERROR();
	BT_ASSERT_PRE_MSG_ITER_CLS_NON_NULL(message_iterator_class);
	BT_ASSERT_PRE_METHOD_NON_NULL(method);
	BT_ASSERT_COND_DEV_MSG_ITER_CLS_HOT(message_iterator_class);
	message_iterator_class->methods.finalize = method;
	BT_LIB_LOGD("Set message iterator class's finalization method"
		": %!+I", message_iterator_class);
	return BT_FUNC_STATUS_OK;
}

bt_message_iterator_class_set_method_status
bt_message_iterator_class_set_seek_ns_from_origin_methods(
		bt_message_iterator_class *message_iterator_class,
		bt_message_iterator_class_seek_ns_from_origin_method seek_method,
		bt_message_iterator_class_can_seek_ns_from_origin_method can_seek_method)
{
	BT_ASSERT_PRE_NO_ERROR();
	BT_ASSERT_PRE_MSG_ITER_CLS_NON_NULL(message_iterator_class);
	BT_ASSERT_PRE_NON_NULL(seek_method, "Seek method");
	BT_ASSERT_COND_DEV_MSG_ITER_CLS_HOT(message_iterator_class);
	message_iterator_class->methods.seek_ns_from_origin = seek_method;
	message_iterator_class->methods.can_seek_ns_from_origin = can_seek_method;
	BT_LIB_LOGD("Set message iterator class's \"seek nanoseconds from origin\" method"
		": %!+I", message_iterator_class);
	return BT_FUNC_STATUS_OK;
}

bt_message_iterator_class_set_method_status
bt_message_iterator_class_set_seek_beginning_methods(
		bt_message_iterator_class *message_iterator_class,
		bt_message_iterator_class_seek_beginning_method seek_method,
		bt_message_iterator_class_can_seek_beginning_method can_seek_method)
{
	BT_ASSERT_PRE_NO_ERROR();
	BT_ASSERT_PRE_MSG_ITER_CLS_NON_NULL(message_iterator_class);
	BT_ASSERT_PRE_NON_NULL(seek_method, "Seek method");
	BT_ASSERT_COND_DEV_MSG_ITER_CLS_HOT(message_iterator_class);
	message_iterator_class->methods.seek_beginning = seek_method;
	message_iterator_class->methods.can_seek_beginning = can_seek_method;
	BT_LIB_LOGD("Set message iterator class's \"seek beginning\" methods"
		": %!+C", message_iterator_class);
	return BT_FUNC_STATUS_OK;
}
