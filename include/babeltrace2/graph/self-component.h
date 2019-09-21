#ifndef BABELTRACE2_GRAPH_SELF_COMPONENT_H
#define BABELTRACE2_GRAPH_SELF_COMPONENT_H

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
@defgroup api-self-comp Self components
@ingroup api-comp-cls-dev

@brief
    Private views of \bt_p_comp for instance methods.

The #bt_self_component, #bt_self_component_source,
#bt_self_component_filter, #bt_self_component_sink types are
private views of a \bt_comp from within a component class
\ref api-comp-cls-dev-instance-meth "instance method".

Add a \bt_port to a component with
bt_self_component_source_add_output_port(),
bt_self_component_filter_add_input_port(),
bt_self_component_filter_add_output_port(), and
bt_self_component_sink_add_input_port().

When you add a port to a component, you can attach custom user data
to it (\bt_voidp). You can retrieve this user data
afterwards with bt_self_component_port_get_data().

Borrow a \bt_self_comp_port from a component by index with
bt_self_component_source_borrow_output_port_by_index(),
bt_self_component_filter_borrow_input_port_by_index(),
bt_self_component_filter_borrow_output_port_by_index(), and
bt_self_component_sink_borrow_input_port_by_index().

Borrow a \bt_self_comp_port from a component by name with
bt_self_component_source_borrow_output_port_by_name(),
bt_self_component_filter_borrow_input_port_by_name(),
bt_self_component_filter_borrow_output_port_by_name(), and
bt_self_component_sink_borrow_input_port_by_name().

Set and get user data attached to a component with
bt_self_component_set_data() and bt_self_component_get_data().

Get a component's owning trace processing \bt_graph's effective
\bt_mip version with bt_self_component_get_graph_mip_version().

Check whether or not a \bt_sink_comp is interrupted with
bt_self_component_sink_is_interrupted().

\ref api-fund-c-typing "Upcast" the "self" (private) types to the
public and common self component types with the
<code>bt_self_component*_as_component*()</code> and
<code>bt_self_component_*_as_self_component()</code> functions.
*/

/*! @{ */

/*!
@name Types
@{

@typedef struct bt_self_component bt_self_component;

@brief
    Self \bt_comp.

@typedef struct bt_self_component_source bt_self_component_source;

@brief
    Self \bt_src_comp.

@typedef struct bt_self_component_filter bt_self_component_filter;

@brief
    Self \bt_flt_comp.

@typedef struct bt_self_component_sink bt_self_component_sink;

@brief
    Self \bt_sink_comp.

@typedef struct bt_self_component_source_configuration bt_self_component_source_configuration;

@brief
    Self \bt_src_comp configuration.

@typedef struct bt_self_component_filter_configuration bt_self_component_filter_configuration;

@brief
    Self \bt_flt_comp configuration.

@typedef struct bt_self_component_sink_configuration bt_self_component_sink_configuration;

@brief
    Self \bt_sink_comp configuration.

@}
*/

/*!
@name Port adding
@{
*/

/*!
@brief
    Status codes for bt_self_component_source_add_output_port(),
    bt_self_component_filter_add_input_port(),
    bt_self_component_filter_add_output_port(), and
    bt_self_component_sink_add_input_port().
*/
typedef enum bt_self_component_add_port_status {
	/*!
	@brief
	    Success.
	*/
	BT_SELF_COMPONENT_ADD_PORT_STATUS_OK		= __BT_FUNC_STATUS_OK,

	/*!
	@brief
	    Out of memory.
	*/
	BT_SELF_COMPONENT_ADD_PORT_STATUS_MEMORY_ERROR	= __BT_FUNC_STATUS_MEMORY_ERROR,

	/*!
	@brief
	    Other error.
	*/
	BT_SELF_COMPONENT_ADD_PORT_STATUS_ERROR		= __BT_FUNC_STATUS_ERROR,
} bt_self_component_add_port_status;

/*!
@brief
    Adds an \bt_oport named \bt_p{name} and having the user data
    \bt_p{user_data} to the \bt_src_comp \bt_p{self_component},
    and sets \bt_p{*self_component_port} to the resulting port.

@attention
    You can only call this function from within the
    \ref api-comp-cls-dev-meth-init "initialization",
    \link api-comp-cls-dev-meth-iport-connected "input port connected"\endlink,
    and
    \link api-comp-cls-dev-meth-oport-connected "output port connected"\endlink
    methods.

@param[in] self_component
    Source component instance.
@param[in] name
    Name of the output port to add to \bt_p{self_component} (copied).
@param[in] user_data
    User data of the output port to add to \bt_p{self_component}.
@param[out] self_component_port
    <strong>On success, if not \c NULL</strong>,
    \bt_p{*self_component_port} is a \em borrowed reference of the
    created port.

@retval #BT_SELF_COMPONENT_ADD_PORT_STATUS_OK
    Success.
@retval #BT_SELF_COMPONENT_ADD_PORT_STATUS_MEMORY_ERROR
    Out of memory.
@retval #BT_SELF_COMPONENT_ADD_PORT_STATUS_ERROR
    Other error.

@bt_pre_not_null{self_component}
@bt_pre_not_null{name}
@pre
    No other output port within \bt_p{self_component} has the name
    \bt_p{name}.
*/
extern bt_self_component_add_port_status
bt_self_component_source_add_output_port(
		bt_self_component_source *self_component,
		const char *name, void *user_data,
		bt_self_component_port_output **self_component_port);

/*!
@brief
    Adds an \bt_iport named \bt_p{name} and having the user data
    \bt_p{user_data} to the \bt_flt_comp \bt_p{self_component},
    and sets \bt_p{*self_component_port} to the resulting port.

@attention
    You can only call this function from within the
    \ref api-comp-cls-dev-meth-init "initialization",
    \link api-comp-cls-dev-meth-iport-connected "input port connected"\endlink,
    and
    \link api-comp-cls-dev-meth-oport-connected "output port connected"\endlink
    methods.

@param[in] self_component
    Filter component instance.
@param[in] name
    Name of the input port to add to \bt_p{self_component} (copied).
@param[in] user_data
    User data of the input port to add to \bt_p{self_component}.
@param[out] self_component_port
    <strong>On success, if not \c NULL</strong>,
    \bt_p{*self_component_port} is a \em borrowed reference of the
    created port.

@retval #BT_SELF_COMPONENT_ADD_PORT_STATUS_OK
    Success.
@retval #BT_SELF_COMPONENT_ADD_PORT_STATUS_MEMORY_ERROR
    Out of memory.
@retval #BT_SELF_COMPONENT_ADD_PORT_STATUS_ERROR
    Other error.

@bt_pre_not_null{self_component}
@bt_pre_not_null{name}
@pre
    No other input port within \bt_p{self_component} has the name
    \bt_p{name}.
*/
extern bt_self_component_add_port_status
bt_self_component_filter_add_input_port(
		bt_self_component_filter *self_component,
		const char *name, void *user_data,
		bt_self_component_port_input **self_component_port);

/*!
@brief
    Adds an \bt_oport named \bt_p{name} and having the user data
    \bt_p{user_data} to the \bt_flt_comp \bt_p{self_component},
    and sets \bt_p{*self_component_port} to the resulting port.

@attention
    You can only call this function from within the
    \ref api-comp-cls-dev-meth-init "initialization",
    \link api-comp-cls-dev-meth-iport-connected "input port connected"\endlink,
    and
    \link api-comp-cls-dev-meth-oport-connected "output port connected"\endlink
    methods.

@param[in] self_component
    Filter component instance.
@param[in] name
    Name of the output port to add to \bt_p{self_component} (copied).
@param[in] user_data
    User data of the output port to add to \bt_p{self_component}.
@param[out] self_component_port
    <strong>On success, if not \c NULL</strong>,
    \bt_p{*self_component_port} is a \em borrowed reference of the
    created port.

@retval #BT_SELF_COMPONENT_ADD_PORT_STATUS_OK
    Success.
@retval #BT_SELF_COMPONENT_ADD_PORT_STATUS_MEMORY_ERROR
    Out of memory.
@retval #BT_SELF_COMPONENT_ADD_PORT_STATUS_ERROR
    Other error.

@bt_pre_not_null{self_component}
@bt_pre_not_null{name}
@pre
    No other output port within \bt_p{self_component} has the name
    \bt_p{name}.
*/
extern bt_self_component_add_port_status
bt_self_component_filter_add_output_port(
		bt_self_component_filter *self_component,
		const char *name, void *user_data,
		bt_self_component_port_output **self_component_port);

/*!
@brief
    Adds an \bt_iport named \bt_p{name} and having the user data
    \bt_p{user_data} to the \bt_sink_comp \bt_p{self_component},
    and sets \bt_p{*self_component_port} to the resulting port.

@attention
    You can only call this function from within the
    \ref api-comp-cls-dev-meth-init "initialization",
    \link api-comp-cls-dev-meth-iport-connected "input port connected"\endlink,
    and
    \link api-comp-cls-dev-meth-oport-connected "output port connected"\endlink
    methods.

@param[in] self_component
    Sink component instance.
@param[in] name
    Name of the input port to add to \bt_p{self_component} (copied).
@param[in] user_data
    User data of the input port to add to \bt_p{self_component}.
@param[out] self_component_port
    <strong>On success, if not \c NULL</strong>,
    \bt_p{*self_component_port} is a \em borrowed reference of the
    created port.

@retval #BT_SELF_COMPONENT_ADD_PORT_STATUS_OK
    Success.
@retval #BT_SELF_COMPONENT_ADD_PORT_STATUS_MEMORY_ERROR
    Out of memory.
@retval #BT_SELF_COMPONENT_ADD_PORT_STATUS_ERROR
    Other error.

@bt_pre_not_null{self_component}
@bt_pre_not_null{name}
@pre
    No other input port within \bt_p{self_component} has the name
    \bt_p{name}.
*/

extern bt_self_component_add_port_status
bt_self_component_sink_add_input_port(
		bt_self_component_sink *self_component,
		const char *name, void *user_data,
		bt_self_component_port_input **self_component_port);

/*! @} */

/*!
@name Port access
@{
*/

/*!
@brief
    Borrows the \bt_self_comp_oport at index \bt_p{index} from the
    \bt_src_comp \bt_p{self_component}.

@param[in] self_component
    Source component instance.
@param[in] index
    Index of the output port to borrow from \bt_p{self_component}.

@returns
    @parblock
    \em Borrowed reference of the output port of
    \bt_p{self_component} at index \bt_p{index}.

    The returned pointer remains valid as long as \bt_p{self_component}
    exists.
    @endparblock

@bt_pre_not_null{self_component}
@pre
    \bt_p{index} is less than the number of output ports
    \bt_p{self_component} has (as returned by
    bt_component_source_get_output_port_count()).

@sa bt_component_source_get_output_port_count() &mdash;
    Returns the number of output ports that a source component has.
*/
extern bt_self_component_port_output *
bt_self_component_source_borrow_output_port_by_index(
		bt_self_component_source *self_component,
		uint64_t index);

/*!
@brief
    Borrows the \bt_self_comp_iport at index \bt_p{index} from the
    \bt_flt_comp \bt_p{self_component}.

@param[in] self_component
    Filter component instance.
@param[in] index
    Index of the input port to borrow from \bt_p{self_component}.

@returns
    @parblock
    \em Borrowed reference of the input port of
    \bt_p{self_component} at index \bt_p{index}.

    The returned pointer remains valid as long as \bt_p{self_component}
    exists.
    @endparblock

@bt_pre_not_null{self_component}
@pre
    \bt_p{index} is less than the number of input ports
    \bt_p{self_component} has (as returned by
    bt_component_filter_get_input_port_count()).

@sa bt_component_filter_get_input_port_count() &mdash;
    Returns the number of input ports that a filter component has.
*/
extern bt_self_component_port_input *
bt_self_component_filter_borrow_input_port_by_index(
		bt_self_component_filter *self_component,
		uint64_t index);

/*!
@brief
    Borrows the \bt_self_comp_oport at index \bt_p{index} from the
    \bt_flt_comp \bt_p{self_component}.

@param[in] self_component
    Filter component instance.
@param[in] index
    Index of the output port to borrow from \bt_p{self_component}.

@returns
    @parblock
    \em Borrowed reference of the output port of
    \bt_p{self_component} at index \bt_p{index}.

    The returned pointer remains valid as long as \bt_p{self_component}
    exists.
    @endparblock

@bt_pre_not_null{self_component}
@pre
    \bt_p{index} is less than the number of output ports
    \bt_p{self_component} has (as returned by
    bt_component_filter_get_output_port_count()).

@sa bt_component_filter_get_output_port_count() &mdash;
    Returns the number of output ports that a filter component has.
*/
extern bt_self_component_port_output *
bt_self_component_filter_borrow_output_port_by_index(
		bt_self_component_filter *self_component,
		uint64_t index);

/*!
@brief
    Borrows the \bt_self_comp_iport at index \bt_p{index} from the
    \bt_sink_comp \bt_p{self_component}.

@param[in] self_component
    Sink component instance.
@param[in] index
    Index of the input port to borrow from \bt_p{self_component}.

@returns
    @parblock
    \em Borrowed reference of the input port of
    \bt_p{self_component} at index \bt_p{index}.

    The returned pointer remains valid as long as \bt_p{self_component}
    exists.
    @endparblock

@bt_pre_not_null{self_component}
@pre
    \bt_p{index} is less than the number of input ports
    \bt_p{self_component} has (as returned by
    bt_component_sink_get_input_port_count()).

@sa bt_component_sink_get_input_port_count() &mdash;
    Returns the number of input ports that a sink component has.
*/
extern bt_self_component_port_input *
bt_self_component_sink_borrow_input_port_by_index(
		bt_self_component_sink *self_component, uint64_t index);

/*!
@brief
    Borrows the \bt_self_comp_oport named \bt_p{name} from the
    \bt_src_comp \bt_p{self_component}.

If \bt_p{self_component} has no output port named \bt_p{name}, this
function returns \c NULL.

@param[in] self_component
    Source component instance.
@param[in] name
    Name of the output port to borrow from \bt_p{self_component}.

@returns
    @parblock
    \em Borrowed reference of the output port of
    \bt_p{self_component} named \bt_p{name}, or \c NULL if none.

    The returned pointer remains valid as long as \bt_p{self_component}
    exists.
    @endparblock

@bt_pre_not_null{self_component}
@bt_pre_not_null{name}
*/
extern bt_self_component_port_output *
bt_self_component_source_borrow_output_port_by_name(
		bt_self_component_source *self_component,
		const char *name);

/*!
@brief
    Borrows the \bt_self_comp_iport named \bt_p{name} from the
    \bt_flt_comp \bt_p{self_component}.

If \bt_p{self_component} has no input port named \bt_p{name}, this
function returns \c NULL.

@param[in] self_component
    Filter component instance.
@param[in] name
    Name of the input port to borrow from \bt_p{self_component}.

@returns
    @parblock
    \em Borrowed reference of the input port of
    \bt_p{self_component} named \bt_p{name}, or \c NULL if none.

    The returned pointer remains valid as long as \bt_p{self_component}
    exists.
    @endparblock

@bt_pre_not_null{self_component}
@bt_pre_not_null{name}
*/
extern bt_self_component_port_input *
bt_self_component_filter_borrow_input_port_by_name(
		bt_self_component_filter *self_component,
		const char *name);

/*!
@brief
    Borrows the \bt_self_comp_oport named \bt_p{name} from the
    \bt_flt_comp \bt_p{self_component}.

If \bt_p{self_component} has no output port named \bt_p{name}, this
function returns \c NULL.

@param[in] self_component
    Filter component instance.
@param[in] name
    Name of the output port to borrow from \bt_p{self_component}.

@returns
    @parblock
    \em Borrowed reference of the output port of
    \bt_p{self_component} named \bt_p{name}, or \c NULL if none.

    The returned pointer remains valid as long as \bt_p{self_component}
    exists.
    @endparblock

@bt_pre_not_null{self_component}
@bt_pre_not_null{name}
*/
extern bt_self_component_port_output *
bt_self_component_filter_borrow_output_port_by_name(
		bt_self_component_filter *self_component,
		const char *name);

/*!
@brief
    Borrows the \bt_self_comp_iport named \bt_p{name} from the
    \bt_sink_comp \bt_p{self_component}.

If \bt_p{self_component} has no input port named \bt_p{name}, this
function returns \c NULL.

@param[in] self_component
    Sink component instance.
@param[in] name
    Name of the input port to borrow from \bt_p{self_component}.

@returns
    @parblock
    \em Borrowed reference of the input port of
    \bt_p{self_component} named \bt_p{name}, or \c NULL if none.

    The returned pointer remains valid as long as \bt_p{self_component}
    exists.
    @endparblock

@bt_pre_not_null{self_component}
@bt_pre_not_null{name}
*/
extern bt_self_component_port_input *
bt_self_component_sink_borrow_input_port_by_name(
		bt_self_component_sink *self_component,
		const char *name);

/*! @} */

/*!
@name User data
@{
*/

/*!
@brief
    Sets the user data of the \bt_comp \bt_p{self_component} to
    \bt_p{data}.

@param[in] self_component
    Component instance.
@param[in] user_data
    New user data of \bt_p{self_component}.

@bt_pre_not_null{self_component}

@sa bt_self_component_get_data() &mdash;
    Returns the user data of a component.
*/
extern void bt_self_component_set_data(
		bt_self_component *self_component, void *user_data);

/*!
@brief
    Returns the user data of the \bt_comp \bt_p{self_component}.

@param[in] self_component
    Component instance.

@returns
    User data of \bt_p{self_component}.

@bt_pre_not_null{self_component}

@sa bt_self_component_set_data() &mdash;
    Sets the user data of a component.
*/
extern void *bt_self_component_get_data(
		const bt_self_component *self_component);

/*! @} */

/*!
@name Trace processing graph's effective MIP version access
@{
*/

/*!
@brief
    Returns the effective \bt_mip (MIP) version of the trace processing
    \bt_graph which contains the \bt_comp \bt_p{self_component}.

@note
    As of \bt_name_version_min_maj, because bt_get_maximal_mip_version()
    returns 0, this function always returns 0.

@param[in] self_component
    Component instance.

@returns
    Effective MIP version of the trace processing graph which
    contains \bt_p{self_component}.

@bt_pre_not_null{self_component}
*/
extern
uint64_t bt_self_component_get_graph_mip_version(
		bt_self_component *self_component);

/*! @} */

/*!
@name Sink component's interruption query
@{
*/

/*!
@brief
    Returns whether or not the \bt_sink_comp \bt_p{self_component}
    is interrupted, that is, whether or not any of its \bt_p_intr
    is set.

@param[in] self_component
    Component instance.

@returns
    #BT_TRUE if \bt_p{self_component} is interrupted (any of its
    interrupters is set).

@bt_pre_not_null{self_component}

@sa bt_graph_borrow_default_interrupter() &mdash;
    Borrows a trace processing graph's default interrupter.
@sa bt_graph_add_interrupter() &mdash;
    Adds an interrupter to a graph.
*/
extern bt_bool bt_self_component_sink_is_interrupted(
		const bt_self_component_sink *self_component);

/*! @} */

/*!
@name Self to public upcast
@{
*/

/*!
@brief
    \ref api-fund-c-typing "Upcasts" the self \bt_comp
    \bt_p{self_component} to the public #bt_component type.

@param[in] self_component
    @parblock
    Component to upcast.

    Can be \c NULL.
    @endparblock

@returns
    \bt_p{self_component} as a public component.
*/
static inline
const bt_component *bt_self_component_as_component(
		bt_self_component *self_component)
{
	return __BT_UPCAST(bt_component, self_component);
}

/*!
@brief
    \ref api-fund-c-typing "Upcasts" the self \bt_src_comp
    \bt_p{self_component} to the public #bt_component_source
    type.

@param[in] self_component
    @parblock
    Source component to upcast.

    Can be \c NULL.
    @endparblock

@returns
    \bt_p{self_component} as a public source component.
*/
static inline
const bt_component_source *
bt_self_component_source_as_component_source(
		bt_self_component_source *self_component)
{
	return __BT_UPCAST_CONST(bt_component_source, self_component);
}

/*!
@brief
    \ref api-fund-c-typing "Upcasts" the self \bt_flt_comp
    \bt_p{self_component} to the public #bt_component_filter
    type.

@param[in] self_component
    @parblock
    Filter component to upcast.

    Can be \c NULL.
    @endparblock

@returns
    \bt_p{self_component} as a public filter component.
*/
static inline
const bt_component_filter *
bt_self_component_filter_as_component_filter(
		bt_self_component_filter *self_component)
{
	return __BT_UPCAST_CONST(bt_component_filter, self_component);
}

/*!
@brief
    \ref api-fund-c-typing "Upcasts" the self \bt_sink_comp
    \bt_p{self_component} to the public #bt_component_sink
    type.

@param[in] self_component
    @parblock
    Sink component to upcast.

    Can be \c NULL.
    @endparblock

@returns
    \bt_p{self_component} as a public sink component.
*/
static inline
const bt_component_sink *
bt_self_component_sink_as_component_sink(
		bt_self_component_sink *self_component)
{
	return __BT_UPCAST_CONST(bt_component_sink, self_component);
}

/*! @} */

/*!
@name Self to common self upcast
@{
*/

/*!
@brief
    \ref api-fund-c-typing "Upcasts" the self \bt_src_comp
    \bt_p{self_component} to the common #bt_self_component
    type.

@param[in] self_component
    @parblock
    Source component to upcast.

    Can be \c NULL.
    @endparblock

@returns
    \bt_p{self_component} as a common self component.
*/
static inline
bt_self_component *bt_self_component_source_as_self_component(
		bt_self_component_source *self_component)
{
	return __BT_UPCAST(bt_self_component, self_component);
}

/*!
@brief
    \ref api-fund-c-typing "Upcasts" the self \bt_flt_comp
    \bt_p{self_component} to the common #bt_self_component
    type.

@param[in] self_component
    @parblock
    Filter component to upcast.

    Can be \c NULL.
    @endparblock

@returns
    \bt_p{self_component} as a common self component.
*/
static inline
bt_self_component *bt_self_component_filter_as_self_component(
		bt_self_component_filter *self_component)
{
	return __BT_UPCAST(bt_self_component, self_component);
}

/*!
@brief
    \ref api-fund-c-typing "Upcasts" the self \bt_sink_comp
    \bt_p{self_component} to the common #bt_self_component
    type.

@param[in] self_component
    @parblock
    Sink component to upcast.

    Can be \c NULL.
    @endparblock

@returns
    \bt_p{self_component} as a common self component.
*/
static inline
bt_self_component *bt_self_component_sink_as_self_component(
		bt_self_component_sink *self_component)
{
	return __BT_UPCAST(bt_self_component, self_component);
}

/*! @} */

/*! @} */

#ifdef __cplusplus
}
#endif

#endif /* BABELTRACE2_GRAPH_SELF_COMPONENT_H */
