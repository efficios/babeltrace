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

#define BT_LOG_TAG "LIB/QUERY-EXECUTOR"
#include "lib/logging.h"

#include "common/assert.h"
#include "common/common.h"
#include "lib/assert-pre.h"
#include "lib/assert-post.h"
#include <babeltrace2/graph/query-executor.h>
#include <babeltrace2/graph/component-class.h>
#include <babeltrace2/graph/query-executor.h>
#include <babeltrace2/value.h>
#include "lib/object.h"
#include "compat/compiler.h"

#include "component-class.h"
#include "query-executor.h"
#include "interrupter.h"
#include "lib/func-status.h"

static
void bt_query_executor_destroy(struct bt_object *obj)
{
	struct bt_query_executor *query_exec =
		container_of(obj, struct bt_query_executor, base);

	BT_LOGD("Destroying query executor: addr=%p", query_exec);

	if (query_exec->interrupters) {
		BT_LOGD_STR("Putting interrupters.");
		g_ptr_array_free(query_exec->interrupters, TRUE);
		query_exec->interrupters = NULL;
	}

	BT_LOGD_STR("Putting component class.");
	BT_OBJECT_PUT_REF_AND_RESET(query_exec->comp_cls);

	if (query_exec->object) {
		g_string_free(query_exec->object, TRUE);
		query_exec->object = NULL;
	}

	BT_LOGD_STR("Putting parameters.");
	BT_OBJECT_PUT_REF_AND_RESET(query_exec->params);
	BT_OBJECT_PUT_REF_AND_RESET(query_exec->default_interrupter);
	g_free(query_exec);
}

struct bt_query_executor *bt_query_executor_create_with_method_data(
		const bt_component_class *comp_cls, const char *object,
		const bt_value *params, void *method_data)
{
	struct bt_query_executor *query_exec;

	BT_ASSERT_PRE_NO_ERROR();
	BT_ASSERT_PRE_NON_NULL(comp_cls, "Component class");
	BT_ASSERT_PRE_NON_NULL(object, "Object");
	BT_LIB_LOGD("Creating query executor: "
		"%![comp-cls-]+C, object=\"%s\", %![params-]+v",
		comp_cls, object, params);
	query_exec = g_new0(struct bt_query_executor, 1);
	if (!query_exec) {
		BT_LIB_LOGE_APPEND_CAUSE(
			"Failed to allocate one query executor.");
		goto end;
	}

	query_exec->interrupters = g_ptr_array_new_with_free_func(
		(GDestroyNotify) bt_object_put_ref_no_null_check);
	if (!query_exec->interrupters) {
		BT_LIB_LOGE_APPEND_CAUSE("Failed to allocate one GPtrArray.");
		BT_OBJECT_PUT_REF_AND_RESET(query_exec);
		goto end;
	}

	query_exec->default_interrupter = bt_interrupter_create();
	if (!query_exec->default_interrupter) {
		BT_LIB_LOGE_APPEND_CAUSE(
			"Failed to create one interrupter object.");
		BT_OBJECT_PUT_REF_AND_RESET(query_exec);
		goto end;
	}

	query_exec->object = g_string_new(object);
	if (!query_exec->object) {
		BT_LIB_LOGE_APPEND_CAUSE("Failed to allocate one GString.");
		BT_OBJECT_PUT_REF_AND_RESET(query_exec);
		goto end;
	}

	query_exec->comp_cls = comp_cls;
	bt_object_get_ref_no_null_check(query_exec->comp_cls);

	if (!params) {
		query_exec->params = bt_value_null;
	} else {
		query_exec->params = params;
	}

	bt_object_get_ref_no_null_check(query_exec->params);
	query_exec->method_data = method_data;
	query_exec->log_level = BT_LOGGING_LEVEL_NONE;
	bt_query_executor_add_interrupter(query_exec,
		query_exec->default_interrupter);
	bt_object_init_shared(&query_exec->base,
		bt_query_executor_destroy);
	BT_LIB_LOGD("Created query executor: "
		"addr=%p, %![comp-cls-]+C, object=\"%s\", %![params-]+v",
		query_exec, comp_cls, object, params);

end:
	return (void *) query_exec;
}

struct bt_query_executor *bt_query_executor_create(
		const bt_component_class *comp_cls, const char *object,
		const bt_value *params)
{
	BT_ASSERT_PRE_NO_ERROR();
	return bt_query_executor_create_with_method_data(comp_cls,
		object, params, NULL);
}

enum bt_query_executor_query_status bt_query_executor_query(
		struct bt_query_executor *query_exec,
		const struct bt_value **user_result)
{
	typedef enum bt_component_class_query_method_status (*method_t)(
		void * /* self component class */,
		void * /* private query executor */,
		const char * /* object */,
		const struct bt_value * /* parameters */,
		void * /* method data */,
		const struct bt_value ** /* result */);

	enum bt_query_executor_query_status status;
	enum bt_component_class_query_method_status query_status;
	method_t method = NULL;

	BT_ASSERT_PRE_NO_ERROR();
	BT_ASSERT_PRE_NON_NULL(query_exec, "Query executor");
	BT_ASSERT_PRE_NON_NULL(user_result, "Result (output)");

	/*
	 * Initial check: is the query executor already interrupted? If
	 * so, return `BT_FUNC_STATUS_AGAIN`. Returning this status is
	 * harmless: it's not `BT_FUNC_STATUS_OK` (there's no result),
	 * and it's not `BT_FUNC_STATUS_ERROR` either (there's no
	 * legitimate error). Since any query operation could return
	 * `BT_FUNC_STATUS_AGAIN` when interrupted or instead of
	 * blocking, the caller is responsible for checking the
	 * interruption state of the query executor when getting this
	 * status.
	 */
	if (bt_query_executor_is_interrupted(query_exec)) {
		BT_LIB_LOGD("Query executor is interrupted: "
			"not performing the query operation: "
			"query-exec-addr=%p, %![cc-]+C, object=\"%s\", "
			"%![params-]+v, log-level=%s",
			query_exec, query_exec->comp_cls,
			query_exec->object->str, query_exec->params,
			bt_common_logging_level_string(query_exec->log_level));
		status = BT_FUNC_STATUS_AGAIN;
		goto end;
	}

	switch (query_exec->comp_cls->type) {
	case BT_COMPONENT_CLASS_TYPE_SOURCE:
	{
		struct bt_component_class_source *src_cc = (void *)
			query_exec->comp_cls;

		method = (method_t) src_cc->methods.query;
		break;
	}
	case BT_COMPONENT_CLASS_TYPE_FILTER:
	{
		struct bt_component_class_filter *flt_cc = (void *)
			query_exec->comp_cls;

		method = (method_t) flt_cc->methods.query;
		break;
	}
	case BT_COMPONENT_CLASS_TYPE_SINK:
	{
		struct bt_component_class_sink *sink_cc = (void *)
			query_exec->comp_cls;

		method = (method_t) sink_cc->methods.query;
		break;
	}
	default:
		bt_common_abort();
	}

	if (!method) {
		/* Not an error: nothing to query */
		BT_LIB_LOGD("Component class has no registered query method: "
			"%!+C", query_exec->comp_cls);
		status = BT_FUNC_STATUS_UNKNOWN_OBJECT;
		goto end;
	}

	BT_LIB_LOGD("Calling user's query method: "
		"query-exec-addr=%p, %![cc-]+C, object=\"%s\", %![params-]+v, "
		"log-level=%s",
		query_exec, query_exec->comp_cls, query_exec->object->str,
		query_exec->params,
		bt_common_logging_level_string(query_exec->log_level));
	*user_result = NULL;
	query_status = method((void *) query_exec->comp_cls,
		(void *) query_exec, query_exec->object->str,
		query_exec->params, query_exec->method_data, user_result);
	BT_LIB_LOGD("User method returned: status=%s, %![res-]+v",
		bt_common_func_status_string(query_status), *user_result);
	BT_ASSERT_POST(query_status != BT_FUNC_STATUS_OK || *user_result,
		"User method returned `BT_FUNC_STATUS_OK` without a result.");
	BT_ASSERT_POST_NO_ERROR_IF_NO_ERROR_STATUS(query_status);
	status = (int) query_status;

	if (status < 0) {
		BT_LIB_LOGW_APPEND_CAUSE(
			"Component class's \"query\" method failed: "
			"query-exec-addr=%p, %![cc-]+C, object=\"%s\", "
			"%![params-]+v, log-level=%s", query_exec,
			query_exec->comp_cls, query_exec->object->str,
			query_exec->params,
			bt_common_logging_level_string(query_exec->log_level));
		goto end;
	}

end:
	return status;
}

enum bt_query_executor_add_interrupter_status bt_query_executor_add_interrupter(
		struct bt_query_executor *query_exec,
		const struct bt_interrupter *intr)
{
	BT_ASSERT_PRE_NO_ERROR();
	BT_ASSERT_PRE_NON_NULL(query_exec, "Query executor");
	BT_ASSERT_PRE_NON_NULL(intr, "Interrupter");
	g_ptr_array_add(query_exec->interrupters, (void *) intr);
	bt_object_get_ref_no_null_check(intr);
	BT_LIB_LOGD("Added interrupter to query executor: "
		"query-exec-addr=%p, %![intr-]+z",
		query_exec, intr);
	return BT_FUNC_STATUS_OK;
}

bt_bool bt_query_executor_is_interrupted(const struct bt_query_executor *query_exec)
{
	BT_ASSERT_PRE_NON_NULL(query_exec, "Query executor");
	return (bt_bool) bt_interrupter_array_any_is_set(
		query_exec->interrupters);
}

struct bt_interrupter *bt_query_executor_borrow_default_interrupter(
		struct bt_query_executor *query_exec)
{
	BT_ASSERT_PRE_NON_NULL(query_exec, "Query executor");
	return query_exec->default_interrupter;
}

enum bt_query_executor_set_logging_level_status
bt_query_executor_set_logging_level(struct bt_query_executor *query_exec,
		enum bt_logging_level log_level)
{
	BT_ASSERT_PRE_NON_NULL(query_exec, "Query executor");
	query_exec->log_level = log_level;
	return BT_FUNC_STATUS_OK;
}

enum bt_logging_level bt_query_executor_get_logging_level(
		const struct bt_query_executor *query_exec)
{
	BT_ASSERT_PRE_NON_NULL(query_exec, "Query executor");
	return query_exec->log_level;
}

void bt_query_executor_get_ref(const struct bt_query_executor *query_executor)
{
	bt_object_get_ref(query_executor);
}

void bt_query_executor_put_ref(const struct bt_query_executor *query_executor)
{
	bt_object_put_ref(query_executor);
}
