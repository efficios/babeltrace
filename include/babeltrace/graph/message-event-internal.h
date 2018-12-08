#ifndef BABELTRACE_GRAPH_MESSAGE_EVENT_INTERNAL_H
#define BABELTRACE_GRAPH_MESSAGE_EVENT_INTERNAL_H

/*
 * Copyright 2017-2018 Philippe Proulx <pproulx@efficios.com>
 * Copyright 2016 Jérémie Galarneau <jeremie.galarneau@efficios.com>
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

#include <babeltrace/compiler-internal.h>
#include <babeltrace/trace-ir/event-class.h>
#include <babeltrace/trace-ir/event.h>
#include <babeltrace/graph/message-internal.h>
#include <babeltrace/assert-internal.h>

#ifdef __cplusplus
extern "C" {
#endif

struct bt_message_event {
	struct bt_message parent;
	struct bt_event *event;
};

BT_HIDDEN
struct bt_message *bt_message_event_new(struct bt_graph *graph);

BT_HIDDEN
void bt_message_event_recycle(struct bt_message *msg);

BT_HIDDEN
void bt_message_event_destroy(struct bt_message *msg);

#ifdef __cplusplus
}
#endif

#endif /* BABELTRACE_GRAPH_MESSAGE_EVENT_INTERNAL_H */
