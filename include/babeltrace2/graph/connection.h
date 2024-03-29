/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (C) 2010-2019 EfficiOS Inc. and Linux Foundation
 */

#ifndef BABELTRACE2_GRAPH_CONNECTION_H
#define BABELTRACE2_GRAPH_CONNECTION_H

/* IWYU pragma: private, include <babeltrace2/babeltrace.h> */

#ifndef __BT_IN_BABELTRACE_H
# error "Please include <babeltrace2/babeltrace.h> instead."
#endif

#include <babeltrace2/types.h>

#ifdef __cplusplus
extern "C" {
#endif

/*!
@defgroup api-conn Connection
@ingroup api-graph

@brief
    \bt_c_comp \bt_port connection.

A <strong><em>connection</em></strong> is a link between an \bt_oport
and an \bt_iport.

@image html component-zoom.png

A connection is a \ref api-fund-shared-object "shared object": get a new
reference with bt_connection_get_ref() and put an existing reference with
bt_connection_put_ref().

The type of a connection is #bt_connection.

Borrow the upstream (output) port and downstream (input) port of a
connection with bt_connection_borrow_upstream_port_const() and
bt_connection_borrow_downstream_port_const().
*/

/*! @{ */

/*!
@name Type
@{

@typedef struct bt_connection bt_connection;

@brief
    Connection.

@}
*/

/*!
@name Port access
@{
*/

/*!
@brief
    Borrows the upstream \bt_iport of the connection \bt_p{connection}.

@param[in] connection
    Connection of which to borrow the upstream port.

@returns
    \em Borrowed reference of the upstream port of \bt_p{connection}.

@bt_pre_not_null{connection}
*/
extern const bt_port_input *bt_connection_borrow_downstream_port_const(
		const bt_connection *connection) __BT_NOEXCEPT;

/*!
@brief
    Borrows the downstream \bt_oport of the connection
    \bt_p{connection}.

@param[in] connection
    Connection of which to borrow the downstream port.

@returns
    \em Borrowed reference of the downstream port of \bt_p{connection}.

@bt_pre_not_null{connection}
*/
extern const bt_port_output *bt_connection_borrow_upstream_port_const(
		const bt_connection *connection) __BT_NOEXCEPT;

/*! @} */

/*!
@name Reference count
@{
*/

/*!
@brief
    Increments the \ref api-fund-shared-object "reference count" of
    the connection \bt_p{connection}.

@param[in] connection
    @parblock
    Connection of which to increment the reference count.

    Can be \c NULL.
    @endparblock

@sa bt_connection_put_ref() &mdash;
    Decrements the reference count of a connection.
*/
extern void bt_connection_get_ref(const bt_connection *connection)
		__BT_NOEXCEPT;

/*!
@brief
    Decrements the \ref api-fund-shared-object "reference count" of
    the connection \bt_p{connection}.

@param[in] connection
    @parblock
    Connection of which to decrement the reference count.

    Can be \c NULL.
    @endparblock

@sa bt_connection_get_ref() &mdash;
    Increments the reference count of a connection.
*/
extern void bt_connection_put_ref(const bt_connection *connection)
		__BT_NOEXCEPT;

/*!
@brief
    Decrements the reference count of the connection
    \bt_p{_connection}, and then sets \bt_p{_connection} to \c NULL.

@param _connection
    @parblock
    Connection of which to decrement the reference count.

    Can contain \c NULL.
    @endparblock

@bt_pre_assign_expr{_connection}
*/
#define BT_CONNECTION_PUT_REF_AND_RESET(_connection)	\
	do {						\
		bt_connection_put_ref(_connection);	\
		(_connection) = NULL;			\
	} while (0)

/*!
@brief
    Decrements the reference count of the connection \bt_p{_dst}, sets
    \bt_p{_dst} to \bt_p{_src}, and then sets \bt_p{_src} to \c NULL.

This macro effectively moves a connection reference from the expression
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
#define BT_CONNECTION_MOVE_REF(_dst, _src)	\
	do {					\
		bt_connection_put_ref(_dst);	\
		(_dst) = (_src);		\
		(_src) = NULL;			\
	} while (0)

/*! @} */

/*! @} */

#ifdef __cplusplus
}
#endif

#endif /* BABELTRACE2_GRAPH_CONNECTION_H */
