/*
 * Copyright 2017-2019 Philippe Proulx <pproulx@efficios.com>
 * Copyright 2017 Jérémie Galarneau <jeremie.galarneau@efficios.com>
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

#define BT_LOG_TAG "LIB/COMP-DESCR-SET"
#include "lib/logging.h"

#include "common/assert.h"
#include "lib/assert-pre.h"
#include "compat/compiler.h"
#include "common/common.h"
#include <babeltrace2/types.h>
#include <babeltrace2/value.h>
#include <unistd.h>
#include <glib.h>

#include "component-class.h"
#include "component-descriptor-set.h"
#include "component-class-sink-simple.h"
#include "lib/value.h"

static
void destroy_component_descriptor_set(struct bt_object *obj)
{
	struct bt_component_descriptor_set *comp_descr_set = (void *) obj;

	if (comp_descr_set->sources) {
		BT_LOGD_STR("Destroying source component descriptors.");
		g_ptr_array_free(comp_descr_set->sources, TRUE);
		comp_descr_set->sources = NULL;
	}

	if (comp_descr_set->filters) {
		BT_LOGD_STR("Destroying filter component descriptors.");
		g_ptr_array_free(comp_descr_set->filters, TRUE);
		comp_descr_set->filters = NULL;
	}

	if (comp_descr_set->sinks) {
		BT_LOGD_STR("Destroying sink component descriptors.");
		g_ptr_array_free(comp_descr_set->sinks, TRUE);
		comp_descr_set->sinks = NULL;
	}

	g_free(comp_descr_set);
}

static
void destroy_component_descriptor_set_entry(gpointer ptr)
{
	struct bt_component_descriptor_set_entry *entry = ptr;

	if (!ptr) {
		goto end;
	}

	BT_OBJECT_PUT_REF_AND_RESET(entry->comp_cls);
	BT_OBJECT_PUT_REF_AND_RESET(entry->params);
	g_free(entry);

end:
	return;
}

struct bt_component_descriptor_set *bt_component_descriptor_set_create(void)
{
	struct bt_component_descriptor_set *comp_descr_set;

	BT_ASSERT_PRE_NO_ERROR();

	BT_LOGI_STR("Creating component descriptor set object.");
	comp_descr_set = g_new0(struct bt_component_descriptor_set, 1);
	if (!comp_descr_set) {
		BT_LIB_LOGE_APPEND_CAUSE(
			"Failed to allocate one component descriptor set.");
		goto end;
	}

	bt_object_init_shared(&comp_descr_set->base,
		destroy_component_descriptor_set);
	comp_descr_set->sources = g_ptr_array_new_with_free_func(
		destroy_component_descriptor_set_entry);
	if (!comp_descr_set->sources) {
		BT_LIB_LOGE_APPEND_CAUSE("Failed to allocate one GPtrArray.");
		goto error;
	}

	comp_descr_set->filters = g_ptr_array_new_with_free_func(
		destroy_component_descriptor_set_entry);
	if (!comp_descr_set->filters) {
		BT_LIB_LOGE_APPEND_CAUSE("Failed to allocate one GPtrArray.");
		goto error;
	}

	comp_descr_set->sinks = g_ptr_array_new_with_free_func(
		destroy_component_descriptor_set_entry);
	if (!comp_descr_set->sinks) {
		BT_LIB_LOGE_APPEND_CAUSE("Failed to allocate one GPtrArray.");
		goto error;
	}

	BT_LOGI("Created component descriptor set object: addr=%p",
		comp_descr_set);
	goto end;

error:
	BT_OBJECT_PUT_REF_AND_RESET(comp_descr_set);

end:
	return comp_descr_set;
}

enum bt_component_descriptor_set_add_descriptor_status
bt_component_descriptor_set_add_descriptor_with_initialize_method_data(
		struct bt_component_descriptor_set *comp_descr_set,
		const struct bt_component_class *comp_cls,
		const struct bt_value *params, void *init_method_data)
{
	bt_component_descriptor_set_add_descriptor_status status =
		BT_FUNC_STATUS_OK;
	struct bt_value *new_params = NULL;
	struct bt_component_descriptor_set_entry *entry = NULL;
	GPtrArray *comp_descr_array = NULL;

	BT_ASSERT_PRE_NO_ERROR();
	BT_ASSERT_PRE_NON_NULL(comp_descr_set, "Component descriptor set");
	BT_ASSERT_PRE_NON_NULL(comp_cls, "Component class");
	BT_ASSERT_PRE(!params || bt_value_is_map(params),
		"Parameter value is not a map value: %!+v", params);
	BT_LIB_LOGI("Adding component descriptor to set: "
		"set-addr=%p, %![cc-]+C, "
		"%![params-]+v, init-method-data-addr=%p",
		comp_descr_set, comp_cls, params, init_method_data);

	if (!params) {
		new_params = bt_value_map_create();
		if (!new_params) {
			BT_LIB_LOGE_APPEND_CAUSE(
				"Cannot create empty map value object.");
			status = BT_FUNC_STATUS_MEMORY_ERROR;
			goto error;
		}

		params = new_params;
	}

	entry = g_new0(struct bt_component_descriptor_set_entry, 1);
	if (!entry) {
		BT_LIB_LOGE_APPEND_CAUSE("Failed to allocate a GPtrArray.");
		status = BT_FUNC_STATUS_MEMORY_ERROR;
		goto error;
	}

	entry->comp_cls = (void *) comp_cls;
	bt_object_get_ref_no_null_check(entry->comp_cls);
	bt_component_class_freeze(entry->comp_cls);
	entry->params = (void *) params;
	bt_object_get_ref_no_null_check(entry->params);
	bt_value_freeze(entry->params);
	entry->init_method_data = init_method_data;

	/* Move to array */
	switch (comp_cls->type) {
	case BT_COMPONENT_CLASS_TYPE_SOURCE:
		comp_descr_array = comp_descr_set->sources;
		break;
	case BT_COMPONENT_CLASS_TYPE_FILTER:
		comp_descr_array = comp_descr_set->filters;
		break;
	case BT_COMPONENT_CLASS_TYPE_SINK:
		comp_descr_array = comp_descr_set->sinks;
		break;
	default:
		bt_common_abort();
	}

	BT_ASSERT(comp_descr_array);
	g_ptr_array_add(comp_descr_array, entry);
	BT_LIB_LOGI("Added component descriptor to set: "
		"set-addr=%p, %![cc-]+C, "
		"%![params-]+v, init-method-data-addr=%p",
		comp_descr_set, comp_cls, params, init_method_data);
	goto end;

error:
	destroy_component_descriptor_set_entry(entry);
	entry = NULL;

end:
	bt_object_put_ref(new_params);
	return status;
}

enum bt_component_descriptor_set_add_descriptor_status
bt_component_descriptor_set_add_descriptor(
		struct bt_component_descriptor_set *comp_descr_set,
		const struct bt_component_class *comp_cls,
		const struct bt_value *params)
{
	BT_ASSERT_PRE_NO_ERROR();

	return bt_component_descriptor_set_add_descriptor_with_initialize_method_data(
		comp_descr_set, comp_cls, params, NULL);
}

void bt_component_descriptor_set_get_ref(
		const struct bt_component_descriptor_set *comp_descr_set)
{
	bt_object_get_ref(comp_descr_set);
}

void bt_component_descriptor_set_put_ref(
		const struct bt_component_descriptor_set *comp_descr_set)
{
	bt_object_put_ref(comp_descr_set);
}
