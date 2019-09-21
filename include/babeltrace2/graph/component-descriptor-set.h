#ifndef BABELTRACE2_GRAPH_COMPONENT_DESCRIPTOR_SET_H
#define BABELTRACE2_GRAPH_COMPONENT_DESCRIPTOR_SET_H

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
@defgroup api-comp-descr-set Component descriptor set
@ingroup api-graph

@brief
    Set of descriptors of prospective \bt_p_comp to use with
    bt_get_greatest_operative_mip_version().

A <strong><em>component descriptor set</em></strong>
is an \em unordered set of component descriptors.

A <strong><em>component descriptor</em></strong> describes a
prospective \bt_comp, that is, everything that is needed to
\ref api-graph-lc-add "instantiate a component class" within a
trace processing \bt_graph without actually doing it:

- The <strong>component class</strong> to instantiate, which you would
  pass as the \bt_p{component_class} parameter of one of the
  <code>bt_graph_add_*_component*()</code> functions.

- The <strong>initialization parameters</strong>, which you
  would pass as the \bt_p{params} parameter of one of the
  <code>bt_graph_add_*_component*()</code> functions.

- The <strong>initialization method data</strong>, which you would pass
  as the \bt_p{initialize_method_data} parameter of one of the
  <code>bt_graph_add_*_component_with_initialize_method_data()</code>
  functions.

As of \bt_name_version_min_maj, the only use case of a component
descriptor set is bt_get_greatest_operative_mip_version(). This
function computes the greatest \bt_mip version which
you can use to create a trace processing graph to which you intend
to \ref api-graph-lc-add "add components" described by a set of
component descriptors.

A component descriptor set is a
\ref api-fund-shared-object "shared object": get a new reference with
bt_component_descriptor_set_get_ref() and put an existing reference with
bt_component_descriptor_set_put_ref().

Create an empty component descriptor set with
bt_component_descriptor_set_create().

Add a component descriptor to a component descriptor set with
bt_component_descriptor_set_add_descriptor() and
bt_component_descriptor_set_add_descriptor_with_initialize_method_data().
*/

/*! @{ */

/*!
@name Type
@{

@typedef struct bt_component_descriptor_set bt_component_descriptor_set;

@brief
    Component descriptor set.

@}
*/

/*!
@name Creation
@{
*/


/*!
@brief
    Creates an empty component descriptor set.

@returns
    New component descriptor set reference, or \c NULL on memory error.
*/
extern bt_component_descriptor_set *bt_component_descriptor_set_create(void);

/*!
@brief
    Status codes for bt_component_descriptor_set_add_descriptor()
    and
    bt_component_descriptor_set_add_descriptor_with_initialize_method_data().
*/
typedef enum bt_component_descriptor_set_add_descriptor_status {
	/*!
	@brief
	    Success.
	*/
	BT_COMPONENT_DESCRIPTOR_SET_ADD_DESCRIPTOR_STATUS_OK		= __BT_FUNC_STATUS_OK,

	/*!
	@brief
	    Out of memory.
	*/
	BT_COMPONENT_DESCRIPTOR_SET_ADD_DESCRIPTOR_STATUS_MEMORY_ERROR	= __BT_FUNC_STATUS_MEMORY_ERROR,
} bt_component_descriptor_set_add_descriptor_status;

/*! @} */

/*!
@name Component descriptor adding
@{
*/

/*!
@brief
    Alias of
    bt_component_descriptor_set_add_descriptor_with_initialize_method_data()
    with the \bt_p{initialize_method_data} parameter set to \c NULL.
*/
extern bt_component_descriptor_set_add_descriptor_status
bt_component_descriptor_set_add_descriptor(
		bt_component_descriptor_set *component_descriptor_set,
		const bt_component_class *component_class,
		const bt_value *params);

/*!
@brief
    Adds a descriptor of a \bt_comp which would be an instance of the
    \bt_comp_cls \bt_p{component_class}, would receive the parameters
    \bt_p{params} and the method data \bt_p{initialize_method_data} at
    initialization time, to the component descriptor set
    \bt_p{component_descriptor_set}.

@param[in] component_descriptor_set
    Component descriptor set to which to add a component descriptor.
@param[in] component_class
    \bt_c_comp_cls which would be instantiated to create the
    described component.
@param[in] params
    @parblock
    Parameters which would be passed to the initialization method of
    the described component as the \bt_p{params} parameter.

    Can be \c NULL, in which case it is equivalent to passing an empty
    \bt_map_val.
    @endparblock
@param[in] initialize_method_data
    User data which would be passed to the initialization method of
    the described component as the \bt_p{initialize_method_data}
    parameter.

@retval #BT_COMPONENT_DESCRIPTOR_SET_ADD_DESCRIPTOR_STATUS_OK
    Success.
@retval #BT_COMPONENT_DESCRIPTOR_SET_ADD_DESCRIPTOR_STATUS_MEMORY_ERROR
    Out of memory.

@bt_pre_not_null{component_descriptor_set}
@bt_pre_not_null{component_class}
@pre
    \bt_p{params} is a \bt_map_val (bt_value_is_map() returns #BT_TRUE)
    or is \c NULL.

@bt_post_success_frozen{component_class}
@bt_post_success_frozen{params}
*/
extern bt_component_descriptor_set_add_descriptor_status
bt_component_descriptor_set_add_descriptor_with_initialize_method_data(
		bt_component_descriptor_set *component_descriptor_set,
		const bt_component_class *component_class,
		const bt_value *params, void *initialize_method_data);

/*! @} */

/*!
@name Reference count
@{
*/

/*!
@brief
    Increments the \ref api-fund-shared-object "reference count" of
    the component descriptor set \bt_p{component_descriptor_set}.

@param[in] component_descriptor_set
    @parblock
    Component descriptor set of which to increment the reference count.

    Can be \c NULL.
    @endparblock

@sa bt_component_descriptor_set_put_ref() &mdash;
    Decrements the reference count of a component descriptor set.
*/
extern void bt_component_descriptor_set_get_ref(
		const bt_component_descriptor_set *component_descriptor_set);

/*!
@brief
    Decrements the \ref api-fund-shared-object "reference count" of
    the component descriptor set \bt_p{component_descriptor_set}.

@param[in] component_descriptor_set
    @parblock
    Component descriptor set of which to decrement the reference count.

    Can be \c NULL.
    @endparblock

@sa bt_component_descriptor_set_get_ref() &mdash;
    Increments the reference count of a component descriptor set.
*/
extern void bt_component_descriptor_set_put_ref(
		const bt_component_descriptor_set *component_descriptor_set);

/*!
@brief
    Decrements the reference count of the component descriptor set
    \bt_p{_component_descriptor_set}, and then sets
    \bt_p{_component_descriptor_set} to \c NULL.

@param _component_descriptor_set
    @parblock
    Component descriptor set of which to decrement the reference count.

    Can contain \c NULL.
    @endparblock

@bt_pre_assign_expr{_component_descriptor_set}
*/
#define BT_COMPONENT_DESCRIPTOR_SET_PUT_REF_AND_RESET(_component_descriptor_set) \
	do {								\
		bt_component_descriptor_set_put_ref(_component_descriptor_set); \
		(_component_descriptor_set) = NULL;			\
	} while (0)

/*!
@brief
    Decrements the reference count of the component descriptor set
    \bt_p{_dst}, sets \bt_p{_dst} to \bt_p{_src}, and then sets
    \bt_p{_src} to \c NULL.

This macro effectively moves a component descriptor set reference from
the expression \bt_p{_src} to the expression \bt_p{_dst}, putting the
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
#define BT_COMPONENT_DESCRIPTOR_SET_MOVE_REF(_dst, _src)	\
	do {							\
		bt_component_descriptor_set_put_ref(_dst);	\
		(_dst) = (_src);				\
		(_src) = NULL;					\
	} while (0)

/*! @} */

/*! @} */

#ifdef __cplusplus
}
#endif

#endif /* BABELTRACE2_GRAPH_COMPONENT_DESCRIPTOR_SET_H */
