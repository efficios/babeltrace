#ifndef BABELTRACE2_GRAPH_INTERRUPTER_H
#define BABELTRACE2_GRAPH_INTERRUPTER_H

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

#ifdef __cplusplus
extern "C" {
#endif

/*!
@defgroup api-intr Interrupter
@ingroup api-graph

@brief
    Interrupter.

An <strong><em>interrupter</em></strong> is a simple object which has
a single boolean state: set or not set.

You can use an interrupter to interrupt a running trace processing
\bt_graph or \ref api-qexec "query". The user and library functions periodically
check if they are interrupted (with
bt_self_component_sink_is_interrupted(),
bt_self_message_iterator_is_interrupted(), or
bt_query_executor_is_interrupted()); meanwhile, another thread or
a signal handler sets the shared interrupter with bt_interrupter_set().

To interrupt a running trace processing graph or query:

-# Create an interrupter with bt_interrupter_create().

-# Before running a trace processing graph with bt_graph_run() or
   performing a query with bt_query_executor_query(), add
   the created interrupter to the object with bt_graph_add_interrupter()
   or bt_query_executor_add_interrupter().

   Alternatively, you can borrow the existing, default interrupter from
   those objects with bt_graph_borrow_default_interrupter() and
   bt_query_executor_borrow_default_interrupter().

-# Run the graph with bt_graph_run() or perform the query with
   bt_query_executor_query().

-# From a signal handler or another thread, call bt_interrupter_set() to
   set the shared interrupter.

Eventually, the trace processing graph or query thread checks if it's
interrupted and stops processing, usually returning a status code which
ends with \c _AGAIN.

You can add more than one interrupter to a trace processing graph and
to a query executor. The bt_self_component_sink_is_interrupted(),
bt_self_message_iterator_is_interrupted(), and
bt_query_executor_is_interrupted() functions return the logical
disjunction of all the added interrupters's states, so that \em any
interrupter can interrupt the thread.

Once a trace processing graph or a query executor is interrupted and
you get the thread's control back, you can reset the interrupter
with bt_interrupter_reset() and continue the previous operation,
calling bt_graph_run() or bt_query_executor_query() again.

An interrupter is a \ref api-fund-shared-object "shared object": get a
new reference with bt_interrupter_get_ref() and put an existing
reference with bt_interrupter_put_ref().

The type of an interrupter is #bt_interrupter.
*/

/*! @{ */

/*!
@name Type
@{

@typedef struct bt_interrupter bt_interrupter;

@brief
    Interrupter.

@}
*/

/*!
@name Creation
@{
*/

/*!
@brief
    Creates a default interrupter.

On success, the returned interrupter is \em not set
(bt_interrupter_is_set() returns #BT_FALSE).

@returns
    New interrupter reference, or \c NULL on memory error.
*/
extern bt_interrupter *bt_interrupter_create(void);

/*! @} */

/*!
@name State
@{
*/

/*!
@brief
    Sets the interrupter \bt_p{interrupter}.

After you call this function, bt_interrupter_is_set() returns
#BT_TRUE.

@param[in] interrupter
    Interrupter to set.

@bt_pre_not_null{interrupter}

@sa bt_interrupter_reset() &mdash;
    Resets an interrupter.
@sa bt_interrupter_is_set() &mdash;
    Returns whether or not an interrupter is set.
*/
extern void bt_interrupter_set(bt_interrupter *interrupter);

/*!
@brief
    Resets the interrupter \bt_p{interrupter}.

After you call this function, bt_interrupter_is_set() returns
#BT_FALSE.

@param[in] interrupter
    Interrupter to reset.

@bt_pre_not_null{interrupter}

@sa bt_interrupter_set() &mdash;
    Sets an interrupter.
@sa bt_interrupter_is_set() &mdash;
    Returns whether or not an interrupter is set.
*/
extern void bt_interrupter_reset(bt_interrupter *interrupter);

/*!
@brief
    Returns whether or not the interrupter \bt_p{interrupter} is set.

@param[in] interrupter
    Interrupter to reset.

@returns
    #BT_TRUE if \bt_p{interrupter} is set.

@bt_pre_not_null{interrupter}

@sa bt_interrupter_set() &mdash;
    Sets an interrupter.
@sa bt_interrupter_reset() &mdash;
    Resets an interrupter.
*/
extern bt_bool bt_interrupter_is_set(const bt_interrupter *interrupter);

/*! @} */

/*!
@name Reference count
@{
*/

/*!
@brief
    Increments the \ref api-fund-shared-object "reference count" of
    the interrupter \bt_p{interrupter}.

@param[in] interrupter
    @parblock
    Interrupter of which to increment the reference count.

    Can be \c NULL.
    @endparblock

@sa bt_interrupter_put_ref() &mdash;
    Decrements the reference count of an interrupter.
*/
extern void bt_interrupter_get_ref(const bt_interrupter *interrupter);

/*!
@brief
    Decrements the \ref api-fund-shared-object "reference count" of
    the interrupter \bt_p{interrupter}.

@param[in] interrupter
    @parblock
    Interrupter of which to decrement the reference count.

    Can be \c NULL.
    @endparblock

@sa bt_interrupter_get_ref() &mdash;
    Increments the reference count of an interrupter.
*/
extern void bt_interrupter_put_ref(const bt_interrupter *interrupter);

/*!
@brief
    Decrements the reference count of the interrupter
    \bt_p{_interrupter}, and then sets \bt_p{_interrupter} to \c NULL.

@param _interrupter
    @parblock
    Interrupter of which to decrement the reference count.

    Can contain \c NULL.
    @endparblock

@bt_pre_assign_expr{_interrupter}
*/
#define BT_INTERRUPTER_PUT_REF_AND_RESET(_interrupter)	\
	do {						\
		bt_interrupter_put_ref(_interrupter);	\
		(_interrupter) = NULL;			\
	} while (0)

/*!
@brief
    Decrements the reference count of the interrupter \bt_p{_dst}, sets
    \bt_p{_dst} to \bt_p{_src}, and then sets \bt_p{_src} to \c NULL.

This macro effectively moves an interrupter reference from the
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
#define BT_INTERRUPTER_MOVE_REF(_dst, _src)	\
	do {					\
		bt_interrupter_put_ref(_dst);	\
		(_dst) = (_src);		\
		(_src) = NULL;			\
	} while (0)

/*! @} */

/*! @} */

#ifdef __cplusplus
}
#endif

#endif /* BABELTRACE2_GRAPH_INTERRUPTER_H */
