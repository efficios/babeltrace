#ifndef BABELTRACE2_GRAPH_COMPONENT_H
#define BABELTRACE2_GRAPH_COMPONENT_H

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

#include <babeltrace2/graph/component-class.h>
#include <babeltrace2/types.h>
#include <babeltrace2/logging.h>

#ifdef __cplusplus
extern "C" {
#endif

/*!
@defgroup api-comp Components
@ingroup api-graph

@brief
    Source, filter, and sink components: nodes in a trace processing
    \bt_graph.

A <strong><em>component</em></strong> is a node within a trace
processing \bt_graph:

@image html component.png

A component is \bt_comp_cls instance. Borrow the class of a
component with bt_component_borrow_class_const(),
bt_component_source_borrow_class_const(),
bt_component_filter_borrow_class_const(), and
bt_component_sink_borrow_class_const().

A component is a \ref api-fund-shared-object "shared object": get a new
reference with bt_component_get_ref() and put an existing reference with
bt_component_put_ref().

The common C&nbsp;type of a port is #bt_component.

There are three types of components, which come from the three types
of component classes:

<dl>
  <dt>\anchor api-comp-src Source component</dt>
  <dd>
    A source component's \bt_msg_iter emits fresh \bt_p_msg.

    A source component's specific type is #bt_component_source and its
    class's type enumerator is #BT_COMPONENT_CLASS_TYPE_SOURCE.

    \ref api-fund-c-typing "Upcast" the #bt_component_source type to the
    #bt_component type with bt_component_source_as_component_const().

    Get a new source component reference with
    bt_component_source_get_ref() and put an existing one with
    bt_component_source_put_ref().

    A source component has \bt_p_oport only.

    Get the number of output ports a source component has with
    bt_component_source_get_output_port_count().

    Borrow a source component's output port by index with
    bt_component_source_borrow_output_port_by_index_const() or by name
    with bt_component_source_borrow_output_port_by_name_const().
  </dd>

  <dt>\anchor api-comp-flt Filter component</dt>
  <dd>
    A filter component's message iterator emits fresh and transformed
    messages. It can also discard existing messages.

    A filter component's specific type is #bt_component_filter and its
    class's type enumerator is #BT_COMPONENT_CLASS_TYPE_FILTER.

    \ref api-fund-c-typing "Upcast" the #bt_component_filter type to the
    #bt_component type with bt_component_filter_as_component_const().

    Get a new filter component reference with
    bt_component_filter_get_ref() and put an existing one with
    bt_component_filter_put_ref().

    A filter component has \bt_p_iport and \bt_p_oport.

    Get the number of output ports a filter component has with
    bt_component_filter_get_output_port_count().

    Borrow a filter component's output port by index with
    bt_component_filter_borrow_output_port_by_index_const() or by name
    with bt_component_filter_borrow_output_port_by_name_const().

    Get the number of input ports a filter component has with
    bt_component_filter_get_input_port_count().

    Borrow a filter component's input port by index with
    bt_component_filter_borrow_input_port_by_index_const() or by name
    with bt_component_filter_borrow_input_port_by_name_const().
  </dd>

  <dt>\anchor api-comp-sink Sink component</dt>
  <dd>
    A sink component consumes messages from a source or filter message
    iterator.

    A filter component's specific type is #bt_component_sink and its
    class's type enumerator is #BT_COMPONENT_CLASS_TYPE_SINK.

    \ref api-fund-c-typing "Upcast" the #bt_component_sink type to the
    #bt_component type with bt_component_sink_as_component_const().

    Get a new sink component reference with bt_component_sink_get_ref()
    and put an existing one with bt_component_sink_put_ref().

    A sink component has \bt_p_iport only.

    Get the number of input ports a sink component has with
    bt_component_sink_get_input_port_count().

    Borrow a sink component's input port by index with
    bt_component_sink_borrow_input_port_by_index_const() or by name
    with bt_component_sink_borrow_input_port_by_name_const().
  </dd>
</dl>

Get a component's class type enumerator with
bt_component_get_class_type(). You can also use the
bt_component_is_source(), bt_component_is_filter(), and
bt_component_is_sink() helper functions.

You cannot directly create a component: there are no
<code>bt_component_*_create()</code> functions. A trace processing
\bt_graph creates a component from a \bt_comp_cls when you call one of
the <code>bt_graph_add_*_component*()</code> functions. Those functions
also return a borrowed reference of the created component through their
\bt_p{component} parameter.

<h1>Properties</h1>

A component has the following common properties:

<dl>
  <dt>
    \anchor api-comp-prop-name
    Name
  </dt>
  <dd>
    Name of the component.

    Each component has a unique name within a given trace processing
    \bt_graph.

    A component's name is set when you
    \ref api-graph-lc-add "add it to a graph" with one of the
    <code>bt_graph_add_*_component*()</code> functions (\bt_p{name}
    parameter); you cannot change it afterwards.

    Get a component's name with bt_component_get_name().
  </dd>

  <dt>
    \anchor api-comp-prop-log-lvl
    Logging level
  </dt>
  <dd>
    Logging level of the component (and its message iterators, if any).

    A component's logging level is set when you
    \ref api-graph-lc-add "add it to a trace processing graph" with one
    of the <code>bt_graph_add_*_component*()</code> functions
    (\bt_p{logging_level} parameter); as of
    \bt_name_version_min_maj, you cannot change it afterwards.

    Get a component's logging level with
    bt_component_get_logging_level().
  </dd>
</dl>
*/

/*! @{ */

/*!
@name Types
@{

@typedef struct bt_component bt_component;

@brief
    Component.

@typedef struct bt_component_source bt_component_source;

@brief
    \bt_c_src_comp.

@typedef struct bt_component_filter bt_component_filter;

@brief
    \bt_c_flt_comp.

@typedef struct bt_component_sink bt_component_sink;

@brief
    \bt_c_sink_comp.

@}
*/

/*!
@name Class type query
@{
*/

/*!
@brief
    Returns the type enumerator of the \ref api-comp-cls "class" of
    the component \bt_p{component}.

@param[in] component
    Component of which to get the class's type enumerator.

@returns
    Type enumerator of the class of \bt_p{component}.

@bt_pre_not_null{component}

@sa bt_component_is_source() &mdash;
    Returns whether or not a component is a \bt_src_comp.
@sa bt_component_is_filter() &mdash;
    Returns whether or not a component is a \bt_flt_comp.
@sa bt_component_is_sink() &mdash;
    Returns whether or not a component is a \bt_sink_comp.
*/
extern bt_component_class_type bt_component_get_class_type(
		const bt_component *component);

/*!
@brief
    Returns whether or not the component \bt_p{component} is a
    \bt_src_comp.

@param[in] component
    Component to check.

@returns
    #BT_TRUE if \bt_p{component} is a source component.

@bt_pre_not_null{component}

@sa bt_component_get_class_type() &mdash;
    Returns the type enumerator of a component's class.
*/
static inline
bt_bool bt_component_is_source(const bt_component *component)
{
	return bt_component_get_class_type(component) ==
		BT_COMPONENT_CLASS_TYPE_SOURCE;
}

/*!
@brief
    Returns whether or not the component \bt_p{component} is a
    \bt_flt_comp.

@param[in] component
    Component to check.

@returns
    #BT_TRUE if \bt_p{component} is a filter component.

@bt_pre_not_null{component}

@sa bt_component_get_class_type() &mdash;
    Returns the type enumerator of a component's class.
*/
static inline
bt_bool bt_component_is_filter(const bt_component *component)
{
	return bt_component_get_class_type(component) ==
		BT_COMPONENT_CLASS_TYPE_FILTER;
}

/*!
@brief
    Returns whether or not the component \bt_p{component} is a
    \bt_sink_comp.

@param[in] component
    Component to check.

@returns
    #BT_TRUE if \bt_p{component} is a sink component.

@bt_pre_not_null{component}

@sa bt_component_get_class_type() &mdash;
    Returns the type enumerator of a component's class.
*/
static inline
bt_bool bt_component_is_sink(const bt_component *component)
{
	return bt_component_get_class_type(component) ==
		BT_COMPONENT_CLASS_TYPE_SINK;
}

/*! @} */

/*!
@name Common class access
@{
*/

/*!
@brief
    Borrows the \ref api-comp-cls "class" of the component
    \bt_p{component}.

@param[in] component
    Component of which to borrow the class.

@returns
    \em Borrowed reference of the class of \bt_p{component}.

@bt_pre_not_null{component}
*/
extern const bt_component_class *bt_component_borrow_class_const(
		const bt_component *component);

/*! @} */

/*!
@name Common properties
@{
*/

/*!
@brief
    Returns the name of the component \bt_p{component}.

See the \ref api-comp-prop-name "name" property.

@param[in] component
    Component of which to get the name.

@returns
    @parblock
    Name of \bt_p{component}.

    The returned pointer remains valid as long as \bt_p{component}
    exists.
    @endparblock

@bt_pre_not_null{component}
*/
extern const char *bt_component_get_name(const bt_component *component);

/*!
@brief
    Returns the logging level of the component \bt_p{component} and its
    \bt_p_msg_iter, if any.

See the \ref api-comp-prop-log-lvl "logging level" property.

@param[in] component
    Component of which to get the logging level.

@returns
    Logging level of \bt_p{component}.

@bt_pre_not_null{component}
*/
extern bt_logging_level bt_component_get_logging_level(
		const bt_component *component);

/*! @} */

/*!
@name Common reference count
@{
*/

/*!
@brief
    Increments the \ref api-fund-shared-object "reference count" of
    the component \bt_p{component}.

@param[in] component
    @parblock
    Component of which to increment the reference count.

    Can be \c NULL.
    @endparblock

@sa bt_component_put_ref() &mdash;
    Decrements the reference count of a component.
*/
extern void bt_component_get_ref(const bt_component *component);

/*!
@brief
    Decrements the \ref api-fund-shared-object "reference count" of
    the component \bt_p{component}.

@param[in] component
    @parblock
    Component of which to decrement the reference count.

    Can be \c NULL.
    @endparblock

@sa bt_component_get_ref() &mdash;
    Increments the reference count of a component.
*/
extern void bt_component_put_ref(const bt_component *component);

/*!
@brief
    Decrements the reference count of the component
    \bt_p{_component}, and then sets \bt_p{_component} to \c NULL.

@param _component
    @parblock
    Component of which to decrement the reference count.

    Can contain \c NULL.
    @endparblock

@bt_pre_assign_expr{_component}
*/
#define BT_COMPONENT_PUT_REF_AND_RESET(_component)	\
	do {						\
		bt_component_put_ref(_component);	\
		(_component) = NULL;			\
	} while (0)

/*!
@brief
    Decrements the reference count of the component \bt_p{_dst}, sets
    \bt_p{_dst} to \bt_p{_src}, and then sets \bt_p{_src} to \c NULL.

This macro effectively moves a component reference from the expression
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
#define BT_COMPONENT_MOVE_REF(_dst, _src)	\
	do {					\
		bt_component_put_ref(_dst);	\
		(_dst) = (_src);		\
		(_src) = NULL;			\
	} while (0)

/*! @} */

/*!
@name Source component class access
@{
*/

/*!
@brief
    Borrows the \ref api-comp-cls "class" of the \bt_src_comp
    \bt_p{component}.

@param[in] component
    Source component of which to borrow the class.

@returns
    \em Borrowed reference of the class of \bt_p{component}.

@bt_pre_not_null{component}
*/
extern const bt_component_class_source *
bt_component_source_borrow_class_const(
		const bt_component_source *component);

/*! @} */

/*!
@name Source component upcast
@{
*/

/*!
@brief
    \ref api-fund-c-typing "Upcasts" the \bt_src_comp \bt_p{component}
    to the common #bt_component type.

@param[in] component
    @parblock
    Source component to upcast.

    Can be \c NULL.
    @endparblock

@returns
    \bt_p{component} as a common component.
*/
static inline
const bt_component *bt_component_source_as_component_const(
		const bt_component_source *component)
{
	return __BT_UPCAST_CONST(bt_component, component);
}

/*! @} */

/*!
@name Source component port access
@{
*/

/*!
@brief
    Returns the number of \bt_p_oport that the \bt_src_comp
    \bt_p{component} has.

@param[in] component
    Source component of which to get the number of output ports.

@returns
    Number of output ports that \bt_p{component} has.

@bt_pre_not_null{component}
*/
extern uint64_t bt_component_source_get_output_port_count(
		const bt_component_source *component);

/*!
@brief
    Borrows the \bt_oport at index \bt_p{index} from the
    \bt_src_comp \bt_p{component}.

@param[in] component
    Source component from which to borrow the output port at index
    \bt_p{index}.
@param[in] index
    Index of the output port to borrow from \bt_p{component}.

@returns
    @parblock
    \em Borrowed reference of the output port of
    \bt_p{component} at index \bt_p{index}.

    The returned pointer remains valid as long as \bt_p{component}
    exists.
    @endparblock

@bt_pre_not_null{component}
@pre
    \bt_p{index} is less than the number of output ports
    \bt_p{component} has (as returned by
    bt_component_source_get_output_port_count()).

@sa bt_component_source_get_output_port_count() &mdash;
    Returns the number of output ports that a source component has.
*/
extern const bt_port_output *
bt_component_source_borrow_output_port_by_index_const(
		const bt_component_source *component, uint64_t index);

/*!
@brief
    Borrows the \bt_oport named \bt_p{name} from the \bt_src_comp
    \bt_p{component}.

If \bt_p{component} has no output port named \bt_p{name}, this function
returns \c NULL.

@param[in] component
    Source component from which to borrow the output port
    named \bt_p{name}.
@param[in] name
    Name of the output port to borrow from \bt_p{component}.

@returns
    @parblock
    \em Borrowed reference of the output port of
    \bt_p{component} named \bt_p{name}, or \c NULL if none.

    The returned pointer remains valid as long as \bt_p{component}
    exists.
    @endparblock

@bt_pre_not_null{component}
@bt_pre_not_null{name}
*/
extern const bt_port_output *
bt_component_source_borrow_output_port_by_name_const(
		const bt_component_source *component, const char *name);

/*! @} */

/*!
@name Source component reference count
@{
*/

/*!
@brief
    Increments the \ref api-fund-shared-object "reference count" of
    the \bt_src_comp \bt_p{component}.

@param[in] component
    @parblock
    Source component of which to increment the reference count.

    Can be \c NULL.
    @endparblock

@sa bt_component_source_put_ref() &mdash;
    Decrements the reference count of a source component.
*/
extern void bt_component_source_get_ref(
		const bt_component_source *component);

/*!
@brief
    Decrements the \ref api-fund-shared-object "reference count" of
    the \bt_src_comp \bt_p{component}.

@param[in] component
    @parblock
    Source component of which to decrement the reference count.

    Can be \c NULL.
    @endparblock

@sa bt_component_source_get_ref() &mdash;
    Increments the reference count of a source component.
*/
extern void bt_component_source_put_ref(
		const bt_component_source *component);

/*!
@brief
    Decrements the reference count of the \bt_src_comp
    \bt_p{_component}, and then sets \bt_p{_component} to \c NULL.

@param _component
    @parblock
    Source component of which to decrement the reference count.

    Can contain \c NULL.
    @endparblock

@bt_pre_assign_expr{_component}
*/
#define BT_COMPONENT_SOURCE_PUT_REF_AND_RESET(_component)	\
	do {							\
		bt_component_source_put_ref(_component);	\
		(_component) = NULL;				\
	} while (0)

/*!
@brief
    Decrements the reference count of the \bt_src_comp \bt_p{_dst}, sets
    \bt_p{_dst} to \bt_p{_src}, and then sets \bt_p{_src} to \c NULL.

This macro effectively moves a source component reference from the
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
#define BT_COMPONENT_SOURCE_MOVE_REF(_dst, _src)	\
	do {						\
		bt_component_source_put_ref(_dst);	\
		(_dst) = (_src);			\
		(_src) = NULL;				\
	} while (0)

/*! @} */

/*!
@name Filter component class access
@{
*/

/*!
@brief
    Borrows the \ref api-comp-cls "class" of the \bt_flt_comp
    \bt_p{component}.

@param[in] component
    Filter component of which to borrow the class.

@returns
    \em Borrowed reference of the class of \bt_p{component}.

@bt_pre_not_null{component}
*/
extern const bt_component_class_filter *
bt_component_filter_borrow_class_const(
		const bt_component_filter *component);

/*! @} */

/*!
@name Filter component upcast
@{
*/

/*!
@brief
    \ref api-fund-c-typing "Upcasts" the \bt_flt_comp \bt_p{component}
    to the common #bt_component type.

@param[in] component
    @parblock
    Filter component to upcast.

    Can be \c NULL.
    @endparblock

@returns
    \bt_p{component} as a common component.
*/
static inline
const bt_component *bt_component_filter_as_component_const(
		const bt_component_filter *component)
{
	return __BT_UPCAST_CONST(bt_component, component);
}

/*! @} */

/*!
@name Filter component port access
@{
*/

/*!
@brief
    Returns the number of \bt_p_iport that the \bt_flt_comp
    \bt_p{component} has.

@param[in] component
    Filter component of which to get the number of input ports.

@returns
    Number of input ports that \bt_p{component} has.

@bt_pre_not_null{component}
*/
extern uint64_t bt_component_filter_get_input_port_count(
		const bt_component_filter *component);

/*!
@brief
    Borrows the \bt_iport at index \bt_p{index} from the
    \bt_flt_comp \bt_p{component}.

@param[in] component
    Filter component from which to borrow the input port at index
    \bt_p{index}.
@param[in] index
    Index of the input port to borrow from \bt_p{component}.

@returns
    @parblock
    \em Borrowed reference of the input port of
    \bt_p{component} at index \bt_p{index}.

    The returned pointer remains valid as long as \bt_p{component}
    exists.
    @endparblock

@bt_pre_not_null{component}
@pre
    \bt_p{index} is less than the number of input ports
    \bt_p{component} has (as returned by
    bt_component_filter_get_input_port_count()).

@sa bt_component_filter_get_input_port_count() &mdash;
    Returns the number of input ports that a filter component has.
*/
extern const bt_port_input *
bt_component_filter_borrow_input_port_by_index_const(
		const bt_component_filter *component, uint64_t index);

/*!
@brief
    Borrows the \bt_iport named \bt_p{name} from the \bt_flt_comp
    \bt_p{component}.

If \bt_p{component} has no input port named \bt_p{name}, this function
returns \c NULL.

@param[in] component
    Filter component from which to borrow the input port
    named \bt_p{name}.
@param[in] name
    Name of the input port to borrow from \bt_p{component}.

@returns
    @parblock
    \em Borrowed reference of the input port of
    \bt_p{component} named \bt_p{name}, or \c NULL if none.

    The returned pointer remains valid as long as \bt_p{component}
    exists.
    @endparblock

@bt_pre_not_null{component}
@bt_pre_not_null{name}
*/
extern const bt_port_input *
bt_component_filter_borrow_input_port_by_name_const(
		const bt_component_filter *component, const char *name);

/*!
@brief
    Returns the number of \bt_p_oport that the \bt_flt_comp
    \bt_p{component} has.

@param[in] component
    Filter component of which to get the number of output ports.

@returns
    Number of output ports that \bt_p{component} has.

@bt_pre_not_null{component}
*/
extern uint64_t bt_component_filter_get_output_port_count(
		const bt_component_filter *component);

/*!
@brief
    Borrows the \bt_oport at index \bt_p{index} from the
    \bt_flt_comp \bt_p{component}.

@param[in] component
    Filter component from which to borrow the output port at index
    \bt_p{index}.
@param[in] index
    Index of the output port to borrow from \bt_p{component}.

@returns
    @parblock
    \em Borrowed reference of the output port of
    \bt_p{component} at index \bt_p{index}.

    The returned pointer remains valid as long as \bt_p{component}
    exists.
    @endparblock

@bt_pre_not_null{component}
@pre
    \bt_p{index} is less than the number of output ports
    \bt_p{component} has (as returned by
    bt_component_filter_get_output_port_count()).

@sa bt_component_filter_get_output_port_count() &mdash;
    Returns the number of output ports that a filter component has.
*/
extern const bt_port_output *
bt_component_filter_borrow_output_port_by_index_const(
		const bt_component_filter *component, uint64_t index);

/*!
@brief
    Borrows the \bt_oport named \bt_p{name} from the \bt_flt_comp
    \bt_p{component}.

If \bt_p{component} has no output port named \bt_p{name}, this function
returns \c NULL.

@param[in] component
    Filter component from which to borrow the output port
    named \bt_p{name}.
@param[in] name
    Name of the output port to borrow from \bt_p{component}.

@returns
    @parblock
    \em Borrowed reference of the output port of
    \bt_p{component} named \bt_p{name}, or \c NULL if none.

    The returned pointer remains valid as long as \bt_p{component}
    exists.
    @endparblock

@bt_pre_not_null{component}
@bt_pre_not_null{name}
*/
extern const bt_port_output *
bt_component_filter_borrow_output_port_by_name_const(
		const bt_component_filter *component, const char *name);

/*! @} */

/*!
@name Filter component reference count
@{
*/

/*!
@brief
    Increments the \ref api-fund-shared-object "reference count" of
    the \bt_flt_comp \bt_p{component}.

@param[in] component
    @parblock
    Filter component of which to increment the reference count.

    Can be \c NULL.
    @endparblock

@sa bt_component_filter_put_ref() &mdash;
    Decrements the reference count of a filter component.
*/
extern void bt_component_filter_get_ref(
		const bt_component_filter *component);

/*!
@brief
    Decrements the \ref api-fund-shared-object "reference count" of
    the \bt_flt_comp \bt_p{component}.

@param[in] component
    @parblock
    Filter component of which to decrement the reference count.

    Can be \c NULL.
    @endparblock

@sa bt_component_filter_get_ref() &mdash;
    Increments the reference count of a filter component.
*/
extern void bt_component_filter_put_ref(
		const bt_component_filter *component);

/*!
@brief
    Decrements the reference count of the \bt_flt_comp
    \bt_p{_component}, and then sets \bt_p{_component} to \c NULL.

@param _component
    @parblock
    Filter component of which to decrement the reference count.

    Can contain \c NULL.
    @endparblock

@bt_pre_assign_expr{_component}
*/
#define BT_COMPONENT_FILTER_PUT_REF_AND_RESET(_component)	\
	do {							\
		bt_component_filter_put_ref(_component);	\
		(_component) = NULL;				\
	} while (0)

/*!
@brief
    Decrements the reference count of the \bt_flt_comp \bt_p{_dst}, sets
    \bt_p{_dst} to \bt_p{_src}, and then sets \bt_p{_src} to \c NULL.

This macro effectively moves a filter component reference from the
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
#define BT_COMPONENT_FILTER_MOVE_REF(_dst, _src)	\
	do {						\
		bt_component_filter_put_ref(_dst);	\
		(_dst) = (_src);			\
		(_src) = NULL;				\
	} while (0)

/*! @} */

/*!
@name Sink component class access
@{
*/

/*!
@brief
    Borrows the \ref api-comp-cls "class" of the \bt_sink_comp
    \bt_p{component}.

@param[in] component
    Sink component of which to borrow the class.

@returns
    \em Borrowed reference of the class of \bt_p{component}.

@bt_pre_not_null{component}
*/
extern const bt_component_class_sink *
bt_component_sink_borrow_class_const(
		const bt_component_sink *component);

/*! @} */

/*!
@name Sink component upcast
@{
*/

/*!
@brief
    \ref api-fund-c-typing "Upcasts" the \bt_sink_comp \bt_p{component}
    to the common #bt_component type.

@param[in] component
    @parblock
    Sink component to upcast.

    Can be \c NULL.
    @endparblock

@returns
    \bt_p{component} as a common component.
*/
static inline
const bt_component *bt_component_sink_as_component_const(
		const bt_component_sink *component)
{
	return __BT_UPCAST_CONST(bt_component, component);
}

/*! @} */

/*!
@name Sink component port access
@{
*/

/*!
@brief
    Returns the number of \bt_p_iport that the \bt_sink_comp
    \bt_p{component} has.

@param[in] component
    Sink component of which to get the number of input ports.

@returns
    Number of input ports that \bt_p{component} has.

@bt_pre_not_null{component}
*/
extern uint64_t bt_component_sink_get_input_port_count(
		const bt_component_sink *component);

/*!
@brief
    Borrows the \bt_iport at index \bt_p{index} from the
    \bt_sink_comp \bt_p{component}.

@param[in] component
    Sink component from which to borrow the input port at index
    \bt_p{index}.
@param[in] index
    Index of the input port to borrow from \bt_p{component}.

@returns
    @parblock
    \em Borrowed reference of the input port of
    \bt_p{component} at index \bt_p{index}.

    The returned pointer remains valid as long as \bt_p{component}
    exists.
    @endparblock

@bt_pre_not_null{component}
@pre
    \bt_p{index} is less than the number of input ports
    \bt_p{component} has (as returned by
    bt_component_sink_get_input_port_count()).

@sa bt_component_sink_get_input_port_count() &mdash;
    Returns the number of input ports that a sink component has.
*/
extern const bt_port_input *
bt_component_sink_borrow_input_port_by_index_const(
		const bt_component_sink *component, uint64_t index);

/*!
@brief
    Borrows the \bt_oport named \bt_p{name} from the \bt_sink_comp
    \bt_p{component}.

If \bt_p{component} has no output port named \bt_p{name}, this function
returns \c NULL.

@param[in] component
    Sink component from which to borrow the output port
    named \bt_p{name}.
@param[in] name
    Name of the output port to borrow from \bt_p{component}.

@returns
    @parblock
    \em Borrowed reference of the output port of
    \bt_p{component} named \bt_p{name}, or \c NULL if none.

    The returned pointer remains valid as long as \bt_p{component}
    exists.
    @endparblock

@bt_pre_not_null{component}
@bt_pre_not_null{name}
*/
extern const bt_port_input *
bt_component_sink_borrow_input_port_by_name_const(
		const bt_component_sink *component, const char *name);

/*! @} */

/*!
@name Sink component reference count
@{
*/

/*!
@brief
    Increments the \ref api-fund-shared-object "reference count" of
    the \bt_sink_comp \bt_p{component}.

@param[in] component
    @parblock
    Sink component of which to increment the reference count.

    Can be \c NULL.
    @endparblock

@sa bt_component_sink_put_ref() &mdash;
    Decrements the reference count of a sink component.
*/
extern void bt_component_sink_get_ref(
		const bt_component_sink *component);

/*!
@brief
    Decrements the \ref api-fund-shared-object "reference count" of
    the \bt_sink_comp \bt_p{component}.

@param[in] component
    @parblock
    Sink component of which to decrement the reference count.

    Can be \c NULL.
    @endparblock

@sa bt_component_sink_get_ref() &mdash;
    Increments the reference count of a sink component.
*/
extern void bt_component_sink_put_ref(
		const bt_component_sink *component);

/*!
@brief
    Decrements the reference count of the \bt_sink_comp
    \bt_p{_component}, and then sets \bt_p{_component} to \c NULL.

@param _component
    @parblock
    Sink component of which to decrement the reference count.

    Can contain \c NULL.
    @endparblock

@bt_pre_assign_expr{_component}
*/
#define BT_COMPONENT_SINK_PUT_REF_AND_RESET(_component)		\
	do {							\
		bt_component_sink_put_ref(_component);		\
		(_component) = NULL;				\
	} while (0)

/*!
@brief
    Decrements the reference count of the \bt_sink_comp \bt_p{_dst},
    sets \bt_p{_dst} to \bt_p{_src}, and then sets \bt_p{_src} to
    \c NULL.

This macro effectively moves a sink component reference from the
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
#define BT_COMPONENT_SINK_MOVE_REF(_dst, _src)		\
	do {						\
		bt_component_sink_put_ref(_dst);	\
		(_dst) = (_src);			\
		(_src) = NULL;				\
	} while (0)

/*! @} */

/*! @} */

#ifdef __cplusplus
}
#endif

#endif /* BABELTRACE2_GRAPH_COMPONENT_H */
