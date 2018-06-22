#ifndef BABELTRACE_PLUGIN_TEXT_DMESG_DMESG_H
#define BABELTRACE_PLUGIN_TEXT_DMESG_DMESG_H

/*
 * Copyright 2017 Philippe Proulx <jeremie.galarneau@efficios.com>
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

#include <stdbool.h>
#include <babeltrace/babeltrace-internal.h>
#include <babeltrace/babeltrace.h>

BT_HIDDEN
enum bt_component_status dmesg_init(struct bt_private_component *priv_comp,
		struct bt_value *params, void *init_method_data);

BT_HIDDEN
void dmesg_finalize(struct bt_private_component *priv_comp);

BT_HIDDEN
enum bt_notification_iterator_status dmesg_notif_iter_init(
		struct bt_private_connection_private_notification_iterator *priv_notif_iter,
		struct bt_private_port *priv_port);

BT_HIDDEN
void dmesg_notif_iter_finalize(
		struct bt_private_connection_private_notification_iterator *priv_notif_iter);

BT_HIDDEN
enum bt_notification_iterator_status dmesg_notif_iter_next(
		struct bt_private_connection_private_notification_iterator *priv_notif_iter,
		bt_notification_array notifs, uint64_t capacity,
		uint64_t *count);

#endif /* BABELTRACE_PLUGIN_TEXT_DMESG_DMESG_H */
