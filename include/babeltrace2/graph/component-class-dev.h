#ifndef BABELTRACE2_GRAPH_COMPONENT_CLASS_DEV_H
#define BABELTRACE2_GRAPH_COMPONENT_CLASS_DEV_H

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
@defgroup api-comp-cls-dev Component class development
@ingroup api-graph

@brief
    Component class development (creation).

A <strong><em>component class</em></strong> is the class of a \bt_comp:

@image html component.png

@attention
    This module (component class development API) offers functions to
    programatically create component classes. To get the properties of
    an existing component class, see \ref api-comp-cls.

A component class has <em>methods</em>. This module essentially
offers:

- Component class method type definitions.

- Component class creation functions, to which you pass a mandatory
  \bt_msg_iter_cls or method.

- Functions to set optional component class methods.

- Functions to set optional component class properties.

A component class method is a user function. There are two types of
methods:

<dl>
  <dt>\anchor api-comp-cls-dev-instance-meth Instance method</dt>
  <dd>
    Operates on an instance (a \bt_comp).

    The type of the method's first parameter is
    #bt_self_component_source, #bt_self_component_filter, or
    bt_self_component_sink, depending on the component class's type.

    This is similar to an instance method in Python (where the instance
    object name is generally <code>self</code>) or a member function
    in C++ (where the instance pointer is named <code>this</code>),
    for example.
  </dd>

  <dt>\anchor api-comp-cls-dev-class-meth Class method</dt>
  <dd>
    Operates on a component class.

    The type of the method's first parameter is
    #bt_self_component_class_source, #bt_self_component_class_filter, or
    bt_self_component_class_sink, depending on the component class's
    type.

    This is similar to a class method in Python or a static member
    function in C++, for example.
  </dd>
</dl>

See \ref api-comp-cls-dev-methods "Methods" to learn more about the
different types of component class methods.

A component class is a \ref api-fund-shared-object "shared object": see
the \ref api-comp-cls module for the reference count functions.

Some library functions \ref api-fund-freezing "freeze" component classes
on success. The documentation of those functions indicate this
postcondition.

Create a component class with bt_component_class_source_create(),
bt_component_class_filter_create(), and
bt_component_class_sink_create(). You must give the component class a
name (not unique in any way) at creation time.

When you create a \bt_src_comp_cls or a \bt_flt_comp_cls, you must pass
a \bt_msg_iter_cls. This is the class of any \bt_msg_iter created for
one of the component class's instance's \bt_oport.

When you create a \bt_sink_comp_cls, you must pass a
\ref api-comp-cls-dev-meth-consume "consuming method".

\ref api-fund-c-typing "Upcast" the #bt_component_class_source,
#bt_component_class_filter, and bt_component_class_sink types returned
by the creation functions to the #bt_component_class type with
bt_component_class_source_as_component_class(),
bt_component_class_filter_as_component_class(), and
bt_component_class_sink_as_component_class().

Set the \ref api-comp-cls-prop-descr "description" and the
\ref api-comp-cls-prop-help "help text" of a component class with
bt_component_class_set_description() and
bt_component_class_set_help().

<h1>\anchor api-comp-cls-dev-methods Methods</h1>

To learn when exactly the methods below are called, see
\ref api-graph-lc "Trace processing graph life cycle".

The available component class methods to implement are:

<table>
  <tr>
    <th>Name
    <th>Method type
    <th>Component class types
    <th>Requirement
    <th>Graph is \ref api-graph-lc "configured"?
    <th>C types
  <tr>
    <td>Consume
    <td>\ref api-comp-cls-dev-instance-meth "Instance"
    <td>Sink
    <td>Mandatory
    <td>Yes
    <td>#bt_component_class_sink_consume_method
  <tr>
    <td>Finalize
    <td>Instance
    <td>All
    <td>Optional
    <td>Yes
    <td>
      #bt_component_class_source_finalize_method<br>
      #bt_component_class_filter_finalize_method<br>
      #bt_component_class_sink_finalize_method
  <tr>
    <td>Get supported \bt_mip (MIP) versions
    <td>\ref api-comp-cls-dev-class-meth "Class"
    <td>All
    <td>Optional
    <td>N/A
    <td>
      #bt_component_class_source_get_supported_mip_versions_method<br>
      #bt_component_class_filter_get_supported_mip_versions_method<br>
      #bt_component_class_sink_get_supported_mip_versions_method
  <tr>
    <td>Graph is \ref api-graph-lc "configured"
    <td>Instance
    <td>Sink
    <td>Mandatory
    <td>Yes
    <td>#bt_component_class_sink_graph_is_configured_method
  <tr>
    <td>Initialize
    <td>Instance
    <td>All
    <td>Optional
    <td>No
    <td>
      #bt_component_class_source_initialize_method<br>
      #bt_component_class_filter_initialize_method<br>
      #bt_component_class_sink_initialize_method
  <tr>
    <td>\bt_c_iport connected
    <td>Instance
    <td>Filter and sink
    <td>Optional
    <td>No
    <td>
      #bt_component_class_filter_input_port_connected_method<br>
      #bt_component_class_sink_input_port_connected_method
  <tr>
    <td>\bt_c_oport connected
    <td>Instance
    <td>Source and filter
    <td>Optional
    <td>No
    <td>
      #bt_component_class_source_output_port_connected_method<br>
      #bt_component_class_filter_output_port_connected_method
  <tr>
    <td>Query
    <td>Class
    <td>All
    <td>Optional
    <td>N/A
    <td>
      #bt_component_class_source_query_method<br>
      #bt_component_class_filter_query_method<br>
      #bt_component_class_sink_query_method
</table>

<dl>
  <dt>
    \anchor api-comp-cls-dev-meth-consume
    Consume
  </dt>
  <dd>
    Called within bt_graph_run() or bt_graph_run_once() to make your
    \bt_sink_comp consume and process \bt_p_msg.

    This method typically gets \em one message batch from one (or more)
    upstream \bt_msg_iter. You are free to get more than one batch of
    messages if needed; however, keep in mind that the \bt_name project
    recommends that this method executes fast enough so as not to block
    an interactive application running on the same thread.

    During what you consider to be a long, blocking operation, the
    project recommends that you periodically check whether or not you
    are interrupted with bt_self_component_sink_is_interrupted(). When
    you are, you can return either
    #BT_COMPONENT_CLASS_SINK_CONSUME_METHOD_STATUS_AGAIN or
    #BT_COMPONENT_CLASS_SINK_CONSUME_METHOD_STATUS_ERROR, depending on
    your capability to continue the current operation later.

    If you need to block the thread, you can instead report to
    try again later to the bt_graph_run() or bt_graph_run_once() caller
    by returning #BT_COMPONENT_CLASS_SINK_CONSUME_METHOD_STATUS_AGAIN.

    If your sink component is done consuming and processing, return
    #BT_COMPONENT_CLASS_SINK_CONSUME_METHOD_STATUS_END from this method.
    The trace processing \bt_graph can continue to run afterwards if
    other sink components are still consuming.

    Within this method, you \em cannot add an \bt_iport with
    bt_self_component_sink_add_input_port().

    Set this mandatory method at sink component class creation time with
    bt_component_class_sink_create().
  </dd>

  <dt>
    \anchor api-comp-cls-dev-meth-fini
    Finalize
  </dt>
  <dd>
    Called to finalize your \bt_comp, that is, to let you
    destroy/free/finalize any user data you have (retrieved with
    bt_self_component_get_data()).

    The \bt_name library does not specify exactly when this method is
    called, but guarantees that it's called before the component is
    destroyed.

    For \bt_p_src_comp and \bt_p_flt_comp, the library guarantees that
    this method is called \em after all the component's \bt_p_msg_iter
    are finalized.

    This method is \em not called if the component's
    \ref api-comp-cls-dev-meth-init "initialization method"
    previously returned an error status code.

    Within this method, you cannot:

    - Add a \bt_port.
    - Use any \bt_msg_iter.

    Set this optional method with
    bt_component_class_source_set_finalize_method(),
    bt_component_class_filter_set_finalize_method(), and
    bt_component_class_sink_set_finalize_method().
  </dd>

  <dt>
    \anchor api-comp-cls-dev-meth-mip
    Get supported \bt_mip (MIP) versions
  </dt>
  <dd>
    Called within bt_get_greatest_operative_mip_version() to get the
    set of MIP versions that an eventual \bt_comp supports.

    This is a \ref api-comp-cls-dev-class-meth "class method" because
    you can call bt_get_greatest_operative_mip_version() before you even
    create a trace processing \bt_graph.

    In this method, you receive initialization parameters as the
    \bt_p{params} parameter and initialization method data as the
    \bt_p{initialize_method_data}. Those parameters are set
    when bt_component_descriptor_set_add_descriptor() is called, before
    bt_get_greatest_operative_mip_version() is called.

    Considering those initialization parameters, you need to fill the
    received \bt_uint_rs \bt_p{supported_versions} with the rangs of
    MIP versions you support.

    As of \bt_name_version_min_maj, you can only support MIP version 0.

    Not having this method is equivalent to having one which adds the
    [0,&nbsp;0] range to the \bt_p{supported_versions} set.

    Set this optional method with
    bt_component_class_source_set_get_supported_mip_versions_method(),
    bt_component_class_filter_set_get_supported_mip_versions_method(),
    and bt_component_class_sink_set_get_supported_mip_versions_method().
  </dd>

  <dt>
    \anchor api-comp-cls-dev-meth-graph-configured
    Graph is \ref api-graph-lc "configured"
  </dt>
  <dd>
    For a given trace processing \bt_graph, called the first time
    bt_graph_run() or bt_graph_run_once() is called to notify your
    \bt_sink_comp that the graph is now configured.

    Within this method, you can create \bt_p_msg_iter on your sink
    component's \bt_p_iport. You can also manipulate those message
    iterators, for example get and process initial messages or make
    them.

    This method is called \em after the component's
    \ref api-comp-cls-dev-meth-init "initialization method"
    is called. You cannot create a message iterator in the
    initialization method.

    Within this method, you \em cannot add an \bt_iport with
    bt_self_component_sink_add_input_port().

    Set this optional method with
    bt_component_class_sink_set_graph_is_configured_method().
  </dd>

  <dt>
    \anchor api-comp-cls-dev-meth-init
    Initialize
  </dt>
  <dd>
    Called within a <code>bt_graph_add_*_component*()</code> function
    (see \ref api-graph) to initialize your \bt_comp.

    Within this method, you receive the initialization parameters and
    initialization method data passed to the
    <code>bt_graph_add_*_component*()</code> function.

    This method is where you can add initial \bt_p_port to your
    component with bt_self_component_source_add_output_port(),
    bt_self_component_filter_add_input_port(),
    bt_self_component_filter_add_output_port(), or
    bt_self_component_sink_add_input_port().
    You can also add ports in the
    \ref api-comp-cls-dev-meth-iport-connected "input port connected"
    and
    \ref api-comp-cls-dev-meth-oport-connected "output port connected"
    methods.

    You can create user data and set it as the \bt_self_comp's user data
    with bt_self_component_set_data().

    If you return #BT_COMPONENT_CLASS_INITIALIZE_METHOD_STATUS_OK from
    this method, then your component's
    \ref api-comp-cls-dev-meth-fini "finalization method" will be
    called, if it exists, when your component is finalized.

    Set this optional method with
    bt_component_class_source_set_initialize_method(),
    bt_component_class_filter_set_initialize_method(),
    and bt_component_class_sink_set_initialize_method().
  </dd>

  <dt>
    \anchor api-comp-cls-dev-meth-iport-connected
    \bt_c_iport connected
  </dt>
  <dd>
    Called within bt_graph_connect_ports() to notify your \bt_comp that
    one of its input ports has been connected.

    Within this method, you can add more \bt_p_port to your
    component with bt_self_component_source_add_output_port(),
    bt_self_component_filter_add_input_port(),
    bt_self_component_filter_add_output_port(), or
    bt_self_component_sink_add_input_port().

    Set this optional method with
    bt_component_class_filter_set_input_port_connected_method() and
    bt_component_class_sink_set_input_port_connected_method().
  </dd>

  <dt>
    \anchor api-comp-cls-dev-meth-oport-connected
    \bt_c_oport connected
  </dt>
  <dd>
    Called within bt_graph_connect_ports() to notify your \bt_comp that
    one of its output ports has been connected.

    Within this method, you can add more \bt_p_port to your
    component with bt_self_component_source_add_output_port(),
    bt_self_component_filter_add_input_port(),
    bt_self_component_filter_add_output_port(), or
    bt_self_component_sink_add_input_port().

    Set this optional method with
    bt_component_class_source_set_output_port_connected_method() and
    bt_component_class_filter_set_output_port_connected_method().
  </dd>

  <dt>
    \anchor api-comp-cls-dev-meth-query
    Query
  </dt>
  <dd>
    Called within bt_query_executor_query() to make your \bt_comp_cls
    perform a query operation.

    Within this method, you receive the query object name, the
    query parameters, and the method data passed when the
    \bt_qexec was created with bt_query_executor_create() or
    bt_query_executor_create_with_method_data().

    You also receive a private view of the query executor which you can
    cast to a \c const query executor with
    bt_private_query_executor_as_query_executor_const() to access the
    executor's logging level with bt_query_executor_get_logging_level().

    On success, set \bt_p{*result} to the query operation's result: a
    \em new \bt_val reference.

    If the queried object's name (\bt_p{object_name} parameter) is
    unknown, return
    #BT_COMPONENT_CLASS_QUERY_METHOD_STATUS_UNKNOWN_OBJECT.

    If you need to block the thread, you can instead report to
    try again later to the bt_query_executor_query() caller
    by returning #BT_COMPONENT_CLASS_QUERY_METHOD_STATUS_AGAIN.

    Set this optional method with
    bt_component_class_source_set_query_method(),
    bt_component_class_filter_set_query_method(), and
    bt_component_class_sink_set_query_method().
  </dd>
</dl>

@attention
    @parblock
    In any of the methods above:

    - \em Never call bt_component_get_ref(),
      bt_component_source_get_ref(), bt_component_filter_get_ref(), or
      bt_component_sink_get_ref() on your own (upcasted) \bt_self_comp
      to avoid reference cycles.

      You can keep a borrowed (weak) \bt_self_comp reference in your
      component's user data (see bt_self_component_set_data()).

    - \em Never call bt_port_get_ref(), bt_port_input_get_ref(), or
      bt_port_output_get_ref() on one of your own (upcasted)
      \bt_p_self_comp_port to avoid reference cycles.

    - \em Never call bt_component_class_get_ref(),
      bt_component_class_source_get_ref(),
      bt_component_class_filter_get_ref(), or
      bt_component_class_sink_get_ref() on your own (upcasted)
      \bt_comp_cls to avoid reference cycles.
    @endparblock

Within any \ref api-comp-cls-dev-instance-meth "instance method", you
can access the \bt_comp's configured
\ref #bt_logging_level "logging level" by first upcasting the
\bt_self_comp to the #bt_component type with
bt_self_component_as_component(), and then with
bt_component_get_logging_level().
*/

/*! @{ */

/*!
@name Method types
@{
*/

/*!
@brief
    Status codes for #bt_component_class_sink_consume_method.
*/
typedef enum bt_component_class_sink_consume_method_status {
	/*!
	@brief
	    Success.
	*/
	BT_COMPONENT_CLASS_SINK_CONSUME_METHOD_STATUS_OK		= __BT_FUNC_STATUS_OK,

	/*!
	@brief
	    Sink component is finished processing.
	*/
	BT_COMPONENT_CLASS_SINK_CONSUME_METHOD_STATUS_END		= __BT_FUNC_STATUS_END,

	/*!
	@brief
	    Try again.
	*/
	BT_COMPONENT_CLASS_SINK_CONSUME_METHOD_STATUS_AGAIN		= __BT_FUNC_STATUS_AGAIN,

	/*!
	@brief
	    Out of memory.
	*/
	BT_COMPONENT_CLASS_SINK_CONSUME_METHOD_STATUS_MEMORY_ERROR	= __BT_FUNC_STATUS_MEMORY_ERROR,

	/*!
	@brief
	    User error.
	*/
	BT_COMPONENT_CLASS_SINK_CONSUME_METHOD_STATUS_ERROR		= __BT_FUNC_STATUS_ERROR,
} bt_component_class_sink_consume_method_status;

/*!
@brief
    \bt_c_sink_comp consuming method.

See the \ref api-comp-cls-dev-meth-consume "consume" method.

@param[in] self_component
    \bt_c_sink_comp instance.

@retval #BT_COMPONENT_CLASS_SINK_CONSUME_METHOD_STATUS_OK
    Success.
@retval #BT_COMPONENT_CLASS_SINK_CONSUME_METHOD_STATUS_END
    Finished processing.
@retval #BT_COMPONENT_CLASS_SINK_CONSUME_METHOD_STATUS_AGAIN
    Try again.
@retval #BT_COMPONENT_CLASS_SINK_CONSUME_METHOD_STATUS_MEMORY_ERROR
    Out of memory.
@retval #BT_COMPONENT_CLASS_SINK_CONSUME_METHOD_STATUS_ERROR
    User error.

@bt_pre_not_null{self_component}

@sa bt_component_class_sink_create() &mdash;
    Creates a \bt_sink_comp.
*/
typedef bt_component_class_sink_consume_method_status
(*bt_component_class_sink_consume_method)(
		bt_self_component_sink *self_component);

/*!
@brief
    \bt_c_src_comp finalization method.

See the \ref api-comp-cls-dev-meth-fini "finalize" method.

@param[in] self_component
    Source component instance.

@bt_pre_not_null{self_component}

@bt_post_no_error

@sa bt_component_class_source_set_finalize_method() &mdash;
    Sets the finalization method of a source component class.
*/
typedef void (*bt_component_class_source_finalize_method)(
		bt_self_component_source *self_component);

/*!
@brief
    \bt_c_flt_comp finalization method.

See the \ref api-comp-cls-dev-meth-fini "finalize" method.

@param[in] self_component
    Filter component instance.

@bt_pre_not_null{self_component}

@bt_post_no_error

@sa bt_component_class_filter_set_finalize_method() &mdash;
    Sets the finalization method of a filter component class.
*/
typedef void (*bt_component_class_filter_finalize_method)(
		bt_self_component_filter *self_component);

/*!
@brief
    \bt_c_sink_comp finalization method.

See the \ref api-comp-cls-dev-meth-fini "finalize" method.

@param[in] self_component
    Sink component instance.

@bt_pre_not_null{self_component}

@bt_post_no_error

@sa bt_component_class_sink_set_finalize_method() &mdash;
    Sets the finalization method of a sink component class.
*/
typedef void (*bt_component_class_sink_finalize_method)(
		bt_self_component_sink *self_component);

/*!
@brief
    Status codes for
    #bt_component_class_source_get_supported_mip_versions_method,
    #bt_component_class_filter_get_supported_mip_versions_method, and
    #bt_component_class_sink_get_supported_mip_versions_method.
*/
typedef enum bt_component_class_get_supported_mip_versions_method_status {
	/*!
	@brief
	    Success.
	*/
	BT_COMPONENT_CLASS_GET_SUPPORTED_MIP_VERSIONS_METHOD_STATUS_OK			= __BT_FUNC_STATUS_OK,

	/*!
	@brief
	    Out of memory.
	*/
	BT_COMPONENT_CLASS_GET_SUPPORTED_MIP_VERSIONS_METHOD_STATUS_MEMORY_ERROR	= __BT_FUNC_STATUS_MEMORY_ERROR,

	/*!
	@brief
	    User error.
	*/
	BT_COMPONENT_CLASS_GET_SUPPORTED_MIP_VERSIONS_METHOD_STATUS_ERROR		= __BT_FUNC_STATUS_ERROR,
} bt_component_class_get_supported_mip_versions_method_status;

/*!
@brief
    \bt_c_src_comp_cls \"get supported \bt_mip versions\" method.

See the \ref api-comp-cls-dev-meth-mip "get supported MIP versions"
method.

As of \bt_name_version_min_maj, you can only add the range [0,&nbsp;0]
to \bt_p{supported_versions}.

@param[in] self_component_class
    Source component class.
@param[in] params
    @parblock
    Initialization parameters, as passed as the \bt_p{params} parameter
    of bt_component_descriptor_set_add_descriptor() or
    bt_component_descriptor_set_add_descriptor_with_initialize_method_data().

    \bt_p{params} is frozen.
    @endparblock
@param[in] initialize_method_data
    User data for this method, as passed as the
    \bt_p{init_method_data} parameter of
    bt_component_descriptor_set_add_descriptor_with_initialize_method_data().
@param[in] logging_level
    Logging level to use during this method's execution, as passed
    as the \bt_p{logging_level} parameter of
    bt_get_greatest_operative_mip_version().
@param[in] supported_versions
    \bt_c_uint_rs to which to add the ranges of supported MIP versions
    of an eventual instance of \bt_p{self_component_class} considering
    \bt_p{params} and \bt_p{initialize_method_data}.

@retval #BT_COMPONENT_CLASS_GET_SUPPORTED_MIP_VERSIONS_METHOD_STATUS_OK
    Success.
@retval #BT_COMPONENT_CLASS_GET_SUPPORTED_MIP_VERSIONS_METHOD_STATUS_MEMORY_ERROR
    Out of memory.
@retval #BT_COMPONENT_CLASS_GET_SUPPORTED_MIP_VERSIONS_METHOD_STATUS_ERROR
    User error.

@bt_pre_not_null{self_component_class}
@bt_pre_not_null{params}
@bt_pre_is_map_val{params}
@bt_pre_not_null{supported_versions}
@pre
    \bt_p{supported_versions} is empty.

@sa bt_component_class_source_set_get_supported_mip_versions_method() &mdash;
    Sets the "get supported MIP versions" method of a source
    component class.
*/
typedef bt_component_class_get_supported_mip_versions_method_status
(*bt_component_class_source_get_supported_mip_versions_method)(
		bt_self_component_class_source *self_component_class,
		const bt_value *params, void *initialize_method_data,
		bt_logging_level logging_level,
		bt_integer_range_set_unsigned *supported_versions);


/*!
@brief
    \bt_c_flt_comp_cls \"get supported \bt_mip versions\" method.

See the \ref api-comp-cls-dev-meth-mip "get supported MIP versions"
method.

As of \bt_name_version_min_maj, you can only add the range [0,&nbsp;0]
to \bt_p{supported_versions}.

@param[in] self_component_class
    Filter component class.
@param[in] params
    @parblock
    Initialization parameters, as passed as the \bt_p{params} parameter
    of bt_component_descriptor_set_add_descriptor() or
    bt_component_descriptor_set_add_descriptor_with_initialize_method_data().

    \bt_p{params} is frozen.
    @endparblock
@param[in] initialize_method_data
    User data for this method, as passed as the
    \bt_p{init_method_data} parameter of
    bt_component_descriptor_set_add_descriptor_with_initialize_method_data().
@param[in] logging_level
    Logging level to use during this method's execution, as passed
    as the \bt_p{logging_level} parameter of
    bt_get_greatest_operative_mip_version().
@param[in] supported_versions
    \bt_c_uint_rs to which to add the ranges of supported MIP versions
    of an eventual instance of \bt_p{self_component_class} considering
    \bt_p{params} and \bt_p{initialize_method_data}.

@retval #BT_COMPONENT_CLASS_GET_SUPPORTED_MIP_VERSIONS_METHOD_STATUS_OK
    Success.
@retval #BT_COMPONENT_CLASS_GET_SUPPORTED_MIP_VERSIONS_METHOD_STATUS_MEMORY_ERROR
    Out of memory.
@retval #BT_COMPONENT_CLASS_GET_SUPPORTED_MIP_VERSIONS_METHOD_STATUS_ERROR
    User error.

@bt_pre_not_null{self_component_class}
@bt_pre_not_null{params}
@bt_pre_is_map_val{params}
@bt_pre_not_null{supported_versions}
@pre
    \bt_p{supported_versions} is empty.

@sa bt_component_class_filter_set_get_supported_mip_versions_method() &mdash;
    Sets the "get supported MIP versions" method of a filter
    component class.
*/
typedef bt_component_class_get_supported_mip_versions_method_status
(*bt_component_class_filter_get_supported_mip_versions_method)(
		bt_self_component_class_filter *source_component_class,
		const bt_value *params, void *initialize_method_data,
		bt_logging_level logging_level,
		bt_integer_range_set_unsigned *supported_versions);

/*!
@brief
    \bt_c_sink_comp_cls \"get supported \bt_mip versions\" method.

See the \ref api-comp-cls-dev-meth-mip "get supported MIP versions"
method.

As of \bt_name_version_min_maj, you can only add the range [0,&nbsp;0]
to \bt_p{supported_versions}.

@param[in] self_component_class
    Sink component class.
@param[in] params
    @parblock
    Initialization parameters, as passed as the \bt_p{params} parameter
    of bt_component_descriptor_set_add_descriptor() or
    bt_component_descriptor_set_add_descriptor_with_initialize_method_data().

    \bt_p{params} is frozen.
    @endparblock
@param[in] initialize_method_data
    User data for this method, as passed as the
    \bt_p{init_method_data} parameter of
    bt_component_descriptor_set_add_descriptor_with_initialize_method_data().
@param[in] logging_level
    Logging level to use during this method's execution, as passed
    as the \bt_p{logging_level} parameter of
    bt_get_greatest_operative_mip_version().
@param[in] supported_versions
    \bt_c_uint_rs to which to add the ranges of supported MIP versions
    of an eventual instance of \bt_p{self_component_class} considering
    \bt_p{params} and \bt_p{initialize_method_data}.

@retval #BT_COMPONENT_CLASS_GET_SUPPORTED_MIP_VERSIONS_METHOD_STATUS_OK
    Success.
@retval #BT_COMPONENT_CLASS_GET_SUPPORTED_MIP_VERSIONS_METHOD_STATUS_MEMORY_ERROR
    Out of memory.
@retval #BT_COMPONENT_CLASS_GET_SUPPORTED_MIP_VERSIONS_METHOD_STATUS_ERROR
    User error.

@bt_pre_not_null{self_component_class}
@bt_pre_not_null{params}
@bt_pre_is_map_val{params}
@bt_pre_not_null{supported_versions}
@pre
    \bt_p{supported_versions} is empty.

@sa bt_component_class_sink_set_get_supported_mip_versions_method() &mdash;
    Sets the "get supported MIP versions" method of a sink
    component class.
*/
typedef bt_component_class_get_supported_mip_versions_method_status
(*bt_component_class_sink_get_supported_mip_versions_method)(
		bt_self_component_class_sink *source_component_class,
		const bt_value *params, void *initialize_method_data,
		bt_logging_level logging_level,
		bt_integer_range_set_unsigned *supported_versions);

/*!
@brief
    Status codes for
    #bt_component_class_sink_graph_is_configured_method.
*/
typedef enum bt_component_class_sink_graph_is_configured_method_status {
	/*!
	@brief
	    Success.
	*/
	BT_COMPONENT_CLASS_SINK_GRAPH_IS_CONFIGURED_METHOD_STATUS_OK		= __BT_FUNC_STATUS_OK,

	/*!
	@brief
	    Out of memory.
	*/
	BT_COMPONENT_CLASS_SINK_GRAPH_IS_CONFIGURED_METHOD_STATUS_MEMORY_ERROR	= __BT_FUNC_STATUS_MEMORY_ERROR,

	/*!
	@brief
	    User error.
	*/
	BT_COMPONENT_CLASS_SINK_GRAPH_IS_CONFIGURED_METHOD_STATUS_ERROR		= __BT_FUNC_STATUS_ERROR,
} bt_component_class_sink_graph_is_configured_method_status;

/*!
@brief
    \bt_c_sink_comp "graph is configured" method.

See the \ref api-comp-cls-dev-meth-graph-configured
"graph is configured" method.

@param[in] self_component
    Sink component instance.

@retval #BT_COMPONENT_CLASS_SINK_GRAPH_IS_CONFIGURED_METHOD_STATUS_OK
    Success.
@retval #BT_COMPONENT_CLASS_SINK_GRAPH_IS_CONFIGURED_METHOD_STATUS_MEMORY_ERROR
    Out of memory.
@retval #BT_COMPONENT_CLASS_SINK_GRAPH_IS_CONFIGURED_METHOD_STATUS_ERROR
    User error.

@bt_pre_not_null{self_component}

@sa bt_component_class_sink_set_graph_is_configured_method() &mdash;
    Sets the "graph is configured" method of a sink component class.
*/
typedef bt_component_class_sink_graph_is_configured_method_status
(*bt_component_class_sink_graph_is_configured_method)(
		bt_self_component_sink *self_component);

/*!
@brief
    Status codes for
    #bt_component_class_source_initialize_method,
    #bt_component_class_filter_initialize_method, and
    #bt_component_class_sink_initialize_method.
*/
typedef enum bt_component_class_initialize_method_status {
	/*!
	@brief
	    Success.
	*/
	BT_COMPONENT_CLASS_INITIALIZE_METHOD_STATUS_OK			= __BT_FUNC_STATUS_OK,

	/*!
	@brief
	    Out of memory.
	*/
	BT_COMPONENT_CLASS_INITIALIZE_METHOD_STATUS_MEMORY_ERROR	= __BT_FUNC_STATUS_MEMORY_ERROR,

	/*!
	@brief
	    User error.
	*/
	BT_COMPONENT_CLASS_INITIALIZE_METHOD_STATUS_ERROR		= __BT_FUNC_STATUS_ERROR,
} bt_component_class_initialize_method_status;

/*!
@brief
    \bt_c_src_comp initialization method.

See the \ref api-comp-cls-dev-meth-init "initialize" method.

As of \bt_name_version_min_maj, the \bt_p{configuration} parameter
is not used.

@param[in] self_component
    Source component instance.
@param[in] configuration
    Initial component configuration (unused).
@param[in] params
    @parblock
    Initialization parameters, as passed as the \bt_p{params} parameter
    of bt_graph_add_source_component() or
    bt_graph_add_source_component_with_initialize_method_data().

    \bt_p{params} is frozen.
    @endparblock
@param[in] initialize_method_data
    User data for this method, as passed as the
    \bt_p{initialize_method_data} parameter of
    bt_graph_add_source_component_with_initialize_method_data().

@retval #BT_COMPONENT_CLASS_INITIALIZE_METHOD_STATUS_OK
    Success.
@retval #BT_COMPONENT_CLASS_INITIALIZE_METHOD_STATUS_MEMORY_ERROR
    Out of memory.
@retval #BT_COMPONENT_CLASS_INITIALIZE_METHOD_STATUS_ERROR
    User error.

@bt_pre_not_null{self_component}
@bt_pre_not_null{configuration}
@bt_pre_not_null{params}
@bt_pre_is_map_val{params}

@sa bt_component_class_source_set_initialize_method() &mdash;
    Sets the initialization method of a source component class.
*/
typedef bt_component_class_initialize_method_status
(*bt_component_class_source_initialize_method)(
		bt_self_component_source *self_component,
		bt_self_component_source_configuration *configuration,
		const bt_value *params, void *initialize_method_data);

/*!
@brief
    \bt_c_flt_comp initialization method.

See the \ref api-comp-cls-dev-meth-init "initialize" method.

As of \bt_name_version_min_maj, the \bt_p{configuration} parameter
is not used.

@param[in] self_component
    Filter component instance.
@param[in] configuration
    Initial component configuration (unused).
@param[in] params
    @parblock
    Initialization parameters, as passed as the \bt_p{params} parameter
    of bt_graph_add_filter_component() or
    bt_graph_add_filter_component_with_initialize_method_data().

    \bt_p{params} is frozen.
    @endparblock
@param[in] initialize_method_data
    User data for this method, as passed as the
    \bt_p{initialize_method_data} parameter of
    bt_graph_add_filter_component_with_initialize_method_data().

@retval #BT_COMPONENT_CLASS_INITIALIZE_METHOD_STATUS_OK
    Success.
@retval #BT_COMPONENT_CLASS_INITIALIZE_METHOD_STATUS_MEMORY_ERROR
    Out of memory.
@retval #BT_COMPONENT_CLASS_INITIALIZE_METHOD_STATUS_ERROR
    User error.

@bt_pre_not_null{self_component}
@bt_pre_not_null{configuration}
@bt_pre_not_null{params}
@bt_pre_is_map_val{params}

@sa bt_component_class_filter_set_initialize_method() &mdash;
    Sets the initialization method of a filter component class.
*/
typedef bt_component_class_initialize_method_status
(*bt_component_class_filter_initialize_method)(
		bt_self_component_filter *self_component,
		bt_self_component_filter_configuration *configuration,
		const bt_value *params, void *initialize_method_data);

/*!
@brief
    \bt_c_sink_comp initialization method.

See the \ref api-comp-cls-dev-meth-init "initialize" method.

As of \bt_name_version_min_maj, the \bt_p{configuration} parameter
is not used.

@param[in] self_component
    Sink component instance.
@param[in] configuration
    Initial component configuration (unused).
@param[in] params
    @parblock
    Initialization parameters, as passed as the \bt_p{params} parameter
    of bt_graph_add_sink_component(),
    bt_graph_add_sink_component_with_initialize_method_data(), or
    bt_graph_add_simple_sink_component().

    \bt_p{params} is frozen.
    @endparblock
@param[in] initialize_method_data
    User data for this method, as passed as the
    \bt_p{initialize_method_data} parameter of
    bt_graph_add_sink_component_with_initialize_method_data().

@retval #BT_COMPONENT_CLASS_INITIALIZE_METHOD_STATUS_OK
    Success.
@retval #BT_COMPONENT_CLASS_INITIALIZE_METHOD_STATUS_MEMORY_ERROR
    Out of memory.
@retval #BT_COMPONENT_CLASS_INITIALIZE_METHOD_STATUS_ERROR
    User error.

@bt_pre_not_null{self_component}
@bt_pre_not_null{configuration}
@bt_pre_not_null{params}
@bt_pre_is_map_val{params}

@sa bt_component_class_sink_set_initialize_method() &mdash;
    Sets the initialization method of a sink component class.
*/
typedef bt_component_class_initialize_method_status
(*bt_component_class_sink_initialize_method)(
		bt_self_component_sink *self_component,
		bt_self_component_sink_configuration *configuration,
		const bt_value *params, void *initialize_method_data);

/*!
@brief
    Status codes for
    #bt_component_class_source_output_port_connected_method,
    #bt_component_class_filter_input_port_connected_method,
    #bt_component_class_filter_output_port_connected_method, and
    #bt_component_class_sink_input_port_connected_method.
*/
typedef enum bt_component_class_port_connected_method_status {
	/*!
	@brief
	    Success.
	*/
	BT_COMPONENT_CLASS_PORT_CONNECTED_METHOD_STATUS_OK		= __BT_FUNC_STATUS_OK,

	/*!
	@brief
	    Out of memory.
	*/
	BT_COMPONENT_CLASS_PORT_CONNECTED_METHOD_STATUS_MEMORY_ERROR	= __BT_FUNC_STATUS_MEMORY_ERROR,

	/*!
	@brief
	    User error.
	*/
	BT_COMPONENT_CLASS_PORT_CONNECTED_METHOD_STATUS_ERROR		= __BT_FUNC_STATUS_ERROR,
} bt_component_class_port_connected_method_status;

/*!
@brief
    \bt_c_src_comp "output port connected" method.

See the
\ref api-comp-cls-dev-meth-oport-connected "output port connected"
method.

@param[in] self_component
    Source component instance.
@param[in] self_port
    Connected \bt_oport of \bt_p{self_component}.
@param[in] other_port
    \bt_c_conn's other (input) port.

@retval #BT_COMPONENT_CLASS_PORT_CONNECTED_METHOD_STATUS_OK
    Success.
@retval #BT_COMPONENT_CLASS_PORT_CONNECTED_METHOD_STATUS_MEMORY_ERROR
    Out of memory.
@retval #BT_COMPONENT_CLASS_PORT_CONNECTED_METHOD_STATUS_ERROR
    User error.

@bt_pre_not_null{self_component}
@bt_pre_not_null{self_port}
@bt_pre_not_null{other_port}

@sa bt_component_class_source_set_output_port_connected_method() &mdash;
    Sets the "output port connected" method of a source component class.
*/
typedef bt_component_class_port_connected_method_status
(*bt_component_class_source_output_port_connected_method)(
		bt_self_component_source *self_component,
		bt_self_component_port_output *self_port,
		const bt_port_input *other_port);

/*!
@brief
    \bt_c_flt_comp "input port connected" method.

See the
\ref api-comp-cls-dev-meth-iport-connected "input port connected"
method.

@param[in] self_component
    Filter component instance.
@param[in] self_port
    Connected \bt_iport of \bt_p{self_component}.
@param[in] other_port
    \bt_c_conn's other (output) port.

@retval #BT_COMPONENT_CLASS_PORT_CONNECTED_METHOD_STATUS_OK
    Success.
@retval #BT_COMPONENT_CLASS_PORT_CONNECTED_METHOD_STATUS_MEMORY_ERROR
    Out of memory.
@retval #BT_COMPONENT_CLASS_PORT_CONNECTED_METHOD_STATUS_ERROR
    User error.

@bt_pre_not_null{self_component}
@bt_pre_not_null{self_port}
@bt_pre_not_null{other_port}

@sa bt_component_class_filter_set_input_port_connected_method() &mdash;
    Sets the "input port connected" method of a filter component class.
*/
typedef bt_component_class_port_connected_method_status
(*bt_component_class_filter_input_port_connected_method)(
		bt_self_component_filter *self_component,
		bt_self_component_port_input *self_port,
		const bt_port_output *other_port);

/*!
@brief
    \bt_c_flt_comp "output port connected" method.

See the
\ref api-comp-cls-dev-meth-oport-connected "output port connected"
method.

@param[in] self_component
    Filter component instance.
@param[in] self_port
    Connected \bt_oport of \bt_p{self_component}.
@param[in] other_port
    \bt_c_conn's other (input) port.

@retval #BT_COMPONENT_CLASS_PORT_CONNECTED_METHOD_STATUS_OK
    Success.
@retval #BT_COMPONENT_CLASS_PORT_CONNECTED_METHOD_STATUS_MEMORY_ERROR
    Out of memory.
@retval #BT_COMPONENT_CLASS_PORT_CONNECTED_METHOD_STATUS_ERROR
    User error.

@bt_pre_not_null{self_component}
@bt_pre_not_null{self_port}
@bt_pre_not_null{other_port}

@sa bt_component_class_filter_set_output_port_connected_method() &mdash;
    Sets the "output port connected" method of a filter component class.
*/
typedef bt_component_class_port_connected_method_status
(*bt_component_class_filter_output_port_connected_method)(
		bt_self_component_filter *self_component,
		bt_self_component_port_output *self_port,
		const bt_port_input *other_port);

/*!
@brief
    \bt_c_sink_comp "input port connected" method.

See the
\ref api-comp-cls-dev-meth-iport-connected "input port connected"
method.

@param[in] self_component
    Sink component instance.
@param[in] self_port
    Connected \bt_iport of \bt_p{self_component}.
@param[in] other_port
    \bt_c_conn's other (output) port.

@retval #BT_COMPONENT_CLASS_PORT_CONNECTED_METHOD_STATUS_OK
    Success.
@retval #BT_COMPONENT_CLASS_PORT_CONNECTED_METHOD_STATUS_MEMORY_ERROR
    Out of memory.
@retval #BT_COMPONENT_CLASS_PORT_CONNECTED_METHOD_STATUS_ERROR
    User error.

@bt_pre_not_null{self_component}
@bt_pre_not_null{self_port}
@bt_pre_not_null{other_port}

@sa bt_component_class_sink_set_input_port_connected_method() &mdash;
    Sets the "input port connected" method of a sink component class.
*/
typedef bt_component_class_port_connected_method_status
(*bt_component_class_sink_input_port_connected_method)(
		bt_self_component_sink *self_component,
		bt_self_component_port_input *self_port,
		const bt_port_output *other_port);

/*!
@brief
    Status codes for
    #bt_component_class_source_query_method,
    #bt_component_class_filter_query_method, and
    #bt_component_class_sink_query_method.
*/
typedef enum bt_component_class_query_method_status {
	/*!
	@brief
	    Success.
	*/
	BT_COMPONENT_CLASS_QUERY_METHOD_STATUS_OK		= __BT_FUNC_STATUS_OK,

	/*!
	@brief
	    Unknown object to query.
	*/
	BT_COMPONENT_CLASS_QUERY_METHOD_STATUS_UNKNOWN_OBJECT	= __BT_FUNC_STATUS_UNKNOWN_OBJECT,

	/*!
	@brief
	    Try again.
	*/
	BT_COMPONENT_CLASS_QUERY_METHOD_STATUS_AGAIN		= __BT_FUNC_STATUS_AGAIN,

	/*!
	@brief
	    Out of memory.
	*/
	BT_COMPONENT_CLASS_QUERY_METHOD_STATUS_MEMORY_ERROR	= __BT_FUNC_STATUS_MEMORY_ERROR,

	/*!
	@brief
	    User error.
	*/
	BT_COMPONENT_CLASS_QUERY_METHOD_STATUS_ERROR		= __BT_FUNC_STATUS_ERROR,
} bt_component_class_query_method_status;

/*!
@brief
    \bt_c_src_comp_cls query method.

See the \ref api-comp-cls-dev-meth-query "query" method.

@param[in] self_component_class
    Source component class, as passed as the \bt_p{component_class}
    parameter of bt_query_executor_create() or
    bt_query_executor_create_with_method_data() when creating this query
    operation's \ref api-qexec "executor".
@param[in] query_executor
    Private view of this query operation's executor.
@param[in] object_name
    Name of the object to query, as passed as the \bt_p{object_name}
    parameter of bt_query_executor_create() or
    bt_query_executor_create_with_method_data() when creating this query
    operation's executor.
@param[in] params
    @parblock
    Query parameters, as passed as the \bt_p{params}
    parameter of bt_query_executor_create() or
    bt_query_executor_create_with_method_data() when creating this query
    operation's executor.

    \bt_p{params} is frozen.
    @endparblock
@param[in] method_data
    User data for this method, as passed as the \bt_p{method_data}
    parameter of bt_query_executor_create_with_method_data() when
    creating this query operation's executor.
@param[out] result
    <strong>On success</strong>, \bt_p{*result} is
    a \em new reference of this query operation's result.

@retval #BT_COMPONENT_CLASS_QUERY_METHOD_STATUS_OK
    Success.
@retval #BT_COMPONENT_CLASS_QUERY_METHOD_STATUS_UNKNOWN_OBJECT
    \bt_p{object_name} is unknown.
@retval #BT_COMPONENT_CLASS_QUERY_METHOD_STATUS_AGAIN
    Try again.
@retval #BT_COMPONENT_CLASS_QUERY_METHOD_STATUS_MEMORY_ERROR
    Out of memory.
@retval #BT_COMPONENT_CLASS_QUERY_METHOD_STATUS_ERROR
    User error.

@bt_pre_not_null{self_component_class}
@bt_pre_not_null{query_executor}
@bt_pre_not_null{object_name}
@bt_pre_not_null{params}
@bt_pre_not_null{result}

@post
    <strong>On success</strong>, \bt_p{*result} is set.

@sa bt_component_class_source_set_query_method() &mdash;
    Sets the query method of a source component class.
*/
typedef bt_component_class_query_method_status
(*bt_component_class_source_query_method)(
		bt_self_component_class_source *self_component_class,
		bt_private_query_executor *query_executor,
		const char *object_name, const bt_value *params,
		void *method_data, const bt_value **result);

/*!
@brief
    \bt_c_flt_comp_cls query method.

See the \ref api-comp-cls-dev-meth-query "query" method.

@param[in] self_component_class
    Filter component class, as passed as the \bt_p{component_class}
    parameter of bt_query_executor_create() or
    bt_query_executor_create_with_method_data() when creating this query
    operation's \ref api-qexec "executor".
@param[in] query_executor
    Private view of this query operation's executor.
@param[in] object_name
    Name of the object to query, as passed as the \bt_p{object_name}
    parameter of bt_query_executor_create() or
    bt_query_executor_create_with_method_data() when creating this query
    operation's executor.
@param[in] params
    @parblock
    Query parameters, as passed as the \bt_p{params}
    parameter of bt_query_executor_create() or
    bt_query_executor_create_with_method_data() when creating this query
    operation's executor.

    \bt_p{params} is frozen.
    @endparblock
@param[in] method_data
    User data for this method, as passed as the \bt_p{method_data}
    parameter of bt_query_executor_create_with_method_data() when
    creating this query operation's executor.
@param[out] result
    <strong>On success</strong>, \bt_p{*result} is
    a \em new reference of this query operation's result.

@retval #BT_COMPONENT_CLASS_QUERY_METHOD_STATUS_OK
    Success.
@retval #BT_COMPONENT_CLASS_QUERY_METHOD_STATUS_UNKNOWN_OBJECT
    \bt_p{object_name} is unknown.
@retval #BT_COMPONENT_CLASS_QUERY_METHOD_STATUS_AGAIN
    Try again.
@retval #BT_COMPONENT_CLASS_QUERY_METHOD_STATUS_MEMORY_ERROR
    Out of memory.
@retval #BT_COMPONENT_CLASS_QUERY_METHOD_STATUS_ERROR
    User error.

@bt_pre_not_null{self_component_class}
@bt_pre_not_null{query_executor}
@bt_pre_not_null{object_name}
@bt_pre_not_null{params}
@bt_pre_not_null{result}

@post
    <strong>On success</strong>, \bt_p{*result} is set.

@sa bt_component_class_filter_set_query_method() &mdash;
    Sets the query method of a filter component class.
*/
typedef bt_component_class_query_method_status
(*bt_component_class_filter_query_method)(
		bt_self_component_class_filter *self_component_class,
		bt_private_query_executor *query_executor,
		const char *object_name, const bt_value *params,
		void *method_data, const bt_value **result);

/*!
@brief
    \bt_c_sink_comp_cls query method.

See the \ref api-comp-cls-dev-meth-query "query" method.

@param[in] self_component_class
    Sink component class, as passed as the \bt_p{component_class}
    parameter of bt_query_executor_create() or
    bt_query_executor_create_with_method_data() when creating this query
    operation's \ref api-qexec "executor".
@param[in] query_executor
    Private view of this query operation's executor.
@param[in] object_name
    Name of the object to query, as passed as the \bt_p{object_name}
    parameter of bt_query_executor_create() or
    bt_query_executor_create_with_method_data() when creating this query
    operation's executor.
@param[in] params
    @parblock
    Query parameters, as passed as the \bt_p{params}
    parameter of bt_query_executor_create() or
    bt_query_executor_create_with_method_data() when creating this query
    operation's executor.

    \bt_p{params} is frozen.
    @endparblock
@param[in] method_data
    User data for this method, as passed as the \bt_p{method_data}
    parameter of bt_query_executor_create_with_method_data() when
    creating this query operation's executor.
@param[out] result
    <strong>On success</strong>, \bt_p{*result} is
    a \em new reference of this query operation's result.

@retval #BT_COMPONENT_CLASS_QUERY_METHOD_STATUS_OK
    Success.
@retval #BT_COMPONENT_CLASS_QUERY_METHOD_STATUS_UNKNOWN_OBJECT
    \bt_p{object_name} is unknown.
@retval #BT_COMPONENT_CLASS_QUERY_METHOD_STATUS_AGAIN
    Try again.
@retval #BT_COMPONENT_CLASS_QUERY_METHOD_STATUS_MEMORY_ERROR
    Out of memory.
@retval #BT_COMPONENT_CLASS_QUERY_METHOD_STATUS_ERROR
    User error.

@bt_pre_not_null{self_component_class}
@bt_pre_not_null{query_executor}
@bt_pre_not_null{object_name}
@bt_pre_not_null{params}
@bt_pre_not_null{result}

@post
    <strong>On success</strong>, \bt_p{*result} is set.

@sa bt_component_class_sink_set_query_method() &mdash;
    Sets the query method of a sink component class.
*/
typedef bt_component_class_query_method_status
(*bt_component_class_sink_query_method)(
		bt_self_component_class_sink *self_component_class,
		bt_private_query_executor *query_executor,
		const char *object_name, const bt_value *params,
		void *method_data, const bt_value **result);

/*! @} */

/*!
@name Creation
@{
*/

/*!
@brief
    Creates a \bt_src_comp_cls named \bt_p{name} and having the
    \bt_msg_iter_cls \bt_p{message_iterator_class}.

On success, the returned source component class has the following
property values:

<table>
  <tr>
    <th>Property
    <th>Value
  <tr>
    <td>\ref api-comp-cls-prop-name "Name"
    <td>\bt_p{name}
  <tr>
    <td>\ref api-comp-cls-prop-descr "Description"
    <td>\em None
  <tr>
    <td>\ref api-comp-cls-prop-help "Help text"
    <td>\em None
</table>

@param[in] name
    Name of the source component class to create (copied).
@param[in] message_iterator_class
    Message iterator class of the source component class to create.

@returns
    New source component class reference, or \c NULL on memory error.

@bt_pre_not_null{name}
@bt_pre_not_null{message_iterator_class}

@bt_post_success_frozen{message_iterator_class}

@sa bt_message_iterator_class_create() &mdash;
    Creates a message iterator class.
*/
extern
bt_component_class_source *bt_component_class_source_create(
		const char *name,
		bt_message_iterator_class *message_iterator_class);

/*!
@brief
    Creates a \bt_flt_comp_cls named \bt_p{name} and having the
    \bt_msg_iter_cls \bt_p{message_iterator_class}.

On success, the returned filter component class has the following
property values:

<table>
  <tr>
    <th>Property
    <th>Value
  <tr>
    <td>\ref api-comp-cls-prop-name "Name"
    <td>\bt_p{name}
  <tr>
    <td>\ref api-comp-cls-prop-descr "Description"
    <td>\em None
  <tr>
    <td>\ref api-comp-cls-prop-help "Help text"
    <td>\em None
</table>

@param[in] name
    Name of the filter component class to create (copied).
@param[in] message_iterator_class
    Message iterator class of the filter component class to create.

@returns
    New filter component class reference, or \c NULL on memory error.

@bt_pre_not_null{name}
@bt_pre_not_null{message_iterator_class}

@bt_post_success_frozen{message_iterator_class}

@sa bt_message_iterator_class_create() &mdash;
    Creates a message iterator class.
*/
extern
bt_component_class_filter *bt_component_class_filter_create(
		const char *name,
		bt_message_iterator_class *message_iterator_class);

/*!
@brief
    Creates a \bt_sink_comp_cls named \bt_p{name} and having the
    \ref api-comp-cls-dev-meth-consume "consuming method"
    \bt_p{consume_method}.

On success, the returned sink component class has the following
property values:

<table>
  <tr>
    <th>Property
    <th>Value
  <tr>
    <td>\ref api-comp-cls-prop-name "Name"
    <td>\bt_p{name}
  <tr>
    <td>\ref api-comp-cls-prop-descr "Description"
    <td>\em None
  <tr>
    <td>\ref api-comp-cls-prop-help "Help text"
    <td>\em None
</table>

@param[in] name
    Name of the sink component class to create (copied).
@param[in] consume_method
    Consuming method of the sink component class to create.

@returns
    New sink component class reference, or \c NULL on memory error.

@bt_pre_not_null{name}
@bt_pre_not_null{consume_method}
*/
extern
bt_component_class_sink *bt_component_class_sink_create(
		const char *name,
		bt_component_class_sink_consume_method consume_method);

/*! @} */

/*!
@name Common properties
@{
*/

/*!
@brief
    Status codes for bt_component_class_set_description().
*/
typedef enum bt_component_class_set_description_status {
	/*!
	@brief
	    Success.
	*/
	BT_COMPONENT_CLASS_SET_DESCRIPTION_STATUS_OK		= __BT_FUNC_STATUS_OK,

	/*!
	@brief
	    Out of memory.
	*/
	BT_COMPONENT_CLASS_SET_DESCRIPTION_STATUS_MEMORY_ERROR	= __BT_FUNC_STATUS_MEMORY_ERROR,
} bt_component_class_set_description_status;

/*!
@brief
    Sets the description of the component class \bt_p{component_class}
    to a copy of \bt_p{description}.

See the \ref api-comp-cls-prop-descr "description" property.

@param[in] component_class
    Component class of which to set the description to
    \bt_p{description}.
@param[in] description
    New description of \bt_p{component_class} (copied).

@retval #BT_COMPONENT_CLASS_SET_DESCRIPTION_STATUS_OK
    Success.
@retval #BT_COMPONENT_CLASS_SET_DESCRIPTION_STATUS_MEMORY_ERROR
    Out of memory.

@bt_pre_not_null{component_class}
@bt_pre_hot{component_class}
@bt_pre_not_null{description}

@sa bt_component_class_get_description() &mdash;
    Returns the description of a component class.
*/
extern bt_component_class_set_description_status
bt_component_class_set_description(bt_component_class *component_class,
		const char *description);

/*!
@brief
    Status codes for bt_component_class_set_help().
*/
typedef enum bt_component_class_set_help_status {
	/*!
	@brief
	    Success.
	*/
	BT_COMPONENT_CLASS_SET_HELP_STATUS_OK		= __BT_FUNC_STATUS_OK,

	/*!
	@brief
	    Out of memory.
	*/
	BT_COMPONENT_CLASS_SET_HELP_STATUS_MEMORY_ERROR	= __BT_FUNC_STATUS_MEMORY_ERROR,
} bt_component_class_set_help_status;

/*!
@brief
    Sets the help text of the component class \bt_p{component_class}
    to a copy of \bt_p{help_text}.

See the \ref api-comp-cls-prop-help "help text" property.

@param[in] component_class
    Component class of which to set the help text to
    \bt_p{help_text}.
@param[in] help_text
    New help text of \bt_p{component_class} (copied).

@retval #BT_COMPONENT_CLASS_SET_HELP_STATUS_OK
    Success.
@retval #BT_COMPONENT_CLASS_SET_HELP_STATUS_MEMORY_ERROR
    Out of memory.

@bt_pre_not_null{component_class}
@bt_pre_hot{component_class}
@bt_pre_not_null{help_text}

@sa bt_component_class_get_help() &mdash;
    Returns the help text of a component class.
*/
extern bt_component_class_set_help_status bt_component_class_set_help(
		bt_component_class *component_class,
		const char *help_text);

/*! @} */

/*!
@name Method setting
@{
*/

/*!
@brief
    Status code for the
    <code>bt_component_class_*_set_*_method()</code> functions.
*/
typedef enum bt_component_class_set_method_status {
	/*!
	@brief
	    Success.
	*/
	BT_COMPONENT_CLASS_SET_METHOD_STATUS_OK	=	__BT_FUNC_STATUS_OK,
} bt_component_class_set_method_status;

/*!
@brief
    Sets the optional finalization method of the \bt_src_comp_cls
    \bt_p{component_class} to \bt_p{method}.

See the \ref api-comp-cls-dev-meth-fini "finalize" method.

@param[in] component_class
    Source component class of which to set the finalization method to
    \bt_p{method}.
@param[in] method
    New finalization method of \bt_p{component_class}.

@retval #BT_COMPONENT_CLASS_SET_METHOD_STATUS_OK
    Success.

@bt_pre_not_null{component_class}
@bt_pre_hot{component_class}
@bt_pre_not_null{method}
*/
extern bt_component_class_set_method_status
bt_component_class_source_set_finalize_method(
		bt_component_class_source *component_class,
		bt_component_class_source_finalize_method method);

/*!
@brief
    Sets the optional finalization method of the \bt_flt_comp_cls
    \bt_p{component_class} to \bt_p{method}.

See the \ref api-comp-cls-dev-meth-fini "finalize" method.

@param[in] component_class
    Filter component class of which to set the finalization method to
    \bt_p{method}.
@param[in] method
    New finalization method of \bt_p{component_class}.

@retval #BT_COMPONENT_CLASS_SET_METHOD_STATUS_OK
    Success.

@bt_pre_not_null{component_class}
@bt_pre_hot{component_class}
@bt_pre_not_null{method}
*/
extern bt_component_class_set_method_status
bt_component_class_filter_set_finalize_method(
		bt_component_class_filter *component_class,
		bt_component_class_filter_finalize_method method);

/*!
@brief
    Sets the optional finalization method of the \bt_sink_comp_cls
    \bt_p{component_class} to \bt_p{method}.

See the \ref api-comp-cls-dev-meth-fini "finalize" method.

@param[in] component_class
    Sink component class of which to set the finalization method to
    \bt_p{method}.
@param[in] method
    New finalization method of \bt_p{component_class}.

@retval #BT_COMPONENT_CLASS_SET_METHOD_STATUS_OK
    Success.

@bt_pre_not_null{component_class}
@bt_pre_hot{component_class}
@bt_pre_not_null{method}
*/
extern
bt_component_class_set_method_status
bt_component_class_sink_set_finalize_method(
		bt_component_class_sink *component_class,
		bt_component_class_sink_finalize_method method);

/*!
@brief
    Sets the \"get supported \bt_mip versions\" method of the
    \bt_src_comp_cls \bt_p{component_class} to \bt_p{method}.

See the \ref api-comp-cls-dev-meth-mip "get supported MIP versions"
method.

@param[in] component_class
    Source component class of which to set the "get supported MIP
    versions" method to \bt_p{method}.
@param[in] method
    New "get supported MIP versions" method of \bt_p{component_class}.

@retval #BT_COMPONENT_CLASS_SET_METHOD_STATUS_OK
    Success.

@bt_pre_not_null{component_class}
@bt_pre_hot{component_class}
@bt_pre_not_null{method}
*/
extern bt_component_class_set_method_status
bt_component_class_source_set_get_supported_mip_versions_method(
		bt_component_class_source *component_class,
		bt_component_class_source_get_supported_mip_versions_method method);

/*!
@brief
    Sets the \"get supported \bt_mip versions\" method of the
    \bt_flt_comp_cls \bt_p{component_class} to \bt_p{method}.

See the \ref api-comp-cls-dev-meth-mip "get supported MIP versions"
method.

@param[in] component_class
    Filter component class of which to set the "get supported MIP
    versions" method to \bt_p{method}.
@param[in] method
    New "get supported MIP versions" method of \bt_p{component_class}.

@retval #BT_COMPONENT_CLASS_SET_METHOD_STATUS_OK
    Success.

@bt_pre_not_null{component_class}
@bt_pre_hot{component_class}
@bt_pre_not_null{method}
*/
extern bt_component_class_set_method_status
bt_component_class_filter_set_get_supported_mip_versions_method(
		bt_component_class_filter *component_class,
		bt_component_class_filter_get_supported_mip_versions_method method);

/*!
@brief
    Sets the \"get supported \bt_mip versions\" method of the
    \bt_sink_comp_cls \bt_p{component_class} to \bt_p{method}.

See the \ref api-comp-cls-dev-meth-mip "get supported MIP versions"
method.

@param[in] component_class
    Sink component class of which to set the "get supported MIP
    versions" method to \bt_p{method}.
@param[in] method
    New "get supported MIP versions" method of \bt_p{component_class}.

@retval #BT_COMPONENT_CLASS_SET_METHOD_STATUS_OK
    Success.

@bt_pre_not_null{component_class}
@bt_pre_hot{component_class}
@bt_pre_not_null{method}
*/
extern bt_component_class_set_method_status
bt_component_class_sink_set_get_supported_mip_versions_method(
		bt_component_class_sink *component_class,
		bt_component_class_sink_get_supported_mip_versions_method method);

/*!
@brief
    Sets the "graph is configured" method of the
    \bt_sink_comp_cls \bt_p{component_class} to \bt_p{method}.

See the
\ref api-comp-cls-dev-meth-graph-configured "graph is configured"
method.

@param[in] component_class
    Sink component class of which to set the "graph is configured"
    method to \bt_p{method}.
@param[in] method
    New "graph is configured" method of \bt_p{component_class}.

@retval #BT_COMPONENT_CLASS_SET_METHOD_STATUS_OK
    Success.

@bt_pre_not_null{component_class}
@bt_pre_hot{component_class}
@bt_pre_not_null{method}
*/
extern
bt_component_class_set_method_status
bt_component_class_sink_set_graph_is_configured_method(
		bt_component_class_sink *component_class,
		bt_component_class_sink_graph_is_configured_method method);

/*!
@brief
    Sets the optional initialization method of the \bt_src_comp_cls
    \bt_p{component_class} to \bt_p{method}.

See the \ref api-comp-cls-dev-meth-init "initialize" method.

@param[in] component_class
    Source component class of which to set the initialization method to
    \bt_p{method}.
@param[in] method
    New initialization method of \bt_p{component_class}.

@retval #BT_COMPONENT_CLASS_SET_METHOD_STATUS_OK
    Success.

@bt_pre_not_null{component_class}
@bt_pre_hot{component_class}
@bt_pre_not_null{method}
*/
extern bt_component_class_set_method_status
bt_component_class_source_set_initialize_method(
		bt_component_class_source *component_class,
		bt_component_class_source_initialize_method method);

/*!
@brief
    Sets the optional initialization method of the \bt_flt_comp_cls
    \bt_p{component_class} to \bt_p{method}.

See the \ref api-comp-cls-dev-meth-init "initialize" method.

@param[in] component_class
    Filter component class of which to set the initialization method to
    \bt_p{method}.
@param[in] method
    New initialization method of \bt_p{component_class}.

@retval #BT_COMPONENT_CLASS_SET_METHOD_STATUS_OK
    Success.

@bt_pre_not_null{component_class}
@bt_pre_hot{component_class}
@bt_pre_not_null{method}
*/
extern bt_component_class_set_method_status
bt_component_class_filter_set_initialize_method(
		bt_component_class_filter *component_class,
		bt_component_class_filter_initialize_method method);

/*!
@brief
    Sets the optional initialization method of the \bt_sink_comp_cls
    \bt_p{component_class} to \bt_p{method}.

See the \ref api-comp-cls-dev-meth-init "initialize" method.

@param[in] component_class
    Sink component class of which to set the initialization method to
    \bt_p{method}.
@param[in] method
    New initialization method of \bt_p{component_class}.

@retval #BT_COMPONENT_CLASS_SET_METHOD_STATUS_OK
    Success.

@bt_pre_not_null{component_class}
@bt_pre_hot{component_class}
@bt_pre_not_null{method}
*/
extern
bt_component_class_set_method_status
bt_component_class_sink_set_initialize_method(
		bt_component_class_sink *component_class,
		bt_component_class_sink_initialize_method method);

/*!
@brief
    Sets the optional "output port connected" method of the
    \bt_src_comp_cls \bt_p{component_class} to \bt_p{method}.

See the
\ref api-comp-cls-dev-meth-oport-connected "output port connected"
method.

@param[in] component_class
    Source component class of which to set the "output port connected"
    method to \bt_p{method}.
@param[in] method
    New "output port connected" method of \bt_p{component_class}.

@retval #BT_COMPONENT_CLASS_SET_METHOD_STATUS_OK
    Success.

@bt_pre_not_null{component_class}
@bt_pre_hot{component_class}
@bt_pre_not_null{method}
*/
extern bt_component_class_set_method_status
bt_component_class_source_set_output_port_connected_method(
		bt_component_class_source *component_class,
		bt_component_class_source_output_port_connected_method method);

/*!
@brief
    Sets the optional "input port connected" method of the
    \bt_flt_comp_cls \bt_p{component_class} to \bt_p{method}.

See the
\ref api-comp-cls-dev-meth-iport-connected "input port connected"
method.

@param[in] component_class
    Filter component class of which to set the "input port connected"
    method to \bt_p{method}.
@param[in] method
    New "input port connected" method of \bt_p{component_class}.

@retval #BT_COMPONENT_CLASS_SET_METHOD_STATUS_OK
    Success.

@bt_pre_not_null{component_class}
@bt_pre_hot{component_class}
@bt_pre_not_null{method}
*/
extern bt_component_class_set_method_status
bt_component_class_filter_set_input_port_connected_method(
		bt_component_class_filter *component_class,
		bt_component_class_filter_input_port_connected_method method);

/*!
@brief
    Sets the optional "output port connected" method of the
    \bt_flt_comp_cls \bt_p{component_class} to \bt_p{method}.

See the
\ref api-comp-cls-dev-meth-oport-connected "output port connected"
method.

@param[in] component_class
    Filter component class of which to set the "output port connected"
    method to \bt_p{method}.
@param[in] method
    New "output port connected" method of \bt_p{component_class}.

@retval #BT_COMPONENT_CLASS_SET_METHOD_STATUS_OK
    Success.

@bt_pre_not_null{component_class}
@bt_pre_hot{component_class}
@bt_pre_not_null{method}
*/
extern bt_component_class_set_method_status
bt_component_class_filter_set_output_port_connected_method(
		bt_component_class_filter *component_class,
		bt_component_class_filter_output_port_connected_method method);

/*!
@brief
    Sets the optional "input port connected" method of the
    \bt_sink_comp_cls \bt_p{component_class} to \bt_p{method}.

See the
\ref api-comp-cls-dev-meth-iport-connected "input port connected"
method.

@param[in] component_class
    Sink component class of which to set the "input port connected"
    method to \bt_p{method}.
@param[in] method
    New "input port connected" method of \bt_p{component_class}.

@retval #BT_COMPONENT_CLASS_SET_METHOD_STATUS_OK
    Success.

@bt_pre_not_null{component_class}
@bt_pre_hot{component_class}
@bt_pre_not_null{method}
*/
extern
bt_component_class_set_method_status
bt_component_class_sink_set_input_port_connected_method(
		bt_component_class_sink *component_class,
		bt_component_class_sink_input_port_connected_method method);

/*!
@brief
    Sets the optional query method of the \bt_src_comp_cls
    \bt_p{component_class} to \bt_p{method}.

See the \ref api-comp-cls-dev-meth-query "query" method.

@param[in] component_class
    Source component class of which to set the query method to
    \bt_p{method}.
@param[in] method
    New query method of \bt_p{component_class}.

@retval #BT_COMPONENT_CLASS_SET_METHOD_STATUS_OK
    Success.

@bt_pre_not_null{component_class}
@bt_pre_hot{component_class}
@bt_pre_not_null{method}
*/
extern bt_component_class_set_method_status
bt_component_class_source_set_query_method(
		bt_component_class_source *component_class,
		bt_component_class_source_query_method method);

/*!
@brief
    Sets the optional query method of the \bt_flt_comp_cls
    \bt_p{component_class} to \bt_p{method}.

See the \ref api-comp-cls-dev-meth-query "query" method.

@param[in] component_class
    Filter component class of which to set the query method to
    \bt_p{method}.
@param[in] method
    New query method of \bt_p{component_class}.

@retval #BT_COMPONENT_CLASS_SET_METHOD_STATUS_OK
    Success.

@bt_pre_not_null{component_class}
@bt_pre_hot{component_class}
@bt_pre_not_null{method}
*/
extern bt_component_class_set_method_status
bt_component_class_filter_set_query_method(
		bt_component_class_filter *component_class,
		bt_component_class_filter_query_method method);

/*!
@brief
    Sets the optional query method of the \bt_sink_comp_cls
    \bt_p{component_class} to \bt_p{method}.

See the \ref api-comp-cls-dev-meth-query "query" method.

@param[in] component_class
    Sink component class of which to set the query method to
    \bt_p{method}.
@param[in] method
    New query method of \bt_p{component_class}.

@retval #BT_COMPONENT_CLASS_SET_METHOD_STATUS_OK
    Success.

@bt_pre_not_null{component_class}
@bt_pre_hot{component_class}
@bt_pre_not_null{method}
*/
extern
bt_component_class_set_method_status
bt_component_class_sink_set_query_method(
		bt_component_class_sink *component_class,
		bt_component_class_sink_query_method method);

/*! @} */

/*!
@name Upcast
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
bt_component_class *bt_component_class_source_as_component_class(
		bt_component_class_source *component_class)
{
	return __BT_UPCAST(bt_component_class, component_class);
}

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
bt_component_class *bt_component_class_filter_as_component_class(
		bt_component_class_filter *component_class)
{
	return __BT_UPCAST(bt_component_class, component_class);
}

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
bt_component_class *bt_component_class_sink_as_component_class(
		bt_component_class_sink *component_class)
{
	return __BT_UPCAST(bt_component_class, component_class);
}

/*! @} */

/*! @} */

#ifdef __cplusplus
}
#endif

#endif /* BABELTRACE2_GRAPH_COMPONENT_CLASS_DEV_H */
