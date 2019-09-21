#ifndef BABELTRACE2_TRACE_IR_EVENT_CLASS_H
#define BABELTRACE2_TRACE_IR_EVENT_CLASS_H

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
@defgroup api-tir-ev-cls Event class
@ingroup api-tir

@brief
    Class of \bt_p_ev.

An <strong><em>event class</em></strong> is the class of \bt_p_ev,
which \bt_p_ev_msg contain:

@image html trace-structure.png

In the illustration above, notice that:

- A \bt_stream is a conceptual \ref api-msg-seq "sequence of messages",
  some of which are \bt_p_ev_msg.
- An event message contains an \bt_ev.
- An event is an instance of an event class.
- The \ref api-tir-ev-prop-payload "payload field" of an event is an
  instance of the \ref api-tir-ev-cls-prop-p-fc "payload field class"
  which an event class contains.

An event class is a \ref api-tir "trace IR" metadata object.

An event class is a \ref api-fund-shared-object "shared object": get a
new reference with bt_event_class_get_ref() and put an existing
reference with bt_event_class_put_ref().

Some library functions \ref api-fund-freezing "freeze" event classes on
success. The documentation of those functions indicate this
postcondition.

The type of an event class is #bt_event_class.

A \bt_stream_cls contains event classes. All the event classes of a
given stream class have unique
\ref api-tir-ev-cls-prop-id "numeric IDs". Borrow the stream class
which contains an event class with bt_event_class_borrow_stream_class()
or bt_event_class_borrow_stream_class_const().

To create a default event class:

<dl>
  <dt>
    If bt_stream_class_assigns_automatic_event_class_id() returns
    #BT_TRUE (the default) for the stream class to use
  </dt>
  <dd>Use bt_event_class_create().</dd>

  <dt>
    If bt_stream_class_assigns_automatic_event_class_id() returns
    #BT_FALSE for the stream class to use
  </dt>
  <dd>Use bt_event_class_create_with_id().</dd>
</dl>

<h1>Properties</h1>

An event class has the following properties:

<dl>
  <dt>\anchor api-tir-ev-cls-prop-id Numeric ID</dt>
  <dd>
    Numeric ID, unique amongst the numeric IDs of the event class's
    \bt_stream_cls's event classes.

    Depending on whether or not the event class's stream class
    automatically assigns event class IDs
    (see bt_stream_class_assigns_automatic_event_class_id()),
    set the event class's numeric ID on creation with
    bt_event_class_create() or
    bt_event_class_create_with_id().

    You cannot change the numeric ID once the event class is created.

    Get an event class's numeric ID with bt_event_class_get_id().
  </dd>

  <dt>\anchor api-tir-ev-cls-prop-name \bt_dt_opt Name</dt>
  <dd>
    Name of the event class.

    Use bt_event_class_set_name() and bt_event_class_get_name().
  </dd>

  <dt>\anchor api-tir-ev-cls-prop-log-lvl \bt_dt_opt Log level</dt>
  <dd>
    Log level of the event class.

    The event class's log level corresponds to the log level of the
    tracer's original instrumentation point.

    Use bt_event_class_set_log_level() and
    bt_event_class_get_log_level().
  </dd>

  <dt>
    \anchor api-tir-ev-cls-prop-emf-uri
    \bt_dt_opt Eclipse Modeling Framework (EMF) URI
  </dt>
  <dd>
    EMF URI of the event class.

    Use bt_event_class_set_emf_uri() and
    bt_event_class_get_emf_uri().
  </dd>

  <dt>
    \anchor api-tir-ev-cls-prop-p-fc
    \bt_dt_opt Payload field class
  </dt>
  <dd>
    Payload \bt_fc of the event class.

    The payload of an event class instance (\bt_ev) contains the
    event's main data.

    Use bt_event_class_set_payload_field_class()
    bt_event_class_borrow_payload_field_class(),
    and bt_event_class_borrow_payload_field_class_const().
  </dd>

  <dt>
    \anchor api-tir-ev-cls-prop-sc-fc
    \bt_dt_opt Specific context field class
  </dt>
  <dd>
    Specific context \bt_fc of the event class.

    The specific context of an event class instance (\bt_ev) contains
    any contextual data of which the layout is specific to the
    event's class and which does not belong to the payload.

    Use bt_event_class_set_specific_context_field_class()
    bt_event_class_borrow_specific_context_field_class(),
    and bt_event_class_borrow_specific_context_field_class_const().
  </dd>

  <dt>
    \anchor api-tir-ev-cls-prop-user-attrs
    \bt_dt_opt User attributes
  </dt>
  <dd>
    User attributes of the event class.

    User attributes are custom attributes attached to an event class.

    Use bt_event_class_set_user_attributes(),
    bt_event_class_borrow_user_attributes(), and
    bt_event_class_borrow_user_attributes_const().
  </dd>
</dl>
*/

/*! @{ */

/*!
@name Type
@{

@typedef struct bt_event_class bt_event_class;

@brief
    Event class.

@}
*/

/*!
@name Creation
@{
*/

/*!
@brief
    Creates a default event class and adds it to the \bt_stream_cls
    \bt_p{stream_class}.

@attention
    @parblock
    Only use this function if

    @code
    bt_stream_class_assigns_automatic_event_class_id(stream_class)
    @endcode

    returns #BT_TRUE.

    Otherwise, use bt_event_class_create_with_id().
    @endparblock

On success, the returned event class has the following property values:

<table>
  <tr>
    <th>Property
    <th>Value
  <tr>
    <td>\ref api-tir-ev-cls-prop-id "Numeric ID"
    <td>Automatically assigned by \bt_p{stream_class}
  <tr>
    <td>\ref api-tir-ev-cls-prop-name "Name"
    <td>\em None
  <tr>
    <td>\ref api-tir-ev-cls-prop-log-lvl "Log level"
    <td>\em None
  <tr>
    <td>\ref api-tir-ev-cls-prop-emf-uri "EMF URI"
    <td>\em None
  <tr>
    <td>\ref api-tir-ev-cls-prop-p-fc "Payload field class"
    <td>\em None
  <tr>
    <td>\ref api-tir-ev-cls-prop-sc-fc "Specific context field class"
    <td>\em None
  <tr>
    <td>\ref api-tir-ev-cls-prop-user-attrs "User attributes"
    <td>Empty \bt_map_val
</table>

@param[in] stream_class
    Stream class to add the created event class to.

@returns
    New event class reference, or \c NULL on memory error.

@bt_pre_not_null{stream_class}
@pre
    <code>bt_stream_class_assigns_automatic_event_class_id(stream_class)</code>
    returns #BT_TRUE.

@bt_post_success_frozen{stream_class}

@sa bt_event_class_create_with_id() &mdash;
    Creates an event class with a specific numeric ID and adds it to a
    stream class.
*/
extern bt_event_class *bt_event_class_create(
		bt_stream_class *stream_class);

/*!
@brief
    Creates a default event class with the numeric ID \bt_p{id} and adds
    it to the \bt_stream_cls \bt_p{stream_class}.

@attention
    @parblock
    Only use this function if

    @code
    bt_stream_class_assigns_automatic_event_class_id(stream_class)
    @endcode

    returns #BT_FALSE.

    Otherwise, use bt_event_class_create().
    @endparblock

On success, the returned event class has the following property values:

<table>
  <tr>
    <th>Property
    <th>Value
  <tr>
    <td>\ref api-tir-ev-cls-prop-id "Numeric ID"
    <td>\bt_p{id}
  <tr>
    <td>\ref api-tir-ev-cls-prop-name "Name"
    <td>\em None
  <tr>
    <td>\ref api-tir-ev-cls-prop-log-lvl "Log level"
    <td>\em None
  <tr>
    <td>\ref api-tir-ev-cls-prop-emf-uri "EMF URI"
    <td>\em None
  <tr>
    <td>\ref api-tir-ev-cls-prop-p-fc "Payload field class"
    <td>\em None
  <tr>
    <td>\ref api-tir-ev-cls-prop-sc-fc "Specific context field class"
    <td>\em None
  <tr>
    <td>\ref api-tir-ev-cls-prop-user-attrs "User attributes"
    <td>Empty \bt_map_val
</table>

@param[in] stream_class
    Stream class to add the created event class to.
@param[in] id
    Numeric ID of the event class to create and add to
    \bt_p{stream_class}.

@returns
    New event class reference, or \c NULL on memory error.

@bt_pre_not_null{stream_class}
@pre
    <code>bt_stream_class_assigns_automatic_event_class_id(stream_class)</code>
    returns #BT_FALSE.
@pre
    \bt_p{stream_class} does not contain an event class with the numeric
    ID \bt_p{id}.

@bt_post_success_frozen{stream_class}

@sa bt_event_class_create() &mdash;
    Creates an event class with an automatic numeric ID and adds it to a
    stream class.
*/
extern bt_event_class *bt_event_class_create_with_id(
		bt_stream_class *stream_class, uint64_t id);

/*! @} */

/*!
@name Stream class access
@{
*/

/*!
@brief
    Borrows the \bt_stream_cls which contains the event class
    \bt_p{event_class}.

@param[in] event_class
    Event class from which to borrow the stream class which contains it.

@returns
    Stream class which contains \bt_p{event_class}.

@bt_pre_not_null{event_class}

@sa bt_event_class_borrow_stream_class_const() &mdash;
    \c const version of this function.
*/
extern bt_stream_class *bt_event_class_borrow_stream_class(
		bt_event_class *event_class);

/*!
@brief
    Borrows the \bt_stream_cls which contains the event class
    \bt_p{event_class} (\c const version).

See bt_event_class_borrow_stream_class().
*/
extern const bt_stream_class *bt_event_class_borrow_stream_class_const(
		const bt_event_class *event_class);

/*! @} */

/*!
@name Properties
@{
*/

/*!
@brief
    Returns the numeric ID of the event class \bt_p{event_class}.

See the \ref api-tir-ev-cls-prop-id "numeric ID" property.

@param[in] event_class
    Event class of which to get the numeric ID.

@returns
    Numeric ID of \bt_p{event_class}.

@bt_pre_not_null{event_class}

@sa bt_event_class_create_with_id() &mdash;
    Creates an event class with a specific numeric ID and adds it to a
    stream class.
*/
extern uint64_t bt_event_class_get_id(const bt_event_class *event_class);

/*!
@brief
    Status codes for bt_event_class_set_name().
*/
typedef enum bt_event_class_set_name_status {
	/*!
	@brief
	    Success.
	*/
	BT_EVENT_CLASS_SET_NAME_STATUS_OK		= __BT_FUNC_STATUS_OK,

	/*!
	@brief
	    Out of memory.
	*/
	BT_EVENT_CLASS_SET_NAME_STATUS_MEMORY_ERROR	= __BT_FUNC_STATUS_MEMORY_ERROR,
} bt_event_class_set_name_status;

/*!
@brief
    Sets the name of the event class \bt_p{event_class} to
    a copy of \bt_p{name}.

See the \ref api-tir-ev-cls-prop-name "name" property.

@param[in] event_class
    Event class of which to set the name to \bt_p{name}.
@param[in] name
    New name of \bt_p{event_class} (copied).

@retval #BT_EVENT_CLASS_SET_NAME_STATUS_OK
    Success.
@retval #BT_EVENT_CLASS_SET_NAME_STATUS_MEMORY_ERROR
    Out of memory.

@bt_pre_not_null{event_class}
@bt_pre_hot{event_class}
@bt_pre_not_null{name}

@sa bt_event_class_get_name() &mdash;
    Returns the name of an event class.
*/
extern bt_event_class_set_name_status bt_event_class_set_name(
		bt_event_class *event_class, const char *name);

/*!
@brief
    Returns the name of the event class \bt_p{event_class}.

See the \ref api-tir-ev-cls-prop-name "name" property.

If \bt_p{event_class} has no name, this function returns \c NULL.

@param[in] event_class
    Event class of which to get the name.

@returns
    @parblock
    Name of \bt_p{event_class}, or \c NULL if none.

    The returned pointer remains valid as long as \bt_p{event_class}
    is not modified.
    @endparblock

@bt_pre_not_null{event_class}

@sa bt_event_class_set_name() &mdash;
    Sets the name of an event class.
*/
extern const char *bt_event_class_get_name(const bt_event_class *event_class);

/*!
@brief
    Event class log level enumerators.
*/
typedef enum bt_event_class_log_level {
	/*!
	@brief
	    System is unusable.
	*/
	BT_EVENT_CLASS_LOG_LEVEL_EMERGENCY		= 0,

	/*!
	@brief
	    Action must be taken immediately.
	*/
	BT_EVENT_CLASS_LOG_LEVEL_ALERT			= 1,

	/*!
	@brief
	    Critical conditions.
	*/
	BT_EVENT_CLASS_LOG_LEVEL_CRITICAL		= 2,

	/*!
	@brief
	    Error conditions.
	*/
	BT_EVENT_CLASS_LOG_LEVEL_ERROR			= 3,

	/*!
	@brief
	    Warning conditions.
	*/
	BT_EVENT_CLASS_LOG_LEVEL_WARNING		= 4,

	/*!
	@brief
	    Normal, but significant, condition.
	*/
	BT_EVENT_CLASS_LOG_LEVEL_NOTICE			= 5,

	/*!
	@brief
	    Informational message.
	*/
	BT_EVENT_CLASS_LOG_LEVEL_INFO			= 6,

	/*!
	@brief
	    Debugging information with system-level scope
	    (set of programs).
	*/
	BT_EVENT_CLASS_LOG_LEVEL_DEBUG_SYSTEM		= 7,

	/*!
	@brief
	    Debugging information with program-level scope
	    (set of processes).
	*/
	BT_EVENT_CLASS_LOG_LEVEL_DEBUG_PROGRAM		= 8,

	/*!
	@brief
	    Debugging information with process-level scope
	    (set of modules).
	*/
	BT_EVENT_CLASS_LOG_LEVEL_DEBUG_PROCESS		= 9,

	/*!
	@brief
	    Debugging information with module (executable/library) scope
	    (set of units).
	*/
	BT_EVENT_CLASS_LOG_LEVEL_DEBUG_MODULE		= 10,

	/*!
	@brief
	    Debugging information with compilation unit scope
	    (set of functions).
	*/
	BT_EVENT_CLASS_LOG_LEVEL_DEBUG_UNIT		= 11,

	/*!
	@brief
	    Debugging information with function-level scope.
	*/
	BT_EVENT_CLASS_LOG_LEVEL_DEBUG_FUNCTION		= 12,

	/*!
	@brief
	    Debugging information with function-level scope.
	*/
	BT_EVENT_CLASS_LOG_LEVEL_DEBUG_LINE		= 13,

	/*!
	@brief
	    Debugging-level message.
	*/
	BT_EVENT_CLASS_LOG_LEVEL_DEBUG			= 14,
} bt_event_class_log_level;

/*!
@brief
    Sets the log level of the event class \bt_p{event_class} to
    \bt_p{log_level}.

See the \ref api-tir-ev-cls-prop-log-lvl "log level" property.

@param[in] event_class
    Event class of which to set the log level to \bt_p{log_level}.
@param[in] log_level
    New log level of \bt_p{event_class}.

@bt_pre_not_null{event_class}
@bt_pre_hot{event_class}

@sa bt_event_class_get_log_level() &mdash;
    Returns the log level of an event class.
*/
extern void bt_event_class_set_log_level(bt_event_class *event_class,
		bt_event_class_log_level log_level);

/*!
@brief
    Returns the log level of the event class \bt_p{event_class}.

See the \ref api-tir-ev-cls-prop-log-lvl "log level" property.

@param[in] event_class
    Event class of which to get the log level.
@param[out] log_level
    <strong>If this function returns
    #BT_PROPERTY_AVAILABILITY_AVAILABLE</strong>, \bt_p{*log_level} is
    the log level of \bt_p{event_class}.

@retval #BT_PROPERTY_AVAILABILITY_AVAILABLE
    The log level of \bt_p{event_class} is available.
@retval #BT_PROPERTY_AVAILABILITY_NOT_AVAILABLE
    The log level of \bt_p{event_class} is not available.

@bt_pre_not_null{event_class}
@bt_pre_not_null{log_level}

@sa bt_event_class_set_log_level() &mdash;
    Sets the log level of an event class.
*/
extern bt_property_availability bt_event_class_get_log_level(
		const bt_event_class *event_class,
		bt_event_class_log_level *log_level);

/*!
@brief
    Status codes for bt_event_class_set_emf_uri().
*/
typedef enum bt_event_class_set_emf_uri_status {
	/*!
	@brief
	    Success.
	*/
	BT_EVENT_CLASS_SET_EMF_URI_STATUS_OK		= __BT_FUNC_STATUS_OK,

	/*!
	@brief
	    Out of memory.
	*/
	BT_EVENT_CLASS_SET_EMF_URI_STATUS_MEMORY_ERROR	= __BT_FUNC_STATUS_MEMORY_ERROR,
} bt_event_class_set_emf_uri_status;

/*!
@brief
    Sets the Eclipse Modeling Framework (EMF) URI of the event class
    \bt_p{event_class} to a copy of \bt_p{emf_uri}.

See the \ref api-tir-ev-cls-prop-emf-uri "EMF URI" property.

@param[in] event_class
    Event class of which to set the EMF URI to \bt_p{emf_uri}.
@param[in] emf_uri
    New EMF URI of \bt_p{event_class} (copied).

@retval #BT_EVENT_CLASS_SET_EMF_URI_STATUS_OK
    Success.
@retval #BT_EVENT_CLASS_SET_EMF_URI_STATUS_MEMORY_ERROR
    Out of memory.

@bt_pre_not_null{event_class}
@bt_pre_hot{event_class}
@bt_pre_not_null{emf_uri}

@sa bt_event_class_get_emf_uri() &mdash;
    Returns the EMF URI of an event class.
*/
extern bt_event_class_set_emf_uri_status bt_event_class_set_emf_uri(
		bt_event_class *event_class, const char *emf_uri);

/*!
@brief
    Returns the Eclipse Modeling Framework (EMF) URI of the event
    class \bt_p{event_class}.

See the \ref api-tir-ev-cls-prop-emf-uri "EMF URI" property.

If \bt_p{event_class} has no EMF URI, this function returns \c NULL.

@param[in] event_class
    Event class of which to get the EMF URI.

@returns
    @parblock
    EMF URI of \bt_p{event_class}, or \c NULL if none.

    The returned pointer remains valid as long as \bt_p{event_class}
    is not modified.
    @endparblock

@bt_pre_not_null{event_class}

@sa bt_event_class_set_emf_uri() &mdash;
    Sets the EMF URI of an event class.
*/
extern const char *bt_event_class_get_emf_uri(
		const bt_event_class *event_class);

/*!
@brief
    Status codes for bt_event_class_set_payload_field_class() and
    bt_event_class_set_specific_context_field_class().
*/
typedef enum bt_event_class_set_field_class_status {
	/*!
	@brief
	    Success.
	*/
	BT_EVENT_CLASS_SET_FIELD_CLASS_STATUS_OK		= __BT_FUNC_STATUS_OK,

	/*!
	@brief
	    Out of memory.
	*/
	BT_EVENT_CLASS_SET_FIELD_CLASS_STATUS_MEMORY_ERROR	= __BT_FUNC_STATUS_MEMORY_ERROR,
} bt_event_class_set_field_class_status;

/*!
@brief
    Sets the payload \bt_fc of the event class
    \bt_p{event_class} to \bt_p{field_class}.

See the \ref api-tir-ev-cls-prop-p-fc "payload field class" property.

@param[in] event_class
    Event class of which to set the payload field class to
    \bt_p{field_class}.
@param[in] field_class
    New payload field class of \bt_p{event_class}.

@retval #BT_EVENT_CLASS_SET_FIELD_CLASS_STATUS_OK
    Success.
@retval #BT_EVENT_CLASS_SET_FIELD_CLASS_STATUS_MEMORY_ERROR
    Out of memory.

@bt_pre_not_null{event_class}
@bt_pre_hot{event_class}
@bt_pre_not_null{field_class}
@bt_pre_is_struct_fc{field_class}
@pre
    \bt_p{field_class}, or any of its contained field classes,
    is not already part of a \bt_stream_cls or of an event class.
@pre
    If any of the field classes recursively contained in
    \bt_p{field_class} has a
    \ref api-tir-fc-link "link to another field class", it must honor
    the field class link rules.

@bt_post_success_frozen{field_class}

@sa bt_event_class_borrow_payload_field_class() &mdash;
    Borrows the payload field class of an event class.
@sa bt_event_class_borrow_payload_field_class_const() &mdash;
    Borrows the payload field class of an event class
    (\c const version).
*/
extern bt_event_class_set_field_class_status
bt_event_class_set_payload_field_class(bt_event_class *event_class,
		bt_field_class *field_class);

/*!
@brief
    Borrows the payload \bt_fc from the event class \bt_p{event_class}.

See the \ref api-tir-ev-cls-prop-p-fc "payload field class" property.

If \bt_p{event_class} has no payload field class, this function
returns \c NULL.

@param[in] event_class
    Event class from which to borrow the payload field class.

@returns
    \em Borrowed reference of the payload field class of
    \bt_p{event_class}, or \c NULL if none.

@bt_pre_not_null{event_class}

@sa bt_event_class_set_payload_field_class() &mdash;
    Sets the payload field class of an event class.
@sa bt_event_class_borrow_payload_field_class_const() &mdash;
    \c const version of this function.
*/
extern bt_field_class *bt_event_class_borrow_payload_field_class(
		bt_event_class *event_class);

/*!
@brief
    Borrows the payload \bt_fc from the event class \bt_p{event_class}
    (\c const version).

See bt_event_class_borrow_payload_field_class().
*/
extern const bt_field_class *bt_event_class_borrow_payload_field_class_const(
		const bt_event_class *event_class);

/*!
@brief
    Sets the specific context \bt_fc of the event class
    \bt_p{event_class} to \bt_p{field_class}.

See the \ref api-tir-ev-cls-prop-sc-fc "specific context field class"
property.

@param[in] event_class
    Event class of which to set the specific context field class to
    \bt_p{field_class}.
@param[in] field_class
    New specific context field class of \bt_p{event_class}.

@retval #BT_EVENT_CLASS_SET_FIELD_CLASS_STATUS_OK
    Success.
@retval #BT_EVENT_CLASS_SET_FIELD_CLASS_STATUS_MEMORY_ERROR
    Out of memory.

@bt_pre_not_null{event_class}
@bt_pre_hot{event_class}
@bt_pre_not_null{field_class}
@bt_pre_is_struct_fc{field_class}
@pre
    \bt_p{field_class}, or any of its contained field classes,
    is not already part of a \bt_stream_cls or of an event class.
@pre
    If any of the field classes recursively contained in
    \bt_p{field_class} has a
    \ref api-tir-fc-link "link to another field class", it must honor
    the field class link rules.

@bt_post_success_frozen{field_class}

@sa bt_event_class_borrow_specific_context_field_class() &mdash;
    Borrows the specific context field class of an event class.
@sa bt_event_class_borrow_specific_context_field_class_const() &mdash;
    Borrows the specific context field class of an event class
    (\c const version).
*/
extern bt_event_class_set_field_class_status
bt_event_class_set_specific_context_field_class(bt_event_class *event_class,
		bt_field_class *field_class);

/*!
@brief
    Borrows the specific context \bt_fc from the event class
    \bt_p{event_class}.

See the \ref api-tir-ev-cls-prop-sc-fc "specific context field class"
property.

If \bt_p{event_class} has no specific context field class, this function
returns \c NULL.

@param[in] event_class
    Event class from which to borrow the specific context field class.

@returns
    \em Borrowed reference of the specific context field class of
    \bt_p{event_class}, or \c NULL if none.

@bt_pre_not_null{event_class}

@sa bt_event_class_set_specific_context_field_class() &mdash;
    Sets the specific context field class of an event class.
@sa bt_event_class_borrow_specific_context_field_class_const() &mdash;
    \c const version of this function.
*/
extern bt_field_class *
bt_event_class_borrow_specific_context_field_class(bt_event_class *event_class);

/*!
@brief
    Borrows the specific context \bt_fc from the event class
    \bt_p{event_class} (\c const version).

See bt_event_class_borrow_specific_context_field_class().
*/
extern const bt_field_class *
bt_event_class_borrow_specific_context_field_class_const(
		const bt_event_class *event_class);

/*!
@brief
    Sets the user attributes of the event class \bt_p{event_class} to
    \bt_p{user_attributes}.

See the \ref api-tir-ev-cls-prop-user-attrs "user attributes" property.

@note
    When you create a default event class with bt_event_class_create()
    or bt_event_class_create_with_id(), the event class's initial user
    attributes is an empty \bt_map_val. Therefore you can borrow it with
    bt_event_class_borrow_user_attributes() and fill it directly instead
    of setting a new one with this function.

@param[in] event_class
    Event class of which to set the user attributes to
    \bt_p{user_attributes}.
@param[in] user_attributes
    New user attributes of \bt_p{event_class}.

@bt_pre_not_null{event_class}
@bt_pre_hot{event_class}
@bt_pre_not_null{user_attributes}
@bt_pre_is_map_val{user_attributes}

@sa bt_event_class_borrow_user_attributes() &mdash;
    Borrows the user attributes of an event class.
*/
extern void bt_event_class_set_user_attributes(
		bt_event_class *event_class, const bt_value *user_attributes);

/*!
@brief
    Borrows the user attributes of the event class \bt_p{event_class}.

See the \ref api-tir-ev-cls-prop-user-attrs "user attributes" property.

@note
    When you create a default event class with bt_event_class_create()
    or bt_event_class_create_with_id(), the event class's initial user
    attributes is an empty \bt_map_val.

@param[in] event_class
    Event class from which to borrow the user attributes.

@returns
    User attributes of \bt_p{event_class} (a \bt_map_val).

@bt_pre_not_null{event_class}

@sa bt_event_class_set_user_attributes() &mdash;
    Sets the user attributes of an event class.
@sa bt_event_class_borrow_user_attributes_const() &mdash;
    \c const version of this function.
*/
extern bt_value *bt_event_class_borrow_user_attributes(
		bt_event_class *event_class);

/*!
@brief
    Borrows the user attributes of the event class \bt_p{event_class}
    (\c const version).

See bt_event_class_borrow_user_attributes().
*/
extern const bt_value *bt_event_class_borrow_user_attributes_const(
		const bt_event_class *event_class);

/*! @} */

/*!
@name Reference count
@{
*/

/*!
@brief
    Increments the \ref api-fund-shared-object "reference count" of
    the event class \bt_p{event_class}.

@param[in] event_class
    @parblock
    Event class of which to increment the reference count.

    Can be \c NULL.
    @endparblock

@sa bt_event_class_put_ref() &mdash;
    Decrements the reference count of an event class.
*/
extern void bt_event_class_get_ref(const bt_event_class *event_class);

/*!
@brief
    Decrements the \ref api-fund-shared-object "reference count" of
    the event class \bt_p{event_class}.

@param[in] event_class
    @parblock
    Event class of which to decrement the reference count.

    Can be \c NULL.
    @endparblock

@sa bt_event_class_get_ref() &mdash;
    Increments the reference count of an event class.
*/
extern void bt_event_class_put_ref(const bt_event_class *event_class);

/*!
@brief
    Decrements the reference count of the event class
    \bt_p{_event_class}, and then sets \bt_p{_event_class} to \c NULL.

@param _event_class
    @parblock
    Event class of which to decrement the reference count.

    Can contain \c NULL.
    @endparblock

@bt_pre_assign_expr{_event_class}
*/
#define BT_EVENT_CLASS_PUT_REF_AND_RESET(_event_class)	\
	do {						\
		bt_event_class_put_ref(_event_class);	\
		(_event_class) = NULL;			\
	} while (0)

/*!
@brief
    Decrements the reference count of the event class \bt_p{_dst}, sets
    \bt_p{_dst} to \bt_p{_src}, and then sets \bt_p{_src} to \c NULL.

This macro effectively moves an event class reference from the expression
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
#define BT_EVENT_CLASS_MOVE_REF(_dst, _src)	\
	do {					\
		bt_event_class_put_ref(_dst);	\
		(_dst) = (_src);		\
		(_src) = NULL;			\
	} while (0)

/*! @} */

/*! @} */

#ifdef __cplusplus
}
#endif

#endif /* BABELTRACE2_TRACE_IR_EVENT_CLASS_H */
