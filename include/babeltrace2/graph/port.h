#ifndef BABELTRACE2_GRAPH_PORT_H
#define BABELTRACE2_GRAPH_PORT_H

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
@defgroup api-port Ports
@ingroup api-comp

@brief
    \bt_c_comp input and output ports.

A <strong><em>port</em></strong> is a point of \bt_conn between
\bt_p_comp:

@image html component-zoom.png

A port is a \ref api-fund-shared-object "shared object": get a new
reference with bt_port_get_ref() and put an existing reference with
bt_port_put_ref().

The common C&nbsp;type of a port is #bt_port.

There are two types of ports:

<dl>
  <dt>\anchor api-port-in Input port</dt>
  <dd>
    Input connection point from which \bt_p_msg are received.

    Filter and sink components have input ports.

    An input port's specific type is #bt_port_input and its type
    enumerator is #BT_PORT_TYPE_INPUT.

    \ref api-fund-c-typing "Upcast" the #bt_port_input type to the
    #bt_port type with bt_port_input_as_port_const().

    Get a new input port reference with bt_port_input_get_ref() and put
    an existing one with bt_port_input_put_ref().
  </dd>

  <dt>\anchor api-port-out Output port</dt>
  <dd>
    Output connection point to which messages are sent.

    Source and filter components have output ports.

    An output port's specific type is #bt_port_output and its type
    enumerator is #BT_PORT_TYPE_OUTPUT.

    \ref api-fund-c-typing "Upcast" the #bt_port_output type to the
    #bt_port type with bt_port_output_as_port_const().

    Get a new output port reference with bt_port_output_get_ref() and
    put an existing one with bt_port_output_put_ref().
  </dd>
</dl>

Get a port's type enumerator with bt_port_get_type(). You can also use
the bt_port_is_input() and bt_port_is_output() helper functions.

A \bt_comp can add a port with:

- bt_self_component_source_add_output_port()
- bt_self_component_filter_add_input_port()
- bt_self_component_filter_add_output_port()
- bt_self_component_sink_add_input_port()

Borrow a port's \bt_conn, if any, with
bt_port_borrow_connection_const().

Borrow the component to which a port belongs with
bt_port_borrow_component_const().

<h1>Properties</h1>

A port has the following common properties:

<dl>
  <dt>
    \anchor api-port-prop-name
    Name
  </dt>
  <dd>
    Name of the port.

    For a given \bt_comp:

    - Each input port has a unique name.
    - Each output port has a unique name.

    A port's name is set when the component adds it; you cannot change
    it afterwards.

    Get a port's name with bt_port_get_name().
  </dd>

  <dt>
    \anchor api-port-prop-is-connected
    Is connected?
  </dt>
  <dd>
    Whether or not the port is currently connected to another port.

    Get whether or not a port is connected with bt_port_is_connected().

    When a port is unconnected, bt_port_borrow_connection_const()
    returns \c NULL.
  </dd>
</dl>
*/

/*! @{ */

/*!
@name Types
@{

@typedef struct bt_port bt_port;

@brief
    Port.

@typedef struct bt_port_input bt_port_input;

@brief
    \bt_c_iport.

@typedef struct bt_port_output bt_port_output;

@brief
    \bt_c_oport.

@}
*/

/*!
@name Type query
@{
*/

/*!
@brief
    Port type enumerators.
*/
typedef enum bt_port_type {
	/*!
	@brief
	    \bt_c_iport.
	*/
	BT_PORT_TYPE_INPUT	= 1 << 0,

	/*!
	@brief
	    \bt_c_oport.
	*/
	BT_PORT_TYPE_OUTPUT	= 1 << 1,
} bt_port_type;

/*!
@brief
    Returns the type enumerator of the port \bt_p{port}.

@param[in] port
    Port of which to get the type enumerator

@returns
    Type enumerator of \bt_p{port}.

@bt_pre_not_null{port}

@sa bt_port_is_input() &mdash;
    Returns whether or not a port is an \bt_iport.
@sa bt_port_is_output() &mdash;
    Returns whether or not a port is an \bt_oport.
*/
extern bt_port_type bt_port_get_type(const bt_port *port);

/*!
@brief
    Returns whether or not the port \bt_p{port} is an \bt_iport.

@param[in] port
    Port to check.

@returns
    #BT_TRUE if \bt_p{port} is an input port.

@bt_pre_not_null{port}

@sa bt_port_get_type() &mdash;
    Returns the type enumerator of a port.
*/
static inline
bt_bool bt_port_is_input(const bt_port *port)
{
	return bt_port_get_type(port) == BT_PORT_TYPE_INPUT;
}

/*!
@brief
    Returns whether or not the port \bt_p{port} is an \bt_oport.

@param[in] port
    Port to check.

@returns
    #BT_TRUE if \bt_p{port} is an output port.

@bt_pre_not_null{port}

@sa bt_port_get_type() &mdash;
    Returns the type enumerator of a port.
*/
static inline
bt_bool bt_port_is_output(const bt_port *port)
{
	return bt_port_get_type(port) == BT_PORT_TYPE_OUTPUT;
}

/*! @} */

/*!
@name Connection access
@{
*/

/*!
@brief
    Borrows the \bt_conn of the port \bt_p{port}.

This function returns \c NULL if \bt_p{port} is unconnected
(bt_port_is_connected() returns #BT_FALSE).

@param[in] port
    Port of which to borrow the connection.

@returns
    \em Borrowed reference of the connection of \bt_p{port}.

@bt_pre_not_null{port}
*/
extern const bt_connection *bt_port_borrow_connection_const(
		const bt_port *port);

/*! @} */

/*!
@name Component access
@{
*/

/*!
@brief
    Borrows the \bt_comp to which the port \bt_p{port} belongs.

@param[in] port
    Port of which to borrow the component which owns it.

@returns
    \em Borrowed reference of the component which owns \bt_p{port}.

@bt_pre_not_null{port}
*/
extern const bt_component *bt_port_borrow_component_const(
		const bt_port *port);

/*! @} */

/*!
@name Properties
@{
*/

/*!
@brief
    Returns the name of the port \bt_p{port}.

See the \ref api-port-prop-name "name" property.

@param[in] port
    Port of which to get the name.

@returns
    @parblock
    Name of \bt_p{port}, or \c NULL if none.

    The returned pointer remains valid as long as \bt_p{port} exists.
    @endparblock

@bt_pre_not_null{port}
*/
extern const char *bt_port_get_name(const bt_port *port);

/*!
@brief
    Returns whether or not the port \bt_p{port} is connected.

See the \ref api-port-prop-is-connected "is connected?" property.

@param[in] port
    Port of which to get whether or not it's connected.

@returns
    #BT_TRUE if \bt_p{port} is connected.

@bt_pre_not_null{port}
*/
extern bt_bool bt_port_is_connected(const bt_port *port);

/*! @} */

/*!
@name Reference count (common)
@{
*/

/*!
@brief
    Increments the \ref api-fund-shared-object "reference count" of
    the port \bt_p{port}.

@param[in] port
    @parblock
    Port of which to increment the reference count.

    Can be \c NULL.
    @endparblock

@sa bt_port_put_ref() &mdash;
    Decrements the reference count of a port.
*/
extern void bt_port_get_ref(const bt_port *port);

/*!
@brief
    Decrements the \ref api-fund-shared-object "reference count" of
    the port \bt_p{port}.

@param[in] port
    @parblock
    Port of which to decrement the reference count.

    Can be \c NULL.
    @endparblock

@sa bt_port_get_ref() &mdash;
    Increments the reference count of a port.
*/
extern void bt_port_put_ref(const bt_port *port);

/*!
@brief
    Decrements the reference count of the port
    \bt_p{_port}, and then sets \bt_p{_port} to \c NULL.

@param _port
    @parblock
    Port of which to decrement the reference count.

    Can contain \c NULL.
    @endparblock

@bt_pre_assign_expr{_port}
*/
#define BT_PORT_PUT_REF_AND_RESET(_port)	\
	do {					\
		bt_port_put_ref(_port);		\
		(_port) = NULL;			\
	} while (0)

/*!
@brief
    Decrements the reference count of the port \bt_p{_dst}, sets
    \bt_p{_dst} to \bt_p{_src}, and then sets \bt_p{_src} to \c NULL.

This macro effectively moves a port reference from the
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
#define BT_PORT_MOVE_REF(_dst, _src)	\
	do {				\
		bt_port_put_ref(_dst);	\
		(_dst) = (_src);	\
		(_src) = NULL;		\
	} while (0)

/*! @} */

/*!
@name Input port
@{
*/

/*!
@brief
    \ref api-fund-c-typing "Upcasts" the \bt_iport \bt_p{port} to the
    common #bt_port type.

@param[in] port
    @parblock
    Input port to upcast.

    Can be \c NULL.
    @endparblock

@returns
    \bt_p{port} as a common port.
*/
static inline
const bt_port *bt_port_input_as_port_const(const bt_port_input *port)
{
	return __BT_UPCAST_CONST(bt_port, port);
}

/*!
@brief
    Increments the \ref api-fund-shared-object "reference count" of
    the \bt_iport \bt_p{port}.

@param[in] port
    @parblock
    Input port of which to increment the reference count.

    Can be \c NULL.
    @endparblock

@sa bt_port_input_put_ref() &mdash;
    Decrements the reference count of an input port.
*/
extern void bt_port_input_get_ref(const bt_port_input *port);

/*!
@brief
    Decrements the \ref api-fund-shared-object "reference count" of
    the \bt_iport \bt_p{port}.

@param[in] port
    @parblock
    Input port of which to decrement the reference count.

    Can be \c NULL.
    @endparblock

@sa bt_port_input_get_ref() &mdash;
    Increments the reference count of an input port.
*/
extern void bt_port_input_put_ref(const bt_port_input *port);

/*!
@brief
    Decrements the reference count of the \bt_iport
    \bt_p{_port}, and then sets \bt_p{_port} to \c NULL.

@param _port
    @parblock
    Input port of which to decrement the reference count.

    Can contain \c NULL.
    @endparblock

@bt_pre_assign_expr{_port}
*/
#define BT_PORT_INPUT_PUT_REF_AND_RESET(_port)		\
	do {						\
		bt_port_input_put_ref(_port);		\
		(_port) = NULL;				\
	} while (0)

/*!
@brief
    Decrements the reference count of the \bt_iport \bt_p{_dst}, sets
    \bt_p{_dst} to \bt_p{_src}, and then sets \bt_p{_src} to \c NULL.

This macro effectively moves an \bt_iport reference from the
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
#define BT_PORT_INPUT_MOVE_REF(_dst, _src)	\
	do {					\
		bt_port_input_put_ref(_dst);	\
		(_dst) = (_src);		\
		(_src) = NULL;			\
	} while (0)

/*! @} */

/*!
@name Output port
@{
*/

/*!
@brief
    \ref api-fund-c-typing "Upcasts" the \bt_oport \bt_p{port} to the
    common #bt_port type.

@param[in] port
    @parblock
    Output port to upcast.

    Can be \c NULL.
    @endparblock

@returns
    \bt_p{port} as a common port.
*/
static inline
const bt_port *bt_port_output_as_port_const(const bt_port_output *port)
{
	return __BT_UPCAST_CONST(bt_port, port);
}

/*!
@brief
    Increments the \ref api-fund-shared-object "reference count" of
    the \bt_oport \bt_p{port}.

@param[in] port
    @parblock
    Output port of which to increment the reference count.

    Can be \c NULL.
    @endparblock

@sa bt_port_output_put_ref() &mdash;
    Decrements the reference count of a \bt_oport.
*/
extern void bt_port_output_get_ref(const bt_port_output *port);

/*!
@brief
    Decrements the \ref api-fund-shared-object "reference count" of
    the \bt_oport \bt_p{port}.

@param[in] port
    @parblock
    Output port of which to decrement the reference count.

    Can be \c NULL.
    @endparblock

@sa bt_port_output_get_ref() &mdash;
    Increments the reference count of a \bt_oport.
*/
extern void bt_port_output_put_ref(const bt_port_output *port);

/*!
@brief
    Decrements the reference count of the \bt_oport
    \bt_p{_port}, and then sets \bt_p{_port} to \c NULL.

@param _port
    @parblock
    Output port of which to decrement the reference count.

    Can contain \c NULL.
    @endparblock

@bt_pre_assign_expr{_port}
*/
#define BT_PORT_OUTPUT_PUT_REF_AND_RESET(_port)		\
	do {						\
		bt_port_output_put_ref(_port);		\
		(_port) = NULL;				\
	} while (0)

/*!
@brief
    Decrements the reference count of the \bt_oport \bt_p{_dst}, sets
    \bt_p{_dst} to \bt_p{_src}, and then sets \bt_p{_src} to \c NULL.

This macro effectively moves an \bt_oport reference from the
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
#define BT_PORT_OUTPUT_MOVE_REF(_dst, _src)	\
	do {					\
		bt_port_output_put_ref(_dst);	\
		(_dst) = (_src);		\
		(_src) = NULL;			\
	} while (0)

/*! @} */

/*! @} */

#ifdef __cplusplus
}
#endif

#endif /* BABELTRACE2_GRAPH_PORT_H */
