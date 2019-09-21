#ifndef BABELTRACE2_GRAPH_PRIVATE_QUERY_EXECUTOR_H
#define BABELTRACE2_GRAPH_PRIVATE_QUERY_EXECUTOR_H

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
@defgroup api-priv-qexec Private query executor
@ingroup api-comp-cls-dev

@brief
    Private view of a \bt_qexec for a \bt_comp_cls
    \ref api-comp-cls-dev-meth-query "query method".

A <strong><em>private query executor</em></strong> is a private view,
from within a \bt_comp_cls
\ref api-comp-cls-dev-meth-query "query method", of a
\bt_qexec.

A query method receives a private query executor as its
\bt_p{query_executor} parameter.

As of \bt_name_version_min_maj, this module only offers the
bt_private_query_executor_as_query_executor_const() function to
\ref api-fund-c-typing "upcast" a private query executor to a
\c const query executor. You need this to get the query executor's
\ref api-qexec-prop-log-lvl "logging level".
*/

/*! @{ */

/*!
@name Type
@{

@typedef struct bt_private_query_executor bt_private_query_executor;

@brief
    Private query executor.

@}
*/

/*!
@name Upcast
@{
*/

/*!
@brief
    \ref api-fund-c-typing "Upcasts" the private query executor
    \bt_p{query_executor} to the public #bt_query_executor type.

@param[in] query_executor
    @parblock
    Private query executor to upcast.

    Can be \c NULL.
    @endparblock

@returns
    \bt_p{query_executor} as a public query executor.
*/
static inline
const bt_query_executor *
bt_private_query_executor_as_query_executor_const(
		bt_private_query_executor *query_executor)
{
	return __BT_UPCAST_CONST(bt_query_executor, query_executor);
}

/*! @} */

/*! @} */

#ifdef __cplusplus
}
#endif

#endif /* BABELTRACE2_GRAPH_PRIVATE_QUERY_EXECUTOR_H */
