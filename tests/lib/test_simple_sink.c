/*
 * Copyright (c) 2019 Philippe Proulx <pproulx@efficios.com>
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

#include <babeltrace2/babeltrace.h>
#include "common/assert.h"
#include <string.h>
#include "tap/tap.h"

#define NR_TESTS 60

struct test_data {
	bt_graph_simple_sink_component_init_func_status init_status;
	bt_graph_simple_sink_component_consume_func_status consume_status;
};

static
bt_graph_simple_sink_component_init_func_status simple_init_func(
		bt_self_component_port_input_message_iterator *iterator,
		void *data)
{
	struct test_data *test_data = data;

	ok(iterator, "Message iterator is not NULL in initialization function");
	ok(data, "Data is not NULL in initialization function");
	return test_data->init_status;
}

static
bt_graph_simple_sink_component_consume_func_status simple_consume_func(
		bt_self_component_port_input_message_iterator *iterator,
		void *data)
{
	struct test_data *test_data = data;

	ok(iterator, "Message iterator is not NULL in consume function");
	ok(data, "Data is not NULL in consume function");
	return test_data->consume_status;
}

static
void simple_fini_func(void *data)
{
	ok(data, "Data is not NULL in finalization function");
}

static
bt_component_class_init_method_status src_init(
		bt_self_component_source *self_comp,
		const bt_value *params, void *init_method_data)
{
	bt_self_component_add_port_status status;

	status = bt_self_component_source_add_output_port(self_comp,
		"out", NULL, NULL);
	BT_ASSERT(status == BT_SELF_COMPONENT_ADD_PORT_STATUS_OK);
	return BT_COMPONENT_CLASS_INIT_METHOD_STATUS_OK;
}

static
bt_component_class_message_iterator_next_method_status src_iter_next(
		bt_self_message_iterator *message_iterator,
		bt_message_array_const msgs, uint64_t capacity,
		uint64_t *count)
{
	return BT_COMPONENT_CLASS_MESSAGE_ITERATOR_NEXT_METHOD_STATUS_END;
}

static
bt_graph *create_graph_with_source(const bt_port_output **out_port)
{
	bt_component_class_source *src_comp_cls;
	bt_graph *graph;
	const bt_component_source *src_comp = NULL;
	bt_graph_add_component_status add_comp_status;
	bt_component_class_set_method_status set_method_status;

	BT_ASSERT(out_port);
	src_comp_cls = bt_component_class_source_create("src", src_iter_next);
	BT_ASSERT(src_comp_cls);
	set_method_status = bt_component_class_source_set_init_method(
		src_comp_cls, src_init);
	BT_ASSERT(set_method_status == BT_COMPONENT_CLASS_SET_METHOD_STATUS_OK);
	graph = bt_graph_create(0);
	BT_ASSERT(graph);
	add_comp_status = bt_graph_add_source_component(graph, src_comp_cls,
		"src", NULL, BT_LOGGING_LEVEL_NONE, &src_comp);
	BT_ASSERT(add_comp_status == BT_GRAPH_ADD_COMPONENT_STATUS_OK);
	BT_ASSERT(src_comp);
	*out_port = bt_component_source_borrow_output_port_by_index_const(
		src_comp, 0);
	BT_ASSERT(*out_port);
	bt_component_source_put_ref(src_comp);
	bt_component_class_source_put_ref(src_comp_cls);
	return graph;
}

static
void test_simple_expect_run_once_status(
		bt_graph_simple_sink_component_init_func_status init_status,
		bt_graph_simple_sink_component_consume_func_status consume_status,
		bt_graph_run_once_status exp_run_once_status)
{
	const bt_port_output *src_out_port = NULL;
	bt_graph *graph;
	const bt_component_sink *sink_comp = NULL;
	const bt_port_input *sink_in_port;
	bt_graph_add_component_status add_comp_status;
	bt_graph_run_once_status run_once_status;
	bt_graph_connect_ports_status connect_status;
	struct test_data test_data = {
		.init_status = init_status,
		.consume_status = consume_status,
	};

	graph = create_graph_with_source(&src_out_port);
	BT_ASSERT(graph);
	BT_ASSERT(src_out_port);
	add_comp_status = bt_graph_add_simple_sink_component(graph, "sink",
		simple_init_func, simple_consume_func, simple_fini_func,
		&test_data, &sink_comp);
	BT_ASSERT(add_comp_status == BT_GRAPH_ADD_COMPONENT_STATUS_OK);
	BT_ASSERT(sink_comp);
	sink_in_port = bt_component_sink_borrow_input_port_by_name_const(
		sink_comp, "in");
	ok(sink_in_port,
		"Simple sink component has an input port named \"in\"");
	connect_status = bt_graph_connect_ports(graph, src_out_port,
		sink_in_port, NULL);
	ok(connect_status == BT_GRAPH_CONNECT_PORTS_STATUS_OK,
		"Simple sink component's \"in\" port is connectable");
	run_once_status = bt_graph_run_once(graph);
	ok(run_once_status == exp_run_once_status,
		"Graph \"run once\" status is the expected one (status code: %d)",
		run_once_status);
	bt_component_sink_put_ref(sink_comp);
	bt_graph_put_ref(graph);
}

int main(void)
{
	plan_tests(NR_TESTS);

	/* Test initialization function status */
	test_simple_expect_run_once_status(
		BT_GRAPH_SIMPLE_SINK_COMPONENT_INIT_FUNC_STATUS_OK,
		BT_GRAPH_SIMPLE_SINK_COMPONENT_CONSUME_FUNC_STATUS_OK,
		BT_GRAPH_RUN_ONCE_STATUS_OK);
	test_simple_expect_run_once_status(
		BT_GRAPH_SIMPLE_SINK_COMPONENT_INIT_FUNC_STATUS_ERROR,
		BT_GRAPH_SIMPLE_SINK_COMPONENT_CONSUME_FUNC_STATUS_OK,
		BT_GRAPH_RUN_ONCE_STATUS_ERROR);
	test_simple_expect_run_once_status(
		BT_GRAPH_SIMPLE_SINK_COMPONENT_INIT_FUNC_STATUS_MEMORY_ERROR,
		BT_GRAPH_SIMPLE_SINK_COMPONENT_CONSUME_FUNC_STATUS_OK,
		BT_GRAPH_RUN_ONCE_STATUS_MEMORY_ERROR);

	/* Test "consume" function status */
	test_simple_expect_run_once_status(
		BT_GRAPH_SIMPLE_SINK_COMPONENT_INIT_FUNC_STATUS_OK,
		BT_GRAPH_SIMPLE_SINK_COMPONENT_CONSUME_FUNC_STATUS_OK,
		BT_GRAPH_RUN_ONCE_STATUS_OK);
	test_simple_expect_run_once_status(
		BT_GRAPH_SIMPLE_SINK_COMPONENT_INIT_FUNC_STATUS_OK,
		BT_GRAPH_SIMPLE_SINK_COMPONENT_CONSUME_FUNC_STATUS_ERROR,
		BT_GRAPH_RUN_ONCE_STATUS_ERROR);
	test_simple_expect_run_once_status(
		BT_GRAPH_SIMPLE_SINK_COMPONENT_INIT_FUNC_STATUS_OK,
		BT_GRAPH_SIMPLE_SINK_COMPONENT_CONSUME_FUNC_STATUS_MEMORY_ERROR,
		BT_GRAPH_RUN_ONCE_STATUS_MEMORY_ERROR);
	test_simple_expect_run_once_status(
		BT_GRAPH_SIMPLE_SINK_COMPONENT_INIT_FUNC_STATUS_OK,
		BT_GRAPH_SIMPLE_SINK_COMPONENT_CONSUME_FUNC_STATUS_AGAIN,
		BT_GRAPH_RUN_ONCE_STATUS_AGAIN);
	test_simple_expect_run_once_status(
		BT_GRAPH_SIMPLE_SINK_COMPONENT_INIT_FUNC_STATUS_OK,
		BT_GRAPH_SIMPLE_SINK_COMPONENT_CONSUME_FUNC_STATUS_END,
		BT_GRAPH_RUN_ONCE_STATUS_END);

	return exit_status();
}
