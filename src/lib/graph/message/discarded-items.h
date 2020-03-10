/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright 2019 Philippe Proulx <pproulx@efficios.com>
 */

#ifndef BABELTRACE_GRAPH_MESSAGE_DISCARDED_ITEMS_INTERNAL_H
#define BABELTRACE_GRAPH_MESSAGE_DISCARDED_ITEMS_INTERNAL_H

#include <glib.h>
#include "lib/trace-ir/clock-snapshot.h"
#include "lib/trace-ir/stream.h"
#include "lib/property.h"
#include <babeltrace2/graph/message.h>

#include "message.h"

struct bt_message_discarded_items {
	struct bt_message parent;
	struct bt_stream *stream;
	struct bt_clock_snapshot *default_begin_cs;
	struct bt_clock_snapshot *default_end_cs;
	struct bt_property_uint count;
};

#endif /* BABELTRACE_GRAPH_MESSAGE_DISCARDED_ITEMS_INTERNAL_H */
