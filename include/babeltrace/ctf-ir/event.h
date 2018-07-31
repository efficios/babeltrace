#ifndef BABELTRACE_CTF_IR_EVENT_H
#define BABELTRACE_CTF_IR_EVENT_H

/*
 * BabelTrace - CTF IR: Event
 *
 * Copyright 2013, 2014 Jérémie Galarneau <jeremie.galarneau@efficios.com>
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
 *
 * The Common Trace Format (CTF) Specification is available at
 * http://www.efficios.com/ctf
 */

/* For bt_get() */
#include <babeltrace/ref.h>

/* For bt_bool */
#include <babeltrace/types.h>

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

struct bt_value;
struct bt_clock_class;

/**
@defgroup ctfirevent CTF IR event
@ingroup ctfir
@brief CTF IR event.

@code
#include <babeltrace/ctf-ir/event.h>
@endcode

A CTF IR <strong><em>event</em></strong> is a container of event
fields:

- <strong>Stream event header</strong> field, described by the
  <em>stream event header field type</em> of a
  \link ctfirstreamclass CTF IR stream class\endlink.
- <strong>Stream event context</strong> field, described by the
  <em>stream event context field type</em> of a stream class.
- <strong>Event context</strong> field, described by the
  <em>event context field type</em> of a
  \link ctfireventclass CTF IR event class\endlink.
- <strong>Event payload</strong>, described by the
  <em>event payload field type</em> of an event class.

As a reminder, here's the structure of a CTF packet:

@imgpacketstructure

You can create a CTF IR event \em from a
\link ctfireventclass CTF IR event class\endlink with
bt_event_create(). The event class you use to create an event
object becomes its parent.

If the \link ctfirtraceclass CTF IR trace class\endlink of an event
object (parent of its \link ctfirstreamclass CTF IR stream class\endlink,
which is the parent of its event class) was created by a
\link ctfwriter CTF writer\endlink object, then the only possible
action you can do with this event object is to append it to a
\link ctfirstream CTF IR stream\endlink with
bt_stream_append_event(). Otherwise, you can create an event
notification with bt_notification_event_create(). The event you pass
to this function \em must have an attached packet object first.

You can attach a \link ctfirpacket CTF IR packet object\endlink to an
event object with bt_event_set_packet().

A CTF IR event has a mapping of
\link ctfirclockvalue CTF IR clock values\endlink. A clock value is
an instance of a specific
\link ctfirclockclass CTF IR clock class\endlink when the event is
emitted. You can set an event object's clock value with
bt_event_set_clock_value().

As with any Babeltrace object, CTF IR event objects have
<a href="https://en.wikipedia.org/wiki/Reference_counting">reference
counts</a>. See \ref refs to learn more about the reference counting
management of Babeltrace objects.

bt_notification_event_create() \em freezes its event parameter on
success. You cannot modify a frozen event object: it is considered
immutable, except for \link refs reference counting\endlink.

@sa ctfireventclass
@sa ctfirpacket

@file
@brief CTF IR event type and functions.
@sa ctfirevent

@addtogroup ctfirevent
@{
*/

/**
@struct bt_event
@brief A CTF IR event.
@sa ctfirevent
*/
struct bt_event;
struct bt_event_header_field;
struct bt_clock;
struct bt_clock_value;
struct bt_event_class;
struct bt_field;
struct bt_field_type;
struct bt_stream_class;
struct bt_packet;

/**
@name Creation and parent access functions
@{
*/

/**
@brief  Creates a default CTF IR event from the CTF IR event class
	\p event_class.

\p event_class \em must have a parent
\link ctfirstreamclass CTF IR stream class\endlink.

On success, the four fields of the created event object are not set. You
can set them with bt_event_set_header(),
bt_event_set_stream_event_context(),
bt_event_set_context(), and bt_event_set_payload().

This function tries to resolve the needed
\link ctfirfieldtypes CTF IR field type\endlink of the dynamic field
types that are found anywhere in the context or payload field
types of \p event_class and in the root field types of the
parent stream class of \p event_class. If any automatic resolving fails,
this function fails. This means that, if any dynamic field type need
a field type which should be found in the trace packet header root
field type, and if the parent stream class of \p event_class was not
added to a \link ctfirtraceclass CTF IR trace class\endlink yet
with bt_trace_add_stream_class(), then this function fails.

@param[in] event_class	CTF IR event class to use to create the
			CTF IR event.
@returns		Created event object, or \c NULL on error.

@prenotnull{event_class}
@pre \p event_class has a parent stream class.
@postsuccessrefcountret1
*/
extern struct bt_event_class *bt_event_borrow_class(struct bt_event *event);

/**
@brief	Returns the parent CTF IR event class of the CTF IR event
	\p event.

This function returns a reference to the event class which was used to
create the event object in the first place with bt_event_create().

@param[in] event	Event of which to get the parent event class.
@returns		Parent event class of \p event,
			or \c NULL on error.

@prenotnull{event}
@postrefcountsame{event}
@postsuccessrefcountretinc
*/
static inline
struct bt_event_class *bt_event_get_class(struct bt_event *event)
{
	return bt_get(bt_event_borrow_class(event));
}

extern struct bt_packet *bt_event_borrow_packet(struct bt_event *event);

/**
@brief	Returns the CTF IR packet associated to the CTF IR event
	\p event.

This function returns a reference to the event class which was set to
\p event in the first place with bt_event_set_packet().

@param[in] event	Event of which to get the associated packet.
@returns		Packet associated to \p event,
			or \c NULL if no packet is associated to
			\p event or on error.

@prenotnull{event}
@postrefcountsame{event}
@postsuccessrefcountretinc

@sa bt_event_set_packet(): Associates a given event to a given
	packet.
*/
static inline
struct bt_packet *bt_event_get_packet(struct bt_event *event)
{
	return bt_get(bt_event_borrow_packet(event));
}

extern struct bt_stream *bt_event_borrow_stream(struct bt_event *event);

/**
@brief	Returns the parent CTF IR stream associated to the CTF IR event
	\p event.

@param[in] event	Event of which to get the parent stream.
@returns		Parent stream of \p event, or \c NULL on error.

@prenotnull{event}
@postrefcountsame{event}
@postsuccessrefcountretinc
*/
static inline
struct bt_stream *bt_event_get_stream(struct bt_event *event)
{
	return bt_get(bt_event_borrow_stream(event));
}

/** @} */

/**
@name Contained fields functions
@{
*/

extern struct bt_field *bt_event_borrow_header(struct bt_event *event);

extern int bt_event_move_header(struct bt_event *event,
		struct bt_event_header_field *header);

extern struct bt_field *bt_event_borrow_stream_event_context(
		struct bt_event *event);

extern struct bt_field *bt_event_borrow_context(struct bt_event *event);

extern struct bt_field *bt_event_borrow_payload(struct bt_event *event);

/** @} */

/**
@name Clock value functions
@{
*/

extern int bt_event_set_clock_value(struct bt_event *event,
		struct bt_clock_class *clock_class, uint64_t raw_value,
		bt_bool is_default);

extern struct bt_clock_value *bt_event_borrow_default_clock_value(
		struct bt_event *event);

/** @} */

/** @} */

#ifdef __cplusplus
}
#endif

#endif /* BABELTRACE_CTF_IR_EVENT_H */
