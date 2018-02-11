#ifndef BABELTRACE_GRAPH_NOTIFICATION_PACKET_H
#define BABELTRACE_GRAPH_NOTIFICATION_PACKET_H

/*
 * BabelTrace - Plug-in Packet-related Notifications
 *
 * Copyright 2016 Jérémie Galarneau <jeremie.galarneau@efficios.com>
 *
 * Author: Jérémie Galarneau <jeremie.galarneau@efficios.com>
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

#ifdef __cplusplus
extern "C" {
#endif

struct bt_private_notification;
struct bt_notification;
struct bt_private_packet;
struct bt_packet;

extern struct bt_private_notification *
bt_private_notification_packet_begin_create(
		struct bt_private_packet *private_packet);

extern struct bt_private_notification *
bt_private_notification_packet_end_create(
		struct bt_private_packet *private_packet);

/*** BT_NOTIFICATION_TYPE_PACKET_BEGIN ***/
extern struct bt_packet *bt_notification_packet_begin_get_packet(
		struct bt_notification *notification);

/*** BT_NOTIFICATION_TYPE_PACKET_END ***/
extern struct bt_packet *bt_notification_packet_end_get_packet(
		struct bt_notification *notification);

#ifdef __cplusplus
}
#endif

#endif /* BABELTRACE_GRAPH_NOTIFICATION_PACKET_H */
