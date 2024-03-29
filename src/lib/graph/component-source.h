/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright 2017-2018 Philippe Proulx <pproulx@efficios.com>
 * Copyright 2015 Jérémie Galarneau <jeremie.galarneau@efficios.com>
 */

#ifndef BABELTRACE_GRAPH_COMPONENT_SOURCE_INTERNAL_H
#define BABELTRACE_GRAPH_COMPONENT_SOURCE_INTERNAL_H

#include "component.h"

struct bt_component_source {
	struct bt_component parent;
};

struct bt_component *bt_component_source_create(void);

#endif /* BABELTRACE_GRAPH_COMPONENT_SOURCE_INTERNAL_H */
