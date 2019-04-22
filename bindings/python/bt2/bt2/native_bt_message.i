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


/* Output argument typemap for clock value output (always appends) */
%typemap(in, numinputs=0)
	(const bt_clock_snapshot **BTOUTCLOCKSNAPSHOT)
	(bt_clock_snapshot *temp_clock_snapshot = NULL) {
	$1 = &temp_clock_snapshot;
}

%typemap(argout)
	(const bt_clock_snapshot **BTOUTCLOCKSNAPSHOT) {
	if (*$1) {
		/* SWIG_Python_AppendOutput() steals the created object */
		$result = SWIG_Python_AppendOutput($result,
				SWIG_NewPointerObj(SWIG_as_voidptr(*$1),
					SWIGTYPE_p_bt_clock_snapshot, 0));
	} else {
		/* SWIG_Python_AppendOutput() steals Py_None */
		Py_INCREF(Py_None);
		$result = SWIG_Python_AppendOutput($result, Py_None);
	}
}

/* From message-const.h */

typedef enum bt_message_type {
	BT_MESSAGE_TYPE_EVENT = 0,
	BT_MESSAGE_TYPE_MESSAGE_ITERATOR_INACTIVITY = 1,
	BT_MESSAGE_TYPE_STREAM_BEGINNING = 2,
	BT_MESSAGE_TYPE_STREAM_END = 3,
	BT_MESSAGE_TYPE_PACKET_BEGINNING = 4,
	BT_MESSAGE_TYPE_PACKET_END = 5,
	BT_MESSAGE_TYPE_STREAM_ACTIVITY_BEGINNING = 6,
	BT_MESSAGE_TYPE_STREAM_ACTIVITY_END = 7,
	BT_MESSAGE_TYPE_DISCARDED_EVENTS = 8,
	BT_MESSAGE_TYPE_DISCARDED_PACKETS = 9,
} bt_message_type;

extern bt_message_type bt_message_get_type(const bt_message *message);

extern void bt_message_get_ref(const bt_message *message);

extern void bt_message_put_ref(const bt_message *message);

/* From message-event-const.h */

extern const bt_event *bt_message_event_borrow_event_const(
		const bt_message *message);

extern bt_clock_snapshot_state
bt_message_event_borrow_default_clock_snapshot_const(
		const bt_message *msg, const bt_clock_snapshot **BTOUTCLOCKSNAPSHOT);

extern const bt_clock_class *
bt_message_event_borrow_stream_class_default_clock_class_const(
		const bt_message *msg);

/* From message-event.h */

extern
bt_message *bt_message_event_create(
		bt_self_message_iterator *message_iterator,
		const bt_event_class *event_class,
		const bt_packet *packet);

extern
bt_message *bt_message_event_create_with_default_clock_snapshot(
		bt_self_message_iterator *message_iterator,
		const bt_event_class *event_class,
		const bt_packet *packet, uint64_t raw_clock_value);

extern bt_event *bt_message_event_borrow_event(
		bt_message *message);

/* From message-message-iterator-inactivity-const.h */

extern bt_clock_snapshot_state
bt_message_message_iterator_inactivity_borrow_default_clock_snapshot_const(
		const bt_message *msg, const bt_clock_snapshot **BTOUTCLOCKSNAPSHOT);

/* From message-message-iterator-inactivity.h */

extern
bt_message *bt_message_message_iterator_inactivity_create(
		bt_self_message_iterator *message_iterator,
		const bt_clock_class *default_clock_class, uint64_t raw_value);

/* From message-stream-beginning-const.h */

extern const bt_stream *bt_message_stream_beginning_borrow_stream_const(
		const bt_message *message);

/* From message-stream-beginning.h */

extern
bt_message *bt_message_stream_beginning_create(
		bt_self_message_iterator *message_iterator,
		const bt_stream *stream);

extern bt_stream *bt_message_stream_beginning_borrow_stream(
		bt_message *message);

/* From message-stream-end-const.h */

extern const bt_stream *bt_message_stream_end_borrow_stream_const(
		const bt_message *message);

/* From message-stream-end.h */

extern
bt_message *bt_message_stream_end_create(
		bt_self_message_iterator *message_iterator,
		const bt_stream *stream);

extern bt_stream *bt_message_stream_end_borrow_stream(
		bt_message *message);

/* From message-packet-beginning-const.h */

extern const bt_packet *bt_message_packet_beginning_borrow_packet_const(
		const bt_message *message);

extern bt_clock_snapshot_state
bt_message_packet_beginning_borrow_default_clock_snapshot_const(
		const bt_message *msg, const bt_clock_snapshot **BTOUTCLOCKSNAPSHOT);

extern const bt_clock_class *
bt_message_packet_beginning_borrow_stream_class_default_clock_class_const(
		const bt_message *msg);

/* From message-packet-beginning.h */

extern
bt_message *bt_message_packet_beginning_create(
		bt_self_message_iterator *message_iterator,
		const bt_packet *packet);

extern
bt_message *bt_message_packet_beginning_create_with_default_clock_snapshot(
		bt_self_message_iterator *message_iterator,
		const bt_packet *packet, uint64_t raw_value);

extern bt_packet *bt_message_packet_beginning_borrow_packet(
		bt_message *message);

/* From message-packet-end-const.h */

extern const bt_packet *bt_message_packet_end_borrow_packet_const(
		const bt_message *message);

extern bt_clock_snapshot_state
bt_message_packet_end_borrow_default_clock_snapshot_const(
		const bt_message *msg, const bt_clock_snapshot **BTOUTCLOCKSNAPSHOT);

extern const bt_clock_class *
bt_message_packet_end_borrow_stream_class_default_clock_class_const(
		const bt_message *msg);

/* From message-packet-end.h */

extern
bt_message *bt_message_packet_end_create(
		bt_self_message_iterator *message_iterator,
		const bt_packet *packet);

extern
bt_message *bt_message_packet_end_create_with_default_clock_snapshot(
		bt_self_message_iterator *message_iterator,
		const bt_packet *packet, uint64_t raw_value);

extern bt_packet *bt_message_packet_end_borrow_packet(
		bt_message *message);

/* From message-stream-activity-const.h */

typedef enum bt_message_stream_activity_clock_snapshot_state {
	BT_MESSAGE_STREAM_ACTIVITY_CLOCK_SNAPSHOT_STATE_KNOWN,
	BT_MESSAGE_STREAM_ACTIVITY_CLOCK_SNAPSHOT_STATE_UNKNOWN,
	BT_MESSAGE_STREAM_ACTIVITY_CLOCK_SNAPSHOT_STATE_INFINITE,
} bt_message_stream_activity_clock_snapshot_state;

/* From message-stream-activity-beginning-const.h */

extern bt_message_stream_activity_clock_snapshot_state
bt_message_stream_activity_beginning_borrow_default_clock_snapshot_const(
		const bt_message *msg, const bt_clock_snapshot **BTOUTCLOCKSNAPSHOT);

extern const bt_clock_class *
bt_message_stream_activity_beginning_borrow_stream_class_default_clock_class_const(
		const bt_message *msg);

extern const bt_stream *
bt_message_stream_activity_beginning_borrow_stream_const(
		const bt_message *message);

/* From message-stream-activity-beginning.h */

extern bt_message *bt_message_stream_activity_beginning_create(
		bt_self_message_iterator *message_iterator,
		const bt_stream *stream);

extern bt_stream *bt_message_stream_activity_beginning_borrow_stream(
		bt_message *message);

extern void bt_message_stream_activity_beginning_set_default_clock_snapshot_state(
		bt_message *msg,
		bt_message_stream_activity_clock_snapshot_state state);

extern void bt_message_stream_activity_beginning_set_default_clock_snapshot(
		bt_message *msg, uint64_t raw_value);

/* From message-stream-activity-end-const.h */

extern bt_message_stream_activity_clock_snapshot_state
bt_message_stream_activity_end_borrow_default_clock_snapshot_const(
		const bt_message *msg, const bt_clock_snapshot **BTOUTCLOCKSNAPSHOT);

extern const bt_clock_class *
bt_message_stream_activity_end_borrow_stream_class_default_clock_class_const(
		const bt_message *msg);

extern const bt_stream *
bt_message_stream_activity_end_borrow_stream_const(
		const bt_message *message);

/* From message-stream-activity-end.h */

extern bt_message *bt_message_stream_activity_end_create(
		bt_self_message_iterator *message_iterator,
		const bt_stream *stream);

extern void bt_message_stream_activity_end_set_default_clock_snapshot_state(
		bt_message *msg,
		bt_message_stream_activity_clock_snapshot_state state);

extern void bt_message_stream_activity_end_set_default_clock_snapshot(
		bt_message *msg, uint64_t raw_value);

extern bt_stream *bt_message_stream_activity_end_borrow_stream(
		bt_message *message);

/* From message-discarded-events-const.h */

extern bt_clock_snapshot_state
bt_message_discarded_events_borrow_default_beginning_clock_snapshot_const(
		const bt_message *msg, const bt_clock_snapshot **BTOUTCLOCKSNAPSHOT);

extern bt_clock_snapshot_state
bt_message_discarded_events_borrow_default_end_clock_snapshot_const(
		const bt_message *msg, const bt_clock_snapshot **BTOUTCLOCKSNAPSHOT);

extern const bt_clock_class *
bt_message_discarded_events_borrow_stream_class_default_clock_class_const(
		const bt_message *msg);

extern const bt_stream *
bt_message_discarded_events_borrow_stream_const(const bt_message *message);

extern bt_property_availability bt_message_discarded_events_get_count(
		const bt_message *message, uint64_t *OUT);

/* From message-discarded-events.h */

extern bt_message *bt_message_discarded_events_create(
		bt_self_message_iterator *message_iterator,
		const bt_stream *stream);

extern bt_message *bt_message_discarded_events_create_with_default_clock_snapshots(
		bt_self_message_iterator *message_iterator,
		const bt_stream *stream, uint64_t beginning_raw_value,
		uint64_t end_raw_value);

extern bt_stream *bt_message_discarded_events_borrow_stream(
		bt_message *message);

extern void bt_message_discarded_events_set_count(bt_message *message,
		uint64_t count);

/* From message-discarded-packets-const.h */

extern bt_clock_snapshot_state
bt_message_discarded_packets_borrow_default_beginning_clock_snapshot_const(
		const bt_message *msg, const bt_clock_snapshot **BTOUTCLOCKSNAPSHOT);

extern bt_clock_snapshot_state
bt_message_discarded_packets_borrow_default_end_clock_snapshot_const(
		const bt_message *msg, const bt_clock_snapshot **BTOUTCLOCKSNAPSHOT);

extern const bt_clock_class *
bt_message_discarded_packets_borrow_stream_class_default_clock_class_const(
		const bt_message *msg);

extern const bt_stream *
bt_message_discarded_packets_borrow_stream_const(const bt_message *message);

extern bt_property_availability bt_message_discarded_packets_get_count(
		const bt_message *message, uint64_t *OUT);

/* From message-discarded-packets.h */

extern bt_message *bt_message_discarded_packets_create(
		bt_self_message_iterator *message_iterator,
		const bt_stream *stream);

extern bt_message *bt_message_discarded_packets_create_with_default_clock_snapshots(
		bt_self_message_iterator *message_iterator,
		const bt_stream *stream, uint64_t beginning_raw_value,
		uint64_t end_raw_value);

extern bt_stream *bt_message_discarded_packets_borrow_stream(
		bt_message *message);

extern void bt_message_discarded_packets_set_count(bt_message *message,
		uint64_t count);
