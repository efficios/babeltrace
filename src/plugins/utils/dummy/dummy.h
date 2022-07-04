/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright 2017 Philippe Proulx <pproulx@efficios.com>
 */

#ifndef BABELTRACE_PLUGINS_UTILS_DUMMY_H
#define BABELTRACE_PLUGINS_UTILS_DUMMY_H

#include <glib.h>
#include <babeltrace2/babeltrace.h>
#include "common/macros.h"

struct dummy {
	bt_message_iterator *msg_iter;
};

bt_component_class_initialize_method_status dummy_init(
		bt_self_component_sink *component,
		bt_self_component_sink_configuration *config,
		const bt_value *params, void *init_method_data);

void dummy_finalize(bt_self_component_sink *component);

bt_component_class_sink_graph_is_configured_method_status dummy_graph_is_configured(
		bt_self_component_sink *comp);

bt_component_class_sink_consume_method_status dummy_consume(
		bt_self_component_sink *component);

#endif /* BABELTRACE_PLUGINS_UTILS_DUMMY_H */
