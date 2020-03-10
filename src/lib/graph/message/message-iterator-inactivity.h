/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright 2017-2018 Philippe Proulx <pproulx@efficios.com>
 */

#ifndef BABELTRACE_GRAPH_MESSAGE_MESSAGE_ITERATOR_INACTIVITY_INTERNAL_H
#define BABELTRACE_GRAPH_MESSAGE_MESSAGE_ITERATOR_INACTIVITY_INTERNAL_H

#include <glib.h>
#include "lib/trace-ir/clock-snapshot.h"
#include <babeltrace2/graph/message.h>

struct bt_message_message_iterator_inactivity {
	struct bt_message parent;
	struct bt_clock_snapshot *cs;
};

#endif /* BABELTRACE_GRAPH_MESSAGE_MESSAGE_ITERATOR_INACTIVITY_INTERNAL_H */
