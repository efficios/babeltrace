/*
 * test_graph_topo.c
 *
 * Copyright 2017 - Philippe Proulx <pproulx@efficios.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; under version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include <babeltrace/object.h>
#include <babeltrace/graph/component-class.h>
#include <babeltrace/graph/component-class-source.h>
#include <babeltrace/graph/component-class-sink.h>
#include <babeltrace/graph/component.h>
#include <babeltrace/graph/component-source.h>
#include <babeltrace/graph/component-sink.h>
#include <babeltrace/graph/graph.h>
#include <babeltrace/graph/connection.h>
#include <babeltrace/graph/port.h>
#include <babeltrace/graph/private-component.h>
#include <babeltrace/graph/private-component-source.h>
#include <babeltrace/graph/private-component-sink.h>
#include <babeltrace/graph/private-port.h>
#include <babeltrace/graph/private-connection.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <babeltrace/assert-internal.h>
#include <glib.h>

#include "tap/tap.h"

#define NR_TESTS	99

enum event_type {
	COMP_ACCEPT_PORT_CONNECTION,
	COMP_PORT_CONNECTED,
	COMP_PORT_DISCONNECTED,
	GRAPH_PORT_ADDED,
	GRAPH_PORT_REMOVED,
	GRAPH_PORTS_CONNECTED,
	GRAPH_PORTS_DISCONNECTED,
};

enum test {
	TEST_EMPTY_GRAPH,
	TEST_SIMPLE,
	TEST_SRC_PORT_CONNECTED_ERROR,
	TEST_SINK_PORT_CONNECTED_ERROR,
	TEST_SRC_ADDS_PORT_IN_PORT_CONNECTED,
	TEST_SINK_REMOVES_PORT_IN_CONSUME,
	TEST_SINK_REMOVES_PORT_IN_CONSUME_THEN_SRC_REMOVES_DISCONNECTED_PORT,
};

struct event {
	enum event_type type;

	union {
		struct {
			struct bt_component *comp;
			struct bt_port *self_port;
			struct bt_port *other_port;
		} comp_accept_port_connection;

		struct {
			struct bt_component *comp;
			struct bt_port *self_port;
			struct bt_port *other_port;
		} comp_port_connected;

		struct {
			struct bt_component *comp;
			struct bt_port *port;
		} comp_port_disconnected;

		struct {
			struct bt_component *comp;
			struct bt_port *port;
		} graph_port_added;

		struct {
			struct bt_component *comp;
			struct bt_port *port;
		} graph_port_removed;

		struct {
			struct bt_component *upstream_comp;
			struct bt_component *downstream_comp;
			struct bt_port *upstream_port;
			struct bt_port *downstream_port;
			struct bt_connection *conn;
		} graph_ports_connected;

		struct {
			struct bt_component *upstream_comp;
			struct bt_component *downstream_comp;
			struct bt_port *upstream_port;
			struct bt_port *downstream_port;
		} graph_ports_disconnected;
	} data;
};

static GArray *events;
static struct bt_component_class *src_comp_class;
static struct bt_component_class *sink_comp_class;
static enum test current_test;

static
void clear_events(void)
{
	g_array_set_size(events, 0);
}

static
void append_event(struct event *event)
{
	g_array_append_val(events, *event);
}

static
bool compare_events(struct event *ev_a, struct event *ev_b)
{
	if (ev_a->type != ev_b->type) {
		return false;
	}

	switch (ev_a->type) {
		case COMP_ACCEPT_PORT_CONNECTION:
			if (ev_a->data.comp_accept_port_connection.comp !=
					ev_b->data.comp_accept_port_connection.comp) {
				return false;
			}

			if (ev_a->data.comp_accept_port_connection.self_port !=
					ev_b->data.comp_accept_port_connection.self_port) {
				return false;
			}

			if (ev_a->data.comp_accept_port_connection.other_port !=
					ev_b->data.comp_accept_port_connection.other_port) {
				return false;
			}
			break;
		case COMP_PORT_CONNECTED:
			if (ev_a->data.comp_port_connected.comp !=
					ev_b->data.comp_port_connected.comp) {
				return false;
			}

			if (ev_a->data.comp_port_connected.self_port !=
					ev_b->data.comp_port_connected.self_port) {
				return false;
			}

			if (ev_a->data.comp_port_connected.other_port !=
					ev_b->data.comp_port_connected.other_port) {
				return false;
			}
			break;
		case COMP_PORT_DISCONNECTED:
			if (ev_a->data.comp_port_disconnected.comp !=
					ev_b->data.comp_port_disconnected.comp) {
				return false;
			}

			if (ev_a->data.comp_port_disconnected.port !=
					ev_b->data.comp_port_disconnected.port) {
				return false;
			}
			break;
		case GRAPH_PORT_ADDED:
			if (ev_a->data.graph_port_added.comp !=
					ev_b->data.graph_port_added.comp) {
				return false;
			}

			if (ev_a->data.graph_port_added.port !=
					ev_b->data.graph_port_added.port) {
				return false;
			}
			break;
		case GRAPH_PORT_REMOVED:
			if (ev_a->data.graph_port_removed.comp !=
					ev_b->data.graph_port_removed.comp) {
				return false;
			}

			if (ev_a->data.graph_port_removed.port !=
					ev_b->data.graph_port_removed.port) {
				return false;
			}
			break;
		case GRAPH_PORTS_CONNECTED:
			if (ev_a->data.graph_ports_connected.upstream_comp !=
					ev_b->data.graph_ports_connected.upstream_comp) {
				return false;
			}

			if (ev_a->data.graph_ports_connected.downstream_comp !=
					ev_b->data.graph_ports_connected.downstream_comp) {
				return false;
			}

			if (ev_a->data.graph_ports_connected.upstream_port !=
					ev_b->data.graph_ports_connected.upstream_port) {
				return false;
			}

			if (ev_a->data.graph_ports_connected.downstream_port !=
					ev_b->data.graph_ports_connected.downstream_port) {
				return false;
			}

			if (ev_a->data.graph_ports_connected.conn !=
					ev_b->data.graph_ports_connected.conn) {
				return false;
			}
			break;
		case GRAPH_PORTS_DISCONNECTED:
			if (ev_a->data.graph_ports_disconnected.upstream_comp !=
					ev_b->data.graph_ports_disconnected.upstream_comp) {
				return false;
			}

			if (ev_a->data.graph_ports_disconnected.downstream_comp !=
					ev_b->data.graph_ports_disconnected.downstream_comp) {
				return false;
			}

			if (ev_a->data.graph_ports_disconnected.upstream_port !=
					ev_b->data.graph_ports_disconnected.upstream_port) {
				return false;
			}

			if (ev_a->data.graph_ports_disconnected.downstream_port !=
					ev_b->data.graph_ports_disconnected.downstream_port) {
				return false;
			}
			break;
		default:
			abort();
	}

	return true;
}

static
bool has_event(struct event *event)
{
	size_t i;

	for (i = 0; i < events->len; i++) {
		struct event *ev = &g_array_index(events, struct event, i);

		if (compare_events(event, ev)) {
			return true;
		}
	}

	return false;
}

static
size_t event_pos(struct event *event)
{
	size_t i;

	for (i = 0; i < events->len; i++) {
		struct event *ev = &g_array_index(events, struct event, i);

		if (compare_events(event, ev)) {
			return i;
		}
	}

	return SIZE_MAX;
}

static
enum bt_notification_iterator_status src_iter_next(
		struct bt_private_connection_private_notification_iterator *priv_iterator,
		bt_notification_array notifs, uint64_t capacity,
		uint64_t *count)
{
	return BT_NOTIFICATION_ITERATOR_STATUS_ERROR;
}

static
enum bt_component_status accept_port_connection(
		struct bt_private_component *private_component,
		struct bt_private_port *self_private_port,
		struct bt_port *other_port)
{
	struct event event = {
		.type = COMP_ACCEPT_PORT_CONNECTION,
		.data.comp_accept_port_connection = {
			.comp = bt_component_borrow_from_private(private_component),
			.self_port = bt_port_borrow_from_private(self_private_port),
			.other_port = other_port,
		},
	};

	append_event(&event);
	return BT_COMPONENT_STATUS_OK;
}

static
enum bt_component_status src_port_connected(
		struct bt_private_component *private_component,
		struct bt_private_port *self_private_port,
		struct bt_port *other_port)
{
	int ret;

	struct event event = {
		.type = COMP_PORT_CONNECTED,
		.data.comp_port_connected = {
			.comp = bt_component_borrow_from_private(private_component),
			.self_port = bt_port_borrow_from_private(self_private_port),
			.other_port = other_port,
		},
	};

	append_event(&event);

	switch (current_test) {
	case TEST_SRC_ADDS_PORT_IN_PORT_CONNECTED:
		ret = bt_private_component_source_add_output_private_port(
			private_component, "hello", NULL, NULL);
		BT_ASSERT(ret == 0);
		break;
	case TEST_SRC_PORT_CONNECTED_ERROR:
		return BT_COMPONENT_STATUS_ERROR;
	default:
		break;
	}

	return BT_COMPONENT_STATUS_OK;
}

static
void src_port_disconnected(struct bt_private_component *private_component,
		struct bt_private_port *private_port)
{
	int ret;
	struct event event = {
		.type = COMP_PORT_DISCONNECTED,
		.data.comp_port_disconnected = {
			.comp = bt_component_borrow_from_private(private_component),
			.port = bt_port_borrow_from_private(private_port),
		},
	};

	append_event(&event);

	switch (current_test) {
	case TEST_SINK_REMOVES_PORT_IN_CONSUME_THEN_SRC_REMOVES_DISCONNECTED_PORT:
		ret = bt_private_port_remove_from_component(private_port);
		BT_ASSERT(ret == 0);
	default:
		break;
	}
}

static
enum bt_component_status src_init(struct bt_private_component *priv_comp,
	struct bt_value *params, void *init_method_data)
{
	int ret;

	ret = bt_private_component_source_add_output_private_port(
		priv_comp, "out", NULL, NULL);
	BT_ASSERT(ret == 0);
	return BT_COMPONENT_STATUS_OK;
}

static
enum bt_component_status sink_consume(
		struct bt_private_component *priv_component)
{
	struct bt_private_port *def_port;
	int ret;

	switch (current_test) {
	case TEST_SINK_REMOVES_PORT_IN_CONSUME:
	case TEST_SINK_REMOVES_PORT_IN_CONSUME_THEN_SRC_REMOVES_DISCONNECTED_PORT:
		def_port = bt_private_component_sink_get_input_private_port_by_name(
			priv_component, "in");
		BT_ASSERT(def_port);
		ret = bt_private_port_remove_from_component(def_port);
		BT_ASSERT(ret == 0);
		bt_object_put_ref(def_port);
		break;
	default:
		break;
	}

	return BT_COMPONENT_STATUS_OK;
}

static
enum bt_component_status sink_port_connected(
		struct bt_private_component *private_component,
		struct bt_private_port *self_private_port,
		struct bt_port *other_port)
{
	struct event event = {
		.type = COMP_PORT_CONNECTED,
		.data.comp_port_connected = {
			.comp = bt_component_borrow_from_private(private_component),
			.self_port = bt_port_borrow_from_private(self_private_port),
			.other_port = other_port,
		},
	};

	append_event(&event);

	if (current_test == TEST_SINK_PORT_CONNECTED_ERROR) {
		return BT_COMPONENT_STATUS_ERROR;
	} else {
		return BT_COMPONENT_STATUS_OK;
	}
}

static
void sink_port_disconnected(struct bt_private_component *private_component,
		struct bt_private_port *private_port)
{
	struct event event = {
		.type = COMP_PORT_DISCONNECTED,
		.data.comp_port_disconnected = {
			.comp = bt_component_borrow_from_private(private_component),
			.port = bt_port_borrow_from_private(private_port),
		},
	};

	append_event(&event);
}

static
enum bt_component_status sink_init(struct bt_private_component *priv_comp,
	struct bt_value *params, void *init_method_data)
{
	int ret;

	ret = bt_private_component_sink_add_input_private_port(priv_comp,
		"in", NULL, NULL);
	BT_ASSERT(ret == 0);
	return BT_COMPONENT_STATUS_OK;
}

static
void graph_port_added(struct bt_port *port,
		void *data)
{
	struct bt_component *comp = bt_port_get_component(port);

	BT_ASSERT(comp);
	bt_object_put_ref(comp);

	struct event event = {
		.type = GRAPH_PORT_ADDED,
		.data.graph_port_added = {
			.comp = comp,
			.port = port,
		},
	};

	append_event(&event);
}

static
void graph_port_removed(struct bt_component *component,
		struct bt_port *port, void *data)
{
	struct event event = {
		.type = GRAPH_PORT_REMOVED,
		.data.graph_port_removed = {
			.comp = component,
			.port = port,
		},
	};

	append_event(&event);
}

static
void graph_ports_connected(struct bt_port *upstream_port,
		struct bt_port *downstream_port, void *data)
{
	struct bt_component *upstream_comp =
		bt_port_get_component(upstream_port);
	struct bt_component *downstream_comp =
		bt_port_get_component(downstream_port);
	struct bt_connection *conn = bt_port_get_connection(upstream_port);

	BT_ASSERT(upstream_comp);
	BT_ASSERT(downstream_comp);
	BT_ASSERT(conn);
	bt_object_put_ref(upstream_comp);
	bt_object_put_ref(downstream_comp);
	bt_object_put_ref(conn);

	struct event event = {
		.type = GRAPH_PORTS_CONNECTED,
		.data.graph_ports_connected = {
			.upstream_comp = upstream_comp,
			.downstream_comp = downstream_comp,
			.upstream_port = upstream_port,
			.downstream_port = downstream_port,
			.conn = conn,
		},
	};

	append_event(&event);
}

static
void graph_ports_disconnected(
		struct bt_component *upstream_comp,
		struct bt_component *downstream_comp,
		struct bt_port *upstream_port, struct bt_port *downstream_port,
		void *data)
{
	struct event event = {
		.type = GRAPH_PORTS_DISCONNECTED,
		.data.graph_ports_disconnected = {
			.upstream_comp = upstream_comp,
			.downstream_comp = downstream_comp,
			.upstream_port = upstream_port,
			.downstream_port = downstream_port,
		},
	};

	append_event(&event);
}

static
void init_test(void)
{
	int ret;

	src_comp_class = bt_component_class_source_create("src", src_iter_next);
	BT_ASSERT(src_comp_class);
	ret = bt_component_class_set_init_method(src_comp_class, src_init);
	BT_ASSERT(ret == 0);
	ret = bt_component_class_set_accept_port_connection_method(
		src_comp_class, accept_port_connection);
	BT_ASSERT(ret == 0);
	ret = bt_component_class_set_port_connected_method(src_comp_class,
		src_port_connected);
	BT_ASSERT(ret == 0);
	ret = bt_component_class_set_port_disconnected_method(
		src_comp_class, src_port_disconnected);
	BT_ASSERT(ret == 0);
	sink_comp_class = bt_component_class_sink_create("sink", sink_consume);
	BT_ASSERT(sink_comp_class);
	ret = bt_component_class_set_init_method(sink_comp_class, sink_init);
	BT_ASSERT(ret == 0);
	ret = bt_component_class_set_accept_port_connection_method(
		sink_comp_class, accept_port_connection);
	BT_ASSERT(ret == 0);
	ret = bt_component_class_set_port_connected_method(sink_comp_class,
		sink_port_connected);
	BT_ASSERT(ret == 0);
	ret = bt_component_class_set_port_disconnected_method(sink_comp_class,
		sink_port_disconnected);
	BT_ASSERT(ret == 0);
	bt_component_class_freeze(src_comp_class);
	bt_component_class_freeze(sink_comp_class);
	events = g_array_new(FALSE, TRUE, sizeof(struct event));
	BT_ASSERT(events);
}

static
void fini_test(void)
{
	bt_object_put_ref(src_comp_class);
	bt_object_put_ref(sink_comp_class);
	g_array_free(events, TRUE);
}

static
struct bt_component *create_src(struct bt_graph *graph)
{
	struct bt_component *comp;
	int ret;

	ret = bt_graph_add_component(graph, src_comp_class, "src-comp", NULL,
		&comp);
	BT_ASSERT(ret == 0);
	return comp;
}

static
struct bt_component *create_sink(struct bt_graph *graph)
{
	struct bt_component *comp;
	int ret;

	ret = bt_graph_add_component(graph, sink_comp_class, "sink-comp",
		NULL, &comp);
	BT_ASSERT(ret == 0);
	return comp;
}

static
struct bt_graph *create_graph(void)
{
	struct bt_graph *graph = bt_graph_create();
	int ret;

	BT_ASSERT(graph);
	ret = bt_graph_add_port_added_listener(graph, graph_port_added, NULL,
		NULL);
	BT_ASSERT(ret >= 0);
	ret = bt_graph_add_port_removed_listener(graph, graph_port_removed,
		NULL, NULL);
	BT_ASSERT(ret >= 0);
	ret = bt_graph_add_ports_connected_listener(graph,
		graph_ports_connected, NULL, NULL);
	BT_ASSERT(ret >= 0);
	ret = bt_graph_add_ports_disconnected_listener(graph,
		graph_ports_disconnected, NULL, NULL);
	BT_ASSERT(ret >= 0);
	return graph;
}

static
void prepare_test(enum test test, const char *name)
{
	clear_events();
	current_test = test;
	diag("test: %s", name);
}

static
void test_sink_removes_port_in_port_connected_then_src_removes_disconnected_port(void)
{
	int ret;
	struct bt_component *src;
	struct bt_component *sink;
	struct bt_graph *graph;
	struct bt_port *src_def_port;
	struct bt_port *sink_def_port;
	struct bt_connection *conn;
	struct event event;
	enum bt_graph_status status;
	size_t src_accept_port_connection_pos;
	size_t sink_accept_port_connection_pos;
	size_t src_port_connected_pos;
	size_t sink_port_connected_pos;
	size_t graph_ports_connected;
	size_t src_port_disconnected_pos;
	size_t sink_port_disconnected_pos;
	size_t graph_ports_disconnected_pos;
	size_t graph_port_removed_src_pos;
	size_t graph_port_removed_sink_pos;

	prepare_test(TEST_SINK_REMOVES_PORT_IN_CONSUME_THEN_SRC_REMOVES_DISCONNECTED_PORT,
		"sink removes port in consume, then source removes disconnected port");
	graph = create_graph();
	BT_ASSERT(graph);
	src = create_src(graph);
	sink = create_sink(graph);
	src_def_port = bt_component_source_get_output_port_by_name(src, "out");
	BT_ASSERT(src_def_port);
	sink_def_port = bt_component_sink_get_input_port_by_name(sink, "in");
	BT_ASSERT(sink_def_port);
	status = bt_graph_connect_ports(graph, src_def_port, sink_def_port,
		&conn);
	BT_ASSERT(status == 0);
	BT_ASSERT(conn);

	/* We're supposed to have 7 events so far */
	ok(events->len == 7, "we have the expected number of events (before consume)");

	/* Source's port added */
	event.type = GRAPH_PORT_ADDED;
	event.data.graph_port_added.comp = src;
	event.data.graph_port_added.port = src_def_port;
	ok(has_event(&event), "got the expected graph's port added event (for source, initial)");

	/* Sink's port added */
	event.type = GRAPH_PORT_ADDED;
	event.data.graph_port_added.comp = sink;
	event.data.graph_port_added.port = sink_def_port;
	ok(has_event(&event), "got the expected graph's port added event (for sink, initial)");

	/* Source's accept port connection */
	event.type = COMP_ACCEPT_PORT_CONNECTION;
	event.data.comp_accept_port_connection.comp = src;
	event.data.comp_accept_port_connection.self_port = src_def_port;
	event.data.comp_accept_port_connection.other_port = sink_def_port;
	ok(has_event(&event), "got the expected source's accept port connection event");
	src_accept_port_connection_pos = event_pos(&event);

	/* Sink's accept port connection */
	event.type = COMP_ACCEPT_PORT_CONNECTION;
	event.data.comp_accept_port_connection.comp = sink;
	event.data.comp_accept_port_connection.self_port = sink_def_port;
	event.data.comp_accept_port_connection.other_port = src_def_port;
	ok(has_event(&event), "got the expected sink's accept port connection event");
	sink_accept_port_connection_pos = event_pos(&event);

	/* Source's port connected */
	event.type = COMP_PORT_CONNECTED;
	event.data.comp_port_connected.comp = src;
	event.data.comp_port_connected.self_port = src_def_port;
	event.data.comp_port_connected.other_port = sink_def_port;
	ok(has_event(&event), "got the expected source's port connected event");
	src_port_connected_pos = event_pos(&event);

	/* Sink's port connected */
	event.type = COMP_PORT_CONNECTED;
	event.data.comp_port_connected.comp = sink;
	event.data.comp_port_connected.self_port = sink_def_port;
	event.data.comp_port_connected.other_port = src_def_port;
	ok(has_event(&event), "got the expected sink's port connected event");
	sink_port_connected_pos = event_pos(&event);

	/* Graph's ports connected */
	event.type = GRAPH_PORTS_CONNECTED;
	event.data.graph_ports_connected.upstream_comp = src;
	event.data.graph_ports_connected.downstream_comp = sink;
	event.data.graph_ports_connected.upstream_port = src_def_port;
	event.data.graph_ports_connected.downstream_port = sink_def_port;
	event.data.graph_ports_connected.conn = conn;
	ok(has_event(&event), "got the expected graph's ports connected event");
	graph_ports_connected = event_pos(&event);

	/* Order of events */
	ok(src_port_connected_pos < graph_ports_connected,
		"event order is good (1)");
	ok(sink_port_connected_pos < graph_ports_connected,
		"event order is good (2)");
	ok(src_accept_port_connection_pos < src_port_connected_pos,
		"event order is good (3)");
	ok(sink_accept_port_connection_pos < sink_port_connected_pos,
		"event order is good (4)");

	/* Consume sink once */
	clear_events();
	ret = bt_graph_consume(graph);
	BT_ASSERT(ret == 0);

	/* We're supposed to have 5 new events */
	ok(events->len == 5, "we have the expected number of events (after consume)");

	/* Source's port disconnected */
	event.type = COMP_PORT_DISCONNECTED;
	event.data.comp_port_disconnected.comp = src;
	event.data.comp_port_disconnected.port = src_def_port;
	ok(has_event(&event), "got the expected source's port disconnected event");
	src_port_disconnected_pos = event_pos(&event);

	/* Sink's port disconnected */
	event.type = COMP_PORT_DISCONNECTED;
	event.data.comp_port_disconnected.comp = sink;
	event.data.comp_port_disconnected.port = sink_def_port;
	ok(has_event(&event), "got the expected sink's port disconnected event");
	sink_port_disconnected_pos = event_pos(&event);

	/* Graph's ports disconnected */
	event.type = GRAPH_PORTS_DISCONNECTED;
	event.data.graph_ports_disconnected.upstream_comp = src;
	event.data.graph_ports_disconnected.downstream_comp = sink;
	event.data.graph_ports_disconnected.upstream_port = src_def_port;
	event.data.graph_ports_disconnected.downstream_port = sink_def_port;
	ok(has_event(&event), "got the expected graph's ports disconnected event");
	graph_ports_disconnected_pos = event_pos(&event);

	/* Graph's port removed (sink) */
	event.type = GRAPH_PORT_REMOVED;
	event.data.graph_port_removed.comp = sink;
	event.data.graph_port_removed.port = sink_def_port;
	ok(has_event(&event), "got the expected graph's port removed event (for sink)");
	graph_port_removed_sink_pos = event_pos(&event);

	/* Graph's port removed (source) */
	event.type = GRAPH_PORT_REMOVED;
	event.data.graph_port_removed.comp = src;
	event.data.graph_port_removed.port = src_def_port;
	ok(has_event(&event), "got the expected graph's port removed event (for source)");
	graph_port_removed_src_pos = event_pos(&event);

	/* Order of events */
	ok(src_port_disconnected_pos < graph_ports_disconnected_pos,
		"event order is good (5)");
	ok(src_port_disconnected_pos < graph_port_removed_sink_pos,
		"event order is good (6)");
	ok(src_port_disconnected_pos < graph_port_removed_src_pos,
		"event order is good (7)");
	ok(sink_port_disconnected_pos < graph_ports_disconnected_pos,
		"event order is good (8)");
	ok(sink_port_disconnected_pos < graph_port_removed_sink_pos,
		"event order is good (9)");
	ok(sink_port_disconnected_pos < graph_port_removed_src_pos,
		"event order is good (10)");
	ok(graph_ports_disconnected_pos < graph_port_removed_sink_pos,
		"event order is good (11)");
	ok(graph_port_removed_src_pos < graph_ports_disconnected_pos,
		"event order is good (12)");
	ok(graph_port_removed_src_pos < graph_port_removed_sink_pos,
		"event order is good (13)");

	bt_object_put_ref(graph);
	bt_object_put_ref(sink);
	bt_object_put_ref(src);
	bt_object_put_ref(conn);
	bt_object_put_ref(src_def_port);
	bt_object_put_ref(sink_def_port);
}

static
void test_sink_removes_port_in_port_connected(void)
{
	int ret;
	struct bt_component *src;
	struct bt_component *sink;
	struct bt_graph *graph;
	struct bt_port *src_def_port;
	struct bt_port *sink_def_port;
	struct bt_connection *conn;
	struct event event;
	enum bt_graph_status status;
	size_t src_accept_port_connection_pos;
	size_t sink_accept_port_connection_pos;
	size_t src_port_connected_pos;
	size_t sink_port_connected_pos;
	size_t graph_ports_connected_pos;
	size_t src_port_disconnected_pos;
	size_t sink_port_disconnected_pos;
	size_t graph_ports_disconnected_pos;
	size_t graph_port_removed_sink_pos;

	prepare_test(TEST_SINK_REMOVES_PORT_IN_CONSUME,
		"sink removes port in consume");
	graph = create_graph();
	BT_ASSERT(graph);
	src = create_src(graph);
	sink = create_sink(graph);
	src_def_port = bt_component_source_get_output_port_by_name(src, "out");
	BT_ASSERT(src_def_port);
	sink_def_port = bt_component_sink_get_input_port_by_name(sink, "in");
	BT_ASSERT(sink_def_port);
	status = bt_graph_connect_ports(graph, src_def_port, sink_def_port,
		&conn);
	BT_ASSERT(status == 0);

	/* We're supposed to have 7 events so far */
	ok(events->len == 7, "we have the expected number of events (before consume)");

	/* Source's port added */
	event.type = GRAPH_PORT_ADDED;
	event.data.graph_port_added.comp = src;
	event.data.graph_port_added.port = src_def_port;
	ok(has_event(&event), "got the expected graph's port added event (for source, initial)");

	/* Sink's port added */
	event.type = GRAPH_PORT_ADDED;
	event.data.graph_port_added.comp = sink;
	event.data.graph_port_added.port = sink_def_port;
	ok(has_event(&event), "got the expected graph's port added event (for sink, initial)");

	/* Source's accept port connection */
	event.type = COMP_ACCEPT_PORT_CONNECTION;
	event.data.comp_accept_port_connection.comp = src;
	event.data.comp_accept_port_connection.self_port = src_def_port;
	event.data.comp_accept_port_connection.other_port = sink_def_port;
	ok(has_event(&event), "got the expected source's accept port connection event");
	src_accept_port_connection_pos = event_pos(&event);

	/* Sink's accept port connection */
	event.type = COMP_ACCEPT_PORT_CONNECTION;
	event.data.comp_accept_port_connection.comp = sink;
	event.data.comp_accept_port_connection.self_port = sink_def_port;
	event.data.comp_accept_port_connection.other_port = src_def_port;
	ok(has_event(&event), "got the expected sink's accept port connection event");
	sink_accept_port_connection_pos = event_pos(&event);

	/* Source's port connected */
	event.type = COMP_PORT_CONNECTED;
	event.data.comp_port_connected.comp = src;
	event.data.comp_port_connected.self_port = src_def_port;
	event.data.comp_port_connected.other_port = sink_def_port;
	ok(has_event(&event), "got the expected source's port connected event");
	src_port_connected_pos = event_pos(&event);

	/* Sink's port connected */
	event.type = COMP_PORT_CONNECTED;
	event.data.comp_port_connected.comp = sink;
	event.data.comp_port_connected.self_port = sink_def_port;
	event.data.comp_port_connected.other_port = src_def_port;
	ok(has_event(&event), "got the expected sink's port connected event");
	sink_port_connected_pos = event_pos(&event);

	/* Graph's ports connected */
	event.type = GRAPH_PORTS_CONNECTED;
	event.data.graph_ports_connected.upstream_comp = src;
	event.data.graph_ports_connected.downstream_comp = sink;
	event.data.graph_ports_connected.upstream_port = src_def_port;
	event.data.graph_ports_connected.downstream_port = sink_def_port;
	event.data.graph_ports_connected.conn = conn;
	ok(has_event(&event), "got the expected graph's ports connected event");
	graph_ports_connected_pos = event_pos(&event);

	/* Order of events */
	ok(src_port_connected_pos < graph_ports_connected_pos,
		"event order is good (1)");
	ok(sink_port_connected_pos < graph_ports_connected_pos,
		"event order is good (2)");
	ok(src_accept_port_connection_pos < src_port_connected_pos,
		"event order is good (3)");
	ok(sink_accept_port_connection_pos < sink_port_connected_pos,
		"event order is good (4)");

	/* Consume sink once */
	clear_events();
	ret = bt_graph_consume(graph);
	BT_ASSERT(ret == 0);

	/* We're supposed to have 4 new events */
	ok(events->len == 4, "we have the expected number of events (after consume)");

	/* Source's port disconnected */
	event.type = COMP_PORT_DISCONNECTED;
	event.data.comp_port_disconnected.comp = src;
	event.data.comp_port_disconnected.port = src_def_port;
	ok(has_event(&event), "got the expected source's port disconnected event");
	src_port_disconnected_pos = event_pos(&event);

	/* Sink's port disconnected */
	event.type = COMP_PORT_DISCONNECTED;
	event.data.comp_port_disconnected.comp = sink;
	event.data.comp_port_disconnected.port = sink_def_port;
	ok(has_event(&event), "got the expected sink's port disconnected event");
	sink_port_disconnected_pos = event_pos(&event);

	/* Graph's ports disconnected */
	event.type = GRAPH_PORTS_DISCONNECTED;
	event.data.graph_ports_disconnected.upstream_comp = src;
	event.data.graph_ports_disconnected.downstream_comp = sink;
	event.data.graph_ports_disconnected.upstream_port = src_def_port;
	event.data.graph_ports_disconnected.downstream_port = sink_def_port;
	ok(has_event(&event), "got the expected graph's ports disconnected event");
	graph_ports_disconnected_pos = event_pos(&event);

	/* Graph's port removed (sink) */
	event.type = GRAPH_PORT_REMOVED;
	event.data.graph_port_removed.comp = sink;
	event.data.graph_port_removed.port = sink_def_port;
	ok(has_event(&event), "got the expected graph's port removed event (for sink)");
	graph_port_removed_sink_pos = event_pos(&event);

	/* Order of events */
	ok(src_port_disconnected_pos < graph_ports_disconnected_pos,
		"event order is good (5)");
	ok(src_port_disconnected_pos < graph_port_removed_sink_pos,
		"event order is good (7)");
	ok(sink_port_disconnected_pos < graph_ports_disconnected_pos,
		"event order is good (8)");
	ok(sink_port_disconnected_pos < graph_port_removed_sink_pos,
		"event order is good (10)");
	ok(graph_ports_disconnected_pos < graph_port_removed_sink_pos,
		"event order is good (11)");

	bt_object_put_ref(graph);
	bt_object_put_ref(sink);
	bt_object_put_ref(src);
	bt_object_put_ref(conn);
	bt_object_put_ref(src_def_port);
	bt_object_put_ref(sink_def_port);
}

static
void test_src_adds_port_in_port_connected(void)
{
	struct bt_component *src;
	struct bt_component *sink;
	struct bt_graph *graph;
	struct bt_port *src_def_port;
	struct bt_port *sink_def_port;
	struct bt_port *src_hello_port;
	struct bt_connection *conn;
	struct event event;
	enum bt_graph_status status;
	size_t src_accept_port_connection_pos;
	size_t sink_accept_port_connection_pos;
	size_t src_port_connected_pos;
	size_t sink_port_connected_pos;
	size_t graph_ports_connected_pos;
	size_t graph_port_added_src_pos;

	prepare_test(TEST_SRC_ADDS_PORT_IN_PORT_CONNECTED,
		"source adds port in port connected");
	graph = create_graph();
	BT_ASSERT(graph);
	src = create_src(graph);
	sink = create_sink(graph);
	src_def_port = bt_component_source_get_output_port_by_name(src, "out");
	BT_ASSERT(src_def_port);
	sink_def_port = bt_component_sink_get_input_port_by_name(sink, "in");
	BT_ASSERT(sink_def_port);
	status = bt_graph_connect_ports(graph, src_def_port, sink_def_port,
		&conn);
	BT_ASSERT(status == 0);
	src_hello_port = bt_component_source_get_output_port_by_name(src,
		"hello");
	BT_ASSERT(src_hello_port);

	/* We're supposed to have 8 events */
	ok(events->len == 8, "we have the expected number of events");

	/* Source's port added */
	event.type = GRAPH_PORT_ADDED;
	event.data.graph_port_added.comp = src;
	event.data.graph_port_added.port = src_def_port;
	ok(has_event(&event), "got the expected graph's port added event (for source, initial)");

	/* Sink's port added */
	event.type = GRAPH_PORT_ADDED;
	event.data.graph_port_added.comp = sink;
	event.data.graph_port_added.port = sink_def_port;
	ok(has_event(&event), "got the expected graph's port added event (for sink, initial)");

	/* Source's accept port connection */
	event.type = COMP_ACCEPT_PORT_CONNECTION;
	event.data.comp_accept_port_connection.comp = src;
	event.data.comp_accept_port_connection.self_port = src_def_port;
	event.data.comp_accept_port_connection.other_port = sink_def_port;
	ok(has_event(&event), "got the expected source's accept port connection event");
	src_accept_port_connection_pos = event_pos(&event);

	/* Sink's accept port connection */
	event.type = COMP_ACCEPT_PORT_CONNECTION;
	event.data.comp_accept_port_connection.comp = sink;
	event.data.comp_accept_port_connection.self_port = sink_def_port;
	event.data.comp_accept_port_connection.other_port = src_def_port;
	ok(has_event(&event), "got the expected sink's accept port connection event");
	sink_accept_port_connection_pos = event_pos(&event);

	/* Source's port connected */
	event.type = COMP_PORT_CONNECTED;
	event.data.comp_port_connected.comp = src;
	event.data.comp_port_connected.self_port = src_def_port;
	event.data.comp_port_connected.other_port = sink_def_port;
	ok(has_event(&event), "got the expected source's port connected event");
	src_port_connected_pos = event_pos(&event);

	/* Graph's port added (source) */
	event.type = GRAPH_PORT_ADDED;
	event.data.graph_port_added.comp = src;
	event.data.graph_port_added.port = src_hello_port;
	ok(has_event(&event), "got the expected graph's port added event (for source)");
	graph_port_added_src_pos = event_pos(&event);

	/* Sink's port connected */
	event.type = COMP_PORT_CONNECTED;
	event.data.comp_port_connected.comp = sink;
	event.data.comp_port_connected.self_port = sink_def_port;
	event.data.comp_port_connected.other_port = src_def_port;
	ok(has_event(&event), "got the expected sink's port connected event");
	sink_port_connected_pos = event_pos(&event);

	/* Graph's ports connected */
	event.type = GRAPH_PORTS_CONNECTED;
	event.data.graph_ports_connected.upstream_comp = src;
	event.data.graph_ports_connected.downstream_comp = sink;
	event.data.graph_ports_connected.upstream_port = src_def_port;
	event.data.graph_ports_connected.downstream_port = sink_def_port;
	event.data.graph_ports_connected.conn = conn;
	ok(has_event(&event), "got the expected graph's port connected event (for source)");
	graph_ports_connected_pos = event_pos(&event);

	/* Order of events */
	ok(src_port_connected_pos < graph_ports_connected_pos,
		"event order is good (1)");
	ok(sink_port_connected_pos < graph_ports_connected_pos,
		"event order is good (2)");
	ok(src_accept_port_connection_pos < src_port_connected_pos,
		"event order is good (3)");
	ok(sink_accept_port_connection_pos < sink_port_connected_pos,
		"event order is good (4)");
	ok(src_port_connected_pos < graph_port_added_src_pos,
		"event order is good (5)");
	ok(graph_port_added_src_pos < graph_ports_connected_pos,
		"event order is good (6)");

	bt_object_put_ref(graph);
	bt_object_put_ref(sink);
	bt_object_put_ref(src);
	bt_object_put_ref(conn);
	bt_object_put_ref(src_def_port);
	bt_object_put_ref(sink_def_port);
	bt_object_put_ref(src_hello_port);
}

static
void test_simple(void)
{
	struct bt_component *src;
	struct bt_component *sink;
	struct bt_graph *graph;
	struct bt_port *src_def_port;
	struct bt_port *sink_def_port;
	struct bt_connection *conn;
	struct event event;
	enum bt_graph_status status;
	size_t src_accept_port_connection_pos;
	size_t sink_accept_port_connection_pos;
	size_t src_port_connected_pos;
	size_t sink_port_connected_pos;
	size_t graph_ports_connected_pos;

	prepare_test(TEST_SIMPLE, "simple");
	graph = create_graph();
	BT_ASSERT(graph);
	src = create_src(graph);
	sink = create_sink(graph);
	src_def_port = bt_component_source_get_output_port_by_name(src, "out");
	BT_ASSERT(src_def_port);
	sink_def_port = bt_component_sink_get_input_port_by_name(sink, "in");
	BT_ASSERT(sink_def_port);
	status = bt_graph_connect_ports(graph, src_def_port, sink_def_port,
		&conn);
	BT_ASSERT(status == 0);

	/* We're supposed to have 7 events */
	ok(events->len == 7, "we have the expected number of events");

	/* Source's port added */
	event.type = GRAPH_PORT_ADDED;
	event.data.graph_port_added.comp = src;
	event.data.graph_port_added.port = src_def_port;
	ok(has_event(&event), "got the expected graph's port added event (for source, initial)");

	/* Sink's port added */
	event.type = GRAPH_PORT_ADDED;
	event.data.graph_port_added.comp = sink;
	event.data.graph_port_added.port = sink_def_port;
	ok(has_event(&event), "got the expected graph's port added event (for sink, initial)");

	/* Source's accept port connection */
	event.type = COMP_ACCEPT_PORT_CONNECTION;
	event.data.comp_accept_port_connection.comp = src;
	event.data.comp_accept_port_connection.self_port = src_def_port;
	event.data.comp_accept_port_connection.other_port = sink_def_port;
	ok(has_event(&event), "got the expected source's accept port connection event");
	src_accept_port_connection_pos = event_pos(&event);

	/* Sink's accept port connection */
	event.type = COMP_ACCEPT_PORT_CONNECTION;
	event.data.comp_accept_port_connection.comp = sink;
	event.data.comp_accept_port_connection.self_port = sink_def_port;
	event.data.comp_accept_port_connection.other_port = src_def_port;
	ok(has_event(&event), "got the expected sink's accept port connection event");
	sink_accept_port_connection_pos = event_pos(&event);

	/* Source's port connected */
	event.type = COMP_PORT_CONNECTED;
	event.data.comp_port_connected.comp = src;
	event.data.comp_port_connected.self_port = src_def_port;
	event.data.comp_port_connected.other_port = sink_def_port;
	ok(has_event(&event), "got the expected source's port connected event");
	src_port_connected_pos = event_pos(&event);

	/* Sink's port connected */
	event.type = COMP_PORT_CONNECTED;
	event.data.comp_port_connected.comp = sink;
	event.data.comp_port_connected.self_port = sink_def_port;
	event.data.comp_port_connected.other_port = src_def_port;
	ok(has_event(&event), "got the expected sink's port connected event");
	sink_port_connected_pos = event_pos(&event);

	/* Graph's port connected */
	event.type = GRAPH_PORTS_CONNECTED;
	event.data.graph_ports_connected.upstream_comp = src;
	event.data.graph_ports_connected.downstream_comp = sink;
	event.data.graph_ports_connected.upstream_port = src_def_port;
	event.data.graph_ports_connected.downstream_port = sink_def_port;
	event.data.graph_ports_connected.conn = conn;
	ok(has_event(&event), "got the expected graph's ports connected event");
	graph_ports_connected_pos = event_pos(&event);

	/* Order of events */
	ok(src_port_connected_pos < graph_ports_connected_pos,
		"event order is good (1)");
	ok(sink_port_connected_pos < graph_ports_connected_pos,
		"event order is good (2)");
	ok(src_accept_port_connection_pos < src_port_connected_pos,
		"event order is good (3)");
	ok(sink_accept_port_connection_pos < sink_port_connected_pos,
		"event order is good (4)");

	bt_object_put_ref(graph);
	bt_object_put_ref(sink);
	bt_object_put_ref(src);
	bt_object_put_ref(conn);
	bt_object_put_ref(src_def_port);
	bt_object_put_ref(sink_def_port);
}

static
void test_src_port_connected_error(void)
{
	struct bt_component *src;
	struct bt_component *sink;
	struct bt_graph *graph;
	struct bt_port *src_def_port;
	struct bt_port *sink_def_port;
	struct bt_connection *conn = NULL;
	struct event event;
	enum bt_graph_status status;
	size_t src_accept_port_connection_pos;
	size_t src_port_connected_pos;

	prepare_test(TEST_SRC_PORT_CONNECTED_ERROR, "port connected error: source");
	graph = create_graph();
	BT_ASSERT(graph);
	src = create_src(graph);
	sink = create_sink(graph);
	src_def_port = bt_component_source_get_output_port_by_name(src, "out");
	BT_ASSERT(src_def_port);
	sink_def_port = bt_component_sink_get_input_port_by_name(sink, "in");
	BT_ASSERT(sink_def_port);
	status = bt_graph_connect_ports(graph, src_def_port, sink_def_port,
		&conn);
	ok(status != BT_GRAPH_STATUS_OK,
		"bt_graph_connect_ports() returns an error");
	ok(!conn, "returned connection is NULL");

	/* We're supposed to have 5 events */
	ok(events->len == 5, "we have the expected number of events");

	/* Source's port added */
	event.type = GRAPH_PORT_ADDED;
	event.data.graph_port_added.comp = src;
	event.data.graph_port_added.port = src_def_port;
	ok(has_event(&event), "got the expected graph's port added event (for source, initial)");

	/* Sink's port added */
	event.type = GRAPH_PORT_ADDED;
	event.data.graph_port_added.comp = sink;
	event.data.graph_port_added.port = sink_def_port;
	ok(has_event(&event), "got the expected graph's port added event (for sink, initial)");

	/* Source's accept port connection */
	event.type = COMP_ACCEPT_PORT_CONNECTION;
	event.data.comp_accept_port_connection.comp = src;
	event.data.comp_accept_port_connection.self_port = src_def_port;
	event.data.comp_accept_port_connection.other_port = sink_def_port;
	ok(has_event(&event), "got the expected source's accept port connection event");
	src_accept_port_connection_pos = event_pos(&event);

	/* Sink's accept port connection */
	event.type = COMP_ACCEPT_PORT_CONNECTION;
	event.data.comp_accept_port_connection.comp = sink;
	event.data.comp_accept_port_connection.self_port = sink_def_port;
	event.data.comp_accept_port_connection.other_port = src_def_port;
	ok(has_event(&event), "got the expected sink's accept port connection event");

	/* Source's port connected */
	event.type = COMP_PORT_CONNECTED;
	event.data.comp_port_connected.comp = src;
	event.data.comp_port_connected.self_port = src_def_port;
	event.data.comp_port_connected.other_port = sink_def_port;
	ok(has_event(&event), "got the expected source's port connected event");
	src_port_connected_pos = event_pos(&event);

	/* Order of events */
	ok(src_accept_port_connection_pos < src_port_connected_pos,
		"event order is good (1)");

	bt_object_put_ref(graph);
	bt_object_put_ref(sink);
	bt_object_put_ref(src);
	bt_object_put_ref(conn);
	bt_object_put_ref(src_def_port);
	bt_object_put_ref(sink_def_port);
}

static
void test_sink_port_connected_error(void)
{
	struct bt_component *src;
	struct bt_component *sink;
	struct bt_graph *graph;
	struct bt_port *src_def_port;
	struct bt_port *sink_def_port;
	struct bt_connection *conn = NULL;
	struct event event;
	enum bt_graph_status status;
	size_t src_accept_port_connection_pos;
	size_t sink_accept_port_connection_pos;
	size_t src_port_connected_pos;
	size_t src_port_disconnected_pos;
	size_t sink_port_connected_pos;

	prepare_test(TEST_SINK_PORT_CONNECTED_ERROR, "port connected error: sink");
	graph = create_graph();
	BT_ASSERT(graph);
	src = create_src(graph);
	sink = create_sink(graph);
	src_def_port = bt_component_source_get_output_port_by_name(src, "out");
	BT_ASSERT(src_def_port);
	sink_def_port = bt_component_sink_get_input_port_by_name(sink, "in");
	BT_ASSERT(sink_def_port);
	status = bt_graph_connect_ports(graph, src_def_port, sink_def_port,
		&conn);
	ok(status != BT_GRAPH_STATUS_OK,
		"bt_graph_connect_ports() returns an error");
	ok(!conn, "returned connection is NULL");

	/* We're supposed to have 5 events */
	ok(events->len == 7, "we have the expected number of events");

	/* Source's port added */
	event.type = GRAPH_PORT_ADDED;
	event.data.graph_port_added.comp = src;
	event.data.graph_port_added.port = src_def_port;
	ok(has_event(&event), "got the expected graph's port added event (for source, initial)");

	/* Sink's port added */
	event.type = GRAPH_PORT_ADDED;
	event.data.graph_port_added.comp = sink;
	event.data.graph_port_added.port = sink_def_port;
	ok(has_event(&event), "got the expected graph's port added event (for sink, initial)");

	/* Source's accept port connection */
	event.type = COMP_ACCEPT_PORT_CONNECTION;
	event.data.comp_accept_port_connection.comp = src;
	event.data.comp_accept_port_connection.self_port = src_def_port;
	event.data.comp_accept_port_connection.other_port = sink_def_port;
	ok(has_event(&event), "got the expected source's accept port connection event");
	src_accept_port_connection_pos = event_pos(&event);

	/* Sink's accept port connection */
	event.type = COMP_ACCEPT_PORT_CONNECTION;
	event.data.comp_accept_port_connection.comp = sink;
	event.data.comp_accept_port_connection.self_port = sink_def_port;
	event.data.comp_accept_port_connection.other_port = src_def_port;
	ok(has_event(&event), "got the expected sink's accept port connection event");
	sink_accept_port_connection_pos = event_pos(&event);

	/* Source's port connected */
	event.type = COMP_PORT_CONNECTED;
	event.data.comp_port_connected.comp = src;
	event.data.comp_port_connected.self_port = src_def_port;
	event.data.comp_port_connected.other_port = sink_def_port;
	ok(has_event(&event), "got the expected source's port connected event");
	src_port_connected_pos = event_pos(&event);

	/* Sink's port connected */
	event.type = COMP_PORT_CONNECTED;
	event.data.comp_port_connected.comp = sink;
	event.data.comp_port_connected.self_port = sink_def_port;
	event.data.comp_port_connected.other_port = src_def_port;
	ok(has_event(&event), "got the expected sink's port connected event");
	sink_port_connected_pos = event_pos(&event);

	/* Source's port disconnected */
	event.type = COMP_PORT_DISCONNECTED;
	event.data.comp_port_disconnected.comp = src;
	event.data.comp_port_disconnected.port = src_def_port;
	ok(has_event(&event), "got the expected source's port disconnected event");
	src_port_disconnected_pos = event_pos(&event);

	/* Order of events */
	ok(src_accept_port_connection_pos < src_port_connected_pos,
		"event order is good (1)");
	ok(sink_accept_port_connection_pos < sink_port_connected_pos,
		"event order is good (2)");
	ok(sink_port_connected_pos < src_port_disconnected_pos,
		"event order is good (3)");

	bt_object_put_ref(graph);
	bt_object_put_ref(sink);
	bt_object_put_ref(src);
	bt_object_put_ref(conn);
	bt_object_put_ref(src_def_port);
	bt_object_put_ref(sink_def_port);
}

static
void test_empty_graph(void)
{
	struct bt_graph *graph;

	prepare_test(TEST_EMPTY_GRAPH, "empty graph");
	graph = create_graph();
	ok(events->len == 0, "empty graph generates no events");
	bt_object_put_ref(graph);
}

int main(int argc, char **argv)
{
	plan_tests(NR_TESTS);
	init_test();
	test_empty_graph();
	test_simple();
	test_src_port_connected_error();
	test_sink_port_connected_error();
	test_src_adds_port_in_port_connected();
	test_sink_removes_port_in_port_connected();
	test_sink_removes_port_in_port_connected_then_src_removes_disconnected_port();
	fini_test();
	return exit_status();
}
