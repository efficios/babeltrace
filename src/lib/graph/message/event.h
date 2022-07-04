/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright 2017-2018 Philippe Proulx <pproulx@efficios.com>
 * Copyright 2016 Jérémie Galarneau <jeremie.galarneau@efficios.com>
 */

#ifndef BABELTRACE_GRAPH_MESSAGE_EVENT_INTERNAL_H
#define BABELTRACE_GRAPH_MESSAGE_EVENT_INTERNAL_H

#include "compat/compiler.h"
#include <babeltrace2/trace-ir/event-class.h>
#include <babeltrace2/trace-ir/event.h>
#include "common/assert.h"
#include "common/macros.h"

#include "message.h"

#ifdef __cplusplus
extern "C" {
#endif

struct bt_message_event {
	struct bt_message parent;
	struct bt_event *event;
	struct bt_clock_snapshot *default_cs;
};

struct bt_message *bt_message_event_new(struct bt_graph *graph);

void bt_message_event_recycle(struct bt_message *msg);

void bt_message_event_destroy(struct bt_message *msg);

#ifdef __cplusplus
}
#endif

#endif /* BABELTRACE_GRAPH_MESSAGE_EVENT_INTERNAL_H */
