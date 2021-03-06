/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright 2017-2018 Philippe Proulx <pproulx@efficios.com>
 * Copyright 2015 Jérémie Galarneau <jeremie.galarneau@efficios.com>
 */

#ifndef BABELTRACE_GRAPH_COMPONENT_SINK_INTERNAL_H
#define BABELTRACE_GRAPH_COMPONENT_SINK_INTERNAL_H

#include <stdbool.h>

#include "common/macros.h"
#include "compat/compiler.h"
#include <babeltrace2/graph/component.h>

#include "component-class.h"
#include "component.h"

struct bt_component_sink {
	struct bt_component parent;
	bool graph_is_configured_method_called;
};

BT_HIDDEN
struct bt_component *bt_component_sink_create(
		const struct bt_component_class *class);

BT_HIDDEN
void bt_component_sink_destroy(struct bt_component *component);

#endif /* BABELTRACE_GRAPH_COMPONENT_SINK_INTERNAL_H */
