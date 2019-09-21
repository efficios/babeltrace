#ifndef BABELTRACE2_TRACE_IR_PACKET_H
#define BABELTRACE2_TRACE_IR_PACKET_H

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
@defgroup api-tir-pkt Packet
@ingroup api-tir

@brief
    Trace packet.

A <strong><em>packet</em></strong> is a conceptual container of
\bt_p_ev within a \bt_stream.

Some trace formats group events together within packets. This is the
case, for example, of the
<a href="https://diamon.org/ctf/">Common Trace Format</a>.

Because a packet could contain millions of events, there are no actual
links from a packet to its events. However, there are links from a
packet's events to it (see bt_event_borrow_packet() and
bt_event_borrow_packet_const()).

A packet can contain a context \bt_field which is data associated to
all the events of the packet.

A packet is a \ref api-tir "trace IR" data object.

A packet conceptually belongs to a \bt_stream. Borrow the stream of a
packet with bt_packet_borrow_stream() and
bt_packet_borrow_stream_const().

Before you create a packet for a given stream, the stream's class must
\ref api-tir-stream-cls-prop-supports-pkt "support packets".

Create a packet with bt_packet_create(). You can then use this packet to
create a \bt_pb_msg and a \bt_pe_msg.

A packet is a \ref api-fund-shared-object "shared object": get a
new reference with bt_packet_get_ref() and put an existing
reference with bt_packet_put_ref().

Some library functions \ref api-fund-freezing "freeze" packets on
success. The documentation of those functions indicate this
postcondition.

The type of a packet is #bt_packet.

<h1>Property</h1>

A packet has the following property:

<dl>
  <dt>\anchor api-tir-pkt-prop-ctx Context field</dt>
  <dd>
    Packet's context \bt_field.

    The context of a packet contains data associated to all its
    events.

    The \ref api-tir-fc "class" of a packet's context field is set
    at the packet's \bt_stream_cls level. See
    bt_stream_class_set_packet_context_field_class()
    bt_stream_class_borrow_packet_context_field_class(),
    and bt_stream_class_borrow_packet_context_field_class_const()

    Use bt_packet_borrow_context_field() and
    bt_packet_borrow_context_field_const().
  </dd>
</dl>
*/

/*! @{ */

/*!
@name Type
@{

@typedef struct bt_packet bt_packet;

@brief
    Packet.

@}
*/

/*!
@name Creation
@{
*/

/*!
@brief
    Creates a packet for the \bt_stream \bt_p{stream}.

@attention
    @parblock
    Only use this function if

    @code
    bt_stream_class_supports_packets(bt_stream_borrow_class_const(stream))
    @endcode

    returns #BT_TRUE.
    @endparblock

On success, the returned packet has the following property value:

<table>
  <tr>
    <th>Property
    <th>Value
  <tr>
    <td>\ref api-tir-pkt-prop-ctx "Context field"
    <td>
      Unset instance of the
      \ref api-tir-stream-cls-prop-pc-fc "packet context field class" of
      the \ref api-tir-stream-cls "class" of \bt_p{stream}.
</table>

@param[in] stream
    Stream for which to create the packet.

@returns
    New packet reference, or \c NULL on memory error.

@pre
    <code>bt_stream_class_supports_packets(bt_stream_borrow_class_const(stream))</code>
    returns #BT_TRUE.
*/
extern bt_packet *bt_packet_create(const bt_stream *stream);

/*! @} */

/*!
@name Stream access
@{
*/

/*!
@brief
    Borrows the \bt_stream conceptually containing the packet
    \bt_p{packet}.

@param[in] packet
    Packet of which to borrow the stream conceptually containing it.

@returns
    \em Borrowed reference of the stream conceptually containing
    \bt_p{packet}.

@bt_pre_not_null{packet}

@sa bt_packet_borrow_stream_const() &mdash;
    \c const version of this function.
*/
extern bt_stream *bt_packet_borrow_stream(bt_packet *packet);

/*!
@brief
    Borrows the \bt_stream conceptually containing the packet
    \bt_p{packet} (\c const version).

See bt_packet_borrow_stream().
*/
extern const bt_stream *bt_packet_borrow_stream_const(
		const bt_packet *packet);

/*! @} */

/*!
@name Property
@{
*/

/*!
@brief
    Borrows the context \bt_field of the packet \bt_p{packet}.

See the \ref api-tir-pkt-prop-ctx "context field" property.

@param[in] packet
    Packet of which to borrow the context field.

@returns
    \em Borrowed reference of the context field of
    \bt_p{packet}, or \c NULL if none.

@bt_pre_not_null{packet}

@sa bt_packet_borrow_context_field_const() &mdash;
    \c const version of this function.
*/
extern
bt_field *bt_packet_borrow_context_field(bt_packet *packet);

/*!
@brief
    Borrows the context \bt_field of the packet \bt_p{packet}
    (\c const version).

See bt_packet_borrow_context_field().
*/
extern
const bt_field *bt_packet_borrow_context_field_const(
		const bt_packet *packet);

/*! @} */

/*!
@name Reference count
@{
*/

/*!
@brief
    Increments the \ref api-fund-shared-object "reference count" of
    the packet \bt_p{packet}.

@param[in] packet
    @parblock
    Packet of which to increment the reference count.

    Can be \c NULL.
    @endparblock

@sa bt_packet_put_ref() &mdash;
    Decrements the reference count of a packet.
*/
extern void bt_packet_get_ref(const bt_packet *packet);

/*!
@brief
    Decrements the \ref api-fund-shared-object "reference count" of
    the packet \bt_p{packet}.

@param[in] packet
    @parblock
    Packet of which to decrement the reference count.

    Can be \c NULL.
    @endparblock

@sa bt_packet_get_ref() &mdash;
    Increments the reference count of a packet.
*/
extern void bt_packet_put_ref(const bt_packet *packet);

/*!
@brief
    Decrements the reference count of the packet
    \bt_p{_packet}, and then sets \bt_p{_packet} to \c NULL.

@param _packet
    @parblock
    Packet of which to decrement the reference count.

    Can contain \c NULL.
    @endparblock

@bt_pre_assign_expr{_packet}
*/
#define BT_PACKET_PUT_REF_AND_RESET(_packet)		\
	do {						\
		bt_packet_put_ref(_packet);		\
		(_packet) = NULL;			\
	} while (0)

/*!
@brief
    Decrements the reference count of the packet \bt_p{_dst}, sets
    \bt_p{_dst} to \bt_p{_src}, and then sets \bt_p{_src} to \c NULL.

This macro effectively moves a packet reference from the expression
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
#define BT_PACKET_MOVE_REF(_dst, _src)		\
	do {					\
		bt_packet_put_ref(_dst);	\
		(_dst) = (_src);		\
		(_src) = NULL;			\
	} while (0)

/*! @} */

/*! @} */

#ifdef __cplusplus
}
#endif

#endif /* BABELTRACE2_TRACE_IR_PACKET_H */
