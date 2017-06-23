#ifndef BABELTRACE_PLUGINS_UTILS_COUNTER_H
#define BABELTRACE_PLUGINS_UTILS_COUNTER_H

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

#include <glib.h>
#include <babeltrace/graph/private-component.h>
#include <babeltrace/graph/private-port.h>
#include <babeltrace/graph/port.h>
#include <stdbool.h>
#include <stdint.h>

struct counter {
	struct bt_notification_iterator *notif_iter;
	struct {
		uint64_t event;
		uint64_t stream_begin;
		uint64_t stream_end;
		uint64_t packet_begin;
		uint64_t packet_end;
		uint64_t inactivity;
		uint64_t discarded_events_notifs;
		uint64_t discarded_events;
		uint64_t discarded_packets_notifs;
		uint64_t discarded_packets;
		uint64_t other;
	} count;
	uint64_t last_printed_total;
	uint64_t step;
	bool hide_zero;
	bool error;
};

enum bt_component_status counter_init(struct bt_private_component *component,
		struct bt_value *params, void *init_method_data);
void counter_finalize(struct bt_private_component *component);
void counter_port_connected(struct bt_private_component *component,
		struct bt_private_port *self_port,
		struct bt_port *other_port);
enum bt_component_status counter_consume(struct bt_private_component *component);

#endif /* BABELTRACE_PLUGINS_UTILS_COUNTER_H */
