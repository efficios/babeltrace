/*
 * Copyright 2017-2018 Philippe Proulx <pproulx@efficios.com>
 * Copyright 2016 Jérémie Galarneau <jeremie.galarneau@efficios.com>
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

#define BT_LOG_TAG "LIB/COMPONENT-FILTER"
#include "lib/logging.h"

#include "common/assert.h"
#include "lib/assert-pre.h"
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

	BT_ASSERT_PRE_NO_ERROR();

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

	BT_ASSERT_PRE_DEV_NON_NULL(component, "Component");

	cls = component->parent.class;

	BT_ASSERT_DBG(cls);
	BT_ASSERT_DBG(cls->type == BT_COMPONENT_CLASS_TYPE_FILTER);

	return (bt_component_class_filter *) cls;
}

uint64_t bt_component_filter_get_output_port_count(
		const struct bt_component_filter *comp)
{
	return bt_component_get_output_port_count((void *) comp);
}

const struct bt_port_output *
bt_component_filter_borrow_output_port_by_name_const(
		const struct bt_component_filter *comp, const char *name)
{
	return bt_component_borrow_output_port_by_name(
		(void *) comp, name);
}

struct bt_self_component_port_output *
bt_self_component_filter_borrow_output_port_by_name(
		struct bt_self_component_filter *comp, const char *name)
{
	return (void *) bt_component_borrow_output_port_by_name(
		(void *) comp, name);
}

const struct bt_port_output *
bt_component_filter_borrow_output_port_by_index_const(
		const struct bt_component_filter *comp, uint64_t index)
{
	return bt_component_borrow_output_port_by_index(
		(void *) comp, index);
}

struct bt_self_component_port_output *
bt_self_component_filter_borrow_output_port_by_index(
		struct bt_self_component_filter *comp, uint64_t index)
{
	return (void *) bt_component_borrow_output_port_by_index(
		(void *) comp, index);
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

	/* bt_component_add_output_port() logs details and errors */
	status = bt_component_add_output_port(comp, name, user_data, &port);
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
	/* bt_component_get_input_port_count() logs details/errors */
	return bt_component_get_input_port_count((void *) component);
}

const struct bt_port_input *bt_component_filter_borrow_input_port_by_name_const(
		const struct bt_component_filter *component, const char *name)
{
	/* bt_component_borrow_input_port_by_name() logs details/errors */
	return bt_component_borrow_input_port_by_name(
		(void *) component, name);
}

struct bt_self_component_port_input *
bt_self_component_filter_borrow_input_port_by_name(
		struct bt_self_component_filter *component, const char *name)
{
	/* bt_component_borrow_input_port_by_name() logs details/errors */
	return (void *) bt_component_borrow_input_port_by_name(
		(void *) component, name);
}

const struct bt_port_input *
bt_component_filter_borrow_input_port_by_index_const(
		const struct bt_component_filter *component, uint64_t index)
{
	/* bt_component_borrow_input_port_by_index() logs details/errors */
	return bt_component_borrow_input_port_by_index(
		(void *) component, index);
}

struct bt_self_component_port_input *
bt_self_component_filter_borrow_input_port_by_index(
		struct bt_self_component_filter *component, uint64_t index)
{
	/* bt_component_borrow_input_port_by_index() logs details/errors */
	return (void *) bt_component_borrow_input_port_by_index(
		(void *) component, index);
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

	/* bt_component_add_input_port() logs details/errors */
	status = bt_component_add_input_port(comp, name, user_data, &port);
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
