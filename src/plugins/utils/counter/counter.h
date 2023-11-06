/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright 2017 Philippe Proulx <pproulx@efficios.com>
 */

#ifndef BABELTRACE_PLUGINS_UTILS_COUNTER_H
#define BABELTRACE_PLUGINS_UTILS_COUNTER_H

#include <glib.h>
#include <babeltrace2/babeltrace.h>
#include <stdbool.h>
#include <stdint.h>
#include "common/macros.h"

#ifdef __cplusplus
extern "C" {
#endif

struct counter {
	bt_message_iterator *msg_iter;
	struct {
		uint64_t event;
		uint64_t stream_begin;
		uint64_t stream_end;
		uint64_t packet_begin;
		uint64_t packet_end;
		uint64_t disc_events;
		uint64_t disc_packets;
		uint64_t msg_iter_inactivity;
		uint64_t other;
	} count;
	uint64_t last_printed_total;
	uint64_t at;
	uint64_t step;
	bool hide_zero;
	bt_logging_level log_level;
	bt_self_component *self_comp;
};

bt_component_class_initialize_method_status counter_init(
		bt_self_component_sink *component,
		bt_self_component_sink_configuration *config,
		const bt_value *params, void *init_method_data);

void counter_finalize(bt_self_component_sink *component);

bt_component_class_sink_graph_is_configured_method_status counter_graph_is_configured(
		bt_self_component_sink *component);

bt_component_class_sink_consume_method_status counter_consume(bt_self_component_sink *component);

#ifdef __cplusplus
}
#endif

#endif /* BABELTRACE_PLUGINS_UTILS_COUNTER_H */
