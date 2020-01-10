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

#define BT_COMP_LOG_SELF_COMP (counter->self_comp)
#define BT_LOG_OUTPUT_LEVEL (counter->log_level)
#define BT_LOG_TAG "PLUGIN/FLT.UTILS.COUNTER"
#include "logging/comp-logging.h"

#include <babeltrace2/babeltrace.h>
#include "common/macros.h"
#include "common/common.h"
#include "common/assert.h"
#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include "plugins/common/param-validation/param-validation.h"

#include "counter.h"

#define PRINTF_COUNT(_what, _var, args...)				\
	do {								\
		if (counter->count._var != 0 || !counter->hide_zero) {	\
			printf("%15" PRIu64 " %s message%s\n",		\
				counter->count._var,			\
				(_what),				\
				counter->count._var == 1 ? "" : "s");	\
		}							\
	} while (0)

static
const char * const in_port_name = "in";

static
uint64_t get_total_count(struct counter *counter)
{
	return counter->count.event +
		counter->count.stream_begin +
		counter->count.stream_end +
		counter->count.packet_begin +
		counter->count.packet_end +
		counter->count.disc_events +
		counter->count.disc_packets +
		counter->count.msg_iter_inactivity +
		counter->count.other;
}

static
void print_count(struct counter *counter)
{
	uint64_t total = get_total_count(counter);

	PRINTF_COUNT("Event", event);
	PRINTF_COUNT("Stream beginning", stream_begin);
	PRINTF_COUNT("Stream end", stream_end);
	PRINTF_COUNT("Packet beginning", packet_begin);
	PRINTF_COUNT("Packet end", packet_end);
	PRINTF_COUNT("Discarded event", disc_events);
	PRINTF_COUNT("Discarded packet", disc_packets);
	PRINTF_COUNT("Message iterator inactivity", msg_iter_inactivity);

	if (counter->count.other > 0) {
		PRINTF_COUNT("Other (unknown)", other);
	}

	printf("%s%15" PRIu64 " message%s (TOTAL)%s\n",
		bt_common_color_bold(), total, total == 1 ? "" : "s",
		bt_common_color_reset());
	counter->last_printed_total = total;
}

static
void try_print_count(struct counter *counter, uint64_t msg_count)
{
	if (counter->step == 0) {
		/* No update */
		return;
	}

	counter->at += msg_count;

	if (counter->at >= counter->step) {
		counter->at = 0;
		print_count(counter);
		putchar('\n');
	}
}

static
void try_print_last(struct counter *counter)
{
	const uint64_t total = get_total_count(counter);

	if (total != counter->last_printed_total) {
		print_count(counter);
	}
}

static
void destroy_private_counter_data(struct counter *counter)
{
	if (counter) {
		bt_message_iterator_put_ref(
			counter->msg_iter);
		g_free(counter);
	}
}

BT_HIDDEN
void counter_finalize(bt_self_component_sink *comp)
{
	struct counter *counter;

	BT_ASSERT(comp);
	counter = bt_self_component_get_data(
			bt_self_component_sink_as_self_component(comp));
	BT_ASSERT(counter);
	try_print_last(counter);
	bt_message_iterator_put_ref(counter->msg_iter);
	g_free(counter);
}

static
struct bt_param_validation_map_value_entry_descr counter_params[] = {
	{ "step", BT_PARAM_VALIDATION_MAP_VALUE_ENTRY_OPTIONAL, { .type = BT_VALUE_TYPE_UNSIGNED_INTEGER } },
	{ "hide-zero", BT_PARAM_VALIDATION_MAP_VALUE_ENTRY_OPTIONAL, { .type = BT_VALUE_TYPE_BOOL } },
	BT_PARAM_VALIDATION_MAP_VALUE_ENTRY_END
};

BT_HIDDEN
bt_component_class_initialize_method_status counter_init(
		bt_self_component_sink *component,
		bt_self_component_sink_configuration *config,
		const bt_value *params,
		__attribute__((unused)) void *init_method_data)
{
	bt_component_class_initialize_method_status status;
	bt_self_component_add_port_status add_port_status;
	struct counter *counter = g_new0(struct counter, 1);
	const bt_value *step = NULL;
	const bt_value *hide_zero = NULL;
	enum bt_param_validation_status validation_status;
	gchar *validate_error = NULL;

	if (!counter) {
		status = BT_COMPONENT_CLASS_INITIALIZE_METHOD_STATUS_MEMORY_ERROR;
		goto error;
	}

	counter->self_comp =
		bt_self_component_sink_as_self_component(component);
	counter->log_level = bt_component_get_logging_level(
		bt_self_component_as_component(counter->self_comp));
	add_port_status = bt_self_component_sink_add_input_port(component,
		"in", NULL, NULL);
	if (add_port_status != BT_SELF_COMPONENT_ADD_PORT_STATUS_OK) {
		status = (int) add_port_status;
		goto error;
	}

	validation_status = bt_param_validation_validate(params,
		counter_params, &validate_error);
	if (validation_status == BT_PARAM_VALIDATION_STATUS_MEMORY_ERROR) {
		status = BT_COMPONENT_CLASS_INITIALIZE_METHOD_STATUS_MEMORY_ERROR;
		goto error;
	} else if (validation_status == BT_PARAM_VALIDATION_STATUS_VALIDATION_ERROR) {
		status = BT_COMPONENT_CLASS_INITIALIZE_METHOD_STATUS_ERROR;
		BT_COMP_LOGE_APPEND_CAUSE(counter->self_comp,
			"%s", validate_error);
		goto error;
	}

	counter->last_printed_total = -1ULL;
	counter->step = 10000;
	step = bt_value_map_borrow_entry_value_const(params, "step");
	if (step) {
		counter->step = bt_value_integer_unsigned_get(step);
	}

	hide_zero = bt_value_map_borrow_entry_value_const(params, "hide-zero");
	if (hide_zero) {
		counter->hide_zero = (bool) bt_value_bool_get(hide_zero);
	}

	bt_self_component_set_data(
		bt_self_component_sink_as_self_component(component),
		counter);

	status = BT_COMPONENT_CLASS_INITIALIZE_METHOD_STATUS_OK;
	goto end;

error:
	destroy_private_counter_data(counter);

end:
	g_free(validate_error);
	return status;
}

BT_HIDDEN
bt_component_class_sink_graph_is_configured_method_status
counter_graph_is_configured(
		bt_self_component_sink *comp)
{
	bt_component_class_sink_graph_is_configured_method_status status;
	bt_message_iterator_create_from_sink_component_status
		msg_iter_status;
	struct counter *counter;
	bt_message_iterator *iterator;

	counter = bt_self_component_get_data(
		bt_self_component_sink_as_self_component(comp));
	BT_ASSERT(counter);

	msg_iter_status = bt_message_iterator_create_from_sink_component(
		comp, bt_self_component_sink_borrow_input_port_by_name(comp,
			in_port_name), &iterator);
	if (msg_iter_status != BT_MESSAGE_ITERATOR_CREATE_FROM_SINK_COMPONENT_STATUS_OK) {
		status = (int) msg_iter_status;
		goto end;
	}

	BT_MESSAGE_ITERATOR_MOVE_REF(
		counter->msg_iter, iterator);

	status = BT_COMPONENT_CLASS_SINK_GRAPH_IS_CONFIGURED_METHOD_STATUS_OK;
end:
	return status;
}

BT_HIDDEN
bt_component_class_sink_consume_method_status counter_consume(
		bt_self_component_sink *comp)
{
	bt_component_class_sink_consume_method_status status =
		BT_COMPONENT_CLASS_SINK_CONSUME_METHOD_STATUS_OK;
	struct counter *counter;
	bt_message_iterator_next_status next_status;
	uint64_t msg_count;
	bt_message_array_const msgs;

	counter = bt_self_component_get_data(
			bt_self_component_sink_as_self_component(comp));
	BT_ASSERT_DBG(counter);

	if (G_UNLIKELY(!counter->msg_iter)) {
		try_print_last(counter);
		status = BT_COMPONENT_CLASS_SINK_CONSUME_METHOD_STATUS_END;
		goto end;
	}

	/* Consume messages */
	next_status = bt_message_iterator_next(
		counter->msg_iter, &msgs, &msg_count);
	if (next_status < 0) {
		status = (int) next_status;
		goto end;
	}

	switch (next_status) {
	case BT_MESSAGE_ITERATOR_NEXT_STATUS_OK:
	{
		uint64_t i;

		for (i = 0; i < msg_count; i++) {
			const bt_message *msg = msgs[i];

			BT_ASSERT_DBG(msg);
			switch (bt_message_get_type(msg)) {
			case BT_MESSAGE_TYPE_EVENT:
				counter->count.event++;
				break;
			case BT_MESSAGE_TYPE_PACKET_BEGINNING:
				counter->count.packet_begin++;
				break;
			case BT_MESSAGE_TYPE_PACKET_END:
				counter->count.packet_end++;
				break;
			case BT_MESSAGE_TYPE_MESSAGE_ITERATOR_INACTIVITY:
				counter->count.msg_iter_inactivity++;
				break;
			case BT_MESSAGE_TYPE_STREAM_BEGINNING:
				counter->count.stream_begin++;
				break;
			case BT_MESSAGE_TYPE_STREAM_END:
				counter->count.stream_end++;
				break;
			case BT_MESSAGE_TYPE_DISCARDED_EVENTS:
				counter->count.disc_events++;
				break;
			case BT_MESSAGE_TYPE_DISCARDED_PACKETS:
				counter->count.disc_packets++;
				break;
			default:
				counter->count.other++;
			}

			bt_message_put_ref(msg);
		}

		status = BT_COMPONENT_CLASS_SINK_CONSUME_METHOD_STATUS_OK;
		break;
	}
	case BT_MESSAGE_ITERATOR_NEXT_STATUS_AGAIN:
		status = BT_COMPONENT_CLASS_SINK_CONSUME_METHOD_STATUS_AGAIN;
		goto end;
	case BT_MESSAGE_ITERATOR_NEXT_STATUS_END:
		try_print_last(counter);
		status = BT_COMPONENT_CLASS_SINK_CONSUME_METHOD_STATUS_END;
		goto end;
	case BT_MESSAGE_ITERATOR_NEXT_STATUS_MEMORY_ERROR:
		status = BT_COMPONENT_CLASS_SINK_CONSUME_METHOD_STATUS_MEMORY_ERROR;
		goto end;
	default:
		break;
	}

	try_print_count(counter, msg_count);

end:
	return status;
}
