/*
 * Copyright 2016-2019 EfficiOS Inc.
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

#define BT_LOG_TAG "CLI/QUERY"
#include "logging.h"

#include <babeltrace2/babeltrace.h>

#include "common/common.h"

#include "babeltrace2-query.h"

static
void set_fail_reason(const char **fail_reason, const char *reason)
{
	if (fail_reason) {
		*fail_reason = reason;
	}
}

BT_HIDDEN
bt_query_executor_query_status cli_query(const bt_component_class *comp_cls,
		const char *obj, const bt_value *params,
		bt_logging_level log_level, const bt_interrupter *interrupter,
		const bt_value **user_result, const char **fail_reason)
{
	const bt_value *result = NULL;
	bt_query_executor_query_status status;
	bt_query_executor *query_exec;

	set_fail_reason(fail_reason, "unknown error");
	BT_ASSERT(user_result);
	query_exec = bt_query_executor_create(comp_cls, obj, params);
	if (!query_exec) {
		BT_CLI_LOGE_APPEND_CAUSE("Cannot create a query executor.");
		goto error;
	}

	if (bt_query_executor_set_logging_level(query_exec, log_level) !=
			BT_QUERY_EXECUTOR_SET_LOGGING_LEVEL_STATUS_OK) {
		BT_CLI_LOGE_APPEND_CAUSE(
			"Cannot set query executor's logging level: "
			"log-level=%s",
			bt_common_logging_level_string(log_level));
		goto error;
	}

	if (interrupter) {
		if (bt_query_executor_add_interrupter(query_exec,
				interrupter) !=
				BT_QUERY_EXECUTOR_ADD_INTERRUPTER_STATUS_OK) {
			BT_CLI_LOGE_APPEND_CAUSE(
				"Cannot add interrupter to query executor.");
			goto error;
		}
	}

	while (true) {
		status = bt_query_executor_query(query_exec, &result);
		switch (status) {
		case BT_QUERY_EXECUTOR_QUERY_STATUS_OK:
			goto ok;
		case BT_QUERY_EXECUTOR_QUERY_STATUS_AGAIN:
		{
			const uint64_t sleep_time_us = 100000;

			if (interrupter && bt_interrupter_is_set(interrupter)) {
				set_fail_reason(fail_reason, "interrupted by user");
				goto error;
			}

			/* Wait 100 ms and retry */
			BT_LOGD("Got BT_QUERY_EXECUTOR_QUERY_STATUS_AGAIN: sleeping: "
				"time-us=%" PRIu64, sleep_time_us);

			if (usleep(sleep_time_us)) {
				if (interrupter && bt_interrupter_is_set(interrupter)) {
					BT_CLI_LOGW_APPEND_CAUSE(
						"Query was interrupted by user: "
						"comp-cls-addr=%p, comp-cls-name=\"%s\", "
						"query-obj=\"%s\"", comp_cls,
						bt_component_class_get_name(comp_cls),
						obj);
					set_fail_reason(fail_reason,
						"interrupted by user");
					goto error;
				}
			}

			continue;
		}
		case BT_QUERY_EXECUTOR_QUERY_STATUS_ERROR:
			if (interrupter && bt_interrupter_is_set(interrupter)) {
				set_fail_reason(fail_reason, "interrupted by user");
				goto error;
			}

			goto error;
		case BT_QUERY_EXECUTOR_QUERY_STATUS_UNKNOWN_OBJECT:
			set_fail_reason(fail_reason, "unknown query object");
			goto end;
		case BT_QUERY_EXECUTOR_QUERY_STATUS_MEMORY_ERROR:
			set_fail_reason(fail_reason, "not enough memory");
			goto error;
		default:
			BT_LOGF("Unknown query status: status=%s",
				bt_common_func_status_string(status));
			bt_common_abort();
		}
	}

ok:
	*user_result = result;
	result = NULL;
	goto end;

error:
	status = BT_QUERY_EXECUTOR_QUERY_STATUS_ERROR;

end:
	bt_query_executor_put_ref(query_exec);
	bt_value_put_ref(result);
	return status;
}
