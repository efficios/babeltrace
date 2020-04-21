/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright 2017-2018 Philippe Proulx <pproulx@efficios.com>
 * Copyright 2016 Jérémie Galarneau <jeremie.galarneau@efficios.com>
 */

#define BT_LOG_TAG "LIB/COMPONENT-FILTER"
#include "lib/logging.h"

#include "common/assert.h"
#include "lib/assert-cond.h"
#include "compat/compiler.h"
#include <babeltrace2/value.h>
#include <babeltrace2/graph/self-component.h>
#include <babeltrace2/graph/component.h>
#include <babeltrace2/graph/graph.h>

#include "component-filter.h"
#include "component.h"
#include "component-class.h"
#include "lib/func-status.h"

BT_HIDDEN
void bt_component_filter_destroy(struct bt_component *component)
{
}

BT_HIDDEN
struct bt_component *bt_component_filter_create(
		const struct bt_component_class *class)
{
	struct bt_component_filter *filter = NULL;

	filter = g_new0(struct bt_component_filter, 1);
	if (!filter) {
		BT_LIB_LOGE_APPEND_CAUSE(
			"Failed to allocate one filter component.");
		goto end;
	}

end:
	return (void *) filter;
}

const bt_component_class_filter *
bt_component_filter_borrow_class_const(
		const bt_component_filter *component)
{
	struct bt_component_class *cls;

	BT_ASSERT_PRE_DEV_COMP_NON_NULL(component);

	cls = component->parent.class;

	BT_ASSERT_DBG(cls);
	BT_ASSERT_DBG(cls->type == BT_COMPONENT_CLASS_TYPE_FILTER);

	return (bt_component_class_filter *) cls;
}

uint64_t bt_component_filter_get_output_port_count(
		const struct bt_component_filter *comp)
{
	/* bt_component_get_output_port_count() checks preconditions */
	return bt_component_get_output_port_count((void *) comp, __func__);
}

const struct bt_port_output *
bt_component_filter_borrow_output_port_by_name_const(
		const struct bt_component_filter *comp, const char *name)
{
	/*
	 * bt_component_borrow_output_port_by_name() logs details/errors
	 * and checks preconditions.
	 */
	return bt_component_borrow_output_port_by_name(
		(void *) comp, name, __func__);
}

struct bt_self_component_port_output *
bt_self_component_filter_borrow_output_port_by_name(
		struct bt_self_component_filter *comp, const char *name)
{
	/*
	 * bt_component_borrow_output_port_by_name() logs details/errors
	 * and checks preconditions.
	 */
	return (void *) bt_component_borrow_output_port_by_name(
		(void *) comp, name, __func__);
}

const struct bt_port_output *
bt_component_filter_borrow_output_port_by_index_const(
		const struct bt_component_filter *comp, uint64_t index)
{
	/*
	 * bt_component_borrow_output_port_by_index() logs
	 * details/errors and checks preconditions.
	 */
	return bt_component_borrow_output_port_by_index(
		(void *) comp, index, __func__);
}

struct bt_self_component_port_output *
bt_self_component_filter_borrow_output_port_by_index(
		struct bt_self_component_filter *comp, uint64_t index)
{
	/*
	 * bt_component_borrow_output_port_by_index() logs
	 * details/errors and checks preconditions.
	 */
	return (void *) bt_component_borrow_output_port_by_index(
		(void *) comp, index, __func__);
}

enum bt_self_component_add_port_status bt_self_component_filter_add_output_port(
		struct bt_self_component_filter *self_comp,
		const char *name, void *user_data,
		struct bt_self_component_port_output **self_port)
{
	struct bt_component *comp = (void *) self_comp;
	enum bt_self_component_add_port_status status;
	struct bt_port *port = NULL;

	BT_ASSERT_PRE_NO_ERROR();

	/*
	 * bt_component_add_output_port() logs details/errors and checks
	 * preconditions.
	 */
	status = bt_component_add_output_port(comp, name, user_data, &port,
		__func__);
	if (status != BT_FUNC_STATUS_OK) {
		goto end;
	}

	if (self_port) {
		/* Move reference to user */
		*self_port = (void *) port;
	}

end:
	bt_object_put_ref(port);
	return status;
}

uint64_t bt_component_filter_get_input_port_count(
		const struct bt_component_filter *component)
{
	/* bt_component_get_input_port_count() checks preconditions */
	return bt_component_get_input_port_count((void *) component, __func__);
}

const struct bt_port_input *bt_component_filter_borrow_input_port_by_name_const(
		const struct bt_component_filter *component, const char *name)
{
	/*
	 * bt_component_borrow_input_port_by_name() logs details/errors
	 * and checks preconditions.
	 */
	return bt_component_borrow_input_port_by_name(
		(void *) component, name, __func__);
}

struct bt_self_component_port_input *
bt_self_component_filter_borrow_input_port_by_name(
		struct bt_self_component_filter *component, const char *name)
{
	/*
	 * bt_component_borrow_input_port_by_name() logs details/errors
	 * and checks preconditions.
	 */
	return (void *) bt_component_borrow_input_port_by_name(
		(void *) component, name, __func__);
}

const struct bt_port_input *
bt_component_filter_borrow_input_port_by_index_const(
		const struct bt_component_filter *component, uint64_t index)
{
	/*
	 * bt_component_borrow_input_port_by_index() logs details/errors
	 * and checks preconditions.
	 */
	return bt_component_borrow_input_port_by_index(
		(void *) component, index, __func__);
}

struct bt_self_component_port_input *
bt_self_component_filter_borrow_input_port_by_index(
		struct bt_self_component_filter *component, uint64_t index)
{
	/*
	 * bt_component_borrow_input_port_by_index() logs details/errors
	 * and checks preconditions.
	 */
	return (void *) bt_component_borrow_input_port_by_index(
		(void *) component, index, __func__);
}

enum bt_self_component_add_port_status bt_self_component_filter_add_input_port(
		struct bt_self_component_filter *self_comp,
		const char *name, void *user_data,
		struct bt_self_component_port_input **self_port)
{
	enum bt_self_component_add_port_status status;
	struct bt_port *port = NULL;
	struct bt_component *comp = (void *) self_comp;

	BT_ASSERT_PRE_NO_ERROR();

	/*
	 * bt_component_add_input_port() logs details/errors and checks
	 * preconditions.
	 */
	status = bt_component_add_input_port(comp, name, user_data, &port,
		__func__);
	if (status != BT_FUNC_STATUS_OK) {
		goto end;
	}

	if (self_port) {
		/* Move reference to user */
		*self_port = (void *) port;
	}

end:
	bt_object_put_ref(port);
	return status;
}

void bt_component_filter_get_ref(
		const struct bt_component_filter *component_filter)
{
	bt_object_get_ref(component_filter);
}

void bt_component_filter_put_ref(
		const struct bt_component_filter *component_filter)
{
	bt_object_put_ref(component_filter);
}
