/*
 * Copyright 2019 Philippe Proulx <pproulx@efficios.com>
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

#define BT_LOG_TAG "LIB/MIP"
#include "lib/logging.h"

#include "lib/assert-pre.h"
#include "lib/assert-post.h"
#include <stdbool.h>
#include <unistd.h>
#include <glib.h>
#include <babeltrace2/graph/graph.h>

#include "common/assert.h"
#include "compat/compiler.h"
#include "common/common.h"
#include "lib/value.h"
#include "component-descriptor-set.h"
#include "lib/integer-range-set.h"

static
bool unsigned_integer_range_set_contains(
		const struct bt_integer_range_set *range_set, uint64_t value)
{
	bool contains = false;
	uint64_t i;

	BT_ASSERT(range_set);

	for (i = 0; i < range_set->ranges->len; i++) {
		const struct bt_integer_range *range =
			BT_INTEGER_RANGE_SET_RANGE_AT_INDEX(range_set, i);

		if (value >= range->lower.u && value <= range->upper.u) {
			contains = true;
			goto end;
		}
	}

end:
	return contains;
}

/*
 * As of this version, this function only validates that all the
 * component descriptors in `descriptors` support MIP version 0, which
 * is the only version supported by this library.
 *
 * If any component descriptor does not support MIP version 0, then this
 * function returns `BT_FUNC_STATUS_NO_MATCH`.
 */
static
int validate_operative_mip_version_in_array(GPtrArray *descriptors,
		enum bt_logging_level log_level)
{
	typedef bt_component_class_get_supported_mip_versions_method_status
		(*method_t)(
			void * /* component class */,
			const struct bt_value *,
			void * /* init method data */,
			enum bt_logging_level,
			struct bt_integer_range_set *);

	int status = BT_FUNC_STATUS_OK;
	uint64_t i;
	struct bt_integer_range_set *range_set = NULL;

	for (i = 0; i < descriptors->len; i++) {
		struct bt_component_descriptor_set_entry *descr =
			descriptors->pdata[i];
		method_t method = NULL;
		bt_component_class_get_supported_mip_versions_method_status method_status;

		switch (descr->comp_cls->type) {
		case BT_COMPONENT_CLASS_TYPE_SOURCE:
		{
			struct bt_component_class_source *src_cc = (void *)
				descr->comp_cls;

			method = (method_t) src_cc->methods.get_supported_mip_versions;
			break;
		}
		case BT_COMPONENT_CLASS_TYPE_FILTER:
		{
			struct bt_component_class_filter *flt_cc = (void *)
				descr->comp_cls;

			method = (method_t) flt_cc->methods.get_supported_mip_versions;
			break;
		}
		case BT_COMPONENT_CLASS_TYPE_SINK:
		{
			struct bt_component_class_sink *sink_cc = (void *)
				descr->comp_cls;

			method = (method_t) sink_cc->methods.get_supported_mip_versions;
			break;
		}
		default:
			bt_common_abort();
		}

		if (!method) {
			/* Assume 0 */
			continue;
		}

		range_set = (void *) bt_integer_range_set_unsigned_create();
		if (!range_set) {
			status = BT_FUNC_STATUS_MEMORY_ERROR;
			goto end;
		}

		BT_ASSERT(descr->params);
		BT_LIB_LOGD("Calling user's \"get supported MIP versions\" method: "
			"%![cc-]+C, %![params-]+v, init-method-data=%p, "
			"log-level=%s",
			descr->comp_cls, descr->params,
			descr->init_method_data,
			bt_common_logging_level_string(log_level));
		method_status = method(descr->comp_cls, descr->params,
			descr->init_method_data, log_level,
			range_set);
		BT_LIB_LOGD("User method returned: status=%s",
			bt_common_func_status_string(method_status));
		BT_ASSERT_POST(method_status != BT_FUNC_STATUS_OK ||
			range_set->ranges->len > 0,
			"User method returned `BT_FUNC_STATUS_OK` without "
			"adding a range to the supported MIP version range set.");
		BT_ASSERT_POST_NO_ERROR_IF_NO_ERROR_STATUS(method_status);
		if (method_status < 0) {
			BT_LIB_LOGW_APPEND_CAUSE(
				"Component class's \"get supported MIP versions\" method failed: "
				"%![cc-]+C, %![params-]+v, init-method-data=%p, "
				"log-level=%s",
				descr->comp_cls, descr->params,
				descr->init_method_data,
				bt_common_logging_level_string(log_level));
			status = (int) method_status;
			goto end;
		}

		if (!unsigned_integer_range_set_contains(range_set, 0)) {
			/*
			 * Supported MIP versions do not include 0,
			 * which is the only MIP versions currently
			 * supported by the library itself.
			 */
			status = BT_FUNC_STATUS_NO_MATCH;
			goto end;
		}

		BT_OBJECT_PUT_REF_AND_RESET(range_set);
	}

end:
	bt_object_put_ref(range_set);
	return status;
}

/*
 * The purpose of this function is eventually to find the greatest
 * common supported MIP version amongst all the component descriptors.
 * But as of this version of the library, only MIP version 0 is
 * supported, so it only checks that they all support MIP version 0 and
 * always sets `*operative_mip_version` to 0.
 *
 * When any component descriptor does not support MIP version 0, this
 * function returns `BT_FUNC_STATUS_NO_MATCH`.
 */
enum bt_get_greatest_operative_mip_version_status
bt_get_greatest_operative_mip_version(
		const struct bt_component_descriptor_set *comp_descr_set,
		enum bt_logging_level log_level,
		uint64_t *operative_mip_version)
{
	int status = BT_FUNC_STATUS_OK;

	BT_ASSERT_PRE_NO_ERROR();
	BT_ASSERT_PRE_NON_NULL(comp_descr_set, "Component descriptor set");
	BT_ASSERT_PRE_NON_NULL(operative_mip_version,
		"Operative MIP version (output)");
	BT_ASSERT_PRE(comp_descr_set->sources->len +
		comp_descr_set->filters->len +
		comp_descr_set->sinks->len > 0,
		"Component descriptor set is empty: addr=%p", comp_descr_set);
	status = validate_operative_mip_version_in_array(
		comp_descr_set->sources, log_level);
	if (status) {
		goto end;
	}

	status = validate_operative_mip_version_in_array(
		comp_descr_set->filters, log_level);
	if (status) {
		goto end;
	}

	status = validate_operative_mip_version_in_array(
		comp_descr_set->sinks, log_level);
	if (status) {
		goto end;
	}

	*operative_mip_version = 0;

end:
	return status;
}

uint64_t bt_get_maximal_mip_version(void)
{
	return 0;
}
