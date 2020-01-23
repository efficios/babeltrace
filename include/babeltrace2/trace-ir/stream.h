#ifndef BABELTRACE2_TRACE_IR_STREAM_H
#define BABELTRACE2_TRACE_IR_STREAM_H

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
@defgroup api-tir-stream Stream
@ingroup api-tir

@brief
    Trace stream.

A <strong><em>stream</em></strong> is a conceptual
\ref api-msg-seq "sequence of messages" within a \bt_trace:

@image html trace-structure.png

In the illustration above, notice that:

- A \bt_stream is a conceptual sequence of \bt_p_msg.

  The sequence always starts with a \bt_sb_msg and ends with a
  \bt_se_msg.

- A stream is an instance of a \bt_stream_cls.

- A \bt_trace contains one or more streams.

A stream is a \ref api-tir "trace IR" data object.

A stream is said to be a \em conceptual sequence of messages because the
stream object itself does not contain messages: it only represents a
common timeline to which messages are associated.

\bt_cp_comp exchange messages, within a trace processing \bt_graph,
which can belong to different streams, as long as the stream clocks are
\ref api-tir-clock-cls-origin "correlatable".

A typical use case for streams is to use one for each traced CPU. Then
the messages related to a given stream are the ones which occurred on a
given CPU. Other schemes are possible: they are completely
application-specific, and \bt_name does not enforce any specific
stream arrangement pattern.

A \bt_trace contains streams. All the streams of a
given trace, for a given stream class, have unique numeric IDs. Borrow
the trace which contains a stream with bt_stream_borrow_trace() or
bt_stream_borrow_trace_const().

A \bt_stream can conceptually contain a default clock if its class
has a \ref api-tir-stream-cls-prop-def-clock-cls "default clock class".
There's no function to access a stream's default clock because it's
a stateful object: \bt_p_msg cannot refer to stateful objects
because they must not change while being transported from one
\bt_comp to the other. Instead of having a stream default clock object,
\bt_p_msg have a default \bt_cs: this is a snapshot of the value of a
stream's default clock (a \bt_clock_cls instance):

@image html clocks.png

In the illustration above, notice that:

- Streams (horizontal blue rectangles) are instances of a
  \bt_stream_cls (orange).

- A stream class has a default \bt_clock_cls (orange bell alarm clock).

- Each stream has a default clock (yellow bell alarm clock): this is an
  instance of the stream's class's default clock class.

- Each \bt_msg (objects in blue stream rectangles) created for a given
  stream has a default \bt_cs (yellow star): this is a snapshot of the
  stream's default clock.

  In other words, a default clock snapshot contains the value of the
  stream's default clock when this message occurred.

To create a stream:

<dl>
  <dt>
    If bt_stream_class_assigns_automatic_stream_id() returns
    #BT_TRUE (the default) for the stream class to use
  </dt>
  <dd>Use bt_stream_create().</dd>

  <dt>
    If bt_stream_class_assigns_automatic_stream_id() returns
    #BT_FALSE for the stream class to use
  </dt>
  <dd>Use bt_stream_create_with_id().</dd>
</dl>

A stream is a \ref api-fund-shared-object "shared object": get a
new reference with bt_stream_get_ref() and put an existing
reference with bt_stream_put_ref().

Some library functions \ref api-fund-freezing "freeze" streams on
success. The documentation of those functions indicate this
postcondition.

The type of a stream is #bt_stream.

<h1>Properties</h1>

A stream has the following property:

<dl>
  <dt>\anchor api-tir-stream-prop-id Numeric ID</dt>
  <dd>
    Numeric ID, unique amongst the numeric IDs of the stream's
    \bt_trace's streams for a given \bt_stream_cls. In other words,
    two streams which belong to the same trace can have the same numeric
    ID if they aren't instances of the same class.

    Depending on whether or not the stream's class
    automatically assigns stream IDs
    (see bt_stream_class_assigns_automatic_stream_id()),
    set the stream's numeric ID on creation with
    bt_stream_create() or bt_stream_create_with_id().

    You cannot change the numeric ID once the stream is created.

    Get a stream's numeric ID with bt_stream_get_id().
  </dd>

  <dt>\anchor api-tir-stream-prop-name \bt_dt_opt Name</dt>
  <dd>
    Name of the stream.

    Use bt_stream_set_name() and bt_stream_get_name().
  </dd>

  <dt>
    \anchor api-tir-stream-prop-user-attrs
    \bt_dt_opt User attributes
  </dt>
  <dd>
    User attributes of the stream.

    User attributes are custom attributes attached to a stream.

    Use bt_stream_set_user_attributes(),
    bt_stream_borrow_user_attributes(), and
    bt_stream_borrow_user_attributes_const().
  </dd>
</dl>
*/

/*! @{ */

/*!
@name Type
@{

@typedef struct bt_stream bt_stream;

@brief
    Stream.

@}
*/

/*!
@name Creation
@{
*/

/*!
@brief
    Creates a stream from the \bt_stream_cls \bt_p{stream_class} and
    adds it to the \bt_trace \bt_p{trace}.

This function instantiates \bt_p{stream_class}.

@attention
    @parblock
    Only use this function if

    @code
    bt_stream_class_assigns_automatic_stream_id(stream_class)
    @endcode

    returns #BT_TRUE.

    Otherwise, use bt_stream_create_with_id().
    @endparblock

On success, the returned stream has the following property values:

<table>
  <tr>
    <th>Property
    <th>Value
  <tr>
    <td>\ref api-tir-stream-prop-id "Numeric ID"
    <td>Automatically assigned by \bt_p{stream_class} and \bt_p{trace}
  <tr>
    <td>\ref api-tir-stream-prop-name "Name"
    <td>\em None
  <tr>
    <td>\ref api-tir-stream-prop-user-attrs "User attributes"
    <td>Empty \bt_map_val
</table>

@param[in] stream_class
    Stream class from which to create the stream.
@param[in] trace
    Trace to add the created stream to.

@returns
    New stream reference, or \c NULL on memory error.

@bt_pre_not_null{stream_class}
@pre
    <code>bt_stream_class_assigns_automatic_stream_id(stream_class)</code>
    returns #BT_TRUE.
@bt_pre_not_null{trace}

@bt_post_success_frozen{stream_class}
@bt_post_success_frozen{trace}

@sa bt_stream_create_with_id() &mdash;
    Creates a stream with a specific numeric ID and adds it to a
    trace.
*/
extern bt_stream *bt_stream_create(bt_stream_class *stream_class,
		bt_trace *trace);

/*!
@brief
    Creates a stream with the numeric ID \bt_p{id}
    from the \bt_stream_cls \bt_p{stream_class} and adds
    it to the \bt_trace \bt_p{trace}.

This function instantiates \bt_p{stream_class}.

@attention
    @parblock
    Only use this function if

    @code
    bt_stream_class_assigns_automatic_stream_id(stream_class)
    @endcode

    returns #BT_FALSE.

    Otherwise, use bt_stream_create().
    @endparblock

On success, the returned stream has the following property values:

<table>
  <tr>
    <th>Property
    <th>Value
  <tr>
    <td>\ref api-tir-stream-prop-id "Numeric ID"
    <td>\bt_p{id}
  <tr>
    <td>\ref api-tir-stream-prop-name "Name"
    <td>\em None
  <tr>
    <td>\ref api-tir-stream-prop-user-attrs "User attributes"
    <td>Empty \bt_map_val
</table>

@param[in] stream_class
    Stream class from which to create the stream.
@param[in] trace
    Trace to add the created stream to.
@param[in] id
    Numeric ID of the stream to create and add to \bt_p{trace}.

@returns
    New stream reference, or \c NULL on memory error.

@bt_pre_not_null{stream_class}
@pre
    <code>bt_stream_class_assigns_automatic_stream_id(stream_class)</code>
    returns #BT_FALSE.
@bt_pre_not_null{trace}
@pre
    \bt_p{trace} does not contain an instance of \bt_p{stream_class}
    with the numeric ID \bt_p{id}.

@bt_post_success_frozen{stream_class}
@bt_post_success_frozen{trace}

@sa bt_stream_create() &mdash;
    Creates a stream with an automatic numeric ID and adds it to a
    trace.
*/
extern bt_stream *bt_stream_create_with_id(
		bt_stream_class *stream_class,
		bt_trace *trace, uint64_t id);

/*! @} */

/*!
@name Class access
@{
*/

/*!
@brief
    Borrows the \ref api-tir-stream-cls "class" of the stream
    \bt_p{stream}.

@param[in] stream
    Stream of which to borrow the class.

@returns
    \em Borrowed reference of the class of \bt_p{stream}.

@bt_pre_not_null{stream}

@sa bt_stream_borrow_class_const() &mdash;
    \c const version of this function.
*/
extern bt_stream_class *bt_stream_borrow_class(bt_stream *stream);

/*!
@brief
    Borrows the \ref api-tir-stream-cls "class" of the stream
    \bt_p{stream} (\c const version).

See bt_stream_borrow_class().
*/
extern const bt_stream_class *bt_stream_borrow_class_const(
		const bt_stream *stream);

/*! @} */

/*!
@name Trace access
@{
*/

/*!
@brief
    Borrows the \bt_trace which contains the stream \bt_p{stream}.

@param[in] stream
    Stream of which to borrow the trace containing it.

@returns
    \em Borrowed reference of the trace containing \bt_p{stream}.

@bt_pre_not_null{stream}

@sa bt_stream_borrow_trace_const() &mdash;
    \c const version of this function.
*/
extern bt_trace *bt_stream_borrow_trace(bt_stream *stream);

/*!
@brief
    Borrows the \bt_trace which contains the stream \bt_p{stream}
    (\c const version).

See bt_stream_borrow_trace().
*/
extern const bt_trace *bt_stream_borrow_trace_const(
		const bt_stream *stream);

/*! @} */

/*!
@name Properties
@{
*/

/*!
@brief
    Returns the numeric ID of the stream \bt_p{stream}.

See the \ref api-tir-stream-prop-id "numeric ID" property.

@param[in] stream
    Stream of which to get the numeric ID.

@returns
    Numeric ID of \bt_p{stream}.

@bt_pre_not_null{stream}

@sa bt_stream_create_with_id() &mdash;
    Creates a stream with a specific numeric ID and adds it to a
    trace.
*/
extern uint64_t bt_stream_get_id(const bt_stream *stream);

/*!
@brief
    Status codes for bt_stream_set_name().
*/
typedef enum bt_stream_set_name_status {
	/*!
	@brief
	    Success.
	*/
	BT_STREAM_SET_NAME_STATUS_OK		= __BT_FUNC_STATUS_OK,

	/*!
	@brief
	    Out of memory.
	*/
	BT_STREAM_SET_NAME_STATUS_MEMORY_ERROR	= __BT_FUNC_STATUS_MEMORY_ERROR,
} bt_stream_set_name_status;

/*!
@brief
    Sets the name of the stream \bt_p{stream} to
    a copy of \bt_p{name}.

See the \ref api-tir-stream-prop-name "name" property.

@param[in] stream
    Stream of which to set the name to \bt_p{name}.
@param[in] name
    New name of \bt_p{stream} (copied).

@retval #BT_STREAM_CLASS_SET_NAME_STATUS_OK
    Success.
@retval #BT_STREAM_CLASS_SET_NAME_STATUS_MEMORY_ERROR
    Out of memory.

@bt_pre_not_null{stream}
@bt_pre_hot{stream}
@bt_pre_not_null{name}

@sa bt_stream_get_name() &mdash;
    Returns the name of a stream.
*/
extern bt_stream_set_name_status bt_stream_set_name(bt_stream *stream,
		const char *name);

/*!
@brief
    Returns the name of the stream \bt_p{stream}.

See the \ref api-tir-stream-prop-name "name" property.

If \bt_p{stream} has no name, this function returns \c NULL.

@param[in] stream
    Stream of which to get the name.

@returns
    @parblock
    Name of \bt_p{stream}, or \c NULL if none.

    The returned pointer remains valid as long as \bt_p{stream}
    is not modified.
    @endparblock

@bt_pre_not_null{stream}

@sa bt_stream_class_set_name() &mdash;
    Sets the name of a stream.
*/
extern const char *bt_stream_get_name(const bt_stream *stream);

/*!
@brief
    Sets the user attributes of the stream \bt_p{stream} to
    \bt_p{user_attributes}.

See the \ref api-tir-stream-prop-user-attrs "user attributes"
property.

@note
    When you create a default stream with bt_stream_create()
    or bt_stream_create_with_id(), the stream's initial user
    attributes is an empty \bt_map_val. Therefore you can borrow it with
    bt_stream_borrow_user_attributes() and fill it directly
    instead of setting a new one with this function.

@param[in] stream
    Stream of which to set the user attributes to
    \bt_p{user_attributes}.
@param[in] user_attributes
    New user attributes of \bt_p{stream}.

@bt_pre_not_null{stream}
@bt_pre_hot{stream}
@bt_pre_not_null{user_attributes}
@bt_pre_is_map_val{user_attributes}

@sa bt_stream_borrow_user_attributes() &mdash;
    Borrows the user attributes of a stream.
*/
extern void bt_stream_set_user_attributes(
		bt_stream *stream, const bt_value *user_attributes);

/*!
@brief
    Borrows the user attributes of the stream \bt_p{stream}.

See the \ref api-tir-stream-prop-user-attrs "user attributes"
property.

@note
    When you create a default stream with bt_stream_create()
    or bt_stream_create_with_id(), the stream's initial user
    attributes is an empty \bt_map_val.

@param[in] stream
    Stream from which to borrow the user attributes.

@returns
    User attributes of \bt_p{stream} (a \bt_map_val).

@bt_pre_not_null{stream}

@sa bt_stream_set_user_attributes() &mdash;
    Sets the user attributes of a stream.
@sa bt_stream_borrow_user_attributes_const() &mdash;
    \c const version of this function.
*/
extern bt_value *bt_stream_borrow_user_attributes(bt_stream *stream);

/*!
@brief
    Borrows the user attributes of the stream \bt_p{stream}
    (\c const version).

See bt_stream_borrow_user_attributes().
*/
extern const bt_value *bt_stream_borrow_user_attributes_const(
		const bt_stream *stream);

/*! @} */

/*!
@name Reference count
@{
*/

/*!
@brief
    Increments the \ref api-fund-shared-object "reference count" of
    the stream \bt_p{stream}.

@param[in] stream
    @parblock
    Stream of which to increment the reference count.

    Can be \c NULL.
    @endparblock

@sa bt_stream_put_ref() &mdash;
    Decrements the reference count of a stream.
*/
extern void bt_stream_get_ref(const bt_stream *stream);

/*!
@brief
    Decrements the \ref api-fund-shared-object "reference count" of
    the stream \bt_p{stream}.

@param[in] stream
    @parblock
    Stream of which to decrement the reference count.

    Can be \c NULL.
    @endparblock

@sa bt_stream_get_ref() &mdash;
    Increments the reference count of a stream.
*/
extern void bt_stream_put_ref(const bt_stream *stream);

/*!
@brief
    Decrements the reference count of the stream
    \bt_p{_stream}, and then sets \bt_p{_stream} to \c NULL.

@param _stream
    @parblock
    Stream of which to decrement the reference count.

    Can contain \c NULL.
    @endparblock

@bt_pre_assign_expr{_stream}
*/
#define BT_STREAM_PUT_REF_AND_RESET(_stream)		\
	do {						\
		bt_stream_put_ref(_stream);		\
		(_stream) = NULL;			\
	} while (0)

/*!
@brief
    Decrements the reference count of the stream \bt_p{_dst}, sets
    \bt_p{_dst} to \bt_p{_src}, and then sets \bt_p{_src} to \c NULL.

This macro effectively moves a stream reference from the expression
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
#define BT_STREAM_MOVE_REF(_dst, _src)		\
	do {					\
		bt_stream_put_ref(_dst);	\
		(_dst) = (_src);		\
		(_src) = NULL;			\
	} while (0)

/*! @} */

/*! @} */

#ifdef __cplusplus
}
#endif

#endif /* BABELTRACE2_TRACE_IR_STREAM_H */
