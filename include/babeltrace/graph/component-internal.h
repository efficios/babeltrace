#ifndef BABELTRACE_COMPONENT_COMPONENT_INTERNAL_H
#define BABELTRACE_COMPONENT_COMPONENT_INTERNAL_H

/*
 * BabelTrace - Component internal
 *
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

#include <babeltrace/babeltrace-internal.h>
#include <babeltrace/graph/component.h>
#include <babeltrace/graph/component-class-internal.h>
#include <babeltrace/graph/port-internal.h>
#include <babeltrace/object-internal.h>
#include <babeltrace/types.h>
#include <glib.h>
#include <stdio.h>

typedef void (*bt_component_destroy_listener_func)(
		struct bt_component *class, void *data);

struct bt_component_destroy_listener {
	bt_component_destroy_listener_func func;
	void *data;
};

struct bt_component {
	struct bt_object base;
	struct bt_component_class *class;
	GString *name;

	/*
	 * Internal destroy function specific to a source, filter, or
	 * sink component object.
	 */
	void (*destroy)(struct bt_component *);

	/* User-defined data */
	void *user_data;

	/* Input and output ports (weak references) */
	GPtrArray *input_ports;
	GPtrArray *output_ports;

	/* Array of struct bt_component_destroy_listener */
	GArray *destroy_listeners;

	bool initialized;
};

static inline
struct bt_component *bt_component_borrow_from_private(
		struct bt_private_component *private_component)
{
	return (void *) private_component;
}

static inline
struct bt_private_component *bt_private_component_from_component(
		struct bt_component *component)
{
	return (void *) component;
}

static inline
struct bt_graph *bt_component_borrow_graph(struct bt_component *comp)
{
	assert(comp);
	return (void *) comp->base.parent;
}

BT_HIDDEN
enum bt_component_status bt_component_create(
		struct bt_component_class *component_class,
		const char *name, struct bt_component **component);

BT_HIDDEN
enum bt_component_status bt_component_accept_port_connection(
		struct bt_component *component, struct bt_port *self_port,
		struct bt_port *other_port);

BT_HIDDEN
void bt_component_port_connected(struct bt_component *comp,
		struct bt_port *self_port, struct bt_port *other_port);

BT_HIDDEN
void bt_component_port_disconnected(struct bt_component *comp,
		struct bt_port *port);

BT_HIDDEN
void bt_component_set_graph(struct bt_component *component,
		struct bt_graph *graph);

BT_HIDDEN
int64_t bt_component_get_input_port_count(struct bt_component *comp);

BT_HIDDEN
int64_t bt_component_get_output_port_count(struct bt_component *comp);

BT_HIDDEN
struct bt_port *bt_component_get_input_port_by_index(struct bt_component *comp,
		uint64_t index);

BT_HIDDEN
struct bt_port *bt_component_get_output_port_by_index(struct bt_component *comp,
		uint64_t index);

BT_HIDDEN
struct bt_port *bt_component_get_input_port_by_name(struct bt_component *comp,
		const char *name);

BT_HIDDEN
struct bt_port *bt_component_get_output_port_by_name(struct bt_component *comp,
		const char *name);

BT_HIDDEN
struct bt_port *bt_component_add_input_port(
		struct bt_component *component, const char *name,
		void *user_data);

BT_HIDDEN
struct bt_port *bt_component_add_output_port(
		struct bt_component *component, const char *name,
		void *user_data);

BT_HIDDEN
enum bt_component_status bt_component_remove_port(
		struct bt_component *component, struct bt_port *port);

BT_HIDDEN
void bt_component_add_destroy_listener(struct bt_component *component,
		bt_component_destroy_listener_func func, void *data);

BT_HIDDEN
void bt_component_remove_destroy_listener(struct bt_component *component,
		bt_component_destroy_listener_func func, void *data);

static inline
const char *bt_component_status_string(enum bt_component_status status)
{
	switch (status) {
	case BT_COMPONENT_STATUS_OK:
		return "BT_COMPONENT_STATUS_OK";
	case BT_COMPONENT_STATUS_END:
		return "BT_COMPONENT_STATUS_END";
	case BT_COMPONENT_STATUS_AGAIN:
		return "BT_COMPONENT_STATUS_AGAIN";
	case BT_COMPONENT_STATUS_REFUSE_PORT_CONNECTION:
		return "BT_COMPONENT_STATUS_REFUSE_PORT_CONNECTION";
	case BT_COMPONENT_STATUS_ERROR:
		return "BT_COMPONENT_STATUS_ERROR";
	case BT_COMPONENT_STATUS_UNSUPPORTED:
		return "BT_COMPONENT_STATUS_UNSUPPORTED";
	case BT_COMPONENT_STATUS_INVALID:
		return "BT_COMPONENT_STATUS_INVALID";
	case BT_COMPONENT_STATUS_NOMEM:
		return "BT_COMPONENT_STATUS_NOMEM";
	case BT_COMPONENT_STATUS_NOT_FOUND:
		return "BT_COMPONENT_STATUS_NOT_FOUND";
	case BT_COMPONENT_STATUS_GRAPH_IS_CANCELED:
		return "BT_COMPONENT_STATUS_GRAPH_IS_CANCELED";
	default:
		return "(unknown)";
	}
}

#endif /* BABELTRACE_COMPONENT_COMPONENT_INTERNAL_H */
