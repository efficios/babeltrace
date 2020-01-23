#ifndef BABELTRACE2_TRACE_IR_CLOCK_SNAPSHOT_H
#define BABELTRACE2_TRACE_IR_CLOCK_SNAPSHOT_H

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
@defgroup api-tir-cs Clock snapshot
@ingroup api-tir

@brief
    Snapshot of a \bt_stream clock.

A <strong><em>clock snapshot</em></strong> is a snapshot of the value
of a \bt_stream clock (a \bt_clock_cls instance).

A clock snapshot is a \ref api-tir "trace IR" data object.

<em>Stream clocks</em> only exist conceptually in \bt_name because they
are stateful objects. \bt_cp_msg cannot refer to stateful objects
because they must not change while being transported from one
\bt_comp to the other.

Instead of having a stream clock object, messages have a default
clock snapshot: this is a snapshot of the value of a stream's
default clock (a clock class instance):

@image html clocks.png

In the illustration above, notice that:

- Each stream has a default clock (yellow bell alarm clock): this is an
  instance of the stream's class's default clock class.
- Each \bt_msg (objects in blue stream rectangles) created for a given
  stream has a default clock snapshot (yellow star): this is a snapshot
  of the stream's default clock.

  In other words, a default clock snapshot contains the value of the
  stream's default clock when this message occurred.

A clock snapshot is a \ref api-fund-unique-object "unique object": it
belongs to a \bt_msg.

The type of a clock snapshot is #bt_clock_snapshot.

You cannot create a clock snapshot: you specify a clock snapshot value
(in clock cycles, a \c uint64_t value) when you create a \bt_msg or set
a message's clock snapshot with one of:

- bt_message_stream_beginning_set_default_clock_snapshot()
- bt_message_stream_end_set_default_clock_snapshot()
- bt_message_event_create_with_default_clock_snapshot()
- bt_message_event_create_with_packet_and_default_clock_snapshot()
- bt_message_packet_beginning_create_with_default_clock_snapshot()
- bt_message_packet_end_create_with_default_clock_snapshot()
- bt_message_discarded_events_create_with_default_clock_snapshots()
- bt_message_discarded_packets_create_with_default_clock_snapshots()
- bt_message_message_iterator_inactivity_create()

See \ref api-tir-clock-cls-origin "Clock value vs. clock class origin"
to understand the meaning of a clock's value in relation to the
properties of its class.
*/

/*! @{ */

/*!
@name Type
@{

@typedef struct bt_clock_snapshot bt_clock_snapshot;

@brief
    Clock snapshot.

@}
*/

/*!
@brief
    Borrows the \ref api-tir-clock-cls "class" of the clock of which
    \bt_p{clock_snapshot} is a snapshot.

@param[in] clock_snapshot
    Clock snapshot of which to borrow the clock class.

@returns
    \em Borrowed reference of the clock class of \bt_p{clock_snapshot}.

@bt_pre_not_null{clock_snapshot}
*/
extern const bt_clock_class *bt_clock_snapshot_borrow_clock_class_const(
		const bt_clock_snapshot *clock_snapshot);

/*!
@brief
    Returns the value, in clock cycles, of the clock snapshot
    \bt_p{clock_snapshot}.

@param[in] clock_snapshot
    Clock snapshot of which to get the value.

@returns
    Value of \bt_p{clock_snapshot} (clock cycles).

@bt_pre_not_null{clock_snapshot}

@sa bt_clock_snapshot_get_ns_from_origin() &mdash;
    Returns the equivalent nanoseconds from clock class origin of a
    clock snapshot's value.
*/
extern uint64_t bt_clock_snapshot_get_value(
		const bt_clock_snapshot *clock_snapshot);

/*!
@brief
    Status codes for bt_clock_snapshot_get_ns_from_origin().
*/
typedef enum bt_clock_snapshot_get_ns_from_origin_status {
	/*!
	@brief
	    Success.
	*/
	BT_CLOCK_SNAPSHOT_GET_NS_FROM_ORIGIN_STATUS_OK			= __BT_FUNC_STATUS_OK,

	/*!
	@brief
	    Integer overflow while computing the result.
	*/
	BT_CLOCK_SNAPSHOT_GET_NS_FROM_ORIGIN_STATUS_OVERFLOW_ERROR	= __BT_FUNC_STATUS_OVERFLOW_ERROR,
} bt_clock_snapshot_get_ns_from_origin_status;

/*!
@brief
    Converts the value of the clock snapshot
    \bt_p{clock_snapshot} from cycles to nanoseconds from the
    \ref api-tir-clock-cls-origin "origin" of its
    \bt_clock_cls and sets \bt_p{*ns_from_origin} to the result.

This function:

-# Converts the
   \link api-tir-clock-cls-prop-offset "offset in cycles"\endlink
   property of the clock class of \bt_p{clock_snapshot} to
   seconds using its
   \ref api-tir-clock-cls-prop-freq "frequency".
-# Converts the value of \bt_p{clock_snapshot} to seconds using the
   frequency of its clock class.
-# Adds the values of 1., 2., and the
   \link api-tir-clock-cls-prop-offset "offset in seconds"\endlink
   property of the clock class of \bt_p{clock_snapshot}.
-# Converts the value of 3. to nanoseconds and sets
   \bt_p{*ns_from_origin} to this result.

The following illustration shows the possible scenarios:

@image html clock-terminology.png

This function can fail and return the
#BT_CLOCK_SNAPSHOT_GET_NS_FROM_ORIGIN_STATUS_OVERFLOW_ERROR status
code if any step of the computation process causes an integer overflow.

@param[in] clock_snapshot
    Clock snapshot containing the value to convert to nanoseconds
    from the origin of its clock class.
@param[out] ns_from_origin
    <strong>On success</strong>, \bt_p{*ns_from_origin} is the value
    of \bt_p{clock_snapshot} converted to nanoseconds from the origin
    of its clock class.

@retval #BT_CLOCK_SNAPSHOT_GET_NS_FROM_ORIGIN_STATUS_OK
    Success.
@retval #BT_CLOCK_SNAPSHOT_GET_NS_FROM_ORIGIN_STATUS_OVERFLOW_ERROR
    Integer overflow while computing the result.

@bt_pre_not_null{clock_snapshot}
@bt_pre_not_null{ns_from_origin}

@sa bt_util_clock_cycles_to_ns_from_origin() &mdash;
    Converts a clock value from cycles to nanoseconds from the clock's
    origin.
@sa bt_clock_class_cycles_to_ns_from_origin() &mdash;
    Converts a clock value from cycles to nanoseconds from a clock
    class's origin.
*/
extern bt_clock_snapshot_get_ns_from_origin_status
bt_clock_snapshot_get_ns_from_origin(
		const bt_clock_snapshot *clock_snapshot,
		int64_t *ns_from_origin);

/*! @} */

#ifdef __cplusplus
}
#endif

#endif /* BABELTRACE2_TRACE_IR_CLOCK_SNAPSHOT_H */
