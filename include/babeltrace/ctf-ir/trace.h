#ifndef BABELTRACE_CTF_IR_TRACE_H
#define BABELTRACE_CTF_IR_TRACE_H

/*
 * BabelTrace - CTF IR: Trace
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

/* For bt_get() */
#include <babeltrace/ref.h>

/* For bt_visitor */
#include <babeltrace/ctf-ir/visitor.h>

/* For bt_bool */
#include <babeltrace/types.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct bt_field_type;
struct bt_value;

/**
@defgroup ctfirtraceclass CTF IR trace class
@ingroup ctfir
@brief CTF IR trace class.

@code
#include <babeltrace/ctf-ir/trace.h>
@endcode

A CTF IR <strong><em>trace class</em></strong> is a descriptor of
traces.

You can obtain a trace class in two different modes:

- <strong>Normal mode</strong>: use bt_trace_create() to create a
  default, empty trace class.
- <strong>CTF writer mode</strong>: use bt_writer_get_trace() to
  get the trace class created by a given CTF writer object.

A trace class has the following properties:

- A \b name.
- A <strong>native byte order</strong>: all the
  \link ctfirfieldtypes field types\endlink eventually part of the trace
  class with a byte order set to #BT_BYTE_ORDER_NATIVE have this
  "real" byte order.
- A \b UUID.
- An \b environment, which is a custom key-value mapping. Keys are
  strings and values can be strings or integers.

In the Babeltrace CTF IR system, a trace class contains zero or more
\link ctfirstreamclass stream classes\endlink, and a stream class
contains zero or more \link ctfireventclass event classes\endlink. You
can add an event class to a stream class with
bt_stream_class_add_event_class(). You can add a stream class to a
trace class with bt_trace_add_stream_class().

You can access the streams of a trace, that is, the streams which were
created from the trace's stream classes with bt_stream_create(),
with bt_trace_get_stream_by_index().

A trace class owns the <strong>trace packet header</strong>
\link ctfirfieldtypes field type\endlink, which represents the
\c trace.packet.header CTF scope. This field type describes the
trace packet header fields of the traces that this trace class
describes.

The trace packet header field type \em must be a structure field type as
of Babeltrace \btversion.

As per the CTF specification, the trace packet header field type \em
must contain a field named \c stream_id if the trace class contains more
than one stream class.

As a reminder, here's the structure of a CTF packet:

@imgpacketstructure

A trace class also contains zero or more
\link ctfirclockclass CTF IR clock classes\endlink.

@todo
Elaborate about clock classes irt clock values.

As with any Babeltrace object, CTF IR trace class objects have
<a href="https://en.wikipedia.org/wiki/Reference_counting">reference
counts</a>. See \ref refs to learn more about the reference counting
management of Babeltrace objects.

The following functions \em freeze their trace class parameter on
success:

- bt_trace_add_stream_class()
- bt_writer_create_stream()
  (\link ctfwriter CTF writer\endlink mode only)

You cannot modify a frozen trace class: it is considered immutable,
except for:

- Adding a stream class to it with
  bt_trace_add_stream_class().
- Adding a CTF IR clock class to it with bt_trace_add_clock_class().
- \link refs Reference counting\endlink.

@sa ctfirstreamclass
@sa ctfireventclass
@sa ctfirclockclass

@file
@brief CTF IR trace class type and functions.
@sa ctfirtraceclass

@addtogroup ctfirtraceclass
@{
*/

/**
@struct bt_trace
@brief A CTF IR trace class.
@sa ctfirtraceclass
*/
struct bt_trace;
struct bt_stream;
struct bt_stream_class;
struct bt_clock_class;

/**
@brief	User function type to use with
	bt_trace_add_is_static_listener().

@param[in] trace_class	Trace class which is now static.
@param[in] data		User data as passed to
			bt_trace_add_is_static_listener() when
			you added the listener.

@prenotnull{trace_class}
*/
typedef void (* bt_trace_is_static_listener)(
	struct bt_trace *trace_class, void *data);

/**
@brief	User function type to use with
	bt_trace_add_is_static_listener().

@param[in] trace_class	Trace class to which the listener was added.
@param[in] data		User data as passed to
			bt_trace_add_is_static_listener() when
			you added the listener.

@prenotnull{trace_class}
*/
typedef void (* bt_trace_listener_removed)(
	struct bt_trace *trace_class, void *data);

/**
@name Creation function
@{
*/

/**
@brief	Creates a default CTF IR trace class.

On success, the trace packet header field type of the created trace
class is an empty structure field type. You can modify this default
trace packet header field type after the trace class is created with
bt_trace_get_packet_header_field_type() and
bt_trace_set_packet_header_field_type().

The created trace class has the following initial properties:

- <strong>Name</strong>: none. You can set a name
  with bt_trace_set_name().
- <strong>UUID</strong>: none. You can set a UUID with
  bt_trace_set_uuid().
- <strong>Native byte order</strong>: #BT_BYTE_ORDER_UNSPECIFIED.
  You can set a native byte order with
  bt_trace_set_native_byte_order().
- <strong>Environment</strong>: empty. You can add environment entries
  with bt_trace_set_environment_field(),
  bt_trace_set_environment_field_integer(), and
  bt_trace_set_environment_field_string().

@returns	Created trace class, or \c NULL on error.

@postsuccessrefcountret1
*/
extern struct bt_trace *bt_trace_create(void);

/** @} */

/**
@name Properties functions
@{
*/

/**
@brief	Returns the name of the CTF IR trace class \p trace_class.

On success, \p trace_class remains the sole owner of the returned
string. The returned string is valid as long as \p trace_class exists
and is not modified.

@param[in] trace_class	Trace class of which to get the name.
@returns		Name of trace class \p trace_class, or
			\c NULL if \p trace_class is unnamed or
			on error.

@prenotnull{trace_class}
@postrefcountsame{trace_class}

@sa bt_trace_set_name(): Sets the name of a given trace class.
*/
extern const char *bt_trace_get_name(struct bt_trace *trace_class);

/**
@brief	Sets the name of the CTF IR trace class \p trace_class
	to \p name.

@param[in] trace_class	Trace class of which to set the name.
@param[in] name		Name of the trace class (copied on success).
@returns		0 on success, or a negative value on error.

@prenotnull{trace_class}
@prenotnull{name}
@prehot{trace_class}
@postrefcountsame{trace_class}

@sa bt_trace_get_name(): Returns the name of a given trace class.
*/
extern int bt_trace_set_name(struct bt_trace *trace_class,
		const char *name);

/**
@brief	Returns the native byte order of the CTF IR trace class
	\p trace_class.

@param[in] trace_class	Trace class of which to get the default byte
			order.
@returns		Native byte order of \p trace_class,
			or #BT_BYTE_ORDER_UNKNOWN on error.

@prenotnull{trace_class}
@postrefcountsame{trace_class}

@sa bt_trace_set_native_byte_order(): Sets the native byte order of
	a given trace class.
*/
extern enum bt_byte_order bt_trace_get_native_byte_order(
		struct bt_trace *trace_class);

/**
@brief	Sets the native byte order of the CTF IR trace class
	\p trace_class to \p native_byte_order.

\p native_byte_order \em must be one of:

- #BT_BYTE_ORDER_LITTLE_ENDIAN
- #BT_BYTE_ORDER_BIG_ENDIAN
- #BT_BYTE_ORDER_NETWORK
- <strong>If the trace is not in CTF writer mode<strong>,
  #BT_BYTE_ORDER_UNSPECIFIED.

@param[in] trace_class		Trace class of which to set the native byte
				order.
@param[in] native_byte_order	Native byte order of the trace class.
@returns			0 on success, or a negative value on error.

@prenotnull{trace_class}
@prehot{trace_class}
@pre \p native_byte_order is either #BT_BYTE_ORDER_UNSPECIFIED (if the
	trace is not in CTF writer mode),
	#BT_BYTE_ORDER_LITTLE_ENDIAN, #BT_BYTE_ORDER_BIG_ENDIAN, or
	#BT_BYTE_ORDER_NETWORK.
@postrefcountsame{trace_class}

@sa bt_trace_get_native_byte_order(): Returns the native byte order of a
	given trace class.
*/
extern int bt_trace_set_native_byte_order(struct bt_trace *trace_class,
		enum bt_byte_order native_byte_order);

/**
@brief	Returns the UUID of the CTF IR trace class \p trace_class.

On success, the return value is an array of 16 bytes.

@param[in] trace_class	Trace class of which to get the UUID.
@returns		UUID of trace class \p trace_class, or
			\c NULL if \p trace_class has no UUID or on error.

@prenotnull{trace_class}
@postrefcountsame{trace_class}

@sa bt_trace_set_uuid(): Sets the UUID of a given trace class.
*/
extern const unsigned char *bt_trace_get_uuid(
		struct bt_trace *trace_class);

/**
@brief  Sets the UUID of the CTF IR trace class \p trace_class to
	\p uuid.

\p uuid \em must be an array of 16 bytes.

@param[in] trace_class		Trace class of which to set the UUID.
@param[in] uuid			UUID of the \p trace_class (copied on
				success).
@returns			0 on success, or a negative value on error.

@prenotnull{trace_class}
@prenotnull{uuid}
@prehot{trace_class}
@pre \p uuid is an array of 16 bytes.
@postrefcountsame{trace_class}

@sa bt_trace_get_uuid(): Returns the UUID of a given trace class.
*/
extern int bt_trace_set_uuid(struct bt_trace *trace_class,
		const unsigned char *uuid);

/**
@brief	Returns the number of entries contained in the environment of
	the CTF IR trace class \p trace_class.

@param[in] trace_class	Trace class of which to get the number
			of environment entries.
@returns		Number of environment entries
			contained in \p trace_class, or
			a negative value on error.

@prenotnull{trace_class}
@postrefcountsame{trace_class}
*/
extern int64_t bt_trace_get_environment_field_count(
		struct bt_trace *trace_class);

/**
@brief	Returns the field name of the environment entry at index
	\p index in the CTF IR trace class \p trace_class.

On success, the returned string is valid as long as this trace class
exists and is \em not modified. \p trace_class remains the sole owner of
the returned string.

@param[in] trace_class	Trace class of which to get the name of the
			environment entry at index \p index.
@param[in] index	Index of environment entry to find.
@returns		Name of the environment entry at index \p index
			in \p trace_class, or \c NULL on error.

@prenotnull{trace_class}
@pre \p index is lesser than the number of environment entries in
	\p trace_class (see bt_trace_get_environment_field_count()).
@postrefcountsame{trace_class}

@sa bt_trace_get_environment_field_value_by_index(): Finds a trace class's
	environment entry by index.
@sa bt_trace_get_environment_field_value_by_name(): Finds a trace
	class's environment entry by name.
@sa bt_trace_set_environment_field(): Sets the value of a trace
	class's environment entry.
*/
extern const char *
bt_trace_get_environment_field_name_by_index(
		struct bt_trace *trace_class, uint64_t index);

extern struct bt_value *
bt_trace_borrow_environment_field_value_by_index(struct bt_trace *trace_class,
		uint64_t index);

/**
@brief	Returns the value of the environment entry at index
	\p index in the CTF IR trace class \p trace_class.

@param[in] trace_class	Trace class of which to get the value of the
			environment entry at index \p index.
@param[in] index	Index of the environment entry to find.
@returns		Value of the environment entry at index \p index
			in \p trace_class, or \c NULL on error.

@prenotnull{trace_class}
@pre \p index is lesser than the number of environment entries in
	\p trace_class (see bt_trace_get_environment_field_count()).
@postrefcountsame{trace_class}
@postsuccessrefcountretinc

@sa bt_trace_get_environment_field_value_by_name(): Finds a trace
	class's environment entry by name.
@sa bt_trace_set_environment_field(): Sets the value of a trace
	class's environment entry.
*/
static inline
struct bt_value *bt_trace_get_environment_field_value_by_index(
		struct bt_trace *trace_class, uint64_t index)
{
	return bt_get(bt_trace_borrow_environment_field_value_by_index(
		trace_class, index));
}

extern struct bt_value *
bt_trace_borrow_environment_field_value_by_name(
		struct bt_trace *trace_class, const char *name);

/**
@brief	Returns the value of the environment entry named \p name
	in the CTF IR trace class \p trace_class.

@param[in] trace_class	Trace class of which to get the value of the
			environment entry named \p name.
@param[in] name		Name of the environment entry to find.
@returns		Value of the environment entry named \p name
			in \p trace_class, or \c NULL if there's no such
			entry or on error.

@prenotnull{trace_class}
@prenotnull{name}
@postrefcountsame{trace_class}
@postsuccessrefcountretinc

@sa bt_trace_get_environment_field_value_by_index(): Finds a trace class's
	environment entry by index.
@sa bt_trace_set_environment_field(): Sets the value of a trace
	class's environment entry.
*/
static inline
struct bt_value *
bt_trace_get_environment_field_value_by_name(
		struct bt_trace *trace_class, const char *name)
{
	return bt_get(
		bt_trace_borrow_environment_field_value_by_name(
			trace_class, name));
}

/**
@brief	Sets the environment entry named \p name in the
	CTF IR trace class \p trace_class to \p value.

If an environment entry named \p name exists in \p trace_class, its
value is first put, and then replaced by \p value.

@param[in] trace_class	Trace class of which to set the environment
			entry.
@param[in] name		Name of the environment entry to set (copied
			on success).
@param[in] value	Value of the environment entry named \p name.
@returns		0 on success, or a negative value on error.

@prenotnull{trace_class}
@prenotnull{name}
@prenotnull{value}
@prehot{trace_class}
@pre \p value is an
	\link bt_value_integer_create() integer value object\endlink
	or a
	\link bt_value_string_create() string value object\endlink.
@postrefcountsame{trace_class}
@postsuccessrefcountinc{value}

@sa bt_trace_get_environment_field_value_by_index(): Finds a trace class's
	environment entry by index.
@sa bt_trace_get_environment_field_value_by_name(): Finds a trace
	class's environment entry by name.
*/
extern int bt_trace_set_environment_field(
		struct bt_trace *trace_class, const char *name,
		struct bt_value *value);

/**
@brief	Sets the environment entry named \p name in the
	CTF IR trace class \p trace_class to \p value.

If an environment entry named \p name exists in \p trace_class, its
value is first put, and then replaced by a new
\link bt_value_integer_create() integer value object\endlink
containing \p value.

@param[in] trace_class	Trace class of which to set the environment
			entry.
@param[in] name		Name of the environment entry to set (copied
			on success).
@param[in] value	Value of the environment entry named \p name.
@returns		0 on success, or a negative value on error.

@prenotnull{trace_class}
@prenotnull{name}
@prehot{trace_class}
@postrefcountsame{trace_class}

@sa bt_trace_set_environment_field(): Sets the value of a trace
	class's environment entry.
*/
extern int bt_trace_set_environment_field_integer(
		struct bt_trace *trace_class, const char *name,
		int64_t value);

/**
@brief	Sets the environment entry named \p name in the
	CTF IR trace class \p trace_class to \p value.

If an environment entry named \p name exists in \p trace_class, its
value is first put, and then replaced by a new
\link bt_value_string_create() string value object\endlink
containing \p value.

@param[in] trace_class	Trace class of which to set the environment
			entry.
@param[in] name		Name of the environment entry to set (copied
			on success).
@param[in] value	Value of the environment entry named \p name
			(copied on success).
@returns		0 on success, or a negative value on error.

@prenotnull{trace_class}
@prenotnull{name}
@prenotnull{value}
@prehot{trace_class}
@postrefcountsame{trace_class}

@sa bt_trace_set_environment_field(): Sets the value of a trace
	class's environment entry.
*/
extern int bt_trace_set_environment_field_string(
		struct bt_trace *trace_class, const char *name,
		const char *value);

/** @} */

/**
@name Contained field types functions
@{
*/

extern struct bt_field_type *bt_trace_borrow_packet_header_field_type(
		struct bt_trace *trace_class);

/**
@brief	Returns the packet header field type of the CTF IR trace class
	\p trace_class.

@param[in] trace_class	Trace class of which to get the packet
			header field type.
@returns		Packet header field type of \p trace_class,
			or \c NULL if \p trace_class has no packet header field
			type or on error.

@prenotnull{trace_class}
@postrefcountsame{trace_class}
@post <strong>On success, if the return value is a field type</strong>, its
	reference count is incremented.

@sa bt_trace_set_packet_header_field_type(): Sets the packet
	header field type of a given trace class.
*/
static inline
struct bt_field_type *bt_trace_get_packet_header_field_type(
		struct bt_trace *trace_class)
{
	return bt_get(bt_trace_borrow_packet_header_field_type(trace_class));
}

/**
@brief	Sets the packet header field type of the CTF IR trace class
	\p trace_class to \p packet_header_type, or unsets the current packet
	header field type from \p trace_class.

If \p packet_header_type is \c NULL, then this function unsets the current
packet header field type from \p trace_class, effectively making \p trace_class
a trace without a packet header field type.

As of Babeltrace \btversion, if \p packet_header_type is not \c NULL,
\p packet_header_type \em must be a CTF IR structure field type object.

@param[in] trace_class		Trace class of which to set the packet
				header field type.
@param[in] packet_header_type	Packet header field type, or \c NULL to unset
				the current packet header field type.
@returns			0 on success, or a negative value on error.

@prenotnull{trace_class}
@prehot{trace_class}
@pre <strong>\p packet_header_type, if not \c NULL</strong>, is a CTF IR
	structure field type.
@postrefcountsame{trace_class}
@post <strong>On success, if \p packet_header_type is not \c NULL</strong>,
	the reference count of \p packet_header_type is incremented.

@sa bt_trace_get_packet_header_field_type(): Returns the packet
	header field type of a given trace class.
*/
extern int bt_trace_set_packet_header_field_type(struct bt_trace *trace_class,
		struct bt_field_type *packet_header_type);

/** @} */

/**
@name Contained clock classes functions
@{
*/

/**
@brief	Returns the number of CTF IR clock classes contained in the
	CTF IR trace class \p trace_class.

@param[in] trace_class	Trace class of which to get the number
			of contained clock classes.
@returns		Number of contained clock classes
			contained in \p trace_class, or a negative
			value on error.

@prenotnull{trace_class}
@postrefcountsame{trace_class}
*/
extern int64_t bt_trace_get_clock_class_count(
		struct bt_trace *trace_class);

extern struct bt_clock_class *bt_trace_borrow_clock_class_by_index(
		struct bt_trace *trace_class, uint64_t index);

/**
@brief  Returns the CTF IR clock class at index \p index in the CTF
	IR trace class \p trace_class.

@param[in] trace_class	Trace class of which to get the clock class
			contained at index \p index.
@param[in] index	Index of the clock class to find in
			\p trace_class.
@returns		Clock class at index \p index in \p trace_class,
			or \c NULL on error.

@prenotnull{trace_class}
@pre \p index is lesser than the number of clock classes contained in
	the trace class \p trace_class (see
	bt_trace_get_clock_class_count()).
@postrefcountsame{trace_class}
@postsuccessrefcountretinc

@sa bt_trace_get_clock_class_by_name(): Finds a clock class by name
	in a given trace class.
@sa bt_trace_add_clock_class(): Adds a clock class to a trace class.
*/
static inline
struct bt_clock_class *bt_trace_get_clock_class_by_index(
		struct bt_trace *trace_class, uint64_t index)
{
	return bt_get(bt_trace_borrow_clock_class_by_index(
		trace_class, index));
}

extern struct bt_clock_class *bt_trace_borrow_clock_class_by_name(
		struct bt_trace *trace_class, const char *name);

/**
@brief  Returns the CTF IR clock class named \c name found in the CTF
	IR trace class \p trace_class.

@param[in] trace_class	Trace class of which to get the clock class
			named \p name.
@param[in] name		Name of the clock class to find in \p trace_class.
@returns		Clock class named \p name in \p trace_class,
			or \c NULL on error.

@prenotnull{trace_class}
@prenotnull{name}
@postrefcountsame{trace_class}
@postsuccessrefcountretinc

@sa bt_trace_get_clock_class_by_index(): Returns the clock class contained
	in a given trace class at a given index.
@sa bt_trace_add_clock_class(): Adds a clock class to a trace class.
*/
static inline
struct bt_clock_class *bt_trace_get_clock_class_by_name(
		struct bt_trace *trace_class, const char *name)
{
	return bt_get(bt_trace_borrow_clock_class_by_name(trace_class, name));
}

/**
@brief	Adds the CTF IR clock class \p clock_class to the CTF IR
	trace class \p trace_class.

On success, \p trace_class contains \p clock_class.

You can call this function even if \p trace_class or \p clock_class
are frozen.

@param[in] trace_class	Trace class to which to add \p clock_class.
@param[in] clock_class	Clock class to add to \p trace_class.
@returns		0 on success, or a negative value on error.

@prenotnull{trace_class}
@prenotnull{clock_class}
@postrefcountsame{trace_class}
@postsuccessrefcountinc{clock_class}
@post <strong>On success, if \p trace_class is frozen</strong>,
	\p clock_class is frozen.

@sa bt_trace_get_clock_class_by_index(): Returns the clock class contained
	in a given trace class at a given index.
@sa bt_trace_get_clock_class_by_name(): Finds a clock class by name
	in a given trace class.
*/
extern int bt_trace_add_clock_class(struct bt_trace *trace_class,
		struct bt_clock_class *clock_class);

/** @} */

/**
@name Stream class children functions
@{
*/

/**
@brief	Returns the number of stream classes contained in the
	CTF IR trace class \p trace_class.

@param[in] trace_class	Trace class of which to get the number
			of children stream classes.
@returns		Number of children stream classes
			contained in \p trace_class, or a negative
			value on error.

@prenotnull{trace_class}
@postrefcountsame{trace_class}
*/
extern int64_t bt_trace_get_stream_class_count(
		struct bt_trace *trace_class);

extern struct bt_stream_class *bt_trace_borrow_stream_class_by_index(
		struct bt_trace *trace_class, uint64_t index);

/**
@brief  Returns the stream class at index \p index in the CTF IR trace
	class \p trace_class.

@param[in] trace_class	Trace class of which to get the stream class.
@param[in] index	Index of the stream class to find.
@returns		Stream class at index \p index, or \c NULL
			on error.

@prenotnull{trace_class}
@pre \p index is lesser than the number of stream classes contained in
	the trace class \p trace_class (see
	bt_trace_get_stream_class_count()).
@postrefcountsame{trace_class}

@sa bt_trace_get_stream_class_by_id(): Finds a stream class by ID.
@sa bt_trace_add_stream_class(): Adds a stream class to a trace class.
*/
static inline
struct bt_stream_class *bt_trace_get_stream_class_by_index(
		struct bt_trace *trace_class, uint64_t index)
{
	return bt_get(bt_trace_borrow_stream_class_by_index(
		trace_class, index));
}

extern struct bt_stream_class *bt_trace_borrow_stream_class_by_id(
		struct bt_trace *trace_class, uint64_t id);

/**
@brief  Returns the stream class with ID \c id found in the CTF IR
	trace class \p trace_class.

@param[in] trace_class	Trace class of which to get the stream class.
@param[in] id		ID of the stream class to find.
@returns		Stream class with ID \p id, or \c NULL
			on error.

@prenotnull{trace_class}
@postrefcountsame{trace_class}
@postsuccessrefcountretinc

@sa bt_trace_get_stream_class_by_index(): Returns the stream class contained
	in a given trace class at a given index.
@sa bt_trace_add_stream_class(): Adds a stream class to a trace class.
*/
static inline
struct bt_stream_class *bt_trace_get_stream_class_by_id(
		struct bt_trace *trace_class, uint64_t id)
{
	return bt_get(bt_trace_borrow_stream_class_by_id(trace_class, id));
}

/**
@brief	Adds the CTF IR stream class \p stream_class to the
	CTF IR trace class \p trace_class.

On success, \p stream_class becomes the child of \p trace_class.

You can only add a given stream class to one trace class.

You can call this function even if \p trace_class is frozen.

This function tries to resolve the needed
\link ctfirfieldtypes CTF IR field type\endlink of the dynamic field
types that are found anywhere in the root field types of
\p stream_class and of all its currently contained
\link ctfireventclass CTF IR event classes\endlink. If any automatic
resolving fails, then this function fails.

@param[in] trace_class	Trace class to which to add \p stream_class.
@param[in] stream_class	Stream class to add to \p trace_class.
@returns		0 on success, or a negative value on error.

@prenotnull{trace_class}
@prenotnull{stream_class}
@postrefcountsame{trace_class}
@postsuccessrefcountinc{stream_class}
@postsuccessfrozen{stream_class}

@sa bt_trace_get_stream_class_by_index(): Returns the stream class contained
	in a given trace class at a given index.
@sa bt_trace_get_stream_class_by_id(): Finds a stream class by ID.
*/
extern int bt_trace_add_stream_class(struct bt_trace *trace_class,
		struct bt_stream_class *stream_class);

/** @} */

/**
@name Stream children functions
@{
*/

/**
@brief  Returns the number of streams contained in the CTF IR trace
	class \p trace_class.

@param[in] trace_class	Trace class of which to get the number
			of children streams.
@returns		Number of children streams
			contained in \p trace_class, or a negative
			value on error.

@prenotnull{trace_class}
@postrefcountsame{trace_class}
*/
extern int64_t bt_trace_get_stream_count(struct bt_trace *trace_class);

extern struct bt_stream *bt_trace_borrow_stream_by_index(
		struct bt_trace *trace_class, uint64_t index);

/**
@brief  Returns the stream at index \p index in the CTF IR trace
	class \p trace_class.

@param[in] trace_class	Trace class of which to get the stream.
@param[in] index	Index of the stream to find.
@returns		Stream at index \p index, or \c NULL
			on error.

@prenotnull{trace_class}
@pre \p index is lesser than the number of streams contained in
	the trace class \p trace_class (see
	bt_trace_get_stream_count()).
@postrefcountsame{trace_class}
*/
static inline
struct bt_stream *bt_trace_get_stream_by_index(
		struct bt_trace *trace_class, uint64_t index)
{
	return bt_get(bt_trace_borrow_stream_by_index(trace_class, index));
}

/** @} */

/**
@name Misc. functions
@{
*/

/**
@brief	Returns whether or not the CTF IR trace class \p trace_class
	is static.

It is guaranteed that a static trace class will never contain new
streams, stream classes, or clock classes. A static class is always
frozen.

This function returns #BT_TRUE if bt_trace_set_is_static() was
previously called on it.

@param[in] trace_class	Trace class to check.
@returns		#BT_TRUE if \p trace_class is static,

@sa bt_trace_set_is_static(): Makes a trace class static.
*/
extern bt_bool bt_trace_is_static(struct bt_trace *trace_class);

/**
@brief	Makes the CTF IR trace class \p trace_class static.

A static trace class is frozen and you cannot call any modifying
function on it:

- bt_trace_add_stream_class()
- bt_trace_add_clock_class()
- bt_trace_set_environment_field()
- bt_trace_set_environment_field_integer()
- bt_trace_set_environment_field_string()
- bt_trace_add_is_static_listener()

You cannot create a stream with bt_stream_create() with any of the
stream classes of a static trace class.

@param[in] trace_class	Trace class to make static.
@returns		0 on success, or a negative value on error.

@prenotnull{trace_class}
@postrefcountsame{trace_class}
@postsuccessfrozen{trace_class}

@sa bt_trace_is_static(): Checks whether or not a given trace class
	is static.
@sa bt_trace_add_is_static_listener(): Adds a listener to a trace
	class which is called when the trace class is made static.
*/
extern int bt_trace_set_is_static(struct bt_trace *trace_class);

/**
@brief  Adds the listener \p listener to the CTF IR trace class
	\p trace_class which is called when the trace is made static.

\p listener is called with \p data, the user data, the first time
bt_trace_set_is_static() is called on \p trace_class.

When the trace is destroyed, or when you remove the added listener with
bt_trace_remove_is_static_listener(), \p listener_removed is called
if it's not \c NULL. You can use \p listener_removed to free any dynamic
data which exists only for the added listener. You cannot call
any function which modifies \p trace_class during the execution of
\p listener_removed, including bt_trace_remove_is_static_listener().

This function fails if \p trace_class is already static: you need to
check the condition first with bt_trace_is_static().

On success, this function returns a unique numeric identifier for this
listener within \p trace. You can use this identifier to remove the
specific listener you added with
bt_trace_remove_is_static_listener().

@param[in] trace_class		Trace class to which to add the
				listener.
@param[in] listener		Listener to add to \p trace_class.
@param[in] listener_removed	Remove listener called when \p listener
				is removed from \p trace_class, or
				\c NULL if you don't need a remove
				listener.
@param[in] data			User data passed when \p listener or
				\p listener_removed is called.
@returns			A unique numeric identifier for this
				listener on success (0 or greater), or a
				negative value on error.

@prenotnull{trace_class}
@prenotnull{listener}
@pre \p trace_class is not static.
@postrefcountsame{trace_class}

@sa bt_trace_remove_is_static_listener(): Removes a "trace is
	static" listener from a trace class previously added with this
	function.
@sa bt_trace_is_static(): Checks whether or not a given trace class
	is static.
@sa bt_trace_set_is_static(): Makes a trace class static.
*/
extern int bt_trace_add_is_static_listener(
		struct bt_trace *trace_class,
		bt_trace_is_static_listener listener,
		bt_trace_listener_removed listener_removed, void *data);

/**
@brief  Removes the "trace is static" listener identified by
	\p listener_id from the trace class \p trace_class.

@param[in] trace_class	Trace class from which to remove the listener
			identified by \p listener_id.
@param[in] listener_id	Identifier of the listener to remove from
			\p trace_class.
@returns		0 if this function removed the listener, or
			a negative value on error.

@prenotnull{trace_class}
@pre \p listener_id is the identifier of a listener that you previously
	added with bt_trace_add_is_static_listener() and did not
	already remove with this function.
@postrefcountsame{trace_class}

@sa bt_trace_add_is_static_listener(): Adds a listener to a trace
	class which is called when the trace class is made static.
*/
extern int bt_trace_remove_is_static_listener(
		struct bt_trace *trace_class, int listener_id);

/**
@brief	Accepts the visitor \p visitor to visit the hierarchy of the
	CTF IR trace class \p trace_class.

This function traverses the hierarchy of \p trace_class in pre-order
and calls \p visitor on each element.

The trace class itself is visited first, then, for each children stream
class, the stream class itself, and all its children event classes.

@param[in] trace_class	Trace class to visit.
@param[in] visitor	Visiting function.
@param[in] data		User data.
@returns		0 on success, or a negative value on error.

@prenotnull{trace_class}
@prenotnull{visitor}
*/
extern int bt_trace_visit(struct bt_trace *trace_class,
		bt_visitor visitor, void *data);

/** @} */

/** @} */

#ifdef __cplusplus
}
#endif

#endif /* BABELTRACE_CTF_IR_TRACE_H */
