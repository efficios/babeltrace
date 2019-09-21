#ifndef BABELTRACE2_GRAPH_COMPONENT_CLASS_H
#define BABELTRACE2_GRAPH_COMPONENT_CLASS_H

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
@defgroup api-comp-cls Component classes
@ingroup api-graph

@brief
    Source, filter, and sink component classes (non-development).

A <strong><em>component class</em></strong> is the class of a \bt_comp:

@image html component.png

@attention
    This module (component class API) offers basic, read-only functions
    to get component class properties. To \em create a component class,
    see \ref api-comp-cls-dev or \ref api-plugin-dev.

You can instantiate a given component class many times, with different
initialization parameters, to create many components with the
<code>bt_graph_add_*_component*()</code> functions (see \ref api-graph).

There are two ways to obtain a component class:

- Create one programatically: see \ref api-comp-cls-dev.

- Borrow one from a \bt_plugin.

  Note that, as of \bt_name_version_min_maj, you cannot access a
  component class's plugin, if any.

A component class is a \ref api-fund-shared-object "shared object": get
a new reference with bt_component_class_get_ref() and put an existing
reference with bt_component_class_put_ref().

The common C&nbsp;type of a component class is #bt_component_class.

There are three types of component classes:

<dl>
  <dt>\anchor api-comp-cls-src Source component class</dt>
  <dd>
    A source component class instance (a \bt_src_comp) \bt_msg_iter
    emits fresh \bt_p_msg.

    A source component class's specific type is
    #bt_component_class_source and its type enumerator is
    #BT_COMPONENT_CLASS_TYPE_SOURCE.

    \ref api-fund-c-typing "Upcast" the #bt_component_class_source type
    to the #bt_component_class type with
    bt_component_class_source_as_component_class_const().

    Get a new source component class reference with Use
    bt_component_class_source_get_ref() and put an existing one with
    bt_component_class_source_put_ref().
  </dd>

  <dt>\anchor api-comp-cls-flt Filter component class</dt>
  <dd>
    A filter component class instance (a \bt_flt_comp) message iterator
    emits fresh and transformed messages. It can also discard
    existing messages.

    A filter component class's specific type is
    #bt_component_class_filter and its type enumerator is
    #BT_COMPONENT_CLASS_TYPE_FILTER.

    \ref api-fund-c-typing "Upcast" the #bt_component_class_filter type
    to the #bt_component_class type with
    bt_component_class_filter_as_component_class_const().

    Get a new filter component class reference with
    bt_component_class_filter_get_ref() and put an existing one with
    bt_component_class_filter_put_ref().
  </dd>

  <dt>\anchor api-comp-cls-sink Sink component class</dt>
  <dd>
    A sink component class instance (a \bt_sink_comp) consumes messages
    from a source or filter message iterator.

    A filter component class's specific type is #bt_component_class_sink
    and its type enumerator is #BT_COMPONENT_CLASS_TYPE_SINK.

    \ref api-fund-c-typing "Upcast" the #bt_component_class_sink type to
    the #bt_component_class type with
    bt_component_class_sink_as_component_class_const().

    Get a new sink component class reference with
    bt_component_class_sink_get_ref() and put an existing one with
    bt_component_class_sink_put_ref().
  </dd>
</dl>

Get a component's class type enumerator with
bt_component_class_get_type(). You can also use the
bt_component_class_is_source(), bt_component_class_is_filter(), and
bt_component_class_is_sink() helper functions.

<h1>Properties</h1>

A component class has the following common properties:

<dl>
  <dt>
    \anchor api-comp-cls-prop-name
    Name
  </dt>
  <dd>
    Name of the component class.

    Within a \bt_plugin, for a given component class type, each
    component class has a unique name.

    Get a component class's name with bt_component_class_get_name().
  </dd>

  <dt>
    \anchor api-comp-cls-prop-descr
    \bt_dt_opt Description
  </dt>
  <dd>
    Textual description of the component class.

    Get a component class's description with
    bt_component_class_get_description().
  </dd>

  <dt>
    \anchor api-comp-cls-prop-help
    \bt_dt_opt Help text
  </dt>
  <dd>
    Help text of the component class.

    Get a component class's help text with
    bt_component_class_get_help().
  </dd>
</dl>
*/

/*! @{ */

/*!
@name Types
@{

@typedef struct bt_component_class bt_component_class;

@brief
    Component class.

@typedef struct bt_component_class_source bt_component_class_source;

@brief
    \bt_c_src_comp_cls.

@typedef struct bt_component_class_filter bt_component_class_filter;

@brief
    \bt_c_flt_comp_cls.

@typedef struct bt_component_class_sink bt_component_class_sink;

@brief
    \bt_c_sink_comp_cls.

@}
*/

/*!
@name Type query
@{
*/

/*!
@brief
    Component class type enumerators.
*/
typedef enum bt_component_class_type {
	/*!
	@brief
	    \bt_c_src_comp_cls.
	*/
	BT_COMPONENT_CLASS_TYPE_SOURCE	= 1 << 0,

	/*!
	@brief
	    \bt_c_flt_comp_cls.
	*/
	BT_COMPONENT_CLASS_TYPE_FILTER	= 1 << 1,

	/*!
	@brief
	    \bt_c_sink_comp_cls.
	*/
	BT_COMPONENT_CLASS_TYPE_SINK	= 1 << 2,
} bt_component_class_type;

/*!
@brief
    Returns the type enumerator of the component class
    \bt_p{component_class}.

@param[in] component_class
    Component class of which to get the type enumerator.

@returns
    Type enumerator of \bt_p{component_class}.

@bt_pre_not_null{component_class}

@sa bt_component_class_is_source() &mdash;
    Returns whether or not a component class is a \bt_src_comp_cls.
@sa bt_component_class_is_filter() &mdash;
    Returns whether or not a component class is a \bt_flt_comp_cls.
@sa bt_component_class_is_sink() &mdash;
    Returns whether or not a component class is a \bt_sink_comp_cls.
*/
extern bt_component_class_type bt_component_class_get_type(
		const bt_component_class *component_class);

/*!
@brief
    Returns whether or not the component class \bt_p{component_class}
    is a \bt_src_comp_cls.

@param[in] component_class
    Component class to check.

@returns
    #BT_TRUE if \bt_p{component_class} is a source component class.

@bt_pre_not_null{component_class}

@sa bt_component_class_get_type() &mdash;
    Returns the type enumerator of a component class.
*/
static inline
bt_bool bt_component_class_is_source(
		const bt_component_class *component_class)
{
	return bt_component_class_get_type(component_class) ==
		BT_COMPONENT_CLASS_TYPE_SOURCE;
}

/*!
@brief
    Returns whether or not the component class \bt_p{component_class}
    is a \bt_flt_comp_cls.

@param[in] component_class
    Component class to check.

@returns
    #BT_TRUE if \bt_p{component_class} is a filter component class.

@bt_pre_not_null{component_class}

@sa bt_component_class_get_type() &mdash;
    Returns the type enumerator of a component class.
*/
static inline
bt_bool bt_component_class_is_filter(
		const bt_component_class *component_class)
{
	return bt_component_class_get_type(component_class) ==
		BT_COMPONENT_CLASS_TYPE_FILTER;
}

/*!
@brief
    Returns whether or not the component class \bt_p{component_class}
    is a \bt_sink_comp_cls.

@param[in] component_class
    Component class to check.

@returns
    #BT_TRUE if \bt_p{component_class} is a sink component class.

@bt_pre_not_null{component_class}

@sa bt_component_class_get_type() &mdash;
    Returns the type enumerator of a component class.
*/
static inline
bt_bool bt_component_class_is_sink(
		const bt_component_class *component_class)
{
	return bt_component_class_get_type(component_class) ==
		BT_COMPONENT_CLASS_TYPE_SINK;
}

/*! @} */

/*!
@name Properties
@{
*/

/*!
@brief
    Returns the name of the component class \bt_p{component_class}.

See the \ref api-comp-cls-prop-name "name" property.

@param[in] component_class
    Component class of which to get the name.

@returns
    @parblock
    Name of \bt_p{component_class}.

    The returned pointer remains valid as long as \bt_p{component_class}
    exists.
    @endparblock

@bt_pre_not_null{component_class}
*/
extern const char *bt_component_class_get_name(
		const bt_component_class *component_class);

/*!
@brief
    Returns the description of the component class
    \bt_p{component_class}.

See the \ref api-comp-cls-prop-descr "description" property.

@param[in] component_class
    Component class of which to get the description.

@returns
    @parblock
    Description of \bt_p{component_class}, or \c NULL if none.

    The returned pointer remains valid as long as \bt_p{component_class}
    exists.
    @endparblock

@bt_pre_not_null{component_class}
*/
extern const char *bt_component_class_get_description(
		const bt_component_class *component_class);

/*!
@brief
    Returns the help text of the component class \bt_p{component_class}.

See the \ref api-comp-cls-prop-help "help text" property.

@param[in] component_class
    Component class of which to get the help text.

@returns
    @parblock
    Help text of \bt_p{component_class}, or \c NULL if none.

    The returned pointer remains valid as long as \bt_p{component_class}
    exists.
    @endparblock

@bt_pre_not_null{component_class}
*/
extern const char *bt_component_class_get_help(
		const bt_component_class *component_class);

/*! @} */

/*!
@name Common reference count
@{
*/

/*!
@brief
    Increments the \ref api-fund-shared-object "reference count" of
    the component class \bt_p{component_class}.

@param[in] component_class
    @parblock
    Component class of which to increment the reference count.

    Can be \c NULL.
    @endparblock

@sa bt_component_put_ref() &mdash;
    Decrements the reference count of a component class.
*/
extern void bt_component_class_get_ref(
		const bt_component_class *component_class);

/*!
@brief
    Decrements the \ref api-fund-shared-object "reference count" of
    the component class \bt_p{component_class}.

@param[in] component_class
    @parblock
    Component class of which to decrement the reference count.

    Can be \c NULL.
    @endparblock

@sa bt_component_get_ref() &mdash;
    Increments the reference count of a component class.
*/
extern void bt_component_class_put_ref(
		const bt_component_class *component_class);

/*!
@brief
    Decrements the reference count of the component class
    \bt_p{_component_class}, and then sets \bt_p{_component_class}
    to \c NULL.

@param _component_class
    @parblock
    Component class of which to decrement the reference count.

    Can contain \c NULL.
    @endparblock

@bt_pre_assign_expr{_component_class}
*/
#define BT_COMPONENT_CLASS_PUT_REF_AND_RESET(_component_class)	\
	do {							\
		bt_component_class_put_ref(_component_class);	\
		(_component_class) = NULL;			\
	} while (0)

/*!
@brief
    Decrements the reference count of the component class \bt_p{_dst},
    sets \bt_p{_dst} to \bt_p{_src}, and then sets \bt_p{_src}
    to \c NULL.

This macro effectively moves a component class reference from the
expression
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
#define BT_COMPONENT_CLASS_MOVE_REF(_dst, _src)		\
	do {						\
		bt_component_class_put_ref(_dst);	\
		(_dst) = (_src);			\
		(_src) = NULL;				\
	} while (0)

/*! @} */

/*!
@name Source component class upcast
@{
*/

/*!
@brief
    \ref api-fund-c-typing "Upcasts" the \bt_src_comp_cls
    \bt_p{component_class} to the common #bt_component_class type.

@param[in] component_class
    @parblock
    Source component class to upcast.

    Can be \c NULL.
    @endparblock

@returns
    \bt_p{component_class} as a common component class.
*/
static inline
const bt_component_class *
bt_component_class_source_as_component_class_const(
		const bt_component_class_source *component_class)
{
	return __BT_UPCAST_CONST(bt_component_class, component_class);
}

/*! @} */

/*!
@name Source component class reference count
@{
*/

/*!
@brief
    Increments the \ref api-fund-shared-object "reference count" of
    the \bt_src_comp_cls \bt_p{component_class}.

@param[in] component_class
    @parblock
    Source component class of which to increment the reference count.

    Can be \c NULL.
    @endparblock

@sa bt_component_class_source_put_ref() &mdash;
    Decrements the reference count of a source component class.
*/
extern void bt_component_class_source_get_ref(
		const bt_component_class_source *component_class);

/*!
@brief
    Decrements the \ref api-fund-shared-object "reference count" of
    the \bt_src_comp_cls \bt_p{component_class}.

@param[in] component_class
    @parblock
    Source component class of which to decrement the reference count.

    Can be \c NULL.
    @endparblock

@sa bt_component_class_source_get_ref() &mdash;
    Increments the reference count of a source component class.
*/
extern void bt_component_class_source_put_ref(
		const bt_component_class_source *component_class);

/*!
@brief
    Decrements the reference count of the \bt_src_comp_cls
    \bt_p{_component_class}, and then sets \bt_p{_component_class} to
    \c NULL.

@param _component_class
    @parblock
    Source component class of which to decrement the reference count.

    Can contain \c NULL.
    @endparblock

@bt_pre_assign_expr{_component_class}
*/
#define BT_COMPONENT_CLASS_SOURCE_PUT_REF_AND_RESET(_component_class)	\
	do {								\
		bt_component_class_source_put_ref(_component_class);	\
		(_component_class) = NULL;				\
	} while (0)

/*!
@brief
    Decrements the reference count of the \bt_src_comp_cls \bt_p{_dst},
    sets \bt_p{_dst} to \bt_p{_src}, and then sets \bt_p{_src} to
    \c NULL.

This macro effectively moves a source component class reference from the
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
#define BT_COMPONENT_CLASS_SOURCE_MOVE_REF(_dst, _src)		\
	do {							\
		bt_component_class_source_put_ref(_dst);	\
		(_dst) = (_src);				\
		(_src) = NULL;					\
	} while (0)

/*! @} */

/*!
@name Filter component class upcast
@{
*/

/*!
@brief
    \ref api-fund-c-typing "Upcasts" the \bt_flt_comp_cls
    \bt_p{component_class} to the common #bt_component_class type.

@param[in] component_class
    @parblock
    Filter component class to upcast.

    Can be \c NULL.
    @endparblock

@returns
    \bt_p{component_class} as a common component class.
*/
static inline
const bt_component_class *
bt_component_class_filter_as_component_class_const(
		const bt_component_class_filter *component_class)
{
	return __BT_UPCAST_CONST(bt_component_class, component_class);
}

/*! @} */

/*!
@name Filter component class reference count
@{
*/

/*!
@brief
    Increments the \ref api-fund-shared-object "reference count" of
    the \bt_flt_comp_cls \bt_p{component_class}.

@param[in] component_class
    @parblock
    Filter component class of which to increment the reference count.

    Can be \c NULL.
    @endparblock

@sa bt_component_class_filter_put_ref() &mdash;
    Decrements the reference count of a filter component class.
*/
extern void bt_component_class_filter_get_ref(
		const bt_component_class_filter *component_class);

/*!
@brief
    Decrements the \ref api-fund-shared-object "reference count" of
    the \bt_flt_comp_cls \bt_p{component_class}.

@param[in] component_class
    @parblock
    Filter component class of which to decrement the reference count.

    Can be \c NULL.
    @endparblock

@sa bt_component_class_filter_get_ref() &mdash;
    Increments the reference count of a filter component class.
*/
extern void bt_component_class_filter_put_ref(
		const bt_component_class_filter *component_class);

/*!
@brief
    Decrements the reference count of the \bt_flt_comp_cls
    \bt_p{_component_class}, and then sets \bt_p{_component_class} to
    \c NULL.

@param _component_class
    @parblock
    Filter component class of which to decrement the reference count.

    Can contain \c NULL.
    @endparblock

@bt_pre_assign_expr{_component_class}
*/
#define BT_COMPONENT_CLASS_FILTER_PUT_REF_AND_RESET(_component_class)	\
	do {								\
		bt_component_class_filter_put_ref(_component_class);	\
		(_component_class) = NULL;				\
	} while (0)

/*!
@brief
    Decrements the reference count of the \bt_flt_comp_cls \bt_p{_dst},
    setsc \bt_p{_dst} to \bt_p{_src}, and then sets \bt_p{_src} to
    \c NULL.

This macro effectively moves a filter component class reference from the
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
#define BT_COMPONENT_CLASS_FILTER_MOVE_REF(_dst, _src)		\
	do {							\
		bt_component_class_filter_put_ref(_dst);	\
		(_dst) = (_src);				\
		(_src) = NULL;					\
	} while (0)

/*! @} */

/*!
@name Sink component class upcast
@{
*/

/*!
@brief
    \ref api-fund-c-typing "Upcasts" the \bt_sink_comp_cls
    \bt_p{component_class} to the common #bt_component_class type.

@param[in] component_class
    @parblock
    Sink component class to upcast.

    Can be \c NULL.
    @endparblock

@returns
    \bt_p{component_class} as a common component class.
*/
static inline
const bt_component_class *
bt_component_class_sink_as_component_class_const(
		const bt_component_class_sink *component_class)
{
	return __BT_UPCAST_CONST(bt_component_class, component_class);
}

/*! @} */

/*!
@name Sink component class reference count
@{
*/

/*!
@brief
    Increments the \ref api-fund-shared-object "reference count" of
    the \bt_sink_comp_cls \bt_p{component_class}.

@param[in] component_class
    @parblock
    Sink component class of which to increment the reference count.

    Can be \c NULL.
    @endparblock

@sa bt_component_class_sink_put_ref() &mdash;
    Decrements the reference count of a sink component class.
*/
extern void bt_component_class_sink_get_ref(
		const bt_component_class_sink *component_class);

/*!
@brief
    Decrements the \ref api-fund-shared-object "reference count" of
    the \bt_sink_comp_cls \bt_p{component_class}.

@param[in] component_class
    @parblock
    Sink component class of which to decrement the reference count.

    Can be \c NULL.
    @endparblock

@sa bt_component_class_sink_get_ref() &mdash;
    Increments the reference count of a sink component class.
*/
extern void bt_component_class_sink_put_ref(
		const bt_component_class_sink *component_class);

/*!
@brief
    Decrements the reference count of the \bt_sink_comp_cls
    \bt_p{_component_class}, and then sets \bt_p{_component_class} to
    \c NULL.

@param _component_class
    @parblock
    Sink component class of which to decrement the reference count.

    Can contain \c NULL.
    @endparblock

@bt_pre_assign_expr{_component_class}
*/
#define BT_COMPONENT_CLASS_SINK_PUT_REF_AND_RESET(_component_class)	\
	do {								\
		bt_component_class_sink_put_ref(_component_class);	\
		(_component_class) = NULL;				\
	} while (0)

/*!
@brief
    Decrements the reference count of the \bt_sink_comp_cls \bt_p{_dst},
    sets \bt_p{_dst} to \bt_p{_src}, and then sets \bt_p{_src} to
    \c NULL.

This macro effectively moves a sink component class reference from the
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
#define BT_COMPONENT_CLASS_SINK_MOVE_REF(_dst, _src)		\
	do {							\
		bt_component_class_sink_put_ref(_dst);		\
		(_dst) = (_src);				\
		(_src) = NULL;					\
	} while (0)

/*! @} */

/*! @} */

#ifdef __cplusplus
}
#endif

#endif /* BABELTRACE2_GRAPH_COMPONENT_CLASS_H */
