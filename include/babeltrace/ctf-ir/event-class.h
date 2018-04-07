#ifndef BABELTRACE_CTF_IR_EVENT_CLASS_H
#define BABELTRACE_CTF_IR_EVENT_CLASS_H

/*
 * BabelTrace - CTF IR: Event class
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

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

struct bt_value;

/**
@defgroup ctfireventclass CTF IR event class
@ingroup ctfir
@brief CTF IR event class.

@code
#include <babeltrace/ctf-ir/event-class.h>
@endcode

A CTF IR <strong><em>event class</em></strong> is a template that you
can use to create concrete \link ctfirevent CTF IR events\endlink.

An event class has the following properties:

- A \b name.
- A numeric \b ID (\em must be unique amongst all the event classes
  contained in the same
  \link ctfirstreamclass CTF IR stream class\endlink).
- A optional <strong>log level</strong>.
- An optional <strong>Eclipse Modeling Framework URI</strong>.

A CTF IR event class owns two
\link ctfirfieldtypes field types\endlink:

- An optional <strong>event context</strong> field type, which
  represents the \c event.context CTF scope.
- A mandatory <strong>event payload</strong> field type, which
  represents the \c event.fields CTF scope.

Both field types \em must be structure field types as of
Babeltrace \btversion.
The event payload field type <em>must not</em> be empty.

As a reminder, here's the structure of a CTF packet:

@imgpacketstructure

In the Babeltrace CTF IR system, a \link ctfirtraceclass trace
class\endlink contains zero or more \link ctfirstreamclass stream
classes\endlink, and a stream class contains zero or more event classes.

Before you can create an event from an event class with
bt_event_create(), you \em must add the prepared event class to a
stream class by calling bt_stream_class_add_event_class(). This
function, when successful, \em freezes the event class, disallowing any
future modification of its properties and field types by the user.

As with any Babeltrace object, CTF IR event class objects have
<a href="https://en.wikipedia.org/wiki/Reference_counting">reference
counts</a>. See \ref refs to learn more about the reference counting
management of Babeltrace objects.

bt_stream_class_add_event_class() \em freezes its event class
parameter on success. You cannot modify a frozen event class: it is
considered immutable, except for \link refs reference counting\endlink.

@sa ctfirevent
@sa ctfirstreamclass

@file
@brief CTF IR event class type and functions.
@sa ctfireventclass

@addtogroup ctfireventclass
@{
*/

/**
@struct bt_event_class
@brief A CTF IR event class.
@sa ctfireventclass
*/
struct bt_event_class;
struct bt_field;
struct bt_field_type;
struct bt_stream_class;

/**
@brief	Log level of an event class.
*/
enum bt_event_class_log_level {
	/// Unknown, used for errors.
	BT_EVENT_CLASS_LOG_LEVEL_UNKNOWN	= -1,

	/// Unspecified log level.
	BT_EVENT_CLASS_LOG_LEVEL_UNSPECIFIED	= 255,

	/// System is unusable.
	BT_EVENT_CLASS_LOG_LEVEL_EMERGENCY	= 0,

	/// Action must be taken immediately.
	BT_EVENT_CLASS_LOG_LEVEL_ALERT		= 1,

	/// Critical conditions.
	BT_EVENT_CLASS_LOG_LEVEL_CRITICAL	= 2,

	/// Error conditions.
	BT_EVENT_CLASS_LOG_LEVEL_ERROR		= 3,

	/// Warning conditions.
	BT_EVENT_CLASS_LOG_LEVEL_WARNING	= 4,

	/// Normal, but significant, condition.
	BT_EVENT_CLASS_LOG_LEVEL_NOTICE		= 5,

	/// Informational message.
	BT_EVENT_CLASS_LOG_LEVEL_INFO		= 6,

	/// Debug information with system-level scope (set of programs).
	BT_EVENT_CLASS_LOG_LEVEL_DEBUG_SYSTEM	= 7,

	/// Debug information with program-level scope (set of processes).
	BT_EVENT_CLASS_LOG_LEVEL_DEBUG_PROGRAM	= 8,

	/// Debug information with process-level scope (set of modules).
	BT_EVENT_CLASS_LOG_LEVEL_DEBUG_PROCESS	= 9,

	/// Debug information with module (executable/library) scope (set of units).
	BT_EVENT_CLASS_LOG_LEVEL_DEBUG_MODULE	= 10,

	/// Debug information with compilation unit scope (set of functions).
	BT_EVENT_CLASS_LOG_LEVEL_DEBUG_UNIT	= 11,

	/// Debug information with function-level scope.
	BT_EVENT_CLASS_LOG_LEVEL_DEBUG_FUNCTION	= 12,

	/// Debug information with line-level scope (default log level).
	BT_EVENT_CLASS_LOG_LEVEL_DEBUG_LINE	= 13,

	/// Debug-level message.
	BT_EVENT_CLASS_LOG_LEVEL_DEBUG		= 14,
};

/**
@name Creation and parent access functions
@{
*/

/**
@brief	Creates a default CTF IR event class named \p name­.

On success, the context and payload field types are empty structure
field types. You can modify those default field types after the
event class is created with
bt_event_class_set_context_field_type() and
bt_event_class_set_payload_field_type().

Upon creation, the event class's ID is <em>not set</em>. You
can set it to a specific value with bt_event_class_set_id(). If it
is still unset when you call bt_stream_class_add_event_class(), then
the stream class assigns a unique ID to this event class before
freezing it.

The created event class's log level is initially set to
#BT_EVENT_CLASS_LOG_LEVEL_UNSPECIFIED and it has no Eclipse Modeling
Framework URI.

@param[in] name	Name of the event class to create (copied on success).
@returns	Created event class, or \c NULL on error.

@prenotnull{name}
@postsuccessrefcountret1
*/
extern struct bt_event_class *bt_event_class_create(const char *name);

/**
@brief	Returns the parent CTF IR stream class of the CTF IR event
	class \p event_class.

It is possible that the event class was not added to a stream class
yet, in which case this function returns \c NULL. You can add an
event class to a stream class with
bt_stream_class_add_event_class().

@param[in] event_class	Event class of which to get the parent
			stream class.
@returns		Parent stream class of \p event_class,
			or \c NULL if \p event_class was not
			added to a stream class yet or on error.

@prenotnull{event_class}
@postrefcountsame{event_class}
@postsuccessrefcountretinc

@sa bt_stream_class_add_event_class(): Add an event class to
	a stream class.
*/
extern struct bt_stream_class *bt_event_class_get_stream_class(
		struct bt_event_class *event_class);

/** @} */

/**
@name Attribute functions
@{
*/

/**
@brief	Returns the name of the CTF IR event class \p event_class.

On success, \p event_class remains the sole owner of the returned
string.

@param[in] event_class	Event class of which to get the name.
@returns		Name of event class \p event_class, or
			\c NULL on error.

@prenotnull{event_class}
@postrefcountsame{event_class}
*/
extern const char *bt_event_class_get_name(
		struct bt_event_class *event_class);

/**
@brief	Returns the numeric ID of the CTF IR event class \p event_class.

@param[in] event_class	Event class of which to get the numeric ID.
@returns		ID of event class \p event_class, or a
			negative value on error.

@prenotnull{event_class}
@postrefcountsame{event_class}

@sa bt_event_class_set_id(): Sets the numeric ID of a given
	event class.
*/
extern int64_t bt_event_class_get_id(
		struct bt_event_class *event_class);

/**
@brief	Sets the numeric ID of the CTF IR event class
	\p event_class to \p id.

\p id must be unique amongst the IDs of all the event classes
of the stream class to which you eventually add \p event_class.

@param[in] event_class	Event class of which to set the numeric ID.
@param[in] id		ID of the event class.
@returns		0 on success, or a negative value on error.

@prenotnull{event_class}
@prehot{event_class}
@pre \p id is lesser than or equal to 9223372036854775807 (\c INT64_MAX).
@postrefcountsame{event_class}

@sa bt_event_class_get_id(): Returns the numeric ID of a given
	event class.
*/
extern int bt_event_class_set_id(
		struct bt_event_class *event_class, uint64_t id);

/**
@brief	Returns the log level of the CTF IR event class \p event_class.

@param[in] event_class	Event class of which to get the log level.
@returns		Log level of event class \p event_class,
			#BT_EVENT_CLASS_LOG_LEVEL_UNSPECIFIED if
			not specified, or
			#BT_EVENT_CLASS_LOG_LEVEL_UNKNOWN on error.

@prenotnull{event_class}
@postrefcountsame{event_class}

@sa bt_event_class_set_log_level(): Sets the log level of a given
	event class.
*/
extern enum bt_event_class_log_level bt_event_class_get_log_level(
		struct bt_event_class *event_class);

/**
@brief	Sets the log level of the CTF IR event class
	\p event_class to \p log_level.

@param[in] event_class	Event class of which to set the log level.
@param[in] log_level	Log level of the event class.
@returns		0 on success, or a negative value on error.

@prenotnull{event_class}
@prehot{event_class}
@pre \p log_level is #BT_EVENT_CLASS_LOG_LEVEL_UNSPECIFIED,
	#BT_EVENT_CLASS_LOG_LEVEL_EMERGENCY,
	#BT_EVENT_CLASS_LOG_LEVEL_ALERT,
	#BT_EVENT_CLASS_LOG_LEVEL_CRITICAL,
	#BT_EVENT_CLASS_LOG_LEVEL_ERROR,
	#BT_EVENT_CLASS_LOG_LEVEL_WARNING,
	#BT_EVENT_CLASS_LOG_LEVEL_NOTICE,
	#BT_EVENT_CLASS_LOG_LEVEL_INFO,
	#BT_EVENT_CLASS_LOG_LEVEL_DEBUG_SYSTEM,
	#BT_EVENT_CLASS_LOG_LEVEL_DEBUG_PROGRAM,
	#BT_EVENT_CLASS_LOG_LEVEL_DEBUG_PROCESS,
	#BT_EVENT_CLASS_LOG_LEVEL_DEBUG_MODULE,
	#BT_EVENT_CLASS_LOG_LEVEL_DEBUG_UNIT,
	#BT_EVENT_CLASS_LOG_LEVEL_DEBUG_FUNCTION,
	#BT_EVENT_CLASS_LOG_LEVEL_DEBUG_LINE, or
	#BT_EVENT_CLASS_LOG_LEVEL_DEBUG.
@postrefcountsame{event_class}

@sa bt_event_class_get_log_level(): Returns the log level of a given
	event class.
*/
extern int bt_event_class_set_log_level(
		struct bt_event_class *event_class,
		enum bt_event_class_log_level log_level);

/**
@brief  Returns the Eclipse Modeling Framework URI of the CTF IR event
	class \p event_class.

@param[in] event_class	Event class of which to get the
			Eclipse Modeling Framework URI.
@returns		Eclipse Modeling Framework URI of event
			class \p event_class, or \c NULL on error.

@prenotnull{event_class}
@postrefcountsame{event_class}

@sa bt_event_class_set_emf_uri(): Sets the Eclipse Modeling
	Framework URI of a given event class.
*/
extern const char *bt_event_class_get_emf_uri(
		struct bt_event_class *event_class);

/**
@brief	Sets the Eclipse Modeling Framework URI of the CTF IR event class
	\p event_class to \p emf_uri, or unsets the event class's EMF URI.

@param[in] event_class	Event class of which to set the
			Eclipse Modeling Framework URI.
@param[in] emf_uri	Eclipse Modeling Framework URI of the
			event class (copied on success), or \c NULL
			to unset the current EMF URI.
@returns		0 on success, or a negative value if there's
			no EMF URI or on error.

@prenotnull{event_class}
@prenotnull{emf_uri}
@prehot{event_class}
@postrefcountsame{event_class}

@sa bt_event_class_get_emf_uri(): Returns the Eclipse Modeling
	Framework URI of a given event class.
*/
extern int bt_event_class_set_emf_uri(
		struct bt_event_class *event_class,
		const char *emf_uri);

/** @} */

/**
@name Contained field types functions
@{
*/

/**
@brief	Returns the context field type of the CTF IR event class
	\p event_class.

@param[in] event_class	Event class of which to get the context field type.
@returns		Context field type of \p event_class, or \c NULL if
			\p event_class has no context field type or on error.

@prenotnull{event_class}
@postrefcountsame{event_class}
@post <strong>On success, if the return value is a field type</strong>, its
	reference count is incremented.

@sa bt_event_class_set_context_field_type(): Sets the context field type of a
	given event class.
*/
extern struct bt_field_type *bt_event_class_get_context_field_type(
		struct bt_event_class *event_class);

/**
@brief	Sets the context field type of the CTF IR event class \p event_class to
	\p context_type, or unsets the current context field type from
	\p event_class.

If \p context_type is \c NULL, then this function unsets the current context
field type from \p event_class, effectively making \p event_class an event class
without a context field type.

As of Babeltrace \btversion, if \p context_type is not \c NULL,
\p context_type \em must be a CTF IR structure field type object.

@param[in] event_class		Event class of which to set the context field
				type.
@param[in] context_type		Context field type, or \c NULL to unset the
				current context field type.
@returns			0 on success, or a negative value on error.

@prenotnull{event_class}
@prehot{event_class}
@pre <strong>If \p context_type is not \c NULL</strong>, \p context_type is a
	CTF IR structure field type.
@postrefcountsame{event_class}
@post <strong>On success, if \p context_type is not \c NULL</strong>,
	the reference count of \p context_type is incremented.

@sa bt_event_class_get_context_field_type(): Returns the context field type of a
	given event class.
*/
extern int bt_event_class_set_context_field_type(
		struct bt_event_class *event_class,
		struct bt_field_type *context_type);

/**
@brief	Returns the payload field type of the CTF IR event class
	\p event_class.

@param[in] event_class	Event class of which to get the payload field type.
@returns		Payload field type of \p event_class, or \c NULL if
			\p event_class has no payload field type or on error.

@prenotnull{event_class}
@postrefcountsame{event_class}
@post <strong>On success, if the return value is a field type</strong>, its
	reference count is incremented.

@sa bt_event_class_set_payload_field_type(): Sets the payload field type of a
	given event class.
*/
extern struct bt_field_type *bt_event_class_get_payload_field_type(
		struct bt_event_class *event_class);

/**
@brief	Sets the payload field type of the CTF IR event class \p event_class to
	\p payload_type, or unsets the current payload field type from
	\p event_class.

If \p payload_type is \c NULL, then this function unsets the current payload
field type from \p event_class, effectively making \p event_class an event class
without a payload field type.

As of Babeltrace \btversion, if \p payload_type is not \c NULL,
\p payload_type \em must be a CTF IR structure field type object.

@param[in] event_class		Event class of which to set the payload field
				type.
@param[in] payload_type		Payload field type, or \c NULL to unset the
				current payload field type.
@returns			0 on success, or a negative value on error.

@prenotnull{event_class}
@prehot{event_class}
@pre <strong>If \p payload_type is not \c NULL</strong>, \p payload_type is a
	CTF IR structure field type.
@postrefcountsame{event_class}
@post <strong>On success, if \p payload_type is not \c NULL</strong>,
	the reference count of \p payload_type is incremented.

@sa bt_event_class_get_payload_field_type(): Returns the payload field type of a
	given event class.
*/
extern int bt_event_class_set_payload_field_type(
		struct bt_event_class *event_class,
		struct bt_field_type *payload_type);

/** @} */

/** @} */

#ifdef __cplusplus
}
#endif

#endif /* BABELTRACE_CTF_IR_EVENT_CLASS_H */
