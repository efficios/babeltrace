#ifndef BABELTRACE2_GRAPH_MESSAGE_ITERATOR_H
#define BABELTRACE2_GRAPH_MESSAGE_ITERATOR_H

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

#ifdef __cplusplus
extern "C" {
#endif

/*!
@defgroup api-msg-iter Message iterator
@ingroup api-comp-cls-dev

@brief
    Iterator of a \bt_msg sequence.

A <strong><em>message iterator</em></strong> iterates a sequence of
\bt_p_msg.

A message iterator is the \bt_name mechanism for the \bt_p_comp of a
trace processing \bt_graph to exchange information. This information
takes the form of a sequence of individual messages which contain
trace data (\bt_p_ev, for example).

A message iterator is a \bt_msg_iter_cls instance. Because a message
iterator class is part of a \bt_src_comp_cls or \bt_flt_comp_cls, a
message iterator is part of a \bt_src_comp or \bt_flt_comp. Borrow
a message iterator's component with
bt_message_iterator_borrow_component().

A message iterator is a \ref api-fund-shared-object "shared object": get
a new reference with bt_component_get_ref() and put an existing
reference with bt_component_put_ref().

The type of a message iterator is #bt_message_iterator.

There are two contexts from which you can create a message iterator:

<dl>
  <dt>From another message iterator</dt>
  <dd>
    This is the case for a \bt_flt_comp's message iterator.

    Use bt_message_iterator_create_from_message_iterator().

    You can call this function from any message iterator
    \ref api-msg-iter-cls-methods "method" except the
    \ref api-msg-iter-cls-meth-fini "finalization method".
  </dd>

  <dt>From a \bt_sink_comp</dt>
  <dd>
    Use bt_message_iterator_create_from_sink_component().

    You can call this function from a sink component
    \ref api-comp-cls-dev-methods "method" once the trace processing
    graph which contains the component is
    \ref api-graph-lc "configured", that is:

    - \ref api-comp-cls-dev-meth-graph-configured "Graph is configured"
      method (typical).

    - \ref api-comp-cls-dev-meth-consume "Consume" method.
  </dd>
</dl>

When you call one of the creation functions above, you pass an \bt_iport
on which to create the message iterator.

You can create more than one message iterator on a given
<em>\ref api-port-prop-is-connected "connected"</em> input port. The
\bt_p_conn between \bt_p_port in a trace processing \bt_graph establish
which \bt_p_comp and message iterators can create message iterators of
other \bt_p_comp. Then:

- Any \bt_sink_comp is free to create one or more message iterators
  on any of its connected input ports.

- Any message iterator is free to create one or more message iterators
  on any of its component's connected input ports.

The following illustration shows a very simple use case where the
\ref api-comp-cls-dev-meth-consume "consuming method" of a sink
component uses a single \bt_flt_comp's message iterator which itself
uses a single \bt_src_comp's message iterator:

@image html msg-iter.png

Many message iterator relations are possible, for example:

@image html msg-iter-complex.png

<h1>\anchor api-msg-iter-ops Operations</h1>

Once you have created a message iterator, there are three possible
operations:

<dl>
  <dt>
    \anchor api-msg-iter-op-next
    Get the message iterator's next messages
  </dt>
  <dd>
    This operation returns a batch of the message iterator's next
    \bt_p_msg considering its current state.

    This operation returns a batch of messages instead of a single
    message for performance reasons.

    This operation is said to \em advance the message iterator.

    Get the next messages of a message iterator with
    bt_message_iterator_next().
  </dd>

  <dt>
    \anchor api-msg-iter-op-seek-beg
    Make the message iterator seek its beginning
  </dt>
  <dd>
    This operation resets the message iterator's position to the
    beginning of its \ref api-msg-seq "message sequence".

    If the operation is successful, then the next call to
    bt_message_iterator_next() returns the first \bt_p_msg of the
    message iterator's sequence.

    If bt_message_iterator_seek_ns_from_origin() returns something
    else than #BT_MESSAGE_ITERATOR_SEEK_BEGINNING_STATUS_OK, you
    \em cannot call bt_message_iterator_next() afterwards. In that case,
    you can only call bt_message_iterator_seek_beginning() again or
    bt_message_iterator_seek_ns_from_origin().

    Before you call bt_message_iterator_seek_beginning() to make
    the message iterator seek its beginning, check if it can currently
    do it with bt_message_iterator_can_seek_beginning().
  </dd>

  <dt>
    \anchor api-msg-iter-op-seek-ns
    Make the message iterator seek a message occurring at or after a
    given time (in nanoseconds) from its clock class origin
  </dt>
  <dd>
    This operation changes the position of the message iterator within
    its \ref api-msg-seq "sequence" so that the next call to
    bt_message_iterator_next() returns \bt_p_msg which occur at or after
    a given time (in nanoseconds) from its
    \ref api-tir-clock-cls-origin "clock class origin".

    When you call bt_message_iterator_seek_ns_from_origin() to perform
    the operation, your pass the specific time to seek as the
    \bt_p{ns_from_origin} parameter. You don't pass any
    \bt_clock_cls: the function operates at the nanosecond from some
    origin level and it is left to the message iterator's implementation
    to seek a message having at least this time.

    If the requested time point is \em after the message iterator's
    sequence's last message, then the next call to
    bt_message_iterator_next() returns
    #BT_MESSAGE_ITERATOR_NEXT_STATUS_END.

    If bt_message_iterator_seek_ns_from_origin() returns something
    else than #BT_MESSAGE_ITERATOR_SEEK_NS_FROM_ORIGIN_STATUS_OK, you
    \em cannot call bt_message_iterator_next() afterwards. In that case,
    you can only call bt_message_iterator_seek_ns_from_origin() again
    or bt_message_iterator_seek_beginning().

    Before you call bt_message_iterator_seek_ns_from_origin() to make
    the message iterator seek a specific point in time, check if it can
    currently do it with bt_message_iterator_can_seek_ns_from_origin().
  </dd>
</dl>
*/

/*! @{ */

/*!
@name Type
@{

@typedef struct bt_message_iterator bt_message_iterator;

@brief
    Message iterator.

@}
*/

/*!
@name Creation
@{
*/

/*!
@brief
    Status code for bt_message_iterator_create_from_message_iterator().
*/
typedef enum bt_message_iterator_create_from_message_iterator_status {
	/*!
	@brief
	    Success.
	*/
	BT_MESSAGE_ITERATOR_CREATE_FROM_MESSAGE_ITERATOR_STATUS_OK		= __BT_FUNC_STATUS_OK,

	/*!
	@brief
	    Out of memory.
	*/
	BT_MESSAGE_ITERATOR_CREATE_FROM_MESSAGE_ITERATOR_STATUS_MEMORY_ERROR	= __BT_FUNC_STATUS_MEMORY_ERROR,

	/*!
	@brief
	    Other error.
	*/
	BT_MESSAGE_ITERATOR_CREATE_FROM_MESSAGE_ITERATOR_STATUS_ERROR		= __BT_FUNC_STATUS_ERROR,
} bt_message_iterator_create_from_message_iterator_status;

/*!
@brief
    Creates a message iterator on the \bt_iport \bt_p{port} from
    another message iterator \bt_p{self_message_iterator}, and sets
    \bt_p{*message_iterator} to the resulting message iterator.

On success, the message iterator's position is at the beginning
of its \ref api-msg-seq "message sequence".

@param[in] self_message_iterator
    Other message iterator from which to create the message iterator.
@param[in] port
    Input port on which to create the message iterator.
@param[out] message_iterator
    <strong>On success</strong>, \bt_p{*message_iterator} is a new
    message iterator reference.

@retval #BT_MESSAGE_ITERATOR_CREATE_FROM_MESSAGE_ITERATOR_STATUS_OK
    Success.
@retval #BT_MESSAGE_ITERATOR_CREATE_FROM_MESSAGE_ITERATOR_STATUS_MEMORY_ERROR
    Out of memory.
@retval #BT_MESSAGE_ITERATOR_CREATE_FROM_MESSAGE_ITERATOR_STATUS_ERROR
    Other error, for example, the created message iterator's
    \ref api-msg-iter-cls-meth-init "initialization method" failed.

@bt_pre_not_null{self_message_iterator}
@bt_pre_not_null{port}
@pre
    <code>bt_port_is_connected(port)</code> returns #BT_TRUE.
@bt_pre_not_null{message_iterator}

@sa bt_message_iterator_create_from_sink_component() &mdash;
    Creates a message iterator from a \bt_sink_comp.
*/
extern bt_message_iterator_create_from_message_iterator_status
bt_message_iterator_create_from_message_iterator(
		bt_self_message_iterator *self_message_iterator,
		bt_self_component_port_input *port,
		bt_message_iterator **message_iterator);

/*!
@brief
    Status code for bt_message_iterator_create_from_sink_component().
*/
typedef enum bt_message_iterator_create_from_sink_component_status {
	/*!
	@brief
	    Success.
	*/
	BT_MESSAGE_ITERATOR_CREATE_FROM_SINK_COMPONENT_STATUS_OK		= __BT_FUNC_STATUS_OK,

	/*!
	@brief
	    Out of memory.
	*/
	BT_MESSAGE_ITERATOR_CREATE_FROM_SINK_COMPONENT_STATUS_MEMORY_ERROR	= __BT_FUNC_STATUS_MEMORY_ERROR,

	/*!
	@brief
	    Other error.
	*/
	BT_MESSAGE_ITERATOR_CREATE_FROM_SINK_COMPONENT_STATUS_ERROR		= __BT_FUNC_STATUS_ERROR,
} bt_message_iterator_create_from_sink_component_status;

/*!
@brief
    Creates a message iterator on the \bt_iport \bt_p{port} from the
    \bt_sink_comp \bt_p{self_component_sink}, and sets
    \bt_p{*message_iterator} to the resulting message iterator.

On success, the message iterator's position is at the beginning
of its \ref api-msg-seq "message sequence".

@param[in] self_component_sink
    Sink component from which to create the message iterator.
@param[in] port
    Input port on which to create the message iterator.
@param[out] message_iterator
    <strong>On success</strong>, \bt_p{*message_iterator} is a new
    message iterator reference.

@retval #BT_MESSAGE_ITERATOR_CREATE_FROM_MESSAGE_ITERATOR_STATUS_OK
    Success.
@retval #BT_MESSAGE_ITERATOR_CREATE_FROM_MESSAGE_ITERATOR_STATUS_MEMORY_ERROR
    Out of memory.
@retval #BT_MESSAGE_ITERATOR_CREATE_FROM_MESSAGE_ITERATOR_STATUS_ERROR
    Other error, for example, the created message iterator's
    \ref api-msg-iter-cls-meth-init "initialization method" failed.

@bt_pre_not_null{self_component_sink}
@bt_pre_not_null{port}
@pre
    <code>bt_port_is_connected(port)</code> returns #BT_TRUE.
@bt_pre_not_null{message_iterator}

@sa bt_message_iterator_create_from_message_iterator() &mdash;
    Creates a message iterator from another message iterator.
*/
extern bt_message_iterator_create_from_sink_component_status
bt_message_iterator_create_from_sink_component(
		bt_self_component_sink *self_component_sink,
		bt_self_component_port_input *port,
		bt_message_iterator **message_iterator);

/*! @} */

/*!
@name Component access
@{
*/

/*!
@brief
    Borrows the \bt_comp which provides the \bt_msg_iter
    \bt_p{message_iterator}.

@param[in] message_iterator
    Message iterator from which to borrow the component which provides
    it.

@returns
    Component which provides \bt_p{message_iterator}.

@bt_pre_not_null{message_iterator}
*/
extern bt_component *
bt_message_iterator_borrow_component(
		bt_message_iterator *message_iterator);

/*! @} */

/*!
@name "Next" operation (get next messages)
@{
*/

/*!
@brief
    Status code for bt_message_iterator_next().
*/
typedef enum bt_message_iterator_next_status {
	/*!
	@brief
	    Success.
	*/
	BT_MESSAGE_ITERATOR_NEXT_STATUS_OK		= __BT_FUNC_STATUS_OK,

	/*!
	@brief
	    End of iteration.
	*/
	BT_MESSAGE_ITERATOR_NEXT_STATUS_END		= __BT_FUNC_STATUS_END,

	/*!
	@brief
	    Try again.
	*/
	BT_MESSAGE_ITERATOR_NEXT_STATUS_AGAIN		= __BT_FUNC_STATUS_AGAIN,

	/*!
	@brief
	    Out of memory.
	*/
	BT_MESSAGE_ITERATOR_NEXT_STATUS_MEMORY_ERROR	= __BT_FUNC_STATUS_MEMORY_ERROR,

	/*!
	@brief
	    Other error.
	*/
	BT_MESSAGE_ITERATOR_NEXT_STATUS_ERROR		= __BT_FUNC_STATUS_ERROR,
} bt_message_iterator_next_status;

/*!
@brief
    Returns the next \bt_p_msg of the message iterator
    \bt_p{message_iterator} into the \bt_p{*messages} array of size
    \bt_p{*count}, effectively advancing \bt_p{message_iterator}.

See \ref api-msg-iter-op-next "this operation's documentation".

On success, the message iterator's position is advanced by \bt_p{*count}
messages.

@param[in] message_iterator
    Message iterator from which to get the next messages.
@param[out] messages
    @parblock
    <strong>On success</strong>, \bt_p{*messages} is an array containing
    the next messages of \bt_p{message_iterator} as its first elements.

    \bt_p{*count} is the number of messages in \bt_p{*messages}.

    The library allocates and manages this array, but until you
    perform another \ref api-msg-iter-ops "operation" on
    \bt_p{message_iterator}, you are free to modify it. For example,
    you can set its elements to \c NULL if your use case needs it.

    You own the references of the messages this array contains. In
    other words, you must put them with bt_message_put_ref() or move
    them to another message array (from a
    \link api-msg-iter-cls-meth-next "next" method\endlink)
    before you perform another operation on \bt_p{message_iterator} or
    before \bt_p{message_iterator} is destroyed.
    @endparblock
@param[out] count
    <strong>On success</strong>, \bt_p{*count} is the number of messages
    in \bt_p{*messages}.

@retval #BT_MESSAGE_ITERATOR_NEXT_STATUS_OK
    Success.
@retval #BT_MESSAGE_ITERATOR_NEXT_STATUS_END
    End of iteration.
@retval #BT_MESSAGE_ITERATOR_NEXT_STATUS_AGAIN
    Try again.
@retval #BT_MESSAGE_ITERATOR_NEXT_STATUS_MEMORY_ERROR
    Out of memory.
@retval #BT_MESSAGE_ITERATOR_NEXT_STATUS_ERROR
    Other error.

@bt_pre_not_null{message_iterator}
@bt_pre_not_null{messages}
@bt_pre_not_null{count}

@post
    <strong>On success</strong>, \bt_p{*count}&nbsp;≥&nbsp;1.
*/
extern bt_message_iterator_next_status
bt_message_iterator_next(bt_message_iterator *message_iterator,
		bt_message_array_const *messages, uint64_t *count);

/*! @} */

/*!
@name Seeking
@{
*/

/*!
@brief
    Status code for bt_message_iterator_can_seek_beginning().
*/
typedef enum bt_message_iterator_can_seek_beginning_status {
	/*!
	@brief
	    Success.
	*/
	BT_MESSAGE_ITERATOR_CAN_SEEK_BEGINNING_STATUS_OK		= __BT_FUNC_STATUS_OK,

	/*!
	@brief
	    Try again.
	*/
	BT_MESSAGE_ITERATOR_CAN_SEEK_BEGINNING_STATUS_AGAIN		= __BT_FUNC_STATUS_AGAIN,

	/*!
	@brief
	    Out of memory.
	*/
	BT_MESSAGE_ITERATOR_CAN_SEEK_BEGINNING_STATUS_MEMORY_ERROR	= __BT_FUNC_STATUS_MEMORY_ERROR,

	/*!
	@brief
	    Other error.
	*/
	BT_MESSAGE_ITERATOR_CAN_SEEK_BEGINNING_STATUS_ERROR		= __BT_FUNC_STATUS_ERROR,
} bt_message_iterator_can_seek_beginning_status;

/*!
@brief
    Returns whether or not the message iterator \bt_p{message_iterator}
    can currently seek its beginning (first \bt_msg).

See the \link api-msg-iter-op-seek-beg "seek beginning"
operation\endlink.

Make sure to call this function, without performing any other
\ref api-msg-iter-ops "operation" on \bt_p{message_iterator}, before you
call bt_message_iterator_seek_beginning().

@param[in] message_iterator
    Message iterator from which to to get whether or not it can seek
    its beginning.
@param[out] can_seek_beginning
    <strong>On success</strong>, \bt_p{*can_seek_beginning} is #BT_TRUE
    if \bt_p{message_iterator} can seek its beginning.

@retval #BT_MESSAGE_ITERATOR_CAN_SEEK_BEGINNING_STATUS_OK
    Success.
@retval #BT_MESSAGE_ITERATOR_CAN_SEEK_BEGINNING_STATUS_AGAIN
    Try again.
@retval #BT_MESSAGE_ITERATOR_CAN_SEEK_BEGINNING_STATUS_MEMORY_ERROR
    Out of memory.
@retval #BT_MESSAGE_ITERATOR_CAN_SEEK_BEGINNING_STATUS_ERROR
    Other error.

@bt_pre_not_null{message_iterator}
@bt_pre_not_null{can_seek_beginning}

@sa bt_message_iterator_seek_beginning() &mdash;
    Makes a message iterator seek its beginning.
*/
extern bt_message_iterator_can_seek_beginning_status
bt_message_iterator_can_seek_beginning(
		bt_message_iterator *message_iterator,
		bt_bool *can_seek_beginning);

/*!
@brief
    Status code for bt_message_iterator_seek_beginning().
*/
typedef enum bt_message_iterator_seek_beginning_status {
	/*!
	@brief
	    Success.
	*/
	BT_MESSAGE_ITERATOR_SEEK_BEGINNING_STATUS_OK		= __BT_FUNC_STATUS_OK,

	/*!
	@brief
	    Try again.
	*/
	BT_MESSAGE_ITERATOR_SEEK_BEGINNING_STATUS_AGAIN		= __BT_FUNC_STATUS_AGAIN,

	/*!
	@brief
	    Out of memory.
	*/
	BT_MESSAGE_ITERATOR_SEEK_BEGINNING_STATUS_MEMORY_ERROR	= __BT_FUNC_STATUS_MEMORY_ERROR,

	/*!
	@brief
	    Other error.
	*/
	BT_MESSAGE_ITERATOR_SEEK_BEGINNING_STATUS_ERROR		= __BT_FUNC_STATUS_ERROR,
} bt_message_iterator_seek_beginning_status;

/*!
@brief
    Makes the message iterator \bt_p{message_iterator} seek its
    beginning (first \bt_msg).

See \ref api-msg-iter-op-seek-beg "this operation's documentation".

Make sure to call bt_message_iterator_can_seek_beginning(),
without performing any other \ref api-msg-iter-ops "operation" on
\bt_p{message_iterator}, before you call this function.

@param[in] message_iterator
    Message iterator to seek to its beginning.

@retval #BT_MESSAGE_ITERATOR_SEEK_BEGINNING_STATUS_OK
    Success.
@retval #BT_MESSAGE_ITERATOR_SEEK_BEGINNING_STATUS_AGAIN
    Try again.
@retval #BT_MESSAGE_ITERATOR_SEEK_BEGINNING_STATUS_MEMORY_ERROR
    Out of memory.
@retval #BT_MESSAGE_ITERATOR_SEEK_BEGINNING_STATUS_ERROR
    Other error.

@bt_pre_not_null{message_iterator}
@pre
    <code>bt_message_iterator_can_seek_beginning(message_iterator)</code>
    returns #BT_TRUE.

@sa bt_message_iterator_can_seek_beginning() &mdash;
    Returns whether or not a message iterator can currently seek its
    beginning.
*/
extern bt_message_iterator_seek_beginning_status
bt_message_iterator_seek_beginning(
		bt_message_iterator *message_iterator);

/*!
@brief
    Status code for bt_message_iterator_can_seek_ns_from_origin().
*/
typedef enum bt_message_iterator_can_seek_ns_from_origin_status {
	/*!
	@brief
	    Success.
	*/
	BT_MESSAGE_ITERATOR_CAN_SEEK_NS_FROM_ORIGIN_STATUS_OK		= __BT_FUNC_STATUS_OK,

	/*!
	@brief
	    Try again.
	*/
	BT_MESSAGE_ITERATOR_CAN_SEEK_NS_FROM_ORIGIN_STATUS_AGAIN	= __BT_FUNC_STATUS_AGAIN,

	/*!
	@brief
	    Out of memory.
	*/
	BT_MESSAGE_ITERATOR_CAN_SEEK_NS_FROM_ORIGIN_STATUS_MEMORY_ERROR	= __BT_FUNC_STATUS_MEMORY_ERROR,

	/*!
	@brief
	    Other error.
	*/
	BT_MESSAGE_ITERATOR_CAN_SEEK_NS_FROM_ORIGIN_STATUS_ERROR	= __BT_FUNC_STATUS_ERROR,
} bt_message_iterator_can_seek_ns_from_origin_status;

/*!
@brief
    Returns whether or not the message iterator \bt_p{message_iterator}
    can currently seek a \bt_msg occurring at or after
    \bt_p{ns_from_origin} nanoseconds from its
    \ref api-tir-clock-cls-origin "clock class origin".

See the \link api-msg-iter-op-seek-ns "seek ns from origin"
operation\endlink.

Make sure to call this function, without performing any other
\ref api-msg-iter-ops "operation" on \bt_p{message_iterator}, before you
call bt_message_iterator_seek_ns_from_origin().

@param[in] message_iterator
    Message iterator from which to to get whether or not it can seek
    its beginning.
@param[in] ns_from_origin
    Requested time point to seek.
@param[out] can_seek_ns_from_origin
    <strong>On success</strong>, \bt_p{*can_seek_ns_from_origin} is
    #BT_TRUE if \bt_p{message_iterator} can seek a message occurring at
    or after \bt_p{ns_from_origin} nanoseconds from its clock class
    origin.

@retval #BT_MESSAGE_ITERATOR_CAN_SEEK_NS_FROM_ORIGIN_STATUS_OK
    Success.
@retval #BT_MESSAGE_ITERATOR_CAN_SEEK_NS_FROM_ORIGIN_STATUS_AGAIN
    Try again.
@retval #BT_MESSAGE_ITERATOR_CAN_SEEK_NS_FROM_ORIGIN_STATUS_MEMORY_ERROR
    Out of memory.
@retval #BT_MESSAGE_ITERATOR_CAN_SEEK_NS_FROM_ORIGIN_STATUS_ERROR
    Other error.

@bt_pre_not_null{message_iterator}
@bt_pre_not_null{can_seek_ns_from_origin}

@sa bt_message_iterator_seek_ns_from_origin() &mdash;
    Makes a message iterator seek a message occurring at or after
    a given time from its clock class origin.
*/

extern bt_message_iterator_can_seek_ns_from_origin_status
bt_message_iterator_can_seek_ns_from_origin(
		bt_message_iterator *message_iterator,
		int64_t ns_from_origin, bt_bool *can_seek_ns_from_origin);

/*!
@brief
    Status code for bt_message_iterator_seek_ns_from_origin().
*/
typedef enum bt_message_iterator_seek_ns_from_origin_status {
	/*!
	@brief
	    Success.
	*/
	BT_MESSAGE_ITERATOR_SEEK_NS_FROM_ORIGIN_STATUS_OK		= __BT_FUNC_STATUS_OK,

	/*!
	@brief
	    Try again.
	*/
	BT_MESSAGE_ITERATOR_SEEK_NS_FROM_ORIGIN_STATUS_AGAIN		= __BT_FUNC_STATUS_AGAIN,

	/*!
	@brief
	    Out of memory.
	*/
	BT_MESSAGE_ITERATOR_SEEK_NS_FROM_ORIGIN_STATUS_MEMORY_ERROR	= __BT_FUNC_STATUS_MEMORY_ERROR,

	/*!
	@brief
	    Other error.
	*/
	BT_MESSAGE_ITERATOR_SEEK_NS_FROM_ORIGIN_STATUS_ERROR		= __BT_FUNC_STATUS_ERROR,
} bt_message_iterator_seek_ns_from_origin_status;

/*!
@brief
    Makes the message iterator \bt_p{message_iterator} seek a \bt_msg
    occurring at or after \bt_p{ns_from_origin} nanoseconds from its
    \ref api-tir-clock-cls-origin "clock class origin".

See \ref api-msg-iter-op-seek-ns "this operation's documentation".

Make sure to call bt_message_iterator_can_seek_ns_from_origin(),
without performing any other \ref api-msg-iter-ops "operation" on
\bt_p{message_iterator}, before you call this function.

@param[in] message_iterator
    Message iterator to seek to a message occurring at or after
    \bt_p{ns_from_origin} nanoseconds from its clock class origin.
@param[in] ns_from_origin
    Time point to seek.

@retval #BT_MESSAGE_ITERATOR_SEEK_NS_FROM_ORIGIN_STATUS_OK
    Success.
@retval #BT_MESSAGE_ITERATOR_SEEK_NS_FROM_ORIGIN_STATUS_AGAIN
    Try again.
@retval #BT_MESSAGE_ITERATOR_SEEK_NS_FROM_ORIGIN_STATUS_MEMORY_ERROR
    Out of memory.
@retval #BT_MESSAGE_ITERATOR_SEEK_NS_FROM_ORIGIN_STATUS_ERROR
    Other error.

@bt_pre_not_null{message_iterator}
@pre
    <code>bt_message_iterator_can_seek_ns_from_origin(message_iterator,&nbsp;ns_from_origin)</code>
    returns #BT_TRUE.

@sa bt_message_iterator_can_seek_ns_from_origin() &mdash;
    Returns whether or not a message iterator can currently seek a
    message occurring at or after a given time from its clock class
    origin.
*/
extern bt_message_iterator_seek_ns_from_origin_status
bt_message_iterator_seek_ns_from_origin(
		bt_message_iterator *message_iterator,
		int64_t ns_from_origin);

/*! @} */

/*!
@name Configuration
@{
*/

/*!
@brief
    Returns whether or not the message iterator \bt_p{message_iterator}
    can seek forward.

A message iterator can seek forward if all the \bt_p_msg of its
message sequence have some \bt_cs.

@param[in] message_iterator
    Message iterator of which to get whether or not it can seek forward.

@returns
    #BT_TRUE if \bt_p{message_iterator} can seek forward.

@sa bt_self_message_iterator_configuration_set_can_seek_forward() &mdash;
    Sets whether or not a message iterator can seek forward.
*/
extern bt_bool
bt_message_iterator_can_seek_forward(
		bt_message_iterator *message_iterator);

/*! @} */

/*!
@name Reference count
@{
*/

/*!
@brief
    Increments the \ref api-fund-shared-object "reference count" of
    the message iterator \bt_p{message_iterator}.

@param[in] message_iterator
    @parblock
    Message iterator of which to increment the reference count.

    Can be \c NULL.
    @endparblock

@sa bt_message_iterator_put_ref() &mdash;
    Decrements the reference count of a message iterator.
*/
extern void bt_message_iterator_get_ref(
		const bt_message_iterator *message_iterator);

/*!
@brief
    Decrements the \ref api-fund-shared-object "reference count" of
    the message iterator \bt_p{message_iterator}.

@param[in] message_iterator
    @parblock
    Message iterator of which to decrement the reference count.

    Can be \c NULL.
    @endparblock

@sa bt_message_iterator_get_ref() &mdash;
    Increments the reference count of a message iterator.
*/
extern void bt_message_iterator_put_ref(
		const bt_message_iterator *message_iterator);

/*!
@brief
    Decrements the reference count of the message iterator
    \bt_p{_message_iterator}, and then sets \bt_p{_message_iterator}
    to \c NULL.

@param _message_iterator
    @parblock
    Message iterator of which to decrement the reference count.

    Can contain \c NULL.
    @endparblock

@bt_pre_assign_expr{_message_iterator}
*/
#define BT_MESSAGE_ITERATOR_PUT_REF_AND_RESET(_message_iterator) 	\
	do {								\
		bt_message_iterator_put_ref(_message_iterator); 	\
		(_message_iterator) = NULL;				\
	} while (0)

/*!
@brief
    Decrements the reference count of the message iterator \bt_p{_dst},
    sets \bt_p{_dst} to \bt_p{_src}, and then sets \bt_p{_src} to
    \c NULL.

This macro effectively moves a message iterator reference from the
expression \bt_p{_src} to the expression \bt_p{_dst}, putting the
existing \bt_p{_dst} reference.

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
#define BT_MESSAGE_ITERATOR_MOVE_REF(_dst, _src)	\
	do {						\
		bt_message_iterator_put_ref(_dst);	\
		(_dst) = (_src);			\
		(_src) = NULL;				\
	} while (0)

/*! @} */

/*! @} */

#ifdef __cplusplus
}
#endif

#endif /* BABELTRACE2_GRAPH_MESSAGE_ITERATOR_H */
