/*
 * Copyright 2019 Efficios, Inc.
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

/*
 * Test that remove a trace class or trace destruction listener from within
 * a destruction listener of the same object works.
 */

#include <babeltrace2/babeltrace.h>
#include <common/assert.h>
#include <tap/tap.h>
#include <stdbool.h>

#define NR_TESTS 16

static bt_listener_id trace_class_destroyed_1_id;
static bt_listener_id trace_class_destroyed_2_id;
static bt_listener_id trace_class_destroyed_3_id;
static bt_listener_id trace_class_destroyed_4_id;
static bt_listener_id trace_class_destroyed_5_id;

static bt_listener_id trace_destroyed_1_id;
static bt_listener_id trace_destroyed_2_id;
static bt_listener_id trace_destroyed_3_id;
static bt_listener_id trace_destroyed_4_id;
static bt_listener_id trace_destroyed_5_id;

static bool trace_class_destroyed_1_called = false;
static bool trace_class_destroyed_2_called = false;
static bool trace_class_destroyed_3_called = false;
static bool trace_class_destroyed_4_called = false;
static bool trace_class_destroyed_5_called = false;

static bool trace_destroyed_1_called = false;
static bool trace_destroyed_2_called = false;
static bool trace_destroyed_3_called = false;
static bool trace_destroyed_4_called = false;
static bool trace_destroyed_5_called = false;

static
void trace_class_destroyed_1(const bt_trace_class *tc, void *data)
{
	trace_class_destroyed_1_called = true;
}

static
void trace_class_destroyed_2(const bt_trace_class *tc, void *data)
{
	bt_trace_class_remove_listener_status remove_listener_status;

	trace_class_destroyed_2_called = true;

	/* Remove self.  You shall not crash. */
	remove_listener_status = bt_trace_class_remove_destruction_listener(
		tc, trace_class_destroyed_2_id);
	ok(remove_listener_status == BT_TRACE_CLASS_REMOVE_LISTENER_STATUS_OK,
		"remove trace class listener 2 from 2");
}

static
void trace_class_destroyed_3(const bt_trace_class *tc, void *data)
{
	bt_trace_class_remove_listener_status remove_listener_status;

	trace_class_destroyed_3_called = true;

	/* Remove an already called listener. */
	remove_listener_status = bt_trace_class_remove_destruction_listener(
		tc, trace_class_destroyed_1_id);
	ok(remove_listener_status == BT_TRACE_CLASS_REMOVE_LISTENER_STATUS_OK,
		"remove trace class listener 1 from 3");
}

static
void trace_class_destroyed_4(const bt_trace_class *tc, void *data)
{
	bt_trace_class_remove_listener_status remove_listener_status;

	trace_class_destroyed_4_called = true;

	/* Remove a not yet called listener. */
	remove_listener_status = bt_trace_class_remove_destruction_listener(
		tc, trace_class_destroyed_5_id);
	ok(remove_listener_status == BT_TRACE_CLASS_REMOVE_LISTENER_STATUS_OK,
		"remove trace class listener 5 from 4");
}

static
void trace_class_destroyed_5(const bt_trace_class *tc, void *data)
{
	trace_class_destroyed_5_called = true;
}

static
void trace_destroyed_1(const bt_trace *t, void *data)
{
	trace_destroyed_1_called = true;
}

static
void trace_destroyed_2(const bt_trace *t, void *data)
{
	bt_trace_remove_listener_status remove_listener_status;

	trace_destroyed_2_called = true;

	/* Remove self.  You shall not crash. */
	remove_listener_status = bt_trace_remove_destruction_listener(
		t, trace_destroyed_2_id);
	ok(remove_listener_status == BT_TRACE_REMOVE_LISTENER_STATUS_OK,
		"remove trace listener 2 from 2");
}

static
void trace_destroyed_3(const bt_trace *t, void *data)
{
	bt_trace_remove_listener_status remove_listener_status;

	trace_destroyed_3_called = true;

	/* Remove an already called listener. */
	remove_listener_status = bt_trace_remove_destruction_listener(
		t, trace_destroyed_1_id);
	ok(remove_listener_status == BT_TRACE_REMOVE_LISTENER_STATUS_OK,
		"remove trace listener 1 from 3");
}

static
void trace_destroyed_4(const bt_trace *t, void *data)
{
	bt_trace_remove_listener_status remove_listener_status;

	trace_destroyed_4_called = true;

	/* Remove a not yet called listener. */
	remove_listener_status = bt_trace_remove_destruction_listener(
		t, trace_destroyed_5_id);
	ok(remove_listener_status == BT_TRACE_REMOVE_LISTENER_STATUS_OK,
		"remove trace listener 5 from 4");
}

static
void trace_destroyed_5(const bt_trace *t, void *data)
{
	trace_destroyed_5_called = true;
}

static
bt_component_class_initialize_method_status hello_init(
		bt_self_component_source *self_component,
		bt_self_component_source_configuration *config,
		const bt_value *params, void *init_method_data)
{
	bt_self_component *self_comp;
	bt_trace_class *tc;
	bt_trace *t;
	bt_trace_class_add_listener_status trace_class_add_listener_status;
	bt_trace_add_listener_status trace_add_listener_status;

	self_comp = bt_self_component_source_as_self_component(self_component);
	tc = bt_trace_class_create(self_comp);
	BT_ASSERT(tc);

	trace_class_add_listener_status = bt_trace_class_add_destruction_listener(
		tc, trace_class_destroyed_1, NULL, &trace_class_destroyed_1_id);
	BT_ASSERT(trace_class_add_listener_status == BT_TRACE_CLASS_ADD_LISTENER_STATUS_OK);

	trace_class_add_listener_status = bt_trace_class_add_destruction_listener(
		tc, trace_class_destroyed_2, NULL, &trace_class_destroyed_2_id);
	BT_ASSERT(trace_class_add_listener_status == BT_TRACE_CLASS_ADD_LISTENER_STATUS_OK);

	trace_class_add_listener_status = bt_trace_class_add_destruction_listener(
		tc, trace_class_destroyed_3, NULL, &trace_class_destroyed_3_id);
	BT_ASSERT(trace_class_add_listener_status == BT_TRACE_CLASS_ADD_LISTENER_STATUS_OK);

	trace_class_add_listener_status = bt_trace_class_add_destruction_listener(
		tc, trace_class_destroyed_4, NULL, &trace_class_destroyed_4_id);
	BT_ASSERT(trace_class_add_listener_status == BT_TRACE_CLASS_ADD_LISTENER_STATUS_OK);

	trace_class_add_listener_status = bt_trace_class_add_destruction_listener(
		tc, trace_class_destroyed_5, NULL, &trace_class_destroyed_5_id);
	BT_ASSERT(trace_class_add_listener_status == BT_TRACE_CLASS_ADD_LISTENER_STATUS_OK);

	t = bt_trace_create(tc);
	BT_ASSERT(t);

	trace_add_listener_status = bt_trace_add_destruction_listener(
		t, trace_destroyed_1, NULL, &trace_destroyed_1_id);
	BT_ASSERT(trace_add_listener_status == BT_TRACE_ADD_LISTENER_STATUS_OK);

	trace_add_listener_status = bt_trace_add_destruction_listener(
		t, trace_destroyed_2, NULL, &trace_destroyed_2_id);
	BT_ASSERT(trace_add_listener_status == BT_TRACE_ADD_LISTENER_STATUS_OK);

	trace_add_listener_status = bt_trace_add_destruction_listener(
		t, trace_destroyed_3, NULL, &trace_destroyed_3_id);
	BT_ASSERT(trace_add_listener_status == BT_TRACE_ADD_LISTENER_STATUS_OK);

	trace_add_listener_status = bt_trace_add_destruction_listener(
		t, trace_destroyed_4, NULL, &trace_destroyed_4_id);
	BT_ASSERT(trace_add_listener_status == BT_TRACE_ADD_LISTENER_STATUS_OK);

	trace_add_listener_status = bt_trace_add_destruction_listener(
		t, trace_destroyed_5, NULL, &trace_destroyed_5_id);
	BT_ASSERT(trace_add_listener_status == BT_TRACE_ADD_LISTENER_STATUS_OK);

	/* Destroy the trace. */
	bt_trace_put_ref(t);

	ok(trace_destroyed_1_called, "trace destruction listener 1 called");
	ok(trace_destroyed_2_called, "trace destruction listener 2 called");
	ok(trace_destroyed_3_called, "trace destruction listener 3 called");
	ok(trace_destroyed_4_called, "trace destruction listener 4 called");
	ok(!trace_destroyed_5_called, "trace destruction listener 5 not called");

	/* Destroy the trace class. */
	bt_trace_class_put_ref(tc);

	ok(trace_class_destroyed_1_called, "trace class destruction listener 1 called");
	ok(trace_class_destroyed_2_called, "trace class destruction listener 2 called");
	ok(trace_class_destroyed_3_called, "trace class destruction listener 3 called");
	ok(trace_class_destroyed_4_called, "trace class destruction listener 4 called");
	ok(!trace_class_destroyed_5_called, "trace class destruction listener 5 not called");

	return BT_COMPONENT_CLASS_INITIALIZE_METHOD_STATUS_OK;
}

static
bt_message_iterator_class_next_method_status hello_iter_next(
		bt_self_message_iterator *message_iterator,
		bt_message_array_const msgs, uint64_t capacity,
		uint64_t *count)
{
	BT_ASSERT(false);
	return BT_MESSAGE_ITERATOR_CLASS_NEXT_METHOD_STATUS_OK;
}

int main(int argc, char **argv)
{
	bt_graph *graph;
	bt_message_iterator_class *msg_iter_cls;
	bt_component_class_source *source_cc;
	bt_component_class_set_method_status set_method_status;
	bt_graph_add_component_status add_component_status;
	const bt_component_source *source;

	plan_tests(NR_TESTS);

	msg_iter_cls = bt_message_iterator_class_create(hello_iter_next);
	BT_ASSERT(msg_iter_cls);

	source_cc = bt_component_class_source_create("Hello", msg_iter_cls);
	BT_ASSERT(source_cc);

	set_method_status = bt_component_class_source_set_initialize_method(
		source_cc, hello_init);
	BT_ASSERT(set_method_status == BT_COMPONENT_CLASS_SET_METHOD_STATUS_OK);

	graph = bt_graph_create(0);
	BT_ASSERT(graph);

	add_component_status = bt_graph_add_source_component(
		graph, source_cc, "name", NULL,
		BT_LOGGING_LEVEL_WARNING, &source);
	BT_ASSERT(add_component_status == BT_GRAPH_ADD_COMPONENT_STATUS_OK);

	bt_component_class_source_put_ref(source_cc);
	bt_message_iterator_class_put_ref(msg_iter_cls);
	bt_graph_put_ref(graph);

	return exit_status();
}
