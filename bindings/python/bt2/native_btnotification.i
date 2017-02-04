/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2017 Philippe Proulx <pproulx@efficios.com>
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
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

%{
#include <babeltrace/component/notification/event.h>
#include <babeltrace/component/notification/packet.h>
#include <babeltrace/component/notification/schema.h>
#include <babeltrace/component/notification/stream.h>
%}

/* Type */
struct bt_notification;;

/* Notification type */
enum bt_notification_type {
	BT_NOTIFICATION_TYPE_UNKNOWN = -1,
	BT_NOTIFICATION_TYPE_ALL = 0,
	BT_NOTIFICATION_TYPE_EVENT = 1,
	BT_NOTIFICATION_TYPE_PACKET_BEGIN = 2,
	BT_NOTIFICATION_TYPE_PACKET_END = 3,
	BT_NOTIFICATION_TYPE_STREAM_END = 4,
	BT_NOTIFICATION_TYPE_NEW_TRACE = 5,
	BT_NOTIFICATION_TYPE_NEW_STREAM_CLASS = 6,
	BT_NOTIFICATION_TYPE_NEW_EVENT_CLASS = 7,
	BT_NOTIFICATION_TYPE_END_OF_TRACE = 8,
	BT_NOTIFICATION_TYPE_NR,
};

/* General functions */
enum bt_notification_type bt_notification_get_type(
		struct bt_notification *notification);

/* Event notification functions */
struct bt_notification *bt_notification_event_create(
		struct bt_ctf_event *event);
struct bt_ctf_event *bt_notification_event_get_event(
		struct bt_notification *notification);

/* Packet notification functions */
struct bt_notification *bt_notification_packet_begin_create(
		struct bt_ctf_packet *packet);
struct bt_ctf_packet *bt_notification_packet_begin_get_packet(
		struct bt_notification *notification);
struct bt_notification *bt_notification_packet_end_create(
		struct bt_ctf_packet *packet);
struct bt_ctf_packet *bt_notification_packet_end_get_packet(
		struct bt_notification *notification);

/* Schema notification functions */
/*
struct bt_notification *bt_notification_new_trace_create(
		struct bt_ctf_trace *trace);
struct bt_ctf_trace *bt_notification_new_trace_get_trace(
		struct bt_notification *notification);
struct bt_notification *bt_notification_new_stream_class_create(
		struct bt_ctf_stream_class *stream_class);
struct bt_ctf_trace *bt_notification_new_stream_class_get_stream_class(
		struct bt_notification *notification);
*/

/* Stream notification functions */
struct bt_notification *bt_notification_stream_end_create(
		struct bt_ctf_stream *stream);
struct bt_ctf_stream *bt_notification_stream_end_get_stream(
		struct bt_notification *notification);
