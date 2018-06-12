/*
 * Copyright 2017 Philippe Proulx <pproulx@efficios.com>
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

#include <babeltrace/graph/query-executor.h>
#include <babeltrace/graph/query-executor-internal.h>
#include <babeltrace/graph/component-class.h>
#include <babeltrace/graph/component-class-internal.h>
#include <babeltrace/values.h>
#include <babeltrace/object-internal.h>
#include <babeltrace/compiler-internal.h>

static
void bt_query_executor_destroy(struct bt_object *obj)
{
	struct bt_query_executor *query_exec =
		container_of(obj, struct bt_query_executor, base);

	BT_LOGD("Destroying port: addr=%p", query_exec);
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
	return query_exec;
}

enum bt_query_status bt_query_executor_query(
		struct bt_query_executor *query_exec,
		struct bt_component_class *component_class,
		const char *object, struct bt_value *params,
		struct bt_value **user_result)
{
	struct bt_component_class_query_method_return ret = {
		.result = NULL,
		.status = BT_QUERY_STATUS_OK,
	};

	if (!query_exec) {
		BT_LOGW_STR("Invalid parameter: query executor is NULL.");
		ret.status = BT_QUERY_STATUS_INVALID;
		goto end;
	}

	if (query_exec->canceled) {
		BT_LOGW_STR("Invalid parameter: query executor is canceled.");
		ret.status = BT_QUERY_STATUS_EXECUTOR_CANCELED;
		goto end;
	}

	if (!component_class) {
		BT_LOGW_STR("Invalid parameter: component class is NULL.");
		ret.status = BT_QUERY_STATUS_INVALID;
		goto end;
	}

	if (!object) {
		BT_LOGW_STR("Invalid parameter: object string is NULL.");
		ret.status = BT_QUERY_STATUS_INVALID;
		goto end;
	}

	if (!params) {
		params = bt_value_null;
	}

	if (!component_class->methods.query) {
		/* Not an error: nothing to query */
		BT_LOGD("Component class has no registered query method: "
			"addr=%p, name=\"%s\", type=%s",
			component_class,
			bt_component_class_get_name(component_class),
			bt_component_class_type_string(component_class->type));
		ret.status = BT_QUERY_STATUS_ERROR;
		goto end;
	}

	BT_LOGD("Calling user's query method: "
		"query-exec-addr=%p, comp-class-addr=%p, "
		"comp-class-name=\"%s\", comp-class-type=%s, "
		"object=\"%s\", params-addr=%p",
		query_exec, component_class,
		bt_component_class_get_name(component_class),
		bt_component_class_type_string(component_class->type),
		object, params);
	ret = component_class->methods.query(component_class, query_exec,
		object, params);
	BT_LOGD("User method returned: status=%s, result-addr=%p",
		bt_query_status_string(ret.status), ret.result);
	if (query_exec->canceled) {
		BT_PUT(ret.result);
		ret.status = BT_QUERY_STATUS_EXECUTOR_CANCELED;
		goto end;
	} else {
		if (ret.status == BT_QUERY_STATUS_EXECUTOR_CANCELED) {
			/*
			 * The user cannot decide that the executor is
			 * canceled if it's not.
			 */
			BT_PUT(ret.result);
			ret.status = BT_QUERY_STATUS_ERROR;
			goto end;
		}
	}

	switch (ret.status) {
	case BT_QUERY_STATUS_INVALID:
		/*
		 * This is reserved for invalid parameters passed to
		 * this function.
		 */
		BT_PUT(ret.result);
		ret.status = BT_QUERY_STATUS_ERROR;
		break;
	case BT_QUERY_STATUS_OK:
		if (!ret.result) {
			ret.result = bt_value_null;
		}
		break;
	default:
		if (ret.result) {
			BT_LOGW("User method did not return BT_QUERY_STATUS_OK, but result is not NULL: "
				"status=%s, result-addr=%p",
				bt_query_status_string(ret.status), ret.result);
			BT_PUT(ret.result);
		}
	}

end:
	if (user_result) {
		*user_result = ret.result;
		ret.result = NULL;
	}

	bt_put(ret.result);
	return ret.status;
}

enum bt_query_status bt_query_executor_cancel(
		struct bt_query_executor *query_exec)
{
	enum bt_query_status ret = BT_QUERY_STATUS_OK;

	if (!query_exec) {
		BT_LOGW_STR("Invalid parameter: query executor is NULL.");
		ret = BT_QUERY_STATUS_INVALID;
		goto end;
	}

	query_exec->canceled = BT_TRUE;
	BT_LOGV("Canceled query executor: addr=%p", query_exec);

end:
	return ret;
}

bt_bool bt_query_executor_is_canceled(struct bt_query_executor *query_exec)
{
	bt_bool canceled = BT_FALSE;

	if (!query_exec) {
		BT_LOGW_STR("Invalid parameter: query executor is NULL.");
		goto end;
	}

	canceled = query_exec->canceled;

end:
	return canceled;
}
