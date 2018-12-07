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

#define BT_LOG_TAG "COMP-SOURCE"
#include <babeltrace/lib-logging-internal.h>

#include <babeltrace/assert-internal.h>
#include <babeltrace/assert-pre-internal.h>
#include <babeltrace/compiler-internal.h>
#include <babeltrace/graph/self-component-source.h>
#include <babeltrace/graph/component-source-const.h>
#include <babeltrace/graph/component-source-internal.h>
#include <babeltrace/graph/component-internal.h>
#include <babeltrace/graph/port-internal.h>
#include <babeltrace/graph/notification-iterator.h>
#include <babeltrace/graph/notification-iterator-internal.h>
#include <babeltrace/graph/graph.h>

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
		BT_LOGE_STR("Failed to allocate one source component.");
		goto end;
	}

end:
	return (void *) source;
}

uint64_t bt_component_source_get_output_port_count(
		const struct bt_component_source *comp)
{
	return bt_component_get_output_port_count((void *) comp);
}

const struct bt_port_output *
bt_component_source_borrow_output_port_by_name_const(
		const struct bt_component_source *comp, const char *name)
{
	return bt_component_borrow_output_port_by_name((void *) comp, name);
}

struct bt_self_component_port_output *
bt_self_component_source_borrow_output_port_by_name(
		struct bt_self_component_source *comp, const char *name)
{
	return (void *) bt_component_borrow_output_port_by_name(
		(void *) comp, name);
}

const struct bt_port_output *
bt_component_source_borrow_output_port_by_index_const(
		const struct bt_component_source *comp, uint64_t index)
{
	return bt_component_borrow_output_port_by_index((void *) comp, index);
}

struct bt_self_component_port_output *
bt_self_component_source_borrow_output_port_by_index(
		struct bt_self_component_source *comp, uint64_t index)
{
	return (void *) bt_component_borrow_output_port_by_index(
		(void *) comp, index);
}

enum bt_self_component_status bt_self_component_source_add_output_port(
		struct bt_self_component_source *self_comp,
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
