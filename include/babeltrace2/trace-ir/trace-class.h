 #ifndef BABELTRACE2_TRACE_IR_TRACE_CLASS_H
#define BABELTRACE2_TRACE_IR_TRACE_CLASS_H

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
@defgroup api-tir-trace-cls Trace class
@ingroup api-tir

@brief
    Class of \bt_p_trace.

A <strong><em>trace class</em></strong> is the class of \bt_p_trace:

@image html trace-structure.png

In the illustration above, notice that a trace is an instance of a
trace class.

A trace class is a \ref api-tir "trace IR" metadata object.

A trace class is a \ref api-fund-shared-object "shared object": get a
new reference with bt_trace_class_get_ref() and put an existing
reference with bt_trace_class_put_ref().

Some library functions \ref api-fund-freezing "freeze" trace classes on
success. The documentation of those functions indicate this
postcondition. With a frozen trace class, you can still:

- Create and add a \bt_p_stream_cls to it with
  bt_stream_class_create() or bt_stream_class_create_with_id().
- Add a destruction listener to it with
  bt_trace_class_add_destruction_listener().

The type of a trace class is #bt_trace_class.

A trace class contains \bt_p_stream_cls. All the stream classes of a
given trace class have unique
\ref api-tir-stream-cls-prop-id "numeric IDs". Get the number of stream
classes in a trace class with bt_trace_class_get_stream_class_count().
Borrow a specific stream class from a trace class with
bt_trace_class_borrow_stream_class_by_index(),
bt_trace_class_borrow_stream_class_by_index_const(),
bt_trace_class_borrow_stream_class_by_id(), or
bt_trace_class_borrow_stream_class_by_id_const().

Set whether or not the \bt_p_stream_cls you create for a trace class get
automatic numeric IDs with
bt_trace_class_set_assigns_automatic_stream_class_id().

Create a default trace class from a \bt_self_comp with
bt_trace_class_create().

Add to and remove a destruction listener from a trace class with
bt_trace_class_add_destruction_listener() and
bt_trace_class_remove_destruction_listener().

<h1>Properties</h1>

A trace class has the following properties:

<dl>
  <dt>
    \anchor api-tir-trace-cls-prop-auto-sc-id
    Assigns automatic stream class IDs?
  </dt>
  <dd>
    Whether or not the stream classes you create and add to the trace
    class get \ref api-tir-stream-cls-prop-id "numeric IDs"
    automatically.

    Depending on the value of this property, to create a stream class
    and add it to the trace class:

    <dl>
      <dt>#BT_TRUE</dt>
      <dd>
        Use bt_stream_class_create().
      </dd>

      <dt>#BT_FALSE</dt>
      <dd>
        Use bt_stream_class_create_with_id().
      </dd>
    </dl>

    Use bt_trace_class_set_assigns_automatic_stream_class_id()
    and bt_trace_class_assigns_automatic_stream_class_id().
  </dd>

  <dt>
    \anchor api-tir-trace-cls-prop-user-attrs
    \bt_dt_opt User attributes
  </dt>
  <dd>
    User attributes of the trace class.

    User attributes are custom attributes attached to a trace class.

    Use bt_trace_class_set_user_attributes(),
    bt_trace_class_borrow_user_attributes(), and
    bt_trace_class_borrow_user_attributes_const().
  </dd>
</dl>
*/

/*! @{ */

/*!
@name Type
@{

@typedef struct bt_trace_class bt_trace_class;

@brief
    Trace class.

@}
*/

/*!
@name Creation
@{
*/

/*!
@brief
    Creates a default trace class from the \bt_self_comp
    \bt_p{self_component}.

On success, the returned trace class has the following property values:

<table>
  <tr>
    <th>Property
    <th>Value
  <tr>
    <td>\ref api-tir-trace-cls-prop-auto-sc-id "Assigns automatic stream class IDs?"
    <td>Yes
  <tr>
    <td>\ref api-tir-trace-cls-prop-user-attrs "User attributes"
    <td>Empty \bt_map_val
</table>

@param[in] self_component
    Self component from which to create the default trace class.

@returns
    New trace class reference, or \c NULL on memory error.
*/
extern bt_trace_class *bt_trace_class_create(bt_self_component *self_component);

/*! @} */

/*!
@name Stream class access
@{
*/

/*!
@brief
    Returns the number of \bt_p_stream_cls contained in the trace
    class \bt_p{trace_class}.

@param[in] trace_class
    Trace class of which to get the number of contained stream classes.

@returns
    Number of contained stream classes in \bt_p{trace_class}.

@bt_pre_not_null{trace_class}
*/
extern uint64_t bt_trace_class_get_stream_class_count(
		const bt_trace_class *trace_class);

/*!
@brief
    Borrows the \bt_stream_cls at index \bt_p{index} from the
    trace class \bt_p{trace_class}.

@param[in] trace_class
    Trace class from which to borrow the stream class at index
    \bt_p{index}.
@param[in] index
    Index of the stream class to borrow from \bt_p{trace_class}.

@returns
    @parblock
    \em Borrowed reference of the stream class of
    \bt_p{trace_class} at index \bt_p{index}.

    The returned pointer remains valid as long as \bt_p{trace_class}
    exists.
    @endparblock

@bt_pre_not_null{trace_class}
@pre
    \bt_p{index} is less than the number of stream classes in
    \bt_p{trace_class} (as returned by
    bt_trace_class_get_stream_class_count()).

@sa bt_trace_class_get_stream_class_count() &mdash;
    Returns the number of stream classes contained in a trace class.
@sa bt_trace_class_borrow_stream_class_by_index_const() &mdash;
    \c const version of this function.
*/
extern bt_stream_class *bt_trace_class_borrow_stream_class_by_index(
		bt_trace_class *trace_class, uint64_t index);

/*!
@brief
    Borrows the \bt_stream_cls at index \bt_p{index} from the
    trace class \bt_p{trace_class} (\c const version).

See bt_trace_class_borrow_stream_class_by_index().
*/
extern const bt_stream_class *
bt_trace_class_borrow_stream_class_by_index_const(
		const bt_trace_class *trace_class, uint64_t index);

/*!
@brief
    Borrows the \bt_stream_cls having the numeric ID \bt_p{id} from the
    trace class \bt_p{trace_class}.

If there's no stream class having the numeric ID \bt_p{id} in
\bt_p{trace_class}, this function returns \c NULL.

@param[in] trace_class
    Trace class from which to borrow the stream class having the
    numeric ID \bt_p{id}.
@param[in] id
    ID of the stream class to borrow from \bt_p{trace_class}.

@returns
    @parblock
    \em Borrowed reference of the stream class of
    \bt_p{trace_class} having the numeric ID \bt_p{id}, or \c NULL
    if none.

    The returned pointer remains valid as long as \bt_p{trace_class}
    exists.
    @endparblock

@bt_pre_not_null{trace_class}

@sa bt_trace_class_borrow_stream_class_by_id_const() &mdash;
    \c const version of this function.
*/
extern bt_stream_class *bt_trace_class_borrow_stream_class_by_id(
		bt_trace_class *trace_class, uint64_t id);

/*!
@brief
    Borrows the \bt_stream_cls having the numeric ID \bt_p{id} from the
    trace class \bt_p{trace_class} (\c const version).

See bt_trace_class_borrow_stream_class_by_id().
*/
extern const bt_stream_class *bt_trace_class_borrow_stream_class_by_id_const(
		const bt_trace_class *trace_class, uint64_t id);

/*! @} */

/*!
@name Properties
@{
*/

/*!
@brief
    Sets whether or not the trace class \bt_p{trace_class} automatically
    assigns a numeric ID to a \bt_stream_cls you create and add to it.

See the \ref api-tir-trace-cls-prop-auto-sc-id "assigns automatic stream class IDs?"
property.

@param[in] trace_class
    Trace class of which to set whether or not it assigns automatic
    stream class IDs.
@param[in] assigns_automatic_stream_class_id
    #BT_TRUE to make \bt_p{trace_class} assign automatic stream class
    IDs.

@bt_pre_not_null{trace_class}
@bt_pre_hot{trace_class}

@sa bt_trace_class_assigns_automatic_stream_class_id() &mdash;
    Returns whether or not a trace class automatically assigns
    stream class IDs.
*/
extern void bt_trace_class_set_assigns_automatic_stream_class_id(
		bt_trace_class *trace_class,
		bt_bool assigns_automatic_stream_class_id);

/*!
@brief
    Returns whether or not the trace class \bt_p{trace_class}
    automatically assigns a numeric ID to a \bt_stream_cls you create
    and add to it.

See the \ref api-tir-trace-cls-prop-auto-sc-id "assigns automatic stream class IDs?"
property.

@param[in] trace_class
    Trace class of which to get whether or not it assigns automatic
    stream class IDs.

@returns
    #BT_TRUE if \bt_p{trace_class} automatically
    assigns stream class IDs.

@bt_pre_not_null{trace_class}

@sa bt_trace_class_set_assigns_automatic_stream_class_id() &mdash;
    Sets whether or not a trace class automatically assigns
    stream class IDs.
*/
extern bt_bool bt_trace_class_assigns_automatic_stream_class_id(
		const bt_trace_class *trace_class);

/*!
@brief
    Sets the user attributes of the trace class \bt_p{trace_class} to
    \bt_p{user_attributes}.

See the \ref api-tir-trace-cls-prop-user-attrs "user attributes"
property.

@note
    When you create a default trace class with bt_trace_class_create()
    or bt_trace_class_create_with_id(), the trace class's initial user
    attributes is an empty \bt_map_val. Therefore you can borrow it with
    bt_trace_class_borrow_user_attributes() and fill it directly
    instead of setting a new one with this function.

@param[in] trace_class
    Trace class of which to set the user attributes to
    \bt_p{user_attributes}.
@param[in] user_attributes
    New user attributes of \bt_p{trace_class}.

@bt_pre_not_null{trace_class}
@bt_pre_hot{trace_class}
@bt_pre_not_null{user_attributes}
@bt_pre_is_map_val{user_attributes}

@sa bt_trace_class_borrow_user_attributes() &mdash;
    Borrows the user attributes of a trace class.
*/
extern void bt_trace_class_set_user_attributes(
		bt_trace_class *trace_class, const bt_value *user_attributes);

/*!
@brief
    Borrows the user attributes of the trace class \bt_p{trace_class}.

See the \ref api-tir-trace-cls-prop-user-attrs "user attributes"
property.

@note
    When you create a default trace class with bt_trace_class_create()
    or bt_trace_class_create_with_id(), the trace class's initial user
    attributes is an empty \bt_map_val.

@param[in] trace_class
    Trace class from which to borrow the user attributes.

@returns
    User attributes of \bt_p{trace_class} (a \bt_map_val).

@bt_pre_not_null{trace_class}

@sa bt_trace_class_set_user_attributes() &mdash;
    Sets the user attributes of a trace class.
@sa bt_trace_class_borrow_user_attributes_const() &mdash;
    \c const version of this function.
*/
extern bt_value *bt_trace_class_borrow_user_attributes(
		bt_trace_class *trace_class);

/*!
@brief
    Borrows the user attributes of the trace class \bt_p{trace_class}
    (\c const version).

See bt_trace_class_borrow_user_attributes().
*/
extern const bt_value *bt_trace_class_borrow_user_attributes_const(
		const bt_trace_class *trace_class);

/*! @} */

/*!
@name Listeners
@{
*/

/*!
@brief
    User function for bt_trace_class_add_destruction_listener().

This is the user function type for a trace class destruction listener.

@param[in] trace_class
    Trace class being destroyed (\ref api-fund-freezing "frozen").
@param[in] user_data
    User data, as passed as the \bt_p{user_data} parameter of
    bt_trace_class_add_destruction_listener().

@bt_pre_not_null{trace_class}

@post
    The reference count of \bt_p{trace_class} is not changed.
@bt_post_no_error

@sa bt_trace_class_add_destruction_listener() &mdash;
    Adds a destruction listener to a trace class.
*/
typedef void (* bt_trace_class_destruction_listener_func)(
		const bt_trace_class *trace_class, void *user_data);

/*!
@brief
    Status codes for bt_trace_class_add_destruction_listener().
*/
typedef enum bt_trace_class_add_listener_status {
	/*!
	@brief
	    Success.
	*/
	BT_TRACE_CLASS_ADD_LISTENER_STATUS_OK		= __BT_FUNC_STATUS_OK,

	/*!
	@brief
	    Out of memory.
	*/
	BT_TRACE_CLASS_ADD_LISTENER_STATUS_MEMORY_ERROR	= __BT_FUNC_STATUS_MEMORY_ERROR,
} bt_trace_class_add_listener_status;

/*!
@brief
    Adds a destruction listener having the function \bt_p{user_func}
    to the trace class \bt_p{trace_class}.

All the destruction listener user functions of a trace class are called
when it's being destroyed.

If \bt_p{listener_id} is not \c NULL, then this function, on success,
sets \bt_p{*listener_id} to the ID of the added destruction listener
within \bt_p{trace_class}. You can then use this ID to remove the
added destruction listener with
bt_trace_class_remove_destruction_listener().

@param[in] trace_class
    Trace class to add the destruction listener to.
@param[in] user_func
    User function of the destruction listener to add to
    \bt_p{trace_class}.
@param[in] user_data
    User data to pass as the \bt_p{user_data} parameter of
    \bt_p{user_func}.
@param[out] listener_id
    <strong>On success and if not \c NULL</strong>, \bt_p{*listener_id}
    is the ID of the added destruction listener within
    \bt_p{trace_class}.

@retval #BT_TRACE_CLASS_ADD_LISTENER_STATUS_OK
    Success.
@retval #BT_TRACE_CLASS_ADD_LISTENER_STATUS_MEMORY_ERROR
    Out of memory.

@bt_pre_not_null{trace_class}
@bt_pre_not_null{user_func}

@sa bt_trace_class_remove_destruction_listener() &mdash;
    Removes a destruction listener from a trace class.
*/
extern bt_trace_class_add_listener_status
bt_trace_class_add_destruction_listener(
        const bt_trace_class *trace_class,
        bt_trace_class_destruction_listener_func user_func,
        void *user_data, bt_listener_id *listener_id);

/*!
@brief
    Status codes for bt_trace_class_remove_destruction_listener().
*/
typedef enum bt_trace_class_remove_listener_status {
	/*!
	@brief
	    Success.
	*/
	BT_TRACE_CLASS_REMOVE_LISTENER_STATUS_OK		= __BT_FUNC_STATUS_OK,

	/*!
	@brief
	    Out of memory.
	*/
	BT_TRACE_CLASS_REMOVE_LISTENER_STATUS_MEMORY_ERROR	= __BT_FUNC_STATUS_MEMORY_ERROR,
} bt_trace_class_remove_listener_status;

/*!
@brief
    Removes the destruction listener having the ID \bt_p{listener_id}
    from the trace class \bt_p{trace_class}.

The destruction listener to remove from \bt_p{trace_class} was
previously added with bt_trace_class_add_destruction_listener().

You can call this function when \bt_p{trace_class} is
\ref api-fund-freezing "frozen".

@param[in] trace_class
    Trace class from which to remove the destruction listener having
    the ID \bt_p{listener_id}.
@param[in] listener_id
    ID of the destruction listener to remove from \bt_p{trace_class}­.

@retval #BT_TRACE_CLASS_REMOVE_LISTENER_STATUS_OK
    Success.
@retval #BT_TRACE_CLASS_REMOVE_LISTENER_STATUS_MEMORY_ERROR
    Out of memory.

@bt_pre_not_null{trace_class}
@pre
    \bt_p{listener_id} is the ID of an existing destruction listener
    in \bt_p{trace_class}.

@sa bt_trace_class_add_destruction_listener() &mdash;
    Adds a destruction listener to a trace class.
*/
extern bt_trace_class_remove_listener_status
bt_trace_class_remove_destruction_listener(
        const bt_trace_class *trace_class, bt_listener_id listener_id);

/*! @} */

/*!
@name Reference count
@{
*/

/*!
@brief
    Increments the \ref api-fund-shared-object "reference count" of
    the trace class \bt_p{trace_class}.

@param[in] trace_class
    @parblock
    Trace class of which to increment the reference count.

    Can be \c NULL.
    @endparblock

@sa bt_trace_class_put_ref() &mdash;
    Decrements the reference count of a trace class.
*/
extern void bt_trace_class_get_ref(const bt_trace_class *trace_class);

/*!
@brief
    Decrements the \ref api-fund-shared-object "reference count" of
    the trace class \bt_p{trace_class}.

@param[in] trace_class
    @parblock
    Trace class of which to decrement the reference count.

    Can be \c NULL.
    @endparblock

@sa bt_trace_class_get_ref() &mdash;
    Increments the reference count of a trace class.
*/
extern void bt_trace_class_put_ref(const bt_trace_class *trace_class);

/*!
@brief
    Decrements the reference count of the trace class
    \bt_p{_trace_class}, and then sets \bt_p{_trace_class} to \c NULL.

@param _trace_class
    @parblock
    Trace class of which to decrement the reference count.

    Can contain \c NULL.
    @endparblock

@bt_pre_assign_expr{_trace_class}
*/
#define BT_TRACE_CLASS_PUT_REF_AND_RESET(_trace_class)		\
	do {							\
		bt_trace_class_put_ref(_trace_class);		\
		(_trace_class) = NULL;				\
	} while (0)

/*!
@brief
    Decrements the reference count of the trace class \bt_p{_dst}, sets
    \bt_p{_dst} to \bt_p{_src}, and then sets \bt_p{_src} to \c NULL.

This macro effectively moves a trace class reference from the expression
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
#define BT_TRACE_CLASS_MOVE_REF(_dst, _src)	\
	do {					\
		bt_trace_class_put_ref(_dst);	\
		(_dst) = (_src);		\
		(_src) = NULL;			\
	} while (0)

/*! @} */

/*! @} */

#ifdef __cplusplus
}
#endif

#endif /* BABELTRACE2_TRACE_IR_TRACE_CLASS_H */
