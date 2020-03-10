/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright 2017-2018 Philippe Proulx <pproulx@efficios.com>
 */

#ifndef BABELTRACE_GRAPH_QUERY_EXECUTOR_INTERNAL_H
#define BABELTRACE_GRAPH_QUERY_EXECUTOR_INTERNAL_H

#include <glib.h>

#include <babeltrace2/types.h>
#include <babeltrace2/graph/query-executor.h>
#include <babeltrace2/graph/component-class.h>

#include "lib/object.h"
#include "lib/value.h"

struct bt_query_executor {
	struct bt_object base;

	/*
	 * Array of `struct bt_interrupter *`, each one owned by this.
	 * If any interrupter is set, then this query executor is deemed
	 * interrupted.
	 */
	GPtrArray *interrupters;

	/*
	 * Default interrupter to support bt_query_executor_interrupt();
	 * owned by this.
	 */
	struct bt_interrupter *default_interrupter;

	/* Owned by this */
	const struct bt_component_class *comp_cls;

	GString *object;

	/* Owned by this */
	const struct bt_value *params;

	void *method_data;
	enum bt_logging_level log_level;
};

#endif /* BABELTRACE_GRAPH_QUERY_EXECUTOR_INTERNAL_H */
