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
#include <babeltrace/graph/notification-iterator.h>
#include <babeltrace/graph/private-notification-iterator.h>
#include <babeltrace/graph/notification.h>
#include <babeltrace/graph/notification-event.h>
#include <babeltrace/graph/notification-stream.h>
#include <babeltrace/graph/notification-packet.h>
#include <babeltrace/graph/component-filter.h>
#include <babeltrace/graph/private-component-filter.h>
#include <babeltrace/graph/private-port.h>
#include <babeltrace/graph/private-connection.h>
#include <babeltrace/graph/private-component.h>
#include <babeltrace/graph/connection.h>
#include <babeltrace/ctf-ir/event.h>
#include <babeltrace/ctf-ir/stream.h>
#include <babeltrace/ctf-ir/stream-class.h>
#include <babeltrace/ctf-ir/clock-class.h>
#include <babeltrace/ctf-ir/packet.h>
#include <babeltrace/ctf-ir/trace.h>
#include <babeltrace/ctf-ir/fields.h>
#include <assert.h>
#include <plugins-common.h>

#include "trimmer.h"
#include "iterator.h"
#include "copy.h"

static
gboolean close_packets(gpointer key, gpointer value, gpointer user_data)
{
	struct bt_ctf_packet *writer_packet = value;

	bt_put(writer_packet);
	return TRUE;
}

BT_HIDDEN
void trimmer_iterator_finalize(struct bt_private_notification_iterator *it)
{
	struct trimmer_iterator *trim_it;

	trim_it = bt_private_notification_iterator_get_user_data(it);
	assert(trim_it);

	bt_put(trim_it->input_iterator);
	g_hash_table_foreach_remove(trim_it->packet_map,
			close_packets, NULL);
	g_hash_table_destroy(trim_it->packet_map);
	g_free(trim_it);
}

BT_HIDDEN
enum bt_notification_iterator_status trimmer_iterator_init(
		struct bt_private_notification_iterator *iterator,
		struct bt_private_port *port)
{
	enum bt_notification_iterator_status ret =
		BT_NOTIFICATION_ITERATOR_STATUS_OK;
	enum bt_notification_iterator_status it_ret;
	enum bt_connection_status conn_status;
	struct bt_private_port *input_port = NULL;
	struct bt_private_connection *connection = NULL;
	struct bt_private_component *component =
		bt_private_notification_iterator_get_private_component(iterator);
	struct trimmer_iterator *it_data = g_new0(struct trimmer_iterator, 1);
	static const enum bt_notification_type notif_types[] = {
		BT_NOTIFICATION_TYPE_EVENT,
		BT_NOTIFICATION_TYPE_STREAM_END,
		BT_NOTIFICATION_TYPE_PACKET_BEGIN,
		BT_NOTIFICATION_TYPE_PACKET_END,
		BT_NOTIFICATION_TYPE_SENTINEL,
	};

	if (!it_data) {
		ret = BT_NOTIFICATION_ITERATOR_STATUS_NOMEM;
		goto end;
	}

	/* Create a new iterator on the upstream component. */
	input_port = bt_private_component_filter_get_input_private_port_by_name(
		component, "in");
	assert(input_port);
	connection = bt_private_port_get_private_connection(input_port);
	assert(connection);

	conn_status = bt_private_connection_create_notification_iterator(connection,
			notif_types, &it_data->input_iterator);
	if (conn_status != BT_CONNECTION_STATUS_OK) {
		ret = BT_NOTIFICATION_ITERATOR_STATUS_ERROR;
		goto end;
	}

	it_data->err = stderr;
	it_data->packet_map = g_hash_table_new_full(g_direct_hash,
			g_direct_equal, NULL, NULL);

	it_ret = bt_private_notification_iterator_set_user_data(iterator,
		it_data);
	if (it_ret) {
		goto end;
	}
end:
	bt_put(component);
	bt_put(connection);
	bt_put(input_port);
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
			BT_LOGE_STR("Failure in bt_gmtime_r()");
			goto error;
		}
		tm.tm_sec = bound->lazy_values.ss;
		tm.tm_min = bound->lazy_values.mm;
		tm.tm_hour = bound->lazy_values.hh;
		timeval = bt_timegm(&tm);
		if (timeval < 0) {
			BT_LOGE("Failure in bt_timegm(), incorrectly formatted %s timestamp",
					name);
			goto error;
		}
	} else {
		/* Get day, month, year. */
		if (!bt_localtime_r(&timeval, &tm)) {
			BT_LOGE_STR("Failure in bt_localtime_r()");
			goto error;
		}
		tm.tm_sec = bound->lazy_values.ss;
		tm.tm_min = bound->lazy_values.mm;
		tm.tm_hour = bound->lazy_values.hh;
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
struct bt_notification *evaluate_event_notification(
		struct bt_notification *notification,
		struct trimmer_iterator *trim_it,
		struct trimmer_bound *begin, struct trimmer_bound *end,
		bool *_event_in_range, bool *finished)
{
	int64_t ts;
	int clock_ret;
	struct bt_ctf_event *event = NULL, *writer_event;
	bool in_range = true;
	struct bt_ctf_clock_class *clock_class = NULL;
	struct bt_ctf_trace *trace = NULL;
	struct bt_ctf_stream *stream = NULL;
	struct bt_ctf_stream_class *stream_class = NULL;
	struct bt_ctf_clock_value *clock_value = NULL;
	bool lazy_update = false;
	struct bt_notification *new_notification = NULL;
	struct bt_clock_class_priority_map *cc_prio_map;

	event = bt_notification_event_get_event(notification);
	assert(event);
	cc_prio_map = bt_notification_event_get_clock_class_priority_map(
			notification);
	assert(cc_prio_map);
	writer_event = trimmer_output_event(trim_it, event);
	assert(writer_event);
	new_notification = bt_notification_event_create(writer_event, cc_prio_map);
	assert(new_notification);
	bt_put(cc_prio_map);

	stream = bt_ctf_event_get_stream(event);
	assert(stream);

	stream_class = bt_ctf_stream_get_class(stream);
	assert(stream_class);

	trace = bt_ctf_stream_class_get_trace(stream_class);
	assert(trace);

	/* FIXME multi-clock? */
	clock_class = bt_ctf_trace_get_clock_class_by_index(trace, 0);
	if (!clock_class) {
		goto end;
	}

	clock_value = bt_ctf_event_get_clock_value(event, clock_class);
	if (!clock_value) {
		BT_LOGE_STR("Failed to retrieve clock value");
		goto error;
	}

	clock_ret = bt_ctf_clock_value_get_value_ns_from_epoch(
			clock_value, &ts);
	if (clock_ret) {
		BT_LOGE_STR("Failed to retrieve clock value timestamp");
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
			BT_LOGE_STR("Unexpected: time range begin value is above end value");
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
	BT_PUT(new_notification);
end:
	bt_put(event);
	bt_put(writer_event);
	bt_put(clock_class);
	bt_put(trace);
	bt_put(stream);
	bt_put(stream_class);
	bt_put(clock_value);
	*_event_in_range = in_range;
	return new_notification;
}

static
int ns_from_integer_field(struct bt_ctf_field *integer, int64_t *ns)
{
	int ret = 0;
	int is_signed;
	uint64_t raw_clock_value;
	struct bt_ctf_field_type *integer_type = NULL;
	struct bt_ctf_clock_class *clock_class = NULL;
	struct bt_ctf_clock_value *clock_value = NULL;

	integer_type = bt_ctf_field_get_type(integer);
	assert(integer_type);
	clock_class = bt_ctf_field_type_integer_get_mapped_clock_class(
		integer_type);
	if (!clock_class) {
		ret = -1;
		goto end;
	}

	is_signed = bt_ctf_field_type_integer_get_signed(integer_type);
	if (!is_signed) {
		ret = bt_ctf_field_unsigned_integer_get_value(integer,
				&raw_clock_value);
		if (ret) {
			goto end;
		}
	} else {
		/* Signed clock values are unsupported. */
		ret = -1;
		goto end;
	}

	clock_value = bt_ctf_clock_value_create(clock_class, raw_clock_value);
        if (!clock_value) {
		goto end;
	}

	ret = bt_ctf_clock_value_get_value_ns_from_epoch(clock_value, ns);
end:
	bt_put(integer_type);
	bt_put(clock_class);
	bt_put(clock_value);
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
int64_t get_raw_timestamp(struct bt_ctf_packet *writer_packet,
		int64_t timestamp)
{
	struct bt_ctf_clock_class *writer_clock_class;
	int64_t sec_offset, cycles_offset, ns;
	struct bt_ctf_trace *writer_trace;
	struct bt_ctf_stream *writer_stream;
	struct bt_ctf_stream_class *writer_stream_class;
	int ret;
	uint64_t freq;

	writer_stream = bt_ctf_packet_get_stream(writer_packet);
	assert(writer_stream);

	writer_stream_class = bt_ctf_stream_get_class(writer_stream);
	assert(writer_stream_class);

	writer_trace = bt_ctf_stream_class_get_trace(writer_stream_class);
	assert(writer_trace);

	/* FIXME multi-clock? */
	writer_clock_class = bt_ctf_trace_get_clock_class_by_index(
		writer_trace, 0);
	assert(writer_clock_class);

	ret = bt_ctf_clock_class_get_offset_s(writer_clock_class, &sec_offset);
	assert(!ret);
	ns = sec_offset * NSEC_PER_SEC;

	freq = bt_ctf_clock_class_get_frequency(writer_clock_class);
	assert(freq != -1ULL);

	ret = bt_ctf_clock_class_get_offset_cycles(writer_clock_class, &cycles_offset);
	assert(!ret);

	ns += ns_from_value(freq, cycles_offset);

	bt_put(writer_clock_class);
	bt_put(writer_trace);
	bt_put(writer_stream_class);
	bt_put(writer_stream);

	return timestamp - ns;
}

static
struct bt_notification *evaluate_packet_notification(
		struct bt_notification *notification,
		struct trimmer_iterator *trim_it,
		struct trimmer_bound *begin, struct trimmer_bound *end,
		bool *_packet_in_range, bool *finished)
{
	int64_t begin_ns, pkt_begin_ns, end_ns, pkt_end_ns;
	bool in_range = true;
	struct bt_ctf_packet *packet = NULL, *writer_packet = NULL;
	struct bt_ctf_field *packet_context = NULL,
			*timestamp_begin = NULL,
			*timestamp_end = NULL;
	struct bt_notification *new_notification = NULL;
	enum bt_component_status ret;
	bool lazy_update = false;

        switch (bt_notification_get_type(notification)) {
	case BT_NOTIFICATION_TYPE_PACKET_BEGIN:
		packet = bt_notification_packet_begin_get_packet(notification);
		assert(packet);
		writer_packet = trimmer_new_packet(trim_it, packet);
		assert(writer_packet);
		break;
	case BT_NOTIFICATION_TYPE_PACKET_END:
		packet = bt_notification_packet_end_get_packet(notification);
		assert(packet);
		writer_packet = trimmer_close_packet(trim_it, packet);
		assert(writer_packet);
		break;
	default:
		goto end;
	}

	packet_context = bt_ctf_packet_get_context(writer_packet);
	if (!packet_context) {
		goto end_no_notif;
	}

	if (!bt_ctf_field_is_structure(packet_context)) {
		goto end_no_notif;
	}

	timestamp_begin = bt_ctf_field_structure_get_field(
			packet_context, "timestamp_begin");
	if (!timestamp_begin || !bt_ctf_field_is_integer(timestamp_begin)) {
		goto end_no_notif;
	}
	timestamp_end = bt_ctf_field_structure_get_field(
			packet_context, "timestamp_end");
	if (!timestamp_end || !bt_ctf_field_is_integer(timestamp_end)) {
		goto end_no_notif;
	}

	if (ns_from_integer_field(timestamp_begin, &pkt_begin_ns)) {
		goto end_no_notif;
	}
	if (ns_from_integer_field(timestamp_end, &pkt_end_ns)) {
		goto end_no_notif;
	}

	if (update_lazy_bound(begin, "begin", pkt_begin_ns, &lazy_update)) {
		goto end_no_notif;
	}
	if (update_lazy_bound(end, "end", pkt_end_ns, &lazy_update)) {
		goto end_no_notif;
	}
	if (lazy_update && begin->set && end->set) {
		if (begin->value > end->value) {
			BT_LOGE_STR("Unexpected: time range begin value is above end value");
			goto end_no_notif;
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
		goto end_no_notif;
	}
	if (pkt_begin_ns > end_ns) {
		*finished = true;
	}

	if (begin_ns > pkt_begin_ns) {
		ret = update_packet_context_field(trim_it->err, writer_packet,
				"timestamp_begin",
				get_raw_timestamp(writer_packet, begin_ns));
		assert(!ret);
	}

	if (end_ns < pkt_end_ns) {
		ret = update_packet_context_field(trim_it->err, writer_packet,
				"timestamp_end",
				get_raw_timestamp(writer_packet, end_ns));
		assert(!ret);
	}

end:
        switch (bt_notification_get_type(notification)) {
	case BT_NOTIFICATION_TYPE_PACKET_BEGIN:
		new_notification = bt_notification_packet_begin_create(writer_packet);
		assert(new_notification);
		break;
	case BT_NOTIFICATION_TYPE_PACKET_END:
		new_notification = bt_notification_packet_end_create(writer_packet);
		assert(new_notification);
		break;
	default:
		break;
	}
end_no_notif:
	*_packet_in_range = in_range;
	bt_put(packet);
	bt_put(writer_packet);
	bt_put(packet_context);
	bt_put(timestamp_begin);
	bt_put(timestamp_end);
	return new_notification;
}

static
struct bt_notification *evaluate_stream_notification(
		struct bt_notification *notification,
		struct trimmer_iterator *trim_it)
{
	struct bt_ctf_stream *stream;

	stream = bt_notification_stream_end_get_stream(notification);
	assert(stream);

	/* FIXME: useless copy */
	return bt_notification_stream_end_create(stream);
}

/* Return true if the notification should be forwarded. */
static
enum bt_notification_iterator_status evaluate_notification(
		struct bt_notification **notification,
		struct trimmer_iterator *trim_it,
		struct trimmer_bound *begin, struct trimmer_bound *end,
		bool *in_range)
{
	enum bt_notification_type type;
	struct bt_notification *new_notification = NULL;
	bool finished = false;

	*in_range = true;
	type = bt_notification_get_type(*notification);
	switch (type) {
	case BT_NOTIFICATION_TYPE_EVENT:
	        new_notification = evaluate_event_notification(*notification,
				trim_it, begin, end, in_range, &finished);
		break;
	case BT_NOTIFICATION_TYPE_PACKET_BEGIN:
	case BT_NOTIFICATION_TYPE_PACKET_END:
	        new_notification = evaluate_packet_notification(*notification,
				trim_it, begin, end, in_range, &finished);
		break;
	case BT_NOTIFICATION_TYPE_STREAM_END:
		new_notification = evaluate_stream_notification(*notification,
				trim_it);
		break;
	default:
		puts("Unhandled notification type");
		break;
	}
	BT_PUT(*notification);
	*notification = new_notification;

	if (finished) {
		return BT_NOTIFICATION_ITERATOR_STATUS_END;
	}

	return BT_NOTIFICATION_ITERATOR_STATUS_OK;
}

BT_HIDDEN
struct bt_notification_iterator_next_return trimmer_iterator_next(
		struct bt_private_notification_iterator *iterator)
{
	struct trimmer_iterator *trim_it = NULL;
	struct bt_private_component *component = NULL;
	struct trimmer *trimmer = NULL;
	struct bt_notification_iterator *source_it = NULL;
	struct bt_notification_iterator_next_return ret = {
		.status = BT_NOTIFICATION_ITERATOR_STATUS_OK,
		.notification = NULL,
	};
	bool notification_in_range = false;

	trim_it = bt_private_notification_iterator_get_user_data(iterator);
	assert(trim_it);

	component = bt_private_notification_iterator_get_private_component(
		iterator);
	assert(component);
	trimmer = bt_private_component_get_user_data(component);
	assert(trimmer);

	source_it = trim_it->input_iterator;
	assert(source_it);

	while (!notification_in_range) {
		ret.status = bt_notification_iterator_next(source_it);
		if (ret.status != BT_NOTIFICATION_ITERATOR_STATUS_OK) {
			goto end;
		}

		ret.notification = bt_notification_iterator_get_notification(
			        source_it);
		if (!ret.notification) {
			ret.status = BT_NOTIFICATION_ITERATOR_STATUS_ERROR;
			goto end;
		}

	        ret.status = evaluate_notification(&ret.notification, trim_it,
				&trimmer->begin, &trimmer->end,
				&notification_in_range);
		if (!notification_in_range) {
			BT_PUT(ret.notification);
		}

		if (ret.status != BT_NOTIFICATION_ITERATOR_STATUS_OK) {
			break;
		}
	}
end:
	bt_put(component);
	return ret;
}

BT_HIDDEN
enum bt_notification_iterator_status trimmer_iterator_seek_time(
		struct bt_private_notification_iterator *iterator,
		int64_t time)
{
	return BT_NOTIFICATION_ITERATOR_STATUS_OK;
}
