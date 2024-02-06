/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright 2017-2018 Philippe Proulx <pproulx@efficios.com>
 * Copyright 2016 Jérémie Galarneau <jeremie.galarneau@efficios.com>
 */

#ifndef BABELTRACE_GRAPH_MESSAGE_STREAM_INTERNAL_H
#define BABELTRACE_GRAPH_MESSAGE_STREAM_INTERNAL_H

#include <babeltrace2/graph/message.h>

#include "compat/compiler.h"
#include "lib/trace-ir/stream.h"
#include "lib/trace-ir/clock-snapshot.h"

#include "message.h"

struct bt_message_stream {
	struct bt_message parent;
	struct bt_stream *stream;
	struct bt_clock_snapshot *default_cs;
	enum bt_message_stream_clock_snapshot_state default_cs_state;
};

static inline
const char *bt_message_stream_clock_snapshot_state_string(
		enum bt_message_stream_clock_snapshot_state state)
{
	switch (state) {
	case BT_MESSAGE_STREAM_CLOCK_SNAPSHOT_STATE_KNOWN:
		return "KNOWN";
	case BT_MESSAGE_STREAM_CLOCK_SNAPSHOT_STATE_UNKNOWN:
		return "UNKNOWN";
	default:
		return "(unknown)";
	}
}


#endif /* BABELTRACE_GRAPH_MESSAGE_STREAM_INTERNAL_H */
