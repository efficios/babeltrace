#ifndef BABELTRACE2_GRAPH_MESSAGE_ITERATOR_CLASS_H
#define BABELTRACE2_GRAPH_MESSAGE_ITERATOR_CLASS_H

/*
 * Copyright (c) 2019 EfficiOS Inc.
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
@defgroup api-msg-iter-cls Message iterator class
@ingroup api-comp-cls-dev

@brief
    \bt_c_msg_iter class.

A <strong><em>message iterator class</em></strong> is the class of a
\bt_msg_iter.

@image html msg-iter-cls.png

\bt_cp_src_comp_cls and \bt_p_flt_comp_cls contain a message iterator
class. For such a component class, its message iterator class is the
class of any message iterator created for any \bt_oport of the
component class's instances (\bt_p_comp).

Therefore, the only thing you can do with a message iterator class is to
pass it to bt_component_class_source_create() or
bt_component_class_filter_create() to set it as the created component
class's message iterator class.

A message iterator class has <em>methods</em>. This module essentially
offers:

- Message iterator class method type definitions.

- A message iterator class creation function, to which you must pass
  the mandatory \link api-msg-iter-cls-meth-next "next" method\endlink.

- Functions to set optional message iterator class methods.

A message iterator class method is a user function. All message iterator
class methods operate on an instance (a \bt_msg_iter). The type of the
method's first parameter is #bt_self_message_iterator. This is similar
to an instance method in Python (where the instance object name is
generally <code>self</code>) or a member function in C++ (where the
instance pointer is named <code>this</code>), for example.

See \ref api-msg-iter-cls-methods "Methods" to learn more about the
different types of message iterator class methods.

A message iterator class is a
\ref api-fund-shared-object "shared object": get a new reference with
bt_message_iterator_class_get_ref() and put an existing reference with
bt_message_iterator_class_put_ref().

Some library functions \ref api-fund-freezing "freeze" message iterator
classes on success. The documentation of those functions indicate this
postcondition.

The type of a message iterator class is #bt_message_iterator_class.

Create a message iterator class with bt_message_iterator_class_create().
When you call this function, you must pass the message iterator
class's mandatory
\link api-msg-iter-cls-meth-next "next" method\endlink.

<h1>\anchor api-msg-iter-cls-methods Methods</h1>

To learn when exactly the methods below are called, see
\ref api-graph-lc "Trace processing graph life cycle" and
\ref api-msg-iter.

The available message iterator class methods to implement are:

<table>
  <tr>
    <th>Name
    <th>Requirement
    <th>C type
  <tr>
    <td>Can seek beginning?
    <td>Optional
    <td>#bt_message_iterator_class_can_seek_beginning_method
  <tr>
    <td>Can seek ns from origin?
    <td>Optional
    <td>#bt_message_iterator_class_can_seek_ns_from_origin_method
  <tr>
    <td>Finalize
    <td>Optional
    <td>#bt_message_iterator_class_finalize_method
  <tr>
    <td>Initialize
    <td>Optional
    <td>#bt_message_iterator_class_initialize_method
  <tr>
    <td>Next
    <td>Mandatory
    <td>#bt_message_iterator_class_next_method
  <tr>
    <td>Seek beginning
    <td>Optional
    <td>#bt_message_iterator_class_seek_beginning_method
  <tr>
    <td>Seek ns from origin
    <td>Optional
    <td>#bt_message_iterator_class_seek_ns_from_origin_method
</table>

<dl>
  <dt>
    \anchor api-msg-iter-cls-meth-can-seek-beg
    Can seek beginning?
  </dt>
  <dd>
    Called to check whether or not your \bt_msg_iter can currently
    seek its beginning (the very first message of its
    \ref api-msg-seq "sequence").

    There are some use cases in which a message iterator cannot always
    seek its beginning, depending on its state.

    If you don't implement this method, then, if you implement the
    \ref api-msg-iter-cls-meth-seek-beg "seek beginning" method, the
    library assumes that your message iterator can always seek its
    beginning.

    The message iterator of a \bt_flt_comp will typically consider
    the beginning seeking capability of its own upstream message
    iterator(s) (with bt_message_iterator_can_seek_beginning()) in this
    method's implementation.

    If you need to block the thread to compute whether or not your
    message iterator can seek its beginning, you can instead report to
    try again later to the caller by returning
    #BT_MESSAGE_ITERATOR_CLASS_CAN_SEEK_BEGINNING_METHOD_STATUS_AGAIN.

    Set this optional method with the \bt_p{can_seek_method} parameter
    of bt_message_iterator_class_set_seek_beginning_methods().
  </dd>

  <dt>
    \anchor api-msg-iter-cls-meth-can-seek-ns
    Can seek ns from origin?
  </dt>
  <dd>
    Called to check whether or not your \bt_msg_iter can currently
    seek a message occurring at or after a specific time given in
    nanoseconds from its
    \ref api-tir-clock-cls-origin "clock class origin".

    There are some use cases in which a message iterator cannot always
    seek some specific time, depending on its state.

    Within this method, your receive the specific time to seek as the
    \bt_p{ns_from_origin} parameter. You don't receive any
    \bt_clock_cls: the method operates at the nanosecond from some
    origin level and it is left to the implementation to decide whether
    or not the message iterator can seek this point in time.

    If you don't implement this method, then, if you implement the
    \ref api-msg-iter-cls-meth-seek-ns "seek ns from origin" method, the
    library assumes that your message iterator can always seek any
    message occurring at or after any time.

    The message iterator of a \bt_flt_comp will typically consider
    the time seeking capability of its own upstream message
    iterator(s) (with bt_message_iterator_can_seek_ns_from_origin()) in
    this method's implementation.

    If you need to block the thread to compute whether or not your
    message iterator can seek a message occurring at or after a given
    time, you can instead report to try again later to the caller by
    returning
    #BT_MESSAGE_ITERATOR_CLASS_CAN_SEEK_NS_FROM_ORIGIN_METHOD_STATUS_AGAIN.

    Set this optional method with the \bt_p{can_seek_method} parameter
    of bt_message_iterator_class_set_seek_ns_from_origin_methods().
  </dd>

  <dt>
    \anchor api-msg-iter-cls-meth-fini
    Finalize
  </dt>
  <dd>
    Called to finalize your \bt_msg_iter, that is, to let you
    destroy/free/finalize any user data you have (retrieved with
    bt_self_message_iterator_get_data()).

    The \bt_name library does not specify exactly when this method is
    called, but guarantees that it's called before the message iterator
    is destroyed.

    The library guarantees that all message iterators are destroyed
    before their component is destroyed.

    This method is \em not called if the message iterator's
    \ref api-msg-iter-cls-meth-init "initialization method"
    previously returned an error status code.

    Set this optional method with
    bt_message_iterator_class_set_finalize_method().
  </dd>

  <dt>
    \anchor api-msg-iter-cls-meth-init
    Initialize
  </dt>
  <dd>
    Called within bt_message_iterator_create_from_message_iterator()
    or bt_message_iterator_create_from_sink_component() to initialize
    your \bt_msg_iter.

    Within this method, you can access your \bt_comp's user data
    by first borrowing it with
    bt_self_message_iterator_borrow_component() and then using
    bt_self_component_get_data().

    For the message iterator of a \bt_flt_comp, this method is
    typically where you create an upstream \bt_msg_iter
    with bt_message_iterator_create_from_message_iterator().

    You can create user data and set it as the \bt_self_msg_iter's user
    data with bt_self_message_iterator_set_data().

    If you return #BT_MESSAGE_ITERATOR_CLASS_INITIALIZE_METHOD_STATUS_OK
    from this method, then your message iterator's
    \ref api-msg-iter-cls-meth-fini "finalization method" will be
    called, if it exists, when your message iterator is finalized.

    This method receives a message iterator configuration object
    (#bt_self_message_iterator_configuration type). As of
    \bt_name_version_min_maj, you can use
    bt_self_message_iterator_configuration_set_can_seek_forward()
    during, and only during, this method's execution to set whether or
    not your message iterator can <em>seek forward</em>.

    For a message iterator to be able to seek forward, all the \bt_p_msg
    of its message sequence must have some \bt_cs.
    bt_message_iterator_seek_ns_from_origin() uses this configuration
    option and the beginning seeking capability of a message iterator
    (bt_message_iterator_can_seek_beginning())
    to make it seek a message occurring at or after a specific time even
    when the message iterator does not implement the
    \ref api-msg-iter-cls-meth-seek-ns "seek ns from origin" method.

    Set this optional method with
    bt_message_iterator_class_set_initialize_method().
  </dd>

  <dt>
    \anchor api-msg-iter-cls-meth-next
    "Next" (get next messages)
  </dt>
  <dd>
    Called within bt_message_iterator_next()
    to "advance" your \bt_msg_iter, that is, to get its next
    messages.

    Within this method, you receive:

    - An array of \bt_p_msg to fill (\bt_p{messages} parameter)
      with your message iterator's next messages, if any.

      Note that this array needs its own message
      \ref api-fund-shared-object "references". In other
      words, if you have a message reference and you put this message
      into the array without calling bt_message_get_ref(), then you just
      \em moved the message reference to the array (the array owns the
      message now).

    - The capacity of the message array (\bt_p{capacity} parameter),
      that is, the maximum number of messages you can put in it.

    - A message count output parameter (\bt_p{count}) which, on success,
      you must set to the number of messages you put in the message
      array.

    If you return #BT_MESSAGE_ITERATOR_CLASS_NEXT_METHOD_STATUS_OK
    from this method, then you must put at least one message in the
    message array. In other words, \bt_p{*count} must be greater than
    zero.

    You must honour the \ref api-msg-seq "message sequence rules" when
    you put new or existing messages in the message array.

    If you return #BT_MESSAGE_ITERATOR_CLASS_NEXT_METHOD_STATUS_OK,
    then all the messages of the message array become
    \ref api-fund-freezing "frozen".

    This method typically:

    <dl>
      <dt>For a \bt_src_comp's message iterator</dt>
      <dd>
        Creates brand new \bt_p_msg to represent one or more input
        traces.
      </dd>

      <dt>For a \bt_flt_comp's message iterator</dt>
      <dd>
        Gets \em one message batch from one (or more) upstream
        \bt_msg_iter and filters them.
      </dd>
    </dl>

    For a source component's message iterator, you are free to create
    as many as \bt_p{capacity} messages. For a filter component's
    message iterator, you are free to get more than one batch of
    messages from upstream message iterators if needed. However, in both
    cases, keep in mind that the \bt_name project recommends that this
    method executes fast enough so as not to block an interactive
    application running on the same thread.

    During what you consider to be a long, blocking operation, the
    project recommends that you periodically check whether or not you
    are interrupted with bt_self_message_iterator_is_interrupted(). When
    you are, you can return either
    #BT_MESSAGE_ITERATOR_CLASS_NEXT_METHOD_STATUS_AGAIN or
    #BT_MESSAGE_ITERATOR_CLASS_NEXT_METHOD_STATUS_ERROR, depending on
    your capability to continue the current operation later.

    If you need to block the thread to insert messages into the message
    array, you can instead report to try again later to the caller by
    returning #BT_MESSAGE_ITERATOR_CLASS_NEXT_METHOD_STATUS_AGAIN.
    When you return this status code, you must \em not put any message
    into the message array.

    If your message iterator's iteration process is done (you have no
    more messages to emit), then return
    #BT_MESSAGE_ITERATOR_CLASS_NEXT_METHOD_STATUS_END. When you return
    this status code, you must \em not put any message into the message
    array.

    Set this mandatory method at message iterator class creation time
    with bt_message_iterator_class_create().
  </dd>

  <dt>
    \anchor api-msg-iter-cls-meth-seek-beg
    Seek beginning
  </dt>
  <dd>
    Called within bt_message_iterator_seek_beginning() to make
    your message iterator seek its beginning, that is, the very first
    message of its \ref api-msg-seq "sequence".

    The sequence of messages of a given message iterator must always be
    the same, in that, if your message iterator emitted the messages A,
    B, C, D, and E, and then this "seek beginning" method is called
    successfully
    (it returns
    #BT_MESSAGE_ITERATOR_CLASS_SEEK_BEGINNING_METHOD_STATUS_OK),
    then your message iterator's next messages (the next time your
    \link api-msg-iter-cls-meth-next "next" method\endlink
    is called) must be A, B, C, D, and E.

    The \bt_name project recommends that this method executes fast
    enough so as not to block an interactive application running on the
    same thread.

    During what you consider to be a long, blocking operation, the
    project recommends that you periodically check whether or not you
    are interrupted with bt_self_message_iterator_is_interrupted(). When
    you are, you can return either
    #BT_MESSAGE_ITERATOR_CLASS_SEEK_BEGINNING_METHOD_STATUS_AGAIN or
    #BT_MESSAGE_ITERATOR_CLASS_SEEK_BEGINNING_METHOD_STATUS_ERROR,
    depending on your capability to continue the current operation
    later.

    If you need to block the thread to make your message iterator seek
    its beginning, you can instead report to try again later to the
    caller by returning
    #BT_MESSAGE_ITERATOR_CLASS_SEEK_BEGINNING_METHOD_STATUS_AGAIN.

    The optional
    \ref api-msg-iter-cls-meth-can-seek-beg "can seek beginning?"
    method can indicate whether or not your message iterator can
    currently seek its beginning. If you implement it, the library
    guarantees that it is called and returns #BT_TRUE before this "seek
    beginning" is called, without any other message iterator methods
    being called in between.

    Set this optional method with the \bt_p{seek_method} parameter
    of bt_message_iterator_class_set_seek_beginning_methods().
  </dd>

  <dt>
    \anchor api-msg-iter-cls-meth-seek-ns
    Seek ns from origin
  </dt>
  <dd>
    Called within bt_message_iterator_seek_ns_from_origin() to make
    your message iterator seek a message occurring at or after a specific
    time given in nanoseconds from its
    \ref api-tir-clock-cls-origin "clock class origin".

    Within this method, your receive the specific time to seek as the
    \bt_p{ns_from_origin} parameter. You don't receive any
    \bt_clock_cls: the method operates at the nanosecond from some
    origin level and it is left to the implementation to seek a message
    having at least this time while still honouring the
    \ref api-msg-seq "message sequence rules" the next time your
    \link api-msg-iter-cls-meth-next "next" method\endlink is called.

    If you return
    #BT_MESSAGE_ITERATOR_CLASS_SEEK_NS_FROM_ORIGIN_METHOD_STATUS_OK from
    this method, then the next time your
    \link api-msg-iter-cls-meth-next "next" method\endlink is called:

    - For each "active" \bt_stream at the seeked time point, you must
      emit a \bt_sb_msg for this stream before you emit any other
      message for this stream.

      The stream beginning message must have a
      \ref api-msg-sb-prop-cs "default clock snapshot" which corresponds
      to the seeked time point.

    - For each "active" \bt_pkt at the seeked time point, you must
      emit a \bt_pb_msg for this packet before you emit any other
      message for this packet.

      The packet beginning message must have a
      \ref api-msg-pb-prop-cs "default clock snapshot" which corresponds
      to the seeked time point.

    The \bt_name project recommends that this method executes fast
    enough so as not to block an interactive application running on the
    same thread.

    During what you consider to be a long, blocking operation, the
    project recommends that you periodically check whether or not you
    are interrupted with bt_self_message_iterator_is_interrupted(). When
    you are, you can return either
    #BT_MESSAGE_ITERATOR_CLASS_SEEK_NS_FROM_ORIGIN_METHOD_STATUS_AGAIN
    or
    #BT_MESSAGE_ITERATOR_CLASS_SEEK_NS_FROM_ORIGIN_METHOD_STATUS_ERROR,
    depending on your capability to continue the current operation
    later.

    If you need to block the thread to make your message iterator seek a
    message occurring at or after a given time, you can instead report to
    try again later to the caller by returning
    #BT_MESSAGE_ITERATOR_CLASS_SEEK_NS_FROM_ORIGIN_METHOD_STATUS_AGAIN.

    The optional
    \ref api-msg-iter-cls-meth-can-seek-ns "can seek ns from origin?"
    method can indicate whether or not your message iterator can
    currently seek a specific time. If you implement it, the library
    guarantees that it is called and returns #BT_TRUE before this "seek
    ns from origin" is called, without any other message iterator
    methods being called in between.

    Set this optional method with the \bt_p{seek_method} parameter
    of bt_message_iterator_class_set_seek_ns_from_origin_methods().
  </dd>
</dl>

Within any method, you can access the \bt_msg_iter's \bt_comp's
configured \ref #bt_logging_level "logging level" by first upcasting the
\bt_self_comp to the #bt_component type with
bt_self_component_as_component(), and then with
bt_component_get_logging_level().
*/

/*! @{ */

/*!
@name Type
@{

@typedef struct bt_message_iterator_class bt_message_iterator_class;

@brief
    Message iterator class.

@}
*/

/*!
@name Method types
@{
*/

/*!
@brief
    Status codes for #bt_message_iterator_class_can_seek_beginning_method.
*/
typedef enum bt_message_iterator_class_can_seek_beginning_method_status {
	/*!
	@brief
	    Success.
	*/
	BT_MESSAGE_ITERATOR_CLASS_CAN_SEEK_BEGINNING_METHOD_STATUS_OK		= __BT_FUNC_STATUS_OK,

	/*!
	@brief
	    Try again.
	*/
	BT_MESSAGE_ITERATOR_CLASS_CAN_SEEK_BEGINNING_METHOD_STATUS_AGAIN	= __BT_FUNC_STATUS_AGAIN,

	/*!
	@brief
	    Out of memory.
	*/
	BT_MESSAGE_ITERATOR_CLASS_CAN_SEEK_BEGINNING_METHOD_STATUS_MEMORY_ERROR	= __BT_FUNC_STATUS_MEMORY_ERROR,

	/*!
	@brief
	    User error.
	*/
	BT_MESSAGE_ITERATOR_CLASS_CAN_SEEK_BEGINNING_METHOD_STATUS_ERROR	= __BT_FUNC_STATUS_ERROR,
} bt_message_iterator_class_can_seek_beginning_method_status;

/*!
@brief
    \bt_c_msg_iter "can seek beginning?" method.

See the \ref api-msg-iter-cls-meth-can-seek-beg "can seek beginning?"
method.

@param[in] self_message_iterator
    Message iterator instance.
@param[out] can_seek_beginning
    <strong>On success</strong>, \bt_p{*can_seek_beginning} is
    #BT_TRUE if \bt_p{self_message_iterator} can currently seek its
    beginning.

@retval #BT_MESSAGE_ITERATOR_CLASS_CAN_SEEK_BEGINNING_METHOD_STATUS_OK
    Success.
@retval #BT_MESSAGE_ITERATOR_CLASS_CAN_SEEK_BEGINNING_METHOD_STATUS_AGAIN
    Try again.
@retval #BT_MESSAGE_ITERATOR_CLASS_CAN_SEEK_BEGINNING_METHOD_STATUS_MEMORY_ERROR
    Out of memory.
@retval #BT_MESSAGE_ITERATOR_CLASS_CAN_SEEK_BEGINNING_METHOD_STATUS_ERROR
    User error.

@bt_pre_not_null{self_message_iterator}
@bt_pre_not_null{can_seek_beginning}

@post
    <strong>On success</strong>, \bt_p{*can_seek_beginning} is set.

@sa bt_message_iterator_class_set_seek_beginning_methods() &mdash;
    Sets the "seek beginning" and "can seek beginning?" methods of a
    message iterator class.
*/
typedef bt_message_iterator_class_can_seek_beginning_method_status
(*bt_message_iterator_class_can_seek_beginning_method)(
		bt_self_message_iterator *self_message_iterator,
		bt_bool *can_seek_beginning);

/*!
@brief
    Status codes for #bt_message_iterator_class_can_seek_ns_from_origin_method.
*/
typedef enum bt_message_iterator_class_can_seek_ns_from_origin_method_status {
	/*!
	@brief
	    Success.
	*/
	BT_MESSAGE_ITERATOR_CLASS_CAN_SEEK_NS_FROM_ORIGIN_METHOD_STATUS_OK		= __BT_FUNC_STATUS_OK,

	/*!
	@brief
	    Try again.
	*/
	BT_MESSAGE_ITERATOR_CLASS_CAN_SEEK_NS_FROM_ORIGIN_METHOD_STATUS_AGAIN		= __BT_FUNC_STATUS_AGAIN,

	/*!
	@brief
	    Out of memory.
	*/
	BT_MESSAGE_ITERATOR_CLASS_CAN_SEEK_NS_FROM_ORIGIN_METHOD_STATUS_MEMORY_ERROR	= __BT_FUNC_STATUS_MEMORY_ERROR,

	/*!
	@brief
	    User error.
	*/
	BT_MESSAGE_ITERATOR_CLASS_CAN_SEEK_NS_FROM_ORIGIN_METHOD_STATUS_ERROR		= __BT_FUNC_STATUS_ERROR,
} bt_message_iterator_class_can_seek_ns_from_origin_method_status;

/*!
@brief
    \bt_c_msg_iter "can seek ns from origin?" method.

See the \ref api-msg-iter-cls-meth-can-seek-ns "can seek ns from origin?"
method.

@param[in] self_message_iterator
    Message iterator instance.
@param[in] ns_from_origin
    Requested time point to seek.
@param[out] can_seek_ns_from_origin
    <strong>On success</strong>, set \bt_p{*can_seek_ns_from_origin} to
    #BT_TRUE if \bt_p{self_message_iterator} can currently seek a
    message occurring at or after \bt_p{ns_from_origin} nanoseconds from
    its \ref api-tir-clock-cls-origin "clock class origin".

@retval #BT_MESSAGE_ITERATOR_CLASS_CAN_SEEK_NS_FROM_ORIGIN_METHOD_STATUS_OK
    Success.
@retval #BT_MESSAGE_ITERATOR_CLASS_CAN_SEEK_NS_FROM_ORIGIN_METHOD_STATUS_AGAIN
    Try again.
@retval #BT_MESSAGE_ITERATOR_CLASS_CAN_SEEK_NS_FROM_ORIGIN_METHOD_STATUS_MEMORY_ERROR
    Out of memory.
@retval #BT_MESSAGE_ITERATOR_CLASS_CAN_SEEK_NS_FROM_ORIGIN_METHOD_STATUS_ERROR
    User error.

@bt_pre_not_null{self_message_iterator}
@bt_pre_not_null{can_seek_ns_from_origin}

@post
    <strong>On success</strong>, \bt_p{*can_seek_ns_from_origin} is set.

@sa bt_message_iterator_class_set_seek_ns_from_origin_methods() &mdash;
    Sets the "seek ns from origin" and "can seek ns from origin?"
    methods of a message iterator class.
*/
typedef bt_message_iterator_class_can_seek_ns_from_origin_method_status
(*bt_message_iterator_class_can_seek_ns_from_origin_method)(
		bt_self_message_iterator *self_message_iterator,
		int64_t ns_from_origin, bt_bool *can_seek_ns_from_origin);

/*!
@brief
    \bt_c_msg_iter finalization method.

See the \ref api-msg-iter-cls-meth-fini "finalize" method.

@param[in] self_message_iterator
    Message iterator instance.

@bt_pre_not_null{self_message_iterator}

@bt_post_no_error

@sa bt_message_iterator_class_set_finalize_method() &mdash;
    Sets the finalization method of a message iterator class.
*/
typedef void
(*bt_message_iterator_class_finalize_method)(
		bt_self_message_iterator *self_message_iterator);

/*!
@brief
    Status codes for #bt_message_iterator_class_initialize_method.
*/
typedef enum bt_message_iterator_class_initialize_method_status {
	/*!
	@brief
	    Success.
	*/
	BT_MESSAGE_ITERATOR_CLASS_INITIALIZE_METHOD_STATUS_OK		= __BT_FUNC_STATUS_OK,

	/*!
	@brief
	    Out of memory.
	*/
	BT_MESSAGE_ITERATOR_CLASS_INITIALIZE_METHOD_STATUS_MEMORY_ERROR	= __BT_FUNC_STATUS_MEMORY_ERROR,

	/*!
	@brief
	    User error.
	*/
	BT_MESSAGE_ITERATOR_CLASS_INITIALIZE_METHOD_STATUS_ERROR	= __BT_FUNC_STATUS_ERROR,
} bt_message_iterator_class_initialize_method_status;

/*!
@brief
    \bt_c_msg_iter initialization method.

See the \ref api-msg-iter-cls-meth-init "initialize" method.

@param[in] self_message_iterator
    Message iterator instance.
@param[in] configuration
    Message iterator's configuration.
@param[in] port
    \bt_c_oport for which \bt_p{self_message_iterator} was created.

@retval #BT_MESSAGE_ITERATOR_CLASS_INITIALIZE_METHOD_STATUS_OK
    Success.
@retval #BT_MESSAGE_ITERATOR_CLASS_INITIALIZE_METHOD_STATUS_MEMORY_ERROR
    Out of memory.
@retval #BT_MESSAGE_ITERATOR_CLASS_INITIALIZE_METHOD_STATUS_ERROR
    User error.

@bt_pre_not_null{self_message_iterator}
@bt_pre_not_null{configuration}
@bt_pre_not_null{port}

@sa bt_message_iterator_class_set_initialize_method() &mdash;
    Sets the initialization method of a message iterator class.
*/
typedef bt_message_iterator_class_initialize_method_status
(*bt_message_iterator_class_initialize_method)(
		bt_self_message_iterator *self_message_iterator,
		bt_self_message_iterator_configuration *configuration,
		bt_self_component_port_output *port);

/*!
@brief
    Status codes for #bt_message_iterator_class_next_method.
*/
typedef enum bt_message_iterator_class_next_method_status {
	/*!
	@brief
	    Success.
	*/
	BT_MESSAGE_ITERATOR_CLASS_NEXT_METHOD_STATUS_OK			= __BT_FUNC_STATUS_OK,

	/*!
	@brief
	    End of iteration.
	*/
	BT_MESSAGE_ITERATOR_CLASS_NEXT_METHOD_STATUS_END		= __BT_FUNC_STATUS_END,

	/*!
	@brief
	    Try again.
	*/
	BT_MESSAGE_ITERATOR_CLASS_NEXT_METHOD_STATUS_AGAIN		= __BT_FUNC_STATUS_AGAIN,

	/*!
	@brief
	    Out of memory.
	*/
	BT_MESSAGE_ITERATOR_CLASS_NEXT_METHOD_STATUS_MEMORY_ERROR	= __BT_FUNC_STATUS_MEMORY_ERROR,

	/*!
	@brief
	    User error.
	*/
	BT_MESSAGE_ITERATOR_CLASS_NEXT_METHOD_STATUS_ERROR		= __BT_FUNC_STATUS_ERROR,
} bt_message_iterator_class_next_method_status;

/*!
@brief
    \bt_c_msg_iter "next" (get next messages) method.

See the \link api-msg-iter-cls-meth-next "next"\endlink method.

If this method returns #BT_MESSAGE_ITERATOR_CLASS_NEXT_METHOD_STATUS_OK,
then all the messages of the message array become
\ref api-fund-freezing "frozen".

@param[in] self_message_iterator
    Message iterator instance.
@param[out] messages
    @parblock
    Message array to fill, on success, with the \bt_p_msg to emit.

    This array needs its own message
    \ref api-fund-shared-object "references". In other
    words, if you have a message reference and you put this message
    into the array without calling bt_message_get_ref(), then you just
    \em moved the message reference to the array (the array owns the
    message now).

    The capacity of this array (maximum number of messages you can put
    in it) is \bt_p{capacity}.
    @endparblock
@param[in] capacity
    Capacity of the \bt_p{messages} array (maximum number of messages
    you can put in it).
@param[out] count
    <strong>On success</strong>, \bt_p{*count} is the number of messages
    you put in \bt_p{messages}.

@retval #BT_MESSAGE_ITERATOR_CLASS_NEXT_METHOD_STATUS_OK
    Success.
@retval #BT_MESSAGE_ITERATOR_CLASS_NEXT_METHOD_STATUS_END
    End of iteration.
@retval #BT_MESSAGE_ITERATOR_CLASS_NEXT_METHOD_STATUS_AGAIN
    Try again.
@retval #BT_MESSAGE_ITERATOR_CLASS_NEXT_METHOD_STATUS_MEMORY_ERROR
    Out of memory.
@retval #BT_MESSAGE_ITERATOR_CLASS_NEXT_METHOD_STATUS_ERROR
    User error.

@bt_pre_not_null{self_message_iterator}
@bt_pre_not_null{messages}
@pre
    \bt_p{capacity} ≥ 1.
@bt_pre_not_null{count}

@post
    <strong>On success</strong>, \bt_p{messages} contains \bt_p{*count}
    message references as its first \bt_p{*count} elements.
@post
    <strong>On success</strong>, the \bt_p_msg in \bt_p{messages} honour
    the \ref api-msg-seq "message sequence rules".
@post
    <strong>On success</strong>, for any \bt_ev_msg in
    \bt_p{messages}, its
    \ref api-tir-ev-prop-payload "payload field",
    \ref api-tir-ev-prop-spec-ctx "specific context field",
    \ref api-tir-ev-prop-common-ctx "common context field", and all
    their inner \bt_p_field, recursively, are set.
@post
    <strong>On success</strong>, \bt_p{*count}&nbsp;≥&nbsp;1.
@post
    <strong>On success</strong>,
    \bt_p{*count}&nbsp;≤&nbsp;\bt_p{capacity}.

@sa bt_message_iterator_class_create() &mdash;
    Creates a message iterator class.
*/
typedef bt_message_iterator_class_next_method_status
(*bt_message_iterator_class_next_method)(
		bt_self_message_iterator *self_message_iterator,
		bt_message_array_const messages, uint64_t capacity,
		uint64_t *count);

/*!
@brief
    Status codes for #bt_message_iterator_class_seek_beginning_method.
*/
typedef enum bt_message_iterator_class_seek_beginning_method_status {
	/*!
	@brief
	    Success.
	*/
	BT_MESSAGE_ITERATOR_CLASS_SEEK_BEGINNING_METHOD_STATUS_OK		= __BT_FUNC_STATUS_OK,

	/*!
	@brief
	    Try again.
	*/
	BT_MESSAGE_ITERATOR_CLASS_SEEK_BEGINNING_METHOD_STATUS_AGAIN		= __BT_FUNC_STATUS_AGAIN,

	/*!
	@brief
	    Out of memory.
	*/
	BT_MESSAGE_ITERATOR_CLASS_SEEK_BEGINNING_METHOD_STATUS_MEMORY_ERROR	= __BT_FUNC_STATUS_MEMORY_ERROR,

	/*!
	@brief
	    User error.
	*/
	BT_MESSAGE_ITERATOR_CLASS_SEEK_BEGINNING_METHOD_STATUS_ERROR		= __BT_FUNC_STATUS_ERROR,
} bt_message_iterator_class_seek_beginning_method_status;

/*!
@brief
    \bt_c_msg_iter "seek beginning" method.

See the \ref api-msg-iter-cls-meth-seek-beg "seek beginning" method.

@param[in] self_message_iterator
    Message iterator instance.

@retval #BT_MESSAGE_ITERATOR_CLASS_SEEK_BEGINNING_METHOD_STATUS_OK
    Success.
@retval #BT_MESSAGE_ITERATOR_CLASS_SEEK_BEGINNING_METHOD_STATUS_AGAIN
    Try again.
@retval #BT_MESSAGE_ITERATOR_CLASS_SEEK_BEGINNING_METHOD_STATUS_MEMORY_ERROR
    Out of memory.
@retval #BT_MESSAGE_ITERATOR_CLASS_SEEK_BEGINNING_METHOD_STATUS_ERROR
    User error.

@bt_pre_not_null{self_message_iterator}
@pre
    <strong>If \bt_p{self_message_iterator} has a
    \ref api-msg-iter-cls-meth-can-seek-beg "can seek beginning?"
    method</strong>, then it was called and returned #BT_TRUE before
    this "seek beginning" method is called, without any other method of
    \bt_p{self_message_iterator} called in between.

@sa bt_message_iterator_class_set_seek_beginning_methods() &mdash;
    Sets the "seek beginning" and "can seek beginning?" methods of a
    message iterator class.
*/
typedef bt_message_iterator_class_seek_beginning_method_status
(*bt_message_iterator_class_seek_beginning_method)(
		bt_self_message_iterator *self_message_iterator);

/*!
@brief
    Status codes for #bt_message_iterator_class_seek_ns_from_origin_method.
*/
typedef enum bt_message_iterator_class_seek_ns_from_origin_method_status {
	/*!
	@brief
	    Success.
	*/
	BT_MESSAGE_ITERATOR_CLASS_SEEK_NS_FROM_ORIGIN_METHOD_STATUS_OK			= __BT_FUNC_STATUS_OK,

	/*!
	@brief
	    Try again.
	*/
	BT_MESSAGE_ITERATOR_CLASS_SEEK_NS_FROM_ORIGIN_METHOD_STATUS_AGAIN		= __BT_FUNC_STATUS_AGAIN,

	/*!
	@brief
	    Out of memory.
	*/
	BT_MESSAGE_ITERATOR_CLASS_SEEK_NS_FROM_ORIGIN_METHOD_STATUS_MEMORY_ERROR	= __BT_FUNC_STATUS_MEMORY_ERROR,

	/*!
	@brief
	    User error.
	*/
	BT_MESSAGE_ITERATOR_CLASS_SEEK_NS_FROM_ORIGIN_METHOD_STATUS_ERROR		= __BT_FUNC_STATUS_ERROR,
} bt_message_iterator_class_seek_ns_from_origin_method_status;

/*!
@brief
    \bt_c_msg_iter "seek ns from origin" method.

See the \ref api-msg-iter-cls-meth-seek-ns "seek ns from origin" method.

@param[in] self_message_iterator
    Message iterator instance.
@param[in] ns_from_origin
    Time point to seek.

@retval #BT_MESSAGE_ITERATOR_CLASS_SEEK_NS_FROM_ORIGIN_METHOD_STATUS_OK
    Success.
@retval #BT_MESSAGE_ITERATOR_CLASS_SEEK_NS_FROM_ORIGIN_METHOD_STATUS_AGAIN
    Try again.
@retval #BT_MESSAGE_ITERATOR_CLASS_SEEK_NS_FROM_ORIGIN_METHOD_STATUS_MEMORY_ERROR
    Out of memory.
@retval #BT_MESSAGE_ITERATOR_CLASS_SEEK_NS_FROM_ORIGIN_METHOD_STATUS_ERROR
    User error.

@bt_pre_not_null{self_message_iterator}
@pre
    <strong>If \bt_p{self_message_iterator} has a
    \ref api-msg-iter-cls-meth-can-seek-ns "can seek ns from origin?"
    method</strong>, then it was called and returned #BT_TRUE before
    this "seek ns from origin" method is called, without any other
    method of \bt_p{self_message_iterator} called in between.

@sa bt_message_iterator_class_set_seek_ns_from_origin_methods() &mdash;
    Sets the "seek ns from origin" and "can seek ns from origin?"
    methods of a message iterator class.
*/
typedef bt_message_iterator_class_seek_ns_from_origin_method_status
(*bt_message_iterator_class_seek_ns_from_origin_method)(
		bt_self_message_iterator *self_message_iterator,
		int64_t ns_from_origin);

/*! @} */

/*!
@name Creation
@{
*/

/*!
@brief
    Creates a message iterator class having the
    \link api-msg-iter-cls-meth-next "next" method\endlink method
    \bt_p{next_method}.

@param[in] next_method
    "Next" method of the message iterator class to create.

@returns
    New message iterator class reference, or \c NULL on memory error.

@bt_pre_not_null{next_method}
*/
extern bt_message_iterator_class *
bt_message_iterator_class_create(
		bt_message_iterator_class_next_method next_method);

/*! @} */

/*!
@name Method setting
@{
*/

/*!
@brief
    Status code for the
    <code>bt_message_iterator_class_set_*_method()</code> functions.
*/
typedef enum bt_message_iterator_class_set_method_status {
	/*!
	@brief
	    Success.
	*/
	BT_MESSAGE_ITERATOR_CLASS_SET_METHOD_STATUS_OK	= __BT_FUNC_STATUS_OK,
} bt_message_iterator_class_set_method_status;

/*!
@brief
    Sets the optional finalization method of the message iterator class
    \bt_p{message_iterator_class} to \bt_p{method}.

See the \ref api-msg-iter-cls-meth-fini "finalize" method.

@param[in] message_iterator_class
    Message iterator class of which to set the finalization method to
    \bt_p{method}.
@param[in] method
    New finalization method of \bt_p{message_iterator_class}.

@retval #BT_MESSAGE_ITERATOR_CLASS_SET_METHOD_STATUS_OK
    Success.

@bt_pre_not_null{message_iterator_class}
@bt_pre_hot{message_iterator_class}
@bt_pre_not_null{method}
*/
extern bt_message_iterator_class_set_method_status
bt_message_iterator_class_set_finalize_method(
		bt_message_iterator_class *message_iterator_class,
		bt_message_iterator_class_finalize_method method);

/*!
@brief
    Sets the optional initialization method of the message iterator
    class \bt_p{message_iterator_class} to \bt_p{method}.

See the \ref api-msg-iter-cls-meth-init "initialize" method.

@param[in] message_iterator_class
    Message iterator class of which to set the initialization method to
    \bt_p{method}.
@param[in] method
    New initialization method of \bt_p{message_iterator_class}.

@retval #BT_MESSAGE_ITERATOR_CLASS_SET_METHOD_STATUS_OK
    Success.

@bt_pre_not_null{message_iterator_class}
@bt_pre_hot{message_iterator_class}
@bt_pre_not_null{method}
*/
extern bt_message_iterator_class_set_method_status
bt_message_iterator_class_set_initialize_method(
		bt_message_iterator_class *message_iterator_class,
		bt_message_iterator_class_initialize_method method);

/*!
@brief
    Sets the optional "seek beginning" and
    "can seek beginning?" methods of the message iterator class
    \bt_p{message_iterator_class} to \bt_p{seek_method} and
    \bt_p{can_seek_method}.

See the \ref api-msg-iter-cls-meth-seek-beg "seek beginning"
and \ref api-msg-iter-cls-meth-can-seek-beg "can seek beginning?"
methods.

@param[in] message_iterator_class
    Message iterator class of which to set the "seek beginning"
    and "can seek beginning?" methods.
@param[in] seek_method
    New "seek beginning" method of \bt_p{message_iterator_class}.
@param[in] can_seek_method
    @parblock
    New "can seek beginning?" method of \bt_p{message_iterator_class}.

    Can be \c NULL, in which case it is equivalent to setting a method
    which always returns #BT_TRUE.
    @endparblock

@retval #BT_MESSAGE_ITERATOR_CLASS_SET_METHOD_STATUS_OK
    Success.

@bt_pre_not_null{message_iterator_class}
@bt_pre_hot{message_iterator_class}
@bt_pre_not_null{seek_method}
*/
extern bt_message_iterator_class_set_method_status
bt_message_iterator_class_set_seek_beginning_methods(
		bt_message_iterator_class *message_iterator_class,
		bt_message_iterator_class_seek_beginning_method seek_method,
		bt_message_iterator_class_can_seek_beginning_method can_seek_method);

/*!
@brief
    Sets the optional "seek ns from origin" and
    "can seek ns from origin?" methods of the message iterator class
    \bt_p{message_iterator_class} to \bt_p{seek_method} and
    \bt_p{can_seek_method}.

See the \ref api-msg-iter-cls-meth-seek-ns "seek ns from origin"
and
\ref api-msg-iter-cls-meth-can-seek-ns "can seek ns from origin?"
methods.

@param[in] message_iterator_class
    Message iterator class of which to set the "seek ns from origin"
    and "can seek ns from origin?" methods.
@param[in] seek_method
    New "seek ns from origin" method of \bt_p{message_iterator_class}.
@param[in] can_seek_method
    @parblock
    New "can seek ns from origin?" method of
    \bt_p{message_iterator_class}.

    Can be \c NULL, in which case it is equivalent to setting a method
    which always returns #BT_TRUE.
    @endparblock

@retval #BT_MESSAGE_ITERATOR_CLASS_SET_METHOD_STATUS_OK
    Success.

@bt_pre_not_null{message_iterator_class}
@bt_pre_hot{message_iterator_class}
@bt_pre_not_null{seek_method}
*/
extern bt_message_iterator_class_set_method_status
bt_message_iterator_class_set_seek_ns_from_origin_methods(
		bt_message_iterator_class *message_iterator_class,
		bt_message_iterator_class_seek_ns_from_origin_method seek_method,
		bt_message_iterator_class_can_seek_ns_from_origin_method can_seek_method);

/*! @} */

/*!
@name Reference count
@{
*/

/*!
@brief
    Increments the \ref api-fund-shared-object "reference count" of
    the message iterator class \bt_p{message_iterator_class}.

@param[in] message_iterator_class
    @parblock
    Message iterator class of which to increment the reference count.

    Can be \c NULL.
    @endparblock

@sa bt_component_put_ref() &mdash;
    Decrements the reference count of a message iterator class.
*/
extern void bt_message_iterator_class_get_ref(
		const bt_message_iterator_class *message_iterator_class);

/*!
@brief
    Decrements the \ref api-fund-shared-object "reference count" of
    the message iterator class \bt_p{message_iterator_class}.

@param[in] message_iterator_class
    @parblock
    Message iterator class of which to decrement the reference count.

    Can be \c NULL.
    @endparblock

@sa bt_component_get_ref() &mdash;
    Increments the reference count of a message iterator class.
*/
extern void bt_message_iterator_class_put_ref(
		const bt_message_iterator_class *message_iterator_class);

/*!
@brief
    Decrements the reference count of the message iterator class
    \bt_p{_message_iterator_class}, and then sets
    \bt_p{_message_iterator_class} to \c NULL.

@param _message_iterator_class
    @parblock
    Message iterator class of which to decrement the reference count.

    Can contain \c NULL.
    @endparblock

@bt_pre_assign_expr{_message_iterator_class}
*/
#define BT_MESSAGE_ITERATOR_CLASS_PUT_REF_AND_RESET(_message_iterator_class) \
	do {								\
		bt_message_iterator_class_put_ref(_message_iterator_class); \
		(_message_iterator_class) = NULL;			\
	} while (0)

/*!
@brief
    Decrements the reference count of the message iterator class \bt_p{_dst},
    sets \bt_p{_dst} to \bt_p{_src}, and then sets \bt_p{_src}
    to \c NULL.

This macro effectively moves a message iterator class reference from the
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
#define BT_MESSAGE_ITERATOR_CLASS_MOVE_MOVE_REF(_dst, _src)	\
	do {							\
		bt_message_iterator_class_put_ref(_dst);	\
		(_dst) = (_src);				\
		(_src) = NULL;					\
	} while (0)

/*! @} */

/*! @} */

#ifdef __cplusplus
}
#endif

#endif /* BABELTRACE2_GRAPH_MESSAGE_ITERATOR_CLASS_H */
