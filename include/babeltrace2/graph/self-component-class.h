/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (C) 2010-2019 EfficiOS Inc. and Linux Foundation
 */

#ifndef BABELTRACE2_GRAPH_SELF_COMPONENT_CLASS_H
#define BABELTRACE2_GRAPH_SELF_COMPONENT_CLASS_H

#ifndef __BT_IN_BABELTRACE_H
# error "Please include <babeltrace2/babeltrace.h> instead."
#endif

#include <babeltrace2/types.h>

#ifdef __cplusplus
extern "C" {
#endif

/*!
@defgroup api-self-comp-cls Self component classes
@ingroup api-comp-cls-dev

@brief
    Private views of \bt_p_comp_cls for class methods.

The #bt_self_component_class, #bt_self_component_class_source,
#bt_self_component_class_filter, #bt_self_component_class_sink types are
private views of a \bt_comp_cls from within a component class
\ref api-comp-cls-dev-class-meth "class method".

As of \bt_name_version_min_maj, this module only contains functions
to \ref api-fund-c-typing "upcast" the "self" (private) types to their
public #bt_component_class, #bt_component_class_source,
#bt_component_class_filter, and #bt_component_class_sink counterparts.
*/

/*! @{ */

/*!
@name Types
@{

@typedef struct bt_self_component_class bt_self_component_class;

@brief
    Self \bt_comp_cls.

@typedef struct bt_self_component_class_source bt_self_component_class_source;

@brief
    Self \bt_src_comp_cls.

@typedef struct bt_self_component_class_filter bt_self_component_class_filter;

@brief
    Self \bt_flt_comp_cls.

@typedef struct bt_self_component_class_sink bt_self_component_class_sink;

@brief
    Self \bt_sink_comp_cls.

@}
*/

/*!
@name Self to public upcast
@{
*/

/*!
@brief
    \ref api-fund-c-typing "Upcasts" the self \bt_comp_cls
    \bt_p{self_component_class} to the public #bt_component_class type.

@param[in] self_component_class
    @parblock
    Component class to upcast.

    Can be \c NULL.
    @endparblock

@returns
    \bt_p{self_component_class} as a public component class.
*/
static inline
const bt_component_class *bt_self_component_class_as_component_class(
		bt_self_component_class *self_component_class)
{
	return __BT_UPCAST(bt_component_class, self_component_class);
}

/*!
@brief
    \ref api-fund-c-typing "Upcasts" the self \bt_src_comp_cls
    \bt_p{self_component_class} to the public #bt_component_class_source
    type.

@param[in] self_component_class
    @parblock
    Source component class to upcast.

    Can be \c NULL.
    @endparblock

@returns
    \bt_p{self_component_class} as a public source component class.
*/
static inline
const bt_component_class_source *
bt_self_component_class_source_as_component_class_source(
		bt_self_component_class_source *self_component_class)
{
	return __BT_UPCAST_CONST(bt_component_class_source,
		self_component_class);
}

/*!
@brief
    \ref api-fund-c-typing "Upcasts" the self \bt_flt_comp_cls
    \bt_p{self_component_class} to the public #bt_component_class_filter
    type.

@param[in] self_component_class
    @parblock
    Filter component class to upcast.

    Can be \c NULL.
    @endparblock

@returns
    \bt_p{self_component_class} as a public filter component class.
*/
static inline
const bt_component_class_filter *
bt_self_component_class_filter_as_component_class_filter(
		bt_self_component_class_filter *self_component_class)
{
	return __BT_UPCAST_CONST(bt_component_class_filter,
		self_component_class);
}

/*!
@brief
    \ref api-fund-c-typing "Upcasts" the self \bt_sink_comp_cls
    \bt_p{self_component_class} to the public #bt_component_class_sink
    type.

@param[in] self_component_class
    @parblock
    Sink component class to upcast.

    Can be \c NULL.
    @endparblock

@returns
    \bt_p{self_component_class} as a public sink component class.
*/
static inline
const bt_component_class_sink *
bt_self_component_class_sink_as_component_class_sink(
		bt_self_component_class_sink *self_component_class)
{
	return __BT_UPCAST_CONST(bt_component_class_sink, self_component_class);
}

/*! @} */

/*!
@name Self to common self upcast
@{
*/

/*!
@brief
    \ref api-fund-c-typing "Upcasts" the self \bt_src_comp_cls
    \bt_p{self_component_class} to the common #bt_self_component_class
    type.

@param[in] self_component_class
    @parblock
    Source component class to upcast.

    Can be \c NULL.
    @endparblock

@returns
    \bt_p{self_component_class} as a common self component class.
*/
static inline
bt_self_component_class*
bt_self_component_class_source_as_self_component_class(
		bt_self_component_class_source *self_component_class)
{
	return __BT_UPCAST(bt_self_component_class, self_component_class);
}

/*!
@brief
    \ref api-fund-c-typing "Upcasts" the self \bt_flt_comp_cls
    \bt_p{self_component_class} to the common #bt_self_component_class
    type.

@param[in] self_component_class
    @parblock
    Filter component class to upcast.

    Can be \c NULL.
    @endparblock

@returns
    \bt_p{self_component_class} as a common self component class.
*/
static inline
bt_self_component_class*
bt_self_component_class_filter_as_self_component_class(
		bt_self_component_class_filter *self_component_class)
{
	return __BT_UPCAST(bt_self_component_class, self_component_class);
}

/*!
@brief
    \ref api-fund-c-typing "Upcasts" the self \bt_sink_comp_cls
    \bt_p{self_component_class} to the common #bt_self_component_class
    type.

@param[in] self_component_class
    @parblock
    Sink component class to upcast.

    Can be \c NULL.
    @endparblock

@returns
    \bt_p{self_component_class} as a common self component class.
*/
static inline
bt_self_component_class*
bt_self_component_class_sink_as_self_component_class(
		bt_self_component_class_sink *self_component_class)
{
	return __BT_UPCAST(bt_self_component_class, self_component_class);
}

/*! @} */

/*! @} */

#ifdef __cplusplus
}
#endif

#endif /* BABELTRACE2_GRAPH_SELF_COMPONENT_CLASS_H */
