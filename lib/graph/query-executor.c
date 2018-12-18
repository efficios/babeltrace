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

#define BT_LOG_TAG "QUERY-EXECUTOR"
#include <babeltrace/lib-logging-internal.h>

#include <babeltrace/assert-internal.h>
#include <babeltrace/assert-pre-internal.h>
#include <babeltrace/graph/query-executor-const.h>
#include <babeltrace/graph/query-executor.h>
#include <babeltrace/graph/query-executor-internal.h>
#include <babeltrace/graph/component-class.h>
#include <babeltrace/graph/component-class-internal.h>
#include <babeltrace/value.h>
#include <babeltrace/value-const.h>
#include <babeltrace/object-internal.h>
#include <babeltrace/compiler-internal.h>

static
void bt_query_executor_destroy(struct bt_object *obj)
{
	struct bt_query_executor *query_exec =
		container_of(obj, struct bt_query_executor, base);

	BT_LOGD("Destroying query executor: addr=%p", query_exec);
	g_free(query_exec);
}

struct bt_query_executor *bt_query_executor_create(void)
{
	struct bt_query_executor *query_exec;

	BT_LOGD_STR("Creating query executor.");
	query_exec = g_new0(struct bt_query_executor, 1);
	if (!query_exec) {
		BT_LOGE_STR("Failed to allocate one query executor.");
		goto end;
	}

	bt_object_init_shared(&query_exec->base,
		bt_query_executor_destroy);
	BT_LOGD("Created query executor: addr=%p", query_exec);

end:
	return (void *) query_exec;
}

enum bt_query_executor_status bt_query_executor_query(
		struct bt_query_executor *query_exec,
		const struct bt_component_class *comp_cls,
		const char *object, const struct bt_value *params,
		const struct bt_value **user_result)
{
	typedef enum bt_query_status (*method_t)(void *, const void *,
		const void *, const void *, const void *);

	enum bt_query_status status;
	enum bt_query_executor_status exec_status;
	method_t method = NULL;

	BT_ASSERT_PRE_NON_NULL(query_exec, "Query executor");
	BT_ASSERT_PRE_NON_NULL(comp_cls, "Component class");
	BT_ASSERT_PRE_NON_NULL(object, "Object");
	BT_ASSERT_PRE_NON_NULL(user_result, "Result (output)");
	BT_ASSERT_PRE(!query_exec->canceled, "Query executor is canceled.");

	if (!params) {
		params = bt_value_null;
	}

	switch (comp_cls->type) {
	case BT_COMPONENT_CLASS_TYPE_SOURCE:
	{
		struct bt_component_class_source *src_cc = (void *) comp_cls;

		method = (method_t) src_cc->methods.query;
		break;
	}
	case BT_COMPONENT_CLASS_TYPE_FILTER:
	{
		struct bt_component_class_filter *flt_cc = (void *) comp_cls;

		method = (method_t) flt_cc->methods.query;
		break;
	}
	case BT_COMPONENT_CLASS_TYPE_SINK:
	{
		struct bt_component_class_sink *sink_cc = (void *) comp_cls;

		method = (method_t) sink_cc->methods.query;
		break;
	}
	default:
		abort();
	}

	if (!method) {
		/* Not an error: nothing to query */
		BT_LIB_LOGD("Component class has no registered query method: "
			"%!+C", comp_cls);
		exec_status = BT_QUERY_EXECUTOR_STATUS_UNSUPPORTED;
		goto end;
	}

	BT_LIB_LOGD("Calling user's query method: "
		"query-exec-addr=%p, %![cc-]+C, object=\"%s\", %![params-]+v",
		query_exec, comp_cls, object, params);
	*user_result = NULL;
	status = method((void *) comp_cls, query_exec, object, params,
		user_result);
	BT_LIB_LOGD("User method returned: status=%s, %![res-]+v",
		bt_query_status_string(status), *user_result);
	BT_ASSERT_PRE(status != BT_QUERY_STATUS_OK || *user_result,
		"User method returned `BT_QUERY_STATUS_OK` without a result.");
	exec_status = (int) status;
	if (query_exec->canceled) {
		BT_OBJECT_PUT_REF_AND_RESET(*user_result);
		exec_status = BT_QUERY_EXECUTOR_STATUS_CANCELED;
		goto end;
	}

end:
	return exec_status;
}

enum bt_query_executor_status bt_query_executor_cancel(
		struct bt_query_executor *query_exec)
{
	BT_ASSERT_PRE_NON_NULL(query_exec, "Query executor");
	query_exec->canceled = BT_TRUE;
	BT_LOGV("Canceled query executor: addr=%p", query_exec);
	return BT_QUERY_EXECUTOR_STATUS_OK;
}

bt_bool bt_query_executor_is_canceled(const struct bt_query_executor *query_exec)
{
	BT_ASSERT_PRE_NON_NULL(query_exec, "Query executor");
	return query_exec->canceled;
}

void bt_query_executor_get_ref(const struct bt_query_executor *query_executor)
{
	bt_object_get_ref(query_executor);
}

void bt_query_executor_put_ref(const struct bt_query_executor *query_executor)
{
	bt_object_put_ref(query_executor);
}
