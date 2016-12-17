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

#include "trimmer.h"
#include "iterator.h"
#include <babeltrace/plugin/notification/iterator.h>
#include <babeltrace/plugin/notification/notification.h>
#include <babeltrace/plugin/notification/event.h>
#include <babeltrace/plugin/notification/stream.h>
#include <babeltrace/plugin/notification/packet.h>
#include <babeltrace/plugin/filter.h>
#include <babeltrace/ctf-ir/event.h>
#include <babeltrace/ctf-ir/stream.h>
#include <babeltrace/ctf-ir/stream-class.h>
#include <babeltrace/ctf-ir/clock.h>
#include <babeltrace/ctf-ir/packet.h>
#include <babeltrace/ctf-ir/trace.h>
#include <babeltrace/ctf-ir/fields.h>
#include <assert.h>

static
void trimmer_iterator_destroy(struct bt_notification_iterator *it)
{
	struct trimmer_iterator *it_data;

	it_data = bt_notification_iterator_get_private_data(it);
	assert(it_data);

	if (it_data->input_iterator_group) {
		g_ptr_array_free(it_data->input_iterator_group, TRUE);
	}
	bt_put(it_data->current_notification);
	g_free(it_data);
}

BT_HIDDEN
enum bt_component_status trimmer_iterator_init(struct bt_component *component,
		struct bt_notification_iterator *iterator)
{
	enum bt_component_status ret = BT_COMPONENT_STATUS_OK;
	enum bt_notification_iterator_status it_ret;
	struct trimmer_iterator *it_data = g_new0(struct trimmer_iterator, 1);

	if (!it_data) {
		ret = BT_COMPONENT_STATUS_NOMEM;
		goto end;
	}

	/* FIXME init trimmer_iterator */
	it_ret = bt_notification_iterator_set_private_data(iterator, it_data);
	if (it_ret) {
		goto end;
	}

	it_ret = bt_notification_iterator_set_destroy_cb(iterator,
		        trimmer_iterator_destroy);
	if (it_ret) {
		ret = BT_COMPONENT_STATUS_ERROR;
		goto end;
	}

	it_ret = bt_notification_iterator_set_next_cb(iterator,
			trimmer_iterator_next);
	if (it_ret) {
		ret = BT_COMPONENT_STATUS_ERROR;
		goto end;
	}

	it_ret = bt_notification_iterator_set_get_cb(iterator,
			trimmer_iterator_get);
	if (it_ret) {
		ret = BT_COMPONENT_STATUS_ERROR;
		goto end;
	}

	it_ret = bt_notification_iterator_set_seek_time_cb(iterator,
			trimmer_iterator_seek_time);
	if (it_ret) {
		ret = BT_COMPONENT_STATUS_ERROR;
		goto end;
	}
end:
	return ret;
}

BT_HIDDEN
struct bt_notification *trimmer_iterator_get(
		struct bt_notification_iterator *iterator)
{
	struct trimmer_iterator *trim_it;

	trim_it = bt_notification_iterator_get_private_data(iterator);
	assert(trim_it);

	if (!trim_it->current_notification) {
		enum bt_notification_iterator_status it_ret;

		it_ret = trimmer_iterator_next(iterator);
		if (it_ret) {
			goto end;
		}
	}
end:
	return bt_get(trim_it->current_notification);
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
		if (!gmtime_r(&timeval, &tm)) {
			printf_error("Failure in gmtime_r()");
			goto error;
		}
		tm.tm_sec = bound->lazy_values.ss;
		tm.tm_min = bound->lazy_values.mm;
		tm.tm_hour = bound->lazy_values.hh;
		timeval = timegm(&tm);
		if (timeval < 0) {
			printf_error("Failure in timegm(), incorrectly formatted %s timestamp",
					name);
			goto error;
		}
	} else {
		/* Get day, month, year. */
		if (!localtime_r(&timeval, &tm)) {
			printf_error("Failure in localtime_r()");
			goto error;
		}
		tm.tm_sec = bound->lazy_values.ss;
		tm.tm_min = bound->lazy_values.mm;
		tm.tm_hour = bound->lazy_values.hh;
		timeval = mktime(&tm);
		if (timeval < 0) {
			printf_error("Failure in mktime(), incorrectly formatted %s timestamp",
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
enum bt_notification_iterator_status
evaluate_event_notification(struct bt_notification *notification,
		struct trimmer_bound *begin, struct trimmer_bound *end,
		bool *_event_in_range)
{
	int64_t ts;
	int clock_ret;
	struct bt_ctf_event *event = NULL;
	bool in_range = true;
	struct bt_ctf_clock *clock = NULL;
	struct bt_ctf_trace *trace = NULL;
	struct bt_ctf_stream *stream = NULL;
	struct bt_ctf_stream_class *stream_class = NULL;
	struct bt_ctf_clock_value *clock_value = NULL;
	enum bt_notification_iterator_status ret =
			BT_NOTIFICATION_ITERATOR_STATUS_OK;
	bool lazy_update = false;

	event = bt_notification_event_get_event(notification);
	assert(event);

	stream = bt_ctf_event_get_stream(event);
	assert(stream);

	stream_class = bt_ctf_stream_get_class(stream);
	assert(stream_class);

	trace = bt_ctf_stream_class_get_trace(stream_class);
	assert(trace);

	/* FIXME multi-clock? */
	clock = bt_ctf_trace_get_clock(trace, 0);
	if (!clock) {
		goto end;
	}

	clock_value = bt_ctf_event_get_clock_value(event, clock);
	if (!clock_value) {
		printf_error("Failed to retrieve clock value");
		ret = BT_NOTIFICATION_ITERATOR_STATUS_ERROR;
		goto end;
	}

	clock_ret = bt_ctf_clock_value_get_value_ns_from_epoch(
			clock_value, &ts);
	if (clock_ret) {
		printf_error("Failed to retrieve clock value timestamp");
		ret = BT_NOTIFICATION_ITERATOR_STATUS_ERROR;
		goto end;
	}
	if (update_lazy_bound(begin, "begin", ts, &lazy_update)) {
		ret = BT_NOTIFICATION_ITERATOR_STATUS_ERROR;
		goto end;
	}
	if (update_lazy_bound(end, "end", ts, &lazy_update)) {
		ret = BT_NOTIFICATION_ITERATOR_STATUS_ERROR;
		goto end;
	}
	if (lazy_update && begin->set && end->set) {
		if (begin->value > end->value) {
			printf_error("Unexpected: time range begin value is above end value");
			ret = BT_NOTIFICATION_ITERATOR_STATUS_ERROR;
			goto end;
		}
	}
	if (begin->set && ts < begin->value) {
		in_range = false;
	}
	if (end->set && ts > end->value) {
		in_range = false;
		ret = BT_NOTIFICATION_ITERATOR_STATUS_END;
	}
end:
	bt_put(event);
	bt_put(clock);
	bt_put(trace);
	bt_put(stream);
	bt_put(stream_class);
	bt_put(clock_value);
	*_event_in_range = in_range;
	return ret;
}

static
int ns_from_integer_field(struct bt_ctf_field *integer, int64_t *ns)
{
	int ret = 0;
	int is_signed;
	uint64_t raw_clock_value;
	struct bt_ctf_field_type *integer_type = NULL;
	struct bt_ctf_clock *clock = NULL;
	struct bt_ctf_clock_value *clock_value = NULL;

	integer_type = bt_ctf_field_get_type(integer);
	assert(integer_type);
	clock = bt_ctf_field_type_integer_get_mapped_clock(integer_type);
	if (!clock) {
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
		goto end;
	}

	clock_value = bt_ctf_clock_value_create(clock, raw_clock_value);
        if (!clock_value) {
		goto end;
	}

	ret = bt_ctf_clock_value_get_value_ns_from_epoch(clock_value, ns);
end:
	bt_put(integer_type);
	bt_put(clock);
	bt_put(clock_value);
	return ret;
}

static
enum bt_notification_iterator_status evaluate_packet_notification(
		struct bt_notification *notification,
		struct trimmer_bound *begin, struct trimmer_bound *end,
		bool *_packet_in_range)
{
        enum bt_notification_iterator_status ret =
			BT_NOTIFICATION_ITERATOR_STATUS_OK;
	int64_t begin_ns, pkt_begin_ns, end_ns, pkt_end_ns;
	bool in_range = true;
	struct bt_ctf_packet *packet = NULL;
	struct bt_ctf_field *packet_context = NULL,
			*timestamp_begin = NULL,
			*timestamp_end = NULL;

        switch (bt_notification_get_type(notification)) {
	case BT_NOTIFICATION_TYPE_PACKET_BEGIN:
		packet = bt_notification_packet_begin_get_packet(notification);
		break;
	case BT_NOTIFICATION_TYPE_PACKET_END:
		packet = bt_notification_packet_end_get_packet(notification);
		break;
	default:
		break;
	}
	assert(packet);

	packet_context = bt_ctf_packet_get_context(packet);
	if (!packet_context) {
		goto end;
	}

	if (!bt_ctf_field_is_structure(packet_context)) {
		goto end;
	}

	timestamp_begin = bt_ctf_field_structure_get_field(
			packet_context, "timestamp_begin");
	if (!timestamp_begin || !bt_ctf_field_is_integer(timestamp_begin)) {
		goto end;
	}
	timestamp_end = bt_ctf_field_structure_get_field(
			packet_context, "timestamp_end");
	if (!timestamp_end || !bt_ctf_field_is_integer(timestamp_end)) {
		goto end;
	}

	if (ns_from_integer_field(timestamp_begin, &pkt_begin_ns)) {
		goto end;
	}
	if (ns_from_integer_field(timestamp_end, &pkt_end_ns)) {
		goto end;
	}

	begin_ns = begin->set ? begin->value : INT64_MIN;
	end_ns = end->set ? end->value : INT64_MAX;

	/*
	 * Accept if there is any overlap between the selected region and the
	 * packet.
	 */
	in_range = (pkt_end_ns >= begin_ns) && (pkt_begin_ns <= end_ns);
	if (pkt_begin_ns > end_ns) {
		ret = BT_NOTIFICATION_ITERATOR_STATUS_END;
	}
end:
	*_packet_in_range = in_range;
	bt_put(packet);
	bt_put(packet_context);
	bt_put(timestamp_begin);
	bt_put(timestamp_end);
	return ret;
}

/* Return true if the notification should be forwarded. */
static
enum bt_notification_iterator_status evaluate_notification(
		struct bt_notification *notification,
		struct trimmer_bound *begin, struct trimmer_bound *end,
		bool *in_range)
{
	enum bt_notification_type type;
	enum bt_notification_iterator_status ret =
			BT_NOTIFICATION_ITERATOR_STATUS_OK;

	type = bt_notification_get_type(notification);
	switch (type) {
	case BT_NOTIFICATION_TYPE_EVENT:
	        ret = evaluate_event_notification(notification, begin,
				end, in_range);
		break;
	case BT_NOTIFICATION_TYPE_PACKET_BEGIN:
	case BT_NOTIFICATION_TYPE_PACKET_END:
	        ret = evaluate_packet_notification(notification, begin,
				end, in_range);
		break;
	default:
		/* Accept all other notifications. */
		break;
	}
	return ret;
}

BT_HIDDEN
enum bt_notification_iterator_status trimmer_iterator_next(
		struct bt_notification_iterator *iterator)
{
	struct trimmer_iterator *trim_it = NULL;
	struct bt_component *component = NULL;
	struct trimmer *trimmer = NULL;
	struct bt_notification_iterator *source_it = NULL;
	enum bt_notification_iterator_status ret =
			BT_NOTIFICATION_ITERATOR_STATUS_OK;
	enum bt_component_status component_ret;
	bool notification_in_range = false;

	trim_it = bt_notification_iterator_get_private_data(iterator);
	assert(trim_it);

	component = bt_notification_iterator_get_component(iterator);
	assert(component);
	trimmer = bt_component_get_private_data(component);
	assert(trimmer);

	/* FIXME, should handle input iterator groups. */
	component_ret = bt_component_filter_get_input_iterator(component, 0,
			&source_it);
	assert((component_ret == BT_COMPONENT_STATUS_OK) && source_it);

	while (!notification_in_range) {
		struct bt_notification *notification;

		ret = bt_notification_iterator_next(source_it);
		if (ret != BT_NOTIFICATION_ITERATOR_STATUS_OK) {
			goto end;
		}

		notification = bt_notification_iterator_get_notification(
			        source_it);
		if (!notification) {
			ret = BT_NOTIFICATION_ITERATOR_STATUS_ERROR;
			goto end;
		}

	        ret = evaluate_notification(notification,
				&trimmer->begin, &trimmer->end,
				&notification_in_range);
		if (notification_in_range) {
			BT_MOVE(trim_it->current_notification, notification);
		} else {
			bt_put(notification);
		}
	}
end:
	bt_put(source_it);
	bt_put(component);
	return ret;
}

BT_HIDDEN
enum bt_notification_iterator_status trimmer_iterator_seek_time(
		struct bt_notification_iterator *iterator, int64_t time)
{
	enum bt_notification_iterator_status ret;

	ret = BT_NOTIFICATION_ITERATOR_STATUS_OK;
end:
	return ret;
}
