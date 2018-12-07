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

#define BT_LOG_TAG "COMP-FILTER"
#include <babeltrace/lib-logging-internal.h>

#include <babeltrace/assert-internal.h>
#include <babeltrace/assert-pre-internal.h>
#include <babeltrace/compiler-internal.h>
#include <babeltrace/value.h>
#include <babeltrace/graph/self-component-filter.h>
#include <babeltrace/graph/component-filter-const.h>
#include <babeltrace/graph/component-filter-internal.h>
#include <babeltrace/graph/component-internal.h>
#include <babeltrace/graph/component-class-internal.h>
#include <babeltrace/graph/graph.h>

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
		BT_LOGE_STR("Failed to allocate one filter component.");
		goto end;
	}

end:
	return (void *) filter;
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

enum bt_self_component_status bt_self_component_filter_add_output_port(
		struct bt_self_component_filter *self_comp,
		const char *name, void *user_data,
		struct bt_self_component_port_output **self_port)
{
	struct bt_component *comp = (void *) self_comp;
	int status = BT_SELF_COMPONENT_STATUS_OK;
	struct bt_port *port = NULL;

	/* bt_component_add_output_port() logs details and errors */
	port = (void *) bt_component_add_output_port(comp, name, user_data);
	if (!port) {
		status = BT_SELF_COMPONENT_STATUS_NOMEM;
		goto end;
	}

	if (self_port) {
		/* Move reference to user */
		*self_port = (void *) port;
		port = NULL;
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

enum bt_self_component_status bt_self_component_filter_add_input_port(
		struct bt_self_component_filter *self_comp,
		const char *name, void *user_data,
		struct bt_self_component_port_input **self_port)
{
	int status = BT_SELF_COMPONENT_STATUS_OK;
	struct bt_port *port = NULL;
	struct bt_component *comp = (void *) self_comp;

	/* bt_component_add_input_port() logs details/errors */
	port = (void *) bt_component_add_input_port(comp, name, user_data);
	if (!port) {
		status = BT_SELF_COMPONENT_STATUS_NOMEM;
		goto end;
	}

	if (self_port) {
		/* Move reference to user */
		*self_port = (void *) port;
		port = NULL;
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
