#ifndef BABELTRACE_GRAPH_PRIVATE_CONNECTION_PRIVATE_NOTIFICATION_ITERATOR_H
#define BABELTRACE_GRAPH_PRIVATE_CONNECTION_PRIVATE_NOTIFICATION_ITERATOR_H

/*
 * Copyright 2017 Philippe Proulx <pproulx@efficios.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
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

#ifdef __cplusplus
extern "C" {
#endif

struct bt_connection;
struct bt_private_port;
struct bt_private_connection;
struct bt_private_connection_private_notification_iterator;

extern struct bt_notification_iterator *
bt_private_connection_notification_iterator_borrow_from_private(
		struct bt_private_connection_private_notification_iterator *private_notification_iterator);

extern struct bt_private_component *
bt_private_connection_private_notification_iterator_get_private_component(
		struct bt_private_connection_private_notification_iterator *private_notification_iterator);

extern enum bt_notification_iterator_status
bt_private_connection_private_notification_iterator_set_user_data(
		struct bt_private_connection_private_notification_iterator *private_notification_iterator,
		void *user_data);

extern void *bt_private_connection_private_notification_iterator_get_user_data(
		struct bt_private_connection_private_notification_iterator *private_notification_iterator);

extern struct bt_graph *bt_private_connection_private_notification_iterator_borrow_graph(
		struct bt_private_connection_private_notification_iterator *private_notification_iterator);

#ifdef __cplusplus
}
#endif

#endif /* BABELTRACE_GRAPH_PRIVATE_CONNECTION_PRIVATE_NOTIFICATION_ITERATOR_H */
