#ifndef BABELTRACE_PLUGINS_UTILS_MUXER_H
#define BABELTRACE_PLUGINS_UTILS_MUXER_H

/*
 * Copyright 2016 Jérémie Galarneau <jeremie.galarneau@efficios.com>
 * Copyright 2017 Philippe Proulx <pproulx@efficios.com>
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

#include <stdint.h>
#include <babeltrace/babeltrace-internal.h>

BT_HIDDEN
enum bt_component_status muxer_init(
		struct bt_private_component *priv_comp,
		struct bt_value *params, void *init_data);

BT_HIDDEN
void muxer_finalize(
		struct bt_private_component *priv_comp);

BT_HIDDEN
enum bt_notification_iterator_status muxer_notif_iter_init(
		struct bt_private_connection_private_notification_iterator *priv_notif_iter,
		struct bt_private_port *priv_port);

BT_HIDDEN
void muxer_notif_iter_finalize(
		struct bt_private_connection_private_notification_iterator *priv_notif_iter);

BT_HIDDEN
enum bt_notification_iterator_status muxer_notif_iter_next(
		struct bt_private_connection_private_notification_iterator *priv_notif_iter,
		bt_notification_array notifs, uint64_t capacity,
		uint64_t *count);

BT_HIDDEN
void muxer_port_connected(
		struct bt_private_component *priv_comp,
		struct bt_private_port *self_private_port,
		struct bt_port *other_port);

BT_HIDDEN
void muxer_port_disconnected(
		struct bt_private_component *priv_comp,
		struct bt_private_port *priv_port);

#endif /* BABELTRACE_PLUGINS_UTILS_MUXER_H */
