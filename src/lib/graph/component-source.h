/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright 2017-2018 Philippe Proulx <pproulx@efficios.com>
 * Copyright 2015 Jérémie Galarneau <jeremie.galarneau@efficios.com>
 */

#ifndef BABELTRACE_GRAPH_COMPONENT_SOURCE_INTERNAL_H
#define BABELTRACE_GRAPH_COMPONENT_SOURCE_INTERNAL_H

#include "common/macros.h"

#include "component-class.h"
#include "component.h"

struct bt_component_source {
	struct bt_component parent;
};

BT_HIDDEN
struct bt_component *bt_component_source_create(
		const struct bt_component_class *class);

BT_HIDDEN
void bt_component_source_destroy(struct bt_component *component);

#endif /* BABELTRACE_GRAPH_COMPONENT_SOURCE_INTERNAL_H */
