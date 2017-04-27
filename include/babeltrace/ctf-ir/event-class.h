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

An event class has the following properties, both of which \em must
be unique amongst all the event classes contained in the same
\link ctfirstreamclass CTF IR stream class\endlink:

- A \b name.
- A numeric \b ID.

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
@name Creation and parent access functions
@{
*/

/**
@brief	Creates a default CTF IR event class named \p name­.

The event class is created \em without an event context
\link ctfirfieldtypes field type\endlink and with an empty event
payload field type.

Upon creation, the event class's ID is <em>not set</em>. You
can set it to a specific value with bt_ctf_event_class_set_id(). If it
is still unset when you call bt_ctf_stream_class_add_event_class(), then
the stream class assigns a unique ID to this event class before
freezing it.

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
@brief	Returns the number of attributes contained in the CTF IR event
	class \p event_class.

@param[in] event_class	Event class of which to get the number
			of contained attributes.
@returns		Number of contained attributes in
			\p event_class, or a negative value on error.

@prenotnull{event_class}
@postrefcountsame{event_class}

@sa bt_ctf_event_class_get_attribute_name_by_index(): Returns the name
	of the attribute of a given event class at a given index.
@sa bt_ctf_event_class_get_attribute_value_by_index(): Returns the value
	of the attribute of a given event class at a given index.
*/
extern int64_t bt_ctf_event_class_get_attribute_count(
		struct bt_ctf_event_class *event_class);

/**
@brief	Returns the name of the attribute at the index \p index of the
	CTF IR event class \p event_class.

On success, \p event_class remains the sole owner of the returned
string.

@param[in] event_class	Event class of which to get the name
			of an attribute.
@param[in] index	Index of the attribute of which to get the name.
@returns		Attribute name, or \c NULL on error.

@prenotnull{event_class}
@pre \p index is lesser than the number of attributes contained by
	\p event_class.
@postrefcountsame{event_class}

@sa bt_ctf_event_class_get_attribute_value_by_index(): Returns the value
	of the attribute of a given event class at a given index.
*/
extern const char *
bt_ctf_event_class_get_attribute_name_by_index(
		struct bt_ctf_event_class *event_class, uint64_t index);

/**
@brief	Returns the value of the attribute at the index \p index of the
	CTF IR event class \p event_class.

@param[in] event_class	Event class of which to get the value
			of an attribute.
@param[in] index	Index of the attribute of which to get the value.
@returns		Attribute value, or \c NULL on error.

@prenotnull{event_class}
@pre \p index is lesser than the number of attributes contained by
	\p event_class.
@postsuccessrefcountretinc
@postrefcountsame{event_class}

@sa bt_ctf_event_class_get_attribute_name_by_index(): Returns the name
	of the attribute of a given event class at a given index.
*/
extern struct bt_value *
bt_ctf_event_class_get_attribute_value_by_index(
		struct bt_ctf_event_class *event_class, uint64_t index);

/**
@brief	Returns the value of the attribute named \p name of the CTF IR
	event class \p event_class.

On success, the reference count of the returned value object is
incremented.

@param[in] event_class	Event class of which to get the value
			of an attribute.
@param[in] name		Name of the attribute to get.
@returns		Attribute value, or \c NULL on error.

@prenotnull{event_class}
@prenotnull{name}
@postsuccessrefcountretinc
@postrefcountsame{event_class}
*/
extern struct bt_value *
bt_ctf_event_class_get_attribute_value_by_name(
		struct bt_ctf_event_class *event_class, const char *name);

/**
@brief	Sets the attribute named \p name of the CTF IR event class
	\p event_class to the value \p value.

Valid attributes, as of Babeltrace \btversion, are:

- <code>id</code>: \em must be an integer value object with a raw value
  that is greater than or equal to 0. This represents the event class's
  numeric ID and you can also set it with bt_ctf_event_class_set_id().

- <code>name</code>: must be a string value object. This represents
  the name of the event class.

- <code>loglevel</code>: must be an integer value object with a raw
  value greater than or equal to 0. This represents the numeric log level
  associated with this event class. Log level values
  are application-specific.

- <code>model.emf.uri</code>: must be a string value object. This
  represents the application-specific Eclipse Modeling Framework URI
  of the event class.

@param[in] event_class	Event class of which to set an
			attribute.
@param[in] name		Attribute name (copied on success).
@param[in] value	Attribute value.
@returns		0 on success, or a negative value on error.

@prenotnull{event_class}
@prenotnull{name}
@prenotnull{value}
@prehot{event_class}
@postrefcountsame{event_class}
@postsuccessrefcountinc{value}

@sa bt_ctf_event_class_get_attribute_value_by_name(): Returns the
	attribute of a given event class having a given name.
*/
extern int bt_ctf_event_class_set_attribute(
		struct bt_ctf_event_class *event_class, const char *name,
		struct bt_value *value);

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
