/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright 2017-2018 Philippe Proulx <pproulx@efficios.com>
 * Copyright 2017 Jérémie Galarneau <jeremie.galarneau@efficios.com>
 */

#ifndef BABELTRACE_GRAPH_COMPONENT_DESCRIPTOR_SET_INTERNAL_H
#define BABELTRACE_GRAPH_COMPONENT_DESCRIPTOR_SET_INTERNAL_H

#include <babeltrace2/graph/graph.h>
#include <babeltrace2/graph/component-descriptor-set.h>
#include "common/macros.h"
#include "lib/object.h"
#include "common/assert.h"
#include "common/common.h"
#include <stdlib.h>
#include <glib.h>

#include "component.h"
#include "component-sink.h"
#include "connection.h"
#include "lib/func-status.h"

/*
 * This structure describes an eventual component instance.
 */
struct bt_component_descriptor_set_entry {
	/* Owned by this */
	struct bt_component_class *comp_cls;

	/* Owned by this */
	struct bt_value *params;

	void *init_method_data;
};

struct bt_component_descriptor_set {
	struct bt_object base;

	/* Array of `struct bt_component_descriptor_set_entry *` */
	GPtrArray *sources;

	/* Array of `struct bt_component_descriptor_set_entry *` */
	GPtrArray *filters;

	/* Array of `struct bt_component_descriptor_set_entry *` */
	GPtrArray *sinks;
};

#endif /* BABELTRACE_GRAPH_COMPONENT_DESCRIPTOR_SET_INTERNAL_H */
