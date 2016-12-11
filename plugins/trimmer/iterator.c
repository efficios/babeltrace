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
	if (it_data->stream_to_packet_start_notification) {
		g_hash_table_destroy(it_data->stream_to_packet_start_notification);
	}
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

/*
 * Remove me. This is a temporary work-around due to our inhability to use
 * libbabeltrace-ctf from libbabeltrace-plugin.
 */
static
struct bt_ctf_stream *internal_bt_notification_get_stream(
		struct bt_notification *notification)
{
	struct bt_ctf_stream *stream = NULL;

	assert(notification);
	switch (bt_notification_get_type(notification)) {
	case BT_NOTIFICATION_TYPE_EVENT:
	{
		struct bt_ctf_event *event;

		event = bt_notification_event_get_event(notification);
		stream = bt_ctf_event_get_stream(event);
		bt_put(event);
		break;
	}
	case BT_NOTIFICATION_TYPE_PACKET_START:
	{
		struct bt_ctf_packet *packet;

		packet = bt_notification_packet_start_get_packet(notification);
		stream = bt_ctf_packet_get_stream(packet);
		bt_put(packet);
		break;
	}
	case BT_NOTIFICATION_TYPE_PACKET_END:
	{
		struct bt_ctf_packet *packet;

		packet = bt_notification_packet_end_get_packet(notification);
		stream = bt_ctf_packet_get_stream(packet);
		bt_put(packet);
		break;
	}
	case BT_NOTIFICATION_TYPE_STREAM_END:
		stream = bt_notification_stream_end_get_stream(notification);
		break;
	default:
		goto end;
	}
end:
	return stream;
}

/* Return true if the notification should be forwarded. */
static
bool evaluate_notification(struct bt_notification *notification,
		struct trimmer *trimmer, struct trimmer_iterator *it)
{
	bool ret = true;
	struct bt_ctf_clock *clock = NULL;
	enum bt_notification_type type;
	struct bt_ctf_trace *trace = NULL;
	struct bt_ctf_stream *stream = NULL;
	struct bt_ctf_stream_class *stream_class = NULL;
	struct bt_ctf_clock_value *clock_value = NULL;

	stream = internal_bt_notification_get_stream(notification);
	if (!stream) {
		goto end;
	}

	stream_class = bt_ctf_stream_get_class(stream);
	assert(stream_class);
	trace = bt_ctf_stream_class_get_trace(stream_class);
	assert(trace);

	type = bt_notification_get_type(notification);
	switch (type) {
	case BT_NOTIFICATION_TYPE_EVENT:
	{
		int64_t ts;
		int clock_ret;
		struct bt_ctf_event *event;

		clock = bt_ctf_trace_get_clock(trace, 0);
		if (!clock) {
			goto end;
		}

		event = bt_notification_event_get_event(notification);
	        assert(event);
		clock_value = bt_ctf_event_get_clock_value(event, clock);
		if (!clock_value) {
			printf_error("Failed to retrieve clock value\n");
			bt_put(event);
			goto end;
		}

		clock_ret = bt_ctf_clock_value_get_value_ns_from_epoch(
				clock_value, &ts);
		if (clock_ret) {
			printf_error("Failed to retrieve clock value timestamp\n");
			goto end;
		}

		if (trimmer->begin.set && ts < trimmer->begin.value) {
			ret = false;
		}
		if (trimmer->end.set && ts > trimmer->end.value) {
			ret = false;
		}
		break;
	}
	default:
		break;
	}
end:
	bt_put(clock);
	bt_put(trace);
	bt_put(stream);
	bt_put(clock_value);
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
			BT_NOTIFICATION_ITERATOR_STATUS_OK;;
	enum bt_component_status component_ret;
	bool event_in_range = false;

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

	while (!event_in_range) {
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

		event_in_range = evaluate_notification(notification, trimmer,
				trim_it);
		if (event_in_range) {
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
