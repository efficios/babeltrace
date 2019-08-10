#ifndef BABELTRACE_GRAPH_COMPONENT_DESCRIPTOR_SET_INTERNAL_H
#define BABELTRACE_GRAPH_COMPONENT_DESCRIPTOR_SET_INTERNAL_H

/*
 * Copyright 2017-2018 Philippe Proulx <pproulx@efficios.com>
 * Copyright 2017 Jérémie Galarneau <jeremie.galarneau@efficios.com>
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
