#ifndef BABELTRACE_CTF_IR_STREAM_CLASS_H
#define BABELTRACE_CTF_IR_STREAM_CLASS_H

/*
 * BabelTrace - CTF IR: Stream Class
 *
 * Copyright 2014 Jérémie Galarneau <jeremie.galarneau@efficios.com>
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

#include <stdint.h>

/* For bt_get() */
#include <babeltrace/ref.h>

/* For bt_visitor */
#include <babeltrace/ctf-ir/visitor.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
@defgroup ctfirstreamclass CTF IR stream class
@ingroup ctfir
@brief CTF IR stream class.

@code
#include <babeltrace/ctf-ir/stream-class.h>
@endcode

@note
See \ref ctfwriterstreamclass which documents additional CTF IR stream
class functions exclusive to the CTF writer mode.

A CTF IR <strong><em>stream class</em></strong> is a template that you
can use to create concrete \link ctfirstream CTF IR streams\endlink.

A stream class has the following properties, both of which \em must
be unique amongst all the stream classes contained in the same
\link ctfirtraceclass CTF IR trace class\endlink:

- A \b name.
- A numeric \b ID.

In the Babeltrace CTF IR system, a \link ctfirtraceclass trace class\endlink
contains zero or more stream classes,
and a stream class contains zero or more
\link ctfireventclass event classes\endlink.
You can add an event class
to a stream class with bt_stream_class_add_event_class().
You can add a stream class to a trace class with
bt_trace_add_stream_class().

A stream class owns three \link ctfirfieldtypes field types\endlink:

- An optional <strong>stream packet context</strong> field type, which
  represents the \c stream.packet.context CTF scope.
- An optional <strong>stream event header</strong> field type, which
  represents the \c stream.event.header CTF scope.
- An optional <strong>stream event context</strong> field type, which
  represents the \c stream.event.context CTF scope.

Those three field types \em must be structure field types as of
Babeltrace \btversion.

As per the CTF specification, the event header field type \em must
contain a field named \c id if the stream class contains more than one
event class.

As a reminder, here's the structure of a CTF packet:

@imgpacketstructure

Before you can create a stream from a stream class with
bt_stream_create(), you \em must add the prepared stream class to a
trace class by calling bt_trace_add_stream_class().

As with any Babeltrace object, CTF IR stream class objects have
<a href="https://en.wikipedia.org/wiki/Reference_counting">reference
counts</a>. See \ref refs to learn more about the reference counting
management of Babeltrace objects.

The following functions \em freeze their stream class parameter on
success:

- bt_trace_add_stream_class()
- bt_event_create()
- bt_writer_create_stream()
  (\link ctfwriter CTF writer\endlink mode only)

You cannot modify a frozen stream class: it is considered immutable,
except for:

- Adding an event class to it with
  bt_stream_class_add_event_class(). If the stream class's parent
  \link ctfirtraceclass trace class\endlink is static, however,
  you cannot call bt_stream_class_add_event_class()
  (see bt_trace_is_static() and bt_trace_set_is_static()).
- \link refs Reference counting\endlink.

@sa ctfirstream
@sa ctfireventclass
@sa ctfirtraceclass
@sa ctfwriterstreamclass

@file
@brief CTF IR stream class type and functions.
@sa ctfirstreamclass

@addtogroup ctfirstreamclass
@{
*/

/**
@struct bt_stream_class
@brief A CTF IR stream class.
@sa ctfirstreamclass
*/
struct bt_stream_class;
struct bt_event_class;
struct bt_clock;

/**
@name Creation and parent access functions
@{
*/

/**
@brief	Creates a default CTF IR stream class named \p name­, or a
	default unnamed stream class if \p name is \c NULL.

On success, the packet context field type of the created stream class
has the following fields:

- <code>timestamp_begin</code>: a 64-bit unsigned integer field type.
- <code>timestamp_end</code>: a 64-bit unsigned integer field type.
- <code>content_size</code>: a 64-bit unsigned integer field type.
- <code>packet_size</code>: a 64-bit unsigned integer field type.
- <code>events_discarded</code>: a 64-bit unsigned integer field type.

On success, the event header field type of the created stream class
has the following fields:

- <code>code</code>: a 32-bit unsigned integer field type.
- <code>timestamp</code>: a 64-bit unsigned integer field type.

You can modify those default field types after the stream class is
created with bt_stream_class_set_packet_context_field_type() and
bt_stream_class_set_event_header_field_type().

@param[in] name	Name of the stream class to create (copied on success),
		or \c NULL to create an unnamed stream class.
@returns	Created default stream class, or \c NULL on error.

@postsuccessrefcountret1

@sa bt_stream_class_create_empty(): Creates an empty stream class.
*/
extern struct bt_stream_class *bt_stream_class_create(const char *name);

extern struct bt_trace *bt_stream_class_borrow_trace(
		struct bt_stream_class *stream_class);

/**
@brief	Returns the parent CTF IR trace class of the CTF IR stream
	class \p stream_class.

It is possible that the stream class was not added to a trace class
yet, in which case this function returns \c NULL. You can add a
stream class to a trace class with
bt_trace_add_stream_class().

@param[in] stream_class	Stream class of which to get the parent
			trace class.
@returns		Parent trace class of \p stream_class,
			or \c NULL if \p stream_class was not
			added to a trace class yet or on error.

@prenotnull{stream_class}
@postrefcountsame{stream_class}
@postsuccessrefcountretinc

@sa bt_trace_add_stream_class(): Add a stream class to
	a trace class.
*/
static inline
struct bt_trace *bt_stream_class_get_trace(
		struct bt_stream_class *stream_class)
{
	return bt_get(bt_stream_class_borrow_trace(stream_class));
}

/** @} */

/**
@name Properties functions
@{
*/

/**
@brief	Returns the name of the CTF IR stream class \p stream_class.

On success, \p stream_class remains the sole owner of the returned
string.

@param[in] stream_class	Stream class of which to get the name.
@returns		Name of stream class \p stream_class, or
			\c NULL if \p stream_class is unnamed or
			on error.

@prenotnull{stream_class}
@postrefcountsame{stream_class}

@sa bt_stream_class_set_name(): Sets the name of a given
	stream class.
*/
extern const char *bt_stream_class_get_name(
		struct bt_stream_class *stream_class);

/**
@brief	Sets the name of the CTF IR stream class
	\p stream_class to \p name, or resets the name of
	\p stream_class.

If \p name is not \c NULL, it must be unique amongst the names of all
the stream classes of the trace class to which you eventually add
\p stream_class.

@param[in] stream_class	Stream class of which to set the name.
@param[in] name		Name of the stream class (copied on success), or
			\c NULL to reset the name of \p stream_class
			(make it unnamed).
@returns		0 on success, or a negative value on error.

@prenotnull{stream_class}
@prehot{stream_class}
@postrefcountsame{stream_class}

@sa bt_stream_class_get_name(): Returns the name of a given
	stream class.
*/
extern int bt_stream_class_set_name(
		struct bt_stream_class *stream_class, const char *name);

/**
@brief	Returns the numeric ID of the CTF IR stream class \p stream_class.

@param[in] stream_class	Stream class of which to get the numeric ID.
@returns		ID of stream class \p stream_class, or a
			negative value on error.

@prenotnull{stream_class}
@postrefcountsame{stream_class}

@sa bt_stream_class_set_id(): Sets the numeric ID of a given
	stream class.
*/
extern int64_t bt_stream_class_get_id(
		struct bt_stream_class *stream_class);

/**
@brief	Sets the numeric ID of the CTF IR stream class
	\p stream_class to \p id.

\p id must be unique amongst the IDs of all the stream classes
of the trace class to which you eventually add \p stream_class.

@param[in] stream_class	Stream class of which to set the numeric ID.
@param[in] id		ID of the stream class.
@returns		0 on success, or a negative value on error.

@prenotnull{stream_class}
@prehot{stream_class}
@pre \p id is lesser than or equal to 9223372036854775807 (\c INT64_MAX).
@postrefcountsame{stream_class}

@sa bt_stream_class_get_id(): Returns the numeric ID of a given
	stream class.
*/
extern int bt_stream_class_set_id(
		struct bt_stream_class *stream_class, uint64_t id);

/** @} */

/**
@name Contained field types functions
@{
*/

extern struct bt_field_type *bt_stream_class_borrow_packet_context_field_type(
		struct bt_stream_class *stream_class);

/**
@brief	Returns the packet context field type of the CTF IR stream class
	\p stream_class.

@param[in] stream_class	Stream class of which to get the packet
			context field type.
@returns		Packet context field type of \p stream_class,
			or \c NULL if \p stream_class has no packet context
			field type or on error.

@prenotnull{stream_class}
@postrefcountsame{stream_class}
@post <strong>On success, if the return value is a field type</strong>, its
	reference count is incremented.

@sa bt_stream_class_set_packet_context_field_type(): Sets the packet
	context field type of a given stream class.
*/
static inline
struct bt_field_type *bt_stream_class_get_packet_context_field_type(
		struct bt_stream_class *stream_class)
{
	return bt_get(bt_stream_class_borrow_packet_context_field_type(
		stream_class));
}

/**
@brief	Sets the packet context field type of the CTF IR stream class
	\p stream_class to \p packet_context_type, or unsets the current packet
	context field type from \p stream_class.

If \p packet_context_type is \c NULL, then this function unsets the current
packet context field type from \p stream_class, effectively making
\p stream_class a stream class without a packet context field type.

As of Babeltrace \btversion, if \p packet_context_type is not \c NULL,
\p packet_context_type \em must be a CTF IR structure field type object.

@param[in] stream_class		Stream class of which to set the packet
				context field type.
@param[in] packet_context_type	Packet context field type, or \c NULL to unset
				the current packet context field type.
@returns			0 on success, or a negative value on error.

@prenotnull{stream_class}
@prehot{stream_class}
@pre <strong>\p packet_context_type, if not \c NULL</strong>, is a CTF IR
	structure field type.
@postrefcountsame{stream_class}
@post <strong>On success, if \p packet_context_type is not \c NULL</strong>,
	the reference count of \p packet_context_type is incremented.

@sa bt_stream_class_get_packet_context_field_type(): Returns the packet
	context field type of a given stream class.
*/
extern int bt_stream_class_set_packet_context_field_type(
		struct bt_stream_class *stream_class,
		struct bt_field_type *packet_context_type);

extern struct bt_field_type *
bt_stream_class_borrow_event_header_field_type(
		struct bt_stream_class *stream_class);

/**
@brief	Returns the event header field type of the CTF IR stream class
	\p stream_class.

@param[in] stream_class	Stream class of which to get the event header
			field type.
@returns		Event header field type of \p stream_class,
			or \c NULL if \p stream_class has no event header field
			type or on error.

@prenotnull{stream_class}
@postrefcountsame{stream_class}
@post <strong>On success, if the return value is a field type</strong>, its
	reference count is incremented.

@sa bt_stream_class_set_event_header_field_type(): Sets the event
	header field type of a given stream class.
*/
static inline
struct bt_field_type *bt_stream_class_get_event_header_field_type(
		struct bt_stream_class *stream_class)
{
	return bt_get(bt_stream_class_borrow_event_header_field_type(
		stream_class));
}

/**
@brief	Sets the event header field type of the CTF IR stream class
	\p stream_class to \p event_header_type, or unsets the current event
	header field type from \p stream_class.

If \p event_header_type is \c NULL, then this function unsets the current
event header field type from \p stream_class, effectively making \p stream_class
a stream class without a event header field type.

As of Babeltrace \btversion, if \p event_header_type is not \c NULL,
\p event_header_type \em must be a CTF IR structure field type object.

@param[in] stream_class		Stream class of which to set the event
				header field type.
@param[in] event_header_type	Event header field type, or \c NULL to unset
				the current event header field type.
@returns			0 on success, or a negative value on error.

@prenotnull{stream_class}
@prehot{stream_class}
@pre <strong>\p event_header_type, if not \c NULL</strong>, is a CTF IR
	structure field type.
@postrefcountsame{stream_class}
@post <strong>On success, if \p event_header_type is not \c NULL</strong>,
	the reference count of \p event_header_type is incremented.

@sa bt_stream_class_get_event_header_field_type(): Returns the event
	header field type of a given stream class.
*/
extern int bt_stream_class_set_event_header_field_type(
		struct bt_stream_class *stream_class,
		struct bt_field_type *event_header_type);

extern struct bt_field_type *
bt_stream_class_borrow_event_context_field_type(
		struct bt_stream_class *stream_class);

/**
@brief	Returns the event context field type of the CTF IR stream class
	\p stream_class.

@param[in] stream_class	Stream class of which to get the event context
			field type.
@returns		Event context field type of \p stream_class,
			or \c NULL if \p stream_class has no event context field
			type or on error.

@prenotnull{stream_class}
@postrefcountsame{stream_class}
@post <strong>On success, if the return value is a field type</strong>,
	its reference count is incremented.


@sa bt_stream_class_set_event_context_field_type(): Sets the event
	context field type of a given stream class.
*/
static inline
struct bt_field_type *
bt_stream_class_get_event_context_field_type(
		struct bt_stream_class *stream_class)
{
	return bt_get(bt_stream_class_borrow_event_context_field_type(
		stream_class));
}

/**
@brief	Sets the event context field type of the CTF IR stream class
	\p stream_class to \p event_context_type, or unsets the current event
	context field type from \p stream_class.

If \p event_context_type is \c NULL, then this function unsets the current
event context field type from \p stream_class, effectively making \p
stream_class a stream class without a event context field type.

As of Babeltrace \btversion, if \p event_context_type is not \c NULL,
\p event_context_type \em must be a CTF IR structure field type object.

@param[in] stream_class		Stream class of which to set the packet
				context field type.
@param[in] event_context_type	Event context field type, or \c NULL to unset
				the current event context field type.
@returns			0 on success, or a negative value on error.

@prenotnull{stream_class}
@prehot{stream_class}
@pre <strong>\p event_context_type, if not \c NULL</strong>, is a CTF IR
	structure field type.
@postrefcountsame{stream_class}
@post <strong>On success, if \p event_context_type is not \c NULL</strong>,
	the reference count of \p event_context_type is incremented.

@sa bt_stream_class_get_event_context_field_type(): Returns the event context
	field type of a given stream class.
*/
extern int bt_stream_class_set_event_context_field_type(
		struct bt_stream_class *stream_class,
		struct bt_field_type *event_context_type);

/** @} */

/**
@name Event class children functions
@{
*/

/**
@brief	Returns the number of event classes contained in the
	CTF IR stream class \p stream_class.

@param[in] stream_class	Stream class of which to get the number
			of children event classes.
@returns		Number of children event classes
			contained in \p stream_class, or
			a negative value on error.

@prenotnull{stream_class}
@postrefcountsame{stream_class}
*/
extern int64_t bt_stream_class_get_event_class_count(
		struct bt_stream_class *stream_class);

extern struct bt_event_class *bt_stream_class_borrow_event_class_by_index(
		struct bt_stream_class *stream_class, uint64_t index);

/**
@brief  Returns the event class at index \p index in the CTF IR stream
	class \p stream_class.

@param[in] stream_class	Stream class of which to get the event class.
@param[in] index	Index of the event class to find.
@returns		Event class at index \p index, or \c NULL
			on error.

@prenotnull{stream_class}
@pre \p index is lesser than the number of event classes contained in the
	stream class \p stream_class (see
	bt_stream_class_get_event_class_count()).
@postrefcountsame{stream_class}
@postsuccessrefcountretinc

@sa bt_stream_class_get_event_class_by_id(): Finds an event class
	by ID.
*/
static inline
struct bt_event_class *bt_stream_class_get_event_class_by_index(
		struct bt_stream_class *stream_class, uint64_t index)
{
	return bt_get(bt_stream_class_borrow_event_class_by_index(stream_class,
		index));
}

extern struct bt_event_class *bt_stream_class_borrow_event_class_by_id(
		struct bt_stream_class *stream_class, uint64_t id);

/**
@brief  Returns the event class with ID \c id found in the CTF IR stream
	class \p stream_class.

@param[in] stream_class	Stream class of which to get the event class.
@param[in] id		ID of the event class to find.
@returns		Event class with ID \p id, or \c NULL
			on error.

@prenotnull{stream_class}
@postrefcountsame{stream_class}
@postsuccessrefcountretinc
*/
static inline
struct bt_event_class *bt_stream_class_get_event_class_by_id(
		struct bt_stream_class *stream_class, uint64_t id)
{
	return bt_get(bt_stream_class_borrow_event_class_by_id(stream_class,
		id));
}

/**
@brief	Adds the CTF IR event class \p event_class to the
	CTF IR stream class \p stream_class.

On success, \p event_class becomes the child of \p stream_class.

You can only add a given event class to one stream class.

You can call this function even if \p stream_class is frozen. Adding
event classes is the only operation that is permitted
on a frozen stream class.

This function tries to resolve the needed
\link ctfirfieldtypes CTF IR field type\endlink of the dynamic field
types that are found anywhere in the context or payload field
types of \p event_class. If any automatic resolving fails:

- If the needed field type should be found in one of the root field
  types of \p event_class or \p stream_class, this function fails.
- If \p stream_class is the child of a
  \link ctfirtraceclass CTF IR trace class\endlink (it was added
  with bt_trace_add_stream_class()), this function fails.
- If \p stream_class is not the child of a trace class yet, the
  automatic resolving is reported to the next call to
  bt_trace_add_stream_class() with \p stream_class.

@param[in] stream_class	Stream class to which to add \p event_class.
@param[in] event_class	Event class to add to \p stream_class.
@returns		0 on success, or a negative value on error.

@prenotnull{stream_class}
@prenotnull{event_class}
@prehot{event_class}
@postrefcountsame{stream_class}
@postsuccessrefcountinc{event_class}
@postsuccessfrozen{event_class}
*/
extern int bt_stream_class_add_event_class(
		struct bt_stream_class *stream_class,
		struct bt_event_class *event_class);

/** @} */

/**
@name Misc. function
@{
*/

/**
@brief	Accepts the visitor \p visitor to visit the hierarchy of the
	CTF IR stream class \p stream_class.

This function traverses the hierarchy of \p stream_class in pre-order
and calls \p visitor on each element.

The stream class itself is visited first, and then all its children
event classes.

@param[in] stream_class	Stream class to visit.
@param[in] visitor	Visiting function.
@param[in] data		User data.
@returns		0 on success, or a negative value on error.

@prenotnull{stream_class}
@prenotnull{visitor}
*/
extern int bt_stream_class_visit(struct bt_stream_class *stream_class,
		bt_visitor visitor, void *data);

/** @} */

/** @} */

#ifdef __cplusplus
}
#endif

#endif /* BABELTRACE_CTF_IR_STREAM_CLASS_H */
