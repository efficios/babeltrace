/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright 2017-2018 Philippe Proulx <pproulx@efficios.com>
 * Copyright 2015 Jérémie Galarneau <jeremie.galarneau@efficios.com>
 */

#ifndef BABELTRACE_GRAPH_COMPONENT_INTERNAL_H
#define BABELTRACE_GRAPH_COMPONENT_INTERNAL_H

#include "common/macros.h"
#include <babeltrace2/graph/component.h>
#include <babeltrace2/graph/component-class.h>
#include "lib/object.h"
#include <babeltrace2/types.h>
#include <babeltrace2/logging.h>
#include "common/assert.h"
#include <glib.h>
#include <stdbool.h>
#include <stdio.h>

#include "component-class.h"
#include "port.h"

#define BT_ASSERT_PRE_OUTPUT_PORT_NAME_UNIQUE(comp, name) 		\
	BT_ASSERT_PRE("output-port-is-unique",				\
		bt_component_port_name_is_unique(comp->output_ports, name), \
		"Output port name is not unique: name=\"%s\", %![comp-]c", name, comp);

typedef void (*bt_component_destroy_listener_func)(
		struct bt_component *class, void *data);

struct bt_component_destroy_listener {
	bt_component_destroy_listener_func func;
	void *data;
};

struct bt_graph;

struct bt_component {
	struct bt_object base;
	struct bt_component_class *class;
	GString *name;
	bt_logging_level log_level;

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
struct bt_graph *bt_component_borrow_graph(struct bt_component *comp)
{
	BT_ASSERT_DBG(comp);
	return (void *) bt_object_borrow_parent(&comp->base);
}

BT_HIDDEN
int bt_component_create(struct bt_component_class *component_class,
		const char *name, bt_logging_level log_level,
		struct bt_component **component);

BT_HIDDEN
enum bt_component_class_port_connected_method_status
bt_component_port_connected(
		struct bt_component *comp,
		struct bt_port *self_port, struct bt_port *other_port);

BT_HIDDEN
void bt_component_set_graph(struct bt_component *component,
		struct bt_graph *graph);

BT_HIDDEN
uint64_t bt_component_get_input_port_count(const struct bt_component *comp,
		const char *api_func);

BT_HIDDEN
uint64_t bt_component_get_output_port_count(const struct bt_component *comp,
		const char *api_func);

BT_HIDDEN
struct bt_port_input *bt_component_borrow_input_port_by_index(
		struct bt_component *comp, uint64_t index,
		const char *api_func);

BT_HIDDEN
struct bt_port_output *bt_component_borrow_output_port_by_index(
		struct bt_component *comp, uint64_t index,
		const char *api_func);

BT_HIDDEN
struct bt_port_input *bt_component_borrow_input_port_by_name(
		struct bt_component *comp, const char *name,
		const char *api_func);

BT_HIDDEN
struct bt_port_output *bt_component_borrow_output_port_by_name(
		struct bt_component *comp, const char *name,
		const char *api_func);

BT_HIDDEN
enum bt_self_component_add_port_status bt_component_add_input_port(
		struct bt_component *component, const char *name,
		void *user_data, struct bt_port **port,
		const char *api_func);

BT_HIDDEN
enum bt_self_component_add_port_status bt_component_add_output_port(
		struct bt_component *component, const char *name,
		void *user_data, struct bt_port **port,
		const char *api_func);

BT_HIDDEN
bool bt_component_port_name_is_unique(GPtrArray *ports, const char *name);

BT_HIDDEN
void bt_component_remove_port(struct bt_component *component,
		struct bt_port *port);

BT_HIDDEN
void bt_component_add_destroy_listener(struct bt_component *component,
		bt_component_destroy_listener_func func, void *data);

BT_HIDDEN
void bt_component_remove_destroy_listener(struct bt_component *component,
		bt_component_destroy_listener_func func, void *data);

#endif /* BABELTRACE_GRAPH_COMPONENT_INTERNAL_H */
