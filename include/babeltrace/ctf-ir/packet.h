#ifndef BABELTRACE_CTF_IR_PACKET_H
#define BABELTRACE_CTF_IR_PACKET_H

/*
 * BabelTrace - CTF IR: Stream packet
 *
 * Copyright 2016 Philippe Proulx <pproulx@efficios.com>
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
 *
 * The Common Trace Format (CTF) Specification is available at
 * http://www.efficios.com/ctf
 */

#include <stdint.h>

/* For bt_get() */
#include <babeltrace/ref.h>

/* For bt_bool */
#include <babeltrace/types.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
@defgroup ctfirpacket CTF IR packet
@ingroup ctfir
@brief CTF IR packet.

@code
#include <babeltrace/ctf-ir/packet.h>
@endcode

A CTF IR <strong><em>packet</em></strong> is a container of packet
fields, that is, of the <strong>trace packet header</strong> and
<strong>stream packet context</strong> fields.

As a reminder, here's the structure of a CTF packet:

@imgpacketstructure

You can create a CTF IR packet \em from a
\link ctfirstream CTF IR stream\endlink with bt_packet_create(). The
stream you use to create a packet object becomes its parent.

When you set the trace packet header and stream packet context fields of
a packet with resp. bt_packet_set_header() and
bt_packet_set_context(), their field type \em must be equivalent to
the field types returned by resp. bt_trace_get_packet_header_type()
and bt_stream_class_get_packet_context_type() for its parent trace
class and stream class.

You can attach a packet object to a \link ctfirevent CTF IR
event\endlink object with bt_event_set_packet().

As with any Babeltrace object, CTF IR packet objects have
<a href="https://en.wikipedia.org/wiki/Reference_counting">reference
counts</a>. See \ref refs to learn more about the reference counting
management of Babeltrace objects.

bt_notification_event_create() \em freezes its event parameter on
success, which in turns freezes the event's associated packet object.
This is the only way that a CTF IR packet object can be frozen.
You cannot modify a frozen packet: it is considered immutable,
except for \link refs reference counting\endlink.

@sa ctfirstream
@sa ctfirstreamclass
@sa ctfirtraceclass

@file
@brief CTF IR packet type and functions.
@sa ctfirpacket

@addtogroup ctfirpacket
@{
*/

/**
@struct bt_packet
@brief A CTF IR packet.
@sa ctfirpacket
*/
struct bt_packet;
struct bt_packet_header_field;
struct bt_packet_context_field;
struct bt_stream;
struct bt_clock_value;
struct bt_clock_class;

/**
@name Creation and parent access functions
@{
*/

/**
@brief	Creates a default CTF IR packet with \p stream as its parent
	CTF IR stream.

On success, the packet object's trace packet header and stream packet
context fields are not set. You can set them with resp.
bt_packet_set_header() and bt_packet_set_context().

@param[in] stream	Parent CTF IR stream of the packet to create.
@returns		Created packet, or \c NULL on error.

@prenotnull{stream}
@postsuccessrefcountret1
*/
extern struct bt_packet *bt_packet_create(struct bt_stream *stream);

extern struct bt_stream *bt_packet_borrow_stream(struct bt_packet *packet);

/**
@brief	Returns the parent CTF IR stream of the CTF IR packet \p packet.

This function returns a reference to the stream which was used to create
the packet object in the first place with bt_packet_create().

@param[in] packet	Packet of which to get the parent stream.
@returns		Parent stream of \p packet, or \c NULL on error.

@prenotnull{packet}
@postrefcountsame{packet}
@postsuccessrefcountretinc
*/
static inline
struct bt_stream *bt_packet_get_stream(
		struct bt_packet *packet)
{
	return bt_get(bt_packet_borrow_stream(packet));
}

/** @} */

/**
@name Contained fields functions
@{
*/

extern
struct bt_field *bt_packet_borrow_header(struct bt_packet *packet);

extern
int bt_packet_move_header(struct bt_packet *packet,
		struct bt_packet_header_field *header);

extern
struct bt_field *bt_packet_borrow_context(struct bt_packet *packet);

extern
int bt_packet_move_context(struct bt_packet *packet,
		struct bt_packet_context_field *context);

/** @} */

extern int bt_packet_set_beginning_clock_value(struct bt_packet *packet,
		struct bt_clock_class *clock_class, uint64_t raw_value,
		bt_bool is_default);

extern struct bt_clock_value *bt_packet_borrow_default_beginning_clock_value(
		struct bt_packet *packet);

extern int bt_packet_set_end_clock_value(struct bt_packet *packet,
		struct bt_clock_class *clock_class, uint64_t raw_value,
		bt_bool is_default);

extern struct bt_clock_value *bt_packet_borrow_default_end_clock_value(
		struct bt_packet *packet);

/** @} */

#ifdef __cplusplus
}
#endif

#endif /* BABELTRACE_CTF_IR_PACKET_H */
