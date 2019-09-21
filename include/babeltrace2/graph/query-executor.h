#ifndef BABELTRACE2_GRAPH_QUERY_EXECUTOR_H
#define BABELTRACE2_GRAPH_QUERY_EXECUTOR_H

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

#include <babeltrace2/types.h>
#include <babeltrace2/logging.h>

#ifdef __cplusplus
extern "C" {
#endif

/*!
@defgroup api-qexec Query executor
@ingroup api-graph

@brief
    Executor of \bt_comp_cls object queries.

A <strong><em>query executor</em></strong> is an executor of
\bt_comp_cls object queries.

A component class can implement a query method to offer one or more
\em objects depending on the query parameters.

Both the query parameters and the returned objects are \bt_p_val.

The query operation feature exists so that you can get benefit from a
component class's implementation to get information about a trace, a
stream, a distant server, and so on. For example, the
<code>source.ctf.lttng-live</code> component class (see
\bt_man{babeltrace2-source.ctf.lttng-live,7}) offers the \c sessions
object to list the available
<a href="https://lttng.org/docs/#doc-lttng-live">LTTng live</a>
tracing session names and other properties.

The semantics of the query parameters and the returned object are
completely defined by the component class implementation: the library
does not enforce or suggest any layout. The best way to know which
objects you can query from a component class, what are the expected and
optional parameters, and what the returned object contains is to read
this component class's documentation.

The purpose of the query executor itself is to keep some state for a
specific query operation. For example, you can set a query executor's
logging level with bt_query_executor_set_logging_level(); then a
component class's query method can get the executor's current logging
level with bt_query_executor_get_logging_level().

Also, the query executor is an interruptible object: a long or blocking
query operation can run, checking whether the executor is interrupted
periodically, while another thread or a signal handler can interrupt the
executor.

A query executor is a
\ref api-fund-shared-object "shared object": get a new reference with
bt_query_executor_get_ref() and put an existing reference with
bt_query_executor_put_ref().

The type of a query executor is #bt_query_executor.

Create a query executor with bt_query_executor_create() or
bt_query_executor_create_with_method_data(). When you do so, you set the
name of the object to query, from which component class to query the
object, and what are the query parameters. You cannot change those
properties once the query executor is created. With
bt_query_executor_create_with_method_data(), you can also pass
a custom, \bt_voidp pointer to the component class's
query method.

Perform a query operation with bt_query_executor_query(). This function
can return #BT_QUERY_EXECUTOR_QUERY_STATUS_AGAIN, in which case you can
try to perform a query operation again later. It can also return
#BT_QUERY_EXECUTOR_QUERY_STATUS_UNKNOWN_OBJECT, which means the
component class does not offer the requested object.

To interrupt a running query operation, either:

- Borrow the query executor's default \bt_intr with
  bt_query_executor_borrow_default_interrupter() and set it with
  bt_interrupter_set().

  When you call bt_query_executor_create() or
  bt_query_executor_create_with_method_data(), the returned query
  executor has a default interrupter.

- Add your own interrupter with bt_query_executor_add_interrupter()
  \em before you perform the query operation with
  bt_query_executor_query().

  Then, set the interrupter with bt_interrupter_set().

<h1>Property</h1>

A query executor has the following property:

<dl>
  <dt>
    \anchor api-qexec-prop-log-lvl
    Logging level
  </dt>
  <dd>
    Logging level of the query executor's query operations.

    Use bt_query_executor_set_logging_level() and
    bt_query_executor_get_logging_level().
  </dd>
</dl>
*/

/*! @{ */

/*!
@name Type
@{

@typedef struct bt_query_executor bt_query_executor;

@brief
    Query executor.

@}
*/

/*!
@name Creation
@{
*/

/*!
@brief
    Alias of bt_query_executor_create_with_method_data()
    with the \bt_p{method_data} parameter set to \c NULL.
*/
extern
bt_query_executor *bt_query_executor_create(
		const bt_component_class *component_class,
		const char *object_name, const bt_value *params);

/*!
@brief
    Creates a query executor to query the object named
    \bt_p{object_name} from the \bt_comp_cls \bt_p{component_class} with
    the parameters \bt_p{params} and the query user data
    \bt_p{method_data}.

When you call bt_query_executor_query() with the returned query
executor, the query method of \bt_p{component_class} receives:

- \bt_p{object_name} as its own \bt_p{object_name} parameter.

- \bt_p{params} as its own \bt_p{params} parameter
  (or #bt_value_null if \bt_p{params} is \c NULL).

- \bt_p{method_data} as its own \bt_p{method_data} parameter.

@param[in] component_class
    Component class from which to query the object named
    \bt_p{object_name}.
@param[in] object_name
    Name of the object to query from \bt_p{component_class}.
@param[in] params
    @parblock
    Parameters for the query operation performed by the query executor
    to create.

    Unlike the \bt_p{params} parameter of
    the <code>bt_graph_add_*_component_*_port_added_listener()</code>
    functions (see \ref api-graph), this parameter does not need to
    be a \bt_map_val.

    Can be \c NULL (equivalent to passing #bt_value_null).
    @endparblock
@param[in] method_data
    User data passed as is to the query method of \bt_p{component_class}
    when you call bt_query_executor_query().

@returns
    New query executor reference, or \c NULL on memory error.

@bt_pre_not_null{component_class}
@bt_pre_not_null{object}

@bt_post_success_frozen{component_class}
@bt_post_success_frozen{params}
*/
extern
bt_query_executor *bt_query_executor_create_with_method_data(
		const bt_component_class *component_class,
		const char *object_name, const bt_value *params,
		void *method_data);

/*! @} */

/*!
@name Query operation
@{
*/

/*!
@brief
    Status codes for bt_query_executor_query().
*/
typedef enum bt_query_executor_query_status {
	/*!
	@brief
	    Success.
	*/
	BT_QUERY_EXECUTOR_QUERY_STATUS_OK		= __BT_FUNC_STATUS_OK,

	/*!
	@brief
	    Unknown object to query.
	*/
	BT_QUERY_EXECUTOR_QUERY_STATUS_UNKNOWN_OBJECT	= __BT_FUNC_STATUS_UNKNOWN_OBJECT,

	/*!
	@brief
	    Try again.
	*/
	BT_QUERY_EXECUTOR_QUERY_STATUS_AGAIN		= __BT_FUNC_STATUS_AGAIN,

	/*!
	@brief
	    Out of memory.
	*/
	BT_QUERY_EXECUTOR_QUERY_STATUS_MEMORY_ERROR	= __BT_FUNC_STATUS_MEMORY_ERROR,

	/*!
	@brief
	    Other error.
	*/
	BT_QUERY_EXECUTOR_QUERY_STATUS_ERROR		= __BT_FUNC_STATUS_ERROR,
} bt_query_executor_query_status;

/*!
@brief
    Performs a query operation using the query executor
    \bt_p{query_executor}, setting \bt_p{*result} to the operation's
    result on success.

This function calls the query executor's target \bt_comp_cls's
query method, passing:

- The object name of \bt_p{query_executor} as the
  \bt_p{object_name} parameter.

- The query parameters of \bt_p{query_executor} as the
  \bt_p{params} parameter.

- The query user data of \bt_p{query_executor} as the \bt_p{method_data}
  parameter.

The three items above were set when you created \bt_p{query_executor}
with bt_query_executor_create() or
bt_query_executor_create_with_method_data().

@param[in] query_executor
    Query executor to use to execute the query operation.
@param[out] result
    <strong>On success</strong>, \bt_p{*result} is a \em strong
    reference of the query operation's result.

@retval #BT_QUERY_EXECUTOR_QUERY_STATUS_OK
    Success.
@retval #BT_QUERY_EXECUTOR_QUERY_STATUS_UNKNOWN_OBJECT
    Unknown object to query.
@retval #BT_QUERY_EXECUTOR_QUERY_STATUS_AGAIN
    Try again.
@retval #BT_QUERY_EXECUTOR_QUERY_STATUS_MEMORY_ERROR
    Out of memory.
@retval #BT_QUERY_EXECUTOR_QUERY_STATUS_ERROR
    Other error.

@bt_pre_not_null{query_executor}
@bt_pre_not_null{result}
*/
extern
bt_query_executor_query_status bt_query_executor_query(
		bt_query_executor *query_executor, const bt_value **result);

/*! @} */

/*!
@name Property
@{
*/

/*!
@brief
    Status codes for bt_query_executor_set_logging_level().
*/
typedef enum bt_query_executor_set_logging_level_status {
	/*!
	@brief
	    Success.
	*/
	BT_QUERY_EXECUTOR_SET_LOGGING_LEVEL_STATUS_OK	= __BT_FUNC_STATUS_OK,
} bt_query_executor_set_logging_level_status;

/*!
@brief
    Sets the logging level of the query executor \bt_p{query_executor}
    to \bt_p{logging_level}.

See the \ref api-qexec-prop-log-lvl "logging level" property.

@param[in] query_executor
    Query executor of which to set the logging level to
    \bt_p{logging_level}.
@param[in] logging_level
    New logging level of \bt_p{query_executor}.

@retval #BT_QUERY_EXECUTOR_SET_LOGGING_LEVEL_STATUS_OK
    Success.

@bt_pre_not_null{query_executor}

@sa bt_stream_class_get_logging_level() &mdash;
    Returns the logging level of a query executor.
*/
extern bt_query_executor_set_logging_level_status
bt_query_executor_set_logging_level(bt_query_executor *query_executor,
		bt_logging_level logging_level);

/*!
@brief
    Returns the logging level of the query executor
    \bt_p{query_executor}.

See the \ref api-qexec-prop-log-lvl "logging level" property.

@param[in] query_executor
    Query executor of which to get the logging level.

@returns
    Logging level of \bt_p{query_executor}.

@bt_pre_not_null{query_executor}

@sa bt_query_executor_set_logging_level() &mdash;
    Sets the logging level of a query executor.
*/
extern bt_logging_level bt_query_executor_get_logging_level(
		const bt_query_executor *query_executor);

/*! @} */

/*!
@name Interruption
@{
*/

/*!
@brief
    Status codes for bt_query_executor_add_interrupter().
*/
typedef enum bt_query_executor_add_interrupter_status {
	/*!
	@brief
	    Success.
	*/
	BT_QUERY_EXECUTOR_ADD_INTERRUPTER_STATUS_OK		= __BT_FUNC_STATUS_OK,

	/*!
	@brief
	    Out of memory.
	*/
	BT_QUERY_EXECUTOR_ADD_INTERRUPTER_STATUS_MEMORY_ERROR	= __BT_FUNC_STATUS_MEMORY_ERROR,
} bt_query_executor_add_interrupter_status;

/*!
@brief
    Adds the \bt_intr \bt_p{interrupter} to the query executor
    \bt_p{query_executor}.

A \bt_comp_cls query method can check whether or not its executor is
interrupted (any of its interrupters, including its default interrupter,
is set) with bt_query_executor_is_interrupted().

@note
    @parblock
    bt_query_executor_create() and
    bt_query_executor_create_with_method_data() return a query executor
    which comes with its own <em>default interrupter</em>.

    Instead of adding your own interrupter to \bt_p{query_executor}, you
    can set its default interrupter with

    @code
    bt_interrupter_set(bt_query_executor_borrow_default_interrupter());
    @endcode
    @endparblock

@param[in] query_executor
    Query executor to which to add \bt_p{interrupter}.
@param[in] interrupter
    Interrupter to add to \bt_p{query_executor}.

@retval #BT_QUERY_EXECUTOR_ADD_INTERRUPTER_STATUS_OK
    Success.
@retval #BT_QUERY_EXECUTOR_ADD_INTERRUPTER_STATUS_MEMORY_ERROR
    Out of memory.

@bt_pre_not_null{query_executor}
@bt_pre_not_null{interrupter}

@sa bt_query_executor_borrow_default_interrupter() &mdash;
    Borrows the default interrupter from a query executor.
*/
extern bt_query_executor_add_interrupter_status
bt_query_executor_add_interrupter(bt_query_executor *query_executor,
		const bt_interrupter *interrupter);

/*!
@brief
    Borrows the default \bt_intr from the query executor
    \bt_p{query_executor}.

@param[in] query_executor
    Query executor from which to borrow the default interrupter.

@returns
    @parblock
    \em Borrowed reference of the default interrupter of
    \bt_p{query_executor}.

    The returned pointer remains valid as long as \bt_p{query_executor}
    exists.
    @endparblock

@bt_pre_not_null{query_executor}

@sa bt_query_executor_add_interrupter() &mdash;
    Adds an interrupter to a query executor.
*/
extern bt_interrupter *bt_query_executor_borrow_default_interrupter(
		bt_query_executor *query_executor);

/*!
@brief
    Returns whether or not the query executor \bt_p{query_executor}
    is interrupted, that is, whether or not any of its \bt_p_intr,
    including its default interrupter, is set.

@param[in] query_executor
    Query executor to check.

@returns
    #BT_TRUE if \bt_p{query_executor} is interrupted (any of its
    interrupters is set).

@bt_pre_not_null{query_executor}
*/
extern bt_bool bt_query_executor_is_interrupted(
		const bt_query_executor *query_executor);

/*! @} */

/*!
@name Reference count
@{
*/

/*!
@brief
    Increments the \ref api-fund-shared-object "reference count" of
    the query executor \bt_p{query_executor}.

@param[in] query_executor
    @parblock
    Query executor of which to increment the reference count.

    Can be \c NULL.
    @endparblock

@sa bt_query_executor_put_ref() &mdash;
    Decrements the reference count of a query executor.
*/
extern void bt_query_executor_get_ref(const bt_query_executor *query_executor);

/*!
@brief
    Decrements the \ref api-fund-shared-object "reference count" of
    the query executor \bt_p{query_executor}.

@param[in] query_executor
    @parblock
    Query executor of which to decrement the reference count.

    Can be \c NULL.
    @endparblock

@sa bt_query_executor_get_ref() &mdash;
    Increments the reference count of a query executor.
*/
extern void bt_query_executor_put_ref(const bt_query_executor *query_executor);

/*!
@brief
    Decrements the reference count of the query executor
    \bt_p{_query_executor}, and then sets \bt_p{_query_executor} to \c NULL.

@param _query_executor
    @parblock
    Query executor of which to decrement the reference count.

    Can contain \c NULL.
    @endparblock

@bt_pre_assign_expr{_query_executor}
*/
#define BT_QUERY_EXECUTOR_PUT_REF_AND_RESET(_query_executor)	\
	do {							\
		bt_query_executor_put_ref(_query_executor);	\
		(_query_executor) = NULL;			\
	} while (0)

/*!
@brief
    Decrements the reference count of the query executor \bt_p{_dst}, sets
    \bt_p{_dst} to \bt_p{_src}, and then sets \bt_p{_src} to \c NULL.

This macro effectively moves a query executor reference from the expression
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
#define BT_QUERY_EXECUTOR_MOVE_REF(_dst, _src)		\
	do {						\
		bt_query_executor_put_ref(_dst);	\
		(_dst) = (_src);			\
		(_src) = NULL;				\
	} while (0)

/*! @} */

/*! @} */

#ifdef __cplusplus
}
#endif

#endif /* BABELTRACE2_GRAPH_QUERY_EXECUTOR_H */
