#ifndef BABELTRACE_GRAPH_NOTIFICATION_EVENT_H
#define BABELTRACE_GRAPH_NOTIFICATION_EVENT_H

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

/*
 * For bt_self_notification_iterator, bt_event, bt_packet,
 * bt_event_class, bt_notification
 */
#include <babeltrace/types.h>

#ifdef __cplusplus
extern "C" {
#endif

extern
bt_notification *bt_notification_event_create(
		bt_self_notification_iterator *notification_iterator,
		bt_event_class *event_class,
		bt_packet *packet);

extern bt_event *bt_notification_event_borrow_event(
		bt_notification *notification);

#ifdef __cplusplus
}
#endif

#endif /* BABELTRACE_GRAPH_NOTIFICATION_EVENT_H */
