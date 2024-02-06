/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright 2016-2018 Philippe Proulx <pproulx@efficios.com>
 */

#ifndef BABELTRACE_TRACE_IR_PACKET_INTERNAL_H
#define BABELTRACE_TRACE_IR_PACKET_INTERNAL_H

#include <stdbool.h>
#include <babeltrace2/trace-ir/clock-snapshot.h>
#include <babeltrace2/trace-ir/packet.h>
#include <babeltrace2/trace-ir/field.h>
#include <babeltrace2/trace-ir/stream.h>
#include "lib/object.h"

#include "field-wrapper.h"

struct bt_packet {
	struct bt_object base;
	struct bt_field_wrapper *context_field;
	struct bt_stream *stream;
	bool frozen;
};

void _bt_packet_set_is_frozen(const struct bt_packet *packet, bool is_frozen);

#ifdef BT_DEV_MODE
# define bt_packet_set_is_frozen	_bt_packet_set_is_frozen
#else
# define bt_packet_set_is_frozen(_packet, _is_frozen)
#endif /* BT_DEV_MODE */

struct bt_packet *bt_packet_new(struct bt_stream *stream);

void bt_packet_recycle(struct bt_packet *packet);

void bt_packet_destroy(struct bt_packet *packet);

#endif /* BABELTRACE_TRACE_IR_PACKET_INTERNAL_H */
