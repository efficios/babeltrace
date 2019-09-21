#ifndef BABELTRACE2_TRACE_IR_STREAM_CLASS_H
#define BABELTRACE2_TRACE_IR_STREAM_CLASS_H

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

#include <stdint.h>

#include <babeltrace2/types.h>

#ifdef __cplusplus
extern "C" {
#endif

/*!
@defgroup api-tir-stream-cls Stream class
@ingroup api-tir

@brief
    Class of \bt_p_stream.

A <strong><em>stream class</em></strong> is the class of \bt_p_stream:

@image html trace-structure.png

In the illustration above, notice that:

- A \bt_stream is a conceptual \ref api-msg-seq "sequence of messages".

  The sequence always starts with a \bt_sb_msg and ends with a
  \bt_se_msg.

- A stream is an instance of a stream class.

A stream class is a \ref api-tir "trace IR" metadata object.

A stream class is a \ref api-fund-shared-object "shared object": get a
new reference with bt_stream_class_get_ref() and put an existing
reference with bt_stream_class_put_ref().

Some library functions \ref api-fund-freezing "freeze" stream classes on
success. The documentation of those functions indicate this
postcondition. You can still create and add an \bt_p_ev_cls to a frozen
stream class with bt_event_class_create() or
bt_event_class_create_with_id().

The type of a stream class is #bt_stream_class.

A \bt_trace_cls contains stream classes. All the stream classes of a
given trace class have unique
\ref api-tir-stream-cls-prop-id "numeric IDs". Borrow the trace class
which contains a stream class with bt_stream_class_borrow_trace_class()
or bt_stream_class_borrow_trace_class_const().

A stream class contains \bt_p_ev_cls. All the event classes of a given
stream class have unique \ref api-tir-ev-cls-prop-id "numeric IDs". Get
the number of event classes in a stream class with
bt_stream_class_get_event_class_count(). Borrow a specific event class
from a stream class with bt_stream_class_borrow_event_class_by_index(),
bt_stream_class_borrow_event_class_by_index_const(),
bt_stream_class_borrow_event_class_by_id(), and
bt_stream_class_borrow_event_class_by_id_const().

A stream class controls what its instances (\bt_p_stream) support:

<dl>
  <dt>Default clock</dt>
  <dd>
    By default, a stream class's streams do not have default clocks.

    Set the default \bt_clock_cls of a stream class with
    bt_stream_class_set_default_clock_class(). This makes all its
    streams have their own default clock.
  </dd>

  <dt>\anchor api-tir-stream-cls-pkt-support Packets</dt>
  <dd>
    By default, a stream class's streams do not support \bt_p_pkt.

    In other words, you cannot create a packet for such a stream,
    therefore you cannot create \bt_p_pb_msg and \bt_p_pe_msg for this
    stream either.

    Enable packet support for a stream class's streams with
    bt_stream_class_set_supports_packets().

    bt_stream_class_set_supports_packets() also configures whether or
    not the packets of the stream class's instances have beginning
    and/or end default \bt_p_cs.
  </dd>

  <dt>Discarded events</dt>
  <dd>
    By default, a stream class's streams do not support discarded
    events.

    In other words, you cannot create \bt_p_disc_ev_msg for such a
    stream.

    Enable discarded events support for a stream class's streams with
    bt_stream_class_set_supports_discarded_events().

    bt_stream_class_set_supports_discarded_events() also configures
    whether or not the discarded events messages of the stream class's
    instances have beginning and end default \bt_p_cs to indicate the
    discarded events time range.
  </dd>

  <dt>Discarded packets</dt>
  <dd>
    By default, a stream class's streams do not support discarded
    packets.

    In other words, you cannot create \bt_p_disc_pkt_msg for such a
    stream.

    Enable discarded packets support for a stream class's streams with
    bt_stream_class_set_supports_discarded_packets(). This also implies
    that you must enable packet support with
    bt_stream_class_set_supports_packets().

    bt_stream_class_set_supports_discarded_packets() also configures
    whether or not the discarded packets messages of the stream class's
    instances have beginning and end \bt_p_cs to indicate the
    discarded packets time range.
  </dd>
</dl>

Set whether or not the \bt_p_ev_cls and \bt_p_stream you create for a
stream class get automatic numeric IDs with
bt_stream_class_set_assigns_automatic_event_class_id() and
bt_stream_class_set_assigns_automatic_stream_id().

To create a default stream class:

<dl>
  <dt>
    If bt_trace_class_assigns_automatic_stream_class_id() returns
    #BT_TRUE (the default) for the trace class to use
  </dt>
  <dd>Use bt_stream_class_create().</dd>

  <dt>
    If bt_trace_class_assigns_automatic_stream_class_id() returns
    #BT_FALSE for the trace class to use
  </dt>
  <dd>Use bt_stream_class_create_with_id().</dd>
</dl>

<h1>Properties</h1>

A stream class has the following properties:

<dl>
  <dt>\anchor api-tir-stream-cls-prop-id Numeric ID</dt>
  <dd>
    Numeric ID, unique amongst the numeric IDs of the stream class's
    \bt_trace_cls's stream classes.

    Depending on whether or not the stream class's trace class
    automatically assigns \bt_ev_cls IDs
    (see bt_trace_class_assigns_automatic_stream_class_id()),
    set the stream class's numeric ID on creation with
    bt_stream_class_create() or bt_stream_class_create_with_id().

    You cannot change the numeric ID once the stream class is created.

    Get a stream class's numeric ID with bt_stream_class_get_id().
  </dd>

  <dt>\anchor api-tir-stream-cls-prop-name \bt_dt_opt Name</dt>
  <dd>
    Name of the stream class.

    Use bt_stream_class_set_name() and bt_stream_class_get_name().
  </dd>

  <dt>
    \anchor api-tir-stream-cls-prop-def-clock-cls
    \bt_dt_opt Default clock class
  </dt>
  <dd>
    Default \bt_clock_cls of the stream class.

    As of \bt_name_version_min_maj, a stream class either has a default
    clock class or none: it cannot have more than one clock class.

    When a stream class has a default clock class, then all its
    instances (\bt_p_stream) have a default clock which is an instance
    of the stream class's default clock class.

    Use bt_stream_class_set_default_clock_class(),
    bt_stream_class_borrow_default_clock_class(), and
    bt_stream_class_borrow_default_clock_class_const().
  </dd>

  <dt>
    \anchor api-tir-stream-cls-prop-pc-fc
    \bt_dt_opt Packet context field class
  </dt>
  <dd>
    \bt_c_pkt context \bt_fc of the stream class.

    This property is only relevant if the stream class
    \ref api-tir-stream-cls-prop-supports-pkt "supports packets".

    The context of a \bt_pkt contains data which is common to all the
    packet's \bt_p_ev.

    Use bt_stream_class_set_packet_context_field_class()
    bt_stream_class_borrow_packet_context_field_class(),
    and bt_stream_class_borrow_packet_context_field_class_const().
  </dd>

  <dt>
    \anchor api-tir-stream-cls-prop-ecc-fc
    \bt_dt_opt Event common context field class
  </dt>
  <dd>
    \bt_c_ev common context \bt_fc of the stream class.

    The common context of an \bt_ev contains contextual data of which
    the layout is common to all the stream class's \bt_p_ev_cls.

    Use bt_stream_class_set_event_common_context_field_class()
    bt_stream_class_borrow_event_common_context_field_class(),
    and bt_stream_class_borrow_event_common_context_field_class_const().
  </dd>

  <dt>
    \anchor api-tir-stream-cls-prop-auto-ec-id
    Assigns automatic event class IDs?
  </dt>
  <dd>
    Whether or not the \bt_p_ev_cls you create and add to the stream
    class get \ref api-tir-ev-cls-prop-id "numeric IDs" automatically.

    Depending on the value of this property, to create an event class
    and add it to the stream class:

    <dl>
      <dt>#BT_TRUE</dt>
      <dd>
        Use bt_event_class_create().
      </dd>

      <dt>#BT_FALSE</dt>
      <dd>
        Use bt_event_class_create_with_id().
      </dd>
    </dl>

    Use bt_stream_class_set_assigns_automatic_event_class_id()
    and bt_stream_class_assigns_automatic_event_class_id().
  </dd>

  <dt>
    \anchor api-tir-stream-cls-prop-auto-stream-id
    Assigns automatic stream IDs?
  </dt>
  <dd>
    Whether or not the streams you create from the stream class
    get \ref api-tir-stream-prop-id "numeric IDs" automatically.

    Depending on the value of this property, to create a stream
    from the stream class:

    <dl>
      <dt>#BT_TRUE</dt>
      <dd>
        Use bt_stream_create().
      </dd>

      <dt>#BT_FALSE</dt>
      <dd>
        Use bt_stream_create_with_id().
      </dd>
    </dl>

    Use bt_stream_class_set_assigns_automatic_stream_id()
    and bt_stream_class_assigns_automatic_stream_id().
  </dd>

  <dt>
    \anchor api-tir-stream-cls-prop-supports-pkt
    Supports packets?
  </dt>
  <dd>
    Whether or not the streams you create from the stream class
    have \bt_p_pkt.

    If a stream has packets, then all the stream's \bt_p_ev are
    conceptually contained within packets, which means you must
    create \bt_p_ev_msg for such streams with
    bt_message_event_create_with_packet() or
    bt_message_event_create_with_packet_and_default_clock_snapshot()
    instead of bt_message_event_create() or
    bt_message_event_create_with_default_clock_snapshot().

    It also means you must create \bt_p_pb_msg and \bt_p_pe_msg to
    indicate where packets begin and end within the stream's
    \ref api-msg-seq "message sequence".

    Use bt_stream_class_set_supports_packets() and
    bt_stream_class_supports_packets().
  </dd>

  <dt>
    \anchor api-tir-stream-cls-prop-pkt-beg-cs
    Packets have a beginning default clock snapshot?
  </dt>
  <dd>
    Whether or not the \bt_p_pkt of the streams you create from the
    stream class have beginning default \bt_p_cs.

    This property is only relevant if the stream class
    \ref api-tir-stream-cls-prop-supports-pkt "supports packets" and
    has a
    \ref api-tir-stream-cls-prop-def-clock-cls "default clock class".

    If the stream packets have a beginning default clock snapshot, then
    you must create \bt_p_pb_msg with
    bt_message_packet_beginning_create_with_default_clock_snapshot()
    instead of bt_message_packet_beginning_create().

    Use bt_stream_class_set_supports_packets() and
    bt_stream_class_packets_have_beginning_default_clock_snapshot().
  </dd>

  <dt>
    \anchor api-tir-stream-cls-prop-pkt-end-cs
    Packets have an end default clock snapshot?
  </dt>
  <dd>
    Whether or not the \bt_p_pkt of the streams you create from the
    stream class have end default \bt_p_cs.

    This property is only relevant if the stream class
    \ref api-tir-stream-cls-prop-supports-pkt "supports packets" and
    has a
    \ref api-tir-stream-cls-prop-def-clock-cls "default clock class".

    If the stream packets have an end default clock snapshot, then you
    must create \bt_p_pe_msg with
    bt_message_packet_end_create_with_default_clock_snapshot() instead
    of bt_message_packet_end_create().

    Use bt_stream_class_set_supports_packets() and
    bt_stream_class_packets_have_end_default_clock_snapshot().
  </dd>

  <dt>
    \anchor api-tir-stream-cls-prop-supports-disc-ev
    Supports discarded events?
  </dt>
  <dd>
    Whether or not the streams you create from the stream class can have
    discarded events.

    If the stream class supports discarded events, then you can create
    \bt_p_disc_ev_msg for this stream.

    Use bt_stream_class_set_supports_discarded_events()
    and bt_stream_class_supports_discarded_events().
  </dd>

  <dt>
    \anchor api-tir-stream-cls-prop-disc-ev-cs
    Discarded events have default clock snapshots?
  </dt>
  <dd>
    Whether or not the stream's \bt_p_disc_ev_msg have
    default beginning and end \bt_p_cs to indicate the discarded events
    time range.

    This property is only relevant if the stream class
    \ref api-tir-stream-cls-prop-supports-disc-ev "supports discarded events"
    and has a
    \ref api-tir-stream-cls-prop-def-clock-cls "default clock class".

    If the stream's discarded events messages have beginning and end
    default clock snapshots, then you must create them with
    bt_message_discarded_events_create_with_default_clock_snapshots()
    instead of bt_message_discarded_events_create().

    Use bt_stream_class_set_supports_discarded_events()
    and bt_stream_class_discarded_events_have_default_clock_snapshots().
  </dd>

  <dt>
    \anchor api-tir-stream-cls-prop-supports-disc-pkt
    Supports discarded packets?
  </dt>
  <dd>
    Whether or not the streams you create from the stream class can have
    discarded packets.

    This property is only relevant if the stream class
    \ref api-tir-stream-cls-prop-supports-pkt "supports packets".

    If the stream class supports discarded packets, then you can create
    \bt_p_disc_pkt_msg for this stream.

    Use bt_stream_class_set_supports_discarded_packets()
    and bt_stream_class_supports_discarded_packets().
  </dd>

  <dt>
    \anchor api-tir-stream-cls-prop-disc-pkt-cs
    Discarded packets have default clock snapshots?
  </dt>
  <dd>
    Whether or not the stream's \bt_p_disc_pkt_msg have
    default beginning and end \bt_p_cs to indicate the discarded
    packets time range.

    This property is only relevant if the stream class
    \ref api-tir-stream-cls-prop-supports-disc-pkt "supports discarded packets"
    and has a
    \ref api-tir-stream-cls-prop-def-clock-cls "default clock class".

    If the stream's discarded packets messages have default clock
    snapshots, then you must create them with
    bt_message_discarded_packets_create_with_default_clock_snapshots()
    instead of bt_message_discarded_packets_create().

    Use bt_stream_class_set_supports_discarded_packets()
    and bt_stream_class_discarded_packets_have_default_clock_snapshots().
  </dd>

  <dt>
    \anchor api-tir-stream-cls-prop-user-attrs
    \bt_dt_opt User attributes
  </dt>
  <dd>
    User attributes of the stream class.

    User attributes are custom attributes attached to a stream class.

    Use bt_stream_class_set_user_attributes(),
    bt_stream_class_borrow_user_attributes(), and
    bt_stream_class_borrow_user_attributes_const().
  </dd>
</dl>
*/

/*! @{ */

/*!
@name Type
@{

@typedef struct bt_stream_class bt_stream_class;

@brief
    Stream class.

@}
*/

/*!
@name Creation
@{
*/

/*!
@brief
    Creates a default stream class and adds it to the \bt_trace_cls
    \bt_p{trace_class}.

@attention
    @parblock
    Only use this function if

    @code
    bt_trace_class_assigns_automatic_stream_class_id(trace_class)
    @endcode

    returns #BT_TRUE.

    Otherwise, use bt_stream_class_create_with_id().
    @endparblock

On success, the returned stream class has the following property values:

<table>
  <tr>
    <th>Property
    <th>Value
  <tr>
    <td>\ref api-tir-stream-cls-prop-id "Numeric ID"
    <td>Automatically assigned by \bt_p{trace_class}
  <tr>
    <td>\ref api-tir-stream-cls-prop-name "Name"
    <td>\em None
  <tr>
    <td>\ref api-tir-stream-cls-prop-def-clock-cls "Default clock class"
    <td>\em None
  <tr>
    <td>\ref api-tir-stream-cls-prop-pc-fc "Packet context field class"
    <td>\em None
  <tr>
    <td>\ref api-tir-stream-cls-prop-ecc-fc "Event common context field class"
    <td>\em None
  <tr>
    <td>\ref api-tir-stream-cls-prop-auto-ec-id "Assigns automatic event class IDs?"
    <td>Yes
  <tr>
    <td>\ref api-tir-stream-cls-prop-auto-stream-id "Assigns automatic stream IDs?"
    <td>Yes
  <tr>
    <td>\ref api-tir-stream-cls-prop-supports-pkt "Supports packets?"
    <td>No
  <tr>
    <td>\ref api-tir-stream-cls-prop-pkt-beg-cs "Packets have a beginning default clock snapshot?"
    <td>No
  <tr>
    <td>\ref api-tir-stream-cls-prop-pkt-end-cs "Packets have an end default clock snapshot?"
    <td>No
  <tr>
    <td>\ref api-tir-stream-cls-prop-supports-disc-ev "Supports discarded events?"
    <td>No
  <tr>
    <td>\ref api-tir-stream-cls-prop-disc-ev-cs "Discarded events have default clock snapshots?"
    <td>No
  <tr>
    <td>\ref api-tir-stream-cls-prop-supports-disc-pkt "Supports discarded packets?"
    <td>No
  <tr>
    <td>\ref api-tir-stream-cls-prop-disc-pkt-cs "Discarded packets have default clock snapshots?"
    <td>No
  <tr>
    <td>\ref api-tir-stream-cls-prop-user-attrs "User attributes"
    <td>Empty \bt_map_val
</table>

@param[in] trace_class
    Trace class to add the created stream class to.

@returns
    New stream class reference, or \c NULL on memory error.

@bt_pre_not_null{trace_class}
@pre
    <code>bt_trace_class_assigns_automatic_stream_class_id(trace_class)</code>
    returns #BT_TRUE.

@bt_post_success_frozen{trace_class}

@sa bt_stream_class_create_with_id() &mdash;
    Creates a stream class with a specific numeric ID and adds it to a
    trace class.
*/
extern bt_stream_class *bt_stream_class_create(
		bt_trace_class *trace_class);

/*!
@brief
    Creates a default stream class with the numeric ID \bt_p{id} and adds
    it to the \bt_trace_cls \bt_p{trace_class}.

@attention
    @parblock
    Only use this function if

    @code
    bt_trace_class_assigns_automatic_stream_class_id(trace_class)
    @endcode

    returns #BT_FALSE.

    Otherwise, use bt_stream_class_create().
    @endparblock

On success, the returned stream class has the following property values:

<table>
  <tr>
    <th>Property
    <th>Value
  <tr>
    <td>\ref api-tir-stream-cls-prop-id "Numeric ID"
    <td>\bt_p{id}
  <tr>
    <td>\ref api-tir-stream-cls-prop-name "Name"
    <td>\em None
  <tr>
    <td>\ref api-tir-stream-cls-prop-def-clock-cls "Default clock class"
    <td>\em None
  <tr>
    <td>\ref api-tir-stream-cls-prop-pc-fc "Packet context field class"
    <td>\em None
  <tr>
    <td>\ref api-tir-stream-cls-prop-ecc-fc "Event common context field class"
    <td>\em None
  <tr>
    <td>\ref api-tir-stream-cls-prop-auto-ec-id "Assigns automatic event class IDs?"
    <td>Yes
  <tr>
    <td>\ref api-tir-stream-cls-prop-auto-stream-id "Assigns automatic stream IDs?"
    <td>Yes
  <tr>
    <td>\ref api-tir-stream-cls-prop-supports-pkt "Supports packets?"
    <td>No
  <tr>
    <td>\ref api-tir-stream-cls-prop-pkt-beg-cs "Packets have a beginning default clock snapshot?"
    <td>No
  <tr>
    <td>\ref api-tir-stream-cls-prop-pkt-end-cs "Packets have an end default clock snapshot?"
    <td>No
  <tr>
    <td>\ref api-tir-stream-cls-prop-supports-disc-ev "Supports discarded events?"
    <td>No
  <tr>
    <td>\ref api-tir-stream-cls-prop-disc-ev-cs "Discarded events have default clock snapshots?"
    <td>No
  <tr>
    <td>\ref api-tir-stream-cls-prop-supports-disc-pkt "Supports discarded packets?"
    <td>No
  <tr>
    <td>\ref api-tir-stream-cls-prop-disc-pkt-cs "Discarded packets have default clock snapshots?"
    <td>No
  <tr>
    <td>\ref api-tir-stream-cls-prop-user-attrs "User attributes"
    <td>Empty \bt_map_val
</table>

@param[in] trace_class
    Trace class to add the created stream class to.
@param[in] id
    Numeric ID of the stream class to create and add to
    \bt_p{trace_class}.

@returns
    New stream class reference, or \c NULL on memory error.

@bt_pre_not_null{trace_class}
@pre
    <code>bt_trace_class_assigns_automatic_stream_class_id(trace_class)</code>
    returns #BT_FALSE.
@pre
    \bt_p{trace_class} does not contain a stream class with the numeric
    ID \bt_p{id}.

@bt_post_success_frozen{trace_class}

@sa bt_stream_class_create() &mdash;
    Creates a stream class with an automatic numeric ID and adds it to a
    trace class.
*/
extern bt_stream_class *bt_stream_class_create_with_id(
		bt_trace_class *trace_class, uint64_t id);

/*! @} */

/*!
@name Trace class access
@{
*/

/*!
@brief
    Borrows the \bt_trace_cls which contains the stream class
    \bt_p{stream_class}.

@param[in] stream_class
    Stream class from which to borrow the trace class which contains it.

@returns
    Trace class which contains \bt_p{stream_class}.

@bt_pre_not_null{stream_class}

@sa bt_stream_class_borrow_trace_class_const() &mdash;
    \c const version of this function.
*/
extern bt_trace_class *bt_stream_class_borrow_trace_class(
		bt_stream_class *stream_class);

/*!
@brief
    Borrows the \bt_trace_cls which contains the stream class
    \bt_p{stream_class} (\c const version).

See bt_stream_class_borrow_trace_class().
*/
extern const bt_trace_class *bt_stream_class_borrow_trace_class_const(
		const bt_stream_class *stream_class);

/*! @} */

/*!
@name Event class access
@{
*/

/*!
@brief
    Returns the number of \bt_p_ev_cls contained in the stream
    class \bt_p{stream_class}.

@param[in] stream_class
    Stream class of which to get the number of contained event classes.

@returns
    Number of contained event classes in \bt_p{stream_class}.

@bt_pre_not_null{stream_class}
*/
extern uint64_t bt_stream_class_get_event_class_count(
		const bt_stream_class *stream_class);

/*!
@brief
    Borrows the \bt_ev_cls at index \bt_p{index} from the
    stream class \bt_p{stream_class}.

@param[in] stream_class
    Stream class from which to borrow the event class at index
    \bt_p{index}.
@param[in] index
    Index of the event class to borrow from \bt_p{stream_class}.

@returns
    @parblock
    \em Borrowed reference of the event class of
    \bt_p{stream_class} at index \bt_p{index}.

    The returned pointer remains valid as long as \bt_p{stream_class}
    exists.
    @endparblock

@bt_pre_not_null{stream_class}
@pre
    \bt_p{index} is less than the number of event classes in
    \bt_p{stream_class} (as returned by
    bt_stream_class_get_event_class_count()).

@sa bt_stream_class_get_event_class_count() &mdash;
    Returns the number of event classes contained in a stream class.
@sa bt_stream_class_borrow_event_class_by_index_const() &mdash;
    \c const version of this function.
*/
extern bt_event_class *
bt_stream_class_borrow_event_class_by_index(
		bt_stream_class *stream_class, uint64_t index);

/*!
@brief
    Borrows the \bt_ev_cls at index \bt_p{index} from the
    stream class \bt_p{stream_class} (\c const version).

See bt_stream_class_borrow_event_class_by_index().
*/
extern const bt_event_class *
bt_stream_class_borrow_event_class_by_index_const(
		const bt_stream_class *stream_class, uint64_t index);

/*!
@brief
    Borrows the \bt_ev_cls having the numeric ID \bt_p{id} from the
    stream class \bt_p{stream_class}.

If there's no event class having the numeric ID \bt_p{id} in
\bt_p{stream_class}, this function returns \c NULL.

@param[in] stream_class
    Stream class from which to borrow the event class having the
    numeric ID \bt_p{id}.
@param[in] id
    ID of the event class to borrow from \bt_p{stream_class}.

@returns
    @parblock
    \em Borrowed reference of the event class of
    \bt_p{stream_class} having the numeric ID \bt_p{id}, or \c NULL
    if none.

    The returned pointer remains valid as long as \bt_p{stream_class}
    exists.
    @endparblock

@bt_pre_not_null{stream_class}

@sa bt_stream_class_borrow_event_class_by_id_const() &mdash;
    \c const version of this function.
*/
extern bt_event_class *
bt_stream_class_borrow_event_class_by_id(
		bt_stream_class *stream_class, uint64_t id);

/*!
@brief
    Borrows the \bt_ev_cls having the numeric ID \bt_p{id} from the
    stream class \bt_p{stream_class} (\c const version).

See bt_stream_class_borrow_event_class_by_id().
*/
extern const bt_event_class *
bt_stream_class_borrow_event_class_by_id_const(
		const bt_stream_class *stream_class, uint64_t id);

/*! @} */

/*!
@name Properties
@{
*/

/*!
@brief
    Returns the numeric ID of the stream class \bt_p{stream_class}.

See the \ref api-tir-stream-cls-prop-id "numeric ID" property.

@param[in] stream_class
    Stream class of which to get the numeric ID.

@returns
    Numeric ID of \bt_p{stream_class}.

@bt_pre_not_null{stream_class}

@sa bt_stream_class_create_with_id() &mdash;
    Creates a stream class with a specific numeric ID and adds it to a
    trace class.
*/
extern uint64_t bt_stream_class_get_id(
		const bt_stream_class *stream_class);

/*!
@brief
    Status codes for bt_stream_class_set_name().
*/
typedef enum bt_stream_class_set_name_status {
	/*!
	@brief
	    Success.
	*/
	BT_STREAM_CLASS_SET_NAME_STATUS_OK		= __BT_FUNC_STATUS_OK,

	/*!
	@brief
	    Out of memory.
	*/
	BT_STREAM_CLASS_SET_NAME_STATUS_MEMORY_ERROR	= __BT_FUNC_STATUS_MEMORY_ERROR,
} bt_stream_class_set_name_status;

/*!
@brief
    Sets the name of the stream class \bt_p{stream_class} to
    a copy of \bt_p{name}.

See the \ref api-tir-stream-cls-prop-name "name" property.

@param[in] stream_class
    Stream class of which to set the name to \bt_p{name}.
@param[in] name
    New name of \bt_p{stream_class} (copied).

@retval #BT_STREAM_CLASS_SET_NAME_STATUS_OK
    Success.
@retval #BT_STREAM_CLASS_SET_NAME_STATUS_MEMORY_ERROR
    Out of memory.

@bt_pre_not_null{stream_class}
@bt_pre_hot{stream_class}
@bt_pre_not_null{name}

@sa bt_stream_class_get_name() &mdash;
    Returns the name of a stream class.
*/
extern bt_stream_class_set_name_status bt_stream_class_set_name(
		bt_stream_class *stream_class, const char *name);

/*!
@brief
    Returns the name of the stream class \bt_p{stream_class}.

See the \ref api-tir-stream-cls-prop-name "name" property.

If \bt_p{stream_class} has no name, this function returns \c NULL.

@param[in] stream_class
    Stream class of which to get the name.

@returns
    @parblock
    Name of \bt_p{stream_class}, or \c NULL if none.

    The returned pointer remains valid as long as \bt_p{stream_class}
    is not modified.
    @endparblock

@bt_pre_not_null{stream_class}

@sa bt_stream_class_set_name() &mdash;
    Sets the name of a stream class.
*/
extern const char *bt_stream_class_get_name(
		const bt_stream_class *stream_class);

/*!
@brief
    Status codes for bt_stream_class_set_default_clock_class().
*/
typedef enum bt_stream_class_set_default_clock_class_status {
	/*!
	@brief
	    Success.
	*/
	BT_STREAM_CLASS_SET_DEFAULT_CLOCK_CLASS_STATUS_OK	= __BT_FUNC_STATUS_OK,
} bt_stream_class_set_default_clock_class_status;

/*!
@brief
    Sets the default \bt_clock_cls of the stream class
    \bt_p{stream_class} to \bt_p{clock_class}.

See the \ref api-tir-stream-cls-prop-def-clock-cls "default clock class"
property.

@param[in] stream_class
    Stream class of which to set the default clock class to
    \bt_p{clock_class}.
@param[in] clock_class
    New default clock class of \bt_p{stream_class}.

@retval #BT_STREAM_CLASS_SET_DEFAULT_CLOCK_CLASS_STATUS_OK
    Success.

@bt_pre_not_null{stream_class}
@bt_pre_hot{stream_class}
@bt_pre_not_null{clock_class}

@sa bt_stream_class_borrow_default_clock_class() &mdash;
    Borrows the default clock class of a stream class.
@sa bt_stream_class_borrow_default_clock_class_const() &mdash;
    Borrows the default clock class of a stream class (\c const version).
*/
extern bt_stream_class_set_default_clock_class_status
bt_stream_class_set_default_clock_class(
		bt_stream_class *stream_class,
		bt_clock_class *clock_class);

/*!
@brief
    Borrows the default \bt_clock_cls from the stream class
    \bt_p{stream_class}.

See the \ref api-tir-stream-cls-prop-def-clock-cls "default clock class"
property.

If \bt_p{stream_class} has no default clock class, this function
returns \c NULL.

@param[in] stream_class
    Stream class from which to borrow the default clock class.

@returns
    \em Borrowed reference of the default clock class of
    \bt_p{stream_class}, or \c NULL if none.

@bt_pre_not_null{stream_class}

@sa bt_stream_class_set_default_clock_class() &mdash;
    Sets the default clock class of a stream class.
@sa bt_stream_class_borrow_default_clock_class_const() &mdash;
    \c const version of this function.
*/
extern bt_clock_class *bt_stream_class_borrow_default_clock_class(
		bt_stream_class *stream_class);

/*!
@brief
    Borrows the default \bt_clock_cls from the stream class
    \bt_p{stream_class} (\c const version).

See bt_stream_class_borrow_default_clock_class().
*/
extern const bt_clock_class *
bt_stream_class_borrow_default_clock_class_const(
		const bt_stream_class *stream_class);

/*!
@brief
    Status codes for bt_stream_class_set_packet_context_field_class()
    and bt_stream_class_set_event_common_context_field_class().
*/
typedef enum bt_stream_class_set_field_class_status {
	/*!
	@brief
	    Success.
	*/
	BT_STREAM_CLASS_SET_FIELD_CLASS_STATUS_OK		= __BT_FUNC_STATUS_OK,

	/*!
	@brief
	    Out of memory.
	*/
	BT_STREAM_CLASS_SET_FIELD_CLASS_STATUS_MEMORY_ERROR	= __BT_FUNC_STATUS_MEMORY_ERROR,
} bt_stream_class_set_field_class_status;

/*!
@brief
    Sets the packet context \bt_fc of the stream class
    \bt_p{stream_class} to \bt_p{field_class}.

See the \ref api-tir-stream-cls-prop-pc-fc "packet context field class"
property.

\bt_p{stream_class} must support packets (see
bt_stream_class_set_supports_packets()).

@param[in] stream_class
    Stream class of which to set the packet context field class to
    \bt_p{field_class}.
@param[in] field_class
    New packet context field class of \bt_p{stream_class}.

@retval #BT_STREAM_CLASS_SET_FIELD_CLASS_STATUS_OK
    Success.
@retval #BT_STREAM_CLASS_SET_FIELD_CLASS_STATUS_MEMORY_ERROR
    Out of memory.

@bt_pre_not_null{stream_class}
@bt_pre_hot{stream_class}
@pre
    <code>bt_stream_class_supports_packets(stream_class)</code>
    returns #BT_TRUE.
@bt_pre_not_null{field_class}
@bt_pre_is_struct_fc{field_class}
@pre
    \bt_p{field_class}, or any of its contained field classes,
    is not already part of a stream class or of an \bt_ev_cls.
@pre
    If any of the field classes recursively contained in
    \bt_p{field_class} has a
    \ref api-tir-fc-link "link to another field class", it must honor
    the field class link rules.
@pre
    If any of the field classes recursively contained in
    \bt_p{field_class} has a
    \ref api-tir-fc-link "link to another field class", it must honor
    the field class link rules.

@bt_post_success_frozen{field_class}

@sa bt_stream_class_borrow_packet_context_field_class() &mdash;
    Borrows the packet context field class of a stream class.
@sa bt_stream_class_borrow_packet_context_field_class_const() &mdash;
    Borrows the packet context field class of a stream class
    (\c const version).
*/
extern bt_stream_class_set_field_class_status
bt_stream_class_set_packet_context_field_class(
		bt_stream_class *stream_class,
		bt_field_class *field_class);

/*!
@brief
    Borrows the packet context \bt_fc from the stream class
    \bt_p{stream_class}.

See the \ref api-tir-stream-cls-prop-pc-fc "packet context field class"
property.

If \bt_p{stream_class} has no packet context field class, this function
returns \c NULL.

@param[in] stream_class
    Stream class from which to borrow the packet context field class.

@returns
    \em Borrowed reference of the packet context field class of
    \bt_p{stream_class}, or \c NULL if none.

@bt_pre_not_null{stream_class}

@sa bt_stream_class_set_packet_context_field_class() &mdash;
    Sets the packet context field class of a stream class.
@sa bt_stream_class_borrow_packet_context_field_class_const() &mdash;
    \c const version of this function.
*/
extern bt_field_class *
bt_stream_class_borrow_packet_context_field_class(
		bt_stream_class *stream_class);

/*!
@brief
    Borrows the packet context \bt_fc from the stream class
    \bt_p{stream_class} (\c const version).

See bt_stream_class_borrow_packet_context_field_class().
*/
extern const bt_field_class *
bt_stream_class_borrow_packet_context_field_class_const(
		const bt_stream_class *stream_class);

/*!
@brief
    Sets the event common context \bt_fc of the stream class
    \bt_p{stream_class} to \bt_p{field_class}.

See the \ref api-tir-stream-cls-prop-ecc-fc "event common context field class"
property.

@param[in] stream_class
    Stream class of which to set the event common context field class to
    \bt_p{field_class}.
@param[in] field_class
    New event common context field class of \bt_p{stream_class}.

@retval #BT_STREAM_CLASS_SET_FIELD_CLASS_STATUS_OK
    Success.
@retval #BT_STREAM_CLASS_SET_FIELD_CLASS_STATUS_MEMORY_ERROR
    Out of memory.

@bt_pre_not_null{stream_class}
@bt_pre_hot{stream_class}
@bt_pre_not_null{field_class}
@bt_pre_is_struct_fc{field_class}
@pre
    \bt_p{field_class}, or any of its contained field classes,
    is not already part of a stream class or of an \bt_ev_cls.
@pre
    If any of the field classes recursively contained in
    \bt_p{field_class} has a
    \ref api-tir-fc-link "link to another field class", it must honor
    the field class link rules.

@bt_post_success_frozen{field_class}

@sa bt_stream_class_borrow_event_common_context_field_class() &mdash;
    Borrows the event common context field class of a stream class.
@sa bt_stream_class_borrow_event_common_context_field_class_const() &mdash;
    Borrows the event common context field class of a stream class
    (\c const version).
*/
extern bt_stream_class_set_field_class_status
bt_stream_class_set_event_common_context_field_class(
		bt_stream_class *stream_class,
		bt_field_class *field_class);

/*!
@brief
    Borrows the event common context \bt_fc from the stream class
    \bt_p{stream_class}.

See the \ref api-tir-stream-cls-prop-pc-fc "event common context field class"
property.

If \bt_p{stream_class} has no event common context field class, this
function returns \c NULL.

@param[in] stream_class
    Stream class from which to borrow the event common context
    field class.

@returns
    \em Borrowed reference of the event common context field class of
    \bt_p{stream_class}, or \c NULL if none.

@bt_pre_not_null{stream_class}

@sa bt_stream_class_set_event_common_context_field_class() &mdash;
    Sets the event common context field class of a stream class.
@sa bt_stream_class_borrow_event_common_context_field_class_const() &mdash;
    \c const version of this function.
*/

extern bt_field_class *
bt_stream_class_borrow_event_common_context_field_class(
		bt_stream_class *stream_class);

/*!
@brief
    Borrows the event common context \bt_fc from the stream class
    \bt_p{stream_class} (\c const version()).

See bt_stream_class_borrow_event_common_context_field_class().
*/
extern const bt_field_class *
bt_stream_class_borrow_event_common_context_field_class_const(
		const bt_stream_class *stream_class);

/*!
@brief
    Sets whether or not the stream class \bt_p{stream_class}
    automatically assigns a numeric ID to an \bt_ev_cls you create and
    add to it.

See the \ref api-tir-stream-cls-prop-auto-ec-id "assigns automatic event class IDs?"
property.

@param[in] stream_class
    Stream class of which to set whether or not it assigns automatic
    event class IDs.
@param[in] assigns_automatic_event_class_id
    #BT_TRUE to make \bt_p{stream_class} assign automatic event class
    IDs.

@bt_pre_not_null{stream_class}
@bt_pre_hot{stream_class}

@sa bt_stream_class_assigns_automatic_event_class_id() &mdash;
    Returns whether or not a stream class automatically assigns
    event class IDs.
*/
extern void bt_stream_class_set_assigns_automatic_event_class_id(
		bt_stream_class *stream_class,
    bt_bool assigns_automatic_event_class_id);

/*!
@brief
    Returns whether or not the stream class \bt_p{stream_class}
    automatically assigns a numeric ID to an \bt_ev_cls you create
    and add to it.

See the \ref api-tir-stream-cls-prop-auto-ec-id "assigns automatic event class IDs?"
property.

@param[in] stream_class
    Stream class of which to get whether or not it assigns automatic
    event class IDs.

@returns
    #BT_TRUE if \bt_p{stream_class} automatically
    assigns event class IDs.

@bt_pre_not_null{stream_class}

@sa bt_stream_class_set_assigns_automatic_event_class_id() &mdash;
    Sets whether or not a stream class automatically assigns
    event class IDs.
*/
extern bt_bool bt_stream_class_assigns_automatic_event_class_id(
		const bt_stream_class *stream_class);

/*!
@brief
    Sets whether or not the stream class \bt_p{stream_class}
    automatically assigns a numeric ID to a \bt_stream you create from
    it.

See the \ref api-tir-stream-cls-prop-auto-stream-id "assigns automatic stream IDs?"
property.

@param[in] stream_class
    Stream class of which to set whether or not it assigns automatic
    stream IDs.
@param[in] assigns_automatic_stream_id
    #BT_TRUE to make \bt_p{stream_class} assign automatic stream
    IDs.

@bt_pre_not_null{stream_class}
@bt_pre_hot{stream_class}

@sa bt_stream_class_assigns_automatic_stream_id() &mdash;
    Returns whether or not a stream class automatically assigns
    stream IDs.
*/
extern void bt_stream_class_set_assigns_automatic_stream_id(
		bt_stream_class *stream_class, bt_bool assigns_automatic_stream_id);

/*!
@brief
    Returns whether or not the stream class \bt_p{stream_class}
    automatically assigns a numeric ID to a \bt_stream you create
    from it.

See the \ref api-tir-stream-cls-prop-auto-stream-id "assigns automatic stream IDs?"
property.

@param[in] stream_class
    Stream class of which to get whether or not it assigns automatic
    stream IDs.

@returns
    #BT_TRUE if \bt_p{stream_class} automatically assigns stream IDs.

@bt_pre_not_null{stream_class}

@sa bt_stream_class_set_assigns_automatic_stream_id() &mdash;
    Sets whether or not a stream class automatically assigns
    stream IDs.
*/
extern bt_bool bt_stream_class_assigns_automatic_stream_id(
		const bt_stream_class *stream_class);

/*!
@brief
    Sets whether or not the instances (\bt_p_stream) of the
    stream class \bt_p{stream_class} have \bt_p_pkt and, if so,
    if those packets have beginning and/or end default
    \bt_p_cs.

See the
\ref api-tir-stream-cls-prop-supports-pkt "supports packets?",
\ref api-tir-stream-cls-prop-pkt-beg-cs "packets have a beginning default clock snapshot?",
and
\ref api-tir-stream-cls-prop-pkt-end-cs "packets have an end default clock snapshot?"
properties.

@param[in] stream_class
    Stream class of which to set whether or not its streams have
    packets.
@param[in] supports_packets
    #BT_TRUE to make the streams of \bt_p{stream_class} have packets.
@param[in] with_beginning_default_clock_snapshot
    #BT_TRUE to make the packets of the streams of \bt_p{stream_class}
    have a beginning default clock snapshot.
@param[in] with_end_default_clock_snapshot
    #BT_TRUE to make the packets of the streams of \bt_p{stream_class}
    have an end default clock snapshot.

@bt_pre_not_null{stream_class}
@bt_pre_hot{stream_class}
@pre
    <strong>If \bt_p{with_beginning_default_clock_snapshot} is
    #BT_TRUE</strong>,
    \bt_p{supports_packets} is also #BT_TRUE.
@pre
    <strong>If \bt_p{with_beginning_default_clock_snapshot} is
    #BT_TRUE</strong>,
    \bt_p{supports_packets} is also #BT_TRUE.
@pre
    <strong>If \bt_p{with_beginning_default_clock_snapshot} or
    \bt_p{with_end_default_clock_snapshot} is #BT_TRUE</strong>,
    \bt_p{stream_class} has a
    \ref api-tir-stream-cls-prop-def-clock-cls "default clock class".

@sa bt_stream_class_supports_packets() &mdash;
    Returns whether or not a stream class's streams have packets.
@sa bt_stream_class_packets_have_beginning_default_clock_snapshot() &mdash;
    Returns whether or not the packets of a stream class's streams
    have a beginning default clock snapshot.
@sa bt_stream_class_packets_have_end_default_clock_snapshot() &mdash;
    Returns whether or not the packets of a stream class's streams
    have an end default clock snapshot.
*/
extern void bt_stream_class_set_supports_packets(
		bt_stream_class *stream_class, bt_bool supports_packets,
		bt_bool with_beginning_default_clock_snapshot,
		bt_bool with_end_default_clock_snapshot);

/*!
@brief
    Returns whether or not the instances (\bt_p_stream) of the
    stream class \bt_p{stream_class} have \bt_p_pkt.

See the \ref api-tir-stream-cls-prop-supports-pkt "supports packets?"
property.

@param[in] stream_class
    Stream class of which to get whether or not its streams have
    packets.

@returns
    #BT_TRUE if the streams of \bt_p{stream_class} have packets.

@bt_pre_not_null{stream_class}

@sa bt_stream_class_set_supports_packets() &mdash;
    Sets whether or not a stream class's streams have packets.
*/
extern bt_bool bt_stream_class_supports_packets(
		const bt_stream_class *stream_class);

/*!
@brief
    Returns whether or not the \bt_p_pkt of the instances (\bt_p_stream)
    of the stream class \bt_p{stream_class} have a beginning
    default \bt_cs.

See the
\ref api-tir-stream-cls-prop-pkt-beg-cs "packets have a beginning default clock snapshot?"
property.

@param[in] stream_class
    Stream class of which to get whether or not its streams's packets
    have a beginning default clock snapshot.

@returns
    #BT_TRUE if the packets of the streams of \bt_p{stream_class} have a
    beginning default clock snapshot.

@bt_pre_not_null{stream_class}

@sa bt_stream_class_set_supports_packets() &mdash;
    Sets whether or not a stream class's streams have packets.
@sa bt_stream_class_packets_have_end_default_clock_snapshot() &mdash;
    Returns whether or not the packets of a stream class's streams
    have an end default clock snapshot.
*/
extern bt_bool bt_stream_class_packets_have_beginning_default_clock_snapshot(
		const bt_stream_class *stream_class);

/*!
@brief
    Returns whether or not the \bt_p_pkt of the instances (\bt_p_stream)
    of the stream class \bt_p{stream_class} have an end
    default \bt_cs.

See the
\ref api-tir-stream-cls-prop-pkt-end-cs "packets have an end default clock snapshot?"
property.

@param[in] stream_class
    Stream class of which to get whether or not its streams's packets
    have an end default clock snapshot.

@returns
    #BT_TRUE if the packets of the streams of \bt_p{stream_class} have
    an end default clock snapshot.

@bt_pre_not_null{stream_class}

@sa bt_stream_class_set_supports_packets() &mdash;
    Sets whether or not a stream class's streams have packets.
@sa bt_stream_class_packets_have_beginning_default_clock_snapshot() &mdash;
    Returns whether or not the packets of a stream class's streams
    have a beginning default clock snapshot.
*/
extern bt_bool bt_stream_class_packets_have_end_default_clock_snapshot(
		const bt_stream_class *stream_class);

/*!
@brief
    Sets whether or not the instances (\bt_p_stream) of the
    stream class \bt_p{stream_class} can have discarded events and,
    if so, if the \bt_p_disc_ev_msg of those streams have
    beginning and end default \bt_p_cs.

See the
\ref api-tir-stream-cls-prop-supports-disc-ev "supports discarded events?"
and
\ref api-tir-stream-cls-prop-disc-ev-cs "discarded events have default clock snapshots?"
properties.

@param[in] stream_class
    Stream class of which to set whether or not its streams can have
    discarded events.
@param[in] supports_discarded_events
    #BT_TRUE to make the streams of \bt_p{stream_class} be able to
    have discarded events.
@param[in] with_default_clock_snapshots
    #BT_TRUE to make the discarded events messages the streams of
    \bt_p{stream_class} have beginning and end default clock snapshots.

@bt_pre_not_null{stream_class}
@bt_pre_hot{stream_class}
@pre
    <strong>If \bt_p{with_default_clock_snapshots} is #BT_TRUE</strong>,
    \bt_p{supports_discarded_events} is also #BT_TRUE.
@pre
    <strong>If \bt_p{with_default_clock_snapshots} is #BT_TRUE</strong>,
    \bt_p{stream_class} has a
    \ref api-tir-stream-cls-prop-def-clock-cls "default clock class".

@sa bt_stream_class_supports_discarded_events() &mdash;
    Returns whether or not a stream class's streams can have
    discarded events.
@sa bt_stream_class_discarded_events_have_default_clock_snapshots() &mdash;
    Returns whether or not the discarded events messages of a
    stream class's streams have beginning and end default clock
    snapshots.
*/
extern void bt_stream_class_set_supports_discarded_events(
		bt_stream_class *stream_class,
		bt_bool supports_discarded_events,
		bt_bool with_default_clock_snapshots);

/*!
@brief
    Returns whether or not the instances (\bt_p_stream) of the
    stream class \bt_p{stream_class} can have discarded events.

See the
\ref api-tir-stream-cls-prop-supports-disc-ev "supports discarded events?"
property.

@param[in] stream_class
    Stream class of which to get whether or not its streams can have
    discarded events.

@returns
    #BT_TRUE if the streams of \bt_p{stream_class} can have discarded
    events.

@bt_pre_not_null{stream_class}

@sa bt_stream_class_set_supports_discarded_events() &mdash;
    Sets whether or not a stream class's streams can have discarded
    events.
*/
extern bt_bool bt_stream_class_supports_discarded_events(
		const bt_stream_class *stream_class);

/*!
@brief
    Returns whether or not the \bt_p_disc_ev_msg of the instances
    (\bt_p_stream) of the stream class \bt_p{stream_class} have
    beginning and end default \bt_p_cs.

See the
\ref api-tir-stream-cls-prop-disc-ev-cs "discarded events have default clock snapshots?"
property.

@param[in] stream_class
    Stream class of which to get whether or not its streams's discarded
    events messages have a beginning and end default clock snapshots.

@returns
    #BT_TRUE if the discarded events messages of the streams of
    \bt_p{stream_class} have beginning and end default clock snapshots.

@bt_pre_not_null{stream_class}

@sa bt_stream_class_set_supports_discarded_events() &mdash;
    Sets whether or not a stream class's streams can have discarded
    events.
*/
extern bt_bool bt_stream_class_discarded_events_have_default_clock_snapshots(
		const bt_stream_class *stream_class);

/*!
@brief
    Sets whether or not the instances (\bt_p_stream) of the
    stream class \bt_p{stream_class} can have discarded packets and,
    if so, if the \bt_p_disc_pkt_msg of those streams have
    beginning and end default \bt_p_cs.

See the
\ref api-tir-stream-cls-prop-supports-disc-pkt "supports discarded packets?"
and
\ref api-tir-stream-cls-prop-disc-pkt-cs "discarded packets have default clock snapshots?"
properties.

\bt_p{stream_class} must support packets (see
bt_stream_class_set_supports_packets()).

@param[in] stream_class
    Stream class of which to set whether or not its streams can have
    discarded packets.
@param[in] supports_discarded_packets
    #BT_TRUE to make the streams of \bt_p{stream_class} be able to
    have discarded packets.
@param[in] with_default_clock_snapshots
    #BT_TRUE to make the discarded packets messages the streams of
    \bt_p{stream_class} have beginning and end default clock snapshots.

@bt_pre_not_null{stream_class}
@bt_pre_hot{stream_class}
@pre
    <code>bt_stream_class_supports_packets(stream_class)</code>
    returns #BT_TRUE.
@pre
    <strong>If \bt_p{with_default_clock_snapshots} is #BT_TRUE</strong>,
    \bt_p{supports_discarded_packets} is also #BT_TRUE.
@pre
    <strong>If \bt_p{with_default_clock_snapshots} is #BT_TRUE</strong>,
    \bt_p{stream_class} has a
    \ref api-tir-stream-cls-prop-def-clock-cls "default clock class".

@sa bt_stream_class_supports_discarded_packets() &mdash;
    Returns whether or not a stream class's streams can have
    discarded packets.
@sa bt_stream_class_discarded_packets_have_default_clock_snapshots() &mdash;
    Returns whether or not the discarded packets messages of a
    stream class's streams have beginning and end default clock
    snapshots.
*/
extern void bt_stream_class_set_supports_discarded_packets(
		bt_stream_class *stream_class,
		bt_bool supports_discarded_packets,
		bt_bool with_default_clock_snapshots);

/*!
@brief
    Returns whether or not the instances (\bt_p_stream) of the
    stream class \bt_p{stream_class} can have discarded packets.

See the
\ref api-tir-stream-cls-prop-supports-disc-pkt "supports discarded packets?"
property.

@param[in] stream_class
    Stream class of which to get whether or not its streams can have
    discarded packets.

@returns
    #BT_TRUE if the streams of \bt_p{stream_class} can have discarded
    packets.

@bt_pre_not_null{stream_class}

@sa bt_stream_class_set_supports_discarded_packets() &mdash;
    Sets whether or not a stream class's streams can have discarded
    packets.
*/
extern bt_bool bt_stream_class_supports_discarded_packets(
		const bt_stream_class *stream_class);

/*!
@brief
    Returns whether or not the \bt_p_disc_pkt_msg of the instances
    (\bt_p_stream) of the stream class \bt_p{stream_class} have
    beginning and end default \bt_p_cs.

See the
\ref api-tir-stream-cls-prop-disc-ev-cs "discarded packets have default clock snapshots?"
property.

@param[in] stream_class
    Stream class of which to get whether or not its streams's discarded
    packets messages have a beginning and end default clock snapshots.

@returns
    #BT_TRUE if the discarded packets messages of the streams of
    \bt_p{stream_class} have beginning and end default clock snapshots.

@bt_pre_not_null{stream_class}

@sa bt_stream_class_set_supports_discarded_packets() &mdash;
    Sets whether or not a stream class's streams can have discarded
    packets.
*/
extern bt_bool bt_stream_class_discarded_packets_have_default_clock_snapshots(
		const bt_stream_class *stream_class);

/*!
@brief
    Sets the user attributes of the stream class \bt_p{stream_class} to
    \bt_p{user_attributes}.

See the \ref api-tir-stream-cls-prop-user-attrs "user attributes"
property.

@note
    When you create a default stream class with bt_stream_class_create()
    or bt_stream_class_create_with_id(), the stream class's initial user
    attributes is an empty \bt_map_val. Therefore you can borrow it with
    bt_stream_class_borrow_user_attributes() and fill it directly
    instead of setting a new one with this function.

@param[in] stream_class
    Stream class of which to set the user attributes to
    \bt_p{user_attributes}.
@param[in] user_attributes
    New user attributes of \bt_p{stream_class}.

@bt_pre_not_null{stream_class}
@bt_pre_hot{stream_class}
@bt_pre_not_null{user_attributes}
@bt_pre_is_map_val{user_attributes}

@sa bt_stream_class_borrow_user_attributes() &mdash;
    Borrows the user attributes of a stream class.
*/
extern void bt_stream_class_set_user_attributes(
		bt_stream_class *stream_class, const bt_value *user_attributes);

/*!
@brief
    Borrows the user attributes of the stream class \bt_p{stream_class}.

See the \ref api-tir-stream-cls-prop-user-attrs "user attributes"
property.

@note
    When you create a default stream class with bt_stream_class_create()
    or bt_stream_class_create_with_id(), the stream class's initial user
    attributes is an empty \bt_map_val.

@param[in] stream_class
    Stream class from which to borrow the user attributes.

@returns
    User attributes of \bt_p{stream_class} (a \bt_map_val).

@bt_pre_not_null{stream_class}

@sa bt_stream_class_set_user_attributes() &mdash;
    Sets the user attributes of a stream class.
@sa bt_stream_class_borrow_user_attributes_const() &mdash;
    \c const version of this function.
*/
extern bt_value *bt_stream_class_borrow_user_attributes(
		bt_stream_class *stream_class);

/*!
@brief
    Borrows the user attributes of the stream class \bt_p{stream_class}
    (\c const version).

See bt_stream_class_borrow_user_attributes().
*/
extern const bt_value *bt_stream_class_borrow_user_attributes_const(
		const bt_stream_class *stream_class);

/*! @} */

/*!
@name Reference count
@{
*/

/*!
@brief
    Increments the \ref api-fund-shared-object "reference count" of
    the stream class \bt_p{stream_class}.

@param[in] stream_class
    @parblock
    Stream class of which to increment the reference count.

    Can be \c NULL.
    @endparblock

@sa bt_stream_class_put_ref() &mdash;
    Decrements the reference count of a stream class.
*/
extern void bt_stream_class_get_ref(const bt_stream_class *stream_class);

/*!
@brief
    Decrements the \ref api-fund-shared-object "reference count" of
    the stream class \bt_p{stream_class}.

@param[in] stream_class
    @parblock
    Stream class of which to decrement the reference count.

    Can be \c NULL.
    @endparblock

@sa bt_stream_class_get_ref() &mdash;
    Increments the reference count of a stream class.
*/
extern void bt_stream_class_put_ref(const bt_stream_class *stream_class);

/*!
@brief
    Decrements the reference count of the stream class
    \bt_p{_stream_class}, and then sets \bt_p{_stream_class} to \c NULL.

@param _stream_class
    @parblock
    Stream class of which to decrement the reference count.

    Can contain \c NULL.
    @endparblock

@bt_pre_assign_expr{_stream_class}
*/
#define BT_STREAM_CLASS_PUT_REF_AND_RESET(_stream_class)	\
	do {							\
		bt_stream_class_put_ref(_stream_class);		\
		(_stream_class) = NULL;				\
	} while (0)

/*!
@brief
    Decrements the reference count of the stream class \bt_p{_dst}, sets
    \bt_p{_dst} to \bt_p{_src}, and then sets \bt_p{_src} to \c NULL.

This macro effectively moves a stream class reference from the expression
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
#define BT_STREAM_CLASS_MOVE_REF(_dst, _src)	\
	do {					\
		bt_stream_class_put_ref(_dst);	\
		(_dst) = (_src);		\
		(_src) = NULL;			\
	} while (0)

/*! @} */

/*! @} */

#ifdef __cplusplus
}
#endif

#endif /* BABELTRACE2_TRACE_IR_STREAM_CLASS_H */
