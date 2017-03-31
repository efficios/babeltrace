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

#include <babeltrace/ctf-ir/field-types.h>
#include <babeltrace/ctf-ir/visitor.h>
#include <babeltrace/values.h>
#include <babeltrace/graph/notification.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

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

- <strong>Normal mode</strong>: use bt_ctf_trace_create() to create a
  default, empty trace class.
- <strong>CTF writer mode</strong>: use bt_ctf_writer_get_trace() to
  get the trace class created by a given CTF writer object.

A trace class has the following properties:

- A \b name.
- A <strong>default byte order</strong>: all the
  \link ctfirfieldtypes field types\endlink eventually part of the trace
  class with a byte order set to #BT_CTF_BYTE_ORDER_NATIVE have this
  "real" byte order.
- An \b environment, which is a custom key-value mapping. Keys are
  strings and values can be strings or integers.

In the Babeltrace CTF IR system, a trace class contains zero or more
\link ctfirstreamclass stream classes\endlink, and a stream class
contains zero or more \link ctfireventclass event classes\endlink. You
can add an event class to a stream class with
bt_ctf_stream_class_add_event_class(). You can add a stream class to a
trace class with bt_ctf_trace_add_stream_class().

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
\link ctfirclockclass clock classes\endlink.

@todo
Elaborate about clock classes irt clock values.

As with any Babeltrace object, CTF IR trace class objects have
<a href="https://en.wikipedia.org/wiki/Reference_counting">reference
counts</a>. See \ref refs to learn more about the reference counting
management of Babeltrace objects.

The following functions \em freeze their trace class parameter on
success:

- bt_ctf_trace_add_stream_class()
- bt_ctf_writer_create_stream()
  (\link ctfwriter CTF writer\endlink mode only)

You cannot modify a frozen trace class: it is considered immutable,
except for:

- Adding a stream class to it with
  bt_ctf_trace_add_stream_class().
- Adding a clock class to it with bt_ctf_trace_add_clock_class().
- \link refs Reference counting\endlink.

You can add a custom listener with bt_ctf_trace_add_listener() to get
notified if anything changes in a trace class, that is, if an event
class is added to one of its stream class, if a stream class is added,
or if a clock class is added.

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
@struct bt_ctf_trace
@brief A CTF IR trace class.
@sa ctfirtraceclass
*/
struct bt_ctf_trace;
struct bt_ctf_stream;
struct bt_ctf_stream_class;
struct bt_ctf_clock_class;

/**
@name Creation function
@{
*/

/**
@brief	Creates a default CTF IR trace class.

On success, the trace packet header field type of the created trace
class has the following fields:

- <code>magic</code>: a 32-bit unsigned integer field type.
- <code>uuid</code>: an array field type of 16 8-bit unsigned integer
  field types.
- <code>stream_id</code>: a 32-bit unsigned integer field type.

You can modify this default trace packet header field type after the
trace class is created with bt_ctf_trace_set_packet_header_type().

The created trace class has the following initial properties:

- <strong>Name</strong>: none. You can set a name
  with bt_ctf_trace_set_name().
- <strong>Default byte order</strong>: #BT_CTF_BYTE_ORDER_NATIVE. You
  can set a default byte order with bt_ctf_trace_set_byte_order().

  Note that you \em must set the default byte order if any field type
  contained in the created trace class, in its stream classes, or in
  its event classes, has a byte order set to #BT_CTF_BYTE_ORDER_NATIVE.
- <strong>Environment</strong>: empty. You can add environment entries
  with bt_ctf_trace_set_environment_field(),
  bt_ctf_trace_set_environment_field_integer(), and
  bt_ctf_trace_set_environment_field_string().

@returns	Created trace class, or \c NULL on error.

@postsuccessrefcountret1
*/
extern struct bt_ctf_trace *bt_ctf_trace_create(void);

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

@sa bt_ctf_trace_set_name(): Sets the name of a given trace class.
*/
extern const char *bt_ctf_trace_get_name(struct bt_ctf_trace *trace_class);

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

@sa bt_ctf_trace_get_name(): Returns the name of a given trace class.
*/
extern int bt_ctf_trace_set_name(struct bt_ctf_trace *trace_class,
		const char *name);

/**
@brief	Returns the default byte order of the CTF IR trace class
	\p trace_class.

@param[in] trace_class	Trace class of which to get the default byte
			order.
@returns		Default byte order of trace class
			\p trace_class, or #BT_CTF_BYTE_ORDER_UNKNOWN
			on error.

@prenotnull{trace_class}
@postrefcountsame{trace_class}

@sa bt_ctf_trace_set_name(): Sets the name of a given trace class.
*/
extern enum bt_ctf_byte_order bt_ctf_trace_get_byte_order(
		struct bt_ctf_trace *trace_class);

/**
@brief	Sets the default byte order of the CTF IR trace class
	\p trace_class to \p name.

\p byte_order \em must be one of:

- #BT_CTF_BYTE_ORDER_LITTLE_ENDIAN
- #BT_CTF_BYTE_ORDER_BIG_ENDIAN
- #BT_CTF_BYTE_ORDER_NETWORK

@param[in] trace_class	Trace class of which to set the default byte
			order.
@param[in] byte_order	Default byte order of the trace class.
@returns		0 on success, or a negative value on error.

@prenotnull{trace_class}
@prenotnull{name}
@prehot{trace_class}
@pre \p byte_order is either #BT_CTF_BYTE_ORDER_LITTLE_ENDIAN,
	#BT_CTF_BYTE_ORDER_BIG_ENDIAN, or
	#BT_CTF_BYTE_ORDER_NETWORK.
@postrefcountsame{trace_class}

@sa bt_ctf_trace_get_name(): Returns the name of a given trace class.
*/
extern int bt_ctf_trace_set_byte_order(struct bt_ctf_trace *trace_class,
		enum bt_ctf_byte_order byte_order);

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
extern int bt_ctf_trace_get_environment_field_count(
		struct bt_ctf_trace *trace_class);

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
	\p trace_class (see bt_ctf_trace_get_environment_field_count()).
@postrefcountsame{trace_class}

@sa bt_ctf_trace_get_environment_field_value(): Finds a trace class's
	environment entry by index.
@sa bt_ctf_trace_get_environment_field_value_by_name(): Finds a trace
	class's environment entry by name.
@sa bt_ctf_trace_set_environment_field(): Sets the value of a trace
	class's environment entry.
*/
extern const char *
bt_ctf_trace_get_environment_field_name(struct bt_ctf_trace *trace_class,
		int index);

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
	\p trace_class (see bt_ctf_trace_get_environment_field_count()).
@postrefcountsame{trace_class}
@postsuccessrefcountretinc

@sa bt_ctf_trace_get_environment_field_value_by_name(): Finds a trace
	class's environment entry by name.
@sa bt_ctf_trace_set_environment_field(): Sets the value of a trace
	class's environment entry.
*/
extern struct bt_value *
bt_ctf_trace_get_environment_field_value(struct bt_ctf_trace *trace_class,
		int index);

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

@sa bt_ctf_trace_get_environment_field_value(): Finds a trace class's
	environment entry by index.
@sa bt_ctf_trace_set_environment_field(): Sets the value of a trace
	class's environment entry.
*/
extern struct bt_value *
bt_ctf_trace_get_environment_field_value_by_name(
		struct bt_ctf_trace *trace_class, const char *name);

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

@sa bt_ctf_trace_get_environment_field_value(): Finds a trace class's
	environment entry by index.
@sa bt_ctf_trace_get_environment_field_value_by_name(): Finds a trace
	class's environment entry by name.
*/
extern int bt_ctf_trace_set_environment_field(
		struct bt_ctf_trace *trace_class, const char *name,
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

@sa bt_ctf_trace_set_environment_field(): Sets the value of a trace
	class's environment entry.
*/
extern int bt_ctf_trace_set_environment_field_integer(
		struct bt_ctf_trace *trace_class, const char *name,
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

@sa bt_ctf_trace_set_environment_field(): Sets the value of a trace
	class's environment entry.
*/
extern int bt_ctf_trace_set_environment_field_string(
		struct bt_ctf_trace *trace_class, const char *name,
		const char *value);

/** @} */

/**
@name Contained field types functions
@{
*/

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

@sa bt_ctf_trace_set_packet_header_type(): Sets the packet
	header field type of a given trace class.
*/
extern struct bt_ctf_field_type *bt_ctf_trace_get_packet_header_type(
		struct bt_ctf_trace *trace_class);

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

@sa bt_ctf_trace_get_packet_header_type(): Returns the packet
	header field type of a given trace class.
*/
extern int bt_ctf_trace_set_packet_header_type(struct bt_ctf_trace *trace_class,
		struct bt_ctf_field_type *packet_header_type);

/** @} */

/**
@name Clock class children functions
@{
*/

/**
@brief	Returns the number of clock classes contained in the
	CTF IR trace class \p trace_class.

@param[in] trace_class	Trace class of which to get the number
			of children clock classes.
@returns		Number of children clock classes
			contained in \p trace_class, or a negative
			value on error.

@prenotnull{trace_class}
@postrefcountsame{trace_class}
*/
extern int bt_ctf_trace_get_clock_class_count(struct bt_ctf_trace *trace_class);

/**
@brief  Returns the clock class at index \p index in the CTF IR trace
	class \p trace_class.

@param[in] trace_class	Trace class of which to get the clock class.
@param[in] index	Index of the clock class to find.
@returns		Clock class at index \p index, or \c NULL
			on error.

@prenotnull{trace_class}
@pre \p index is lesser than the number of clock classes contained in
	the trace class \p trace_class (see
	bt_ctf_trace_get_clock_class_count()).
@postrefcountsame{trace_class}
@postsuccessrefcountretinc

@sa bt_ctf_trace_get_clock_class_by_name(): Finds a clock class by name.
@sa bt_ctf_trace_add_clock_class(): Adds a clock class to a trace class.
*/
extern struct bt_ctf_clock_class *bt_ctf_trace_get_clock_class(
		struct bt_ctf_trace *trace_class, int index);

/**
@brief  Returns the clock class named \c name found in the CTF IR trace
	class \p trace_class.

@param[in] trace_class	Trace class of which to get the clock class.
@param[in] name		Name of the clock class to find.
@returns		Clock class named \p name, or \c NULL
			on error.

@prenotnull{trace_class}
@prenotnull{name}
@postrefcountsame{trace_class}
@postsuccessrefcountretinc

@sa bt_ctf_trace_get_clock_class(): Returns the clock class contained
	in a given trace class at a given index.
@sa bt_ctf_trace_add_clock_class(): Adds a clock class to a trace class.
*/
extern struct bt_ctf_clock_class *bt_ctf_trace_get_clock_class_by_name(
		struct bt_ctf_trace *trace_class, const char *name);

/**
@brief	Adds the CTF IR clock class \p clock_class to the CTF IR
	trace class \p trace_class.

On success, \p clock_class becomes the child of \p trace_class.

You can call this function even if \p trace_class is frozen.

@param[in] trace_class	Trace class to which to add \p clock_class.
@param[in] clock_class	Clock class to add to \p trace_class.
@returns		0 on success, or a negative value on error.

@prenotnull{trace_class}
@prenotnull{clock_class}
@postrefcountsame{trace_class}
@postsuccessrefcountinc{clock_class}
@post <strong>On success, if \p trace_class is frozen</strong>,
	\p clock_class is frozen.

@sa bt_ctf_trace_get_clock_class(): Returns the clock class contained
	in a given trace class at a given index.
@sa bt_ctf_trace_get_clock_class_by_name(): Finds a clock class by name.
*/
extern int bt_ctf_trace_add_clock_class(struct bt_ctf_trace *trace_class,
		struct bt_ctf_clock_class *clock_class);

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
extern int bt_ctf_trace_get_stream_class_count(struct bt_ctf_trace *trace_class);

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
	bt_ctf_trace_get_stream_class_count()).
@postrefcountsame{trace_class}

@sa bt_ctf_trace_get_stream_class_by_id(): Finds a stream class by ID.
@sa bt_ctf_trace_add_stream_class(): Adds a stream class to a trace class.
*/
extern struct bt_ctf_stream_class *bt_ctf_trace_get_stream_class(
		struct bt_ctf_trace *trace_class, int index);

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

@sa bt_ctf_trace_get_stream_class(): Returns the stream class contained
	in a given trace class at a given index.
@sa bt_ctf_trace_add_stream_class(): Adds a stream class to a trace class.
*/
extern struct bt_ctf_stream_class *bt_ctf_trace_get_stream_class_by_id(
		struct bt_ctf_trace *trace_class, uint32_t id);

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

@sa bt_ctf_trace_get_stream_class(): Returns the stream class contained
	in a given trace class at a given index.
@sa bt_ctf_trace_get_stream_class_by_id(): Finds a stream class by ID.
*/
extern int bt_ctf_trace_add_stream_class(struct bt_ctf_trace *trace_class,
		struct bt_ctf_stream_class *stream_class);

/** @} */

/**
@name Misc. functions
@{
*/

/**
@brief	User function type to use with bt_ctf_trace_add_listener().

@param[in] obj	New CTF IR object which is part of the trace
		class hierarchy.
@param[in] data	User data.

@prenotnull{obj}
*/
typedef void (*bt_ctf_listener_cb)(struct bt_ctf_object *obj, void *data);

/**
@brief	Adds the trace class modification listener \p listener to
	the CTF IR trace class \p trace_class.

Once you add \p listener to \p trace_class, whenever \p trace_class
is modified, \p listener is called with the new element and with
\p data (user data).

@param[in] trace_class	Trace class to which to add \p listener.
@param[in] listener	Modification listener function.
@param[in] data		User data.
@returns		0 on success, or a negative value on error.

@prenotnull{trace_class}
@prenotnull{listener}
@postrefcountsame{trace_class}
*/
extern int bt_ctf_trace_add_listener(struct bt_ctf_trace *trace_class,
		bt_ctf_listener_cb listener, void *data);

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
extern int bt_ctf_trace_visit(struct bt_ctf_trace *trace_class,
		bt_ctf_visitor visitor, void *data);

/** @} */

/** @} */

/*
 * bt_ctf_trace_get_metadata_string: get metadata string.
 *
 * Get the trace's TSDL metadata. The caller assumes the ownership of the
 * returned string.
 *
 * @param trace Trace instance.
 *
 * Returns the metadata string on success, NULL on error.
 */
extern char *bt_ctf_trace_get_metadata_string(struct bt_ctf_trace *trace);

#ifdef __cplusplus
}
#endif

#endif /* BABELTRACE_CTF_IR_TRACE_H */
