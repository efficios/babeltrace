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

/* Type */
struct bt_notification;

/* Notification type */
enum bt_notification_type {
	BT_NOTIFICATION_TYPE_SENTINEL = -1000,
	BT_NOTIFICATION_TYPE_UNKNOWN = -1,
	BT_NOTIFICATION_TYPE_ALL = -2,
	BT_NOTIFICATION_TYPE_EVENT = 0,
	BT_NOTIFICATION_TYPE_INACTIVITY = 1,
	BT_NOTIFICATION_TYPE_STREAM_BEGIN = 2,
	BT_NOTIFICATION_TYPE_STREAM_END = 3,
	BT_NOTIFICATION_TYPE_PACKET_BEGIN = 4,
	BT_NOTIFICATION_TYPE_PACKET_END = 5,
	BT_NOTIFICATION_TYPE_DISCARDED_EVENTS = 6,
	BT_NOTIFICATION_TYPE_DISCARDED_PACKETS = 7,
};

/* General functions */
enum bt_notification_type bt_notification_get_type(
		struct bt_notification *notification);

/* Event notification functions */
struct bt_notification *bt_notification_event_create(
		struct bt_event *event,
		struct bt_clock_class_priority_map *clock_class_priority_map);
struct bt_event *bt_notification_event_get_event(
		struct bt_notification *notification);
struct bt_clock_class_priority_map *
bt_notification_event_get_clock_class_priority_map(
		struct bt_notification *notification);

/* Inactivity notification functions */
struct bt_notification *bt_notification_inactivity_create(
		struct bt_clock_class_priority_map *clock_class_priority_map);
struct bt_clock_class_priority_map *
bt_notification_inactivity_get_clock_class_priority_map(
		struct bt_notification *notification);
struct bt_clock_value *bt_notification_inactivity_get_clock_value(
		struct bt_notification *notification,
		struct bt_clock_class *clock_class);
int bt_notification_inactivity_set_clock_value(
		struct bt_notification *notification,
		struct bt_clock_value *clock_value);

/* Packet notification functions */
struct bt_notification *bt_notification_packet_begin_create(
		struct bt_packet *packet);
struct bt_notification *bt_notification_packet_end_create(
		struct bt_packet *packet);
struct bt_packet *bt_notification_packet_begin_get_packet(
		struct bt_notification *notification);
struct bt_packet *bt_notification_packet_end_get_packet(
		struct bt_notification *notification);

/* Stream notification functions */
struct bt_notification *bt_notification_stream_begin_create(
		struct bt_stream *stream);
struct bt_notification *bt_notification_stream_end_create(
		struct bt_stream *stream);
struct bt_stream *bt_notification_stream_begin_get_stream(
		struct bt_notification *notification);
struct bt_stream *bt_notification_stream_end_get_stream(
		struct bt_notification *notification);

/* Discarded packets notification functions */
struct bt_clock_value *
bt_notification_discarded_packets_get_begin_clock_value(
		struct bt_notification *notification);
struct bt_clock_value *
bt_notification_discarded_packets_get_end_clock_value(
		struct bt_notification *notification);
int64_t bt_notification_discarded_packets_get_count(
		struct bt_notification *notification);
struct bt_stream *bt_notification_discarded_packets_get_stream(
		struct bt_notification *notification);

/* Discarded events notification functions */
struct bt_clock_value *
bt_notification_discarded_events_get_begin_clock_value(
		struct bt_notification *notification);
struct bt_clock_value *
bt_notification_discarded_events_get_end_clock_value(
		struct bt_notification *notification);
int64_t bt_notification_discarded_events_get_count(
		struct bt_notification *notification);
struct bt_stream *bt_notification_discarded_events_get_stream(
		struct bt_notification *notification);
