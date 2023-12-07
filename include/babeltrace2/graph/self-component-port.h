/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (C) 2010-2019 EfficiOS Inc. and Linux Foundation
 */

#ifndef BABELTRACE2_GRAPH_SELF_COMPONENT_PORT_H
#define BABELTRACE2_GRAPH_SELF_COMPONENT_PORT_H

/* IWYU pragma: private, include <babeltrace2/babeltrace.h> */

#ifndef __BT_IN_BABELTRACE_H
# error "Please include <babeltrace2/babeltrace.h> instead."
#endif

#include <babeltrace2/types.h>

#ifdef __cplusplus
extern "C" {
#endif

/*!
@defgroup api-self-comp-port Self component ports
@ingroup api-self-comp

@brief
    Private views of \bt_p_port for \bt_comp_cls instance methods.

The #bt_self_component_port, #bt_self_component_port_input, and
#bt_self_component_port_output types are private views of a \bt_port
from within a component class
\ref api-comp-cls-dev-instance-meth "instance method".

Borrow the \bt_self_comp of a port with
bt_self_component_port_borrow_component().

Get the user data attached to a port with
bt_self_component_port_get_data().

\ref api-fund-c-typing "Upcast" the "self" (private) types to the
public and common self component port types with the
<code>bt_self_component_port*_as_port*()</code> and
<code>bt_self_component_port_*_as_self_component_port()</code>
functions.
*/

/*! @{ */

/*!
@name Types
@{

@typedef struct bt_self_component_port bt_self_component_port;

@brief
    Self component \bt_comp.

@typedef struct bt_self_component_port_input bt_self_component_port_input;

@brief
    Self component \bt_iport.

@typedef struct bt_self_component_port_output bt_self_component_port_output;

@brief
    Self component \bt_oport.

@}
*/

/*!
@name Component access
@{
*/

/*!
@brief
    Borrows the \bt_comp of the \bt_port \bt_p{self_component_port}.

@param[in] self_component_port
    Port from which to borrow the component which owns it.

@returns
    Component which owns \bt_p{self_component_port}.

@bt_pre_not_null{self_component_port}
*/
extern bt_self_component *bt_self_component_port_borrow_component(
		bt_self_component_port *self_component_port);

/*! @} */

/*!
@name User data access
@{
*/

/*!
@brief
    Returns the user data of the \bt_port \bt_p{self_component_port}.

You can attach user data to a port when you add it to a component with
bt_self_component_source_add_output_port(),
bt_self_component_filter_add_input_port(),
bt_self_component_filter_add_output_port(), and
bt_self_component_sink_add_input_port().

@param[in] self_component_port
    Port of which to get the user data.

@returns
    User data of \bt_p{self_component_port}.

@bt_pre_not_null{self_component_port}
*/
extern void *bt_self_component_port_get_data(
		const bt_self_component_port *self_component_port);

/*! @} */

/*!
@name Self to public upcast
@{
*/

/*!
@brief
    \ref api-fund-c-typing "Upcasts" the self component port
    \bt_p{self_component_port} to the public #bt_port type.

@param[in] self_component_port
    @parblock
    Port to upcast.

    Can be \c NULL.
    @endparblock

@returns
    \bt_p{self_component_port} as a public port.
*/
static inline
const bt_port *bt_self_component_port_as_port(
		bt_self_component_port *self_component_port)
{
	return __BT_UPCAST_CONST(bt_port, self_component_port);
}

/*!
@brief
    \ref api-fund-c-typing "Upcasts" the self component \bt_iport
    \bt_p{self_component_port} to the public #bt_port_input type.

@param[in] self_component_port
    @parblock
    Input port to upcast.

    Can be \c NULL.
    @endparblock

@returns
    \bt_p{self_component_port} as a public input port.
*/
static inline
const bt_port_input *bt_self_component_port_input_as_port_input(
		const bt_self_component_port_input *self_component_port)
{
	return __BT_UPCAST_CONST(bt_port_input, self_component_port);
}

/*!
@brief
    \ref api-fund-c-typing "Upcasts" the self component \bt_oport
    \bt_p{self_component_port} to the public #bt_port_output type.

@param[in] self_component_port
    @parblock
    Output port to upcast.

    Can be \c NULL.
    @endparblock

@returns
    \bt_p{self_component_port} as a public output port.
*/
static inline
const bt_port_output *bt_self_component_port_output_as_port_output(
		bt_self_component_port_output *self_component_port)
{
	return __BT_UPCAST_CONST(bt_port_output, self_component_port);
}

/*! @} */

/*!
@name Self to common self upcast
@{
*/

/*!
@brief
    \ref api-fund-c-typing "Upcasts" the self \bt_iport
    \bt_p{self_component_port} to the common #bt_self_component_port
    type.

@param[in] self_component_port
    @parblock
    Input port to upcast.

    Can be \c NULL.
    @endparblock

@returns
    \bt_p{self_component_port} as a common self component port.
*/
static inline
bt_self_component_port *
bt_self_component_port_input_as_self_component_port(
		bt_self_component_port_input *self_component_port)
{
	return __BT_UPCAST(bt_self_component_port, self_component_port);
}

/*!
@brief
    \ref api-fund-c-typing "Upcasts" the self \bt_oport
    \bt_p{self_component_port} to the common #bt_self_component_port
    type.

@param[in] self_component_port
    @parblock
    Output port to upcast.

    Can be \c NULL.
    @endparblock

@returns
    \bt_p{self_component_port} as a common self component port.
*/
static inline
bt_self_component_port *
bt_self_component_port_output_as_self_component_port(
		bt_self_component_port_output *self_component_port)
{
	return __BT_UPCAST(bt_self_component_port, self_component_port);
}

/*! @} */

/*! @} */

#ifdef __cplusplus
}
#endif

#endif /* BABELTRACE2_GRAPH_SELF_COMPONENT_PORT_H */
