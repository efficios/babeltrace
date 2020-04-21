/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright 2017-2018 Philippe Proulx <pproulx@efficios.com>
 * Copyright 2015 Jérémie Galarneau <jeremie.galarneau@efficios.com>
 */

#define BT_LOG_TAG "LIB/COMPONENT-SOURCE"
#include "lib/logging.h"

#include "common/assert.h"
#include "lib/assert-cond.h"
#include "compat/compiler.h"
#include <babeltrace2/graph/self-component.h>
#include <babeltrace2/graph/component.h>
#include <babeltrace2/graph/graph.h>

#include "component-source.h"
#include "component.h"
#include "port.h"
#include "message/iterator.h"
#include "lib/func-status.h"

BT_HIDDEN
void bt_component_source_destroy(struct bt_component *component)
{
}

BT_HIDDEN
struct bt_component *bt_component_source_create(
		const struct bt_component_class *class)
{
	struct bt_component_source *source = NULL;

	source = g_new0(struct bt_component_source, 1);
	if (!source) {
		BT_LIB_LOGE_APPEND_CAUSE(
			"Failed to allocate one source component.");
		goto end;
	}

end:
	return (void *) source;
}

const bt_component_class_source *
bt_component_source_borrow_class_const(
		const bt_component_source *component)
{
	struct bt_component_class *cls;

	BT_ASSERT_PRE_DEV_COMP_NON_NULL(component);

	cls = component->parent.class;

	BT_ASSERT_DBG(cls);
	BT_ASSERT_DBG(cls->type == BT_COMPONENT_CLASS_TYPE_SOURCE);

	return (bt_component_class_source *) cls;
}

uint64_t bt_component_source_get_output_port_count(
		const struct bt_component_source *comp)
{
	/* bt_component_get_output_port_count() checks preconditions */
	return bt_component_get_output_port_count((void *) comp, __func__);
}

const struct bt_port_output *
bt_component_source_borrow_output_port_by_name_const(
		const struct bt_component_source *comp, const char *name)
{
	/*
	 * bt_component_borrow_output_port_by_name() logs details/errors
	 * and checks preconditions.
	 */
	return bt_component_borrow_output_port_by_name((void *) comp, name,
		__func__);
}

struct bt_self_component_port_output *
bt_self_component_source_borrow_output_port_by_name(
		struct bt_self_component_source *comp, const char *name)
{
	/*
	 * bt_component_borrow_output_port_by_name() logs details/errors
	 * and checks preconditions.
	 */
	return (void *) bt_component_borrow_output_port_by_name(
		(void *) comp, name, __func__);
}

const struct bt_port_output *
bt_component_source_borrow_output_port_by_index_const(
		const struct bt_component_source *comp, uint64_t index)
{
	/*
	 * bt_component_borrow_output_port_by_index() logs
	 * details/errors and checks preconditions.
	 */
	return bt_component_borrow_output_port_by_index((void *) comp, index,
		__func__);
}

struct bt_self_component_port_output *
bt_self_component_source_borrow_output_port_by_index(
		struct bt_self_component_source *comp, uint64_t index)
{
	/*
	 * bt_component_borrow_output_port_by_index() logs
	 * details/errors and checks preconditions.
	 */
	return (void *) bt_component_borrow_output_port_by_index(
		(void *) comp, index, __func__);
}

enum bt_self_component_add_port_status bt_self_component_source_add_output_port(
		struct bt_self_component_source *self_comp,
		const char *name, void *user_data,
		struct bt_self_component_port_output **self_port)
{
	struct bt_component *comp = (void *) self_comp;
	enum bt_self_component_add_port_status status;
	struct bt_port *port = NULL;

	BT_ASSERT_PRE_NO_ERROR();
	BT_ASSERT_PRE_OUTPUT_PORT_NAME_UNIQUE(comp, name);

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

void bt_component_source_get_ref(
		const struct bt_component_source *component_source)
{
	bt_object_get_ref(component_source);
}

void bt_component_source_put_ref(
		const struct bt_component_source *component_source)
{
	bt_object_put_ref(component_source);
}
