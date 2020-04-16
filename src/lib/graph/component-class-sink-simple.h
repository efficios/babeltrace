/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright 2017-2019 Philippe Proulx <pproulx@efficios.com>
 */

#ifndef BABELTRACE_GRAPH_COMPONENT_CLASS_SINK_SIMPLE_H
#define BABELTRACE_GRAPH_COMPONENT_CLASS_SINK_SIMPLE_H

#include <stdint.h>
#include <babeltrace2/types.h>
#include <babeltrace2/graph/message.h>

struct simple_sink_init_method_data {
	bt_graph_simple_sink_component_initialize_func init_func;
	bt_graph_simple_sink_component_consume_func consume_func;
	bt_graph_simple_sink_component_finalize_func finalize_func;
	void *user_data;
};

BT_HIDDEN
struct bt_component_class_sink *bt_component_class_sink_simple_borrow(void);

#endif /* BABELTRACE_GRAPH_COMPONENT_CLASS_SINK_SIMPLE_H */
