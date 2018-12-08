/*
 * iterator.c
 *
 * Babeltrace Trace Trimmer Iterator
 *
 * Copyright 2016 Jérémie Galarneau <jeremie.galarneau@efficios.com>
 *
 * Author: Jérémie Galarneau <jeremie.galarneau@efficios.com>
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

#define BT_LOG_TAG "PLUGIN-UTILS-TRIMMER-FLT-ITER"
#include "logging.h"

#include <babeltrace/compat/time-internal.h>
#include <babeltrace/compat/utc-internal.h>
#include <babeltrace/babeltrace.h>
#include <babeltrace/assert-internal.h>
#include <plugins-common.h>

#include "trimmer.h"
#include "iterator.h"
#include "copy.h"

static
gboolean close_packets(gpointer key, gpointer value, gpointer user_data)
{
	const bt_packet *writer_packet = value;

	bt_packet_put_ref(writer_packet);
	return TRUE;
}

BT_HIDDEN
void trimmer_iterator_finalize(bt_self_message_iterator *it)
{
	struct trimmer_iterator *trim_it;

	trim_it = bt_self_message_iterator_get_user_data(it);
	BT_ASSERT(trim_it);

	bt_object_put_ref(trim_it->input_iterator);
	g_hash_table_foreach_remove(trim_it->packet_map,
			close_packets, NULL);
	g_hash_table_destroy(trim_it->packet_map);
	g_free(trim_it);
}

BT_HIDDEN
enum bt_message_iterator_status trimmer_iterator_init(
		bt_self_message_iterator *iterator,
		struct bt_private_port *port)
{
	enum bt_message_iterator_status ret =
		BT_MESSAGE_ITERATOR_STATUS_OK;
	enum bt_message_iterator_status it_ret;
	enum bt_connection_status conn_status;
	struct bt_private_port *input_port = NULL;
	struct bt_private_connection *connection = NULL;
	bt_self_component *component =
		bt_self_message_iterator_get_private_component(iterator);
	struct trimmer_iterator *it_data = g_new0(struct trimmer_iterator, 1);

	if (!it_data) {
		ret = BT_MESSAGE_ITERATOR_STATUS_NOMEM;
		goto end;
	}

	/* Create a new iterator on the upstream component. */
	input_port = bt_self_component_filter_get_input_port_by_name(
		component, "in");
	BT_ASSERT(input_port);
	connection = bt_private_port_get_connection(input_port);
	BT_ASSERT(connection);

	conn_status = bt_private_connection_create_message_iterator(connection,
			&it_data->input_iterator);
	if (conn_status != BT_CONNECTION_STATUS_OK) {
		ret = BT_MESSAGE_ITERATOR_STATUS_ERROR;
		goto end;
	}

	it_data->err = stderr;
	it_data->packet_map = g_hash_table_new_full(g_direct_hash,
			g_direct_equal, NULL, NULL);

	it_ret = bt_self_message_iterator_set_user_data(iterator,
		it_data);
	if (it_ret) {
		goto end;
	}
end:
	bt_object_put_ref(component);
	bt_object_put_ref(connection);
	bt_object_put_ref(input_port);
	return ret;
}

static
int update_lazy_bound(struct trimmer_bound *bound, const char *name,
		int64_t ts, bool *lazy_update)
{
	struct tm tm;
	int64_t value;
	time_t timeval;

	*lazy_update = false;

	if (!bound->lazy) {
		return 0;
	}
	tm.tm_isdst = -1;
	timeval = ts / NSEC_PER_SEC;

	if (bound->lazy_values.gmt) {
		/* Get day, month, year. */
		if (!bt_gmtime_r(&timeval, &tm)) {
			BT_LOGE_STR("Failure in bt_gmtime_r().");
			goto error;
		}
		tm.tm_sec = bound->lazy_values.ss;
		tm.tm_min = bound->lazy_values.mm;
		tm.tm_hour = bound->lazy_value.hh;
		timeval = bt_timegm(&tm);
		if (timeval < 0) {
			BT_LOGE("Failure in bt_timegm(), incorrectly formatted %s timestamp",
					name);
			goto error;
		}
	} else {
		/* Get day, month, year. */
		if (!bt_localtime_r(&timeval, &tm)) {
			BT_LOGE_STR("Failure in bt_localtime_r().");
			goto error;
		}
		tm.tm_sec = bound->lazy_values.ss;
		tm.tm_min = bound->lazy_values.mm;
		tm.tm_hour = bound->lazy_value.hh;
		timeval = mktime(&tm);
		if (timeval < 0) {
			BT_LOGE("Failure in mktime(), incorrectly formatted %s timestamp",
				name);
			goto error;
		}
	}
	value = (int64_t) timeval;
	value *= NSEC_PER_SEC;
	value += bound->lazy_values.ns;
	bound->value = value;
	bound->set = true;
	bound->lazy = false;
	*lazy_update = true;
	return 0;

error:
	return -1;
}

static
const bt_message *evaluate_event_message(
		const bt_message *message,
		struct trimmer_iterator *trim_it,
		struct trimmer_bound *begin, struct trimmer_bound *end,
		bool *_event_in_range, bool *finished)
{
	int64_t ts;
	int clock_ret;
	const bt_event *event = NULL, *writer_event;
	bool in_range = true;
	const bt_clock_class *clock_class = NULL;
	const bt_trace *trace = NULL;
	const bt_stream *stream = NULL;
	const bt_stream_class *stream_class = NULL;
	bt_clock_value *clock_value = NULL;
	bool lazy_update = false;
	const bt_message *new_message = NULL;
	bt_clock_class_priority_map *cc_prio_map;

	event = bt_message_event_get_event(message);
	BT_ASSERT(event);
	cc_prio_map = bt_message_event_get_clock_class_priority_map(
			message);
	BT_ASSERT(cc_prio_map);
	writer_event = trimmer_output_event(trim_it, event);
	BT_ASSERT(writer_event);
	new_message = bt_message_event_create(writer_event, cc_prio_map);
	BT_ASSERT(new_message);
	bt_object_put_ref(cc_prio_map);

	stream = bt_event_get_stream(event);
	BT_ASSERT(stream);

	stream_class = bt_stream_get_class(stream);
	BT_ASSERT(stream_class);

	trace = bt_stream_class_get_trace(stream_class);
	BT_ASSERT(trace);

	/* FIXME multi-clock? */
	clock_class = bt_trace_get_clock_class_by_index(trace, 0);
	if (!clock_class) {
		goto end;
	}

	clock_value = bt_event_get_clock_value(event, clock_class);
	if (!clock_value) {
		BT_LOGE_STR("Failed to retrieve clock value.");
		goto error;
	}

	clock_ret = bt_clock_value_get_value_ns_from_epoch(
			clock_value, &ts);
	if (clock_ret) {
		BT_LOGE_STR("Failed to retrieve clock value timestamp.");
		goto error;
	}
	if (update_lazy_bound(begin, "begin", ts, &lazy_update)) {
		goto end;
	}
	if (update_lazy_bound(end, "end", ts, &lazy_update)) {
		goto end;
	}
	if (lazy_update && begin->set && end->set) {
		if (begin->value > end->value) {
			BT_LOGE_STR("Unexpected: time range begin value is above end value.");
			goto error;
		}
	}
	if (begin->set && ts < begin->value) {
		in_range = false;
	}
	if (end->set && ts > end->value) {
		in_range = false;
		*finished = true;
	}

	goto end;

error:
	BT_MESSAGE_PUT_REF_AND_RESET(new_message);
end:
	bt_object_put_ref(event);
	bt_object_put_ref(writer_event);
	bt_clock_class_put_ref(clock_class);
	bt_trace_put_ref(trace);
	bt_stream_put_ref(stream);
	bt_stream_class_put_ref(stream_class);
	bt_object_put_ref(clock_value);
	*_event_in_range = in_range;
	return new_message;
}

static
int ns_from_integer_field(const bt_field *integer, int64_t *ns)
{
	int ret = 0;
	int is_signed;
	uint64_t raw_clock_value;
	const bt_field_class *integer_class = NULL;
	const bt_clock_class *clock_class = NULL;
	bt_clock_value *clock_value = NULL;

	integer_class = bt_field_get_class(integer);
	BT_ASSERT(integer_class);
	clock_class = bt_field_class_integer_get_mapped_clock_class(
		integer_class);
	if (!clock_class) {
		ret = -1;
		goto end;
	}

	is_signed = bt_field_class_integer_is_signed(integer_class);
	if (!is_signed) {
		ret = bt_field_unsigned_integer_get_value(integer,
				&raw_clock_value);
		if (ret) {
			goto end;
		}
	} else {
		/* Signed clock values are unsupported. */
		ret = -1;
		goto end;
	}

	clock_value = bt_clock_value_create(clock_class, raw_clock_value);
        if (!clock_value) {
		goto end;
	}

	ret = bt_clock_value_get_value_ns_from_epoch(clock_value, ns);
end:
	bt_field_class_put_ref(integer_class);
	bt_clock_class_put_ref(clock_class);
	bt_object_put_ref(clock_value);
	return ret;
}

static uint64_t ns_from_value(uint64_t frequency, uint64_t value)
{
	uint64_t ns;

	if (frequency == NSEC_PER_SEC) {
		ns = value;
	} else {
		ns = (uint64_t) ((1e9 * (double) value) / (double) frequency);
	}

	return ns;
}

/*
 * timestamp minus the offset.
 */
static
int64_t get_raw_timestamp(const bt_packet *writer_packet,
		int64_t timestamp)
{
	const bt_clock_class *writer_clock_class;
	int64_t sec_offset, cycles_offset, ns;
	const bt_trace *writer_trace;
	const bt_stream *writer_stream;
	const bt_stream_class *writer_stream_class;
	int ret;
	uint64_t freq;

	writer_stream = bt_packet_get_stream(writer_packet);
	BT_ASSERT(writer_stream);

	writer_stream_class = bt_stream_get_class(writer_stream);
	BT_ASSERT(writer_stream_class);

	writer_trace = bt_stream_class_get_trace(writer_stream_class);
	BT_ASSERT(writer_trace);

	/* FIXME multi-clock? */
	writer_clock_class = bt_trace_get_clock_class_by_index(
		writer_trace, 0);
	BT_ASSERT(writer_clock_class);

	ret = bt_clock_class_get_offset_s(writer_clock_class, &sec_offset);
	BT_ASSERT(!ret);
	ns = sec_offset * NSEC_PER_SEC;

	freq = bt_clock_class_get_frequency(writer_clock_class);
	BT_ASSERT(freq != -1ULL);

	ret = bt_clock_class_get_offset_cycles(writer_clock_class, &cycles_offset);
	BT_ASSERT(!ret);

	ns += ns_from_value(freq, cycles_offset);

	bt_clock_class_put_ref(writer_clock_class);
	bt_trace_put_ref(writer_trace);
	bt_stream_class_put_ref(writer_stream_class);
	bt_stream_put_ref(writer_stream);

	return timestamp - ns;
}

static
const bt_message *evaluate_packet_message(
		const bt_message *message,
		struct trimmer_iterator *trim_it,
		struct trimmer_bound *begin, struct trimmer_bound *end,
		bool *_packet_in_range, bool *finished)
{
	int64_t begin_ns, pkt_begin_ns, end_ns, pkt_end_ns;
	bool in_range = true;
	const bt_packet *packet = NULL, *writer_packet = NULL;
	const bt_field *packet_context = NULL,
			*timestamp_begin = NULL,
			*timestamp_end = NULL;
	const bt_message *new_message = NULL;
	enum bt_component_status ret;
	bool lazy_update = false;

        switch (bt_message_get_type(message)) {
	case BT_MESSAGE_TYPE_PACKET_BEGINNING:
		packet = bt_message_packet_beginning_get_packet(message);
		BT_ASSERT(packet);
		writer_packet = trimmer_new_packet(trim_it, packet);
		BT_ASSERT(writer_packet);
		break;
	case BT_MESSAGE_TYPE_PACKET_END:
		packet = bt_message_packet_end_get_packet(message);
		BT_ASSERT(packet);
		writer_packet = trimmer_close_packet(trim_it, packet);
		BT_ASSERT(writer_packet);
		break;
	default:
		goto end;
	}

	packet_context = bt_packet_get_context(writer_packet);
	if (!packet_context) {
		goto end_no_msg;
	}

	if (!bt_field_is_structure(packet_context)) {
		goto end_no_msg;
	}

	timestamp_begin = bt_field_structure_get_field_by_name(
			packet_context, "timestamp_begin");
	if (!timestamp_begin || !bt_field_is_integer(timestamp_begin)) {
		goto end_no_msg;
	}
	timestamp_end = bt_field_structure_get_field_by_name(
			packet_context, "timestamp_end");
	if (!timestamp_end || !bt_field_is_integer(timestamp_end)) {
		goto end_no_msg;
	}

	if (ns_from_integer_field(timestamp_begin, &pkt_begin_ns)) {
		goto end_no_msg;
	}
	if (ns_from_integer_field(timestamp_end, &pkt_end_ns)) {
		goto end_no_msg;
	}

	if (update_lazy_bound(begin, "begin", pkt_begin_ns, &lazy_update)) {
		goto end_no_msg;
	}
	if (update_lazy_bound(end, "end", pkt_end_ns, &lazy_update)) {
		goto end_no_msg;
	}
	if (lazy_update && begin->set && end->set) {
		if (begin->value > end->value) {
			BT_LOGE_STR("Unexpected: time range begin value is above end value.");
			goto end_no_msg;
		}
	}

	begin_ns = begin->set ? begin->value : INT64_MIN;
	end_ns = end->set ? end->value : INT64_MAX;

	/*
	 * Accept if there is any overlap between the selected region and the
	 * packet.
	 */
	in_range = (pkt_end_ns >= begin_ns) && (pkt_begin_ns <= end_ns);
	if (!in_range) {
		goto end_no_msg;
	}
	if (pkt_begin_ns > end_ns) {
		*finished = true;
	}

	if (begin_ns > pkt_begin_ns) {
		ret = update_packet_context_field(trim_it->err, writer_packet,
				"timestamp_begin",
				get_raw_timestamp(writer_packet, begin_ns));
		BT_ASSERT(!ret);
	}

	if (end_ns < pkt_end_ns) {
		ret = update_packet_context_field(trim_it->err, writer_packet,
				"timestamp_end",
				get_raw_timestamp(writer_packet, end_ns));
		BT_ASSERT(!ret);
	}

end:
        switch (bt_message_get_type(message)) {
	case BT_MESSAGE_TYPE_PACKET_BEGINNING:
		new_message = bt_message_packet_beginning_create(writer_packet);
		BT_ASSERT(new_message);
		break;
	case BT_MESSAGE_TYPE_PACKET_END:
		new_message = bt_message_packet_end_create(writer_packet);
		BT_ASSERT(new_message);
		break;
	default:
		break;
	}
end_no_msg:
	*_packet_in_range = in_range;
	bt_packet_put_ref(packet);
	bt_packet_put_ref(writer_packet);
	bt_object_put_ref(packet_context);
	bt_object_put_ref(timestamp_begin);
	bt_object_put_ref(timestamp_end);
	return new_message;
}

static
const bt_message *evaluate_stream_message(
		const bt_message *message,
		struct trimmer_iterator *trim_it)
{
	const bt_stream *stream;

	stream = bt_message_stream_end_get_stream(message);
	BT_ASSERT(stream);

	/* FIXME: useless copy */
	return bt_message_stream_end_create(stream);
}

/* Return true if the message should be forwarded. */
static
enum bt_message_iterator_status evaluate_message(
		const bt_message **message,
		struct trimmer_iterator *trim_it,
		struct trimmer_bound *begin, struct trimmer_bound *end,
		bool *in_range)
{
	enum bt_message_type type;
	const bt_message *new_message = NULL;
	bool finished = false;

	*in_range = true;
	type = bt_message_get_type(*message);
	switch (type) {
	case BT_MESSAGE_TYPE_EVENT:
	        new_message = evaluate_event_message(*message,
				trim_it, begin, end, in_range, &finished);
		break;
	case BT_MESSAGE_TYPE_PACKET_BEGINNING:
	case BT_MESSAGE_TYPE_PACKET_END:
	        new_message = evaluate_packet_message(*message,
				trim_it, begin, end, in_range, &finished);
		break;
	case BT_MESSAGE_TYPE_STREAM_END:
		new_message = evaluate_stream_message(*message,
				trim_it);
		break;
	default:
		break;
	}
	BT_MESSAGE_PUT_REF_AND_RESET(*message);
	*message = new_message;

	if (finished) {
		return BT_MESSAGE_ITERATOR_STATUS_END;
	}

	return BT_MESSAGE_ITERATOR_STATUS_OK;
}

BT_HIDDEN
bt_message_iterator_next_method_return trimmer_iterator_next(
		bt_self_message_iterator *iterator)
{
	struct trimmer_iterator *trim_it = NULL;
	bt_self_component *component = NULL;
	struct trimmer *trimmer = NULL;
	bt_message_iterator *source_it = NULL;
	bt_message_iterator_next_method_return ret = {
		.status = BT_MESSAGE_ITERATOR_STATUS_OK,
		.message = NULL,
	};
	bool message_in_range = false;

	trim_it = bt_self_message_iterator_get_user_data(iterator);
	BT_ASSERT(trim_it);

	component = bt_self_message_iterator_get_private_component(
		iterator);
	BT_ASSERT(component);
	trimmer = bt_self_component_get_user_data(component);
	BT_ASSERT(trimmer);

	source_it = trim_it->input_iterator;
	BT_ASSERT(source_it);

	while (!message_in_range) {
		ret.status = bt_message_iterator_next(source_it);
		if (ret.status != BT_MESSAGE_ITERATOR_STATUS_OK) {
			goto end;
		}

		ret.message = bt_message_iterator_get_message(
			        source_it);
		if (!ret.message) {
			ret.status = BT_MESSAGE_ITERATOR_STATUS_ERROR;
			goto end;
		}

	        ret.status = evaluate_message(&ret.message, trim_it,
				&trimmer->begin, &trimmer->end,
				&message_in_range);
		if (!message_in_range) {
			BT_OBJECT_PUT_REF_AND_RESET(ret.message);
		}

		if (ret.status != BT_MESSAGE_ITERATOR_STATUS_OK) {
			break;
		}
	}
end:
	bt_object_put_ref(component);
	return ret;
}
