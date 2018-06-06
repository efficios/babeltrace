/*
 * Copyright 2017 Philippe Proulx <pproulx@efficios.com>
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

#include <babeltrace/babeltrace.h>
#include <babeltrace/babeltrace-internal.h>
#include <babeltrace/common-internal.h>
#include <plugins-common.h>
#include <babeltrace/assert-internal.h>
#include <inttypes.h>
#include <stdint.h>

#include "counter.h"

#define PRINTF_COUNT(_what_sing, _what_plur, _var, args...)		\
	do {								\
		if (counter->count._var != 0 || !counter->hide_zero) {	\
			printf("%15" PRIu64 " %s\n",			\
				counter->count._var,			\
				counter->count._var == 1 ? _what_sing : _what_plur); \
		}							\
	} while (0)

static
uint64_t get_total_count(struct counter *counter)
{
	return counter->count.event +
		counter->count.stream_begin +
		counter->count.stream_end +
		counter->count.packet_begin +
		counter->count.packet_end +
		counter->count.inactivity +
		counter->count.discarded_events +
		counter->count.discarded_packets +
		counter->count.other;
}

static
void print_count(struct counter *counter, uint64_t total)
{
	PRINTF_COUNT("event", "events", event);
	PRINTF_COUNT("stream beginning", "stream beginnings", stream_begin);
	PRINTF_COUNT("stream end", "stream ends", stream_end);
	PRINTF_COUNT("packet beginning", "packet beginnings", packet_begin);
	PRINTF_COUNT("packet end", "packet ends", packet_end);
	PRINTF_COUNT("inactivity", "inactivities", inactivity);
	PRINTF_COUNT("discarded events notification",
		"discarded events notifications", discarded_events_notifs);
	PRINTF_COUNT("  known discarded event", "  known discarded events",
		discarded_events);
	PRINTF_COUNT("discarded packets notification",
		"discarded packets notifications", discarded_packets_notifs);
	PRINTF_COUNT("  known discarded packet", "  known discarded packets",
		discarded_packets);

	if (counter->count.other > 0) {
		PRINTF_COUNT("  other (unknown) notification",
			"  other (unknown) notifications", other);
	}

	printf("%s%15" PRIu64 " notification%s (TOTAL)%s\n",
		bt_common_color_bold(), total, total == 1 ? "" : "s",
		bt_common_color_reset());
	counter->last_printed_total = total;
}

static
void try_print_count(struct counter *counter)
{
	uint64_t total;

	if (counter->step == 0) {
		/* No update */
		return;
	}

	total = get_total_count(counter);

	if (total % counter->step == 0) {
		print_count(counter, total);
		putchar('\n');
	}
}

static
void try_print_last(struct counter *counter)
{
	const uint64_t total = get_total_count(counter);

	if (total != counter->last_printed_total) {
		print_count(counter, total);
	}
}

void destroy_private_counter_data(struct counter *counter)
{
	bt_put(counter->notif_iter);
	g_free(counter);
}

void counter_finalize(struct bt_private_component *component)
{
	struct counter *counter;

	BT_ASSERT(component);
	counter = bt_private_component_get_user_data(component);
	BT_ASSERT(counter);
	try_print_last(counter);
	bt_put(counter->notif_iter);
	g_free(counter);
}

enum bt_component_status counter_init(struct bt_private_component *component,
		struct bt_value *params, UNUSED_VAR void *init_method_data)
{
	enum bt_component_status ret;
	struct counter *counter = g_new0(struct counter, 1);
	struct bt_value *step = NULL;
	struct bt_value *hide_zero = NULL;

	if (!counter) {
		ret = BT_COMPONENT_STATUS_NOMEM;
		goto end;
	}

	ret = bt_private_component_sink_add_input_private_port(component,
		"in", NULL, NULL);
	if (ret != BT_COMPONENT_STATUS_OK) {
		goto end;
	}

	counter->last_printed_total = -1ULL;
	counter->step = 1000;
	step = bt_value_map_borrow(params, "step");
	if (step && bt_value_is_integer(step)) {
		int64_t val;

		(void) bt_value_integer_get(step, &val);

		if (val >= 0) {
			counter->step = (uint64_t) val;
		}
	}

	hide_zero = bt_value_map_borrow(params, "hide-zero");
	if (hide_zero && bt_value_is_bool(hide_zero)) {
		bt_bool val;

		(void) bt_value_bool_get(hide_zero, &val);
		counter->hide_zero = (bool) val;
	}

	ret = bt_private_component_set_user_data(component, counter);
	if (ret != BT_COMPONENT_STATUS_OK) {
		goto error;
	}

	goto end;

error:
	destroy_private_counter_data(counter);

end:
	return ret;
}

void counter_port_connected(
		struct bt_private_component *component,
		struct bt_private_port *self_port,
		struct bt_port *other_port)
{
	struct counter *counter;
	struct bt_notification_iterator *iterator;
	struct bt_private_connection *connection;
	enum bt_connection_status conn_status;

	counter = bt_private_component_get_user_data(component);
	BT_ASSERT(counter);
	connection = bt_private_port_get_private_connection(self_port);
	BT_ASSERT(connection);
	conn_status = bt_private_connection_create_notification_iterator(
		connection, &iterator);
	if (conn_status != BT_CONNECTION_STATUS_OK) {
		counter->error = true;
		goto end;
	}

	BT_MOVE(counter->notif_iter, iterator);

end:
	bt_put(connection);
}

enum bt_component_status counter_consume(struct bt_private_component *component)
{
	enum bt_component_status ret = BT_COMPONENT_STATUS_OK;
	struct counter *counter;
	enum bt_notification_iterator_status it_ret;
	int64_t count;
	struct bt_notification *notif = NULL;

	counter = bt_private_component_get_user_data(component);
	BT_ASSERT(counter);

	if (unlikely(counter->error)) {
		ret = BT_COMPONENT_STATUS_ERROR;
		goto end;
	}

	if (unlikely(!counter->notif_iter)) {
		try_print_last(counter);
		ret = BT_COMPONENT_STATUS_END;
		goto end;
	}

	/* Consume one notification  */
	it_ret = bt_private_connection_notification_iterator_next(
		counter->notif_iter, &notif);
	if (it_ret < 0) {
		ret = BT_COMPONENT_STATUS_ERROR;
		goto end;
	}

	switch (it_ret) {
	case BT_NOTIFICATION_ITERATOR_STATUS_AGAIN:
		ret = BT_COMPONENT_STATUS_AGAIN;
		goto end;
	case BT_NOTIFICATION_ITERATOR_STATUS_END:
		try_print_last(counter);
		ret = BT_COMPONENT_STATUS_END;
		goto end;
	case BT_NOTIFICATION_ITERATOR_STATUS_OK:
	{
		BT_ASSERT(notif);
		switch (bt_notification_get_type(notif)) {
		case BT_NOTIFICATION_TYPE_EVENT:
			counter->count.event++;
			break;
		case BT_NOTIFICATION_TYPE_INACTIVITY:
			counter->count.inactivity++;
			break;
		case BT_NOTIFICATION_TYPE_STREAM_BEGIN:
			counter->count.stream_begin++;
			break;
		case BT_NOTIFICATION_TYPE_STREAM_END:
			counter->count.stream_end++;
			break;
		case BT_NOTIFICATION_TYPE_PACKET_BEGIN:
			counter->count.packet_begin++;
			break;
		case BT_NOTIFICATION_TYPE_PACKET_END:
			counter->count.packet_end++;
			break;
		case BT_NOTIFICATION_TYPE_DISCARDED_EVENTS:
			counter->count.discarded_events_notifs++;
			count = bt_notification_discarded_events_get_count(
				notif);
			if (count >= 0) {
				counter->count.discarded_events += count;
			}
			break;
		case BT_NOTIFICATION_TYPE_DISCARDED_PACKETS:
			counter->count.discarded_packets_notifs++;
			count = bt_notification_discarded_packets_get_count(
				notif);
			if (count >= 0) {
				counter->count.discarded_packets += count;
			}
			break;
		default:
			counter->count.other++;
		}
	}
	default:
		break;
	}

	try_print_count(counter);

end:
	bt_put(notif);
	return ret;
}
