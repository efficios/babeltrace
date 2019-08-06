#ifndef BABELTRACE_GRAPH_QUERY_EXECUTOR_INTERNAL_H
#define BABELTRACE_GRAPH_QUERY_EXECUTOR_INTERNAL_H

/*
 * Copyright 2017-2018 Philippe Proulx <pproulx@efficios.com>
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
