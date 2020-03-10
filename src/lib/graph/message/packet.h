/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright 2017-2018 Philippe Proulx <pproulx@efficios.com>
 * Copyright 2016 Jérémie Galarneau <jeremie.galarneau@efficios.com>
 */

#ifndef BABELTRACE_GRAPH_MESSAGE_PACKET_INTERNAL_H
#define BABELTRACE_GRAPH_MESSAGE_PACKET_INTERNAL_H

#include "compat/compiler.h"
#include <babeltrace2/trace-ir/packet.h>
#include "lib/trace-ir/clock-snapshot.h"
#include "common/assert.h"
#include "common/macros.h"

#include "message.h"

struct bt_message_packet {
	struct bt_message parent;
	struct bt_packet *packet;
	struct bt_clock_snapshot *default_cs;
};

BT_HIDDEN
void bt_message_packet_destroy(struct bt_message *msg);

BT_HIDDEN
struct bt_message *bt_message_packet_beginning_new(
		struct bt_graph *graph);
BT_HIDDEN
void bt_message_packet_beginning_recycle(struct bt_message *msg);

BT_HIDDEN
struct bt_message *bt_message_packet_end_new(struct bt_graph *graph);

BT_HIDDEN
void bt_message_packet_end_recycle(struct bt_message *msg);

#endif /* BABELTRACE_GRAPH_MESSAGE_PACKET_INTERNAL_H */
