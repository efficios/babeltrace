#ifndef BABELTRACE2_GRAPH_MESSAGE_H
#define BABELTRACE2_GRAPH_MESSAGE_H

/*
 * Copyright (c) 2010-2019 EfficiOS Inc. and Linux Foundation
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

#ifndef __BT_IN_BABELTRACE_H
# error "Please include <babeltrace2/babeltrace.h> instead."
#endif

#include <babeltrace2/types.h>

#ifdef __cplusplus
extern "C" {
#endif

/*!
@defgroup api-msg Messages
@ingroup api-comp-cls-dev

@brief
    Elements exchanged between \bt_p_comp.

<strong><em>Messages</em></strong> are the objects which are exchanged
between \bt_p_comp in a trace processing \bt_graph to accomplish a
trace processing job.

\bt_cp_msg_iter create messages while message iterators \em and
\bt_p_sink_comp consume messages.

There are eight types of messages:

- \bt_c_sb_msg
- \bt_c_se_msg
- \bt_c_ev_msg
- \bt_c_pb_msg
- \bt_c_pe_msg
- \bt_c_disc_ev_msg
- \bt_c_disc_pkt_msg
- \bt_c_inac_msg

The type of a message is #bt_message.

Get the type enumerator of a message with bt_message_get_type().

A message is a \ref api-fund-shared-object "shared object": get a
new reference with bt_message_get_ref() and put an existing
reference with bt_message_put_ref().

Some library functions \ref api-fund-freezing "freeze" messages on
success. The documentation of those functions indicate this
postcondition.

Messages transport objects of the \ref api-tir API, which is an
intermediate representation of the tracing domain concepts.

All types of messages, except the \bt_inac_msg type, are related to a
specific <em>\bt_stream</em>, which represents a conceptual
\ref api-msg-seq "sequence of messages".

Some types of messages can have a default \bt_cs, depending on whether
or not their stream has a conceptual default clock, that is, whether or
not the stream's \ref api-tir-stream-cls "class" has a
\ref api-tir-stream-cls-prop-def-clock-cls "default clock class".
The creation functions for those types of messages contain
<code>_with_default_clock_snapshot</code> (for example,
bt_message_event_create_with_default_clock_snapshot()).

For the \bt_sb_msg and \bt_se_msg, the default clock snapshot property
is optional, therefore they have dedicated
bt_message_stream_beginning_set_default_clock_snapshot() and
bt_message_stream_end_set_default_clock_snapshot() functions.

All the message creation functions take a \bt_self_msg_iter as their
first parameter. This is because a message iterator method is the only
valid context to create a message.

<h1>Message types</h1>

This section details each type of message.

The following table shows the creation functions and types for each type
of message:

<table>
  <tr>
    <th>Name
    <th>Type enumerator
    <th>Creation functions
  <tr>
    <td>\ref api-msg-sb "Stream beginning"
    <td>#BT_MESSAGE_TYPE_STREAM_BEGINNING
    <td>bt_message_stream_beginning_create()
  <tr>
    <td>\ref api-msg-se "Stream end"
    <td>#BT_MESSAGE_TYPE_STREAM_END
    <td>bt_message_stream_end_create()
  <tr>
    <td>\ref api-msg-ev "Event"
    <td>#BT_MESSAGE_TYPE_EVENT
    <td>
      bt_message_event_create()<br>
      bt_message_event_create_with_default_clock_snapshot()<br>
      bt_message_event_create_with_packet()<br>
      bt_message_event_create_with_packet_and_default_clock_snapshot()
  <tr>
    <td>\ref api-msg-pb "Packet beginning"
    <td>#BT_MESSAGE_TYPE_PACKET_BEGINNING
    <td>
      bt_message_packet_beginning_create()<br>
      bt_message_packet_beginning_create_with_default_clock_snapshot()
  <tr>
    <td>\ref api-msg-pe "Packet end"
    <td>#BT_MESSAGE_TYPE_PACKET_END
    <td>
      bt_message_packet_end_create()<br>
      bt_message_packet_end_create_with_default_clock_snapshot()
  <tr>
    <td>\ref api-msg-disc-ev "Discarded events"
    <td>#BT_MESSAGE_TYPE_DISCARDED_EVENTS
    <td>
      bt_message_discarded_events_create()<br>
      bt_message_discarded_events_create_with_default_clock_snapshots()
  <tr>
    <td>\ref api-msg-disc-pkt "Discarded packets"
    <td>#BT_MESSAGE_TYPE_DISCARDED_PACKETS
    <td>
      bt_message_discarded_packets_create()<br>
      bt_message_discarded_packets_create_with_default_clock_snapshots()
  <tr>
    <td>\ref api-msg-inac "Message iterator inactivity"
    <td>#BT_MESSAGE_TYPE_MESSAGE_ITERATOR_INACTIVITY
    <td>bt_message_message_iterator_inactivity_create()
</table>

<h2>\anchor api-msg-sb Stream beginning message</h2>

A <strong><em>stream beginning message</em></strong> indicates the
beginning of a \bt_stream.

For a given stream:

- A stream beginning message is always the first one in the
  \ref api-msg-seq "message sequence".

- There can be only one stream beginning message.

Create a stream beginning message with
bt_message_stream_beginning_create().

A stream beginning message has the following properties:

<dl>
  <dt>\anchor api-msg-sb-prop-stream Stream</dt>
  <dd>
    \bt_c_stream of which the message indicates the beginning.

    You cannot change the stream once the message is created.

    Borrow a stream beginning message's stream with
    bt_message_stream_beginning_borrow_stream() and
    bt_message_stream_beginning_borrow_stream_const().
  </dd>

  <dt>
    \anchor api-msg-sb-prop-cs
    \bt_dt_opt Default \bt_cs
  </dt>
  <dd>
    Snapshot of the message's \bt_stream's default clock when the
    stream begins.

    A stream beginning message can only have a default clock snapshot
    if its stream's \ref api-tir-stream-cls "class" has a
    \ref api-tir-stream-cls-prop-def-clock-cls "default clock class".

    When a stream beginning message has no default clock snapshot,
    then its time is <em>unknown</em>.

    Set a stream beginning message's default clock snapshot with
    bt_message_stream_beginning_set_default_clock_snapshot().

    Borrow a stream beginning message's default clock snapshot with
    bt_message_stream_beginning_borrow_default_clock_snapshot_const().
  </dd>
</dl>

<h2>\anchor api-msg-se Stream end message</h2>

A <strong><em>stream end message</em></strong> indicates the
end of a \bt_stream.

For a given stream:

- A stream end message is always the last one in the
  \ref api-msg-seq "message sequence".

- There can be only one stream end message.

Create a stream end message with bt_message_stream_end_create().

A stream end message has the following properties:

<dl>
  <dt>\anchor api-msg-se-prop-stream Stream</dt>
  <dd>
    \bt_c_stream of which the message indicates the end.

    You cannot change the stream once the message is created.

    Borrow a stream end message's stream with
    bt_message_stream_end_borrow_stream() and
    bt_message_stream_end_borrow_stream_const().
  </dd>

  <dt>
    \anchor api-msg-se-prop-cs
    \bt_dt_opt Default \bt_cs
  </dt>
  <dd>
    Snapshot of the message's \bt_stream's default clock when the
    stream ends.

    A stream end message can only have a default clock snapshot
    if its stream's \ref api-tir-stream-cls "class" has a
    \ref api-tir-stream-cls-prop-def-clock-cls "default clock class".

    When a stream end message has no default clock snapshot, then its
    time is <em>unknown</em>.

    Set a stream end message's default clock snapshot with
    bt_message_stream_end_set_default_clock_snapshot().

    Borrow a stream end message's default clock snapshot with
    bt_message_stream_end_borrow_default_clock_snapshot_const().
  </dd>
</dl>

<h2>\anchor api-msg-ev Event message</h2>

An <strong><em>event message</em></strong> transports an \bt_ev and has,
possibly, a default \bt_cs.

Within its \bt_stream's \ref api-msg-seq "message sequence", an event
message can only occur:

<dl>
  <dt>
    If the stream's \ref api-tir-stream-cls "class"
    \ref api-tir-stream-cls-prop-supports-pkt "supports packets"
  </dt>
  <dd>After a \bt_pb_msg and before a \bt_pe_msg.</dd>

  <dt>
    If the stream's class does not support packets
  </dt>
  <dd>After the \bt_sb_msg and before the \bt_se_msg.</dd>
</dl>

To create an event message for a given stream, use:

<dl>
  <dt>
    If the stream's \ref api-tir-stream-cls "class"
    \ref api-tir-stream-cls-prop-supports-pkt "supports packets"
  </dt>
  <dd>
    <dl>
      <dt>
        If the stream's class has a
        \ref api-tir-stream-cls-prop-def-clock-cls "default clock class"
      </dt>
      <dd>bt_message_event_create_with_packet_and_default_clock_snapshot()</dd>

      <dt>
        If the stream's class does not have a
        \ref api-tir-stream-cls-prop-def-clock-cls "default clock class"
      </dt>
      <dd>bt_message_event_create_with_packet()</dd>
    </dl>

    Those two creation functions accept a \bt_pkt parameter which is
    the packet logically containing the message's event. A packet is
    part of a stream.
  </dd>

  <dt>
    If the stream's class does not supports packets
  </dt>
  <dd>
    <dl>
      <dt>
        If the stream's class has a default clock class
      </dt>
      <dd>bt_message_event_create_with_default_clock_snapshot()</dd>

      <dt>
        If the stream's class does not have a
        \ref api-tir-stream-cls-prop-def-clock-cls "default clock class"
      </dt>
      <dd>bt_message_event_create()</dd>
    </dl>
  </dd>
</dl>

The four creation functions above accept an \bt_ev_cls parameter. When
you create the message, the library instantiates this event class as an
\bt_ev. Borrow the resulting event with bt_message_event_borrow_event().
This event class must be part of the class of the event message's
stream.

An event message's event is initially <em>not set</em>: before you emit
the event message from a \bt_msg_iter's
\link api-msg-iter-cls-meth-next "next" method\endlink, you need to
borrow each of its \bt_p_field (with bt_event_borrow_payload_field(),
bt_event_borrow_specific_context_field(), and
bt_event_borrow_common_context_field()) and, recursively, set the values
of the all their inner fields.

An event message has the following properties:

<dl>
  <dt>\anchor api-msg-ev-prop-ev Event</dt>
  <dd>
    \bt_c_ev which the message transports.

    This is an instance of the \bt_ev_cls which was passed to the
    message's creation function.

    With this event, you can access its \bt_pkt (if any) with
    bt_event_borrow_packet_const() and its
    \bt_stream with bt_event_borrow_stream_const().

    Borrow an event message's event with bt_message_event_borrow_event()
    and bt_message_event_borrow_event_const().
  </dd>

  <dt>
    \anchor api-msg-ev-prop-cs
    \bt_dt_opt Default \bt_cs
  </dt>
  <dd>
    Snapshot of the message's \bt_stream's default clock when the
    event occurs.

    An event message has a default clock snapshot
    if its stream's \ref api-tir-stream-cls "class" has a
    \ref api-tir-stream-cls-prop-def-clock-cls "default clock class",
    and has none otherwise.

    Within its \bt_msg_iter's \ref api-msg-seq "message sequence",
    the default clock snapshot of an event message must be greater than
    or equal to any default clock snapshot of any previous message.

    Borrow an event message's default clock snapshot with
    bt_message_event_borrow_default_clock_snapshot_const().
  </dd>
</dl>

<h2>\anchor api-msg-pb Packet beginning message</h2>

A <strong><em>packet beginning message</em></strong> indicates the
beginning of a \bt_pkt.

A packet beginning message can only exist if its \bt_stream's
\ref api-tir-stream-cls "class"
\ref api-tir-stream-cls-prop-supports-pkt "supports packets".

For a given packet, there can be only one packet beginning message.

Within its \bt_stream's \ref api-msg-seq "message sequence", a packet
beginning message can only occur after the \bt_sb_msg and before the
\bt_se_msg.

To create a packet beginning message for a given stream, use:

<dl>
  <dt>
    If, for this stream's class,
    \ref api-tir-stream-cls-prop-pkt-beg-cs "packets have a beginning default clock snapshot"
  </dt>
  <dd>bt_message_packet_beginning_create_with_default_clock_snapshot()</dd>

  <dt>
    If, for this stream's class, packets do not have a beginning default
    clock snapshot
  </dt>
  <dd>bt_message_packet_beginning_create()</dd>
</dl>

A packet beginning message has the following properties:

<dl>
  <dt>\anchor api-msg-pb-prop-pkt Packet</dt>
  <dd>
    \bt_c_pkt of which the message indicates the beginning.

    You cannot change the packet once the message is created.

    Borrow a packet beginning message's packet with
    bt_message_packet_beginning_borrow_packet() and
    bt_message_packet_beginning_borrow_packet_const().
  </dd>

  <dt>
    \anchor api-msg-pb-prop-cs
    \bt_dt_opt Default \bt_cs
  </dt>
  <dd>
    Snapshot of the message's \bt_stream's default clock when the
    packet begins.

    A packet beginning message has a default clock snapshot if:

    - Its stream's \ref api-tir-stream-cls "class" has a
      \ref api-tir-stream-cls-prop-def-clock-cls "default clock class".

    - For its stream's class,
      \ref api-tir-stream-cls-prop-pkt-beg-cs "packets have a beginning default clock snapshot".

    Within its \bt_msg_iter's \ref api-msg-seq "message sequence",
    the default clock snapshot of a packet beginning message must be
    greater than or equal to any clock snapshot of any previous message.

    Borrow a packet beginning message's default clock snapshot with
    bt_message_packet_beginning_borrow_default_clock_snapshot_const().
  </dd>
</dl>

<h2>\anchor api-msg-pe Packet end message</h2>

A <strong><em>packet end message</em></strong> indicates the
end of a \bt_pkt.

A packet end message can only exist if its \bt_stream's
\ref api-tir-stream-cls "class"
\ref api-tir-stream-cls-prop-supports-pkt "supports packets".

For a given packet, there can be only one packet end message.

Within its \bt_stream's \ref api-msg-seq "message sequence", a packet
end message can only occur:

- After the \bt_sb_msg and before the \bt_se_msg.
- After a \bt_pb_msg for the same packet.

To create a packet end message for a given stream, use:

<dl>
  <dt>
    If, for this stream's class,
    \ref api-tir-stream-cls-prop-pkt-end-cs "packets have an end default clock snapshot"
  </dt>
  <dd>bt_message_packet_end_create_with_default_clock_snapshot()</dd>

  <dt>
    If, for this stream's class, packets do not have an end default
    clock snapshot
  </dt>
  <dd>bt_message_packet_end_create()</dd>
</dl>

A packet end message has the following properties:

<dl>
  <dt>\anchor api-msg-pe-prop-pkt Packet</dt>
  <dd>
    \bt_c_pkt of which the message indicates the end.

    You cannot change the packet once the message is created.

    Borrow a packet end message's packet with
    bt_message_packet_end_borrow_packet() and
    bt_message_packet_end_borrow_packet_const().
  </dd>

  <dt>
    \anchor api-msg-pe-prop-cs
    \bt_dt_opt Default \bt_cs
  </dt>
  <dd>
    Snapshot of the message's \bt_stream's default clock when the
    packet ends.

    A packet end message has a default clock snapshot if:

    - Its stream's \ref api-tir-stream-cls "class" has a
      \ref api-tir-stream-cls-prop-def-clock-cls "default clock class".

    - For its stream's class,
      \ref api-tir-stream-cls-prop-pkt-end-cs "packets have an end default clock snapshot".

    Within its \bt_msg_iter's \ref api-msg-seq "message sequence",
    the default clock snapshot of a packet end message must be greater
    than or equal to any clock snapshot of any previous message.

    Borrow a packet end message's default clock snapshot with
    bt_message_packet_end_borrow_default_clock_snapshot_const().
  </dd>
</dl>

<h2>\anchor api-msg-disc-ev Discarded events message</h2>

A <strong><em>discarded events message</em></strong> indicates that
events were discarded at <em>tracing time</em>. It does \em not indicate
that \bt_p_ev_msg were dropped during a trace processing \bt_graph run.

A discarded events message can only exist if its \bt_stream's
\ref api-tir-stream-cls "class"
\ref api-tir-stream-cls-prop-supports-disc-ev "supports discarded events".

Within its \bt_stream's \ref api-msg-seq "message sequence", a discarded
events message can only occur after the \bt_sb_msg and before the
\bt_se_msg.

To create a discarded events message for a given stream, use:

<dl>
  <dt>
    If, for this stream's class,
    \ref api-tir-stream-cls-prop-disc-ev-cs "discarded events have default clock snapshots"
  </dt>
  <dd>bt_message_discarded_events_create_with_default_clock_snapshots()</dd>

  <dt>
    If, for this stream's class, discarded events do not have default
    clock snapshots
  </dt>
  <dd>bt_message_discarded_events_create()</dd>
</dl>

A discarded events message has the following properties:

<dl>
  <dt>\anchor api-msg-disc-ev-prop-stream Stream</dt>
  <dd>
    \bt_c_stream into which events were discarded.

    You cannot change the stream once the message is created.

    Borrow a discarded events message's stream with
    bt_message_discarded_events_borrow_stream() and
    bt_message_discarded_events_borrow_stream_const().
  </dd>

  <dt>
    \anchor api-msg-disc-ev-prop-cs-beg
    \bt_dt_opt Beginning default \bt_cs
  </dt>
  <dd>
    Snapshot of the message's \bt_stream's default clock which indicates
    the beginning of the discarded events time range.

    A discarded events message has a beginning default clock snapshot
    if:

    - Its stream's \ref api-tir-stream-cls "class" has a
      \ref api-tir-stream-cls-prop-def-clock-cls "default clock class".

    - For its stream's class,
      \ref api-tir-stream-cls-prop-disc-ev-cs "discarded events have default clock snapshots".

    Within its \bt_msg_iter's \ref api-msg-seq "message sequence",
    the beginning default clock snapshot of a discarded events message
    must be greater than or equal to any clock snapshot of any previous
    message.

    Borrow a discarded events message's beginning default clock snapshot
    with
    bt_message_discarded_events_borrow_beginning_default_clock_snapshot_const().
  </dd>

  <dt>
    \anchor api-msg-disc-ev-prop-cs-end
    \bt_dt_opt End default \bt_cs
  </dt>
  <dd>
    Snapshot of the message's \bt_stream's default clock which indicates
    the end of the discarded events time range.

    A discarded events message has an end default clock snapshot if:

    - Its stream's \ref api-tir-stream-cls "class" has a
      \ref api-tir-stream-cls-prop-def-clock-cls "default clock class".

    - For its stream's class,
      \ref api-tir-stream-cls-prop-disc-ev-cs "discarded events have default clock snapshots".

    If a discarded events message has both a
    \ref api-msg-disc-ev-prop-cs-beg "beginning" and an end default
    clock snapshots, the end default clock snapshot must be greater than
    or equal to the beginning default clock snapshot.

    Within its \bt_msg_iter's \ref api-msg-seq "message sequence",
    the end default clock snapshot of a discarded events message must be
    greater than or equal to any clock snapshot of any previous message.

    Borrow a discarded events message's end default clock snapshot with
    bt_message_discarded_events_borrow_end_default_clock_snapshot_const().
  </dd>

  <dt>
    \anchor api-msg-disc-ev-prop-count
    \bt_dt_opt Discarded event count
  </dt>
  <dd>
    Exact number of discarded events.

    If this property is missing, then the number of discarded events
    is at least one.

    Use bt_message_discarded_events_set_count() and
    bt_message_discarded_events_get_count().
  </dd>
</dl>

<h2>\anchor api-msg-disc-pkt Discarded packets message</h2>

A <strong><em>discarded packets message</em></strong> indicates that
packets were discarded at <em>tracing time</em>. It does \em not
indicate that whole packets were dropped during a trace processing
\bt_graph run.

A discarded packets message can only exist if its \bt_stream's
\ref api-tir-stream-cls "class"
\ref api-tir-stream-cls-prop-supports-disc-pkt "supports discarded packets".

Within its \bt_stream's \ref api-msg-seq "message sequence", a discarded
packets message can only occur:

- After the \bt_sb_msg.
- Before the \bt_se_msg.
- One of:
  - Before any \bt_pb_msg.
  - After any \bt_pe_msg.
  - Between a packet end and a packet beginning message.

To create a discarded packets message for a given stream, use:

<dl>
  <dt>
    If, for this stream's class,
    \ref api-tir-stream-cls-prop-disc-pkt-cs "discarded packets have default clock snapshots"
  </dt>
  <dd>bt_message_discarded_packets_create_with_default_clock_snapshots()</dd>

  <dt>
    If, for this stream's class, discarded packets do not have default
    clock snapshots
  </dt>
  <dd>bt_message_discarded_packets_create()</dd>
</dl>

A discarded packets message has the following properties:

<dl>
  <dt>\anchor api-msg-disc-pkt-prop-stream Stream</dt>
  <dd>
    \bt_c_stream into which packets were discarded.

    You cannot change the stream once the message is created.

    Borrow a discarded packets message's stream with
    bt_message_discarded_packets_borrow_stream() and
    bt_message_discarded_packets_borrow_stream_const().
  </dd>

  <dt>
    \anchor api-msg-disc-pkt-prop-cs-beg
    \bt_dt_opt Beginning default \bt_cs
  </dt>
  <dd>
    Snapshot of the message's \bt_stream's default clock which indicates
    the beginning of the discarded packets time range.

    A discarded packets message has a beginning default clock snapshot
    if:

    - Its stream's \ref api-tir-stream-cls "class" has a
      \ref api-tir-stream-cls-prop-def-clock-cls "default clock class".

    - For its stream's class,
      \ref api-tir-stream-cls-prop-disc-pkt-cs "discarded packets have default clock snapshots".

    Within its \bt_msg_iter's \ref api-msg-seq "message sequence",
    the beginning default clock snapshot of a discarded packets message
    must be greater than or equal to any clock snapshot of any previous
    message.

    Borrow a discarded packets message's beginning default clock snapshot
    with
    bt_message_discarded_packets_borrow_beginning_default_clock_snapshot_const().
  </dd>

  <dt>
    \anchor api-msg-disc-pkt-prop-cs-end
    \bt_dt_opt End default \bt_cs
  </dt>
  <dd>
    Snapshot of the message's \bt_stream's default clock which indicates
    the end of the discarded packets time range.

    A discarded packets message has an end default clock snapshot if:

    - Its stream's \ref api-tir-stream-cls "class" has a
      \ref api-tir-stream-cls-prop-def-clock-cls "default clock class".

    - For its stream's class,
      \ref api-tir-stream-cls-prop-disc-pkt-cs "discarded packets have default clock snapshots".

    If a discarded packets message has both a
    \ref api-msg-disc-pkt-prop-cs-beg "beginning" and an end default
    clock snapshots, the end default clock snapshot must be greater than
    or equal to the beginning default clock snapshot.

    Within its \bt_msg_iter's \ref api-msg-seq "message sequence",
    the end default clock snapshot of a discarded packets message must
    be greater than or equal to any clock snapshot of any previous
    message.

    Borrow a discarded packets message's end default clock snapshot with
    bt_message_discarded_packets_borrow_end_default_clock_snapshot_const().
  </dd>

  <dt>
    \anchor api-msg-disc-pkt-prop-count
    \bt_dt_opt Discarded packet count
  </dt>
  <dd>
    Exact number of discarded packets.

    If this property is missing, then the number of discarded packets
    is at least one.

    Use bt_message_discarded_packets_set_count() and
    bt_message_discarded_packets_get_count().
  </dd>
</dl>

<h2>\anchor api-msg-inac Message iterator inactivity</h2>

A <strong><em>message iterator inactivity message</em></strong>
indicates that, within the \ref api-msg-seq "message sequence" of a
given \bt_msg_iter, there's no messages since the last message (if any)
until a given point in time.

A message iterator inactivity message is the only type of message that's
not related to a \bt_stream: it targets the whole message sequence of a
message iterator, and can occur at any position within the sequence.

This message is mostly significant for real-time message iterators: if a
message iterator A indicates that there's no messages until a given
point in time T, then a downstream filter message iterator B which
relies on multiple upstream message iterators does not have to wait for
new messages from A until T.

In other words, a message iterator inactivity message can help
downstream message iterators or \bt_p_sink_comp <em>progress</em>.

Create a message iterator inactivity message with
bt_message_message_iterator_inactivity_create(). You must pass a
\bt_clock_cls and the value of a fictitious (clock) instance to this
function so that it creates a \bt_cs.

A message iterator inactivity message has the following property:

<dl>
  <dt>
    \anchor api-msg-inac-prop-cs
    \bt_dt_opt \bt_c_cs
  </dt>
  <dd>
    Snapshot of a fictitious instance of the message's \bt_clock_cls
    which indicates the point in time until when there's no messages
    in the message iterator's \ref api-msg-seq "message sequence".

    Within its \bt_msg_iter's message sequence, the clock snapshot of a
    message iterator inactivity message must be greater than or equal to
    any clock snapshot of any previous message.

    Borrow a message iterator inactivity message's clock snapshot
    with
    bt_message_message_iterator_inactivity_borrow_clock_snapshot_const().
  </dd>
</dl>

<h1>\anchor api-msg-mip Message Interchange Protocol</h1>

The <em>Message Interchange Protocol</em> (MIP) is the system of rules
used by \bt_p_comp and \bt_p_msg_iter to exchance messages within a
trace processing graph.

The MIP covers everything related to messages and what they contain, as
well as how they are ordered within the \ref api-msg-seq "sequence" that
a message iterator produces.

For example:

- A valid message sequence for a given \bt_stream starts with a
  \bt_sb_msg and ends with a \bt_se_msg.

- The maximum
  \ref api-tir-fc-int-prop-size "field value range" for an \bt_uint_fc
  is [0,&nbsp;2<sup>64</sup>&nbsp;-&nbsp;1].

- The available message types are stream beginning and end, event,
  packet beginning and end, discarded events and packets, and message
  iterator inactivity.

The MIP has a version which is a single major number, independent from
the \bt_name project's version. As of \bt_name_version_min_maj, the only
available MIP version is 0.

If what the MIP covers changes in a breaking or semantical way in the
future, the MIP and \bt_name's minor versions will be bumped.

When you create a trace processing \bt_graph with bt_graph_create(), you
must pass the effective MIP version to use. Then, the components you
\ref api-graph-lc-add "add" to this graph can access this configured MIP
version with bt_self_component_get_graph_mip_version() and behave
accordingly. In other words, if the configured MIP version is 0, then a
component cannot use features introduced by MIP version&nbsp;1. For
example, should the project introduce a new type of \bt_fc, the MIP
version would be bumped.

A component which cannot honor a given MIP can fail at
initialization time, making the corresponding
<code>bt_graph_add_*_component*()</code> call fail too. To avoid any
surprise, you can create a \bt_comp_descr_set with descriptors of the
components you intend to add to a trace processing graph and call
bt_get_greatest_operative_mip_version() to get the greatest (most
recent) MIP version you can use.

To get the library's latest MIP version, use
bt_get_maximal_mip_version().

The ultimate goal of the MIP version feature is for the \bt_name project
to be able to introduce new features or even major breaking changes
without breaking existing \bt_p_comp_cls. This is especially important
considering that \bt_name supports \bt_p_plugin written by different
authors. Of course one of the project's objectives is to bump the MIP
version as rarely as possible. When it is required, though, it's a
welcome tool to make the project evolve gracefully.

The Message Interchange Protocol has no dedicated documentation as this
very message module (and its submodules, like \ref api-tir)
documentation is enough. You can consider that all the
functions of the message and trace IR objects have an implicit MIP
version \ref api-fund-pre-post "precondition". When a given
function documentation does not explicitly document a MIP version
precondition, it means that the effective MIP version has no effect on
said function's behaviour.

<h2>\anchor api-msg-seq Message sequence rules</h2>

The purpose of a \bt_msg_iter is to iterate a sequence of messages.

Those messages can be related to different \bt_p_stream:

@image html trace-structure-msg-seq.png "Messages of multiple streams as a single message sequence for a given message iterator."

However, for such a message sequence, the current \bt_mip
(version \bt_max_mip_version) dictates that:

<ul>
  <li>
    For a given \bt_stream:

    - The sequence must begin with a \bt_sb_msg.
    - The sequence must end with a \bt_se_msg.
    - <strong>If the stream's \ref api-tir-stream-cls "class"
      \ref api-tir-stream-cls-prop-supports-pkt "supports packets"</strong>:
      - Any \bt_pb_msg must be followed with a \bt_pe_msg.
      - All \bt_p_ev_msg must be between a packet beginning and a
        packet end message.
      - A \bt_disc_pkt_msg must be (one of):
        - Before the first packet beginning message.
        - Between a packet end message and a packet beginning message.
        - After the last packet end message.

    The rules above can be summarized by the following regular
    expressions:

    <dl>
      <dt>Without packets</dt>
      <dd>
        @code{.unparsed}
        SB (E | DE)* SE
        @endcode
      </dd>

      <dt>With packets</dt>
      <dd>
        @code{.unparsed}
        SB ((PB (E | DE)* PE) | DE | DP)* SE
        @endcode
      </dd>
    </dl>

    With this alphabet:

    <dl>
      <dt>SB</dt>
      <dd>\bt_c_sb_msg</dd>

      <dt>SE</dt>
      <dd>\bt_c_se_msg</dd>

      <dt>E</dt>
      <dd>\bt_c_ev_msg</dd>

      <dt>PB</dt>
      <dd>\bt_c_pb_msg</dd>

      <dt>PE</dt>
      <dd>\bt_c_pe_msg</dd>

      <dt>DE</dt>
      <dd>\bt_c_disc_ev_msg</dd>

      <dt>DP</dt>
      <dd>\bt_c_disc_pkt_msg</dd>
    </dl>
  <li>
    For a given message iterator, for any message with a \bt_cs, its
    clock snapshot must be greater than or equal to any clock snapshot
    of any previous message.

    For the scope of this rule, the clock snapshot of a \bt_disc_ev_msg
    or of a \bt_disc_pkt_msg is its beginning default clock snapshot.
  <li>
    For a given message iterator, the \bt_p_cs of all the messages of
    the sequence with a clock snapshot must be correlatable
    (see \ref api-tir-clock-cls-origin "Clock value vs. clock class origin").
</ul>
*/

/*! @{ */

/*!
@name Type
@{

@typedef struct bt_message bt_message;

@brief
    Message.

@}
*/

/*!
@name Type query
@{
*/

/*!
@brief
    Message type enumerators.
*/
typedef enum bt_message_type {
	/*!
	@brief
	    \bt_c_sb_msg.
	*/
	BT_MESSAGE_TYPE_STREAM_BEGINNING		= 1 << 0,

	/*!
	@brief
	    \bt_c_se_msg.
	*/
	BT_MESSAGE_TYPE_STREAM_END			= 1 << 1,

	/*!
	@brief
	    \bt_c_ev_msg.
	*/
	BT_MESSAGE_TYPE_EVENT				= 1 << 2,

	/*!
	@brief
	    \bt_c_pb_msg.
	*/
	BT_MESSAGE_TYPE_PACKET_BEGINNING		= 1 << 3,

	/*!
	@brief
	    \bt_c_pe_msg.
	*/
	BT_MESSAGE_TYPE_PACKET_END			= 1 << 4,

	/*!
	@brief
	    \bt_c_disc_ev_msg.
	*/
	BT_MESSAGE_TYPE_DISCARDED_EVENTS		= 1 << 5,

	/*!
	@brief
	    \bt_c_disc_pkt_msg.
	*/
	BT_MESSAGE_TYPE_DISCARDED_PACKETS		= 1 << 6,

	/*!
	@brief
	    \bt_c_inac_msg.
	*/
	BT_MESSAGE_TYPE_MESSAGE_ITERATOR_INACTIVITY	= 1 << 7,
} bt_message_type;

/*!
@brief
    Returns the type enumerator of the message \bt_p{message}.

@param[in] message
    Message of which to get the type enumerator

@returns
    Type enumerator of \bt_p{message}.

@bt_pre_not_null{message}
*/
extern bt_message_type bt_message_get_type(const bt_message *message);

/*! @} */

/*!
@name Common stream message
@{
*/

/*!
@brief
    Return type of
    bt_message_stream_beginning_borrow_default_clock_snapshot_const()
    and
    bt_message_stream_end_borrow_default_clock_snapshot_const().
*/
typedef enum bt_message_stream_clock_snapshot_state {
	/*!
	@brief
	    Known \bt_cs.
	*/
	BT_MESSAGE_STREAM_CLOCK_SNAPSHOT_STATE_KNOWN	= 1,

	/*!
	@brief
	    Unknown (no) \bt_cs.
	*/
	BT_MESSAGE_STREAM_CLOCK_SNAPSHOT_STATE_UNKNOWN	= 0,
} bt_message_stream_clock_snapshot_state;

/*! @} */

/*!
@name Stream beginning message
@{
*/

/*!
@brief
    Creates a \bt_sb_msg for the \bt_stream \bt_p{stream} from the
    \bt_msg_iter \bt_p{self_message_iterator}.

On success, the returned stream beginning message has the following
property values:

<table>
  <tr>
    <th>Property
    <th>Value
  <tr>
    <td>\ref api-msg-sb-prop-stream "Stream"
    <td>\bt_p{stream}
  <tr>
    <td>\ref api-msg-sb-prop-cs "Default clock snapshot"
    <td>\em None
</table>

@param[in] self_message_iterator
    Self message iterator from which to create the stream beginning
    message.
@param[in] stream
    Stream of which the message to create indicates the beginning.

@returns
    New stream beginning message reference, or \c NULL on memory error.

@bt_pre_not_null{self_message_iterator}
@bt_pre_not_null{stream}

@bt_post_success_frozen{stream}
*/
extern
bt_message *bt_message_stream_beginning_create(
		bt_self_message_iterator *self_message_iterator,
		const bt_stream *stream);

/*!
@brief
    Borrows the \bt_stream of the \bt_sb_msg \bt_p{message}.

See the \ref api-msg-sb-prop-stream "stream" property.

@param[in] message
    Stream beginning message from which to borrow the stream.

@returns
    @parblock
    \em Borrowed reference of the stream of \bt_p{message}.

    The returned pointer remains valid as long as \bt_p{message} exists.
    @endparblock

@bt_pre_not_null{message}
@bt_pre_is_sb_msg{message}

@sa bt_message_stream_beginning_borrow_stream_const() &mdash;
    \c const version of this function.
*/
extern bt_stream *bt_message_stream_beginning_borrow_stream(
		bt_message *message);

/*!
@brief
    Borrows the \bt_stream of the \bt_sb_msg \bt_p{message}
    (\c const version).

See bt_message_stream_beginning_borrow_stream().
*/
extern const bt_stream *bt_message_stream_beginning_borrow_stream_const(
		const bt_message *message);

/*!
@brief
    Sets the value, in clock cycles, of the default \bt_cs of the
    \bt_sb_msg \bt_p{message} to \bt_p{value}.

See the \ref api-msg-sb-prop-cs "default clock snapshot" property.

@param[in] message
    Stream beginning message of which to set the default clock snapshot
    value to \bt_p{value}.
@param[in] value
    New value (clock cycles) of the default clock snapshot of
    \bt_p{message}.

@bt_pre_not_null{message}
@bt_pre_hot{message}
@bt_pre_is_sb_msg{message}
@pre
    The \bt_stream_cls of \bt_p{message} has a
    \ref api-tir-stream-cls-prop-def-clock-cls "default clock class".

@sa bt_message_stream_beginning_borrow_default_clock_snapshot_const() &mdash;
    Borrows the default clock snapshot of a stream beginning message.
*/
extern
void bt_message_stream_beginning_set_default_clock_snapshot(
		bt_message *message, uint64_t value);

/*!
@brief
    Borrows the default \bt_cs of the \bt_sb_msg \bt_p{message}.

See the \ref api-msg-sb-prop-cs "default clock snapshot" property.

@param[in] message
    Stream beginning message from which to borrow the default clock
    snapshot.
@param[out] clock_snapshot
    <strong>If this function returns
    #BT_MESSAGE_STREAM_CLOCK_SNAPSHOT_STATE_KNOWN</strong>,
    \bt_p{*clock_snapshot} is a \em borrowed reference of the default
    clock snapshot of \bt_p{message}.

@retval #BT_MESSAGE_STREAM_CLOCK_SNAPSHOT_STATE_KNOWN
    The default clock snapshot of \bt_p{message} is known and returned
    as \bt_p{*clock_snapshot}.
@retval #BT_MESSAGE_STREAM_CLOCK_SNAPSHOT_STATE_UNKNOWN
    \bt_p{message} has no default clock snapshot: its time is unknown.

@bt_pre_not_null{message}
@bt_pre_is_sb_msg{message}
@pre
    The \bt_stream_cls of \bt_p{message} has a
    \ref api-tir-stream-cls-prop-def-clock-cls "default clock class".
@bt_pre_not_null{clock_snapshot}

@sa bt_message_stream_beginning_set_default_clock_snapshot() &mdash;
    Sets the default clock snapshot of a stream beginning message.
*/
extern bt_message_stream_clock_snapshot_state
bt_message_stream_beginning_borrow_default_clock_snapshot_const(
		const bt_message *message,
		const bt_clock_snapshot **clock_snapshot);

/*!
@brief
    Borrows the default \bt_clock_cls of the \bt_stream_cls
    of the \bt_sb_msg \bt_p{message}.

See the stream class's
\ref api-tir-stream-cls-prop-def-clock-cls "default clock class"
property.

This is a helper which is equivalent to

@code
bt_stream_class_borrow_default_clock_class_const(
    bt_stream_borrow_class_const(
        bt_message_stream_beginning_borrow_stream_const(message)))
@endcode

@param[in] message
    Stream beginning message from which to borrow its stream's class's
    default clock class.

@returns
    \em Borrowed reference of the default clock class of
    the stream class of \bt_p{message}, or \c NULL if none.

@bt_pre_not_null{message}
@bt_pre_is_sb_msg{message}
*/
extern const bt_clock_class *
bt_message_stream_beginning_borrow_stream_class_default_clock_class_const(
		const bt_message *message);

/*! @} */

/*!
@name Stream end message
@{
*/

/*!
@brief
    Creates a \bt_se_msg for the \bt_stream \bt_p{stream} from the
    \bt_msg_iter \bt_p{self_message_iterator}.

On success, the returned stream end message has the following
property values:

<table>
  <tr>
    <th>Property
    <th>Value
  <tr>
    <td>\ref api-msg-se-prop-stream "Stream"
    <td>\bt_p{stream}
  <tr>
    <td>\ref api-msg-se-prop-cs "Default clock snapshot"
    <td>\em None
</table>

@param[in] self_message_iterator
    Self message iterator from which to create the stream end
    message.
@param[in] stream
    Stream of which the message to create indicates the end.

@returns
    New stream end message reference, or \c NULL on memory error.

@bt_pre_not_null{self_message_iterator}
@bt_pre_not_null{stream}

@bt_post_success_frozen{stream}
*/
extern
bt_message *bt_message_stream_end_create(
		bt_self_message_iterator *self_message_iterator,
		const bt_stream *stream);

/*!
@brief
    Borrows the \bt_stream of the \bt_se_msg \bt_p{message}.

See the \ref api-msg-se-prop-stream "stream" property.

@param[in] message
    Stream end message from which to borrow the stream.

@returns
    @parblock
    \em Borrowed reference of the stream of \bt_p{message}.

    The returned pointer remains valid as long as \bt_p{message} exists.
    @endparblock

@bt_pre_not_null{message}
@bt_pre_is_se_msg{message}

@sa bt_message_stream_end_borrow_stream_const() &mdash;
    \c const version of this function.
*/
extern bt_stream *bt_message_stream_end_borrow_stream(
		bt_message *message);

/*!
@brief
    Borrows the \bt_stream of the \bt_se_msg \bt_p{message}
    (\c const version).

See bt_message_stream_end_borrow_stream().
*/
extern const bt_stream *bt_message_stream_end_borrow_stream_const(
		const bt_message *message);

/*!
@brief
    Sets the value, in clock cycles, of the default \bt_cs of the
    \bt_se_msg \bt_p{message} to \bt_p{value}.

See the \ref api-msg-se-prop-cs "default clock snapshot" property.

@param[in] message
    Stream end message of which to set the default clock snapshot
    value to \bt_p{value}.
@param[in] value
    New value (clock cycles) of the default clock snapshot of
    \bt_p{message}.

@bt_pre_not_null{message}
@bt_pre_hot{message}
@bt_pre_is_se_msg{message}
@pre
    The \bt_stream_cls of \bt_p{message} has a
    \ref api-tir-stream-cls-prop-def-clock-cls "default clock class".

@sa bt_message_stream_end_borrow_default_clock_snapshot_const() &mdash;
    Borrows the default clock snapshot of a stream end message.
*/
extern
void bt_message_stream_end_set_default_clock_snapshot(
		bt_message *message, uint64_t value);

/*!
@brief
    Borrows the default \bt_cs of the \bt_se_msg \bt_p{message}.

See the \ref api-msg-se-prop-cs "default clock snapshot" property.

@param[in] message
    Stream end message from which to borrow the default clock
    snapshot.
@param[out] clock_snapshot
    <strong>If this function returns
    #BT_MESSAGE_STREAM_CLOCK_SNAPSHOT_STATE_KNOWN</strong>,
    \bt_p{*clock_snapshot} is a \em borrowed reference of the default
    clock snapshot of \bt_p{message}.

@retval #BT_MESSAGE_STREAM_CLOCK_SNAPSHOT_STATE_KNOWN
    The default clock snapshot of \bt_p{message} is known and returned
    as \bt_p{*clock_snapshot}.
@retval #BT_MESSAGE_STREAM_CLOCK_SNAPSHOT_STATE_UNKNOWN
    \bt_p{message} has no default clock snapshot: its time is unknown.

@bt_pre_not_null{message}
@bt_pre_is_se_msg{message}
@pre
    The \bt_stream_cls of \bt_p{message} has a
    \ref api-tir-stream-cls-prop-def-clock-cls "default clock class".
@bt_pre_not_null{clock_snapshot}

@sa bt_message_stream_end_set_default_clock_snapshot() &mdash;
    Sets the default clock snapshot of a stream end message.
*/
extern bt_message_stream_clock_snapshot_state
bt_message_stream_end_borrow_default_clock_snapshot_const(
		const bt_message *message,
		const bt_clock_snapshot **clock_snapshot);

/*!
@brief
    Borrows the default \bt_clock_cls of the \bt_stream_cls
    of the \bt_se_msg \bt_p{message}.

See the stream class's
\ref api-tir-stream-cls-prop-def-clock-cls "default clock class"
property.

This is a helper which is equivalent to

@code
bt_stream_class_borrow_default_clock_class_const(
    bt_stream_borrow_class_const(
        bt_message_stream_end_borrow_stream_const(message)))
@endcode

@param[in] message
    Stream end message from which to borrow its stream's class's
    default clock class.

@returns
    \em Borrowed reference of the default clock class of
    the stream class of \bt_p{message}, or \c NULL if none.

@bt_pre_not_null{message}
@bt_pre_is_se_msg{message}
*/
extern const bt_clock_class *
bt_message_stream_end_borrow_stream_class_default_clock_class_const(
		const bt_message *message);

/*! @} */

/*!
@name Event message
@{
*/

/*!
@brief
    Creates an \bt_ev_msg, having an instance of the \bt_ev_cls
    \bt_p{event_class}, for the \bt_stream \bt_p{stream} from the
    \bt_msg_iter \bt_p{self_message_iterator}.

@attention
    @parblock
    Only use this function if

    @code
    bt_stream_class_supports_packets(bt_stream_borrow_class_const(stream))
    @endcode

    returns #BT_FALSE and

    @code
    bt_stream_class_borrow_default_clock_class_const(bt_stream_borrow_class_const(stream))
    @endcode

    returns \c NULL.

    Otherwise, use
    bt_message_event_create_with_default_clock_snapshot(),
    bt_message_event_create_with_packet(), or
    bt_message_event_create_with_packet_and_default_clock_snapshot().
    @endparblock

On success, the returned event message has the following property
values:

<table>
  <tr>
    <th>Property
    <th>Value
  <tr>
    <td>\ref api-msg-ev-prop-ev "Event"
    <td>
      An instance (with \bt_p_field that are not set) of
      \bt_p{event_class}.
  <tr>
    <td>\ref api-msg-se-prop-cs "Default clock snapshot"
    <td>\em None
</table>

@param[in] self_message_iterator
    Self message iterator from which to create the event message.
@param[in] event_class
    Class of the \bt_ev of the message to create.
@param[in] stream
    Stream conceptually containing the event of the message to create.

@returns
    New event message reference, or \c NULL on memory error.

@bt_pre_not_null{self_message_iterator}
@pre
    The \bt_stream_cls of \bt_p{event_class} is also the class of
    \bt_p{stream}, that is,
    <code>bt_event_class_borrow_stream_class_const(event_class)</code>
    and
    <code>bt_stream_borrow_class_const(stream)</code> have the
    same value.
@bt_pre_not_null{stream}
@pre
    <code>bt_stream_class_supports_packets(bt_stream_borrow_class_const(stream))</code>
    returns #BT_FALSE.
@pre
    <code>bt_stream_class_borrow_default_clock_class_const(bt_stream_borrow_class_const(stream))</code>
    returns \c NULL.

@bt_post_success_frozen{event_class}
@bt_post_success_frozen{stream}
*/
extern
bt_message *bt_message_event_create(
		bt_self_message_iterator *self_message_iterator,
		const bt_event_class *event_class,
		const bt_stream *stream);

/*!
@brief
    Creates an \bt_ev_msg, having an instance of the \bt_ev_cls
    \bt_p{event_class} and a default \bt_cs with the value
    \bt_p{clock_snapshot_value}, for the \bt_stream \bt_p{stream} from
    the \bt_msg_iter \bt_p{self_message_iterator}.

@attention
    @parblock
    Only use this function if

    @code
    bt_stream_class_supports_packets(bt_stream_borrow_class_const(stream))
    @endcode

    returns #BT_FALSE and

    @code
    bt_stream_class_borrow_default_clock_class_const(bt_stream_borrow_class_const(stream))
    @endcode

    does \em not return \c NULL.

    Otherwise, use
    bt_message_event_create(),
    bt_message_event_create_with_packet(), or
    bt_message_event_create_with_packet_and_default_clock_snapshot().
    @endparblock

On success, the returned event message has the following property
values:

<table>
  <tr>
    <th>Property
    <th>Value
  <tr>
    <td>\ref api-msg-ev-prop-ev "Event"
    <td>
      An instance (with \bt_p_field that are not set) of
      \bt_p{event_class}.
  <tr>
    <td>\ref api-msg-se-prop-cs "Default clock snapshot"
    <td>\bt_c_cs with the value \bt_p{clock_snapshot_value}.
</table>

@param[in] self_message_iterator
    Self message iterator from which to create the event message.
@param[in] event_class
    Class of the \bt_ev of the message to create.
@param[in] stream
    Stream conceptually containing the event of the message to create.
@param[in] clock_snapshot_value
    Value (clock cycles) of the default clock snapshot of
    \bt_p{message}.

@returns
    New event message reference, or \c NULL on memory error.

@bt_pre_not_null{self_message_iterator}
@pre
    The \bt_stream_cls of \bt_p{event_class} is also the class of
    \bt_p{stream}, that is,
    <code>bt_event_class_borrow_stream_class_const(event_class)</code>
    and
    <code>bt_stream_borrow_class_const(stream)</code> have the
    same value.
@bt_pre_not_null{stream}
@pre
    <code>bt_stream_class_supports_packets(bt_stream_borrow_class_const(stream))</code>
    returns #BT_FALSE.
@pre
    <code>bt_stream_class_borrow_default_clock_class_const(bt_stream_borrow_class_const(stream))</code>
    does \em not return \c NULL.

@bt_post_success_frozen{event_class}
@bt_post_success_frozen{stream}
*/
extern
bt_message *bt_message_event_create_with_default_clock_snapshot(
		bt_self_message_iterator *self_message_iterator,
		const bt_event_class *event_class,
		const bt_stream *stream, uint64_t clock_snapshot_value);

/*!
@brief
    Creates an \bt_ev_msg, having an instance of the \bt_ev_cls
    \bt_p{event_class}, for the \bt_pkt \bt_p{packet} from the
    \bt_msg_iter \bt_p{self_message_iterator}.

@attention
    @parblock
    Only use this function if

    @code
    bt_stream_class_supports_packets(
        bt_stream_borrow_class_const(bt_packet_borrow_stream_const(packet)))
    @endcode

    returns #BT_TRUE and

    @code
    bt_stream_class_borrow_default_clock_class_const(
        bt_stream_borrow_class_const(bt_packet_borrow_stream_const(packet)))
    @endcode

    returns \c NULL.

    Otherwise, use
    bt_message_event_create(),
    bt_message_event_create_with_default_clock_snapshot(), or
    bt_message_event_create_with_packet_and_default_clock_snapshot().
    @endparblock

On success, the returned event message has the following property
values:

<table>
  <tr>
    <th>Property
    <th>Value
  <tr>
    <td>\ref api-msg-ev-prop-ev "Event"
    <td>
      An instance (with \bt_p_field that are not set) of
      \bt_p{event_class}.
  <tr>
    <td>\ref api-msg-se-prop-cs "Default clock snapshot"
    <td>\em None
</table>

@param[in] self_message_iterator
    Self message iterator from which to create the event message.
@param[in] event_class
    Class of the \bt_ev of the message to create.
@param[in] packet
    Packet conceptually containing the event of the message to create.

@returns
    New event message reference, or \c NULL on memory error.

@bt_pre_not_null{self_message_iterator}
@pre
    The \bt_stream_cls of \bt_p{event_class} is also the stream class of
    \bt_p{packet}, that is,
    <code>bt_event_class_borrow_stream_class_const(event_class)</code>
    and
    <code>bt_stream_borrow_class_const(bt_packet_borrow_stream_const(packet))</code>
    have the same value.
@bt_pre_not_null{packet}
@pre
    <code>bt_stream_class_supports_packets(bt_stream_borrow_class_const(bt_packet_borrow_stream_const(packet)))</code>
    returns #BT_TRUE.
@pre
    <code>bt_stream_class_borrow_default_clock_class_const(bt_stream_borrow_class_const(bt_packet_borrow_stream_const(packet)))</code>
    returns \c NULL.
@pre
    The \ref api-tir-pkt-prop-ctx "context field" of \bt_p{packet}, if
    any, and all its contained \bt_p_field, recursively, are set.

@bt_post_success_frozen{event_class}
@bt_post_success_frozen{packet}
*/
extern
bt_message *bt_message_event_create_with_packet(
		bt_self_message_iterator *self_message_iterator,
		const bt_event_class *event_class,
		const bt_packet *packet);

/*!
@brief
    Creates an \bt_ev_msg, having an instance of the \bt_ev_cls
    \bt_p{event_class} and a default \bt_cs with the value
    \bt_p{clock_snapshot_value}, for the \bt_pkt \bt_p{packet} from
    the \bt_msg_iter \bt_p{self_message_iterator}.

@attention
    @parblock
    Only use this function if

    @code
    bt_stream_class_supports_packets(
        bt_stream_borrow_class_const(bt_packet_borrow_stream_const(packet)))
    @endcode

    returns #BT_TRUE and

    @code
    bt_stream_class_borrow_default_clock_class_const(
        bt_stream_borrow_class_const(bt_packet_borrow_stream_const(packet)))
    @endcode

    does \em not return \c NULL.

    Otherwise, use
    bt_message_event_create(),
    bt_message_event_create_with_default_clock_snapshot(), or
    bt_message_event_create_with_packet().
    @endparblock

On success, the returned event message has the following property
values:

<table>
  <tr>
    <th>Property
    <th>Value
  <tr>
    <td>\ref api-msg-ev-prop-ev "Event"
    <td>
      An instance (with \bt_p_field that are not set) of
      \bt_p{event_class}.
  <tr>
    <td>\ref api-msg-se-prop-cs "Default clock snapshot"
    <td>\bt_c_cs with the value \bt_p{clock_snapshot_value}.
</table>

@param[in] self_message_iterator
    Self message iterator from which to create the event message.
@param[in] event_class
    Class of the \bt_ev of the message to create.
@param[in] packet
    Packet conceptually containing the event of the message to create.
@param[in] clock_snapshot_value
    Value (clock cycles) of the default clock snapshot of
    \bt_p{message}.

@returns
    New event message reference, or \c NULL on memory error.

@bt_pre_not_null{self_message_iterator}
@pre
    The \bt_stream_cls of \bt_p{event_class} is also the stream class of
    \bt_p{packet}, that is,
    <code>bt_event_class_borrow_stream_class_const(event_class)</code>
    and
    <code>bt_stream_borrow_class_const(bt_packet_borrow_stream_const(packet))</code>
    have the same value.
@bt_pre_not_null{packet}
@pre
    <code>bt_stream_class_supports_packets(bt_stream_borrow_class_const(bt_packet_borrow_stream_const(packet)))</code>
    returns #BT_TRUE.
@pre
    <code>bt_stream_class_borrow_default_clock_class_const(bt_stream_borrow_class_const(bt_packet_borrow_stream_const(packet)))</code>
    does \em not return \c NULL.
@pre
    The \ref api-tir-pkt-prop-ctx "context field" of \bt_p{packet}, if
    any, and all its contained \bt_p_field, recursively, are set.

@bt_post_success_frozen{event_class}
@bt_post_success_frozen{stream}
*/
extern
bt_message *bt_message_event_create_with_packet_and_default_clock_snapshot(
		bt_self_message_iterator *self_message_iterator,
		const bt_event_class *event_class,
		const bt_packet *packet, uint64_t clock_snapshot_value);

/*!
@brief
    Borrows the \bt_ev of the \bt_ev_msg \bt_p{message}.

See the \ref api-msg-ev-prop-ev "event" property.

@param[in] message
    Event message from which to borrow the event.

@returns
    @parblock
    \em Borrowed reference of the event of \bt_p{message}.

    The returned pointer remains valid as long as \bt_p{message} exists.
    @endparblock

@bt_pre_not_null{message}
@bt_pre_is_ev_msg{message}

@sa bt_message_event_borrow_event_const() &mdash;
    \c const version of this function.
*/
extern bt_event *bt_message_event_borrow_event(
		bt_message *message);

/*!
@brief
    Borrows the \bt_ev of the \bt_ev_msg \bt_p{message}
    (\c const version).

See bt_message_event_borrow_event().
*/
extern const bt_event *bt_message_event_borrow_event_const(
		const bt_message *message);

/*!
@brief
    Borrows the default \bt_cs of the \bt_ev_msg \bt_p{message}.

See the \ref api-msg-ev-prop-cs "default clock snapshot" property.

@param[in] message
    Event message from which to borrow the default clock snapshot.

@returns
    Default clock snapshot of \bt_p{message}.

@bt_pre_not_null{message}
@bt_pre_is_ev_msg{message}
@pre
    The \bt_stream_cls of \bt_p{message} has a
    \ref api-tir-stream-cls-prop-def-clock-cls "default clock class".
*/
extern const bt_clock_snapshot *
bt_message_event_borrow_default_clock_snapshot_const(const bt_message *message);

/*!
@brief
    Borrows the default \bt_clock_cls of the \bt_stream_cls
    of the \bt_ev_msg \bt_p{message}.

See the stream class's
\ref api-tir-stream-cls-prop-def-clock-cls "default clock class"
property.

This is a helper which is equivalent to

@code
bt_stream_class_borrow_default_clock_class_const(
    bt_stream_borrow_class_const(
        bt_event_borrow_stream_const(
            bt_message_event_borrow_event_const(message))))
@endcode

@param[in] message
    Event message from which to borrow its stream's class's
    default clock class.

@returns
    \em Borrowed reference of the default clock class of
    the stream class of \bt_p{message}, or \c NULL if none.

@bt_pre_not_null{message}
@bt_pre_is_ev_msg{message}
*/
extern const bt_clock_class *
bt_message_event_borrow_stream_class_default_clock_class_const(
		const bt_message *message);

/*! @} */

/*!
@name Packet beginning message
@{
*/

/*!
@brief
    Creates a \bt_pb_msg for the \bt_pkt \bt_p{packet} from the
    \bt_msg_iter \bt_p{self_message_iterator}.

@attention
    @parblock
    Only use this function if

    @code
    bt_stream_class_packets_have_beginning_default_clock_snapshot(
        bt_stream_borrow_class_const(bt_packet_borrow_stream_const(packet)))
    @endcode

    returns #BT_FALSE.

    Otherwise, use
    bt_message_packet_beginning_create_with_default_clock_snapshot().
    @endparblock

On success, the returned packet beginning message has the following
property values:

<table>
  <tr>
    <th>Property
    <th>Value
  <tr>
    <td>\ref api-msg-pb-prop-pkt "Packet"
    <td>\bt_p{packet}
  <tr>
    <td>\ref api-msg-pb-prop-cs "Default clock snapshot"
    <td>\em None
</table>

@param[in] self_message_iterator
    Self message iterator from which to create the packet beginning
    message.
@param[in] packet
    Packet of which the message to create indicates the beginning.

@returns
    New packet beginning message reference, or \c NULL on memory error.

@bt_pre_not_null{self_message_iterator}
@bt_pre_not_null{packet}
@pre
    bt_stream_class_packets_have_beginning_default_clock_snapshot()
    returns #BT_FALSE for
    <code>bt_stream_borrow_class_const(bt_packet_borrow_stream_const(packet))</code>.
@pre
    The \ref api-tir-pkt-prop-ctx "context field" of \bt_p{packet}, if
    any, and all its contained \bt_p_field, recursively, are set.

@bt_post_success_frozen{packet}
*/
extern
bt_message *bt_message_packet_beginning_create(
		bt_self_message_iterator *self_message_iterator,
		const bt_packet *packet);

/*!
@brief
    Creates a \bt_pb_msg having a default \bt_cs with the value
    \bt_p{clock_snapshot_value} for the \bt_pkt \bt_p{packet} from the
    \bt_msg_iter \bt_p{self_message_iterator}.

@attention
    @parblock
    Only use this function if

    @code
    bt_stream_class_packets_have_beginning_default_clock_snapshot(
        bt_stream_borrow_class_const(bt_packet_borrow_stream_const(packet)))
    @endcode

    returns #BT_TRUE.

    Otherwise, use
    bt_message_packet_beginning_create().
    @endparblock

On success, the returned packet beginning message has the following
property values:

<table>
  <tr>
    <th>Property
    <th>Value
  <tr>
    <td>\ref api-msg-pb-prop-pkt "Packet"
    <td>\bt_p{packet}
  <tr>
    <td>\ref api-msg-pb-prop-cs "Default clock snapshot"
    <td>\bt_c_cs with the value \bt_p{clock_snapshot_value}.
</table>

@param[in] self_message_iterator
    Self message iterator from which to create the packet beginning
    message.
@param[in] packet
    Packet of which the message to create indicates the beginning.
@param[in] clock_snapshot_value
    Value (clock cycles) of the default clock snapshot of
    \bt_p{message}.

@returns
    New packet beginning message reference, or \c NULL on memory error.

@bt_pre_not_null{self_message_iterator}
@bt_pre_not_null{packet}
@pre
    bt_stream_class_packets_have_beginning_default_clock_snapshot()
    returns #BT_TRUE for
    <code>bt_stream_borrow_class_const(bt_packet_borrow_stream_const(packet))</code>.
@pre
    The \ref api-tir-pkt-prop-ctx "context field" of \bt_p{packet}, if
    any, and all its contained \bt_p_field, recursively, are set.

@bt_post_success_frozen{packet}
*/
extern
bt_message *bt_message_packet_beginning_create_with_default_clock_snapshot(
		bt_self_message_iterator *self_message_iterator,
		const bt_packet *packet, uint64_t clock_snapshot_value);

/*!
@brief
    Borrows the \bt_pkt of the \bt_pb_msg \bt_p{message}.

See the \ref api-msg-pb-prop-pkt "packet" property.

@param[in] message
    Packet beginning message from which to borrow the packet.

@returns
    @parblock
    \em Borrowed reference of the packet of \bt_p{message}.

    The returned pointer remains valid as long as \bt_p{message} exists.
    @endparblock

@bt_pre_not_null{message}
@bt_pre_is_pb_msg{message}

@sa bt_message_packet_beginning_borrow_packet_const() &mdash;
    \c const version of this function.
*/
extern bt_packet *bt_message_packet_beginning_borrow_packet(
		bt_message *message);

/*!
@brief
    Borrows the \bt_pkt of the \bt_pb_msg \bt_p{message}
    (\c const version).

See bt_message_packet_beginning_borrow_packet().
*/
extern const bt_packet *bt_message_packet_beginning_borrow_packet_const(
		const bt_message *message);

/*!
@brief
    Borrows the default \bt_cs of the \bt_pb_msg \bt_p{message}.

See the \ref api-msg-pb-prop-cs "default clock snapshot" property.

@param[in] message
    Packet beginning message from which to borrow the default clock
    snapshot.

@returns
    Default clock snapshot of \bt_p{message}.

@bt_pre_not_null{message}
@bt_pre_is_pb_msg{message}
@pre
    The packets of the \bt_stream_cls of \bt_p{message}
    \ref api-tir-stream-cls-prop-pkt-beg-cs "have a beginning default clock snapshot".
*/
extern const bt_clock_snapshot *
bt_message_packet_beginning_borrow_default_clock_snapshot_const(
		const bt_message *message);

/*!
@brief
    Borrows the default \bt_clock_cls of the \bt_stream_cls
    of the \bt_pb_msg \bt_p{message}.

See the stream class's
\ref api-tir-stream-cls-prop-def-clock-cls "default clock class"
property.

This is a helper which is equivalent to

@code
bt_stream_class_borrow_default_clock_class_const(
    bt_stream_borrow_class_const(
        bt_packet_borrow_stream_const(
            bt_message_packet_beginning_borrow_packet_const(message))))
@endcode

@param[in] message
    Packet beginning message from which to borrow its stream's class's
    default clock class.

@returns
    \em Borrowed reference of the default clock class of
    the stream class of \bt_p{message}, or \c NULL if none.

@bt_pre_not_null{message}
@bt_pre_is_pb_msg{message}
*/
extern const bt_clock_class *
bt_message_packet_beginning_borrow_stream_class_default_clock_class_const(
		const bt_message *message);

/*! @} */

/*!
@name Packet end message
@{
*/

/*!
@brief
    Creates a \bt_pe_msg for the \bt_pkt \bt_p{packet} from the
    \bt_msg_iter \bt_p{self_message_iterator}.

@attention
    @parblock
    Only use this function if

    @code
    bt_stream_class_packets_have_end_default_clock_snapshot(
        bt_stream_borrow_class_const(bt_packet_borrow_stream_const(packet)))
    @endcode

    returns #BT_FALSE.

    Otherwise, use
    bt_message_packet_end_create_with_default_clock_snapshot().
    @endparblock

On success, the returned packet end message has the following
property values:

<table>
  <tr>
    <th>Property
    <th>Value
  <tr>
    <td>\ref api-msg-pe-prop-pkt "Packet"
    <td>\bt_p{packet}
  <tr>
    <td>\ref api-msg-pe-prop-cs "Default clock snapshot"
    <td>\em None
</table>

@param[in] self_message_iterator
    Self message iterator from which to create the packet end message.
@param[in] packet
    Packet of which the message to create indicates the end.

@returns
    New packet end message reference, or \c NULL on memory error.

@bt_pre_not_null{self_message_iterator}
@bt_pre_not_null{packet}
@pre
    bt_stream_class_packets_have_end_default_clock_snapshot()
    returns #BT_FALSE for
    <code>bt_stream_borrow_class_const(bt_packet_borrow_stream_const(packet))</code>.
@pre
    The \ref api-tir-pkt-prop-ctx "context field" of \bt_p{packet}, if
    any, and all its contained \bt_p_field, recursively, are set.

@bt_post_success_frozen{packet}
*/
extern
bt_message *bt_message_packet_end_create(
		bt_self_message_iterator *self_message_iterator,
		const bt_packet *packet);

/*!
@brief
    Creates a \bt_pe_msg having a default \bt_cs with the value
    \bt_p{clock_snapshot_value} for the \bt_pkt \bt_p{packet} from the
    \bt_msg_iter \bt_p{self_message_iterator}.

@attention
    @parblock
    Only use this function if

    @code
    bt_stream_class_packets_have_end_default_clock_snapshot(
        bt_stream_borrow_class_const(bt_packet_borrow_stream_const(packet)))
    @endcode

    returns #BT_TRUE.

    Otherwise, use
    bt_message_packet_end_create().
    @endparblock

On success, the returned packet end message has the following
property values:

<table>
  <tr>
    <th>Property
    <th>Value
  <tr>
    <td>\ref api-msg-pe-prop-pkt "Packet"
    <td>\bt_p{packet}
  <tr>
    <td>\ref api-msg-pe-prop-cs "Default clock snapshot"
    <td>\bt_c_cs with the value \bt_p{clock_snapshot_value}.
</table>

@param[in] self_message_iterator
    Self message iterator from which to create the packet end
    message.
@param[in] packet
    Packet of which the message to create indicates the end.
@param[in] clock_snapshot_value
    Value (clock cycles) of the default clock snapshot of
    \bt_p{message}.

@returns
    New packet end message reference, or \c NULL on memory error.

@bt_pre_not_null{self_message_iterator}
@bt_pre_not_null{packet}
@pre
    bt_stream_class_packets_have_end_default_clock_snapshot()
    returns #BT_TRUE for
    <code>bt_stream_borrow_class_const(bt_packet_borrow_stream_const(packet))</code>.
@pre
    The \ref api-tir-pkt-prop-ctx "context field" of \bt_p{packet}, if
    any, and all its contained \bt_p_field, recursively, are set.

@bt_post_success_frozen{packet}
*/
extern
bt_message *bt_message_packet_end_create_with_default_clock_snapshot(
		bt_self_message_iterator *self_message_iterator,
		const bt_packet *packet, uint64_t clock_snapshot_value);

/*!
@brief
    Borrows the \bt_pkt of the \bt_pe_msg \bt_p{message}.

See the \ref api-msg-pe-prop-pkt "packet" property.

@param[in] message
    Packet end message from which to borrow the packet.

@returns
    @parblock
    \em Borrowed reference of the packet of \bt_p{message}.

    The returned pointer remains valid as long as \bt_p{message} exists.
    @endparblock

@bt_pre_not_null{message}
@bt_pre_is_pe_msg{message}

@sa bt_message_packet_end_borrow_packet_const() &mdash;
    \c const version of this function.
*/
extern bt_packet *bt_message_packet_end_borrow_packet(
		bt_message *message);

/*!
@brief
    Borrows the \bt_pkt of the \bt_pe_msg \bt_p{message}
    (\c const version).

See bt_message_packet_end_borrow_packet().
*/
extern const bt_packet *bt_message_packet_end_borrow_packet_const(
		const bt_message *message);

/*!
@brief
    Borrows the default \bt_cs of the \bt_pe_msg \bt_p{message}.

See the \ref api-msg-pe-prop-cs "default clock snapshot" property.

@param[in] message
    Packet end message from which to borrow the default clock
    snapshot.

@returns
    Default clock snapshot of \bt_p{message}.

@bt_pre_not_null{message}
@bt_pre_is_pe_msg{message}
@pre
    The packets of the \bt_stream_cls of \bt_p{message}
    \ref api-tir-stream-cls-prop-pkt-end-cs "have an end default clock snapshot".
*/
extern const bt_clock_snapshot *
bt_message_packet_end_borrow_default_clock_snapshot_const(
		const bt_message *message);

/*!
@brief
    Borrows the default \bt_clock_cls of the \bt_stream_cls
    of the \bt_pe_msg \bt_p{message}.

See the stream class's
\ref api-tir-stream-cls-prop-def-clock-cls "default clock class"
property.

This is a helper which is equivalent to

@code
bt_stream_class_borrow_default_clock_class_const(
    bt_stream_borrow_class_const(
        bt_packet_borrow_stream_const(
            bt_message_packet_end_borrow_packet_const(message))))
@endcode

@param[in] message
    Packet end message from which to borrow its stream's class's
    default clock class.

@returns
    \em Borrowed reference of the default clock class of
    the stream class of \bt_p{message}, or \c NULL if none.

@bt_pre_not_null{message}
@bt_pre_is_pe_msg{message}
*/
extern const bt_clock_class *
bt_message_packet_end_borrow_stream_class_default_clock_class_const(
		const bt_message *message);

/*! @} */

/*!
@name Discarded events message
@{
*/

/*!
@brief
    Creates a \bt_disc_ev_msg for the \bt_stream \bt_p{stream} from the
    \bt_msg_iter \bt_p{self_message_iterator}.

@attention
    @parblock
    Only use this function if

    @code
    bt_stream_class_discarded_events_have_default_clock_snapshots(
        bt_stream_borrow_class_const(stream))
    @endcode

    returns #BT_FALSE.

    Otherwise, use
    bt_message_discarded_events_create_with_default_clock_snapshots().
    @endparblock

On success, the returned discarded events message has the following
property values:

<table>
  <tr>
    <th>Property
    <th>Value
  <tr>
    <td>\ref api-msg-disc-ev-prop-stream "Stream"
    <td>\bt_p{stream}
  <tr>
    <td>\ref api-msg-disc-ev-prop-cs-beg "Beginning default clock snapshot"
    <td>\em None
  <tr>
    <td>\ref api-msg-disc-ev-prop-cs-end "End default clock snapshot"
    <td>\em None
  <tr>
    <td>\ref api-msg-disc-ev-prop-count "Discarded event count"
    <td>\em None
</table>

@param[in] self_message_iterator
    Self message iterator from which to create the discarded events
    message.
@param[in] stream
    Stream from which the events were discarded.

@returns
    New discarded events message reference, or \c NULL on memory error.

@bt_pre_not_null{self_message_iterator}
@bt_pre_not_null{stream}
@pre
    <code>bt_stream_class_discarded_events_have_default_clock_snapshots(bt_stream_borrow_class_const(stream))</code>
    returns #BT_FALSE.

@bt_post_success_frozen{stream}
*/
extern bt_message *bt_message_discarded_events_create(
		bt_self_message_iterator *self_message_iterator,
		const bt_stream *stream);

/*!
@brief
    Creates a \bt_disc_ev_msg having the beginning and end default
    \bt_p_cs with the values \bt_p{beginning_clock_snapshot_value} and
    \bt_p{end_clock_snapshot_value} for the \bt_stream \bt_p{stream}
    from the \bt_msg_iter \bt_p{self_message_iterator}.

@attention
    @parblock
    Only use this function if

    @code
    bt_stream_class_discarded_events_have_default_clock_snapshots(
        bt_stream_borrow_class_const(stream))
    @endcode

    returns #BT_TRUE.

    Otherwise, use
    bt_message_discarded_events_create().
    @endparblock

On success, the returned discarded events message has the following
property values:

<table>
  <tr>
    <th>Property
    <th>Value
  <tr>
    <td>\ref api-msg-disc-ev-prop-stream "Stream"
    <td>\bt_p{stream}
  <tr>
    <td>\ref api-msg-disc-ev-prop-cs-beg "Beginning default clock snapshot"
    <td>\bt_c_cs with the value \bt_p{beginning_clock_snapshot_value}.
  <tr>
    <td>\ref api-msg-disc-ev-prop-cs-end "End default clock snapshot"
    <td>\bt_c_cs with the value \bt_p{end_clock_snapshot_value}.
  <tr>
    <td>\ref api-msg-disc-ev-prop-count "Discarded event count"
    <td>\em None
</table>

@param[in] self_message_iterator
    Self message iterator from which to create the discarded events
    message.
@param[in] stream
    Stream from which the events were discarded.
@param[in] beginning_clock_snapshot_value
    Value (clock cycles) of the beginning default clock snapshot of
    \bt_p{message}.
@param[in] end_clock_snapshot_value
    Value (clock cycles) of the end default clock snapshot of
    \bt_p{message}.

@returns
    New discarded events message reference, or \c NULL on memory error.

@bt_pre_not_null{self_message_iterator}
@bt_pre_not_null{stream}
@pre
    <code>bt_stream_class_discarded_events_have_default_clock_snapshots(bt_stream_borrow_class_const(stream))</code>
    returns #BT_TRUE.

@bt_post_success_frozen{stream}
*/
extern bt_message *bt_message_discarded_events_create_with_default_clock_snapshots(
		bt_self_message_iterator *self_message_iterator,
		const bt_stream *stream,
		uint64_t beginning_clock_snapshot_value,
		uint64_t end_clock_snapshot_value);

/*!
@brief
    Borrows the \bt_stream of the \bt_disc_ev_msg \bt_p{message}.

See the \ref api-msg-disc-ev-prop-stream "stream" property.

@param[in] message
    Discarded events message from which to borrow the stream.

@returns
    @parblock
    \em Borrowed reference of the stream of \bt_p{message}.

    The returned pointer remains valid as long as \bt_p{message} exists.
    @endparblock

@bt_pre_not_null{message}
@bt_pre_is_disc_ev_msg{message}

@sa bt_message_discarded_events_borrow_stream_const() &mdash;
    \c const version of this function.
*/
extern bt_stream *bt_message_discarded_events_borrow_stream(
		bt_message *message);

/*!
@brief
    Borrows the \bt_stream of the \bt_disc_ev_msg \bt_p{message}
    (\c const version).

See bt_message_discarded_events_borrow_stream().
*/
extern const bt_stream *
bt_message_discarded_events_borrow_stream_const(const bt_message *message);

/*!
@brief
    Borrows the beginning default \bt_cs of the \bt_disc_ev_msg
    \bt_p{message}.

See the
\ref api-msg-disc-ev-prop-cs-beg "beginning default clock snapshot"
property.

@param[in] message
    Discarded events message from which to borrow the beginning default
    clock snapshot.

@returns
    Beginning default clock snapshot of \bt_p{message}.

@bt_pre_not_null{message}
@bt_pre_is_disc_ev_msg{message}
@pre
    The discarded packets messages of the \bt_stream_cls of
    \bt_p{message}
    \ref api-tir-stream-cls-prop-disc-pkt-cs "have default clock snapshots".
*/
extern const bt_clock_snapshot *
bt_message_discarded_events_borrow_beginning_default_clock_snapshot_const(
		const bt_message *message);

/*!
@brief
    Borrows the end default \bt_cs of the \bt_disc_ev_msg
    \bt_p{message}.

See the
\ref api-msg-disc-ev-prop-cs-end "end default clock snapshot"
property.

@param[in] message
    Discarded events message from which to borrow the end default clock
    snapshot.

@returns
    End default clock snapshot of \bt_p{message}.

@bt_pre_not_null{message}
@bt_pre_is_disc_ev_msg{message}
@pre
    The discarded packets messages of the \bt_stream_cls of
    \bt_p{message}
    \ref api-tir-stream-cls-prop-disc-pkt-cs "have default clock snapshots".
*/
extern const bt_clock_snapshot *
bt_message_discarded_events_borrow_end_default_clock_snapshot_const(
		const bt_message *message);

/*!
@brief
    Borrows the default \bt_clock_cls of the \bt_stream_cls
    of the \bt_disc_ev_msg \bt_p{message}.

See the stream class's
\ref api-tir-stream-cls-prop-def-clock-cls "default clock class"
property.

This is a helper which is equivalent to

@code
bt_stream_class_borrow_default_clock_class_const(
    bt_stream_borrow_class_const(
        bt_message_discarded_events_borrow_stream_const(message)))
@endcode

@param[in] message
    Discarded events message from which to borrow its stream's class's
    default clock class.

@returns
    \em Borrowed reference of the default clock class of
    the stream class of \bt_p{message}, or \c NULL if none.

@bt_pre_not_null{message}
@bt_pre_is_disc_ev_msg{message}
*/
extern const bt_clock_class *
bt_message_discarded_events_borrow_stream_class_default_clock_class_const(
		const bt_message *message);

/*!
@brief
    Sets the number of discarded events of the \bt_disc_ev_msg
    \bt_p{message} to \bt_p{count}.

See the \ref api-msg-disc-ev-prop-count "discarded event count"
property.

@param[in] message
    Discarded events message of which to set the number of discarded
    events to \bt_p{count}.
@param[in] count
    New number of discarded events of \bt_p{message}.

@bt_pre_not_null{message}
@bt_pre_hot{message}
@bt_pre_is_disc_ev_msg{message}

@sa bt_message_discarded_events_get_count() &mdash;
    Returns the number of discarded events of a discarded events
    message.
*/
extern void bt_message_discarded_events_set_count(bt_message *message,
		uint64_t count);

/*!
@brief
    Returns the number of discarded events of the \bt_disc_ev_msg
    \bt_p{message}.

See the \ref api-msg-disc-ev-prop-count "discarded event count"
property.

@param[in] message
    Discarded events message of which to get the number of discarded
    events.
@param[out] count
    <strong>If this function returns
    #BT_PROPERTY_AVAILABILITY_AVAILABLE</strong>, \bt_p{*count} is
    the number of discarded events of \bt_p{message}.

@retval #BT_PROPERTY_AVAILABILITY_AVAILABLE
    The number of discarded events of \bt_p{message} is available.
@retval #BT_PROPERTY_AVAILABILITY_NOT_AVAILABLE
    The number of discarded events of \bt_p{message} is not available.

@bt_pre_not_null{message}
@bt_pre_is_disc_ev_msg{message}
@bt_pre_not_null{count}

@sa bt_message_discarded_events_set_count() &mdash;
    Sets the number of discarded events of a discarded events message.
*/
extern bt_property_availability bt_message_discarded_events_get_count(
		const bt_message *message, uint64_t *count);

/*! @} */

/*!
@name Discarded packets message
@{
*/

/*!
@brief
    Creates a \bt_disc_pkt_msg for the \bt_stream \bt_p{stream} from the
    \bt_msg_iter \bt_p{self_message_iterator}.

@attention
    @parblock
    Only use this function if

    @code
    bt_stream_class_discarded_packets_have_default_clock_snapshots(
        bt_stream_borrow_class_const(stream))
    @endcode

    returns #BT_FALSE.

    Otherwise, use
    bt_message_discarded_packets_create_with_default_clock_snapshots().
    @endparblock

On success, the returned discarded packets message has the following
property values:

<table>
  <tr>
    <th>Property
    <th>Value
  <tr>
    <td>\ref api-msg-disc-pkt-prop-stream "Stream"
    <td>\bt_p{stream}
  <tr>
    <td>\ref api-msg-disc-pkt-prop-cs-beg "Beginning default clock snapshot"
    <td>\em None
  <tr>
    <td>\ref api-msg-disc-pkt-prop-cs-end "End default clock snapshot"
    <td>\em None
  <tr>
    <td>\ref api-msg-disc-pkt-prop-count "Discarded packet count"
    <td>\em None
</table>

@param[in] self_message_iterator
    Self message iterator from which to create the discarded packets
    message.
@param[in] stream
    Stream from which the packets were discarded.

@returns
    New discarded packets message reference, or \c NULL on memory error.

@bt_pre_not_null{self_message_iterator}
@bt_pre_not_null{stream}
@pre
    <code>bt_stream_class_discarded_packets_have_default_clock_snapshots(bt_stream_borrow_class_const(stream))</code>
    returns #BT_FALSE.

@bt_post_success_frozen{stream}
*/
extern bt_message *bt_message_discarded_packets_create(
		bt_self_message_iterator *self_message_iterator,
		const bt_stream *stream);

/*!
@brief
    Creates a \bt_disc_pkt_msg having the beginning and end default
    \bt_p_cs with the values \bt_p{beginning_clock_snapshot_value} and
    \bt_p{end_clock_snapshot_value} for the \bt_stream \bt_p{stream}
    from the \bt_msg_iter \bt_p{self_message_iterator}.

@attention
    @parblock
    Only use this function if

    @code
    bt_stream_class_discarded_packets_have_default_clock_snapshots(
        bt_stream_borrow_class_const(stream))
    @endcode

    returns #BT_TRUE.

    Otherwise, use
    bt_message_discarded_packets_create().
    @endparblock

On success, the returned discarded packets message has the following
property values:

<table>
  <tr>
    <th>Property
    <th>Value
  <tr>
    <td>\ref api-msg-disc-pkt-prop-stream "Stream"
    <td>\bt_p{stream}
  <tr>
    <td>\ref api-msg-disc-pkt-prop-cs-beg "Beginning default clock snapshot"
    <td>\bt_c_cs with the value \bt_p{beginning_clock_snapshot_value}.
  <tr>
    <td>\ref api-msg-disc-pkt-prop-cs-end "End default clock snapshot"
    <td>\bt_c_cs with the value \bt_p{end_clock_snapshot_value}.
  <tr>
    <td>\ref api-msg-disc-pkt-prop-count "Discarded packet count"
    <td>\em None
</table>

@param[in] self_message_iterator
    Self message iterator from which to create the discarded packets
    message.
@param[in] stream
    Stream from which the packets were discarded.
@param[in] beginning_clock_snapshot_value
    Value (clock cycles) of the beginning default clock snapshot of
    \bt_p{message}.
@param[in] end_clock_snapshot_value
    Value (clock cycles) of the end default clock snapshot of
    \bt_p{message}.

@returns
    New discarded packets message reference, or \c NULL on memory error.

@bt_pre_not_null{self_message_iterator}
@bt_pre_not_null{stream}
@pre
    <code>bt_stream_class_discarded_packets_have_default_clock_snapshots(bt_stream_borrow_class_const(stream))</code>
    returns #BT_TRUE.

@bt_post_success_frozen{stream}
*/
extern bt_message *bt_message_discarded_packets_create_with_default_clock_snapshots(
		bt_self_message_iterator *self_message_iterator,
		const bt_stream *stream, uint64_t beginning_clock_snapshot_value,
		uint64_t end_clock_snapshot_value);

/*!
@brief
    Borrows the \bt_stream of the \bt_disc_pkt_msg \bt_p{message}.

See the \ref api-msg-disc-ev-prop-stream "stream" property.

@param[in] message
    Discarded packets message from which to borrow the stream.

@returns
    @parblock
    \em Borrowed reference of the stream of \bt_p{message}.

    The returned pointer remains valid as long as \bt_p{message} exists.
    @endparblock

@bt_pre_not_null{message}
@bt_pre_is_disc_pkt_msg{message}

@sa bt_message_discarded_packets_borrow_stream_const() &mdash;
    \c const version of this function.
*/
extern bt_stream *bt_message_discarded_packets_borrow_stream(
		bt_message *message);

/*!
@brief
    Borrows the \bt_stream of the \bt_disc_pkt_msg \bt_p{message}
    (\c const version).

See bt_message_discarded_packets_borrow_stream().
*/
extern const bt_stream *
bt_message_discarded_packets_borrow_stream_const(const bt_message *message);

/*!
@brief
    Borrows the beginning default \bt_cs of the \bt_disc_pkt_msg
    \bt_p{message}.

See the
\ref api-msg-disc-pkt-prop-cs-beg "beginning default clock snapshot"
property.

@param[in] message
    Discarded packets message from which to borrow the beginning default
    clock snapshot.

@returns
    Beginning default clock snapshot of \bt_p{message}.

@bt_pre_not_null{message}
@bt_pre_is_disc_pkt_msg{message}
@pre
    The discarded packets messages of the \bt_stream_cls of
    \bt_p{message}
    \ref api-tir-stream-cls-prop-disc-pkt-cs "have default clock snapshots".
*/
extern const bt_clock_snapshot *
bt_message_discarded_packets_borrow_beginning_default_clock_snapshot_const(
		const bt_message *message);

/*!
@brief
    Borrows the end default \bt_cs of the \bt_disc_pkt_msg
    \bt_p{message}.

See the
\ref api-msg-disc-pkt-prop-cs-end "end default clock snapshot"
property.

@param[in] message
    Discarded packets message from which to borrow the end default clock
    snapshot.

@returns
    End default clock snapshot of \bt_p{message}.

@bt_pre_not_null{message}
@bt_pre_is_disc_pkt_msg{message}
@pre
    The discarded packets messages of the \bt_stream_cls of
    \bt_p{message}
    \ref api-tir-stream-cls-prop-disc-pkt-cs "have default clock snapshots".
*/
extern const bt_clock_snapshot *
bt_message_discarded_packets_borrow_end_default_clock_snapshot_const(
		const bt_message *message);

/*!
@brief
    Borrows the default \bt_clock_cls of the \bt_stream_cls
    of the \bt_disc_pkt_msg \bt_p{message}.

See the stream class's
\ref api-tir-stream-cls-prop-def-clock-cls "default clock class"
property.

This is a helper which is equivalent to

@code
bt_stream_class_borrow_default_clock_class_const(
    bt_stream_borrow_class_const(
        bt_message_discarded_packets_borrow_stream_const(message)))
@endcode

@param[in] message
    Discarded packets message from which to borrow its stream's class's
    default clock class.

@returns
    \em Borrowed reference of the default clock class of
    the stream class of \bt_p{message}, or \c NULL if none.

@bt_pre_not_null{message}
@bt_pre_is_disc_pkt_msg{message}
*/
extern const bt_clock_class *
bt_message_discarded_packets_borrow_stream_class_default_clock_class_const(
		const bt_message *message);

/*!
@brief
    Sets the number of discarded packets of the \bt_disc_pkt_msg
    \bt_p{message} to \bt_p{count}.

See the \ref api-msg-disc-ev-prop-count "discarded packet count"
property.

@param[in] message
    Discarded packets message of which to set the number of discarded
    packets to \bt_p{count}.
@param[in] count
    New number of discarded packets of \bt_p{message}.

@bt_pre_not_null{message}
@bt_pre_hot{message}
@bt_pre_is_disc_pkt_msg{message}

@sa bt_message_discarded_packets_get_count() &mdash;
    Returns the number of discarded packets of a discarded packets
    message.
*/
extern void bt_message_discarded_packets_set_count(bt_message *message,
		uint64_t count);

/*!
@brief
    Returns the number of discarded packets of the \bt_disc_pkt_msg
    \bt_p{message}.

See the \ref api-msg-disc-ev-prop-count "discarded packet count"
property.

@param[in] message
    Discarded packets message of which to get the number of discarded
    packets.
@param[out] count
    <strong>If this function returns
    #BT_PROPERTY_AVAILABILITY_AVAILABLE</strong>, \bt_p{*count} is
    the number of discarded packets of \bt_p{message}.

@retval #BT_PROPERTY_AVAILABILITY_AVAILABLE
    The number of discarded packets of \bt_p{message} is available.
@retval #BT_PROPERTY_AVAILABILITY_NOT_AVAILABLE
    The number of discarded packets of \bt_p{message} is not available.

@bt_pre_not_null{message}
@bt_pre_is_disc_pkt_msg{message}
@bt_pre_not_null{count}

@sa bt_message_discarded_packets_set_count() &mdash;
    Sets the number of discarded packets of a discarded packets message.
*/
extern bt_property_availability bt_message_discarded_packets_get_count(
		const bt_message *message, uint64_t *count);

/*! @} */

/*!
@name Message iterator inactivity message
@{
*/

/*!
@brief
    Creates a \bt_inac_msg having a \bt_cs of a fictitious instance of
    the \bt_clock_cls \bt_p{clock_class} with the value
    \bt_p{clock_snapshot_value} from the \bt_msg_iter
    \bt_p{self_message_iterator}.

On success, the returned message iterator inactivity message has the
following property values:

<table>
  <tr>
    <th>Property
    <th>Value
  <tr>
    <td>\ref api-msg-inac-prop-cs "Clock snapshot"
    <td>
      \bt_c_cs (snapshot of a fictitious instance of \bt_p{clock_class})
      with the value \bt_p{clock_snapshot_value}.
</table>

@param[in] self_message_iterator
    Self message iterator from which to create the message iterator
    inactivity message.
@param[in] clock_class
    Class of the fictitious instance of which
    \bt_p{clock_snapshot_value} is the value of its snapshot.
@param[in] clock_snapshot_value
    Value (clock cycles) of the clock snapshot of \bt_p{message}.

@returns
    New message iterator inactivity message reference, or \c NULL on
    memory error.

@bt_pre_not_null{self_message_iterator}
@bt_pre_not_null{clock_class}

@bt_post_success_frozen{clock_class}
*/
extern
bt_message *bt_message_message_iterator_inactivity_create(
		bt_self_message_iterator *self_message_iterator,
		const bt_clock_class *clock_class,
		uint64_t clock_snapshot_value);

/*!
@brief
    Borrows the \bt_cs of the \bt_inac_msg \bt_p{message}.

See the \ref api-msg-inac-prop-cs "clock snapshot" property.

@param[in] message
    Message iterator inactivity message from which to borrow the clock
    snapshot.

@returns
    Clock snapshot of \bt_p{message}.

@bt_pre_not_null{message}
@bt_pre_is_inac_msg{message}
*/
extern const bt_clock_snapshot *
bt_message_message_iterator_inactivity_borrow_clock_snapshot_const(
		const bt_message *message);

/*! @} */

/*!
@name Message reference count
@{
*/

/*!
@brief
    Increments the \ref api-fund-shared-object "reference count" of
    the message \bt_p{message}.

@param[in] message
    @parblock
    Message of which to increment the reference count.

    Can be \c NULL.
    @endparblock

@sa bt_message_put_ref() &mdash;
    Decrements the reference count of a message.
*/
extern void bt_message_get_ref(const bt_message *message);

/*!
@brief
    Decrements the \ref api-fund-shared-object "reference count" of
    the message \bt_p{message}.

@param[in] message
    @parblock
    Message of which to decrement the reference count.

    Can be \c NULL.
    @endparblock

@sa bt_message_get_ref() &mdash;
    Increments the reference count of a message.
*/
extern void bt_message_put_ref(const bt_message *message);

/*!
@brief
    Decrements the reference count of the message \bt_p{_message}, and
    then sets \bt_p{_message} to \c NULL.

@param _message
    @parblock
    Message of which to decrement the reference count.

    Can contain \c NULL.
    @endparblock

@bt_pre_assign_expr{_message}
*/
#define BT_MESSAGE_PUT_REF_AND_RESET(_message)		\
	do {						\
		bt_message_put_ref(_message);		\
		(_message) = NULL;			\
	} while (0)

/*!
@brief
    Decrements the reference count of the message \bt_p{_dst}, sets
    \bt_p{_dst} to \bt_p{_src}, and then sets \bt_p{_src} to \c NULL.

This macro effectively moves a message reference from the expression
\bt_p{_src} to the expression \bt_p{_dst}, putting the existing
\bt_p{_dst} reference.

@param _dst
    @parblock
    Destination expression.

    Can contain \c NULL.
    @endparblock
@param _src
    @parblock
    Source expression.

    Can contain \c NULL.
    @endparblock

@bt_pre_assign_expr{_dst}
@bt_pre_assign_expr{_src}
*/
#define BT_MESSAGE_MOVE_REF(_dst, _src)		\
	do {					\
		bt_message_put_ref(_dst);	\
		(_dst) = (_src);		\
		(_src) = NULL;			\
	} while (0)

/*! @} */


/*!
@name Message Interchange Protocol version
@{
*/

/*!
@brief
    Status codes for bt_get_greatest_operative_mip_version().
*/
typedef enum bt_get_greatest_operative_mip_version_status {
	/*!
	@brief
	    Success.
	*/
	BT_GET_GREATEST_OPERATIVE_MIP_VERSION_STATUS_OK			= __BT_FUNC_STATUS_OK,

	/*!
	@brief
	    No match found.
	*/
	BT_GET_GREATEST_OPERATIVE_MIP_VERSION_STATUS_NO_MATCH		= __BT_FUNC_STATUS_NO_MATCH,

	/*!
	@brief
	    Out of memory.
	*/
	BT_GET_GREATEST_OPERATIVE_MIP_VERSION_STATUS_MEMORY_ERROR	= __BT_FUNC_STATUS_MEMORY_ERROR,

	/*!
	@brief
	    Other error.
	*/
	BT_GET_GREATEST_OPERATIVE_MIP_VERSION_STATUS_ERROR		= __BT_FUNC_STATUS_ERROR,
} bt_get_greatest_operative_mip_version_status;

/*!
@brief
    Computes the greatest \bt_mip version which
    you can use to create a trace processing \bt_graph to which you
    intend to \ref api-graph-lc-add "add components" described by the
    component descriptors \bt_p{component_descriptors}, and sets
    \bt_p{*mip_version} to the result.

This function calls the
\link api-comp-cls-dev-meth-mip "get supported MIP versions"\endlink
method for each component descriptor in \bt_p{component_descriptors},
and then returns the greatest common (operative) MIP version, if any.
The "get supported MIP versions" method receives \bt_p{logging_level} as
its \bt_p{logging_level} parameter.

If this function does not find an operative MIP version for the
component descriptors of \bt_p{component_descriptors}, it returns
#BT_GET_GREATEST_OPERATIVE_MIP_VERSION_STATUS_NO_MATCH.

@note
    As of \bt_name_version_min_maj, because bt_get_maximal_mip_version()
    returns 0, this function always sets \bt_p{*mip_version} to
    0 on success.

@param[in] component_descriptors
    Component descriptors for which to get the supported MIP versions
    to compute the greatest operative MIP version.
@param[in] logging_level
    Logging level to use when calling the "get supported MIP versions"
    method for each component descriptor in
    \bt_p{component_descriptors}.
@param[out] mip_version
    <strong>On success</strong>, \bt_p{*mip_version} is the greatest
    operative MIP version of all the component descriptors in
    \bt_p{component_descriptors}.

@retval #BT_GET_GREATEST_OPERATIVE_MIP_VERSION_STATUS_OK
    Success.
@retval #BT_GET_GREATEST_OPERATIVE_MIP_VERSION_STATUS_NO_MATCH
    No operative MIP version exists for the component descriptors of
    \bt_p{component_descriptors}.
@retval #BT_GET_GREATEST_OPERATIVE_MIP_VERSION_STATUS_MEMORY_ERROR
    Out of memory.
@retval #BT_GET_GREATEST_OPERATIVE_MIP_VERSION_STATUS_ERROR
    Other error.

@bt_pre_not_null{component_descriptors}
@pre
    \bt_p{component_descriptors} contains one or more component
    descriptors.
@bt_pre_not_null{mip_version}
*/
extern bt_get_greatest_operative_mip_version_status
bt_get_greatest_operative_mip_version(
		const bt_component_descriptor_set *component_descriptors,
		bt_logging_level logging_level, uint64_t *mip_version);

/*!
@brief
    Returns the maximal available \bt_mip version as of
    \bt_name_version_min_maj.

As of \bt_name_version_min_maj, this function returns
\bt_max_mip_version.

@returns
    Maximal available MIP version (\bt_max_mip_version).
*/
extern uint64_t bt_get_maximal_mip_version(void);

/*! @} */

/*! @} */

#ifdef __cplusplus
}
#endif

#endif /* BABELTRACE2_GRAPH_MESSAGE_H */
