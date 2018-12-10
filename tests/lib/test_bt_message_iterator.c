/*
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

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <inttypes.h>
#include <string.h>
#include <babeltrace/babeltrace.h>
#include <babeltrace/assert-internal.h>
#include <glib.h>

#include "tap/tap.h"

#define NR_TESTS	5

enum test {
	TEST_NO_AUTO_MSGS,
	TEST_OUTPUT_PORT_MESSAGE_ITERATOR,
};

enum test_event_type {
	TEST_EV_TYPE_MSG_UNEXPECTED,
	TEST_EV_TYPE_MSG_EVENT,
	TEST_EV_TYPE_MSG_STREAM_BEGIN,
	TEST_EV_TYPE_MSG_PACKET_BEGIN,
	TEST_EV_TYPE_MSG_PACKET_END,
	TEST_EV_TYPE_MSG_STREAM_END,
	TEST_EV_TYPE_END,
	TEST_EV_TYPE_SENTINEL,
};

struct test_event {
	enum test_event_type type;
	const bt_stream *stream;
	const bt_packet *packet;
};

static bool debug = false;
static enum test current_test;
static GArray *test_events;
static bt_graph *graph;
static bt_stream_class *src_stream_class;
static bt_event_class *src_event_class;
static bt_stream *src_stream1;
static bt_stream *src_stream2;
static bt_packet *src_stream1_packet1;
static bt_packet *src_stream1_packet2;
static bt_packet *src_stream2_packet1;
static bt_packet *src_stream2_packet2;

enum {
	SEQ_END = -1,
	SEQ_STREAM1_BEGIN = -2,
	SEQ_STREAM2_BEGIN = -3,
	SEQ_STREAM1_END = -4,
	SEQ_STREAM2_END = -5,
	SEQ_STREAM1_PACKET1_BEGIN = -6,
	SEQ_STREAM1_PACKET2_BEGIN = -7,
	SEQ_STREAM2_PACKET1_BEGIN = -8,
	SEQ_STREAM2_PACKET2_BEGIN = -9,
	SEQ_STREAM1_PACKET1_END = -10,
	SEQ_STREAM1_PACKET2_END = -11,
	SEQ_STREAM2_PACKET1_END = -12,
	SEQ_STREAM2_PACKET2_END = -13,
	SEQ_EVENT_STREAM1_PACKET1 = -14,
	SEQ_EVENT_STREAM1_PACKET2 = -15,
	SEQ_EVENT_STREAM2_PACKET1 = -16,
	SEQ_EVENT_STREAM2_PACKET2 = -17,
};

struct src_iter_user_data {
	int64_t *seq;
	size_t at;
};

struct sink_user_data {
	bt_self_component_port_input_message_iterator *msg_iter;
};

/*
 * No automatic messages generated in this block.
 * Stream 2 messages are more indented.
 */
static int64_t seq_no_auto_msgs[] = {
	SEQ_STREAM1_BEGIN,
	SEQ_STREAM1_PACKET1_BEGIN,
	SEQ_EVENT_STREAM1_PACKET1,
	SEQ_EVENT_STREAM1_PACKET1,
		SEQ_STREAM2_BEGIN,
	SEQ_EVENT_STREAM1_PACKET1,
		SEQ_STREAM2_PACKET2_BEGIN,
		SEQ_EVENT_STREAM2_PACKET2,
	SEQ_EVENT_STREAM1_PACKET1,
	SEQ_STREAM1_PACKET1_END,
		SEQ_STREAM2_PACKET2_END,
	SEQ_STREAM1_PACKET2_BEGIN,
	SEQ_EVENT_STREAM1_PACKET2,
		SEQ_STREAM2_END,
	SEQ_STREAM1_PACKET2_END,
	SEQ_STREAM1_END,
	SEQ_END,
};

static
void clear_test_events(void)
{
	g_array_set_size(test_events, 0);
}

static
void print_test_event(FILE *fp, const struct test_event *event)
{
	fprintf(fp, "{ type = ");

	switch (event->type) {
	case TEST_EV_TYPE_MSG_UNEXPECTED:
		fprintf(fp, "TEST_EV_TYPE_MSG_UNEXPECTED");
		break;
	case TEST_EV_TYPE_MSG_EVENT:
		fprintf(fp, "TEST_EV_TYPE_MSG_EVENT");
		break;
	case TEST_EV_TYPE_MSG_STREAM_BEGIN:
		fprintf(fp, "TEST_EV_TYPE_MSG_STREAM_BEGIN");
		break;
	case TEST_EV_TYPE_MSG_STREAM_END:
		fprintf(fp, "TEST_EV_TYPE_MSG_STREAM_END");
		break;
	case TEST_EV_TYPE_MSG_PACKET_BEGIN:
		fprintf(fp, "TEST_EV_TYPE_MSG_PACKET_BEGIN");
		break;
	case TEST_EV_TYPE_MSG_PACKET_END:
		fprintf(fp, "TEST_EV_TYPE_MSG_PACKET_END");
		break;
	case TEST_EV_TYPE_END:
		fprintf(fp, "TEST_EV_TYPE_END");
		break;
	case TEST_EV_TYPE_SENTINEL:
		fprintf(fp, "TEST_EV_TYPE_SENTINEL");
		break;
	default:
		fprintf(fp, "(UNKNOWN)");
		break;
	}

	fprintf(fp, ", stream = %p, packet = %p }", event->stream,
		event->packet);
}

static
void append_test_event(struct test_event *event)
{
	g_array_append_val(test_events, *event);
}

static
bool compare_single_test_events(const struct test_event *ev_a,
		const struct test_event *ev_b)
{
	if (debug) {
		fprintf(stderr, ":: Comparing test events: ");
		print_test_event(stderr, ev_a);
		fprintf(stderr, " vs. ");
		print_test_event(stderr, ev_b);
		fprintf(stderr, "\n");
	}

	if (ev_a->type != ev_b->type) {
		return false;
	}

	switch (ev_a->type) {
	case TEST_EV_TYPE_END:
	case TEST_EV_TYPE_SENTINEL:
		break;
	default:
		if (ev_a->stream != ev_b->stream) {
			return false;
		}

		if (ev_a->packet != ev_b->packet) {
			return false;
		}
		break;
	}

	return true;
}

static
bool compare_test_events(const struct test_event *expected_events)
{
	const struct test_event *expected_event = expected_events;
	size_t i = 0;

	BT_ASSERT(expected_events);

	while (true) {
		const struct test_event *event;

		if (expected_event->type == TEST_EV_TYPE_SENTINEL) {
			break;
		}

		if (i >= test_events->len) {
			return false;
		}

		event = &g_array_index(test_events, struct test_event, i);

		if (!compare_single_test_events(event, expected_event)) {
			return false;
		}

		i++;
		expected_event++;
	}

	if (i != test_events->len) {
		return false;
	}

	return true;
}

static
void init_static_data(bt_self_component_source *self_comp)
{
	bt_trace_class *trace_class;
	bt_trace *trace;

	/* Metadata, streams, and packets*/
	trace_class = bt_trace_class_create(
		bt_self_component_source_as_self_component(self_comp));
	BT_ASSERT(trace_class);
	src_stream_class = bt_stream_class_create(trace_class);
	BT_ASSERT(src_stream_class);
	src_event_class = bt_event_class_create(src_stream_class);
	BT_ASSERT(src_event_class);
	trace = bt_trace_create(trace_class);
	BT_ASSERT(trace);
	src_stream1 = bt_stream_create(src_stream_class, trace);
	BT_ASSERT(src_stream1);
	src_stream2 = bt_stream_create(src_stream_class, trace);
	BT_ASSERT(src_stream2);
	src_stream1_packet1 = bt_packet_create(src_stream1);
	BT_ASSERT(src_stream1_packet1);
	src_stream1_packet2 = bt_packet_create(src_stream1);
	BT_ASSERT(src_stream1_packet2);
	src_stream2_packet1 = bt_packet_create(src_stream2);
	BT_ASSERT(src_stream2_packet1);
	src_stream2_packet2 = bt_packet_create(src_stream2);
	BT_ASSERT(src_stream2_packet2);

	if (debug) {
		fprintf(stderr, ":: stream 1: %p\n", src_stream1);
		fprintf(stderr, ":: stream 2: %p\n", src_stream2);
		fprintf(stderr, ":: stream 1, packet 1: %p\n", src_stream1_packet1);
		fprintf(stderr, ":: stream 1, packet 2: %p\n", src_stream1_packet2);
		fprintf(stderr, ":: stream 2, packet 1: %p\n", src_stream2_packet1);
		fprintf(stderr, ":: stream 2, packet 2: %p\n", src_stream2_packet2);
	}

	bt_trace_put_ref(trace);
	bt_trace_class_put_ref(trace_class);
}

static
void fini_static_data(void)
{
	/* Metadata */
	bt_stream_class_put_ref(src_stream_class);
	bt_event_class_put_ref(src_event_class);
	bt_stream_put_ref(src_stream1);
	bt_stream_put_ref(src_stream2);
	bt_packet_put_ref(src_stream1_packet1);
	bt_packet_put_ref(src_stream1_packet2);
	bt_packet_put_ref(src_stream2_packet1);
	bt_packet_put_ref(src_stream2_packet2);
}

static
void src_iter_finalize(bt_self_message_iterator *self_msg_iter)
{
	struct src_iter_user_data *user_data =
		bt_self_message_iterator_get_data(
			self_msg_iter);

	if (user_data) {
		g_free(user_data);
	}
}

static
bt_self_message_iterator_status src_iter_init(
		bt_self_message_iterator *self_msg_iter,
		bt_self_component_source *self_comp,
		bt_self_component_port_output *self_port)
{
	struct src_iter_user_data *user_data =
		g_new0(struct src_iter_user_data, 1);

	BT_ASSERT(user_data);
	bt_self_message_iterator_set_data(self_msg_iter, user_data);

	switch (current_test) {
	case TEST_NO_AUTO_MSGS:
	case TEST_OUTPUT_PORT_MESSAGE_ITERATOR:
		user_data->seq = seq_no_auto_msgs;
		break;
	default:
		abort();
	}

	return BT_SELF_MESSAGE_ITERATOR_STATUS_OK;
}

static
void src_iter_next_seq_one(bt_self_message_iterator* msg_iter,
		struct src_iter_user_data *user_data,
		const bt_message **msg)
{
	bt_packet *event_packet = NULL;

	switch (user_data->seq[user_data->at]) {
	case SEQ_STREAM1_BEGIN:
		*msg = bt_message_stream_beginning_create(msg_iter,
								     src_stream1);
		break;
	case SEQ_STREAM2_BEGIN:
		*msg = bt_message_stream_beginning_create(msg_iter,
							     src_stream2);
		break;
	case SEQ_STREAM1_END:
		*msg = bt_message_stream_end_create(msg_iter,
							   src_stream1);
		break;
	case SEQ_STREAM2_END:
		*msg = bt_message_stream_end_create(msg_iter,
							   src_stream2);
		break;
	case SEQ_STREAM1_PACKET1_BEGIN:
		*msg = bt_message_packet_beginning_create(msg_iter,
							     src_stream1_packet1);
		break;
	case SEQ_STREAM1_PACKET2_BEGIN:
		*msg = bt_message_packet_beginning_create(msg_iter,
							     src_stream1_packet2);
		break;
	case SEQ_STREAM2_PACKET1_BEGIN:
		*msg = bt_message_packet_beginning_create(msg_iter,
							     src_stream2_packet1);
		break;
	case SEQ_STREAM2_PACKET2_BEGIN:
		*msg = bt_message_packet_beginning_create(msg_iter,
							     src_stream2_packet2);
		break;
	case SEQ_STREAM1_PACKET1_END:
		*msg = bt_message_packet_end_create(msg_iter,
							   src_stream1_packet1);
		break;
	case SEQ_STREAM1_PACKET2_END:
		*msg = bt_message_packet_end_create(msg_iter,
							   src_stream1_packet2);
		break;
	case SEQ_STREAM2_PACKET1_END:
		*msg = bt_message_packet_end_create(msg_iter,
							   src_stream2_packet1);
		break;
	case SEQ_STREAM2_PACKET2_END:
		*msg = bt_message_packet_end_create(msg_iter,
							   src_stream2_packet2);
		break;
	case SEQ_EVENT_STREAM1_PACKET1:
		event_packet = src_stream1_packet1;
		break;
	case SEQ_EVENT_STREAM1_PACKET2:
		event_packet = src_stream1_packet2;
		break;
	case SEQ_EVENT_STREAM2_PACKET1:
		event_packet = src_stream2_packet1;
		break;
	case SEQ_EVENT_STREAM2_PACKET2:
		event_packet = src_stream2_packet2;
		break;
	default:
		abort();
	}

	if (event_packet) {
		*msg = bt_message_event_create(msg_iter,
						      src_event_class,
						      event_packet);
	}

	BT_ASSERT(*msg);
	user_data->at++;
}

static
bt_self_message_iterator_status src_iter_next_seq(
		bt_self_message_iterator *msg_iter,
		struct src_iter_user_data *user_data,
		bt_message_array_const msgs, uint64_t capacity,
		uint64_t *count)
{
	bt_self_message_iterator_status status =
		BT_SELF_MESSAGE_ITERATOR_STATUS_OK;
	uint64_t i = 0;

	BT_ASSERT(user_data->seq);

	if (user_data->seq[user_data->at] == SEQ_END) {
		status = BT_SELF_MESSAGE_ITERATOR_STATUS_END;
		goto end;
	}

	while (i < capacity && user_data->seq[user_data->at] != SEQ_END) {
		src_iter_next_seq_one(msg_iter, user_data, &msgs[i]);
		i++;
	}

	BT_ASSERT(i > 0 && i <= capacity);
	*count = i;

end:
	return status;
}

static
bt_self_message_iterator_status src_iter_next(
		bt_self_message_iterator *self_msg_iter,
		bt_message_array_const msgs, uint64_t capacity,
		uint64_t *count)
{
	struct src_iter_user_data *user_data =
		bt_self_message_iterator_get_data(self_msg_iter);

	BT_ASSERT(user_data);
	return src_iter_next_seq(self_msg_iter, user_data, msgs,
		capacity, count);
}

static
bt_self_component_status src_init(
		bt_self_component_source *self_comp,
		const bt_value *params, void *init_method_data)
{
	int ret;

	init_static_data(self_comp);
	ret = bt_self_component_source_add_output_port(
		self_comp, "out", NULL, NULL);
	BT_ASSERT(ret == 0);
	return BT_SELF_COMPONENT_STATUS_OK;
}

static
void src_finalize(bt_self_component_source *self_comp)
{
}

static
void append_test_events_from_message(const bt_message *message)
{
	struct test_event test_event = { 0 };

	switch (bt_message_get_type(message)) {
	case BT_MESSAGE_TYPE_EVENT:
	{
		const bt_event *event;

		test_event.type = TEST_EV_TYPE_MSG_EVENT;
		event = bt_message_event_borrow_event_const(message);
		BT_ASSERT(event);
		test_event.packet = bt_event_borrow_packet_const(event);
		BT_ASSERT(test_event.packet);
		break;
	}
	case BT_MESSAGE_TYPE_STREAM_BEGINNING:
		test_event.type = TEST_EV_TYPE_MSG_STREAM_BEGIN;
		test_event.stream =
			bt_message_stream_beginning_borrow_stream_const(message);
		BT_ASSERT(test_event.stream);
		break;
	case BT_MESSAGE_TYPE_STREAM_END:
		test_event.type = TEST_EV_TYPE_MSG_STREAM_END;
		test_event.stream =
			bt_message_stream_end_borrow_stream_const(message);
		BT_ASSERT(test_event.stream);
		break;
	case BT_MESSAGE_TYPE_PACKET_BEGINNING:
		test_event.type = TEST_EV_TYPE_MSG_PACKET_BEGIN;
		test_event.packet =
			bt_message_packet_beginning_borrow_packet_const(message);
		BT_ASSERT(test_event.packet);
		break;
	case BT_MESSAGE_TYPE_PACKET_END:
		test_event.type = TEST_EV_TYPE_MSG_PACKET_END;
		test_event.packet =
			bt_message_packet_end_borrow_packet_const(message);
		BT_ASSERT(test_event.packet);
		break;
	default:
		test_event.type = TEST_EV_TYPE_MSG_UNEXPECTED;
		break;
	}

	if (test_event.packet) {
		test_event.stream = bt_packet_borrow_stream_const(
			test_event.packet);
		BT_ASSERT(test_event.stream);
	}

	append_test_event(&test_event);
}

static
bt_message_iterator_status common_consume(
		void *msg_iter, bool is_output_port_msg_iter)
{
	bt_message_iterator_status ret;
	bt_message_array_const messages = NULL;
	uint64_t count = 0;
	struct test_event test_event = { 0 };
	uint64_t i;

	BT_ASSERT(msg_iter);

	if (is_output_port_msg_iter) {
		ret = bt_port_output_message_iterator_next(msg_iter,
			&messages, &count);
	} else {
		ret = bt_self_component_port_input_message_iterator_next(
			msg_iter, &messages, &count);
	}

	if (ret < 0) {
		goto end;
	}

	switch (ret) {
	case BT_MESSAGE_ITERATOR_STATUS_END:
		test_event.type = TEST_EV_TYPE_END;
		append_test_event(&test_event);
		goto end;
	case BT_MESSAGE_ITERATOR_STATUS_AGAIN:
		abort();
	default:
		break;
	}

	BT_ASSERT(messages);
	BT_ASSERT(count > 0);

	for (i = 0; i < count; i++) {
		append_test_events_from_message(messages[i]);
		bt_message_put_ref(messages[i]);
	}

end:
	return ret;
}

static
bt_self_component_status sink_consume(
		bt_self_component_sink *self_comp)
{
	bt_self_component_status ret = BT_SELF_COMPONENT_STATUS_OK;
	struct sink_user_data *user_data =
		bt_self_component_get_data(
			bt_self_component_sink_as_self_component(
				self_comp));
	bt_message_iterator_status it_ret;

	BT_ASSERT(user_data && user_data->msg_iter);
	it_ret = common_consume(user_data->msg_iter, false);

	if (it_ret < 0) {
		ret = BT_SELF_COMPONENT_STATUS_ERROR;
		goto end;
	}

	switch (it_ret) {
	case BT_MESSAGE_ITERATOR_STATUS_END:
		ret = BT_SELF_COMPONENT_STATUS_END;
		BT_SELF_COMPONENT_PORT_INPUT_MESSAGE_ITERATOR_PUT_REF_AND_RESET(
			user_data->msg_iter);
		goto end;
	case BT_MESSAGE_ITERATOR_STATUS_AGAIN:
		abort();
	default:
		break;
	}

end:
	return ret;
}

static
bt_self_component_status sink_port_connected(
		bt_self_component_sink *self_comp,
		bt_self_component_port_input *self_port,
		const bt_port_output *other_port)
{
	struct sink_user_data *user_data =
		bt_self_component_get_data(
			bt_self_component_sink_as_self_component(
				self_comp));

	BT_ASSERT(user_data);
	user_data->msg_iter =
		bt_self_component_port_input_message_iterator_create(
			self_port);
	return BT_SELF_COMPONENT_STATUS_OK;
}

static
bt_self_component_status sink_init(
		bt_self_component_sink *self_comp,
		const bt_value *params, void *init_method_data)
{
	struct sink_user_data *user_data = g_new0(struct sink_user_data, 1);
	int ret;

	BT_ASSERT(user_data);
	bt_self_component_set_data(
		bt_self_component_sink_as_self_component(self_comp),
		user_data);
	ret = bt_self_component_sink_add_input_port(
		self_comp, "in", NULL, NULL);
	BT_ASSERT(ret == 0);
	return BT_SELF_COMPONENT_STATUS_OK;
}

static
void sink_finalize(bt_self_component_sink *self_comp)
{
	struct sink_user_data *user_data =
		bt_self_component_get_data(
			bt_self_component_sink_as_self_component(
				self_comp));

	if (user_data) {
		BT_SELF_COMPONENT_PORT_INPUT_MESSAGE_ITERATOR_PUT_REF_AND_RESET(
			user_data->msg_iter);
		g_free(user_data);
	}
}

static
void create_source_sink(bt_graph *graph,
		const bt_component_source **source,
		const bt_component_sink **sink)
{
	bt_component_class_source *src_comp_class;
	bt_component_class_sink *sink_comp_class;
	int ret;

	/* Create source component */
	if (source) {
		src_comp_class = bt_component_class_source_create("src",
			src_iter_next);
		BT_ASSERT(src_comp_class);
		ret = bt_component_class_source_set_init_method(
			src_comp_class, src_init);
		BT_ASSERT(ret == 0);
		ret = bt_component_class_source_set_finalize_method(
			src_comp_class, src_finalize);
		BT_ASSERT(ret == 0);
		ret = bt_component_class_source_set_message_iterator_init_method(
			src_comp_class, src_iter_init);
		BT_ASSERT(ret == 0);
		ret = bt_component_class_source_set_message_iterator_finalize_method(
			src_comp_class, src_iter_finalize);
		BT_ASSERT(ret == 0);
		ret = bt_graph_add_source_component(graph,
			src_comp_class, "source", NULL, source);
		BT_ASSERT(ret == 0);
		bt_component_class_source_put_ref(src_comp_class);
	}

	/* Create sink component */
	if (sink) {
		sink_comp_class = bt_component_class_sink_create("sink",
			sink_consume);
		BT_ASSERT(sink_comp_class);
		ret = bt_component_class_sink_set_init_method(
			sink_comp_class, sink_init);
		BT_ASSERT(ret == 0);
		ret = bt_component_class_sink_set_finalize_method(
			sink_comp_class, sink_finalize);
		ret = bt_component_class_sink_set_input_port_connected_method(
			sink_comp_class, sink_port_connected);
		BT_ASSERT(ret == 0);
		ret = bt_graph_add_sink_component(graph,
			sink_comp_class,
			"sink", NULL, sink);
		BT_ASSERT(ret == 0);
		bt_component_class_sink_put_ref(sink_comp_class);
	}
}

typedef void (*compare_func_t)(void);

static
void do_std_test(enum test test, const char *name, compare_func_t compare_func)
{
	const bt_component_source *src_comp;
	const bt_component_sink *sink_comp;
	const bt_port_output *upstream_port;
	const bt_port_input *downstream_port;
	bt_graph_status graph_status = BT_GRAPH_STATUS_OK;

	clear_test_events();
	current_test = test;
	diag("test: %s", name);
	BT_ASSERT(!graph);
	graph = bt_graph_create();
	BT_ASSERT(graph);
	create_source_sink(graph, &src_comp, &sink_comp);

	/* Connect source to sink */
	upstream_port =
		bt_component_source_borrow_output_port_by_name_const(
			src_comp, "out");
	BT_ASSERT(upstream_port);
	downstream_port = bt_component_sink_borrow_input_port_by_name_const(
		sink_comp, "in");
	BT_ASSERT(downstream_port);
	graph_status = bt_graph_connect_ports(graph, upstream_port,
		downstream_port, NULL);

	/* Run the graph until the end */
	while (graph_status == BT_GRAPH_STATUS_OK ||
			graph_status == BT_GRAPH_STATUS_AGAIN) {
		graph_status = bt_graph_run(graph);
	}

	ok(graph_status == BT_GRAPH_STATUS_END,
		"graph finishes without any error");

	/* Compare the resulting test events */
	if (compare_func) {
		compare_func();
	}

	bt_component_source_put_ref(src_comp);
	bt_component_sink_put_ref(sink_comp);
	fini_static_data();
	BT_GRAPH_PUT_REF_AND_RESET(graph);
}

static
void test_no_auto_msgs_compare(void)
{
	const struct test_event expected_test_events[] = {
		{ .type = TEST_EV_TYPE_MSG_STREAM_BEGIN, .stream = src_stream1, .packet = NULL, },
		{ .type = TEST_EV_TYPE_MSG_PACKET_BEGIN, .stream = src_stream1, .packet = src_stream1_packet1, },
		{ .type = TEST_EV_TYPE_MSG_EVENT, .stream = src_stream1, .packet = src_stream1_packet1, },
		{ .type = TEST_EV_TYPE_MSG_EVENT, .stream = src_stream1, .packet = src_stream1_packet1, },
		{ .type = TEST_EV_TYPE_MSG_STREAM_BEGIN, .stream = src_stream2, .packet = NULL, },
		{ .type = TEST_EV_TYPE_MSG_EVENT, .stream = src_stream1, .packet = src_stream1_packet1, },
		{ .type = TEST_EV_TYPE_MSG_PACKET_BEGIN, .stream = src_stream2, .packet = src_stream2_packet2, },
		{ .type = TEST_EV_TYPE_MSG_EVENT, .stream = src_stream2, .packet = src_stream2_packet2, },
		{ .type = TEST_EV_TYPE_MSG_EVENT, .stream = src_stream1, .packet = src_stream1_packet1, },
		{ .type = TEST_EV_TYPE_MSG_PACKET_END, .stream = src_stream1, .packet = src_stream1_packet1, },
		{ .type = TEST_EV_TYPE_MSG_PACKET_END, .stream = src_stream2, .packet = src_stream2_packet2, },
		{ .type = TEST_EV_TYPE_MSG_PACKET_BEGIN, .stream = src_stream1, .packet = src_stream1_packet2, },
		{ .type = TEST_EV_TYPE_MSG_EVENT, .stream = src_stream1, .packet = src_stream1_packet2, },
		{ .type = TEST_EV_TYPE_MSG_STREAM_END, .stream = src_stream2, .packet = NULL, },
		{ .type = TEST_EV_TYPE_MSG_PACKET_END, .stream = src_stream1, .packet = src_stream1_packet2, },
		{ .type = TEST_EV_TYPE_MSG_STREAM_END, .stream = src_stream1, .packet = NULL, },
		{ .type = TEST_EV_TYPE_END, },
		{ .type = TEST_EV_TYPE_SENTINEL, },
	};

	ok(compare_test_events(expected_test_events),
		"the produced sequence of test events is the expected one");
}

static
void test_no_auto_msgs(void)
{
	do_std_test(TEST_NO_AUTO_MSGS, "no automatic messages",
		test_no_auto_msgs_compare);
}

static
void test_output_port_message_iterator(void)
{
	const bt_component_source *src_comp;
	bt_port_output_message_iterator *msg_iter;
	bt_message_iterator_status iter_status =
		BT_MESSAGE_ITERATOR_STATUS_OK;
	const bt_port_output *upstream_port;

	clear_test_events();
	current_test = TEST_OUTPUT_PORT_MESSAGE_ITERATOR;
	diag("test: output port message iterator");
	BT_ASSERT(!graph);
	graph = bt_graph_create();
	BT_ASSERT(graph);
	create_source_sink(graph, &src_comp, NULL);

	/* Create message iterator on source's output port */
	upstream_port = bt_component_source_borrow_output_port_by_name_const(
		src_comp, "out");
	msg_iter = bt_port_output_message_iterator_create(graph, upstream_port);
	ok(msg_iter, "bt_private_output_port_message_iterator_create() succeeds");

	/* Consume the message iterator */
	while (iter_status == BT_MESSAGE_ITERATOR_STATUS_OK) {
		iter_status = common_consume(msg_iter, true);
	}

	ok(iter_status == BT_MESSAGE_ITERATOR_STATUS_END,
		"output port message iterator finishes without any error");

	/* Compare the resulting test events */
	{
		const struct test_event expected_test_events[] = {
			{ .type = TEST_EV_TYPE_MSG_STREAM_BEGIN, .stream = src_stream1, .packet = NULL, },
			{ .type = TEST_EV_TYPE_MSG_PACKET_BEGIN, .stream = src_stream1, .packet = src_stream1_packet1, },
			{ .type = TEST_EV_TYPE_MSG_EVENT, .stream = src_stream1, .packet = src_stream1_packet1, },
			{ .type = TEST_EV_TYPE_MSG_EVENT, .stream = src_stream1, .packet = src_stream1_packet1, },
			{ .type = TEST_EV_TYPE_MSG_STREAM_BEGIN, .stream = src_stream2, .packet = NULL, },
			{ .type = TEST_EV_TYPE_MSG_EVENT, .stream = src_stream1, .packet = src_stream1_packet1, },
			{ .type = TEST_EV_TYPE_MSG_PACKET_BEGIN, .stream = src_stream2, .packet = src_stream2_packet2, },
			{ .type = TEST_EV_TYPE_MSG_EVENT, .stream = src_stream2, .packet = src_stream2_packet2, },
			{ .type = TEST_EV_TYPE_MSG_EVENT, .stream = src_stream1, .packet = src_stream1_packet1, },
			{ .type = TEST_EV_TYPE_MSG_PACKET_END, .stream = src_stream1, .packet = src_stream1_packet1, },
			{ .type = TEST_EV_TYPE_MSG_PACKET_END, .stream = src_stream2, .packet = src_stream2_packet2, },
			{ .type = TEST_EV_TYPE_MSG_PACKET_BEGIN, .stream = src_stream1, .packet = src_stream1_packet2, },
			{ .type = TEST_EV_TYPE_MSG_EVENT, .stream = src_stream1, .packet = src_stream1_packet2, },
			{ .type = TEST_EV_TYPE_MSG_STREAM_END, .stream = src_stream2, .packet = NULL, },
			{ .type = TEST_EV_TYPE_MSG_PACKET_END, .stream = src_stream1, .packet = src_stream1_packet2, },
			{ .type = TEST_EV_TYPE_MSG_STREAM_END, .stream = src_stream1, .packet = NULL, },
			{ .type = TEST_EV_TYPE_END, },
			{ .type = TEST_EV_TYPE_SENTINEL, },
		};

		ok(compare_test_events(expected_test_events),
			"the produced sequence of test events is the expected one");
	}

	fini_static_data();
	bt_component_source_put_ref(src_comp);
	BT_GRAPH_PUT_REF_AND_RESET(graph);
	bt_port_output_message_iterator_put_ref(msg_iter);
}

#define DEBUG_ENV_VAR	"TEST_BT_MESSAGE_ITERATOR_DEBUG"

int main(int argc, char **argv)
{
	if (getenv(DEBUG_ENV_VAR) && strcmp(getenv(DEBUG_ENV_VAR), "1") == 0) {
		debug = true;
	}

	plan_tests(NR_TESTS);
	test_events = g_array_new(FALSE, TRUE, sizeof(struct test_event));
	BT_ASSERT(test_events);
	test_no_auto_msgs();
	test_output_port_message_iterator();
	g_array_free(test_events, TRUE);
	return exit_status();
}
