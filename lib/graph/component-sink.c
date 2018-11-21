/*
 * Copyright 2015 Jérémie Galarneau <jeremie.galarneau@efficios.com>
 *
 * Author: Jérémie Galarneau <jeremie.galarneau@efficios.com>
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

#define BT_LOG_TAG "COMP-SINK"
#include <babeltrace/lib-logging-internal.h>

#include <babeltrace/compiler-internal.h>
#include <babeltrace/values.h>
#include <babeltrace/graph/self-component-sink.h>
#include <babeltrace/graph/component-sink.h>
#include <babeltrace/graph/component-sink-internal.h>
#include <babeltrace/graph/component-internal.h>
#include <babeltrace/graph/notification.h>
#include <babeltrace/graph/graph.h>
#include <babeltrace/object.h>
#include <babeltrace/assert-internal.h>
#include <babeltrace/assert-pre-internal.h>

BT_HIDDEN
void bt_component_sink_destroy(struct bt_component *component)
{
}

BT_HIDDEN
struct bt_component *bt_component_sink_create(struct bt_component_class *class)
{
	struct bt_component_sink *sink = NULL;

	sink = g_new0(struct bt_component_sink, 1);
	if (!sink) {
		BT_LOGE_STR("Failed to allocate one sink component.");
		goto end;
	}

end:
	return (void *) sink;
}

uint64_t bt_component_sink_get_input_port_count(
		struct bt_component_sink *component)
{
	/* bt_component_get_input_port_count() logs details/errors */
	return bt_component_get_input_port_count((void *) component);
}

struct bt_port_input *bt_component_sink_borrow_input_port_by_name(
		struct bt_component_sink *component, const char *name)
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

struct bt_port_input *bt_component_sink_borrow_input_port_by_index(
		struct bt_component_sink *component, uint64_t index)
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

enum bt_self_component_status bt_self_component_sink_add_input_port(
		struct bt_self_component_sink *self_comp,
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
