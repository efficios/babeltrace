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
#include <babeltrace/assert-internal.h>
#include <babeltrace/babeltrace.h>
#include <glib.h>

#include "tap/tap.h"

#define NR_TESTS	12

enum test {
	TEST_NO_TS,
	TEST_NO_UPSTREAM_CONNECTION,
	TEST_SIMPLE_4_PORTS,
	TEST_4_PORTS_WITH_RETRIES,
	TEST_SINGLE_END_THEN_MULTIPLE_FULL,
	TEST_SINGLE_AGAIN_END_THEN_MULTIPLE_FULL,
};

enum test_event_type {
	TEST_EV_TYPE_NOTIF_UNEXPECTED,
	TEST_EV_TYPE_NOTIF_EVENT,
	TEST_EV_TYPE_NOTIF_INACTIVITY,
	TEST_EV_TYPE_NOTIF_PACKET_BEGIN,
	TEST_EV_TYPE_NOTIF_PACKET_END,
	TEST_EV_TYPE_NOTIF_STREAM_BEGIN,
	TEST_EV_TYPE_NOTIF_STREAM_END,
	TEST_EV_TYPE_AGAIN,
	TEST_EV_TYPE_END,
	TEST_EV_TYPE_SENTINEL,
};

struct test_event {
	enum test_event_type type;
	int64_t ts_ns;
};

struct source_muxer_sink {
	struct bt_component *source;
	struct bt_component *muxer;
	struct bt_component *sink;
};

struct graph_listener_data {
	struct bt_graph *graph;
	struct bt_component *source;
	struct bt_component *muxer;
	struct bt_component *sink;
};

static bool debug = false;
static enum test current_test;
static GArray *test_events;
static struct bt_graph *graph;
static struct bt_clock_class_priority_map *src_cc_prio_map;
static struct bt_clock_class_priority_map *src_empty_cc_prio_map;
static struct bt_clock_class *src_clock_class;
static struct bt_stream_class *src_stream_class;
static struct bt_event_class *src_event_class;
static struct bt_packet *src_packet0;
static struct bt_packet *src_packet1;
static struct bt_packet *src_packet2;
static struct bt_packet *src_packet3;

enum {
	SEQ_END = -1,
	SEQ_AGAIN = -2,
	SEQ_PACKET_BEGIN = -3,
	SEQ_PACKET_END = -4,
	SEQ_STREAM_BEGIN = -5,
	SEQ_STREAM_END = -6,
};

struct src_iter_user_data {
	size_t iter_index;
	int64_t *seq;
	size_t at;
	struct bt_packet *packet;
};

struct sink_user_data {
	struct bt_notification_iterator *notif_iter;
};

static int64_t seq1[] = {
	SEQ_STREAM_BEGIN, SEQ_PACKET_BEGIN, 24, 53, 97, 105, 119, 210,
	222, 240, 292, 317, 353, 407, 433, 473, 487, 504, 572, 615, 708,
	766, 850, 852, 931, 951, 956, 996, SEQ_PACKET_END,
	SEQ_STREAM_END, SEQ_END,
};

static int64_t seq2[] = {
	SEQ_STREAM_BEGIN, SEQ_PACKET_BEGIN, 51, 59, 68, 77, 91, 121,
	139, 170, 179, 266, 352, 454, 478, 631, 644, 668, 714, 744, 750,
	778, 790, 836, SEQ_PACKET_END, SEQ_STREAM_END, SEQ_END,
};

static int64_t seq3[] = {
	SEQ_STREAM_BEGIN, SEQ_PACKET_BEGIN, 8, 71, 209, 254, 298, 320,
	350, 393, 419, 624, 651, 678, 717, 731, 733, 788, 819, 820, 857,
	892, 903, 944, 998, SEQ_PACKET_END, SEQ_STREAM_END, SEQ_END,
};

static int64_t seq4[] = {
	SEQ_STREAM_BEGIN, SEQ_PACKET_BEGIN, 41, 56, 120, 138, 154, 228,
	471, 479, 481, 525, 591, 605, 612, 618, 632, 670, 696, 825, 863,
	867, 871, 884, 953, 985, 999, SEQ_PACKET_END, SEQ_STREAM_END,
	SEQ_END,
};

static int64_t seq1_with_again[] = {
	SEQ_STREAM_BEGIN, SEQ_PACKET_BEGIN, 24, 53, 97, 105, 119, 210,
	SEQ_AGAIN, SEQ_AGAIN, 222, 240, 292, 317, 353, 407, 433, 473,
	487, 504, 572, 615, 708, 766, 850, 852, 931, 951, 956, 996,
	SEQ_PACKET_END, SEQ_STREAM_END, SEQ_END,
};

static int64_t seq2_with_again[] = {
	SEQ_STREAM_BEGIN, SEQ_PACKET_BEGIN, 51, 59, 68, 77, 91, 121,
	139, 170, 179, 266, 352, 454, 478, 631, 644, 668, 714, 744, 750,
	778, 790, 836, SEQ_AGAIN, SEQ_AGAIN, SEQ_PACKET_END,
	SEQ_STREAM_END, SEQ_END,
};

static int64_t seq3_with_again[] = {
	SEQ_STREAM_BEGIN, SEQ_PACKET_BEGIN, 8, 71, 209, 254, 298, 320,
	350, 393, 419, 624, 651, SEQ_AGAIN, SEQ_AGAIN, 678, 717, 731,
	733, 788, 819, 820, 857, 892, 903, 944, 998, SEQ_PACKET_END,
	SEQ_STREAM_END, SEQ_END,
};

static int64_t seq4_with_again[] = {
	SEQ_AGAIN, SEQ_STREAM_BEGIN, SEQ_PACKET_BEGIN, 41, 56, 120, 138,
	154, 228, 471, 479, 481, 525, 591, 605, 612, 618, 632, 670, 696,
	825, 863, 867, 871, 884, 953, 985, 999, SEQ_PACKET_END,
	SEQ_STREAM_END, SEQ_END,
};

static int64_t seq5[] = {
	SEQ_STREAM_BEGIN, SEQ_PACKET_BEGIN, 1, 4, 189, 1001,
	SEQ_PACKET_END, SEQ_STREAM_END, SEQ_END,
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
	case TEST_EV_TYPE_NOTIF_UNEXPECTED:
		fprintf(fp, "TEST_EV_TYPE_NOTIF_UNEXPECTED");
		break;
	case TEST_EV_TYPE_NOTIF_EVENT:
		fprintf(fp, "TEST_EV_TYPE_NOTIF_EVENT");
		break;
	case TEST_EV_TYPE_NOTIF_INACTIVITY:
		fprintf(fp, "TEST_EV_TYPE_NOTIF_INACTIVITY");
		break;
	case TEST_EV_TYPE_NOTIF_PACKET_BEGIN:
		fprintf(fp, "TEST_EV_TYPE_NOTIF_PACKET_BEGIN");
		break;
	case TEST_EV_TYPE_NOTIF_PACKET_END:
		fprintf(fp, "TEST_EV_TYPE_NOTIF_PACKET_END");
		break;
	case TEST_EV_TYPE_NOTIF_STREAM_BEGIN:
		fprintf(fp, "TEST_EV_TYPE_NOTIF_STREAM_BEGIN");
		break;
	case TEST_EV_TYPE_NOTIF_STREAM_END:
		fprintf(fp, "TEST_EV_TYPE_NOTIF_STREAM_END");
		break;
	case TEST_EV_TYPE_AGAIN:
		fprintf(fp, "TEST_EV_TYPE_AGAIN");
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

	switch (event->type) {
	case TEST_EV_TYPE_NOTIF_EVENT:
	case TEST_EV_TYPE_NOTIF_INACTIVITY:
		fprintf(fp, ", ts-ns = %" PRId64, event->ts_ns);
	default:
		break;
	}

	fprintf(fp, " }");
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
		case TEST_EV_TYPE_NOTIF_EVENT:
		case TEST_EV_TYPE_NOTIF_INACTIVITY:
			if (ev_a->ts_ns != ev_b->ts_ns) {
				return false;
			}
			break;
		default:
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
		if (debug) {
			fprintf(stderr, ":: Length mismatch\n");
		}

		return false;
	}

	return true;
}

static
void init_static_data(void)
{
	int ret;
	struct bt_trace *trace;
	struct bt_stream *stream;
	struct bt_field_type *empty_struct_ft;
	struct bt_field_type *eh_ft;
	struct bt_field_type *eh_ts_ft;

	/* Test events */
	test_events = g_array_new(FALSE, TRUE, sizeof(struct test_event));
	BT_ASSERT(test_events);

	/* Metadata */
	empty_struct_ft = bt_field_type_structure_create();
	BT_ASSERT(empty_struct_ft);
	src_clock_class = bt_clock_class_create("my-clock", 1000000000);
	BT_ASSERT(src_clock_class);
	trace = bt_trace_create();
	BT_ASSERT(trace);
	ret = bt_trace_set_native_byte_order(trace,
		BT_BYTE_ORDER_LITTLE_ENDIAN);
	BT_ASSERT(ret == 0);
	ret = bt_trace_set_packet_header_field_type(trace, empty_struct_ft);
	BT_ASSERT(ret == 0);
	ret = bt_clock_class_set_is_absolute(src_clock_class, 1);
	BT_ASSERT(ret == 0);
	ret = bt_trace_add_clock_class(trace, src_clock_class);
	BT_ASSERT(ret == 0);
	src_empty_cc_prio_map = bt_clock_class_priority_map_create();
	BT_ASSERT(src_empty_cc_prio_map);
	src_cc_prio_map = bt_clock_class_priority_map_create();
	BT_ASSERT(src_cc_prio_map);
	ret = bt_clock_class_priority_map_add_clock_class(src_cc_prio_map,
		src_clock_class, 0);
	BT_ASSERT(ret == 0);
	src_stream_class = bt_stream_class_create("my-stream-class");
	BT_ASSERT(src_stream_class);
	ret = bt_stream_class_set_packet_context_field_type(src_stream_class,
		empty_struct_ft);
	BT_ASSERT(ret == 0);
	eh_ft = bt_field_type_structure_create();
	BT_ASSERT(eh_ft);
	eh_ts_ft = bt_field_type_integer_create(64);
	BT_ASSERT(eh_ts_ft);
	ret = bt_field_type_structure_add_field(eh_ft, eh_ts_ft, "ts");
	BT_ASSERT(ret == 0);
	ret = bt_field_type_integer_set_mapped_clock_class(eh_ts_ft,
		src_clock_class);
	BT_ASSERT(ret == 0);
	ret = bt_stream_class_set_event_header_field_type(src_stream_class,
		eh_ft);
	BT_ASSERT(ret == 0);
	ret = bt_stream_class_set_event_context_field_type(src_stream_class,
		empty_struct_ft);
	BT_ASSERT(ret == 0);
	src_event_class = bt_event_class_create("my-event-class");
	ret = bt_event_class_set_context_field_type(src_event_class,
		empty_struct_ft);
	BT_ASSERT(ret == 0);
	ret = bt_event_class_set_context_field_type(src_event_class,
		empty_struct_ft);
	BT_ASSERT(ret == 0);
	ret = bt_stream_class_add_event_class(src_stream_class,
		src_event_class);
	BT_ASSERT(ret == 0);
	ret = bt_trace_add_stream_class(trace, src_stream_class);
	BT_ASSERT(ret == 0);
	stream = bt_stream_create(src_stream_class, "stream0", 0);
	BT_ASSERT(stream);
	src_packet0 = bt_packet_create(stream);
	BT_ASSERT(src_packet0);
	bt_put(stream);
	stream = bt_stream_create(src_stream_class, "stream1", 1);
	BT_ASSERT(stream);
	src_packet1 = bt_packet_create(stream);
	BT_ASSERT(src_packet0);
	bt_put(stream);
	stream = bt_stream_create(src_stream_class, "stream2", 2);
	BT_ASSERT(stream);
	src_packet2 = bt_packet_create(stream);
	BT_ASSERT(src_packet0);
	bt_put(stream);
	stream = bt_stream_create(src_stream_class, "stream3", 3);
	BT_ASSERT(stream);
	src_packet3 = bt_packet_create(stream);
	BT_ASSERT(src_packet0);
	bt_put(stream);

	bt_put(trace);
	bt_put(empty_struct_ft);
	bt_put(eh_ft);
	bt_put(eh_ts_ft);
}

static
void fini_static_data(void)
{
	/* Test events */
	g_array_free(test_events, TRUE);

	/* Metadata */
	bt_put(src_empty_cc_prio_map);
	bt_put(src_cc_prio_map);
	bt_put(src_clock_class);
	bt_put(src_stream_class);
	bt_put(src_event_class);
	bt_put(src_packet0);
	bt_put(src_packet1);
	bt_put(src_packet2);
	bt_put(src_packet3);
}

static
void src_iter_finalize(
		struct bt_private_connection_private_notification_iterator *private_notification_iterator)
{
	struct src_iter_user_data *user_data =
		bt_private_connection_private_notification_iterator_get_user_data(
			private_notification_iterator);

	if (user_data) {
		g_free(user_data);
	}
}

static
enum bt_notification_iterator_status src_iter_init(
		struct bt_private_connection_private_notification_iterator *priv_notif_iter,
		struct bt_private_port *private_port)
{
	struct src_iter_user_data *user_data =
		g_new0(struct src_iter_user_data, 1);
	struct bt_port *port = bt_port_borrow_from_private(private_port);
	const char *port_name;
	int ret;

	BT_ASSERT(user_data);
	BT_ASSERT(port);
	ret = bt_private_connection_private_notification_iterator_set_user_data(priv_notif_iter,
		user_data);
	BT_ASSERT(ret == 0);
	port_name = bt_port_get_name(port);
	BT_ASSERT(port_name);
	user_data->iter_index = port_name[3] - '0';

	switch (user_data->iter_index) {
	case 0:
		user_data->packet = src_packet0;
		break;
	case 1:
		user_data->packet = src_packet1;
		break;
	case 2:
		user_data->packet = src_packet2;
		break;
	case 3:
		user_data->packet = src_packet3;
		break;
	default:
		abort();
	}

	switch (current_test) {
	case TEST_NO_TS:
		if (user_data->iter_index == 1) {
			user_data->seq = seq5;
		}
		break;
	case TEST_SIMPLE_4_PORTS:
		if (user_data->iter_index == 0) {
			user_data->seq = seq1;
		} else if (user_data->iter_index == 1) {
			user_data->seq = seq2;
		} else if (user_data->iter_index == 2) {
			user_data->seq = seq3;
		} else {
			user_data->seq = seq4;
		}
		break;
	case TEST_4_PORTS_WITH_RETRIES:
		if (user_data->iter_index == 0) {
			user_data->seq = seq1_with_again;
		} else if (user_data->iter_index == 1) {
			user_data->seq = seq2_with_again;
		} else if (user_data->iter_index == 2) {
			user_data->seq = seq3_with_again;
		} else {
			user_data->seq = seq4_with_again;
		}
		break;
	case TEST_SINGLE_END_THEN_MULTIPLE_FULL:
	case TEST_SINGLE_AGAIN_END_THEN_MULTIPLE_FULL:
		if (user_data->iter_index == 0) {
			/* Ignore: this iterator only returns END */
		} else if (user_data->iter_index == 1) {
			user_data->seq = seq2;
		} else {
			user_data->seq = seq3;
		}
		break;
	default:
		abort();
	}

	return BT_NOTIFICATION_ITERATOR_STATUS_OK;
}

static
struct bt_notification *src_create_event_notif(struct bt_packet *packet,
		struct bt_clock_class_priority_map *cc_prio_map, int64_t ts_ns)
{
	int ret;
	struct bt_event *event;
	struct bt_notification *notif;
	struct bt_clock_value *clock_value;
	struct bt_field *field;

	notif = bt_notification_event_create(graph, src_event_class,
		packet, cc_prio_map);
	BT_ASSERT(notif);
	event = bt_notification_event_borrow_event(notif);
	BT_ASSERT(event);
	field = bt_event_borrow_header(event);
	BT_ASSERT(field);
	field = bt_field_structure_borrow_field_by_name(field, "ts");
	BT_ASSERT(field);
	ret = bt_field_integer_unsigned_set_value(field, (uint64_t) ts_ns);
	BT_ASSERT(ret == 0);
	clock_value = bt_event_borrow_clock_value(event, src_clock_class);
	BT_ASSERT(clock_value);
	ret = bt_clock_value_set_value(clock_value, (uint64_t) ts_ns);
	BT_ASSERT(ret == 0);
	return notif;
}

static
enum bt_notification_iterator_status src_iter_next_seq(
		struct src_iter_user_data *user_data,
		bt_notification_array notifs)
{
	enum bt_notification_iterator_status status =
		BT_NOTIFICATION_ITERATOR_STATUS_OK;
	int64_t cur_ts_ns;
	struct bt_stream *stream;

	BT_ASSERT(user_data->seq);
	cur_ts_ns = user_data->seq[user_data->at];

	switch (cur_ts_ns) {
	case SEQ_END:
		status = BT_NOTIFICATION_ITERATOR_STATUS_END;
		break;
	case SEQ_AGAIN:
		status = BT_NOTIFICATION_ITERATOR_STATUS_AGAIN;
		break;
	case SEQ_PACKET_BEGIN:
		notifs[0] = bt_notification_packet_begin_create(graph,
				user_data->packet);
		BT_ASSERT(notifs[0]);
		break;
	case SEQ_PACKET_END:
		notifs[0] = bt_notification_packet_end_create(graph,
				user_data->packet);
		BT_ASSERT(notifs[0]);
		break;
	case SEQ_STREAM_BEGIN:
		stream = bt_packet_get_stream(user_data->packet);
		notifs[0] = bt_notification_stream_begin_create(graph, stream);
		BT_ASSERT(notifs[0]);
		bt_put(stream);
		break;
	case SEQ_STREAM_END:
		stream = bt_packet_get_stream(user_data->packet);
		notifs[0] = bt_notification_stream_end_create(graph, stream);
		BT_ASSERT(notifs[0]);
		bt_put(stream);
		break;
	default:
	{
		notifs[0] = src_create_event_notif(user_data->packet,
			src_cc_prio_map, cur_ts_ns);
		BT_ASSERT(notifs[0]);
		break;
	}
	}

	if (status != BT_NOTIFICATION_ITERATOR_STATUS_END) {
		user_data->at++;
	}

	return status;
}

static
enum bt_notification_iterator_status src_iter_next(
		struct bt_private_connection_private_notification_iterator *priv_iterator,
		bt_notification_array notifs, uint64_t capacity,
		uint64_t *count)
{
	enum bt_notification_iterator_status status =
		BT_NOTIFICATION_ITERATOR_STATUS_OK;
	struct src_iter_user_data *user_data =
		bt_private_connection_private_notification_iterator_get_user_data(priv_iterator);
	struct bt_private_component *private_component =
		bt_private_connection_private_notification_iterator_get_private_component(priv_iterator);
	struct bt_stream *stream;
	int ret;

	BT_ASSERT(user_data);
	BT_ASSERT(private_component);

	/*
	 * We can always set it to 1: it's not going to be considered
	 * anyway if the status is not
	 * BT_NOTIFICATION_ITERATOR_STATUS_OK.
	 */
	*count = 1;

	switch (current_test) {
	case TEST_NO_TS:
		if (user_data->iter_index == 0) {
			if (user_data->at == 0) {
				stream = bt_packet_get_stream(user_data->packet);
				notifs[0] =
					bt_notification_stream_begin_create(
						graph, stream);
				bt_put(stream);
				BT_ASSERT(notifs[0]);
			} else if (user_data->at == 1) {
				notifs[0] =
					bt_notification_packet_begin_create(
						graph, user_data->packet);
				BT_ASSERT(notifs[0]);
			} else if (user_data->at < 7) {
				notifs[0] =
					src_create_event_notif(
						user_data->packet,
						src_empty_cc_prio_map, 0);
				BT_ASSERT(notifs[0]);
			} else if (user_data->at == 7) {
				notifs[0] =
					bt_notification_packet_end_create(
						graph, user_data->packet);
				BT_ASSERT(notifs[0]);
			} else if (user_data->at == 8) {
				stream = bt_packet_get_stream(user_data->packet);
				notifs[0] =
					bt_notification_stream_end_create(
						graph, stream);
				bt_put(stream);
				BT_ASSERT(notifs[0]);
			} else {
				status =
					BT_NOTIFICATION_ITERATOR_STATUS_END;
			}

			user_data->at++;
		} else {
			status = src_iter_next_seq(user_data, notifs);
		}
		break;
	case TEST_SIMPLE_4_PORTS:
	case TEST_4_PORTS_WITH_RETRIES:
		status = src_iter_next_seq(user_data, notifs);
		break;
	case TEST_SINGLE_END_THEN_MULTIPLE_FULL:
		if (user_data->iter_index == 0) {
			ret = bt_private_component_source_add_output_private_port(
				private_component, "out1", NULL, NULL);
			BT_ASSERT(ret == 0);
			ret = bt_private_component_source_add_output_private_port(
				private_component, "out2", NULL, NULL);
			BT_ASSERT(ret == 0);
			status = BT_NOTIFICATION_ITERATOR_STATUS_END;
		} else {
			status = src_iter_next_seq(user_data, notifs);
		}
		break;
	case TEST_SINGLE_AGAIN_END_THEN_MULTIPLE_FULL:
		if (user_data->iter_index == 0) {
			if (user_data->at == 0) {
				status = BT_NOTIFICATION_ITERATOR_STATUS_AGAIN;
				user_data->at++;
			} else {
				ret = bt_private_component_source_add_output_private_port(
					private_component, "out1", NULL, NULL);
				BT_ASSERT(ret == 0);
				ret = bt_private_component_source_add_output_private_port(
					private_component, "out2", NULL, NULL);
				BT_ASSERT(ret == 0);
				status = BT_NOTIFICATION_ITERATOR_STATUS_END;
			}
		} else {
			status = src_iter_next_seq(user_data, notifs);
		}
		break;
	default:
		abort();
	}

	bt_put(private_component);
	return status;
}

static
enum bt_component_status src_init(
		struct bt_private_component *private_component,
		struct bt_value *params, void *init_method_data)
{
	int ret;
	size_t nb_ports;

	switch (current_test) {
	case TEST_NO_TS:
		nb_ports = 2;
		break;
	case TEST_SINGLE_END_THEN_MULTIPLE_FULL:
	case TEST_SINGLE_AGAIN_END_THEN_MULTIPLE_FULL:
		nb_ports = 1;
		break;
	default:
		nb_ports = 4;
		break;
	}

	if (nb_ports >= 1) {
		ret = bt_private_component_source_add_output_private_port(
			private_component, "out0", NULL, NULL);
		BT_ASSERT(ret == 0);
	}

	if (nb_ports >= 2) {
		ret = bt_private_component_source_add_output_private_port(
			private_component, "out1", NULL, NULL);
		BT_ASSERT(ret == 0);
	}

	if (nb_ports >= 3) {
		ret = bt_private_component_source_add_output_private_port(
			private_component, "out2", NULL, NULL);
		BT_ASSERT(ret == 0);
	}

	if (nb_ports >= 4) {
		ret = bt_private_component_source_add_output_private_port(
			private_component, "out3", NULL, NULL);
		BT_ASSERT(ret == 0);
	}

	return BT_COMPONENT_STATUS_OK;
}

static
void src_finalize(struct bt_private_component *private_component)
{
}

static
void append_test_event_from_notification(struct bt_notification *notification)
{
	int ret;
	struct test_event test_event;

	switch (bt_notification_get_type(notification)) {
	case BT_NOTIFICATION_TYPE_EVENT:
	{
		struct bt_event *event;
		struct bt_clock_class_priority_map *cc_prio_map;

		test_event.type = TEST_EV_TYPE_NOTIF_EVENT;
		cc_prio_map =
			bt_notification_event_borrow_clock_class_priority_map(
				notification);
		BT_ASSERT(cc_prio_map);
		event = bt_notification_event_borrow_event(notification);
		BT_ASSERT(event);

		if (bt_clock_class_priority_map_get_clock_class_count(cc_prio_map) > 0) {
			struct bt_clock_value *clock_value;
			struct bt_clock_class *clock_class =
				bt_clock_class_priority_map_borrow_highest_priority_clock_class(
					cc_prio_map);

			BT_ASSERT(clock_class);
			clock_value = bt_event_borrow_clock_value(event,
				clock_class);
			BT_ASSERT(clock_value);
			ret = bt_clock_value_get_value_ns_from_epoch(
					clock_value, &test_event.ts_ns);
			BT_ASSERT(ret == 0);
		} else {
			test_event.ts_ns = -1;
		}

		break;
	}
	case BT_NOTIFICATION_TYPE_INACTIVITY:
	{
		struct bt_clock_class_priority_map *cc_prio_map;

		test_event.type = TEST_EV_TYPE_NOTIF_INACTIVITY;
		cc_prio_map = bt_notification_inactivity_borrow_clock_class_priority_map(
			notification);
		BT_ASSERT(cc_prio_map);

		if (bt_clock_class_priority_map_get_clock_class_count(cc_prio_map) > 0) {
			struct bt_clock_value *clock_value;
			struct bt_clock_class *clock_class =
				bt_clock_class_priority_map_borrow_highest_priority_clock_class(
					cc_prio_map);

			BT_ASSERT(clock_class);
			clock_value =
				bt_notification_inactivity_borrow_clock_value(
					notification, clock_class);
			BT_ASSERT(clock_value);
			ret = bt_clock_value_get_value_ns_from_epoch(
					clock_value, &test_event.ts_ns);
			BT_ASSERT(ret == 0);
		} else {
			test_event.ts_ns = -1;
		}

		break;
	}
	case BT_NOTIFICATION_TYPE_PACKET_BEGIN:
		test_event.type = TEST_EV_TYPE_NOTIF_PACKET_BEGIN;
		break;
	case BT_NOTIFICATION_TYPE_PACKET_END:
		test_event.type = TEST_EV_TYPE_NOTIF_PACKET_END;
		break;
	case BT_NOTIFICATION_TYPE_STREAM_BEGIN:
		test_event.type = TEST_EV_TYPE_NOTIF_STREAM_BEGIN;
		break;
	case BT_NOTIFICATION_TYPE_STREAM_END:
		test_event.type = TEST_EV_TYPE_NOTIF_STREAM_END;
		break;
	default:
		test_event.type = TEST_EV_TYPE_NOTIF_UNEXPECTED;
		break;
	}

	append_test_event(&test_event);
}

static
enum bt_component_status sink_consume(
		struct bt_private_component *priv_component)
{
	enum bt_component_status ret = BT_COMPONENT_STATUS_OK;
	bt_notification_array notifications = NULL;
	uint64_t count;
	struct sink_user_data *user_data =
		bt_private_component_get_user_data(priv_component);
	enum bt_notification_iterator_status it_ret;
	struct test_event test_event;
	bool do_append_test_event = true;
	uint64_t i;

	BT_ASSERT(user_data && user_data->notif_iter);
	it_ret = bt_private_connection_notification_iterator_next(
		user_data->notif_iter, &notifications, &count);
	if (it_ret < 0) {
		ret = BT_COMPONENT_STATUS_ERROR;
		do_append_test_event = false;
		goto end;
	}

	switch (it_ret) {
	case BT_NOTIFICATION_ITERATOR_STATUS_END:
		test_event.type = TEST_EV_TYPE_END;
		ret = BT_COMPONENT_STATUS_END;
		BT_PUT(user_data->notif_iter);
		goto end;
	case BT_NOTIFICATION_ITERATOR_STATUS_AGAIN:
		test_event.type = TEST_EV_TYPE_AGAIN;
		ret = BT_COMPONENT_STATUS_AGAIN;
		goto end;
	default:
		break;
	}

	BT_ASSERT(notifications);

	for (i = 0; i < count; i++) {
		append_test_event_from_notification(notifications[i]);
		bt_put(notifications[i]);
	}

	do_append_test_event = false;

end:
	if (do_append_test_event) {
		append_test_event(&test_event);
	}

	return ret;
}

static
enum bt_component_status sink_port_connected(
		struct bt_private_component *private_component,
		struct bt_private_port *self_private_port,
		struct bt_port *other_port)
{
	struct bt_private_connection *priv_conn =
		bt_private_port_get_private_connection(self_private_port);
	struct sink_user_data *user_data = bt_private_component_get_user_data(
		private_component);
	enum bt_connection_status conn_status;

	BT_ASSERT(user_data);
	BT_ASSERT(priv_conn);
	conn_status = bt_private_connection_create_notification_iterator(
		priv_conn, &user_data->notif_iter);
	BT_ASSERT(conn_status == 0);
	bt_put(priv_conn);
	return BT_COMPONENT_STATUS_OK;
}

static
enum bt_component_status sink_init(
		struct bt_private_component *private_component,
		struct bt_value *params, void *init_method_data)
{
	struct sink_user_data *user_data = g_new0(struct sink_user_data, 1);
	int ret;

	BT_ASSERT(user_data);
	ret = bt_private_component_set_user_data(private_component,
		user_data);
	BT_ASSERT(ret == 0);
	ret = bt_private_component_sink_add_input_private_port(
		private_component, "in", NULL, NULL);
	BT_ASSERT(ret == 0);
	return BT_COMPONENT_STATUS_OK;
}

static
void sink_finalize(struct bt_private_component *private_component)
{
	struct sink_user_data *user_data = bt_private_component_get_user_data(
		private_component);

	if (user_data) {
		bt_put(user_data->notif_iter);
		g_free(user_data);
	}
}

static
void create_source_muxer_sink(struct bt_graph *graph,
		struct bt_component **source,
		struct bt_component **muxer,
		struct bt_component **sink)
{
	struct bt_component_class *src_comp_class;
	struct bt_component_class *muxer_comp_class;
	struct bt_component_class *sink_comp_class;
	int ret;

	/* Create source component */
	src_comp_class = bt_component_class_source_create("src", src_iter_next);
	BT_ASSERT(src_comp_class);
	ret = bt_component_class_set_init_method(src_comp_class, src_init);
	BT_ASSERT(ret == 0);
	ret = bt_component_class_set_finalize_method(src_comp_class,
		src_finalize);
	BT_ASSERT(ret == 0);
	ret = bt_component_class_source_set_notification_iterator_init_method(
		src_comp_class, src_iter_init);
	BT_ASSERT(ret == 0);
	ret = bt_component_class_source_set_notification_iterator_finalize_method(
		src_comp_class, src_iter_finalize);
	BT_ASSERT(ret == 0);
	ret = bt_graph_add_component(graph, src_comp_class, "source", NULL, source);
	BT_ASSERT(ret == 0);

	/* Create muxer component */
	muxer_comp_class = bt_plugin_find_component_class("utils", "muxer",
		BT_COMPONENT_CLASS_TYPE_FILTER);
	BT_ASSERT(muxer_comp_class);
	ret = bt_graph_add_component(graph, muxer_comp_class, "muxer", NULL, muxer);
	BT_ASSERT(ret == 0);

	/* Create sink component */
	sink_comp_class = bt_component_class_sink_create("sink", sink_consume);
	BT_ASSERT(sink_comp_class);
	ret = bt_component_class_set_init_method(sink_comp_class, sink_init);
	BT_ASSERT(ret == 0);
	ret = bt_component_class_set_finalize_method(sink_comp_class,
		sink_finalize);
	ret = bt_component_class_set_port_connected_method(sink_comp_class,
		sink_port_connected);
	BT_ASSERT(ret == 0);
	ret = bt_graph_add_component(graph, sink_comp_class, "sink", NULL, sink);
	BT_ASSERT(ret == 0);

	bt_put(src_comp_class);
	bt_put(muxer_comp_class);
	bt_put(sink_comp_class);
}

static
void do_std_test(enum test test, const char *name,
		const struct test_event *expected_test_events,
		bool with_upstream)
{
	struct bt_component *src_comp;
	struct bt_component *muxer_comp;
	struct bt_component *sink_comp;
	struct bt_port *upstream_port;
	struct bt_port *downstream_port;
	int64_t i;
	int64_t count;
	enum bt_graph_status graph_status = BT_GRAPH_STATUS_OK;

	clear_test_events();
	current_test = test;
	diag("test: %s", name);
	BT_ASSERT(!graph);
	graph = bt_graph_create();
	BT_ASSERT(graph);
	create_source_muxer_sink(graph, &src_comp, &muxer_comp, &sink_comp);

	/* Connect source output ports to muxer input ports */
	if (with_upstream) {
		count = bt_component_source_get_output_port_count(src_comp);
		BT_ASSERT(count >= 0);

		for (i = 0; i < count; i++) {
			upstream_port = bt_component_source_get_output_port_by_index(
				src_comp, i);
			BT_ASSERT(upstream_port);
			downstream_port = bt_component_filter_get_input_port_by_index(
				muxer_comp, i);
			BT_ASSERT(downstream_port);
			graph_status = bt_graph_connect_ports(graph,
				upstream_port, downstream_port, NULL);
			BT_ASSERT(graph_status == 0);
			bt_put(upstream_port);
			bt_put(downstream_port);
		}
	}

	/* Connect muxer output port to sink input port */
	upstream_port = bt_component_filter_get_output_port_by_name(muxer_comp,
		"out");
	BT_ASSERT(upstream_port);
	downstream_port = bt_component_sink_get_input_port_by_name(sink_comp, "in");
	BT_ASSERT(downstream_port);
	graph_status = bt_graph_connect_ports(graph, upstream_port,
		downstream_port, NULL);
	BT_ASSERT(graph_status == 0);
	bt_put(upstream_port);
	bt_put(downstream_port);

	while (graph_status == BT_GRAPH_STATUS_OK ||
			graph_status == BT_GRAPH_STATUS_AGAIN) {
		graph_status = bt_graph_run(graph);
	}

	ok(graph_status == BT_GRAPH_STATUS_END, "graph finishes without any error");
	ok(compare_test_events(expected_test_events),
		"the produced sequence of test events is the expected one");

	bt_put(src_comp);
	bt_put(muxer_comp);
	bt_put(sink_comp);
	BT_PUT(graph);
}

static
void test_no_ts(void)
{
	const struct test_event expected_test_events[] = {
		{ .type = TEST_EV_TYPE_NOTIF_STREAM_BEGIN, },
		{ .type = TEST_EV_TYPE_NOTIF_PACKET_BEGIN, },
		{ .type = TEST_EV_TYPE_NOTIF_STREAM_BEGIN, },
		{ .type = TEST_EV_TYPE_NOTIF_PACKET_BEGIN, },
		{ .type = TEST_EV_TYPE_NOTIF_EVENT, .ts_ns = -1, },
		{ .type = TEST_EV_TYPE_NOTIF_EVENT, .ts_ns = -1, },
		{ .type = TEST_EV_TYPE_NOTIF_EVENT, .ts_ns = -1, },
		{ .type = TEST_EV_TYPE_NOTIF_EVENT, .ts_ns = -1, },
		{ .type = TEST_EV_TYPE_NOTIF_EVENT, .ts_ns = -1, },
		{ .type = TEST_EV_TYPE_NOTIF_PACKET_END, },
		{ .type = TEST_EV_TYPE_NOTIF_STREAM_END, },
		{ .type = TEST_EV_TYPE_NOTIF_EVENT, .ts_ns = 1, },
		{ .type = TEST_EV_TYPE_NOTIF_EVENT, .ts_ns = 4, },
		{ .type = TEST_EV_TYPE_NOTIF_EVENT, .ts_ns = 189, },
		{ .type = TEST_EV_TYPE_NOTIF_EVENT, .ts_ns = 1001, },
		{ .type = TEST_EV_TYPE_NOTIF_PACKET_END, },
		{ .type = TEST_EV_TYPE_NOTIF_STREAM_END, },
		{ .type = TEST_EV_TYPE_END, },
		{ .type = TEST_EV_TYPE_SENTINEL, },
	};

	do_std_test(TEST_NO_TS, "event notifications with no time",
		expected_test_events, true);
}

static
void test_no_upstream_connection(void)
{
	const struct test_event expected_test_events[] = {
		{ .type = TEST_EV_TYPE_END, },
		{ .type = TEST_EV_TYPE_SENTINEL, },
	};

	do_std_test(TEST_NO_UPSTREAM_CONNECTION, "no upstream connection",
		expected_test_events, false);
}

static
void test_simple_4_ports(void)
{
	const struct test_event expected_test_events[] = {
		{ .type = TEST_EV_TYPE_NOTIF_STREAM_BEGIN, },
		{ .type = TEST_EV_TYPE_NOTIF_PACKET_BEGIN, },
		{ .type = TEST_EV_TYPE_NOTIF_STREAM_BEGIN, },
		{ .type = TEST_EV_TYPE_NOTIF_PACKET_BEGIN, },
		{ .type = TEST_EV_TYPE_NOTIF_STREAM_BEGIN, },
		{ .type = TEST_EV_TYPE_NOTIF_PACKET_BEGIN, },
		{ .type = TEST_EV_TYPE_NOTIF_STREAM_BEGIN, },
		{ .type = TEST_EV_TYPE_NOTIF_PACKET_BEGIN, },
		{ .type = TEST_EV_TYPE_NOTIF_EVENT, .ts_ns = 8 },
		{ .type = TEST_EV_TYPE_NOTIF_EVENT, .ts_ns = 24 },
		{ .type = TEST_EV_TYPE_NOTIF_EVENT, .ts_ns = 41 },
		{ .type = TEST_EV_TYPE_NOTIF_EVENT, .ts_ns = 51 },
		{ .type = TEST_EV_TYPE_NOTIF_EVENT, .ts_ns = 53 },
		{ .type = TEST_EV_TYPE_NOTIF_EVENT, .ts_ns = 56 },
		{ .type = TEST_EV_TYPE_NOTIF_EVENT, .ts_ns = 59 },
		{ .type = TEST_EV_TYPE_NOTIF_EVENT, .ts_ns = 68 },
		{ .type = TEST_EV_TYPE_NOTIF_EVENT, .ts_ns = 71 },
		{ .type = TEST_EV_TYPE_NOTIF_EVENT, .ts_ns = 77 },
		{ .type = TEST_EV_TYPE_NOTIF_EVENT, .ts_ns = 91 },
		{ .type = TEST_EV_TYPE_NOTIF_EVENT, .ts_ns = 97 },
		{ .type = TEST_EV_TYPE_NOTIF_EVENT, .ts_ns = 105 },
		{ .type = TEST_EV_TYPE_NOTIF_EVENT, .ts_ns = 119 },
		{ .type = TEST_EV_TYPE_NOTIF_EVENT, .ts_ns = 120 },
		{ .type = TEST_EV_TYPE_NOTIF_EVENT, .ts_ns = 121 },
		{ .type = TEST_EV_TYPE_NOTIF_EVENT, .ts_ns = 138 },
		{ .type = TEST_EV_TYPE_NOTIF_EVENT, .ts_ns = 139 },
		{ .type = TEST_EV_TYPE_NOTIF_EVENT, .ts_ns = 154 },
		{ .type = TEST_EV_TYPE_NOTIF_EVENT, .ts_ns = 170 },
		{ .type = TEST_EV_TYPE_NOTIF_EVENT, .ts_ns = 179 },
		{ .type = TEST_EV_TYPE_NOTIF_EVENT, .ts_ns = 209 },
		{ .type = TEST_EV_TYPE_NOTIF_EVENT, .ts_ns = 210 },
		{ .type = TEST_EV_TYPE_NOTIF_EVENT, .ts_ns = 222 },
		{ .type = TEST_EV_TYPE_NOTIF_EVENT, .ts_ns = 228 },
		{ .type = TEST_EV_TYPE_NOTIF_EVENT, .ts_ns = 240 },
		{ .type = TEST_EV_TYPE_NOTIF_EVENT, .ts_ns = 254 },
		{ .type = TEST_EV_TYPE_NOTIF_EVENT, .ts_ns = 266 },
		{ .type = TEST_EV_TYPE_NOTIF_EVENT, .ts_ns = 292 },
		{ .type = TEST_EV_TYPE_NOTIF_EVENT, .ts_ns = 298 },
		{ .type = TEST_EV_TYPE_NOTIF_EVENT, .ts_ns = 317 },
		{ .type = TEST_EV_TYPE_NOTIF_EVENT, .ts_ns = 320 },
		{ .type = TEST_EV_TYPE_NOTIF_EVENT, .ts_ns = 350 },
		{ .type = TEST_EV_TYPE_NOTIF_EVENT, .ts_ns = 352 },
		{ .type = TEST_EV_TYPE_NOTIF_EVENT, .ts_ns = 353 },
		{ .type = TEST_EV_TYPE_NOTIF_EVENT, .ts_ns = 393 },
		{ .type = TEST_EV_TYPE_NOTIF_EVENT, .ts_ns = 407 },
		{ .type = TEST_EV_TYPE_NOTIF_EVENT, .ts_ns = 419 },
		{ .type = TEST_EV_TYPE_NOTIF_EVENT, .ts_ns = 433 },
		{ .type = TEST_EV_TYPE_NOTIF_EVENT, .ts_ns = 454 },
		{ .type = TEST_EV_TYPE_NOTIF_EVENT, .ts_ns = 471 },
		{ .type = TEST_EV_TYPE_NOTIF_EVENT, .ts_ns = 473 },
		{ .type = TEST_EV_TYPE_NOTIF_EVENT, .ts_ns = 478 },
		{ .type = TEST_EV_TYPE_NOTIF_EVENT, .ts_ns = 479 },
		{ .type = TEST_EV_TYPE_NOTIF_EVENT, .ts_ns = 481 },
		{ .type = TEST_EV_TYPE_NOTIF_EVENT, .ts_ns = 487 },
		{ .type = TEST_EV_TYPE_NOTIF_EVENT, .ts_ns = 504 },
		{ .type = TEST_EV_TYPE_NOTIF_EVENT, .ts_ns = 525 },
		{ .type = TEST_EV_TYPE_NOTIF_EVENT, .ts_ns = 572 },
		{ .type = TEST_EV_TYPE_NOTIF_EVENT, .ts_ns = 591 },
		{ .type = TEST_EV_TYPE_NOTIF_EVENT, .ts_ns = 605 },
		{ .type = TEST_EV_TYPE_NOTIF_EVENT, .ts_ns = 612 },
		{ .type = TEST_EV_TYPE_NOTIF_EVENT, .ts_ns = 615 },
		{ .type = TEST_EV_TYPE_NOTIF_EVENT, .ts_ns = 618 },
		{ .type = TEST_EV_TYPE_NOTIF_EVENT, .ts_ns = 624 },
		{ .type = TEST_EV_TYPE_NOTIF_EVENT, .ts_ns = 631 },
		{ .type = TEST_EV_TYPE_NOTIF_EVENT, .ts_ns = 632 },
		{ .type = TEST_EV_TYPE_NOTIF_EVENT, .ts_ns = 644 },
		{ .type = TEST_EV_TYPE_NOTIF_EVENT, .ts_ns = 651 },
		{ .type = TEST_EV_TYPE_NOTIF_EVENT, .ts_ns = 668 },
		{ .type = TEST_EV_TYPE_NOTIF_EVENT, .ts_ns = 670 },
		{ .type = TEST_EV_TYPE_NOTIF_EVENT, .ts_ns = 678 },
		{ .type = TEST_EV_TYPE_NOTIF_EVENT, .ts_ns = 696 },
		{ .type = TEST_EV_TYPE_NOTIF_EVENT, .ts_ns = 708 },
		{ .type = TEST_EV_TYPE_NOTIF_EVENT, .ts_ns = 714 },
		{ .type = TEST_EV_TYPE_NOTIF_EVENT, .ts_ns = 717 },
		{ .type = TEST_EV_TYPE_NOTIF_EVENT, .ts_ns = 731 },
		{ .type = TEST_EV_TYPE_NOTIF_EVENT, .ts_ns = 733 },
		{ .type = TEST_EV_TYPE_NOTIF_EVENT, .ts_ns = 744 },
		{ .type = TEST_EV_TYPE_NOTIF_EVENT, .ts_ns = 750 },
		{ .type = TEST_EV_TYPE_NOTIF_EVENT, .ts_ns = 766 },
		{ .type = TEST_EV_TYPE_NOTIF_EVENT, .ts_ns = 778 },
		{ .type = TEST_EV_TYPE_NOTIF_EVENT, .ts_ns = 788 },
		{ .type = TEST_EV_TYPE_NOTIF_EVENT, .ts_ns = 790 },
		{ .type = TEST_EV_TYPE_NOTIF_EVENT, .ts_ns = 819 },
		{ .type = TEST_EV_TYPE_NOTIF_EVENT, .ts_ns = 820 },
		{ .type = TEST_EV_TYPE_NOTIF_EVENT, .ts_ns = 825 },
		{ .type = TEST_EV_TYPE_NOTIF_EVENT, .ts_ns = 836 },
		{ .type = TEST_EV_TYPE_NOTIF_PACKET_END, },
		{ .type = TEST_EV_TYPE_NOTIF_STREAM_END, },
		{ .type = TEST_EV_TYPE_NOTIF_EVENT, .ts_ns = 850 },
		{ .type = TEST_EV_TYPE_NOTIF_EVENT, .ts_ns = 852 },
		{ .type = TEST_EV_TYPE_NOTIF_EVENT, .ts_ns = 857 },
		{ .type = TEST_EV_TYPE_NOTIF_EVENT, .ts_ns = 863 },
		{ .type = TEST_EV_TYPE_NOTIF_EVENT, .ts_ns = 867 },
		{ .type = TEST_EV_TYPE_NOTIF_EVENT, .ts_ns = 871 },
		{ .type = TEST_EV_TYPE_NOTIF_EVENT, .ts_ns = 884 },
		{ .type = TEST_EV_TYPE_NOTIF_EVENT, .ts_ns = 892 },
		{ .type = TEST_EV_TYPE_NOTIF_EVENT, .ts_ns = 903 },
		{ .type = TEST_EV_TYPE_NOTIF_EVENT, .ts_ns = 931 },
		{ .type = TEST_EV_TYPE_NOTIF_EVENT, .ts_ns = 944 },
		{ .type = TEST_EV_TYPE_NOTIF_EVENT, .ts_ns = 951 },
		{ .type = TEST_EV_TYPE_NOTIF_EVENT, .ts_ns = 953 },
		{ .type = TEST_EV_TYPE_NOTIF_EVENT, .ts_ns = 956 },
		{ .type = TEST_EV_TYPE_NOTIF_EVENT, .ts_ns = 985 },
		{ .type = TEST_EV_TYPE_NOTIF_EVENT, .ts_ns = 996 },
		{ .type = TEST_EV_TYPE_NOTIF_PACKET_END, },
		{ .type = TEST_EV_TYPE_NOTIF_STREAM_END, },
		{ .type = TEST_EV_TYPE_NOTIF_EVENT, .ts_ns = 998 },
		{ .type = TEST_EV_TYPE_NOTIF_PACKET_END, },
		{ .type = TEST_EV_TYPE_NOTIF_STREAM_END, },
		{ .type = TEST_EV_TYPE_NOTIF_EVENT, .ts_ns = 999 },
		{ .type = TEST_EV_TYPE_NOTIF_PACKET_END, },
		{ .type = TEST_EV_TYPE_NOTIF_STREAM_END, },
		{ .type = TEST_EV_TYPE_END, },
		{ .type = TEST_EV_TYPE_SENTINEL, },
	};

	do_std_test(TEST_SIMPLE_4_PORTS, "simple: 4 ports without retries",
		expected_test_events, true);
}

static
void test_4_ports_with_retries(void)
{
	const struct test_event expected_test_events[] = {
		{ .type = TEST_EV_TYPE_AGAIN, },
		{ .type = TEST_EV_TYPE_NOTIF_STREAM_BEGIN, },
		{ .type = TEST_EV_TYPE_NOTIF_PACKET_BEGIN, },
		{ .type = TEST_EV_TYPE_NOTIF_STREAM_BEGIN, },
		{ .type = TEST_EV_TYPE_NOTIF_PACKET_BEGIN, },
		{ .type = TEST_EV_TYPE_NOTIF_STREAM_BEGIN, },
		{ .type = TEST_EV_TYPE_NOTIF_PACKET_BEGIN, },
		{ .type = TEST_EV_TYPE_NOTIF_STREAM_BEGIN, },
		{ .type = TEST_EV_TYPE_NOTIF_PACKET_BEGIN, },
		{ .type = TEST_EV_TYPE_NOTIF_EVENT, .ts_ns = 8 },
		{ .type = TEST_EV_TYPE_NOTIF_EVENT, .ts_ns = 24 },
		{ .type = TEST_EV_TYPE_NOTIF_EVENT, .ts_ns = 41 },
		{ .type = TEST_EV_TYPE_NOTIF_EVENT, .ts_ns = 51 },
		{ .type = TEST_EV_TYPE_NOTIF_EVENT, .ts_ns = 53 },
		{ .type = TEST_EV_TYPE_NOTIF_EVENT, .ts_ns = 56 },
		{ .type = TEST_EV_TYPE_NOTIF_EVENT, .ts_ns = 59 },
		{ .type = TEST_EV_TYPE_NOTIF_EVENT, .ts_ns = 68 },
		{ .type = TEST_EV_TYPE_NOTIF_EVENT, .ts_ns = 71 },
		{ .type = TEST_EV_TYPE_NOTIF_EVENT, .ts_ns = 77 },
		{ .type = TEST_EV_TYPE_NOTIF_EVENT, .ts_ns = 91 },
		{ .type = TEST_EV_TYPE_NOTIF_EVENT, .ts_ns = 97 },
		{ .type = TEST_EV_TYPE_NOTIF_EVENT, .ts_ns = 105 },
		{ .type = TEST_EV_TYPE_NOTIF_EVENT, .ts_ns = 119 },
		{ .type = TEST_EV_TYPE_NOTIF_EVENT, .ts_ns = 120 },
		{ .type = TEST_EV_TYPE_NOTIF_EVENT, .ts_ns = 121 },
		{ .type = TEST_EV_TYPE_NOTIF_EVENT, .ts_ns = 138 },
		{ .type = TEST_EV_TYPE_NOTIF_EVENT, .ts_ns = 139 },
		{ .type = TEST_EV_TYPE_NOTIF_EVENT, .ts_ns = 154 },
		{ .type = TEST_EV_TYPE_NOTIF_EVENT, .ts_ns = 170 },
		{ .type = TEST_EV_TYPE_NOTIF_EVENT, .ts_ns = 179 },
		{ .type = TEST_EV_TYPE_NOTIF_EVENT, .ts_ns = 209 },
		{ .type = TEST_EV_TYPE_NOTIF_EVENT, .ts_ns = 210 },
		{ .type = TEST_EV_TYPE_AGAIN, },
		{ .type = TEST_EV_TYPE_NOTIF_EVENT, .ts_ns = 222 },
		{ .type = TEST_EV_TYPE_NOTIF_EVENT, .ts_ns = 228 },
		{ .type = TEST_EV_TYPE_NOTIF_EVENT, .ts_ns = 240 },
		{ .type = TEST_EV_TYPE_NOTIF_EVENT, .ts_ns = 254 },
		{ .type = TEST_EV_TYPE_NOTIF_EVENT, .ts_ns = 266 },
		{ .type = TEST_EV_TYPE_NOTIF_EVENT, .ts_ns = 292 },
		{ .type = TEST_EV_TYPE_NOTIF_EVENT, .ts_ns = 298 },
		{ .type = TEST_EV_TYPE_NOTIF_EVENT, .ts_ns = 317 },
		{ .type = TEST_EV_TYPE_NOTIF_EVENT, .ts_ns = 320 },
		{ .type = TEST_EV_TYPE_NOTIF_EVENT, .ts_ns = 350 },
		{ .type = TEST_EV_TYPE_NOTIF_EVENT, .ts_ns = 352 },
		{ .type = TEST_EV_TYPE_NOTIF_EVENT, .ts_ns = 353 },
		{ .type = TEST_EV_TYPE_NOTIF_EVENT, .ts_ns = 393 },
		{ .type = TEST_EV_TYPE_NOTIF_EVENT, .ts_ns = 407 },
		{ .type = TEST_EV_TYPE_NOTIF_EVENT, .ts_ns = 419 },
		{ .type = TEST_EV_TYPE_NOTIF_EVENT, .ts_ns = 433 },
		{ .type = TEST_EV_TYPE_NOTIF_EVENT, .ts_ns = 454 },
		{ .type = TEST_EV_TYPE_NOTIF_EVENT, .ts_ns = 471 },
		{ .type = TEST_EV_TYPE_NOTIF_EVENT, .ts_ns = 473 },
		{ .type = TEST_EV_TYPE_NOTIF_EVENT, .ts_ns = 478 },
		{ .type = TEST_EV_TYPE_NOTIF_EVENT, .ts_ns = 479 },
		{ .type = TEST_EV_TYPE_NOTIF_EVENT, .ts_ns = 481 },
		{ .type = TEST_EV_TYPE_NOTIF_EVENT, .ts_ns = 487 },
		{ .type = TEST_EV_TYPE_NOTIF_EVENT, .ts_ns = 504 },
		{ .type = TEST_EV_TYPE_NOTIF_EVENT, .ts_ns = 525 },
		{ .type = TEST_EV_TYPE_NOTIF_EVENT, .ts_ns = 572 },
		{ .type = TEST_EV_TYPE_NOTIF_EVENT, .ts_ns = 591 },
		{ .type = TEST_EV_TYPE_NOTIF_EVENT, .ts_ns = 605 },
		{ .type = TEST_EV_TYPE_NOTIF_EVENT, .ts_ns = 612 },
		{ .type = TEST_EV_TYPE_NOTIF_EVENT, .ts_ns = 615 },
		{ .type = TEST_EV_TYPE_NOTIF_EVENT, .ts_ns = 618 },
		{ .type = TEST_EV_TYPE_NOTIF_EVENT, .ts_ns = 624 },
		{ .type = TEST_EV_TYPE_NOTIF_EVENT, .ts_ns = 631 },
		{ .type = TEST_EV_TYPE_NOTIF_EVENT, .ts_ns = 632 },
		{ .type = TEST_EV_TYPE_NOTIF_EVENT, .ts_ns = 644 },
		{ .type = TEST_EV_TYPE_NOTIF_EVENT, .ts_ns = 651 },
		{ .type = TEST_EV_TYPE_AGAIN, },
		{ .type = TEST_EV_TYPE_NOTIF_EVENT, .ts_ns = 668 },
		{ .type = TEST_EV_TYPE_NOTIF_EVENT, .ts_ns = 670 },
		{ .type = TEST_EV_TYPE_NOTIF_EVENT, .ts_ns = 678 },
		{ .type = TEST_EV_TYPE_NOTIF_EVENT, .ts_ns = 696 },
		{ .type = TEST_EV_TYPE_NOTIF_EVENT, .ts_ns = 708 },
		{ .type = TEST_EV_TYPE_NOTIF_EVENT, .ts_ns = 714 },
		{ .type = TEST_EV_TYPE_NOTIF_EVENT, .ts_ns = 717 },
		{ .type = TEST_EV_TYPE_NOTIF_EVENT, .ts_ns = 731 },
		{ .type = TEST_EV_TYPE_NOTIF_EVENT, .ts_ns = 733 },
		{ .type = TEST_EV_TYPE_NOTIF_EVENT, .ts_ns = 744 },
		{ .type = TEST_EV_TYPE_NOTIF_EVENT, .ts_ns = 750 },
		{ .type = TEST_EV_TYPE_NOTIF_EVENT, .ts_ns = 766 },
		{ .type = TEST_EV_TYPE_NOTIF_EVENT, .ts_ns = 778 },
		{ .type = TEST_EV_TYPE_NOTIF_EVENT, .ts_ns = 788 },
		{ .type = TEST_EV_TYPE_NOTIF_EVENT, .ts_ns = 790 },
		{ .type = TEST_EV_TYPE_NOTIF_EVENT, .ts_ns = 819 },
		{ .type = TEST_EV_TYPE_NOTIF_EVENT, .ts_ns = 820 },
		{ .type = TEST_EV_TYPE_NOTIF_EVENT, .ts_ns = 825 },
		{ .type = TEST_EV_TYPE_NOTIF_EVENT, .ts_ns = 836 },
		{ .type = TEST_EV_TYPE_AGAIN, },
		{ .type = TEST_EV_TYPE_NOTIF_PACKET_END, },
		{ .type = TEST_EV_TYPE_NOTIF_STREAM_END, },
		{ .type = TEST_EV_TYPE_NOTIF_EVENT, .ts_ns = 850 },
		{ .type = TEST_EV_TYPE_NOTIF_EVENT, .ts_ns = 852 },
		{ .type = TEST_EV_TYPE_NOTIF_EVENT, .ts_ns = 857 },
		{ .type = TEST_EV_TYPE_NOTIF_EVENT, .ts_ns = 863 },
		{ .type = TEST_EV_TYPE_NOTIF_EVENT, .ts_ns = 867 },
		{ .type = TEST_EV_TYPE_NOTIF_EVENT, .ts_ns = 871 },
		{ .type = TEST_EV_TYPE_NOTIF_EVENT, .ts_ns = 884 },
		{ .type = TEST_EV_TYPE_NOTIF_EVENT, .ts_ns = 892 },
		{ .type = TEST_EV_TYPE_NOTIF_EVENT, .ts_ns = 903 },
		{ .type = TEST_EV_TYPE_NOTIF_EVENT, .ts_ns = 931 },
		{ .type = TEST_EV_TYPE_NOTIF_EVENT, .ts_ns = 944 },
		{ .type = TEST_EV_TYPE_NOTIF_EVENT, .ts_ns = 951 },
		{ .type = TEST_EV_TYPE_NOTIF_EVENT, .ts_ns = 953 },
		{ .type = TEST_EV_TYPE_NOTIF_EVENT, .ts_ns = 956 },
		{ .type = TEST_EV_TYPE_NOTIF_EVENT, .ts_ns = 985 },
		{ .type = TEST_EV_TYPE_NOTIF_EVENT, .ts_ns = 996 },
		{ .type = TEST_EV_TYPE_NOTIF_PACKET_END, },
		{ .type = TEST_EV_TYPE_NOTIF_STREAM_END, },
		{ .type = TEST_EV_TYPE_NOTIF_EVENT, .ts_ns = 998 },
		{ .type = TEST_EV_TYPE_NOTIF_PACKET_END, },
		{ .type = TEST_EV_TYPE_NOTIF_STREAM_END, },
		{ .type = TEST_EV_TYPE_NOTIF_EVENT, .ts_ns = 999 },
		{ .type = TEST_EV_TYPE_NOTIF_PACKET_END, },
		{ .type = TEST_EV_TYPE_NOTIF_STREAM_END, },
		{ .type = TEST_EV_TYPE_END, },
		{ .type = TEST_EV_TYPE_SENTINEL, },
	};

	do_std_test(TEST_4_PORTS_WITH_RETRIES, "4 ports with retries",
		expected_test_events, true);
}

static
void connect_port_to_first_avail_muxer_port(struct bt_graph *graph,
		struct bt_port *source_port,
		struct bt_component *muxer_comp)
{
	struct bt_port *avail_muxer_port = NULL;
	int64_t i;
	int64_t count;
	enum bt_graph_status graph_status;

	count = bt_component_filter_get_input_port_count(muxer_comp);
	BT_ASSERT(count >= 0);

	for (i = 0; i < count; i++) {
		struct bt_port *muxer_port =
			bt_component_filter_get_input_port_by_index(
				muxer_comp, i);

		BT_ASSERT(muxer_port);

		if (!bt_port_is_connected(muxer_port)) {
			BT_MOVE(avail_muxer_port, muxer_port);
			break;
		} else {
			bt_put(muxer_port);
		}
	}

	graph_status = bt_graph_connect_ports(graph, source_port,
		avail_muxer_port, NULL);
	BT_ASSERT(graph_status == 0);
	bt_put(avail_muxer_port);
}

static
void graph_port_added_listener_connect_to_avail_muxer_port(struct bt_port *port,
		void *data)
{
	struct graph_listener_data *graph_listener_data = data;
	struct bt_component *comp;

	comp = bt_port_get_component(port);
	BT_ASSERT(comp);

	if (comp != graph_listener_data->source) {
		goto end;
	}

	connect_port_to_first_avail_muxer_port(graph_listener_data->graph,
		port, graph_listener_data->muxer);

end:
	bt_put(comp);
}

static
void test_single_end_then_multiple_full(void)
{
	struct bt_component *src_comp;
	struct bt_component *muxer_comp;
	struct bt_component *sink_comp;
	struct bt_port *upstream_port;
	struct bt_port *downstream_port;
	int64_t i;
	int64_t count;
	int ret;
	enum bt_graph_status graph_status = BT_GRAPH_STATUS_OK;
	struct graph_listener_data graph_listener_data;
	const struct test_event expected_test_events[] = {
		{ .type = TEST_EV_TYPE_NOTIF_STREAM_BEGIN, },
		{ .type = TEST_EV_TYPE_NOTIF_PACKET_BEGIN, },
		{ .type = TEST_EV_TYPE_NOTIF_STREAM_BEGIN, },
		{ .type = TEST_EV_TYPE_NOTIF_PACKET_BEGIN, },
		{ .type = TEST_EV_TYPE_NOTIF_EVENT, .ts_ns = 8 },
		{ .type = TEST_EV_TYPE_NOTIF_EVENT, .ts_ns = 51 },
		{ .type = TEST_EV_TYPE_NOTIF_EVENT, .ts_ns = 59 },
		{ .type = TEST_EV_TYPE_NOTIF_EVENT, .ts_ns = 68 },
		{ .type = TEST_EV_TYPE_NOTIF_EVENT, .ts_ns = 71 },
		{ .type = TEST_EV_TYPE_NOTIF_EVENT, .ts_ns = 77 },
		{ .type = TEST_EV_TYPE_NOTIF_EVENT, .ts_ns = 91 },
		{ .type = TEST_EV_TYPE_NOTIF_EVENT, .ts_ns = 121 },
		{ .type = TEST_EV_TYPE_NOTIF_EVENT, .ts_ns = 139 },
		{ .type = TEST_EV_TYPE_NOTIF_EVENT, .ts_ns = 170 },
		{ .type = TEST_EV_TYPE_NOTIF_EVENT, .ts_ns = 179 },
		{ .type = TEST_EV_TYPE_NOTIF_EVENT, .ts_ns = 209 },
		{ .type = TEST_EV_TYPE_NOTIF_EVENT, .ts_ns = 254 },
		{ .type = TEST_EV_TYPE_NOTIF_EVENT, .ts_ns = 266 },
		{ .type = TEST_EV_TYPE_NOTIF_EVENT, .ts_ns = 298 },
		{ .type = TEST_EV_TYPE_NOTIF_EVENT, .ts_ns = 320 },
		{ .type = TEST_EV_TYPE_NOTIF_EVENT, .ts_ns = 350 },
		{ .type = TEST_EV_TYPE_NOTIF_EVENT, .ts_ns = 352 },
		{ .type = TEST_EV_TYPE_NOTIF_EVENT, .ts_ns = 393 },
		{ .type = TEST_EV_TYPE_NOTIF_EVENT, .ts_ns = 419 },
		{ .type = TEST_EV_TYPE_NOTIF_EVENT, .ts_ns = 454 },
		{ .type = TEST_EV_TYPE_NOTIF_EVENT, .ts_ns = 478 },
		{ .type = TEST_EV_TYPE_NOTIF_EVENT, .ts_ns = 624 },
		{ .type = TEST_EV_TYPE_NOTIF_EVENT, .ts_ns = 631 },
		{ .type = TEST_EV_TYPE_NOTIF_EVENT, .ts_ns = 644 },
		{ .type = TEST_EV_TYPE_NOTIF_EVENT, .ts_ns = 651 },
		{ .type = TEST_EV_TYPE_NOTIF_EVENT, .ts_ns = 668 },
		{ .type = TEST_EV_TYPE_NOTIF_EVENT, .ts_ns = 678 },
		{ .type = TEST_EV_TYPE_NOTIF_EVENT, .ts_ns = 714 },
		{ .type = TEST_EV_TYPE_NOTIF_EVENT, .ts_ns = 717 },
		{ .type = TEST_EV_TYPE_NOTIF_EVENT, .ts_ns = 731 },
		{ .type = TEST_EV_TYPE_NOTIF_EVENT, .ts_ns = 733 },
		{ .type = TEST_EV_TYPE_NOTIF_EVENT, .ts_ns = 744 },
		{ .type = TEST_EV_TYPE_NOTIF_EVENT, .ts_ns = 750 },
		{ .type = TEST_EV_TYPE_NOTIF_EVENT, .ts_ns = 778 },
		{ .type = TEST_EV_TYPE_NOTIF_EVENT, .ts_ns = 788 },
		{ .type = TEST_EV_TYPE_NOTIF_EVENT, .ts_ns = 790 },
		{ .type = TEST_EV_TYPE_NOTIF_EVENT, .ts_ns = 819 },
		{ .type = TEST_EV_TYPE_NOTIF_EVENT, .ts_ns = 820 },
		{ .type = TEST_EV_TYPE_NOTIF_EVENT, .ts_ns = 836 },
		{ .type = TEST_EV_TYPE_NOTIF_PACKET_END, },
		{ .type = TEST_EV_TYPE_NOTIF_STREAM_END, },
		{ .type = TEST_EV_TYPE_NOTIF_EVENT, .ts_ns = 857 },
		{ .type = TEST_EV_TYPE_NOTIF_EVENT, .ts_ns = 892 },
		{ .type = TEST_EV_TYPE_NOTIF_EVENT, .ts_ns = 903 },
		{ .type = TEST_EV_TYPE_NOTIF_EVENT, .ts_ns = 944 },
		{ .type = TEST_EV_TYPE_NOTIF_EVENT, .ts_ns = 998 },
		{ .type = TEST_EV_TYPE_NOTIF_PACKET_END, },
		{ .type = TEST_EV_TYPE_NOTIF_STREAM_END, },
		{ .type = TEST_EV_TYPE_END, },
		{ .type = TEST_EV_TYPE_SENTINEL, },
	};

	clear_test_events();
	current_test = TEST_SINGLE_END_THEN_MULTIPLE_FULL;
	diag("test: single end then multiple full");
	BT_ASSERT(!graph);
	graph = bt_graph_create();
	BT_ASSERT(graph);
	create_source_muxer_sink(graph, &src_comp, &muxer_comp, &sink_comp);
	graph_listener_data.graph = graph;
	graph_listener_data.source = src_comp;
	graph_listener_data.muxer = muxer_comp;
	graph_listener_data.sink = sink_comp;
	ret = bt_graph_add_port_added_listener(graph,
		graph_port_added_listener_connect_to_avail_muxer_port, NULL,
		&graph_listener_data);
	BT_ASSERT(ret >= 0);

	/* Connect source output ports to muxer input ports */
	count = bt_component_source_get_output_port_count(src_comp);
	BT_ASSERT(ret == 0);

	for (i = 0; i < count; i++) {
		upstream_port = bt_component_source_get_output_port_by_index(
			src_comp, i);
		BT_ASSERT(upstream_port);
		connect_port_to_first_avail_muxer_port(graph,
			upstream_port, muxer_comp);
		bt_put(upstream_port);
	}

	/* Connect muxer output port to sink input port */
	upstream_port = bt_component_filter_get_output_port_by_name(muxer_comp,
		"out");
	BT_ASSERT(upstream_port);
	downstream_port = bt_component_sink_get_input_port_by_name(sink_comp, "in");
	BT_ASSERT(downstream_port);
	graph_status = bt_graph_connect_ports(graph, upstream_port,
		downstream_port, NULL);
	BT_ASSERT(graph_status == 0);
	bt_put(upstream_port);
	bt_put(downstream_port);

	while (graph_status == BT_GRAPH_STATUS_OK ||
			graph_status == BT_GRAPH_STATUS_AGAIN) {
		graph_status = bt_graph_run(graph);
	}

	ok(graph_status == BT_GRAPH_STATUS_END, "graph finishes without any error");
	ok(compare_test_events(expected_test_events),
		"the produced sequence of test events is the expected one");

	bt_put(src_comp);
	bt_put(muxer_comp);
	bt_put(sink_comp);
	BT_PUT(graph);
}

static
void test_single_again_end_then_multiple_full(void)
{
	struct bt_component *src_comp;
	struct bt_component *muxer_comp;
	struct bt_component *sink_comp;
	struct bt_port *upstream_port;
	struct bt_port *downstream_port;
	int64_t i;
	int64_t count;
	int ret;
	enum bt_graph_status graph_status = BT_GRAPH_STATUS_OK;
	struct graph_listener_data graph_listener_data;
	const struct test_event expected_test_events[] = {
		{ .type = TEST_EV_TYPE_AGAIN, },
		{ .type = TEST_EV_TYPE_NOTIF_STREAM_BEGIN, },
		{ .type = TEST_EV_TYPE_NOTIF_PACKET_BEGIN, },
		{ .type = TEST_EV_TYPE_NOTIF_STREAM_BEGIN, },
		{ .type = TEST_EV_TYPE_NOTIF_PACKET_BEGIN, },
		{ .type = TEST_EV_TYPE_NOTIF_EVENT, .ts_ns = 8 },
		{ .type = TEST_EV_TYPE_NOTIF_EVENT, .ts_ns = 51 },
		{ .type = TEST_EV_TYPE_NOTIF_EVENT, .ts_ns = 59 },
		{ .type = TEST_EV_TYPE_NOTIF_EVENT, .ts_ns = 68 },
		{ .type = TEST_EV_TYPE_NOTIF_EVENT, .ts_ns = 71 },
		{ .type = TEST_EV_TYPE_NOTIF_EVENT, .ts_ns = 77 },
		{ .type = TEST_EV_TYPE_NOTIF_EVENT, .ts_ns = 91 },
		{ .type = TEST_EV_TYPE_NOTIF_EVENT, .ts_ns = 121 },
		{ .type = TEST_EV_TYPE_NOTIF_EVENT, .ts_ns = 139 },
		{ .type = TEST_EV_TYPE_NOTIF_EVENT, .ts_ns = 170 },
		{ .type = TEST_EV_TYPE_NOTIF_EVENT, .ts_ns = 179 },
		{ .type = TEST_EV_TYPE_NOTIF_EVENT, .ts_ns = 209 },
		{ .type = TEST_EV_TYPE_NOTIF_EVENT, .ts_ns = 254 },
		{ .type = TEST_EV_TYPE_NOTIF_EVENT, .ts_ns = 266 },
		{ .type = TEST_EV_TYPE_NOTIF_EVENT, .ts_ns = 298 },
		{ .type = TEST_EV_TYPE_NOTIF_EVENT, .ts_ns = 320 },
		{ .type = TEST_EV_TYPE_NOTIF_EVENT, .ts_ns = 350 },
		{ .type = TEST_EV_TYPE_NOTIF_EVENT, .ts_ns = 352 },
		{ .type = TEST_EV_TYPE_NOTIF_EVENT, .ts_ns = 393 },
		{ .type = TEST_EV_TYPE_NOTIF_EVENT, .ts_ns = 419 },
		{ .type = TEST_EV_TYPE_NOTIF_EVENT, .ts_ns = 454 },
		{ .type = TEST_EV_TYPE_NOTIF_EVENT, .ts_ns = 478 },
		{ .type = TEST_EV_TYPE_NOTIF_EVENT, .ts_ns = 624 },
		{ .type = TEST_EV_TYPE_NOTIF_EVENT, .ts_ns = 631 },
		{ .type = TEST_EV_TYPE_NOTIF_EVENT, .ts_ns = 644 },
		{ .type = TEST_EV_TYPE_NOTIF_EVENT, .ts_ns = 651 },
		{ .type = TEST_EV_TYPE_NOTIF_EVENT, .ts_ns = 668 },
		{ .type = TEST_EV_TYPE_NOTIF_EVENT, .ts_ns = 678 },
		{ .type = TEST_EV_TYPE_NOTIF_EVENT, .ts_ns = 714 },
		{ .type = TEST_EV_TYPE_NOTIF_EVENT, .ts_ns = 717 },
		{ .type = TEST_EV_TYPE_NOTIF_EVENT, .ts_ns = 731 },
		{ .type = TEST_EV_TYPE_NOTIF_EVENT, .ts_ns = 733 },
		{ .type = TEST_EV_TYPE_NOTIF_EVENT, .ts_ns = 744 },
		{ .type = TEST_EV_TYPE_NOTIF_EVENT, .ts_ns = 750 },
		{ .type = TEST_EV_TYPE_NOTIF_EVENT, .ts_ns = 778 },
		{ .type = TEST_EV_TYPE_NOTIF_EVENT, .ts_ns = 788 },
		{ .type = TEST_EV_TYPE_NOTIF_EVENT, .ts_ns = 790 },
		{ .type = TEST_EV_TYPE_NOTIF_EVENT, .ts_ns = 819 },
		{ .type = TEST_EV_TYPE_NOTIF_EVENT, .ts_ns = 820 },
		{ .type = TEST_EV_TYPE_NOTIF_EVENT, .ts_ns = 836 },
		{ .type = TEST_EV_TYPE_NOTIF_PACKET_END, },
		{ .type = TEST_EV_TYPE_NOTIF_STREAM_END, },
		{ .type = TEST_EV_TYPE_NOTIF_EVENT, .ts_ns = 857 },
		{ .type = TEST_EV_TYPE_NOTIF_EVENT, .ts_ns = 892 },
		{ .type = TEST_EV_TYPE_NOTIF_EVENT, .ts_ns = 903 },
		{ .type = TEST_EV_TYPE_NOTIF_EVENT, .ts_ns = 944 },
		{ .type = TEST_EV_TYPE_NOTIF_EVENT, .ts_ns = 998 },
		{ .type = TEST_EV_TYPE_NOTIF_PACKET_END, },
		{ .type = TEST_EV_TYPE_NOTIF_STREAM_END, },
		{ .type = TEST_EV_TYPE_END, },
		{ .type = TEST_EV_TYPE_SENTINEL, },
	};

	clear_test_events();
	current_test = TEST_SINGLE_AGAIN_END_THEN_MULTIPLE_FULL;
	diag("test: single again then end then multiple full");
	BT_ASSERT(!graph);
	graph = bt_graph_create();
	BT_ASSERT(graph);
	create_source_muxer_sink(graph, &src_comp, &muxer_comp, &sink_comp);
	graph_listener_data.graph = graph;
	graph_listener_data.source = src_comp;
	graph_listener_data.muxer = muxer_comp;
	graph_listener_data.sink = sink_comp;
	ret = bt_graph_add_port_added_listener(graph,
		graph_port_added_listener_connect_to_avail_muxer_port, NULL,
		&graph_listener_data);
	BT_ASSERT(ret >= 0);

	/* Connect source output ports to muxer input ports */
	count = bt_component_source_get_output_port_count(src_comp);
	BT_ASSERT(ret == 0);

	for (i = 0; i < count; i++) {
		upstream_port = bt_component_source_get_output_port_by_index(
			src_comp, i);
		BT_ASSERT(upstream_port);
		connect_port_to_first_avail_muxer_port(graph,
			upstream_port, muxer_comp);
		bt_put(upstream_port);
	}

	/* Connect muxer output port to sink input port */
	upstream_port = bt_component_filter_get_output_port_by_name(muxer_comp,
		"out");
	BT_ASSERT(upstream_port);
	downstream_port = bt_component_sink_get_input_port_by_name(sink_comp, "in");
	BT_ASSERT(downstream_port);
	graph_status = bt_graph_connect_ports(graph, upstream_port,
		downstream_port, NULL);
	BT_ASSERT(graph_status == 0);
	bt_put(upstream_port);
	bt_put(downstream_port);

	while (graph_status == BT_GRAPH_STATUS_OK ||
			graph_status == BT_GRAPH_STATUS_AGAIN) {
		graph_status = bt_graph_run(graph);
	}

	ok(graph_status == BT_GRAPH_STATUS_END, "graph finishes without any error");
	ok(compare_test_events(expected_test_events),
		"the produced sequence of test events is the expected one");

	bt_put(src_comp);
	bt_put(muxer_comp);
	bt_put(sink_comp);
	BT_PUT(graph);
}

#define DEBUG_ENV_VAR	"TEST_UTILS_MUXER_DEBUG"

int main(int argc, char **argv)
{
	if (getenv(DEBUG_ENV_VAR) && strcmp(getenv(DEBUG_ENV_VAR), "1") == 0) {
		debug = true;
	}

	plan_tests(NR_TESTS);
	init_static_data();
	test_no_ts();
	test_no_upstream_connection();
	test_simple_4_ports();
	test_4_ports_with_retries();
	test_single_end_then_multiple_full();
	test_single_again_end_then_multiple_full();
	fini_static_data();
	return exit_status();
}
