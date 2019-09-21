#ifndef BABELTRACE2_ERROR_REPORTING_H
#define BABELTRACE2_ERROR_REPORTING_H

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

#include <stdarg.h>

#include <babeltrace2/types.h>
#include <babeltrace2/graph/component-class.h>

#ifdef __cplusplus
extern "C" {
#endif

/*!
@defgroup api-error Error reporting

@brief
    Error reporting functions and macros.

This module contains functions and macros to report rich errors from a
user function (a \bt_comp_cls method, a
\ref api-qexec "query operation", or a trace processing \bt_graph
listener, for example) to any function caller.

Because \bt_name orchestrates pieces written by different authors,
it is important that an error which occurs deep into the function call
stack can percolate up to its callers.

The very basic mechanism to report an error from a function is to
\ref api-fund-func-status "return an error status"
(a status code enumerator which contains the word
\c ERROR): each function caller can clean its own context and return an
error status code itself until one caller "catches" the status code and
reacts to it. For example, the reaction can be to show an error message
to the end user.

This error reporting API adds a layer so that each function which
returns an error status code can append a message which describes the
cause of the error within the function's context.

Functions append error causes to the current thread's error. Having one
error object per thread makes this API thread-safe.

Here's a visual, step-by-step example:

@image html error-reporting-steps-1234.png

-# The trace processing \bt_graph user calls bt_graph_run().

-# bt_graph_run() calls the
   \ref api-comp-cls-dev-meth-consume "consuming method" of the
   \bt_sink_comp.

-# The sink \bt_comp calls bt_message_iterator_next() on its upstream
   source \bt_msg_iter.

-# bt_message_iterator_next() calls the source message iterator's
   \link api-msg-iter-cls-meth-next "next" method\endlink.

@image html error-reporting-step-5.png

<ol start="5">
  <li>
    An error occurs within the "next" method of the source message
    iterator: the function cannot read a file because permission was
    denied.
</ol>

@image html error-reporting-step-6.png

<ol start="6">
  <li>
    The source message iterator's "next" method appends the error
    cause <em>"Cannot read file /some/file: permission denied"</em>
    and returns
    #BT_MESSAGE_ITERATOR_CLASS_NEXT_METHOD_STATUS_ERROR.
</ol>

@image html error-reporting-step-7.png

<ol start="7">
  <li>
    bt_message_iterator_next() appends
    the error cause <em>"Message iterator's 'next' method failed"</em>
    with details about the source component and
    returns #BT_MESSAGE_ITERATOR_NEXT_STATUS_ERROR.
</ol>

@image html error-reporting-steps-89.png

<ol start="8">
  <li>
    The sink component's "consume" method appends the error
    cause <em>"Cannot consume upstream message iterator's messages"</em>
    and returns #BT_COMPONENT_CLASS_SINK_CONSUME_METHOD_STATUS_ERROR.

  <li>
    bt_graph_run() appends the error cause <em>"Component's 'consume'
    method failed"</em> with details about the sink component and
    returns #BT_GRAPH_RUN_STATUS_ERROR.
</ol>

At this point, the current thread's error contains four causes:

- <em>"Cannot read file /some/file: permission denied"</em>
- <em>"Message iterator's 'next' method failed"</em>
- <em>"Cannot consume upstream message iterator's messages"</em>
- <em>"Component's 'consume' method failed"</em>

This list of error causes is much richer for the end user than dealing
only with #BT_GRAPH_RUN_STATUS_ERROR (the last error status code).

Both error (#bt_error) and error cause (#bt_error_cause) objects are
\ref api-fund-unique-object "unique objects":

- An error belongs to either
  the library or you (see \ref api-error-handle "Handle an error").

- An error cause belongs to the error which contains it.

<h1>\anchor api-error-append-cause Append an error cause</h1>

When your function returns an error status code, use one of the
<code>bt_current_thread_error_append_cause_from_*()</code>
functions or <code>BT_CURRENT_THREAD_ERROR_APPEND_CAUSE_FROM_*()</code>
macros to append an error cause to the
current thread's error. Use the appropriate function or macro
depending on your function's actor amongst:

<dl>
  <dt>Component</dt>
  <dd>
    Append an error cause from a \bt_comp method.

    Use bt_current_thread_error_append_cause_from_component() or
    BT_CURRENT_THREAD_ERROR_APPEND_CAUSE_FROM_COMPONENT().
  </dd>

  <dt>Message iterator</dt>
  <dd>
    Append an error cause from a \bt_msg_iter method.

    Use bt_current_thread_error_append_cause_from_message_iterator() or
    BT_CURRENT_THREAD_ERROR_APPEND_CAUSE_FROM_MESSAGE_ITERATOR().
  </dd>

  <dt>Component class</dt>
  <dd>
    Append an error cause from a \bt_comp_cls method
    ("query" method).

    Use bt_current_thread_error_append_cause_from_component_class() or
    BT_CURRENT_THREAD_ERROR_APPEND_CAUSE_FROM_COMPONENT_CLASS().
  </dd>

  <dt>Unknown</dt>
  <dd>
    Append an error cause from any other function, for example
    a \bt_graph listener or a function of your user application).

    Use bt_current_thread_error_append_cause_from_unknown() or
    BT_CURRENT_THREAD_ERROR_APPEND_CAUSE_FROM_UNKNOWN().
  </dd>
</dl>

The <code>BT_CURRENT_THREAD_ERROR_APPEND_CAUSE_FROM_*()</code> macros
uses \c __FILE__ and \c __LINE__ as the file name and line number
parameters of their corresponding
<code>bt_current_thread_error_append_cause_from_*()</code> function.

<h1>\anchor api-error-handle Handle an error</h1>

If any libbabeltrace2 function you call returns an error status code, do
one of:

- Return an error status code too.

  In that case, you \em can
  \ref api-error-append-cause "append an error cause" to the current
  thread's error.

- \em Take the current thread's error with
  bt_current_thread_take_error().

  This function moves the ownership of the error object from the library
  to you.

  At this point, you can inspect its causes with
  bt_error_get_cause_count() and bt_error_borrow_cause_by_index(), and
  then do one of:

  - Call bt_error_release() to free the error object.

    In <a href="https://en.wikipedia.org/wiki/Object-oriented_programming">object-oriented programming</a>
    terms, this corresponds to catching an exception and discarding it.

  - Call bt_current_thread_move_error() to move back the error object's
    ownership to the library.

    In object-oriented programming terms, this corresponds to catching
    an exception and rethrowing it.

bt_current_thread_clear_error() is a helper which is the equivalent of:

@code
bt_error_release(bt_current_thread_take_error());
@endcode

<h1>Error cause</h1>

All error causes have the type #bt_error_cause.

There are four types of error cause actors:

- \bt_c_comp
- \bt_c_msg_iter
- \bt_c_comp_cls
- Unknown

Get the type enumerator of an error cause's actor with
bt_error_cause_get_actor_type().

An error cause has the following common properties:

<dl>
  <dt>Message</dt>
  <dd>
    Description of the error cause.

    Use bt_error_cause_get_message().
  </dd>

  <dt>Module name</dt>
  <dd>
    Name of the module causing the error.

    For example, libbabeltrace2 uses "libbabeltrace2" and the \bt_cli
    CLI tool uses "Babeltrace CLI".

    Use bt_error_cause_get_module_name().
  </dd>

  <dt>File name</dt>
  <dd>
    Name of the source file causing the error.

    Use bt_error_cause_get_file_name().
  </dd>

  <dt>Line number</dt>
  <dd>
    Line number of the statement causing the error.

    Use bt_error_cause_get_line_number().
  </dd>
</dl>

<h2>Error cause with a component actor</h2>

An error cause with a \bt_comp actor has the following specific
properties:

<dl>
  <dt>Component name</dt>
  <dd>
    Name of the component.

    Use bt_error_cause_component_actor_get_component_name().
  </dd>

  <dt>Component class name</dt>
  <dd>
    Name of the component's \ref api-comp-cls "class".

    Use bt_error_cause_component_actor_get_component_class_type().
  </dd>

  <dt>Component class type</dt>
  <dd>
    Type of the component's class.

    Use bt_error_cause_component_actor_get_component_class_name().
  </dd>

  <dt>\bt_dt_opt Plugin name</dt>
  <dd>
    Name of the \bt_plugin which provides the component's class, if any.

    Use bt_error_cause_component_actor_get_plugin_name().
  </dd>
</dl>

<h2>Error cause with a message iterator actor</h2>

An error cause with a \bt_msg_iter actor has the following specific
properties:

<dl>
  <dt>Component output port name</dt>
  <dd>
    Name of the \bt_comp \bt_oport from which the message iterator
    was created.

    Use bt_error_cause_component_actor_get_component_name().
  </dd>

  <dt>Component name</dt>
  <dd>
    Name of the component.

    Use bt_error_cause_component_actor_get_component_name().
  </dd>

  bt_error_cause_message_iterator_actor_get_component_output_port_name

  <dt>Component class name</dt>
  <dd>
    Name of the component's \ref api-comp-cls "class".

    Use bt_error_cause_component_actor_get_component_class_type().
  </dd>

  <dt>Component class type</dt>
  <dd>
    Type of the component's class.

    Use bt_error_cause_component_actor_get_component_class_name().
  </dd>

  <dt>\bt_dt_opt Plugin name</dt>
  <dd>
    Name of the \bt_plugin which provides the component's class, if any.

    Use bt_error_cause_component_actor_get_plugin_name().
  </dd>
</dl>

<h2>Error cause with a component class actor</h2>

An error cause with a \bt_comp_cls actor has the following specific
properties:

<dl>
  <dt>Component class name</dt>
  <dd>
    Name of the component class.

    Use bt_error_cause_component_class_actor_get_component_class_type().
  </dd>

  <dt>Component class type</dt>
  <dd>
    Type of the component class.

    Use bt_error_cause_component_class_actor_get_component_class_name().
  </dd>

  <dt>\bt_dt_opt Plugin name</dt>
  <dd>
    Name of the \bt_plugin which provides the component class, if any.

    Use bt_error_cause_component_class_actor_get_plugin_name().
  </dd>
</dl>
*/

/*! @{ */

/*!
@name Types
@{

@typedef struct bt_error bt_error;

@brief
    Error.


@typedef struct bt_error_cause bt_error_cause;

@brief
    Error cause.

@}
*/

/*!
@name Current thread's error
@{
*/

/*!
@brief
    \em Takes the current thread's error, that is, moves its ownership
    from the library to the caller.

This function can return \c NULL if the current thread has no error.

Once you are done with the returned error, do one of:

- Call bt_error_release() to free the error object.

  In <a href="https://en.wikipedia.org/wiki/Object-oriented_programming">object-oriented programming</a>
  terms, this corresponds to catching an exception and discarding it.

- Call bt_current_thread_move_error() to move back the error object's
  ownership to the library.

  In object-oriented programming terms, this corresponds to catching
  an exception and rethrowing it.

@returns
    Unique reference of the current thread's error, or \c NULL if the
    current thread has no error.

@post
    <strong>If this function does not return <code>NULL</code></strong>,
    the caller owns the returned error.

@sa bt_error_release() &mdash;
    Releases (frees) an error.
@sa bt_current_thread_move_error() &mdash;
    Moves an error's ownership to the library.
*/
extern
const bt_error *bt_current_thread_take_error(void);

/*!
@brief
    Moves the ownership of the error \bt_p{error} from the caller to
    the library.

After you call this function, you don't own \bt_p{error} anymore.

In <a href="https://en.wikipedia.org/wiki/Object-oriented_programming">object-oriented programming</a>
terms, calling this function corresponds to catching an
exception and rethrowing it.

You can instead release (free) the error with bt_error_release(), which,
in object-oriented programming terms,
corresponds to catching an exception and discarding it.

@param[in] error
    Error of which to move the ownership from you to the library.

@bt_pre_not_null{error}
@pre
    The caller owns \bt_p{error}.

@post
    The library owns \bt_p{error}.

@sa bt_error_release() &mdash;
    Releases (frees) an error.
@sa BT_CURRENT_THREAD_MOVE_ERROR_AND_RESET() &mdash;
    Calls this function and assigns \c NULL to the expression.
*/
extern
void bt_current_thread_move_error(const bt_error *error);

/*!
@brief
    Moves the ownership of the error \bt_p{_error} from the caller to
    the library, and then sets \bt_p{_error} to \c NULL.

@param[in] _error
    Error of which to move the ownership from you to the library.

@bt_pre_not_null{_error}
@bt_pre_assign_expr{_error}
@pre
    The caller owns \bt_p{_error}.

@post
    The library owns \bt_p{_error}.

@sa bt_current_thread_move_error() &mdash;
    Moves an error's ownership to the library.
*/
#define BT_CURRENT_THREAD_MOVE_ERROR_AND_RESET(_error)	\
	do {						\
		bt_current_thread_move_error(_error);	\
		(_error) = NULL;			\
	} while (0)

/*!
@brief
    Releases the current thread's error, if any.

This function is equivalent to:

@code
bt_error_release(bt_current_thread_take_error());
@endcode

@post
    The current thread has no error.

@sa bt_error_release() &mdash;
    Releases (frees) an error.
@sa bt_current_thread_take_error &mdash;
    Takes the current thread's error, moving its ownership from the
    library to the caller.
*/
extern
void bt_current_thread_clear_error(void);

/*! @} */

/*!
@name Error cause appending
@{
*/

/*!
@brief
    Status codes for the
    <code>bt_current_thread_error_append_cause_from_*()</code>
    functions.
*/
typedef enum bt_current_thread_error_append_cause_status {
	/*!
	@brief
	    Success.
	*/
	BT_CURRENT_THREAD_ERROR_APPEND_CAUSE_STATUS_OK			= __BT_FUNC_STATUS_OK,

	/*!
	@brief
	    Out of memory.
	*/
	BT_CURRENT_THREAD_ERROR_APPEND_CAUSE_STATUS_MEMORY_ERROR	= __BT_FUNC_STATUS_MEMORY_ERROR,
} bt_current_thread_error_append_cause_status;

/*!
@brief
    Appends an error cause to the current thread's error from a
    \bt_comp method.

This this a <code>printf()</code>-like function starting from the
format string parameter \bt_p{message_format}.

On success, the appended error cause's module name
(see bt_error_cause_get_module_name()) is:

@code{.unparsed}
NAME: CC-TYPE.PLUGIN-NAME.CC-NAME
@endcode

or

@code{.unparsed}
NAME: CC-TYPE.CC-NAME
@endcode

if no \bt_plugin provides the class of \bt_p{self_component}, with:

<dl>
  <dt>\c NAME</dt>
  <dd>Name of \bt_p{self_component}.</dd>

  <dt>\c CC-TYPE</dt>
  <dd>
    Type of the \ref api-comp-cls "class" of \bt_p{self_component}
    (\c src, \c flt, or \c sink).
  </dd>

  <dt>\c PLUGIN-NAME</dt>
  <dd>
    Name of the plugin which provides the class of
    \bt_p{self_component}.
  </dd>

  <dt>\c CC-NAME</dt>
  <dd>Name of the class of \bt_p{self_component}.</dd>
</dl>

@param[in] self_component
    Self component of the appending method.
@param[in] file_name
    Name of the source file containing the method which appends the
    error cause.
@param[in] line_number
    Line number of the statement which appends the error cause.
@param[in] message_format
    Format string which specifies how the function converts the
    subsequent arguments (like <code>printf()</code>).

@retval #BT_CURRENT_THREAD_ERROR_APPEND_CAUSE_STATUS_OK
    Success.
@retval #BT_CURRENT_THREAD_ERROR_APPEND_CAUSE_STATUS_MEMORY_ERROR
    Out of memory.

@bt_pre_not_null{self_component}
@bt_pre_not_null{file_name}
@bt_pre_not_null{message_format}
@bt_pre_valid_fmt{message_format}

@post
    <strong>On success</strong>, the number of error causes in the
    current thread's error is incremented.

@sa BT_CURRENT_THREAD_ERROR_APPEND_CAUSE_FROM_COMPONENT() &mdash;
    Calls this function with \c __FILE__ and \c __LINE__ as the
    \bt_p{file_name} and \bt_p{line_number} parameters.
*/
extern
bt_current_thread_error_append_cause_status
bt_current_thread_error_append_cause_from_component(
		bt_self_component *self_component, const char *file_name,
		uint64_t line_number, const char *message_format, ...);

/*!
@brief
    Appends an error cause to the current thread's error from a
    \bt_comp method using \c __FILE__ and \c __LINE__ as the source
    file name and line number.

This macro calls bt_current_thread_error_append_cause_from_component()
with \c __FILE__ and \c __LINE__ as its
\bt_p{file_name} and \bt_p{line_number} parameters.
*/
#define BT_CURRENT_THREAD_ERROR_APPEND_CAUSE_FROM_COMPONENT(_self_component, _message_format, ...) \
	bt_current_thread_error_append_cause_from_component( \
		(_self_component), __FILE__, __LINE__, (_message_format), ##__VA_ARGS__)

/*!
@brief
    Appends an error cause to the current thread's error from a
    \bt_msg_iter method.

This this a <code>printf()</code>-like function starting from the
format string parameter \bt_p{message_format}.

On success, the appended error cause's module name
(see bt_error_cause_get_module_name()) is:

@code{.unparsed}
COMP-NAME (OUT-PORT-NAME): CC-TYPE.PLUGIN-NAME.CC-NAME
@endcode

or

@code{.unparsed}
COMP-NAME (OUT-PORT-NAME): CC-TYPE.CC-NAME
@endcode

if no \bt_plugin provides the component class of
\bt_p{self_message_iterator}, with:

<dl>
  <dt>\c COMP-NAME</dt>
  <dd>Name of the \bt_comp of \bt_p{self_message_iterator}.</dd>

  <dt>\c OUT-PORT-NAME</dt>
  <dd>
    Name of the \bt_oport from which \bt_p{self_message_iterator}.
    was created.
  </dd>

  <dt>\c CC-TYPE</dt>
  <dd>
    Type of the \bt_comp_cls of \bt_p{self_message_iterator}
    (\c src, \c flt, or \c sink).
  </dd>

  <dt>\c PLUGIN-NAME</dt>
  <dd>
    Name of the plugin which provides the component class of
    \bt_p{self_message_iterator}.
  </dd>

  <dt>\c CC-NAME</dt>
  <dd>Name of the component class of \bt_p{self_message_iterator}.</dd>
</dl>

@param[in] self_message_iterator
    Self message iterator of the appending method.
@param[in] file_name
    Name of the source file containing the method which appends the
    error cause.
@param[in] line_number
    Line number of the statement which appends the error cause.
@param[in] message_format
    Format string which specifies how the function converts the
    subsequent arguments (like <code>printf()</code>).

@retval #BT_CURRENT_THREAD_ERROR_APPEND_CAUSE_STATUS_OK
    Success.
@retval #BT_CURRENT_THREAD_ERROR_APPEND_CAUSE_STATUS_MEMORY_ERROR
    Out of memory.

@bt_pre_not_null{self_message_iterator}
@bt_pre_not_null{file_name}
@bt_pre_not_null{message_format}
@bt_pre_valid_fmt{message_format}

@post
    <strong>On success</strong>, the number of error causes in the
    current thread's error is incremented.

@sa BT_CURRENT_THREAD_ERROR_APPEND_CAUSE_FROM_MESSAGE_ITERATOR() &mdash;
    Calls this function with \c __FILE__ and \c __LINE__ as the
    \bt_p{file_name} and \bt_p{line_number} parameters.
*/
extern
bt_current_thread_error_append_cause_status
bt_current_thread_error_append_cause_from_message_iterator(
		bt_self_message_iterator *self_message_iterator,
		const char *file_name, uint64_t line_number,
		const char *message_format, ...);

/*!
@brief
    Appends an error cause to the current thread's error from a
    \bt_msg_iter method using \c __FILE__ and \c __LINE__ as the source file
    name and line number.

This macro calls
bt_current_thread_error_append_cause_from_message_iterator() with
\c __FILE__ and \c __LINE__ as its
\bt_p{file_name} and \bt_p{line_number} parameters.
*/
#define BT_CURRENT_THREAD_ERROR_APPEND_CAUSE_FROM_MESSAGE_ITERATOR(_self_message_iterator, _message_format, ...) \
	bt_current_thread_error_append_cause_from_message_iterator( \
		(_self_message_iterator), __FILE__, __LINE__, (_message_format), ##__VA_ARGS__)

/*!
@brief
    Appends an error cause to the current thread's error from a
    \bt_comp_cls method.

This this a <code>printf()</code>-like function starting from the
format string parameter \bt_p{message_format}.

As of \bt_name_version_min_maj, the only component class method is the
\ref api-qexec "query" method.

On success, the appended error cause's module name
(see bt_error_cause_get_module_name()) is:

@code{.unparsed}
CC-TYPE.PLUGIN-NAME.CC-NAME
@endcode

or

@code{.unparsed}
CC-TYPE.CC-NAME
@endcode

if no \bt_plugin provides \bt_p{self_component_class}, with:

<dl>
  <dt>\c CC-TYPE</dt>
  <dd>
    Type of \bt_p{self_component_class} (\c src, \c flt, or \c sink).
  </dd>

  <dt>\c PLUGIN-NAME</dt>
  <dd>
    Name of the plugin which provides \bt_p{self_component_class}.
  </dd>

  <dt>\c CC-NAME</dt>
  <dd>Name of \bt_p{self_component_class}.</dd>
</dl>

@param[in] self_component_class
    Self component class of the appending method.
@param[in] file_name
    Name of the source file containing the method which appends the
    error cause.
@param[in] line_number
    Line number of the statement which appends the error cause.
@param[in] message_format
    Format string which specifies how the function converts the
    subsequent arguments (like <code>printf()</code>).

@retval #BT_CURRENT_THREAD_ERROR_APPEND_CAUSE_STATUS_OK
    Success.
@retval #BT_CURRENT_THREAD_ERROR_APPEND_CAUSE_STATUS_MEMORY_ERROR
    Out of memory.

@bt_pre_not_null{self_component_class}
@bt_pre_not_null{file_name}
@bt_pre_not_null{message_format}
@bt_pre_valid_fmt{message_format}

@post
    <strong>On success</strong>, the number of error causes in the
    current thread's error is incremented.

@sa BT_CURRENT_THREAD_ERROR_APPEND_CAUSE_FROM_COMPONENT_CLASS() &mdash;
    Calls this function with \c __FILE__ and \c __LINE__ as the
    \bt_p{file_name} and \bt_p{line_number} parameters.
*/
extern
bt_current_thread_error_append_cause_status
bt_current_thread_error_append_cause_from_component_class(
		bt_self_component_class *self_component_class,
		const char *file_name, uint64_t line_number,
		const char *message_format, ...);

/*!
@brief
    Appends an error cause to the current thread's error from a
    component class method using \c __FILE__ and \c __LINE__ as the
    source file name and line number.

This macro calls
bt_current_thread_error_append_cause_from_component_class()
with \c __FILE__ and \c __LINE__ as its
\bt_p{file_name} and \bt_p{line_number} parameters.
*/
#define BT_CURRENT_THREAD_ERROR_APPEND_CAUSE_FROM_COMPONENT_CLASS(_self_component_class, _message_format, ...) \
	bt_current_thread_error_append_cause_from_component_class( \
		(_self_component_class), __FILE__, __LINE__, (_message_format), ##__VA_ARGS__)

/*!
@brief
    Appends an error cause to the current thread's error from any
    function.

Use this function when you cannot use
bt_current_thread_error_append_cause_from_component(),
bt_current_thread_error_append_cause_from_message_iterator(),
or bt_current_thread_error_append_cause_from_component_class().

This this a <code>printf()</code>-like function starting from the
format string parameter \bt_p{message_format}.

@param[in] module_name
    Module name of the error cause to append.
@param[in] file_name
    Name of the source file containing the method which appends the
    error cause.
@param[in] line_number
    Line number of the statement which appends the error cause.
@param[in] message_format
    Format string which specifies how the function converts the
    subsequent arguments (like <code>printf()</code>).

@retval #BT_CURRENT_THREAD_ERROR_APPEND_CAUSE_STATUS_OK
    Success.
@retval #BT_CURRENT_THREAD_ERROR_APPEND_CAUSE_STATUS_MEMORY_ERROR
    Out of memory.

@bt_pre_not_null{module_name}
@bt_pre_not_null{file_name}
@bt_pre_not_null{message_format}
@bt_pre_valid_fmt{message_format}

@post
    <strong>On success</strong>, the number of error causes in the
    current thread's error is incremented.

@sa BT_CURRENT_THREAD_ERROR_APPEND_CAUSE_FROM_UNKNOWN() &mdash;
    Calls this function with \c __FILE__ and \c __LINE__ as the
    \bt_p{file_name} and \bt_p{line_number} parameters.
*/
extern
bt_current_thread_error_append_cause_status
bt_current_thread_error_append_cause_from_unknown(
		const char *module_name, const char *file_name,
		uint64_t line_number, const char *message_format, ...);

/*!
@brief
    Appends an error cause to the current thread's error from any
    function using \c __FILE__ and \c __LINE__ as the source
    file name and line number.

Use this function when you cannot use
BT_CURRENT_THREAD_ERROR_APPEND_CAUSE_FROM_COMPONENT(),
BT_CURRENT_THREAD_ERROR_APPEND_CAUSE_FROM_MESSAGE_ITERATOR(),
or BT_CURRENT_THREAD_ERROR_APPEND_CAUSE_FROM_COMPONENT_CLASS().

This macro calls bt_current_thread_error_append_cause_from_unknown()
with \c __FILE__ and \c __LINE__ as its
\bt_p{file_name} and \bt_p{line_number} parameters.
*/
#define BT_CURRENT_THREAD_ERROR_APPEND_CAUSE_FROM_UNKNOWN(_module_name, _message_format, ...) \
	bt_current_thread_error_append_cause_from_unknown( \
		(_module_name), __FILE__, __LINE__, (_message_format), ##__VA_ARGS__)

/*! @} */

/*!
@name Error
@{
*/

/*!
@brief
    Returns the number of error causes contained in the error
    \bt_p{error}.

@param[in] error
    Error of which to get the number of contained error causes.

@returns
    Number of contained error causes in \bt_p{error}.

@bt_pre_not_null{error}
*/
extern
uint64_t bt_error_get_cause_count(const bt_error *error);

/*!
@brief
    Borrows the error cause at index \bt_p{index} from the
    error \bt_p{error}.

@param[in] error
    Error from which to borrow the error cause at index \bt_p{index}.
@param[in] index
    Index of the error cause to borrow from \bt_p{error}.

@returns
    @parblock
    \em Borrowed reference of the error cause of
    \bt_p{error} at index \bt_p{index}.

    The returned pointer remains valid until \bt_p{error} is modified.
    @endparblock

@bt_pre_not_null{error}
@pre
    \bt_p{index} is less than the number of error causes in
    \bt_p{error} (as returned by bt_error_get_cause_count()).
*/
extern
const bt_error_cause *bt_error_borrow_cause_by_index(
		const bt_error *error, uint64_t index);

/*!
@brief
    Releases (frees) the error \bt_p{error}.

After you call this function, \bt_p{error} does not exist.

Take the current thread's error with bt_current_thread_take_error().

In <a href="https://en.wikipedia.org/wiki/Object-oriented_programming">object-oriented programming</a>
terms, calling this function corresponds to catching an
exception and discarding it.

You can instead move the ownership of \bt_p{error} to the library with
bt_current_thread_move_error(), which,
in object-oriented programming terms,
corresponds to catching an exception and rethrowing it.

@param[in] error
    Error to release (free).

@bt_pre_not_null{error}

@post
    \bt_p{error} does not exist.

@sa bt_current_thread_move_error() &mdash;
    Moves an error's ownership to the library.
*/
extern
void bt_error_release(const bt_error *error);

/*! @} */

/*!
@name Error cause: common
@{
*/

/*!
@brief
    Error cause actor type enumerators.
*/
typedef enum bt_error_cause_actor_type {
	/*!
	@brief
	    Any function.
	*/
	BT_ERROR_CAUSE_ACTOR_TYPE_UNKNOWN		= 1 << 0,

	/*!
	@brief
	    Component method.
	*/
	BT_ERROR_CAUSE_ACTOR_TYPE_COMPONENT		= 1 << 1,

	/*!
	@brief
	    Component class method.
	*/
	BT_ERROR_CAUSE_ACTOR_TYPE_COMPONENT_CLASS	= 1 << 2,

	/*!
	@brief
	    Message iterator method.
	*/
	BT_ERROR_CAUSE_ACTOR_TYPE_MESSAGE_ITERATOR	= 1 << 3,
} bt_error_cause_actor_type;

/*!
@brief
    Returns the actor type enumerator of the error cause
    \bt_p{error_cause}.

@param[in] error_cause
    Error cause of which to get the actor type enumerator.

@returns
    Actor type enumerator of \bt_p{error_cause}.

@bt_pre_not_null{error_cause}
*/
extern
bt_error_cause_actor_type bt_error_cause_get_actor_type(
		const bt_error_cause *error_cause);

/*!
@brief
    Returns the message of the error cause \bt_p{error_cause}.

@param[in] error_cause
    Error cause of which to get the message.

@returns
    @parblock
    Message of \bt_p{error_cause}.

    The returned pointer remains valid as long as the error which
    contains \bt_p{error_cause} exists.
    @endparblock

@bt_pre_not_null{error_cause}
*/
extern
const char *bt_error_cause_get_message(const bt_error_cause *error_cause);

/*!
@brief
    Returns the module name of the error cause \bt_p{error_cause}.

@param[in] error_cause
    Error cause of which to get the module name.

@returns
    @parblock
    Module name of \bt_p{error_cause}.

    The returned pointer remains valid as long as the error which
    contains \bt_p{error_cause} exists.
    @endparblock

@bt_pre_not_null{error_cause}
*/
extern
const char *bt_error_cause_get_module_name(const bt_error_cause *error_cause);

/*!
@brief
    Returns the name of the source file which contains the function
    which appended the error cause \bt_p{error_cause} to the
    current thread's error.

@param[in] error_cause
    Error cause of which to get the source file name.

@returns
    @parblock
    Source file name of \bt_p{error_cause}.

    The returned pointer remains valid as long as the error which
    contains \bt_p{error_cause} exists.
    @endparblock

@bt_pre_not_null{error_cause}
*/
extern
const char *bt_error_cause_get_file_name(const bt_error_cause *error_cause);

/*!
@brief
    Returns the line number of the statement
    which appended the error cause \bt_p{error_cause} to the
    current thread's error.

@param[in] error_cause
    Error cause of which to get the source statement's line number.

@returns
    Source statement's line number of \bt_p{error_cause}.

@bt_pre_not_null{error_cause}
*/
extern
uint64_t bt_error_cause_get_line_number(const bt_error_cause *error_cause);

/*! @} */

/*!
@name Error cause with a component actor
@{
*/

/*!
@brief
    Returns the name of the \bt_comp of which a method
    appended the error cause \bt_p{error_cause} to the current
    thread's error.

@param[in] error_cause
    Error cause of which to get the component name.

@returns
    @parblock
    Component name of \bt_p{error_cause}.

    The returned pointer remains valid as long as the error which
    contains \bt_p{error_cause} exists.
    @endparblock

@bt_pre_not_null{error_cause}
@pre
    The actor type of \bt_p{error_cause} is
    #BT_ERROR_CAUSE_ACTOR_TYPE_COMPONENT.
*/
extern
const char *bt_error_cause_component_actor_get_component_name(
		const bt_error_cause *error_cause);

/*!
@brief
    Returns the \ref api-comp-cls "class" type of the
    \bt_comp of which a method appended the error cause
    \bt_p{error_cause} to the current thread's error.

@param[in] error_cause
    Error cause of which to get the component class type.

@returns
    Component class type of \bt_p{error_cause}.

@bt_pre_not_null{error_cause}
@pre
    The actor type of \bt_p{error_cause} is
    #BT_ERROR_CAUSE_ACTOR_TYPE_COMPONENT.
*/
extern
bt_component_class_type bt_error_cause_component_actor_get_component_class_type(
		const bt_error_cause *error_cause);

/*!
@brief
    Returns the \ref api-comp-cls "class" name of the
    \bt_comp of which a method appended the error cause
    \bt_p{error_cause} to the current thread's error.

@param[in] error_cause
    Error cause of which to get the component class name.

@returns
    @parblock
    Component class name of \bt_p{error_cause}.

    The returned pointer remains valid as long as the error which
    contains \bt_p{error_cause} exists.
    @endparblock

@bt_pre_not_null{error_cause}
@pre
    The actor type of \bt_p{error_cause} is
    #BT_ERROR_CAUSE_ACTOR_TYPE_COMPONENT.
*/
extern
const char *bt_error_cause_component_actor_get_component_class_name(
		const bt_error_cause *error_cause);

/*!
@brief
    Returns the name of the \bt_plugin which provides the
    \ref api-comp-cls "class" of the \bt_comp of which a method
    appended the error cause \bt_p{error_cause} to the
    current thread's error.

This function returns \c NULL if no plugin provides the error cause's
component class.

@param[in] error_cause
    Error cause of which to get the plugin name.

@returns
    @parblock
    Plugin name of \bt_p{error_cause}, or \c NULL if no plugin
    provides the component class of \bt_p{error_cause}.

    The returned pointer remains valid as long as the error which
    contains \bt_p{error_cause} exists.
    @endparblock

@bt_pre_not_null{error_cause}
@pre
    The actor type of \bt_p{error_cause} is
    #BT_ERROR_CAUSE_ACTOR_TYPE_COMPONENT.
*/
extern
const char *bt_error_cause_component_actor_get_plugin_name(
		const bt_error_cause *error_cause);

/*! @} */

/*!
@name Error cause with a message iterator actor
@{
*/

/*!
@brief
    Returns the name of the \bt_oport from which was created
    the message iterator of which the method
    appended the error cause \bt_p{error_cause} to the current
    thread's error.

@param[in] error_cause
    Error cause of which to get the output port name.

@returns
    @parblock
    Output port name of \bt_p{error_cause}.

    The returned pointer remains valid as long as the error which
    contains \bt_p{error_cause} exists.
    @endparblock

@bt_pre_not_null{error_cause}
@pre
    The actor type of \bt_p{error_cause} is
    #BT_ERROR_CAUSE_ACTOR_TYPE_MESSAGE_ITERATOR.
*/
extern
const char *bt_error_cause_message_iterator_actor_get_component_output_port_name(
		const bt_error_cause *error_cause);

/*!
@brief
    Returns the name of the \bt_comp of which a message iterator method
    appended the error cause \bt_p{error_cause} to the current
    thread's error.

@param[in] error_cause
    Error cause of which to get the component name.

@returns
    @parblock
    Component name of \bt_p{error_cause}.

    The returned pointer remains valid as long as the error which
    contains \bt_p{error_cause} exists.
    @endparblock

@bt_pre_not_null{error_cause}
@pre
    The actor type of \bt_p{error_cause} is
    #BT_ERROR_CAUSE_ACTOR_TYPE_MESSAGE_ITERATOR.
*/
extern
const char *bt_error_cause_message_iterator_actor_get_component_name(
		const bt_error_cause *error_cause);

/*!
@brief
    Returns the \ref api-comp-cls "class" type of the
    \bt_comp of which a message iterator method appended the error cause
    \bt_p{error_cause} to the current thread's error.

@param[in] error_cause
    Error cause of which to get the component class type.

@returns
    Component class type of \bt_p{error_cause}.

@bt_pre_not_null{error_cause}
@pre
    The actor type of \bt_p{error_cause} is
    #BT_ERROR_CAUSE_ACTOR_TYPE_MESSAGE_ITERATOR.
*/
extern
bt_component_class_type
bt_error_cause_message_iterator_actor_get_component_class_type(
		const bt_error_cause *error_cause);

/*!
@brief
    Returns the \ref api-comp-cls "class" name of the
    \bt_comp of which a message iterator method appended the error cause
    \bt_p{error_cause} to the current thread's error.

@param[in] error_cause
    Error cause of which to get the component class name.

@returns
    @parblock
    Component class name of \bt_p{error_cause}.

    The returned pointer remains valid as long as the error which
    contains \bt_p{error_cause} exists.
    @endparblock

@bt_pre_not_null{error_cause}
@pre
    The actor type of \bt_p{error_cause} is
    #BT_ERROR_CAUSE_ACTOR_TYPE_MESSAGE_ITERATOR.
*/
extern
const char *bt_error_cause_message_iterator_actor_get_component_class_name(
		const bt_error_cause *error_cause);

/*!
@brief
    Returns the name of the \bt_plugin which provides the
    \ref api-comp-cls "class" of the \bt_comp of which a
    message iterator method
    appended the error cause \bt_p{error_cause} to the
    current thread's error.

This function returns \c NULL if no plugin provides the error cause's
component class.

@param[in] error_cause
    Error cause of which to get the plugin name.

@returns
    @parblock
    Plugin name of \bt_p{error_cause}, or \c NULL if no plugin
    provides the component class of \bt_p{error_cause}.

    The returned pointer remains valid as long as the error which
    contains \bt_p{error_cause} exists.
    @endparblock

@bt_pre_not_null{error_cause}
@pre
    The actor type of \bt_p{error_cause} is
    #BT_ERROR_CAUSE_ACTOR_TYPE_MESSAGE_ITERATOR.
*/
extern
const char *bt_error_cause_message_iterator_actor_get_plugin_name(
		const bt_error_cause *error_cause);

/*! @} */

/*!
@name Error cause with a component class actor
@{
*/

/*!
@brief
    Returns the name of the \bt_comp_cls
    of which a method appended the error cause
    \bt_p{error_cause} to the current thread's error.

@param[in] error_cause
    Error cause of which to get the component class name.

@returns
    Component class name of \bt_p{error_cause}.

@bt_pre_not_null{error_cause}
@pre
    The actor type of \bt_p{error_cause} is
    #BT_ERROR_CAUSE_ACTOR_TYPE_COMPONENT_CLASS.
*/
extern
bt_component_class_type
bt_error_cause_component_class_actor_get_component_class_type(
		const bt_error_cause *error_cause);

/*!
@brief
    Returns the name of the \bt_comp_cls
    of which a method appended the error cause
    \bt_p{error_cause} to the current thread's error.

@param[in] error_cause
    Error cause of which to get the component class name.

@returns
    @parblock
    Component class name of \bt_p{error_cause}.

    The returned pointer remains valid as long as the error which
    contains \bt_p{error_cause} exists.
    @endparblock

@bt_pre_not_null{error_cause}
@pre
    The actor type of \bt_p{error_cause} is
    #BT_ERROR_CAUSE_ACTOR_TYPE_COMPONENT_CLASS.
*/
extern
const char *bt_error_cause_component_class_actor_get_component_class_name(
		const bt_error_cause *error_cause);

/*!
@brief
    Returns the name of the \bt_plugin which provides the
    \bt_comp_cls of which a method
    appended the error cause \bt_p{error_cause} to the
    current thread's error.

This function returns \c NULL if no plugin provides the error cause's
component class.

@param[in] error_cause
    Error cause of which to get the plugin name.

@returns
    @parblock
    Plugin name of \bt_p{error_cause}, or \c NULL if no plugin
    provides the component class of \bt_p{error_cause}.

    The returned pointer remains valid as long as the error which
    contains \bt_p{error_cause} exists.
    @endparblock

@bt_pre_not_null{error_cause}
@pre
    The actor type of \bt_p{error_cause} is
    #BT_ERROR_CAUSE_ACTOR_TYPE_COMPONENT_CLASS.
*/
extern
const char *bt_error_cause_component_class_actor_get_plugin_name(
		const bt_error_cause *error_cause);

/*! @} */

/*! @} */

#ifdef __cplusplus
}
#endif

#endif /* BABELTRACE2_ERROR_REPORTING_H */
