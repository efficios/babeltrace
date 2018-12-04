#ifndef BABELTRACE_GRAPH_COMPONENT_CLASS_INTERNAL_H
#define BABELTRACE_GRAPH_COMPONENT_CLASS_INTERNAL_H

/*
 * Copyright 2015 Jérémie Galarneau <jeremie.galarneau@efficios.com>
 * Copyright 2017 Philippe Proulx <pproulx@efficios.com>
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

#include <babeltrace/graph/component-const.h>
#include <babeltrace/graph/component-class.h>
#include <babeltrace/graph/component-class-source.h>
#include <babeltrace/graph/component-class-filter.h>
#include <babeltrace/graph/component-class-sink.h>
#include <babeltrace/babeltrace-internal.h>
#include <babeltrace/object-internal.h>
#include <babeltrace/list-internal.h>
#include <babeltrace/types.h>
#include <glib.h>

struct bt_component_class;
struct bt_plugin_so_shared_lib_handle;

typedef void (*bt_component_class_destroy_listener_func)(
		struct bt_component_class *class, void *data);

struct bt_component_class_destroy_listener {
	bt_component_class_destroy_listener_func func;
	void *data;
};

struct bt_component_class {
	struct bt_object base;
	enum bt_component_class_type type;
	GString *name;
	GString *description;
	GString *help;

	/* Array of struct bt_component_class_destroy_listener */
	GArray *destroy_listeners;
	bool frozen;
	struct bt_list_head node;
	struct bt_plugin_so_shared_lib_handle *so_handle;
};

struct bt_component_class_source {
	struct bt_component_class parent;
	struct {
		bt_component_class_source_init_method init;
		bt_component_class_source_finalize_method finalize;
		bt_component_class_source_notification_iterator_init_method notif_iter_init;
		bt_component_class_source_notification_iterator_finalize_method notif_iter_finalize;
		bt_component_class_source_notification_iterator_next_method notif_iter_next;
		bt_component_class_source_query_method query;
		bt_component_class_source_accept_output_port_connection_method accept_output_port_connection;
		bt_component_class_source_output_port_connected_method output_port_connected;
		bt_component_class_source_output_port_disconnected_method output_port_disconnected;
	} methods;
};

struct bt_component_class_sink {
	struct bt_component_class parent;
	struct {
		bt_component_class_sink_init_method init;
		bt_component_class_sink_finalize_method finalize;
		bt_component_class_sink_query_method query;
		bt_component_class_sink_accept_input_port_connection_method accept_input_port_connection;
		bt_component_class_sink_input_port_connected_method input_port_connected;
		bt_component_class_sink_input_port_disconnected_method input_port_disconnected;
		bt_component_class_sink_consume_method consume;
	} methods;
};

struct bt_component_class_filter {
	struct bt_component_class parent;
	struct {
		bt_component_class_filter_init_method init;
		bt_component_class_filter_finalize_method finalize;
		bt_component_class_filter_notification_iterator_init_method notif_iter_init;
		bt_component_class_filter_notification_iterator_finalize_method notif_iter_finalize;
		bt_component_class_filter_notification_iterator_next_method notif_iter_next;
		bt_component_class_filter_query_method query;
		bt_component_class_filter_accept_input_port_connection_method accept_input_port_connection;
		bt_component_class_filter_accept_output_port_connection_method accept_output_port_connection;
		bt_component_class_filter_input_port_connected_method input_port_connected;
		bt_component_class_filter_output_port_connected_method output_port_connected;
		bt_component_class_filter_input_port_disconnected_method input_port_disconnected;
		bt_component_class_filter_output_port_disconnected_method output_port_disconnected;
	} methods;
};

BT_HIDDEN
void bt_component_class_add_destroy_listener(struct bt_component_class *class,
		bt_component_class_destroy_listener_func func, void *data);

BT_HIDDEN
void _bt_component_class_freeze(
		const struct bt_component_class *component_class);

#ifdef BT_DEV_MODE
# define bt_component_class_freeze	_bt_component_class_freeze
#else
# define bt_component_class_freeze(_cc)
#endif

static inline
const char *bt_component_class_type_string(enum bt_component_class_type type)
{
	switch (type) {
	case BT_COMPONENT_CLASS_TYPE_SOURCE:
		return "BT_COMPONENT_CLASS_TYPE_SOURCE";
	case BT_COMPONENT_CLASS_TYPE_SINK:
		return "BT_COMPONENT_CLASS_TYPE_SINK";
	case BT_COMPONENT_CLASS_TYPE_FILTER:
		return "BT_COMPONENT_CLASS_TYPE_FILTER";
	default:
		return "(unknown)";
	}
}

#endif /* BABELTRACE_GRAPH_COMPONENT_CLASS_INTERNAL_H */
