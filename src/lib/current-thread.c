/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2019 Philippe Proulx <pproulx@efficios.com>
 */

#define BT_LOG_TAG "LIB/CUR-THREAD"
#include "lib/logging.h"

#include <babeltrace2/babeltrace.h>
#include <stdint.h>
#include <stdarg.h>

#include "error.h"
#include "common/assert.h"
#include "lib/assert-cond.h"
#include "lib/func-status.h"

/*
 * This points to the thread's error object, or it's `NULL` if there's
 * no current error object.
 */
static __thread struct bt_error *thread_error;

const struct bt_error *bt_current_thread_take_error(void)
{
	struct bt_error *error = thread_error;

	thread_error = NULL;
	BT_LOGD("Took current thread's error object: addr=%p",
		error);
	return error;
}

void bt_current_thread_clear_error(void)
{
	bt_error_destroy(thread_error);
	BT_LOGD("Cleared current thread's error object: addr=%p",
		thread_error);
	thread_error = NULL;
}

void bt_current_thread_move_error(const struct bt_error *error)
{
	BT_ASSERT_PRE_ERROR_NON_NULL(error);
	bt_current_thread_clear_error();
	thread_error = (void *) error;
	BT_LOGD("Moved error object as current thread's error: addr=%p",
		thread_error);
}

/*
 * Creates the current thread's error object if it does not already
 * exist.
 */
static
int try_create_thread_error(void)
{
	int status = BT_FUNC_STATUS_OK;

	if (thread_error) {
		goto end;
	}

	BT_LOGD_STR("Creating current thread's error object.");
	thread_error = bt_error_create();
	if (!thread_error) {
		/* bt_error_create() logs errors */
		status = BT_FUNC_STATUS_MEMORY_ERROR;
		goto end;
	}

	BT_LOGD("Created current thread's error object: addr=%p", thread_error);

end:
	return status;
}

enum bt_current_thread_error_append_cause_status
bt_current_thread_error_append_cause_from_unknown(
		const char *module_name, const char *file_name,
		uint64_t line_no, const char *msg_fmt, ...)
{
	enum bt_current_thread_error_append_cause_status status =
		try_create_thread_error();
	va_list args;

	BT_ASSERT_PRE_NON_NULL(module_name, "Module name");
	BT_ASSERT_PRE_NON_NULL(file_name, "File name");
	BT_ASSERT_PRE_NON_NULL(msg_fmt, "Message format string");

	if (status) {
		goto end;
	}

	BT_LOGD("Appending error cause to current thread's error from unknown actor: "
		"error-addr=%p", thread_error);
	va_start(args, msg_fmt);
	status = bt_error_append_cause_from_unknown(thread_error, module_name,
		file_name, line_no, msg_fmt, args);
	va_end(args);

end:
	return status;
}

enum bt_current_thread_error_append_cause_status
bt_current_thread_error_append_cause_from_component(
		bt_self_component *self_comp, const char *file_name,
		uint64_t line_no, const char *msg_fmt, ...)
{
	enum bt_current_thread_error_append_cause_status status =
		try_create_thread_error();
	va_list args;

	BT_ASSERT_PRE_COMP_NON_NULL(self_comp);
	BT_ASSERT_PRE_NON_NULL(file_name, "File name");
	BT_ASSERT_PRE_NON_NULL(msg_fmt, "Message format string");

	if (status) {
		goto end;
	}

	BT_LOGD("Appending error cause to current thread's error from component: "
		"error-addr=%p", thread_error);
	va_start(args, msg_fmt);
	status = bt_error_append_cause_from_component(thread_error, self_comp,
		file_name, line_no, msg_fmt, args);
	va_end(args);

end:
	return status;
}

enum bt_current_thread_error_append_cause_status
bt_current_thread_error_append_cause_from_component_class(
		bt_self_component_class *self_comp_class, const char *file_name,
		uint64_t line_no, const char *msg_fmt, ...)
{
	enum bt_current_thread_error_append_cause_status status =
		try_create_thread_error();
	va_list args;

	BT_ASSERT_PRE_COMP_CLS_NON_NULL(self_comp_class);
	BT_ASSERT_PRE_NON_NULL(file_name, "File name");
	BT_ASSERT_PRE_NON_NULL(msg_fmt, "Message format string");

	if (status) {
		goto end;
	}

	BT_LOGD("Appending error cause to current thread's error from component class actor: "
		"error-addr=%p", thread_error);
	va_start(args, msg_fmt);
	status = bt_error_append_cause_from_component_class(thread_error,
		self_comp_class, file_name, line_no, msg_fmt, args);
	va_end(args);

end:
	return status;
}

enum bt_current_thread_error_append_cause_status
bt_current_thread_error_append_cause_from_message_iterator(
		bt_self_message_iterator *self_iter, const char *file_name,
		uint64_t line_no, const char *msg_fmt, ...)
{
	enum bt_current_thread_error_append_cause_status status =
		try_create_thread_error();
	va_list args;

	BT_ASSERT_PRE_MSG_ITER_NON_NULL(self_iter);
	BT_ASSERT_PRE_NON_NULL(file_name, "File name");
	BT_ASSERT_PRE_NON_NULL(msg_fmt, "Message format string");

	if (status) {
		goto end;
	}

	BT_LOGD("Appending error cause to current thread's error from message iterator actor: "
		"error-addr=%p", thread_error);
	va_start(args, msg_fmt);
	status = bt_error_append_cause_from_message_iterator(thread_error,
		self_iter, file_name, line_no, msg_fmt, args);
	va_end(args);

end:
	return status;
}
