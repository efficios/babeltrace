/*
 * Copyright 2017-2018 Philippe Proulx <pproulx@efficios.com>
 * Copyright 2015 Jérémie Galarneau <jeremie.galarneau@efficios.com>
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

#define BT_LOG_TAG "LIB/COMPONENT-SINK"
#include "lib/logging.h"

#include "common/assert.h"
#include "lib/assert-pre.h"
#include "lib/assert-post.h"
#include "compat/compiler.h"
#include <babeltrace2/value.h>
#include <babeltrace2/graph/self-component.h>
#include <babeltrace2/graph/component.h>
#include <babeltrace2/graph/graph.h>

#include "component-sink.h"
#include "component.h"
#include "graph.h"
#include "lib/func-status.h"

BT_HIDDEN
void bt_component_sink_destroy(struct bt_component *component)
{
}

BT_HIDDEN
struct bt_component *bt_component_sink_create(
		const struct bt_component_class *class)
{
	struct bt_component_sink *sink = NULL;

	BT_ASSERT_PRE_NO_ERROR();

	sink = g_new0(struct bt_component_sink, 1);
	if (!sink) {
		BT_LIB_LOGE_APPEND_CAUSE(
			"Failed to allocate one sink component.");
		goto end;
	}

end:
	return (void *) sink;
}

const bt_component_class_sink *
bt_component_sink_borrow_class_const(
		const bt_component_sink *component)
{
	struct bt_component_class *cls;

	BT_ASSERT_PRE_DEV_NON_NULL(component, "Component");

	cls = component->parent.class;

	BT_ASSERT_DBG(cls);
	BT_ASSERT_DBG(cls->type == BT_COMPONENT_CLASS_TYPE_SINK);

	return (bt_component_class_sink *) cls;
}

uint64_t bt_component_sink_get_input_port_count(
		const struct bt_component_sink *component)
{
	/* bt_component_get_input_port_count() logs details/errors */
	return bt_component_get_input_port_count((void *) component);
}

const struct bt_port_input *
bt_component_sink_borrow_input_port_by_name_const(
		const struct bt_component_sink *component, const char *name)
{
	/* bt_component_borrow_input_port_by_name() logs details/errors */
	return bt_component_borrow_input_port_by_name((void *) component, name);
}

struct bt_self_component_port_input *
bt_self_component_sink_borrow_input_port_by_name(
		struct bt_self_component_sink *component, const char *name)
{
	/* bt_component_borrow_input_port_by_name() logs details/errors */
	return (void *) bt_component_borrow_input_port_by_name(
		(void *) component, name);
}

const struct bt_port_input *bt_component_sink_borrow_input_port_by_index_const(
		const struct bt_component_sink *component, uint64_t index)
{
	/* bt_component_borrow_input_port_by_index() logs details/errors */
	return bt_component_borrow_input_port_by_index(
		(void *) component, index);
}

struct bt_self_component_port_input *
bt_self_component_sink_borrow_input_port_by_index(
		struct bt_self_component_sink *component, uint64_t index)
{
	/* bt_component_borrow_input_port_by_index() logs details/errors */
	return (void *) bt_component_borrow_input_port_by_index(
		(void *) component, index);
}

enum bt_self_component_add_port_status bt_self_component_sink_add_input_port(
		struct bt_self_component_sink *self_comp,
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

bt_bool bt_self_component_sink_is_interrupted(
		const struct bt_self_component_sink *self_comp)
{
	struct bt_component *comp = (void *) self_comp;

	BT_ASSERT_PRE_NON_NULL(comp, "Component");
	return (bt_bool) bt_graph_is_interrupted(
		bt_component_borrow_graph(comp));
}

void bt_component_sink_get_ref(
		const struct bt_component_sink *component_sink)
{
	bt_object_get_ref(component_sink);
}

void bt_component_sink_put_ref(
		const struct bt_component_sink *component_sink)
{
	bt_object_put_ref(component_sink);
}
