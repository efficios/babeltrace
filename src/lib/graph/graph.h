#ifndef BABELTRACE_GRAPH_GRAPH_INTERNAL_H
#define BABELTRACE_GRAPH_GRAPH_INTERNAL_H

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

/* Protection: this file uses BT_LIB_LOG*() macros directly */
#ifndef BT_LIB_LOG_SUPPORTED
# error Please include "lib/logging.h" before including this file.
#endif

#include <babeltrace2/graph/graph.h>
#include <babeltrace2/graph/message.h>
#include "common/macros.h"
#include "lib/object.h"
#include "lib/object-pool.h"
#include "common/assert.h"
#include "common/common.h"
#include <stdbool.h>
#include <stdlib.h>
#include <glib.h>

#include "component.h"
#include "component-sink.h"
#include "connection.h"
#include "lib/func-status.h"

/* Protection: this file uses BT_LIB_LOG*() macros directly */
#ifndef BT_LIB_LOG_SUPPORTED
# error Please include "lib/logging.h" before including this file.
#endif

/* Protection: this file uses BT_ASSERT_PRE*() macros directly */
#ifndef BT_ASSERT_PRE_SUPPORTED
# error Please include "lib/assert-pre.h" before including this file.
#endif

/* Protection: this file uses BT_ASSERT_POST*() macros directly */
#ifndef BT_ASSERT_POST_SUPPORTED
# error Please include "lib/assert-post.h" before including this file.
#endif

struct bt_component;
struct bt_port;

enum bt_graph_configuration_state {
	BT_GRAPH_CONFIGURATION_STATE_CONFIGURING,
	BT_GRAPH_CONFIGURATION_STATE_PARTIALLY_CONFIGURED,
	BT_GRAPH_CONFIGURATION_STATE_CONFIGURED,
	BT_GRAPH_CONFIGURATION_STATE_FAULTY,
	BT_GRAPH_CONFIGURATION_STATE_DESTROYING,
};

struct bt_graph {
	/**
	 * A component graph contains components and point-to-point connection
	 * between these components.
	 *
	 * In terms of ownership:
	 * 1) The graph is the components' parent,
	 * 2) The graph is the connnections' parent,
	 * 3) Components share the ownership of their connections,
	 * 4) A connection holds weak references to its two component endpoints.
	 */
	struct bt_object base;

	/* Array of pointers to bt_connection. */
	GPtrArray *connections;
	/* Array of pointers to bt_component. */
	GPtrArray *components;
	/* Queue of pointers (weak references) to sink bt_components. */
	GQueue *sinks_to_consume;

	uint64_t mip_version;

	/*
	 * Array of `struct bt_interrupter *`, each one owned by this.
	 * If any interrupter is set, then this graph is deemed
	 * interrupted.
	 */
	GPtrArray *interrupters;

	/*
	 * Default interrupter, owned by this.
	 */
	struct bt_interrupter *default_interrupter;

	bool has_sink;

	/*
	 * If this is false, then the public API's consuming
	 * functions (bt_graph_consume() and bt_graph_run()) return
	 * BT_FUNC_STATUS_CANNOT_CONSUME. The internal "no check"
	 * functions always work.
	 *
	 * In bt_port_output_message_iterator_create(), on success,
	 * this flag is cleared so that the iterator remains the only
	 * consumer for the graph's lifetime.
	 */
	bool can_consume;

	enum bt_graph_configuration_state config_state;

	struct {
		GArray *source_output_port_added;
		GArray *filter_output_port_added;
		GArray *filter_input_port_added;
		GArray *sink_input_port_added;
	} listeners;

	/* Pool of `struct bt_message_event *` */
	struct bt_object_pool event_msg_pool;

	/* Pool of `struct bt_message_packet_beginning *` */
	struct bt_object_pool packet_begin_msg_pool;

	/* Pool of `struct bt_message_packet_end *` */
	struct bt_object_pool packet_end_msg_pool;

	/*
	 * Array of `struct bt_message *` (weak).
	 *
	 * This is an array of all the messages ever created from
	 * this graph. Some of them can be in one of the pools above,
	 * some of them can be at large. Because each message has a
	 * weak pointer to the graph containing its pool, we need to
	 * notify each message that the graph is gone on graph
	 * destruction.
	 *
	 * TODO: When we support a maximum size for object pools,
	 * add a way for a message to remove itself from this
	 * array (on destruction).
	 */
	GPtrArray *messages;
};

static inline
void bt_graph_set_can_consume(struct bt_graph *graph, bool can_consume)
{
	BT_ASSERT_DBG(graph);
	graph->can_consume = can_consume;
}

BT_HIDDEN
int bt_graph_consume_sink_no_check(struct bt_graph *graph,
		struct bt_component_sink *sink);

BT_HIDDEN
enum bt_graph_listener_func_status bt_graph_notify_port_added(struct bt_graph *graph,
		struct bt_port *port);

BT_HIDDEN
void bt_graph_remove_connection(struct bt_graph *graph,
		struct bt_connection *connection);

BT_HIDDEN
void bt_graph_add_message(struct bt_graph *graph,
		struct bt_message *msg);

BT_HIDDEN
bool bt_graph_is_interrupted(const struct bt_graph *graph);

static inline
const char *bt_graph_configuration_state_string(
		enum bt_graph_configuration_state state)
{
	switch (state) {
	case BT_GRAPH_CONFIGURATION_STATE_CONFIGURING:
		return "CONFIGURING";
	case BT_GRAPH_CONFIGURATION_STATE_PARTIALLY_CONFIGURED:
		return "PARTIALLY_CONFIGURED";
	case BT_GRAPH_CONFIGURATION_STATE_CONFIGURED:
		return "CONFIGURED";
	case BT_GRAPH_CONFIGURATION_STATE_FAULTY:
		return "FAULTY";
	case BT_GRAPH_CONFIGURATION_STATE_DESTROYING:
		return "DESTROYING";
	default:
		return "(unknown)";
	}
}

static inline
int bt_graph_configure(struct bt_graph *graph)
{
	int status = BT_FUNC_STATUS_OK;
	uint64_t i;

	BT_ASSERT_DBG(graph->config_state !=
		BT_GRAPH_CONFIGURATION_STATE_FAULTY);

	if (G_LIKELY(graph->config_state ==
			BT_GRAPH_CONFIGURATION_STATE_CONFIGURED)) {
		goto end;
	}

	BT_ASSERT_PRE(graph->has_sink, "Graph has no sink component: %!+g", graph);
	graph->config_state = BT_GRAPH_CONFIGURATION_STATE_PARTIALLY_CONFIGURED;

	for (i = 0; i < graph->components->len; i++) {
		struct bt_component *comp = graph->components->pdata[i];
		struct bt_component_sink *comp_sink = (void *) comp;
		struct bt_component_class_sink *comp_cls_sink =
			(void *) comp->class;

		if (comp->class->type != BT_COMPONENT_CLASS_TYPE_SINK) {
			continue;
		}

		if (comp_sink->graph_is_configured_method_called) {
			continue;
		}

		if (comp_cls_sink->methods.graph_is_configured) {
			enum bt_component_class_sink_graph_is_configured_method_status comp_status;

			BT_LIB_LOGD("Calling user's \"graph is configured\" method: "
				"%![graph-]+g, %![comp-]+c",
				graph, comp);
			comp_status = comp_cls_sink->methods.graph_is_configured(
				(void *) comp_sink);
			BT_LIB_LOGD("User method returned: status=%s",
				bt_common_func_status_string(comp_status));
			BT_ASSERT_POST(comp_status == BT_FUNC_STATUS_OK ||
				comp_status == BT_FUNC_STATUS_ERROR ||
				comp_status == BT_FUNC_STATUS_MEMORY_ERROR,
				"Unexpected returned status: status=%s",
				bt_common_func_status_string(comp_status));
			BT_ASSERT_POST_NO_ERROR_IF_NO_ERROR_STATUS(comp_status);
			if (comp_status != BT_FUNC_STATUS_OK) {
				if (comp_status < 0) {
					BT_LIB_LOGW_APPEND_CAUSE(
						"Component's \"graph is configured\" method failed: "
						"%![comp-]+c, status=%s",
						comp,
						bt_common_func_status_string(
							comp_status));
				}

				status = comp_status;
				goto end;
			}
		}

		comp_sink->graph_is_configured_method_called = true;
	}

	graph->config_state = BT_GRAPH_CONFIGURATION_STATE_CONFIGURED;

end:
	return status;
}

static inline
void bt_graph_make_faulty(struct bt_graph *graph)
{
	graph->config_state = BT_GRAPH_CONFIGURATION_STATE_FAULTY;
	BT_LIB_LOGI("Set graph's state to faulty: %![graph-]+g", graph);
}

#endif /* BABELTRACE_GRAPH_GRAPH_INTERNAL_H */
