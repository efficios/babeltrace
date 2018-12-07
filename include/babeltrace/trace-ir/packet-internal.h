#ifndef BABELTRACE_TRACE_IR_PACKET_INTERNAL_H
#define BABELTRACE_TRACE_IR_PACKET_INTERNAL_H

/*
 * Copyright 2016-2018 Philippe Proulx <pproulx@efficios.com>
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

#include <stdbool.h>
#include <babeltrace/assert-internal.h>
#include <babeltrace/trace-ir/clock-value-const.h>
#include <babeltrace/trace-ir/packet.h>
#include <babeltrace/trace-ir/field.h>
#include <babeltrace/trace-ir/stream.h>
#include <babeltrace/trace-ir/field-wrapper-internal.h>
#include <babeltrace/object-internal.h>
#include <babeltrace/babeltrace-internal.h>
#include <babeltrace/property-internal.h>

struct bt_packet {
	struct bt_object base;
	struct bt_field_wrapper *header_field;
	struct bt_field_wrapper *context_field;
	struct bt_stream *stream;
	struct bt_clock_value *default_beginning_cv;
	struct bt_clock_value *default_end_cv;
	struct bt_property_uint discarded_event_counter_snapshot;
	struct bt_property_uint packet_counter_snapshot;
	bool frozen;
};

BT_HIDDEN
void _bt_packet_set_is_frozen(const struct bt_packet *packet, bool is_frozen);

#ifdef BT_DEV_MODE
# define bt_packet_set_is_frozen	_bt_packet_set_is_frozen
#else
# define bt_packet_set_is_frozen(_packet, _is_frozen)
#endif /* BT_DEV_MODE */

BT_HIDDEN
struct bt_packet *bt_packet_new(struct bt_stream *stream);

BT_HIDDEN
void bt_packet_recycle(struct bt_packet *packet);

BT_HIDDEN
void bt_packet_destroy(struct bt_packet *packet);

#endif /* BABELTRACE_TRACE_IR_PACKET_INTERNAL_H */
