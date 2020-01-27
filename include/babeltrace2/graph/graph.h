#ifndef BABELTRACE2_GRAPH_GRAPH_H
#define BABELTRACE2_GRAPH_GRAPH_H

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
@defgroup api-graph Graph

@brief
    Trace processing graph.

A <strong><em>trace processing graph</em></strong> is a specialized
<a href="https://en.wikipedia.org/wiki/Filter_graph">filter graph</a>
to manipulate one or more traces.

Whereas the nodes of a multimedia filter graph typically exchange
video frames and audio samples, the nodes, or \bt_p_comp, of a trace
processing graph exchange immutable \bt_p_msg containing trace data.

The following illustration shows a basic trace processing graph which
converts many traces to a single log of pretty-printed lines:

@image html basic-convert-graph.png "Basic trace processing graph (conversion graph)."

In the illustrations above, notice that:

- The graph (blue) contains five components.

- Three source components (green) are connected to a single filter
  component (yellow).

  There are five \bt_p_conn, from five \bt_p_oport
  to five \bt_p_iport.

- The filter component is connected to a single sink component (red).

  There's a single connection.

- Source components read external resources while the sink component
  writes to the standard output or to a file.

- There are two source components having the same
  \ref api-tir-comp-cls "class": <code>source.ctf.fs</code>.

- All components are instances of \bt_plugin-provided classes:
  <code>babeltrace2-plugin-ctf.so</code>,
  <code>user-plugin.so</code>,
  <code>babeltrace2-plugin-utils.so</code>, and
  <code>babeltrace2-plugin-text.so</code>.

Of course the topology can be much more complex if needed:

@image html complex-graph.png "Complex trace processing graph."

In a trace processing graph, \bt_p_comp are instances of \bt_p_comp_cls.

A \bt_plugin can provide a component class, but you can also create one
dynamically (see \ref api-comp-cls-dev).

The connections between component ports in a trace processing graph
indicate the needed topology to achieve a trace processing task. That
being said,
\bt_p_msg do not flow within connections. Instead, a source-to-sink
or filter-to-sink connection makes it
possible for a sink component to create a \bt_msg_iter on its end of
the connection (an \bt_iport). In turn, a filter component message
iterator can create other message iterators on one or more of the
component's input ports. For a single connection, there can exist
multiple message iterators.

A trace processing graph is a
\ref api-fund-shared-object "shared object": get a new reference with
bt_graph_get_ref() and put an existing reference with
bt_graph_put_ref().

The type of a trace processing graph is #bt_graph.

Create a default trace processing graph with bt_graph_create(). You need
to pass a \bt_mip (MIP) version number when you call this function.
Then, see
\ref api-graph-lc "Trace processing graph life cycle"
to learn how to add components to the graph, connect their ports, and
run it.

To interrupt a \ref api-graph-lc-run "running" trace processing graph,
either:

- Borrow the graph's default \bt_intr with
  bt_graph_borrow_default_interrupter() and set it with
  bt_interrupter_set().

  When you call bt_graph_create(), the returned trace processing
  graph has a default interrupter.

- Add your own interrupter with bt_graph_add_interrupter() \em before
  you run the graph with bt_graph_run() or bt_graph_run_once().

  Then, set the interrupter with bt_interrupter_set().

<h1>\anchor api-graph-lc Trace processing graph life cycle</h1>

The following diagram shows the life cycle of a trace processing
graph:

@image html graph-lifetime.png "Trace processing graph lifecycle."

After you create a default (empty) trace processing  graph with
bt_graph_create(), there are three steps to make the trace processing
task execute:

-# \ref api-graph-lc-add "Add components" to the graph.
-# \ref api-graph-lc-connect "Connect component ports".
-# \ref api-graph-lc-run "Run the graph".

You can repeat steps 1 and 2 before step 3, as connecting a
component \bt_port can make this \bt_comp create a new port:
you might want to add a component in reaction to this event
(see \ref api-graph-listeners "Listeners").

Steps 1 and 2 constitute the \em configuration phase of the trace
processing graph. The first time you call bt_graph_run() or
bt_graph_run_once() for a given graph (step&nbsp;3), the graph becomes
<em>configured</em>, and it notifies the sink components as such so that
they can create their \bt_p_msg_iter.

Once a trace processing graph becomes configured:

- You cannot \ref api-graph-lc-add "add a component" to it.

- You cannot \ref api-graph-lc-connect "connect component ports".

- Components cannot add ports.

In general, as of \bt_name_version_min_maj:

- You cannot remove a component from a trace processing graph.

- You cannot end (remove) a port \bt_conn.

- Components cannot remove ports.

If \em any error occurs (a function returns a status code which ends
with <code>ERROR</code>) during step 1 or 2, the trace processing
graph is considered <strong>faulty</strong>: you cannot use it anymore.
The only functions you can call with a faulty graph are
bt_graph_get_ref() and bt_graph_put_ref().

<h2>\anchor api-graph-lc-add Add components</h2>

Adding a \bt_comp to a trace processing graph is also known as
<em>instantiating a \bt_comp_cls</em> because the graph must first
create the component from its class before adding it.

Because component and component class C&nbsp;types are
\ref api-fund-c-typing "highly specific", there are six functions
to add a component to a trace processing graph:

<dl>
  <dt>Source component</dt>
  <dd>
    <dl>
      <dt>Without initialization method data</dt>
      <dd>bt_graph_add_source_component()</dd>

      <dt>With initialization method data</dt>
      <dd>bt_graph_add_source_component_with_initialize_method_data()</dd>
    </dl>
  </dd>

  <dt>Filter component</dt>
  <dd>
    <dl>
      <dt>Without initialization method data</dt>
      <dd>bt_graph_add_filter_component()</dd>

      <dt>With initialization method data</dt>
      <dd>bt_graph_add_filter_component_with_initialize_method_data()</dd>
    </dl>
  </dd>

  <dt>Sink component</dt>
  <dd>
    <dl>
      <dt>Without initialization method data</dt>
      <dd>bt_graph_add_sink_component()</dd>

      <dt>With initialization method data</dt>
      <dd>bt_graph_add_sink_component_with_initialize_method_data()</dd>
    </dl>
  </dd>
</dl>

The <code>*_with_initialize_method_data()</code> versions can pass a
custom, \bt_voidp pointer to the component's
\ref api-comp-cls-dev-meth-init "initialization method".
The other versions pass \c NULL as this parameter.

All the functions above accept the same parameters:

@param[in] graph
    Trace processing graph to which to add the created component.
@param[in] component_class
    Class to instantiate within \bt_p{graph}.
@param[in] name
    Unique name of the component to add within \bt_p{graph}.
@param[in] params
    Initialization parameters to use when creating the component.
@param[in] logging_level
    Initial logging level of the component to create.
@param[out] component
    <strong>On success</strong>, \bt_p{*component} is the created
    component.

Once you have the created and added component (\bt_p{*component}),
you can borrow its \bt_p_port to
\ref api-graph-lc-connect "connect them".

You can add as many components as needed to a trace processing graph,
but they must all have a unique name.

<h3>\anchor api-graph-lc-add-ss Add a simple sink component</h3>

To add a very basic \bt_sink_comp which has a single \bt_iport and
creates a single \bt_msg_iter when the trace processing graph becomes
configured, use bt_graph_add_simple_sink_component(). When you call
this function, you pass three user functions:

<dl>
  <dt>\bt_dt_opt Initialization</dt>
  <dd>
    Called when the trace processing graph becomes
    configured and when the sink component created its single
    message iterator.

    You can do an initial action with the message iterator such as
    making it seek a specific point in time or getting messages.
  </dd>

  <dt>Consume</dt>
  <dd>
    Called every time the sink component's
    \ref api-comp-cls-dev-meth-consume "consuming method" is called.

    You can get the next messages from the component's message
    iterator and process them.
  </dd>

  <dt>\bt_dt_opt Finalization</dt>
  <dd>
    Called when the sink component is finalized.
  </dd>
</dl>

The three functions above receive, as their last parameter, the user
data you passed to bt_graph_add_simple_sink_component() .

<h2>\anchor api-graph-lc-connect Connect component ports</h2>

When you \ref api-graph-lc-add "add a component" to a trace processing
graph with one of the <code>bt_graph_add_*_component*()</code>
functions, the function sets \bt_p{*component} to the
created and added component.

With such a \bt_comp, you can borrow one of its \bt_p_port by index or
by name.

Connect two component ports within a trace processing graph with
bt_graph_connect_ports().

After you connect component ports, you can still
\ref api-graph-lc-add "add" more components to the graph, and then
connect more ports.

You don't need to connect all the available ports of a given component,
depending on the trace processing task you need to perform. However,
when you \ref api-graph-lc-run "run" the trace processing graph:

- At least one \bt_oport of any \bt_src_comp must be connected.
- At least one \bt_iport and one output port of any \bt_flt_comp must
  be connected.
- At least one input port of any \bt_sink_comp must be connected.

<h2>\anchor api-graph-lc-run Run</h2>

When you are done configuring a trace processing graph, you can run it
with bt_graph_run().

bt_graph_run() does \em not return until one of:

- All the sink components are ended: bt_graph_run() returns
  #BT_GRAPH_RUN_STATUS_OK.

- One of the sink component returns an error:
  bt_graph_run() returns #BT_GRAPH_RUN_STATUS_ERROR or
  #BT_GRAPH_RUN_STATUS_MEMORY_ERROR.

- One of the sink component returns "try again":
  bt_graph_run() returns #BT_GRAPH_RUN_STATUS_AGAIN.

  In that case, you can call bt_graph_run() again later, usually after
  waiting for some time.

  This feature exists to allow blocking operations within components
  to be postponed until they don't block. The graph user can perform
  other tasks instead of the graph's thread blocking.

- The trace processing graph is interrupted (see
  bt_graph_borrow_default_interrupter() and bt_graph_add_interrupter()):
  bt_graph_run() returns #BT_GRAPH_RUN_STATUS_AGAIN.

  Check the \bt_intr's state with bt_interrupter_is_set() to
  distinguish between a sink component returning "try again" and
  the trace processing graph getting interrupted.

If you want to make a single sink component consume and process a few
\bt_p_msg before controlling the thread again, use bt_graph_run_once()
instead of bt_graph_run().

<h1>Standard \bt_name component classes</h1>

The \bt_name project ships with project \bt_p_plugin which provide
"standard" \bt_p_comp_cls.

Those standard component classes can be useful in many trace processing
graph topologies. They are:

<dl>
  <dt>\c ctf plugin</dt>
  <dd>
    <dl>
      <dt>\c fs source component class</dt>
      <dd>
        Opens a
        <a href="https://diamon.org/ctf/">CTF</a> trace on the file
        system and emits the \bt_p_msg of their data stream files.

        See \bt_man{babeltrace2-source.ctf.fs,7}.
      </dd>

      <dt>\c lttng-live source component class</dt>
      <dd>
        Connects to an <a href="https://lttng.org/">LTTng</a> relay
        daemon and emits the messages of the received CTF data streams.

        See \bt_man{babeltrace2-source.ctf.lttng-live,7}.
      </dd>

      <dt>\c fs sink component class</dt>
      <dd>
        Writes the consumed messages as one or more CTF traces on the
        file system.

        See \bt_man{babeltrace2-sink.ctf.fs,7}.
      </dd>
    </dl>
  </dd>

  <dt>\c lttng-utils plugin</dt>
  <dd>
    <dl>
      <dt>\c debug-info filter component class</dt>
      <dd>
        Adds debugging information to compatible LTTng event messages.

        This component class is only available if the project was
        built with the <code>\-\-enable-debug-info</code>
        configuration option.

        See \bt_man{babeltrace2-filter.lttng-utils.debug-info,7}.
      </dd>
    </dl>
  </dd>

  <dt>\c text plugin</dt>
  <dd>
    <dl>
      <dt>\c dmesg source component class</dt>
      <dd>
        Reads the lines of a Linux kernel ring buffer, that is, the
        output of the
        <a href="http://man7.org/linux/man-pages/man1/dmesg.1.html"><code>dmesg</code></a>
        tool, and emits each line as an \bt_ev_msg.

        See \bt_man{babeltrace2-source.text.dmesg,7}.
      </dd>

      <dt>\c details sink component class</dt>
      <dd>
        Deterministically prints the messages it consumes with details
        to the standard output.

        See \bt_man{babeltrace2-sink.text.details,7}.
      </dd>

      <dt>\c pretty sink component class</dt>
      <dd>
        Pretty-prints the messages it consumes to the standard output or
        to a file.

        This is equivalent to the \c text output format of the
        <a href="https://manpages.debian.org/stretch/babeltrace/babeltrace.1.en.html">Babeltrace&nbsp;1
        command-line tool</a>.

        See \bt_man{babeltrace2-sink.text.pretty,7}.
      </dd>
    </dl>
  </dd>

  <dt>\c utils plugin</dt>
  <dd>
    <dl>
      <dt>\c muxer filter component class</dt>
      <dd>
        Muxes messages by time.

        See \bt_man{babeltrace2-filter.utils.muxer,7}.
      </dd>

      <dt>\c trimmer filter component class</dt>
      <dd>
        Discards all the consumed messages with a time outside a given
        time range, effectively "cutting" \bt_p_stream.

        See \bt_man{babeltrace2-filter.utils.trimmer,7}.
      </dd>

      <dt>\c counter sink component class</dt>
      <dd>
        Prints the number of consumed messages, either once at the end
        or periodically, to the standard output.

        See \bt_man{babeltrace2-sink.utils.counter,7}.
      </dd>

      <dt>\c dummy sink component class</dt>
      <dd>
        Consumes messages and discards them (does nothing with them).

        This is useful for testing and benchmarking a trace processing
        graph application, for example.

        See \bt_man{babeltrace2-sink.utils.dummy,7}.
      </dd>
    </dl>
  </dd>
</dl>

To access such a component class, get its plugin by name with
bt_plugin_find() and then borrow the component class by name
with bt_plugin_borrow_source_component_class_by_name_const(),
bt_plugin_borrow_filter_component_class_by_name_const(), or
bt_plugin_borrow_sink_component_class_by_name_const().

For example:

@code
const bt_plugin *plugin;

if (bt_plugin_find("utils", BT_TRUE, BT_TRUE, BT_TRUE, BT_TRUE, BT_TRUE,
                   &plugin) != BT_PLUGIN_FIND_STATUS_OK) {
    // handle error
}

const bt_component_class_filter *muxer_component_class =
    bt_plugin_borrow_filter_component_class_by_name_const(plugin, "muxer");

if (!muxer_component_class) {
    // handle error
}
@endcode

<h1>\anchor api-graph-listeners Listeners</h1>

A \bt_comp can add a \bt_port:

- At initialization time, that is, when you call one of the
  <code>bt_graph_add_*_component*()</code> functions.

- When one of its ports gets connected, that is, when you call
  bt_graph_connect_ports().

To get notified when any component of a given trace processing
graph adds a port, add one or more "port
added" listeners to the graph with:

- bt_graph_add_source_component_output_port_added_listener()
- bt_graph_add_filter_component_input_port_added_listener()
- bt_graph_add_filter_component_output_port_added_listener()
- bt_graph_add_sink_component_input_port_added_listener()

Within a "port added" listener function, you can
\ref api-graph-lc-add "add more components"
and \ref api-graph-lc-connect "connect more component ports".
*/

/*! @{ */

/*!
@name Type
@{

@typedef struct bt_graph bt_graph;

@brief
    Trace processing graph.

@}
*/

/*!
@name Creation
@{
*/

/*!
@brief
    Creates a default, empty trace processing graph honouring version
    \bt_p{mip_version} of the \bt_mip.

On success, the returned graph contains no components (see
\ref api-graph-lc-add "Add components").

The graph is configured as operating following version
\bt_p{mip_version} of the Message Interchange Protocol, so that
bt_self_component_get_graph_mip_version() returns this version when
components call it.

@note
    As of \bt_name_version_min_maj, the only available MIP version is 0.

The returned graph has a default \bt_intr. Any \bt_comp you add with the
<code>bt_graph_add_*_component*()</code> functions and all their
\bt_p_msg_iter also have this same default interrupter. Borrow the graph's
default interrupter with bt_graph_borrow_default_interrupter().

@param[in] mip_version
    Version of the Message Interchange Protocol to use within the
    graph to create.

@returns
    New trace processing graph reference, or \c NULL on memory error.

@pre
    \bt_p{mip_version} is 0.
*/
extern bt_graph *bt_graph_create(uint64_t mip_version);

/*! @} */

/*!
@name Component adding
@{
*/

/*!
@brief
    Status codes for the
    <code>bt_graph_add_*_component*()</code> functions.
*/
typedef enum bt_graph_add_component_status {
	/*!
	@brief
	    Success.
	*/
	BT_GRAPH_ADD_COMPONENT_STATUS_OK		= __BT_FUNC_STATUS_OK,

	/*!
	@brief
	    Out of memory.
	*/
	BT_GRAPH_ADD_COMPONENT_STATUS_MEMORY_ERROR	= __BT_FUNC_STATUS_MEMORY_ERROR,

	/*!
	@brief
	    Other error.
	*/
	BT_GRAPH_ADD_COMPONENT_STATUS_ERROR		= __BT_FUNC_STATUS_ERROR,
} bt_graph_add_component_status;

/*!
@brief
    Alias of bt_graph_add_source_component_with_initialize_method_data()
    with the \bt_p{initialize_method_data} parameter set to \c NULL.
*/
extern bt_graph_add_component_status
bt_graph_add_source_component(bt_graph *graph,
		const bt_component_class_source *component_class,
		const char *name, const bt_value *params,
		bt_logging_level logging_level,
		const bt_component_source **component);

/*!
@brief
    Creates a \bt_src_comp from the
    \ref api-tir-comp-cls "class" \bt_p{component_class}
    with the initialization parameters \bt_p{params}, the initialization
    user data \bt_p{initialize_method_data}, and the initial
    logging level \bt_p{logging_level}, adds it to the trace processing
    graph \bt_p{graph} with the name \bt_p{name}, and sets
    \bt_p{*component} to the resulting component.

See \ref api-graph-lc-add "Add components" to learn more about adding
components to a trace processing graph.

This function calls the source component's
\ref api-comp-cls-dev-meth-init "initialization method" after
creating it.

The created source component's initialization method receives:

- \bt_p{params} as its own \bt_p{params} parameter (or an empty
  \bt_map_val if \bt_p{params} is \c NULL).

- \bt_p{initialize_method_data} as its own \bt_p{initialize_method_data}
  parameter.

The created source component can get its logging level
(\bt_p{logging_level}) with bt_component_get_logging_level().

@param[in] graph
    Trace processing graph to which to add the created source component.
@param[in] component_class
    Class to instantiate within \bt_p{graph}.
@param[in] name
    Unique name, within \bt_p{graph}, of the component to create.
@param[in] params
    @parblock
    Initialization parameters to use when creating the source component.

    Can be \c NULL, in which case the created source component's
    initialization method receives an empty \bt_map_val as its
    \bt_p{params} parameter.
    @endparblock
@param[in] initialize_method_data
    User data passed as is to the created source component's
    initialization method.
@param[in] logging_level
    Initial logging level of the source component to create.
@param[out] component
    <strong>On success, if not \c NULL</strong>, \bt_p{*component} is
    a \em borrowed reference of the created source component.

@retval #BT_GRAPH_ADD_COMPONENT_STATUS_OK
    Success.
@retval #BT_GRAPH_ADD_COMPONENT_STATUS_MEMORY_ERROR
    Out of memory.
@retval #BT_GRAPH_ADD_COMPONENT_STATUS_ERROR
    Other error, for example, the created source component's
    initialization method failed.

@bt_pre_not_null{graph}
@bt_pre_graph_not_configured{graph}
@bt_pre_graph_not_faulty{graph}
@bt_pre_not_null{component_class}
@bt_pre_not_null{name}
@pre
    No other component within \bt_p{graph} has the name \bt_p{name}.
@pre
    \bt_p{params} is a \bt_map_val (bt_value_is_map() returns #BT_TRUE)
    or is \c NULL.

@bt_post_success_frozen{component_class}
@bt_post_success_frozen{params}
*/
extern bt_graph_add_component_status
bt_graph_add_source_component_with_initialize_method_data(
		bt_graph *graph,
		const bt_component_class_source *component_class,
		const char *name, const bt_value *params,
		void *initialize_method_data, bt_logging_level logging_level,
		const bt_component_source **component);

/*!
@brief
    Alias of bt_graph_add_filter_component_with_initialize_method_data()
    with the \bt_p{initialize_method_data} parameter set to \c NULL.
*/
extern bt_graph_add_component_status
bt_graph_add_filter_component(bt_graph *graph,
		const bt_component_class_filter *component_class,
		const char *name, const bt_value *params,
		bt_logging_level logging_level,
		const bt_component_filter **component);

/*!
@brief
    Creates a \bt_flt_comp from the
    \ref api-tir-comp-cls "class" \bt_p{component_class}
    with the initialization parameters \bt_p{params}, the initialization
    user data \bt_p{initialize_method_data}, and the initial
    logging level \bt_p{logging_level}, adds it to the trace processing
    graph \bt_p{graph} with the name \bt_p{name}, and sets
    \bt_p{*component} to the resulting component.

See \ref api-graph-lc-add "Add components" to learn more about adding
components to a trace processing graph.

This function calls the filter component's
\ref api-comp-cls-dev-meth-init "initialization method" after
creating it.

The created filter component's initialization method receives:

- \bt_p{params} as its own \bt_p{params} parameter (or an empty
  \bt_map_val if \bt_p{params} is \c NULL).

- \bt_p{initialize_method_data} as its own \bt_p{initialize_method_data}
  parameter.

The created filter component can get its logging level
(\bt_p{logging_level}) with bt_component_get_logging_level().

@param[in] graph
    Trace processing graph to which to add the created filter component.
@param[in] component_class
    Class to instantiate within \bt_p{graph}.
@param[in] name
    Unique name, within \bt_p{graph}, of the component to create.
@param[in] params
    @parblock
    Initialization parameters to use when creating the filter component.

    Can be \c NULL, in which case the created filter component's
    initialization method receives an empty \bt_map_val as its
    \bt_p{params} parameter.
    @endparblock
@param[in] initialize_method_data
    User data passed as is to the created filter component's
    initialization method.
@param[in] logging_level
    Initial logging level of the filter component to create.
@param[out] component
    <strong>On success, if not \c NULL</strong>, \bt_p{*component} is
    a \em borrowed reference of the created filter component.

@retval #BT_GRAPH_ADD_COMPONENT_STATUS_OK
    Success.
@retval #BT_GRAPH_ADD_COMPONENT_STATUS_MEMORY_ERROR
    Out of memory.
@retval #BT_GRAPH_ADD_COMPONENT_STATUS_ERROR
    Other error, for example, the created filter component's
    initialization method failed.

@bt_pre_not_null{graph}
@bt_pre_graph_not_configured{graph}
@bt_pre_graph_not_faulty{graph}
@bt_pre_not_null{component_class}
@bt_pre_not_null{name}
@pre
    No other component within \bt_p{graph} has the name \bt_p{name}.
@pre
    \bt_p{params} is a \bt_map_val (bt_value_is_map() returns #BT_TRUE)
    or is \c NULL.

@bt_post_success_frozen{component_class}
@bt_post_success_frozen{params}
*/
extern bt_graph_add_component_status
bt_graph_add_filter_component_with_initialize_method_data(
		bt_graph *graph,
		const bt_component_class_filter *component_class,
		const char *name, const bt_value *params,
		void *initialize_method_data, bt_logging_level logging_level,
		const bt_component_filter **component);

/*!
@brief
    Alias of bt_graph_add_sink_component_with_initialize_method_data()
    with the \bt_p{initialize_method_data} parameter set to \c NULL.
*/
extern bt_graph_add_component_status
bt_graph_add_sink_component(
		bt_graph *graph, const bt_component_class_sink *component_class,
		const char *name, const bt_value *params,
		bt_logging_level logging_level,
		const bt_component_sink **component);

/*!
@brief
    Creates a \bt_sink_comp from the
    \ref api-tir-comp-cls "class" \bt_p{component_class}
    with the initialization parameters \bt_p{params}, the initialization
    user data \bt_p{initialize_method_data}, and the initial
    logging level \bt_p{logging_level}, adds it to the trace processing
    graph \bt_p{graph} with the name \bt_p{name}, and sets
    \bt_p{*component} to the resulting component.

See \ref api-graph-lc-add "Add components" to learn more about adding
components to a trace processing graph.

This function calls the sink component's
\ref api-comp-cls-dev-meth-init "initialization method" after
creating it.

The created sink component's initialization method receives:

- \bt_p{params} as its own \bt_p{params} parameter (or an empty
  \bt_map_val if \bt_p{params} is \c NULL).

- \bt_p{initialize_method_data} as its own \bt_p{initialize_method_data}
  parameter.

The created sink component can get its logging level
(\bt_p{logging_level}) with bt_component_get_logging_level().

@param[in] graph
    Trace processing graph to which to add the created sink component.
@param[in] component_class
    Class to instantiate within \bt_p{graph}.
@param[in] name
    Unique name, within \bt_p{graph}, of the component to create.
@param[in] params
    @parblock
    Initialization parameters to use when creating the sink component.

    Can be \c NULL, in which case the created sink component's
    initialization method receives an empty \bt_map_val as its
    \bt_p{params} parameter.
    @endparblock
@param[in] initialize_method_data
    User data passed as is to the created sink component's
    initialization method.
@param[in] logging_level
    Initial logging level of the sink component to create.
@param[out] component
    <strong>On success, if not \c NULL</strong>, \bt_p{*component} is
    a \em borrowed reference of the created sink component.

@retval #BT_GRAPH_ADD_COMPONENT_STATUS_OK
    Success.
@retval #BT_GRAPH_ADD_COMPONENT_STATUS_MEMORY_ERROR
    Out of memory.
@retval #BT_GRAPH_ADD_COMPONENT_STATUS_ERROR
    Other error, for example, the created sink component's
    initialization method failed.

@bt_pre_not_null{graph}
@bt_pre_graph_not_configured{graph}
@bt_pre_graph_not_faulty{graph}
@bt_pre_not_null{component_class}
@bt_pre_not_null{name}
@pre
    No other component within \bt_p{graph} has the name \bt_p{name}.
@pre
    \bt_p{params} is a \bt_map_val (bt_value_is_map() returns #BT_TRUE)
    or is \c NULL.

@bt_post_success_frozen{component_class}
@bt_post_success_frozen{params}
*/
extern bt_graph_add_component_status
bt_graph_add_sink_component_with_initialize_method_data(
		bt_graph *graph, const bt_component_class_sink *component_class,
		const char *name, const bt_value *params,
		void *initialize_method_data, bt_logging_level logging_level,
		const bt_component_sink **component);

/*! @} */

/*!
@name Simple sink component adding
@{
*/

/*!
@brief
    Status codes for the #bt_graph_simple_sink_component_initialize_func
    type.
*/
typedef enum bt_graph_simple_sink_component_initialize_func_status {
	/*!
	@brief
	    Success.
	*/
	BT_GRAPH_SIMPLE_SINK_COMPONENT_INITIALIZE_FUNC_STATUS_OK		= __BT_FUNC_STATUS_OK,

	/*!
	@brief
	    Out of memory.
	*/
	BT_GRAPH_SIMPLE_SINK_COMPONENT_INITIALIZE_FUNC_STATUS_MEMORY_ERROR	= __BT_FUNC_STATUS_MEMORY_ERROR,

	/*!
	@brief
	    Other error.
	*/
	BT_GRAPH_SIMPLE_SINK_COMPONENT_INITIALIZE_FUNC_STATUS_ERROR		= __BT_FUNC_STATUS_ERROR,
} bt_graph_simple_sink_component_initialize_func_status;

/*!
@brief
    User initialization function for
    bt_graph_add_simple_sink_component().

Such an initialization function is called when the trace processing
graph becomes configured and when the simple sink component has created
its single \bt_msg_iter. This occurs the first time you call
bt_graph_run() or bt_graph_run_once() on the trace processing graph.

See \ref api-graph-lc-add-ss "Add a simple sink component" to learn more
about adding a simple component to a trace processing graph.

@param[in] message_iterator
    @parblock
    Simple sink component's upstream message iterator.

    This user function is free to get the message iterator's next
    message or to make it seek.
    @endparblock
@param[in] user_data
    User data, as passed as the \bt_p{user_data} parameter of
    bt_graph_add_simple_sink_component().

@retval #BT_GRAPH_SIMPLE_SINK_COMPONENT_INITIALIZE_FUNC_STATUS_OK
    Success.
@retval #BT_GRAPH_SIMPLE_SINK_COMPONENT_INITIALIZE_FUNC_STATUS_MEMORY_ERROR
    Out of memory.
@retval #BT_GRAPH_SIMPLE_SINK_COMPONENT_INITIALIZE_FUNC_STATUS_ERROR
    Other error.

@bt_pre_not_null{message_iterator}

@post
    The reference count of \bt_p{message_iterator} is not changed.

@sa bt_graph_add_simple_sink_component() &mdash;
    Creates and adds a simple sink component to a trace processing
    graph.
*/
typedef bt_graph_simple_sink_component_initialize_func_status
(*bt_graph_simple_sink_component_initialize_func)(
		bt_message_iterator *message_iterator,
		void *user_data);

/*!
@brief
    Status codes for the #bt_graph_simple_sink_component_consume_func
    type.
*/
typedef enum bt_graph_simple_sink_component_consume_func_status {
	/*!
	@brief
	    Success.
	*/
	BT_GRAPH_SIMPLE_SINK_COMPONENT_CONSUME_FUNC_STATUS_OK			= __BT_FUNC_STATUS_OK,

	/*!
	@brief
	    End of processing.
	*/
	BT_GRAPH_SIMPLE_SINK_COMPONENT_CONSUME_FUNC_STATUS_END			= __BT_FUNC_STATUS_END,

	/*!
	@brief
	    Try again.
	*/
	BT_GRAPH_SIMPLE_SINK_COMPONENT_CONSUME_FUNC_STATUS_AGAIN		= __BT_FUNC_STATUS_AGAIN,

	/*!
	@brief
	    Out of memory.
	*/
	BT_GRAPH_SIMPLE_SINK_COMPONENT_CONSUME_FUNC_STATUS_MEMORY_ERROR		= __BT_FUNC_STATUS_MEMORY_ERROR,

	/*!
	@brief
	    Other error.
	*/
	BT_GRAPH_SIMPLE_SINK_COMPONENT_CONSUME_FUNC_STATUS_ERROR		= __BT_FUNC_STATUS_ERROR,
} bt_graph_simple_sink_component_consume_func_status;

/*!
@brief
    User consuming function for bt_graph_add_simple_sink_component().

Such a consuming function is called when the simple sink component's own
\ref api-comp-cls-dev-meth-consume "consuming method" is called. This
occurs in a loop within bt_graph_run() or when it's this sink
component's turn to consume in
bt_graph_run_once().

See \ref api-graph-lc-add-ss "Add a simple sink component" to learn more
about adding a simple component to a trace processing graph.

If you are not done consuming messages, return
#BT_GRAPH_SIMPLE_SINK_COMPONENT_CONSUME_FUNC_STATUS_OK.

If you are done consuming messages, return
#BT_GRAPH_SIMPLE_SINK_COMPONENT_CONSUME_FUNC_STATUS_END.

If you wish to avoid a blocking operation and make
bt_graph_run() or bt_graph_run_once() aware, return
#BT_GRAPH_SIMPLE_SINK_COMPONENT_CONSUME_FUNC_STATUS_AGAIN.

@param[in] message_iterator
    @parblock
    Simple sink component's upstream message iterator.

    This user function is free to get the message iterator's next
    message or to make it seek.
    @endparblock
@param[in] user_data
    User data, as passed as the \bt_p{user_data} parameter of
    bt_graph_add_simple_sink_component().

@retval #BT_GRAPH_SIMPLE_SINK_COMPONENT_CONSUME_FUNC_STATUS_OK
    Success.
@retval #BT_GRAPH_SIMPLE_SINK_COMPONENT_CONSUME_FUNC_STATUS_END
    End of processing.
@retval #BT_GRAPH_SIMPLE_SINK_COMPONENT_CONSUME_FUNC_STATUS_AGAIN
    Try again.
@retval #BT_GRAPH_SIMPLE_SINK_COMPONENT_CONSUME_FUNC_STATUS_MEMORY_ERROR
    Out of memory.
@retval #BT_GRAPH_SIMPLE_SINK_COMPONENT_CONSUME_FUNC_STATUS_ERROR
    Other error.

@bt_pre_not_null{message_iterator}

@post
    The reference count of \bt_p{message_iterator} is not changed.

@sa bt_graph_add_simple_sink_component() &mdash;
    Creates and adds a simple sink component to a trace processing
    graph.
*/
typedef bt_graph_simple_sink_component_consume_func_status
(*bt_graph_simple_sink_component_consume_func)(
		bt_message_iterator *message_iterator,
		void *user_data);

/*!
@brief
    User finalization function for bt_graph_add_simple_sink_component().

Such a finalization function is called when the simple sink component
is finalized. This occurs when the trace processing graph is destroyed
(you put its last strong \ref api-fund-shared-object "reference"
with bt_graph_put_ref()).

See \ref api-graph-lc-add-ss "Add a simple sink component" to learn more
about adding a simple component to a trace processing graph.

@param[in] user_data
    User data, as passed as the \bt_p{user_data} parameter of
    bt_graph_add_simple_sink_component().

@bt_post_no_error

@sa bt_graph_add_simple_sink_component() &mdash;
    Creates and adds a simple sink component to a trace processing
    graph.
*/
typedef void (*bt_graph_simple_sink_component_finalize_func)(void *user_data);

/*!
@brief
    Creates a simple \bt_sink_comp, adds it to the trace processing
    graph \bt_p{graph} with the name \bt_p{name}, and sets
    \bt_p{*component} to the resulting component.

See \ref api-graph-lc-add-ss "Add a simple sink component" to learn more
about adding a simple component to a trace processing graph.

\bt_p{initialize_func} (if not \c NULL), \bt_p{consume_func},
and \bt_p{finalize_func} (if not \c NULL) receive \bt_p{user_data}
as their own \bt_p{user_data} parameter.

@param[in] graph
    Trace processing graph to which to add the created simple sink
    component.
@param[in] name
    Unique name, within \bt_p{graph}, of the component to create.
@param[in] initialize_func
    @parblock
    User initialization function.

    Can be \c NULL.
    @endparblock
@param[in] consume_func
    User consuming function.
@param[in] finalize_func
    @parblock
    User finalization function.

    Can be \c NULL.
    @endparblock
@param[in] user_data
    User data to pass as the \bt_p{user_data} parameter of
    \bt_p{initialize_func}, \bt_p{consume_func}, and
    \bt_p{finalize_func}.
@param[out] component
    <strong>On success, if not \c NULL</strong>, \bt_p{*component} is
    a \em borrowed reference of the created simple sink component.

@retval #BT_GRAPH_ADD_COMPONENT_STATUS_OK
    Success.
@retval #BT_GRAPH_ADD_COMPONENT_STATUS_MEMORY_ERROR
    Out of memory.
@retval #BT_GRAPH_ADD_COMPONENT_STATUS_ERROR
    Other error, for example, the created sink component's
    \ref api-comp-cls-dev-meth-init "initialization method" failed.

@bt_pre_not_null{graph}
@bt_pre_graph_not_configured{graph}
@bt_pre_graph_not_faulty{graph}
@bt_pre_not_null{name}
@pre
    No other component within \bt_p{graph} has the name \bt_p{name}.
@bt_pre_not_null{consume_func}
*/
extern bt_graph_add_component_status
bt_graph_add_simple_sink_component(bt_graph *graph, const char *name,
		bt_graph_simple_sink_component_initialize_func initialize_func,
		bt_graph_simple_sink_component_consume_func consume_func,
		bt_graph_simple_sink_component_finalize_func finalize_func,
		void *user_data, const bt_component_sink **component);

/*! @} */

/*!
@name Component port connection
@{
*/

/*!
@brief
    Status codes for bt_graph_connect_ports().
*/
typedef enum bt_graph_connect_ports_status {
	/*!
	@brief
	    Success.
	*/
	BT_GRAPH_CONNECT_PORTS_STATUS_OK		= __BT_FUNC_STATUS_OK,

	/*!
	@brief
	    Out of memory.
	*/
	BT_GRAPH_CONNECT_PORTS_STATUS_MEMORY_ERROR	= __BT_FUNC_STATUS_MEMORY_ERROR,

	/*!
	@brief
	    Other error.
	*/
	BT_GRAPH_CONNECT_PORTS_STATUS_ERROR		= __BT_FUNC_STATUS_ERROR,
} bt_graph_connect_ports_status;

/*!
@brief
    Connects the \bt_oport \bt_p{upstream_port} to the \bt_iport
    \bt_p{downstream_port} within the trace processing graph
    \bt_p{graph}, and sets \bt_p{*connection} to the resulting
    \bt_conn.

See \ref api-graph-lc-connect "Connect component ports" to learn more
about connecting ports within a trace processing graph.

Both \bt_p{upstream_port} and \bt_p{downstream_port} must be
unconnected (bt_port_is_connected() returns #BT_FALSE) when you call
this function.

@param[in] graph
    Trace processing graph containing the \bt_p_comp to which belong
    \bt_p{upstream_port} and \bt_p{downstream_port}.
@param[in] upstream_port
    \bt_c_oport to connect to \bt_p{downstream_port}.
@param[in] downstream_port
    \bt_c_iport to connect to \bt_p{upstream_port}.
@param[in] connection
    <strong>On success, if not \c NULL</strong>, \bt_p{*connection} is
    a \em borrowed reference of the resulting \bt_conn.

@retval #BT_GRAPH_CONNECT_PORTS_STATUS_OK
    Success.
@retval #BT_GRAPH_CONNECT_PORTS_STATUS_MEMORY_ERROR
    Out of memory.
@retval #BT_GRAPH_CONNECT_PORTS_STATUS_ERROR
    Other error.

@bt_pre_not_null{graph}
@bt_pre_graph_not_configured{graph}
@bt_pre_graph_not_faulty{graph}
@bt_pre_not_null{upstream_port}
@pre
    \bt_p{graph} contains the component of \bt_p{upstream_port},
    as returned by bt_port_borrow_component_const().
@pre
    \bt_p{upstream_port} is unconnected
    (bt_port_is_connected() returns #BT_FALSE).
@bt_pre_not_null{downstream_port}
@pre
    \bt_p{graph} contains the component of \bt_p{downstream_port},
    as returned by bt_port_borrow_component_const().
@pre
    \bt_p{downstream_port} is unconnected
    (bt_port_is_connected() returns #BT_FALSE).
@pre
    Connecting \bt_p{upstream_port} to \bt_p{downstream_port} does not
    form a connection cycle within \bt_p{graph}.
*/
extern bt_graph_connect_ports_status bt_graph_connect_ports(bt_graph *graph,
		const bt_port_output *upstream_port,
		const bt_port_input *downstream_port,
		const bt_connection **connection);

/*! @} */

/*!
@name Running
@{
*/

/*!
@brief
    Status codes for bt_graph_run().
*/
typedef enum bt_graph_run_status {
	/*!
	@brief
	    Success.
	*/
	BT_GRAPH_RUN_STATUS_OK			= __BT_FUNC_STATUS_OK,

	/*!
	@brief
	    Try again.
	*/
	BT_GRAPH_RUN_STATUS_AGAIN		= __BT_FUNC_STATUS_AGAIN,

	/*!
	@brief
	    Out of memory.
	*/
	BT_GRAPH_RUN_STATUS_MEMORY_ERROR	= __BT_FUNC_STATUS_MEMORY_ERROR,

	/*!
	@brief
	    Other error.
	*/
	BT_GRAPH_RUN_STATUS_ERROR		= __BT_FUNC_STATUS_ERROR,
} bt_graph_run_status;

/*!
@brief
    Runs the trace processing graph \bt_p{graph}, calling each
    \bt_sink_comp's
    \ref api-comp-cls-dev-meth-consume "consuming method" in a round
    robin fashion until they are all done consuming or an exception
    occurs.

See \ref api-graph-lc-run "Run" to learn more about running a trace
processing graph.

This function does \em not return until one of:

- All the sink components are ended, in which case this function
  returns #BT_GRAPH_RUN_STATUS_OK.

- One of the sink component returns an error, in which case this
  function returns #BT_GRAPH_RUN_STATUS_ERROR or
  #BT_GRAPH_RUN_STATUS_MEMORY_ERROR.

- One of the sink component returns "try again", in which case
  this function returns #BT_GRAPH_RUN_STATUS_AGAIN.

  In that case, you can call this function again later, usually after
  waiting for some time.

  This feature exists to allow blocking operations within components
  to be postponed until they don't block. The graph user can perform
  other tasks instead of the graph's thread blocking.

- \bt_p{graph} is interrupted (see bt_graph_borrow_default_interrupter()
  and bt_graph_add_interrupter()), in which case this function returns
  #BT_GRAPH_RUN_STATUS_AGAIN.

  Check the \bt_intr's state with bt_interrupter_is_set() to
  distinguish between a sink component returning "try again" and
  \bt_p{graph} getting interrupted.

To make a single sink component consume, then get the thread's control
back, use bt_graph_run_once().

When you call this function or bt_graph_run_once() for the first time,
\bt_p{graph} becomes <em>configured</em>. See
\ref api-graph-lc "Trace processing graph life cycle"
to learn what happens when a trace processing graph becomes configured,
and what you can and cannot do with a configured graph.

@param[in] graph
    Trace processing graph to run.

@retval #BT_GRAPH_RUN_STATUS_OK
    Success.
@retval #BT_GRAPH_RUN_STATUS_AGAIN
    Try again.
@retval #BT_GRAPH_RUN_STATUS_MEMORY_ERROR
    Out of memory.
@retval #BT_GRAPH_RUN_STATUS_ERROR
    Other error.

@bt_pre_not_null{graph}
@bt_pre_graph_not_faulty{graph}
@pre
    For each \bt_src_comp within \bt_p{graph}, at least one \bt_oport
    is connected.
@pre
    For each \bt_flt_comp within \bt_p{graph}, at least one \bt_iport
    and one \bt_iport are connected.
@pre
    For each \bt_sink_comp within \bt_p{graph}, at least one \bt_iport
    is connected.
@pre
    \bt_p{graph} contains at least one sink component.

@sa bt_graph_run_once() &mdash;
    Calls a single trace processing graph's sink component's consuming
    method once.
*/
extern bt_graph_run_status bt_graph_run(bt_graph *graph);

/*!
@brief
    Status codes for bt_graph_run().
*/
typedef enum bt_graph_run_once_status {
	/*!
	@brief
	    Success.
	*/
	BT_GRAPH_RUN_ONCE_STATUS_OK		= __BT_FUNC_STATUS_OK,

	/*!
	@brief
	    All sink components are finished processing.
	*/
	BT_GRAPH_RUN_ONCE_STATUS_END		= __BT_FUNC_STATUS_END,

	/*!
	@brief
	    Try again.
	*/
	BT_GRAPH_RUN_ONCE_STATUS_AGAIN		= __BT_FUNC_STATUS_AGAIN,

	/*!
	@brief
	    Out of memory.
	*/
	BT_GRAPH_RUN_ONCE_STATUS_MEMORY_ERROR	= __BT_FUNC_STATUS_MEMORY_ERROR,

	/*!
	@brief
	    Other error.
	*/
	BT_GRAPH_RUN_ONCE_STATUS_ERROR		= __BT_FUNC_STATUS_ERROR,
} bt_graph_run_once_status;

/*!
@brief
    Calls the \ref api-comp-cls-dev-meth-consume "consuming method" of
    the next non-ended \bt_sink_comp to make consume within the trace
    processing graph \bt_p{graph}.

See \ref api-graph-lc-run "Run" to learn more about running a trace
processing graph.

This function makes the \em next non-ended sink component consume. For
example, if \bt_p{graph} has two non-ended sink components A and B:

- Calling bt_graph_run_once() makes sink component A consume.
- Calling bt_graph_run_once() again makes sink component B consume.
- Calling bt_graph_run_once() again makes sink component A consume.
- ...

Considering this, if \bt_p{graph} contains a single non-ended sink
component, this function \em always makes this sink component consume.

If the sink component's consuming method:

<dl>
  <dt>Succeeds</dt>
  <dd>This function returns #BT_GRAPH_RUN_ONCE_STATUS_OK.</dd>

  <dt>Returns "try again"</dt>
  <dd>This function returns #BT_GRAPH_RUN_ONCE_STATUS_AGAIN.</dd>

  <dt>Fails</dt>
  <dd>
    This function returns #BT_GRAPH_RUN_ONCE_STATUS_MEMORY_ERROR
    or #BT_GRAPH_RUN_ONCE_STATUS_ERROR.
  </dd>
</dl>

When all the sink components of \bt_p{graph} are finished processing
(ended), this function returns #BT_GRAPH_RUN_ONCE_STATUS_END.

bt_graph_run() calls this function in a loop until are the sink
components are ended or an exception occurs.

When you call this function or bt_graph_run() for the first time,
\bt_p{graph} becomes <em>configured</em>. See
\ref api-graph-lc "Trace processing graph life cycle"
to learn what happens when a trace processing graph becomes configured,
and what you can and cannot do with a configured graph.

@param[in] graph
    Trace processing graph of which to make the next sink component
    consume.

@retval #BT_GRAPH_RUN_ONCE_STATUS_OK
    Success.
@retval #BT_GRAPH_RUN_ONCE_STATUS_END
    All sink components are finished processing.
@retval #BT_GRAPH_RUN_ONCE_STATUS_AGAIN
    Try again.
@retval #BT_GRAPH_RUN_ONCE_STATUS_MEMORY_ERROR
    Out of memory.
@retval #BT_GRAPH_RUN_ONCE_STATUS_ERROR
    Other error.

@bt_pre_not_null{graph}
@bt_pre_graph_not_faulty{graph}

@sa bt_graph_run() &mdash;
    Runs a trace processing graph, making all its sink components
    consume in a round robin fashion.
*/
extern bt_graph_run_once_status bt_graph_run_once(bt_graph *graph);

/*! @} */

/*!
@name Interruption
@{
*/

/*!
@brief
    Status codes for bt_graph_add_interrupter().
*/
typedef enum bt_graph_add_interrupter_status {
	/*!
	@brief
	    Success.
	*/
	BT_GRAPH_ADD_INTERRUPTER_STATUS_OK		= __BT_FUNC_STATUS_OK,

	/*!
	@brief
	    Out of memory.
	*/
	BT_GRAPH_ADD_INTERRUPTER_STATUS_MEMORY_ERROR	= __BT_FUNC_STATUS_MEMORY_ERROR,
} bt_graph_add_interrupter_status;

/*!
@brief
    Adds the \bt_intr \bt_p{interrupter} to all the current and future
    \bt_p_sink_comp and \bt_p_msg_iter of the trace processing graph
    \bt_p{graph}, as well as to the graph itself.

Sink components can check whether or not they are interrupted
with bt_self_component_sink_is_interrupted().

Message iterators can check whether or not they are interrupted
with bt_self_message_iterator_is_interrupted().

The bt_graph_run() loop intermittently checks whether or not any of the
graph's interrupters is set. If so, bt_graph_run() returns
#BT_GRAPH_RUN_STATUS_AGAIN.

@note
    @parblock
    bt_graph_create() returns a trace processing graph which comes
    with its own <em>default interrupter</em>.

    Instead of adding your own interrupter to \bt_p{graph}, you can
    set its default interrupter with

    @code
    bt_interrupter_set(bt_graph_borrow_default_interrupter());
    @endcode
    @endparblock

@param[in] graph
    Trace processing graph to which to add \bt_p{interrupter}.
@param[in] interrupter
    Interrupter to add to \bt_p{graph}.

@retval #BT_GRAPH_ADD_INTERRUPTER_STATUS_OK
    Success.
@retval #BT_GRAPH_ADD_INTERRUPTER_STATUS_MEMORY_ERROR
    Out of memory.

@bt_pre_not_null{graph}
@bt_pre_graph_not_faulty{graph}
@bt_pre_not_null{interrupter}

@sa bt_graph_borrow_default_interrupter() &mdash;
    Borrows the default interrupter from a trace processing graph.
*/
extern bt_graph_add_interrupter_status bt_graph_add_interrupter(bt_graph *graph,
		const bt_interrupter *interrupter);

/*!
@brief
    Borrows the default \bt_intr from the trace processing graph
    \bt_p{graph}.

@param[in] graph
    Trace processing graph from which to borrow the default interrupter.

@returns
    @parblock
    \em Borrowed reference of the default interrupter of \bt_p{graph}.

    The returned pointer remains valid as long as \bt_p{graph} exists.
    @endparblock

@bt_pre_not_null{graph}

@sa bt_graph_add_interrupter() &mdash;
    Adds an interrupter to a trace processing graph.
*/
extern bt_interrupter *bt_graph_borrow_default_interrupter(bt_graph *graph);

/*! @} */

/*!
@name Listeners
@{
*/

/*!
@brief
    Status codes for the
    <code>bt_graph_add_*_component_*_port_added_listener()</code>
    functions.
*/
typedef enum bt_graph_add_listener_status {
	/*!
	@brief
	    Success.
	*/
	BT_GRAPH_ADD_LISTENER_STATUS_OK			= __BT_FUNC_STATUS_OK,

	/*!
	@brief
	    Out of memory.
	*/
	BT_GRAPH_ADD_LISTENER_STATUS_MEMORY_ERROR	= __BT_FUNC_STATUS_MEMORY_ERROR,
} bt_graph_add_listener_status;

/*!
@brief
    Status codes for the
    <code>bt_graph_*_component_*_port_added_listener_func()</code>
    types.
*/
typedef enum bt_graph_listener_func_status {
	/*!
	@brief
	    Success.
	*/
	BT_GRAPH_LISTENER_FUNC_STATUS_OK		= __BT_FUNC_STATUS_OK,

	/*!
	@brief
	    Out of memory.
	*/
	BT_GRAPH_LISTENER_FUNC_STATUS_MEMORY_ERROR	= __BT_FUNC_STATUS_MEMORY_ERROR,

	/*!
	@brief
	    Other error.
	*/
	BT_GRAPH_LISTENER_FUNC_STATUS_ERROR		= __BT_FUNC_STATUS_ERROR,
} bt_graph_listener_func_status;

/*!
@brief
    User function for
    bt_graph_add_filter_component_input_port_added_listener().

Such a function is called whenever a \bt_flt_comp within a trace
processing graph adds an \bt_iport during the graph
\ref api-graph-lc "configuration" phase.

See \ref api-graph-listeners "Listeners" to learn more.

@param[in] component
    Filter component which added \bt_p{port}.
@param[in] port
    Input port added by \bt_p{component}.
@param[in] user_data
    User data, as passed as the \bt_p{user_data} parameter of
    bt_graph_add_filter_component_input_port_added_listener().

@retval #BT_GRAPH_LISTENER_FUNC_STATUS_OK
    Success.
@retval #BT_GRAPH_LISTENER_FUNC_STATUS_MEMORY_ERROR
    Out of memory.
@retval #BT_GRAPH_LISTENER_FUNC_STATUS_ERROR
    Other error.

@bt_pre_not_null{component}
@bt_pre_not_null{port}

@sa bt_graph_add_filter_component_input_port_added_listener() &mdash;
    Adds a "filter component input port added" listener to a trace
    processing graph.
*/
typedef bt_graph_listener_func_status
(*bt_graph_filter_component_input_port_added_listener_func)(
		const bt_component_filter *component,
		const bt_port_input *port, void *user_data);

/*!
@brief
    Adds a \"\bt_flt_comp \bt_iport added\" listener to the trace
    processing graph \bt_p{graph}.

Once this function returns, \bt_p{user_func} is called whenever a
filter component adds an input port within \bt_p{graph}.

See \ref api-graph-listeners "Listeners" to learn more.

@param[in] graph
    Trace processing graph to add the "filter component input port
    added" listener to.
@param[in] user_func
    User function of the "filter component input port added" listener to
    add to \bt_p{graph}.
@param[in] user_data
    User data to pass as the \bt_p{user_data} parameter of
    \bt_p{user_func}.
@param[out] listener_id
    <strong>On success and if not \c NULL</strong>, \bt_p{*listener_id}
    is the ID of the added listener within \bt_p{graph}.

@retval #BT_GRAPH_ADD_LISTENER_STATUS_OK
    Success.
@retval #BT_GRAPH_ADD_LISTENER_STATUS_MEMORY_ERROR
    Out of memory.

@bt_pre_not_null{graph}
@bt_pre_graph_not_faulty{graph}
@bt_pre_not_null{user_func}
*/
extern bt_graph_add_listener_status
bt_graph_add_filter_component_input_port_added_listener(
		bt_graph *graph,
		bt_graph_filter_component_input_port_added_listener_func user_func,
		void *user_data, bt_listener_id *listener_id);

/*!
@brief
    User function for
    bt_graph_add_sink_component_input_port_added_listener().

Such a function is called whenever a \bt_sink_comp within a trace
processing graph adds an \bt_iport during the graph
\ref api-graph-lc "configuration" phase.

See \ref api-graph-listeners "Listeners" to learn more.

@param[in] component
    Filter component which added \bt_p{port}.
@param[in] port
    Input port added by \bt_p{component}.
@param[in] user_data
    User data, as passed as the \bt_p{user_data} parameter of
    bt_graph_add_sink_component_input_port_added_listener().

@retval #BT_GRAPH_LISTENER_FUNC_STATUS_OK
    Success.
@retval #BT_GRAPH_LISTENER_FUNC_STATUS_MEMORY_ERROR
    Out of memory.
@retval #BT_GRAPH_LISTENER_FUNC_STATUS_ERROR
    Other error.

@bt_pre_not_null{component}
@bt_pre_not_null{port}

@sa bt_graph_add_sink_component_input_port_added_listener() &mdash;
    Adds a "sink component input port added" listener to a trace
    processing graph.
*/
typedef bt_graph_listener_func_status
(*bt_graph_sink_component_input_port_added_listener_func)(
		const bt_component_sink *component,
		const bt_port_input *port, void *user_data);

/*!
@brief
    Adds a \"\bt_sink_comp \bt_iport added\" listener to the trace
    processing graph \bt_p{graph}.

Once this function returns, \bt_p{user_func} is called whenever a
sink component adds an input port within \bt_p{graph}.

See \ref api-graph-listeners "Listeners" to learn more.

@param[in] graph
    Trace processing graph to add the "sink component input port
    added" listener to.
@param[in] user_func
    User function of the "sink component input port added" listener to
    add to \bt_p{graph}.
@param[in] user_data
    User data to pass as the \bt_p{user_data} parameter of
    \bt_p{user_func}.
@param[out] listener_id
    <strong>On success and if not \c NULL</strong>, \bt_p{*listener_id}
    is the ID of the added listener within \bt_p{graph}.

@retval #BT_GRAPH_ADD_LISTENER_STATUS_OK
    Success.
@retval #BT_GRAPH_ADD_LISTENER_STATUS_MEMORY_ERROR
    Out of memory.

@bt_pre_not_null{graph}
@bt_pre_graph_not_faulty{graph}
@bt_pre_not_null{user_func}
*/
extern bt_graph_add_listener_status
bt_graph_add_sink_component_input_port_added_listener(
		bt_graph *graph,
		bt_graph_sink_component_input_port_added_listener_func user_func,
		void *user_data, bt_listener_id *listener_id);

/*!
@brief
    User function for
    bt_graph_add_source_component_output_port_added_listener().

Such a function is called whenever a \bt_src_comp within a trace
processing graph adds an \bt_oport during the graph
\ref api-graph-lc "configuration" phase.

See \ref api-graph-listeners "Listeners" to learn more.

@param[in] component
    Filter component which added \bt_p{port}.
@param[in] port
    Input port added by \bt_p{component}.
@param[in] user_data
    User data, as passed as the \bt_p{user_data} parameter of
    bt_graph_add_source_component_output_port_added_listener().

@retval #BT_GRAPH_LISTENER_FUNC_STATUS_OK
    Success.
@retval #BT_GRAPH_LISTENER_FUNC_STATUS_MEMORY_ERROR
    Out of memory.
@retval #BT_GRAPH_LISTENER_FUNC_STATUS_ERROR
    Other error.

@bt_pre_not_null{component}
@bt_pre_not_null{port}

@sa bt_graph_add_source_component_output_port_added_listener() &mdash;
    Adds a "source component output port added" listener to a trace
    processing graph.
*/
typedef bt_graph_listener_func_status
(*bt_graph_source_component_output_port_added_listener_func)(
		const bt_component_source *component,
		const bt_port_output *port, void *user_data);

/*!
@brief
    Adds a \"\bt_src_comp \bt_oport added\" listener to the trace
    processing graph \bt_p{graph}.

Once this function returns, \bt_p{user_func} is called whenever a
source component adds an output port within \bt_p{graph}.

See \ref api-graph-listeners "Listeners" to learn more.

@param[in] graph
    Trace processing graph to add the "source component output port
    added" listener to.
@param[in] user_func
    User function of the "source component output port added" listener
    to add to \bt_p{graph}.
@param[in] user_data
    User data to pass as the \bt_p{user_data} parameter of
    \bt_p{user_func}.
@param[out] listener_id
    <strong>On success and if not \c NULL</strong>, \bt_p{*listener_id}
    is the ID of the added listener within \bt_p{graph}.

@retval #BT_GRAPH_ADD_LISTENER_STATUS_OK
    Success.
@retval #BT_GRAPH_ADD_LISTENER_STATUS_MEMORY_ERROR
    Out of memory.

@bt_pre_not_null{graph}
@bt_pre_graph_not_faulty{graph}
@bt_pre_not_null{user_func}
*/
extern bt_graph_add_listener_status
bt_graph_add_source_component_output_port_added_listener(
		bt_graph *graph,
		bt_graph_source_component_output_port_added_listener_func user_func,
		void *user_data, bt_listener_id *listener_id);

/*!
@brief
    User function for
    bt_graph_add_filter_component_output_port_added_listener().

Such a function is called whenever a \bt_flt_comp within a trace
processing graph adds an \bt_oport during the graph
\ref api-graph-lc "configuration" phase.

See \ref api-graph-listeners "Listeners" to learn more.

@param[in] component
    Filter component which added \bt_p{port}.
@param[in] port
    Input port added by \bt_p{component}.
@param[in] user_data
    User data, as passed as the \bt_p{user_data} parameter of
    bt_graph_add_filter_component_output_port_added_listener().

@retval #BT_GRAPH_LISTENER_FUNC_STATUS_OK
    Success.
@retval #BT_GRAPH_LISTENER_FUNC_STATUS_MEMORY_ERROR
    Out of memory.
@retval #BT_GRAPH_LISTENER_FUNC_STATUS_ERROR
    Other error.

@bt_pre_not_null{component}
@bt_pre_not_null{port}

@sa bt_graph_add_filter_component_output_port_added_listener() &mdash;
    Adds a "filter component output port added" listener to a trace
    processing graph.
*/
typedef bt_graph_listener_func_status
(*bt_graph_filter_component_output_port_added_listener_func)(
		const bt_component_filter *component,
		const bt_port_output *port, void *user_data);

/*!
@brief
    Adds a \"\bt_flt_comp \bt_oport added\" listener to the trace
    processing graph \bt_p{graph}.

Once this function returns, \bt_p{user_func} is called whenever a
filter component adds an output port within \bt_p{graph}.

See \ref api-graph-listeners "Listeners" to learn more.

@param[in] graph
    Trace processing graph to add the "filter component output port
    added" listener to.
@param[in] user_func
    User function of the "filter component output port added" listener
    to add to \bt_p{graph}.
@param[in] user_data
    User data to pass as the \bt_p{user_data} parameter of
    \bt_p{user_func}.
@param[out] listener_id
    <strong>On success and if not \c NULL</strong>, \bt_p{*listener_id}
    is the ID of the added listener within \bt_p{graph}.

@retval #BT_GRAPH_ADD_LISTENER_STATUS_OK
    Success.
@retval #BT_GRAPH_ADD_LISTENER_STATUS_MEMORY_ERROR
    Out of memory.

@bt_pre_not_null{graph}
@bt_pre_graph_not_faulty{graph}
@bt_pre_not_null{user_func}
*/
extern bt_graph_add_listener_status
bt_graph_add_filter_component_output_port_added_listener(
		bt_graph *graph,
		bt_graph_filter_component_output_port_added_listener_func user_func,
		void *user_data, bt_listener_id *listener_id);

/*! @} */

/*!
@name Reference count
@{
*/

/*!
@brief
    Increments the \ref api-fund-shared-object "reference count" of
    the trace processing graph \bt_p{graph}.

@param[in] graph
    @parblock
    Trace processing graph of which to increment the reference count.

    Can be \c NULL.
    @endparblock

@sa bt_graph_put_ref() &mdash;
    Decrements the reference count of a trace processing graph.
*/
extern void bt_graph_get_ref(const bt_graph *graph);

/*!
@brief
    Decrements the \ref api-fund-shared-object "reference count" of
    the trace processing graph \bt_p{graph}.

@param[in] graph
    @parblock
    Trace processing graph of which to decrement the reference count.

    Can be \c NULL.
    @endparblock

@sa bt_graph_get_ref() &mdash;
    Increments the reference count of a trace processing graph.
*/
extern void bt_graph_put_ref(const bt_graph *graph);

/*!
@brief
    Decrements the reference count of the trace processing graph
    \bt_p{_graph}, and then sets \bt_p{_graph} to \c NULL.

@param _graph
    @parblock
    Trace processing graph of which to decrement the reference count.

    Can contain \c NULL.
    @endparblock

@bt_pre_assign_expr{_graph}
*/
#define BT_GRAPH_PUT_REF_AND_RESET(_graph)	\
	do {					\
		bt_graph_put_ref(_graph);	\
		(_graph) = NULL;		\
	} while (0)

/*!
@brief
    Decrements the reference count of the trace processing graph
    \bt_p{_dst}, sets \bt_p{_dst} to \bt_p{_src}, and then sets
    \bt_p{_src} to \c NULL.

This macro effectively moves a trace processing graph reference from the
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
#define BT_GRAPH_MOVE_REF(_dst, _src)		\
	do {					\
		bt_graph_put_ref(_dst);		\
		(_dst) = (_src);		\
		(_src) = NULL;			\
	} while (0)

/*! @} */

/*! @} */

#ifdef __cplusplus
}
#endif

#endif /* BABELTRACE2_GRAPH_GRAPH_H */
