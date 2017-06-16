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
#include <babeltrace/values.h>

#ifdef __cplusplus
extern "C" {
#endif

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
bt_ctf_event_create(), you \em must add the prepared event class to a
stream class by calling bt_ctf_stream_class_add_event_class(). This
function, when successful, \em freezes the event class, disallowing any
future modification of its properties and field types by the user.

As with any Babeltrace object, CTF IR event class objects have
<a href="https://en.wikipedia.org/wiki/Reference_counting">reference
counts</a>. See \ref refs to learn more about the reference counting
management of Babeltrace objects.

bt_ctf_stream_class_add_event_class() \em freezes its event class
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
@struct bt_ctf_event_class
@brief A CTF IR event class.
@sa ctfireventclass
*/
struct bt_ctf_event_class;
struct bt_ctf_field;
struct bt_ctf_field_type;
struct bt_ctf_stream_class;

/**
@brief	Log level of an event class.
*/
enum bt_ctf_event_class_log_level {
	/// Unknown, used for errors.
	BT_CTF_EVENT_CLASS_LOG_LEVEL_UNKNOWN		= -1,

	/// Unspecified log level.
	BT_CTF_EVENT_CLASS_LOG_LEVEL_UNSPECIFIED	= 255,

	/// System is unusable.
	BT_CTF_EVENT_CLASS_LOG_LEVEL_EMERGENCY		= 0,

	/// Action must be taken immediately.
	BT_CTF_EVENT_CLASS_LOG_LEVEL_ALERT		= 1,

	/// Critical conditions.
	BT_CTF_EVENT_CLASS_LOG_LEVEL_CRITICAL		= 2,

	/// Error conditions.
	BT_CTF_EVENT_CLASS_LOG_LEVEL_ERROR		= 3,

	/// Warning conditions.
	BT_CTF_EVENT_CLASS_LOG_LEVEL_WARNING		= 4,

	/// Normal, but significant, condition.
	BT_CTF_EVENT_CLASS_LOG_LEVEL_NOTICE		= 5,

	/// Informational message.
	BT_CTF_EVENT_CLASS_LOG_LEVEL_INFO		= 6,

	/// Debug information with system-level scope (set of programs).
	BT_CTF_EVENT_CLASS_LOG_LEVEL_DEBUG_SYSTEM	= 7,

	/// Debug information with program-level scope (set of processes).
	BT_CTF_EVENT_CLASS_LOG_LEVEL_DEBUG_PROGRAM	= 8,

	/// Debug information with process-level scope (set of modules).
	BT_CTF_EVENT_CLASS_LOG_LEVEL_DEBUG_PROCESS	= 9,

	/// Debug information with module (executable/library) scope (set of units).
	BT_CTF_EVENT_CLASS_LOG_LEVEL_DEBUG_MODULE	= 10,

	/// Debug information with compilation unit scope (set of functions).
	BT_CTF_EVENT_CLASS_LOG_LEVEL_DEBUG_UNIT		= 11,

	/// Debug information with function-level scope.
	BT_CTF_EVENT_CLASS_LOG_LEVEL_DEBUG_FUNCTION	= 12,

	/// Debug information with line-level scope (default log level).
	BT_CTF_EVENT_CLASS_LOG_LEVEL_DEBUG_LINE		= 13,

	/// Debug-level message.
	BT_CTF_EVENT_CLASS_LOG_LEVEL_DEBUG		= 14,
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
bt_ctf_event_class_set_context_type() and
bt_ctf_event_class_set_payload_type().

Upon creation, the event class's ID is <em>not set</em>. You
can set it to a specific value with bt_ctf_event_class_set_id(). If it
is still unset when you call bt_ctf_stream_class_add_event_class(), then
the stream class assigns a unique ID to this event class before
freezing it.

The created event class's log level is initially set to
#BT_CTF_EVENT_CLASS_LOG_LEVEL_UNSPECIFIED and it has no Eclipse Modeling
Framework URI.

@param[in] name	Name of the event class to create (copied on success).
@returns	Created event class, or \c NULL on error.

@prenotnull{name}
@postsuccessrefcountret1
*/
extern struct bt_ctf_event_class *bt_ctf_event_class_create(const char *name);

/**
@brief	Returns the parent CTF IR stream class of the CTF IR event
	class \p event_class.

It is possible that the event class was not added to a stream class
yet, in which case this function returns \c NULL. You can add an
event class to a stream class with
bt_ctf_stream_class_add_event_class().

@param[in] event_class	Event class of which to get the parent
			stream class.
@returns		Parent stream class of \p event_class,
			or \c NULL if \p event_class was not
			added to a stream class yet or on error.

@prenotnull{event_class}
@postrefcountsame{event_class}
@postsuccessrefcountretinc

@sa bt_ctf_stream_class_add_event_class(): Add an event class to
	a stream class.
*/
extern struct bt_ctf_stream_class *bt_ctf_event_class_get_stream_class(
		struct bt_ctf_event_class *event_class);

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
extern const char *bt_ctf_event_class_get_name(
		struct bt_ctf_event_class *event_class);

/**
@brief	Returns the numeric ID of the CTF IR event class \p event_class.

@param[in] event_class	Event class of which to get the numeric ID.
@returns		ID of event class \p event_class, or a
			negative value on error.

@prenotnull{event_class}
@postrefcountsame{event_class}

@sa bt_ctf_event_class_set_id(): Sets the numeric ID of a given
	event class.
*/
extern int64_t bt_ctf_event_class_get_id(
		struct bt_ctf_event_class *event_class);

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

@sa bt_ctf_event_class_get_id(): Returns the numeric ID of a given
	event class.
*/
extern int bt_ctf_event_class_set_id(
		struct bt_ctf_event_class *event_class, uint64_t id);

/**
@brief	Returns the log level of the CTF IR event class \p event_class.

@param[in] event_class	Event class of which to get the log level.
@returns		Log level of event class \p event_class,
			#BT_CTF_EVENT_CLASS_LOG_LEVEL_UNSPECIFIED if
			not specified, or
			#BT_CTF_EVENT_CLASS_LOG_LEVEL_UNKNOWN on error.

@prenotnull{event_class}
@postrefcountsame{event_class}

@sa bt_ctf_event_class_set_log_level(): Sets the log level of a given
	event class.
*/
extern enum bt_ctf_event_class_log_level bt_ctf_event_class_get_log_level(
		struct bt_ctf_event_class *event_class);

/**
@brief	Sets the log level of the CTF IR event class
	\p event_class to \p log_level.

@param[in] event_class	Event class of which to set the log level.
@param[in] log_level	Log level of the event class.
@returns		0 on success, or a negative value on error.

@prenotnull{event_class}
@prehot{event_class}
@pre \p log_level is #BT_CTF_EVENT_CLASS_LOG_LEVEL_UNSPECIFIED,
	#BT_CTF_EVENT_CLASS_LOG_LEVEL_EMERGENCY,
	#BT_CTF_EVENT_CLASS_LOG_LEVEL_ALERT,
	#BT_CTF_EVENT_CLASS_LOG_LEVEL_CRITICAL,
	#BT_CTF_EVENT_CLASS_LOG_LEVEL_ERROR,
	#BT_CTF_EVENT_CLASS_LOG_LEVEL_WARNING,
	#BT_CTF_EVENT_CLASS_LOG_LEVEL_NOTICE,
	#BT_CTF_EVENT_CLASS_LOG_LEVEL_INFO,
	#BT_CTF_EVENT_CLASS_LOG_LEVEL_DEBUG_SYSTEM,
	#BT_CTF_EVENT_CLASS_LOG_LEVEL_DEBUG_PROGRAM,
	#BT_CTF_EVENT_CLASS_LOG_LEVEL_DEBUG_PROCESS,
	#BT_CTF_EVENT_CLASS_LOG_LEVEL_DEBUG_MODULE,
	#BT_CTF_EVENT_CLASS_LOG_LEVEL_DEBUG_UNIT,
	#BT_CTF_EVENT_CLASS_LOG_LEVEL_DEBUG_FUNCTION,
	#BT_CTF_EVENT_CLASS_LOG_LEVEL_DEBUG_LINE, or
	#BT_CTF_EVENT_CLASS_LOG_LEVEL_DEBUG.
@postrefcountsame{event_class}

@sa bt_ctf_event_class_get_log_level(): Returns the log level of a given
	event class.
*/
extern int bt_ctf_event_class_set_log_level(
		struct bt_ctf_event_class *event_class,
		enum bt_ctf_event_class_log_level log_level);

/**
@brief  Returns the Eclipse Modeling Framework URI of the CTF IR event
	class \p event_class.

@param[in] event_class	Event class of which to get the
			Eclipse Modeling Framework URI.
@returns		Eclipse Modeling Framework URI of event
			class \p event_class, or \c NULL on error.

@prenotnull{event_class}
@postrefcountsame{event_class}

@sa bt_ctf_event_class_set_emf_uri(): Sets the Eclipse Modeling
	Framework URI of a given event class.
*/
extern const char *bt_ctf_event_class_get_emf_uri(
		struct bt_ctf_event_class *event_class);

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

@sa bt_ctf_event_class_get_emf_uri(): Returns the Eclipse Modeling
	Framework URI of a given event class.
*/
extern int bt_ctf_event_class_set_emf_uri(
		struct bt_ctf_event_class *event_class,
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

@sa bt_ctf_event_class_set_context_type(): Sets the context field type of a
	given event class.
*/
extern struct bt_ctf_field_type *bt_ctf_event_class_get_context_type(
		struct bt_ctf_event_class *event_class);

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

@sa bt_ctf_event_class_get_context_type(): Returns the context field type of a
	given event class.
*/
extern int bt_ctf_event_class_set_context_type(
		struct bt_ctf_event_class *event_class,
		struct bt_ctf_field_type *context_type);

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

@sa bt_ctf_event_class_set_payload_type(): Sets the payload field type of a
	given event class.
*/
extern struct bt_ctf_field_type *bt_ctf_event_class_get_payload_type(
		struct bt_ctf_event_class *event_class);

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

@sa bt_ctf_event_class_get_payload_type(): Returns the payload field type of a
	given event class.
*/
extern int bt_ctf_event_class_set_payload_type(
		struct bt_ctf_event_class *event_class,
		struct bt_ctf_field_type *payload_type);

/**
@brief	Returns the number of fields contained in the
	payload field type of the CTF IR event class \p event_class.

@remarks
Calling this function is the equivalent of getting the payload field
type of \p event_class with bt_ctf_event_class_get_payload_type() and
getting its field count with
bt_ctf_field_type_structure_get_field_count().

@param[in] event_class	Event class of which to get the number
			of fields contained in its payload field type.
@returns		Number of fields in the payload field type
			of \p event_class, or a negative value on error.

@prenotnull{event_class}
@postrefcountsame{event_class}
*/
extern int64_t bt_ctf_event_class_get_payload_type_field_count(
		struct bt_ctf_event_class *event_class);

/**
@brief	Returns the type and the name of the field at index \p index
	in the payload field type of the CTF IR event class
	\p event_class.

On success, the field's type is placed in \p *field_type if
\p field_type is not \c NULL. The field's name is placed in
\p *name if \p name is not \c NULL. \p event_class remains the sole
owner of \p *name.

Both \p name and \p field_type can be \c NULL if the caller is not
interested in one of them.

@remarks
Calling this function is the equivalent of getting the payload field
type of \p event_class with bt_ctf_event_class_get_payload_type() and
getting the type and name of one of its field with
bt_ctf_field_type_structure_get_field().

@param[in] event_class	Event class of which to get the type and name
			of a field in its payload field type.
@param[out] field_name	Name of the field at the index
			\p index in the payload field type of
			\p event_class (can be \c NULL).
@param[out] field_type	Type of the field at the index \p index in the
			payload field type of \p event_class
			(can be \c NULL).
@param[in] index	Index of the payload field type's field to find.
@returns		0 on success, or a negative value on error.

@prenotnull{event_class}
@pre \p index is lesser than the number of fields contained in the
	payload field type of \p event_class (see
	bt_ctf_event_class_get_payload_type_field_count()).
@postrefcountsame{event_class}
@post <strong>On success, if \p field_type is not \c NULL</strong>, the
	reference count of \p *field_type is incremented.
*/
extern int bt_ctf_event_class_get_payload_type_field_by_index(
		struct bt_ctf_event_class *event_class,
		const char **field_name, struct bt_ctf_field_type **field_type,
		uint64_t index);

/**
@brief  Returns the type of the field named \p name in the payload
	field type of the CTF IR event class \p event_class.

@remarks
Calling this function is the equivalent of getting the payload field
type of \p event_class with bt_ctf_event_class_get_payload_type() and
getting the type of one of its field with
bt_ctf_field_type_structure_get_field_type_by_name().

@param[in] event_class	Event class of which to get the type of a
			payload field type's field.
@param[in] name		Name of the payload field type's field to get.
@returns		Type of the field named \p name in the payload
			field type of \p event_class, or \c NULL if
			the function cannot find the field or
			on error.

@prenotnull{event_class}
@prenotnull{name}
@postrefcountsame{event_class}
@postsuccessrefcountretinc
*/
extern struct bt_ctf_field_type *
bt_ctf_event_class_get_payload_type_field_type_by_name(
		struct bt_ctf_event_class *event_class, const char *name);

/* Pre-2.0 CTF writer compatibility */
#define bt_ctf_event_class_get_field_by_name bt_ctf_event_class_get_payload_type_field_type_by_name

/**
@brief	Adds a field named \p name with the type \p field_type to the
	payload field type of the CTF IR event class \p event_class.

@remarks
Calling this function is the equivalent of getting the payload field
type of \p event_class with bt_ctf_event_class_get_payload_type() and
adding a field to it with bt_ctf_field_type_structure_add_field().

@param[in] event_class	Event class containing the payload field
			type in which to add a field.
@param[in] field_type	Type of the field to add.
@param[in] name		Name of the field to add (copied on
			success).
@returns		0 on success, or a negative value on error.

@prenotnull{event_class}
@prenotnull{type}
@prenotnull{name}
@prehot{event_class}
@postrefcountsame{event_class}
@postsuccessrefcountinc{field_type}
*/
extern int bt_ctf_event_class_add_field(struct bt_ctf_event_class *event_class,
		struct bt_ctf_field_type *field_type,
		const char *name);

/** @} */

/** @} */

#ifdef __cplusplus
}
#endif

#endif /* BABELTRACE_CTF_IR_EVENT_CLASS_H */
