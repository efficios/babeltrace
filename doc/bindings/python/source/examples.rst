.. include:: common.rst

.. _examples:

Examples
========
This section contains a few short and straightforward examples which
show how to use the Babeltrace |~| 2 Python bindings.

The :mod:`bt2` package provides the Babeltrace |~| 2 Python bindings.
Note that the :mod:`babeltrace` package is part of the Babeltrace |~| 1
project: it's somewhat out-of-date and not compatible with the
:mod:`bt2` package.

Assume that all the examples below are named :file:`example.py`.

.. _examples_tcmi:

Iterate trace events
--------------------
The most convenient and high-level way to iterate the events of one or
more traces is with a :class:`bt2.TraceCollectionMessageIterator`
object.

A :class:`bt2.TraceCollectionMessageIterator` object roughly offers the
same features as the ``convert`` command of the :command:`babeltrace2`
command-line program (see the :bt2man:`babeltrace2-convert(1)` manual
page), but in a programmatic, Pythonic way.

As of Babeltrace |~| |version|, the trace collection message iterator
class is a Python bindings-only feature: the Python code uses
libbabeltrace2 internally, but the latter does not offer this utility as
such.

The :class:`bt2.TraceCollectionMessageIterator` interface features:

* **Automatic source component (trace format) discovery**.

  ``convert`` command equivalent example:

  .. code-block:: text

     $ babeltrace2 /path/to/my/trace

* **Explicit component class instantiation**.

  ``convert`` command equivalent example:

  .. code-block:: text

     $ babeltrace2 --component=source.my.format

* **Passing initialization parameters to both auto-discovered and
  explicitly created components**.

  ``convert`` command equivalent example:

  .. code-block:: text

     $ babeltrace2 /path/to/my/trace --params=detailed=no \
                   --component=source.ctf.fs \
                   --params='inputs=["/path/to/my/trace"]'

* **Trace event muxing**.

  The message iterator muxes (combines) the events from multiple
  compatible streams into a single, time-sorted sequence of events.

  .. code-block:: text

     $ babeltrace2 /path/to/trace1 /path/to/trace2 /path/to/trace3

* **Stream intersection mode**.

  ``convert`` command equivalent example:

  .. code-block:: text

     $ babeltrace2 /path/to/my/trace --stream-intersection

* **Stream trimming with beginning and/or end times**.

  ``convert`` command equivalent example:

  .. code-block:: text

     $ babeltrace2 /path/to/my/trace --begin=22:14:38 --end=22:15:07

While the :command:`babeltrace2 convert` command creates a ``sink.text.pretty``
component class (by default) to pretty-print events as plain text lines,
a :class:`bt2.TraceCollectionMessageIterator` object is a Python
iterator which makes its user a message consumer (there's no sink
component)::

    import bt2

    for msg in bt2.TraceCollectionMessageIterator('/path/to/trace'):
        if type(msg) is bt2._EventMessageConst:
            print(msg.event.name)

.. _examples_tcmi_autodisc:

Discover traces
~~~~~~~~~~~~~~~
Pass one or more file paths, directory paths, or other strings when you
build a :class:`bt2.TraceCollectionMessageIterator` object to let it
automatically determine which source components to create for you.

If you pass a directory path, the message iterator traverses the
directory recursively to find traces, automatically selecting the
appropriate source component classes to instantiate.

The :class:`bt2.TraceCollectionMessageIterator` object and the
:command:`babeltrace2 convert` CLI command share the same automatic
component discovery algorithm. See the
:bt2link:`Create implicit components from non-option arguments <https://babeltrace.org/docs/v@ver@/man1/babeltrace2-convert.1/#doc-comp-create-impl-non-opt>`
section of the :bt2man:`babeltrace2-convert(1)` manual page for more
details.

The following example shows how to use a
:class:`bt2.TraceCollectionMessageIterator` object to automatically
discover one or more traces from a single path (file or directory). For
each trace event, the example prints its name::

    import bt2
    import sys

    # Get the trace path from the first command-line argument.
    path = sys.argv[1]

    # Create a trace collection message iterator with this path.
    msg_it = bt2.TraceCollectionMessageIterator(path)

    # Iterate the trace messages.
    for msg in msg_it:
        # `bt2._EventMessageConst` is the Python type of an event message.
        if type(msg) is bt2._EventMessageConst:
            # An event message holds a trace event.
            event = msg.event

            # Print event's name.
            print(event.name)

Run this example:

.. code-block:: text

   $ python3 example.py /path/to/one/or/more/traces

Output example:

.. code-block:: text

    kmem_kmalloc
    kmem_kfree
    kmem_cache_alloc_node
    block_getrq
    kmem_kmalloc
    block_plug
    kmem_kfree
    block_rq_insert
    kmem_kmalloc
    kmem_kfree
    kmem_kmalloc
    kmem_kfree

The example above is simplistic; it does not catch the exceptions that
some statements can raise:

* ``bt2.TraceCollectionMessageIterator(path)`` raises an exception if
  it cannot find any trace.

* Each iteration of the loop, or, more precisely, the
  :meth:`bt2.TraceCollectionMessageIterator.__next__` method, raises
  an exception if there's any error during the iteration process.

  For example, an internal source component message iterator can fail
  when trying to decode a malformed trace file.

.. _examples_tcmi_expl:

Create explicit source components
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
If `automatic source component discovery <#examples-tcmi-autodisc>`_
doesn't work for you (for example, because the source component class
you actually need to instantiate doesn't offer the
``babeltrace.support-info`` query object), create explicit source
components when you build a :class:`bt2.TraceCollectionMessageIterator`
object.

The following example builds a trace collection message iterator to
explicitly instantiate a ``source.ctf.fs`` component class (found in the
``ctf`` plugin). Again, for each trace event, the example prints its
name::

    import bt2
    import sys

    # Find the `ctf` plugin (shipped with Babeltrace 2).
    ctf_plugin = bt2.find_plugin('ctf')

    # Get the `source.ctf.fs` component class from the plugin.
    fs_cc = ctf_plugin.source_component_classes['fs']

    # Create a trace collection message iterator, instantiating a single
    # `source.ctf.fs` component class with the `inputs` initialization
    # parameter set to open a single CTF trace.
    msg_it = bt2.TraceCollectionMessageIterator(bt2.ComponentSpec(fs_cc, {
        # Get the CTF trace path from the first command-line argument.
        'inputs': [sys.argv[1]],
    }))

    # Iterate the trace messages.
    for msg in msg_it:
        # `bt2._EventMessageConst` is the Python type of an event message.
        if type(msg) is bt2._EventMessageConst:
            # Print event's name.
            print(msg.event.name)

Run this example:

.. code-block:: text

   $ python3 example.py /path/to/ctf/trace

Output example:

.. code-block:: text

    kmem_kmalloc
    kmem_kfree
    kmem_cache_alloc_node
    block_getrq
    kmem_kmalloc
    block_plug
    kmem_kfree
    block_rq_insert
    kmem_kmalloc
    kmem_kfree
    kmem_kmalloc
    kmem_kfree

The example above looks similar to the previous one using
`automatic source component discovery <#examples-tcmi-autodisc>`_,
but there are notable differences:

* A ``source.ctf.fs`` component expects to receive the path to a
  *single* `CTF <https://diamon.org/ctf/>`_ trace (a directory
  containing a file named ``metadata``).

  Unlike the previous example, you must pass the exact
  :abbr:`CTF (Common Trace Format)` trace directory path, *not* a
  parent directory path.

* Unlike the previous example, the example above can only read a single
  trace.

  If you want to read multiple :abbr:`CTF (Common Trace Format)` traces
  using explicit component class instantiation with a single trace
  collection message iterator, you must create one ``source.ctf.fs``
  component per trace.

Note that the :class:`bt2.ComponentSpec` class offers the
:meth:`from_named_plugin_and_component_class` convenience static method
which finds the plugin and component class for you. You could therefore
rewrite the trace collection message iterator creation part of the
example above as::

    # Create a trace collection message iterator, instantiating a single
    # `source.ctf.fs` component class with the `inputs` initialization
    # parameter set to open a single CTF trace.
    msg_it = bt2.TraceCollectionMessageIterator(
        bt2.ComponentSpec.from_named_plugin_and_component_class('ctf', 'fs', {
            # Get the CTF trace path from the first command-line argument.
            'inputs': [sys.argv[1]],
        })
    )

.. _examples_tcmi_ev_field:

Get a specific event field's value
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
The :ref:`examples_tcmi_autodisc` and :ref:`examples_tcmi_expl` examples
show that a :class:`bt2.TraceCollectionMessageIterator` iterates the
time-sorted *messages* of one or more traces.

One specific type of message is :class:`bt2._EventMessageConst`, which
holds a trace event object.

.. note::

   Everything you can find in the :mod:`bt2` package is publicly
   accessible.

   Names which start with ``_`` (underscore), like
   :class:`bt2._EventMessageConst`, indicate that you can't
   *instantiate* such a class (you cannot call the class). However, the
   type itself remains public so that you can use its name to check an
   object's type:

   .. code-block:: python

       if type(msg) is bt2._EventMessageConst:
           # ...

   .. code-block:: python

       if isinstance(field, bt2._IntegerFieldConst):
           # ...

Access an event object's field by using the event as a simple mapping
(like a read-only :class:`dict`), where keys are field names. The field
can belong to any part of the event (contexts or payload) and to its
packet's context, if any::

    import bt2
    import sys

    # Create a trace collection message iterator from the first
    # command-line argument.
    msg_it = bt2.TraceCollectionMessageIterator(sys.argv[1])

    # Iterate the trace messages.
    for msg in msg_it:
        # `bt2._EventMessageConst` is the Python type of an event message.
        # Only keep such messages.
        if type(msg) is not bt2._EventMessageConst:
            continue

        # An event message holds a trace event.
        event = msg.event

        # Only check `sched_switch` events.
        if event.name != 'sched_switch':
            continue

        # In an LTTng trace, the `cpu_id` field is a packet context field.
        # The mapping interface of `event` can still find it.
        cpu_id = event['cpu_id']

        # Previous and next process short names are found in the event's
        # `prev_comm` and `next_comm` fields.
        prev_comm = event['prev_comm']
        next_comm = event['next_comm']

        # Print line, using field values.
        msg = 'CPU {}: Switching process `{}` → `{}`'
        print(msg.format(cpu_id, prev_comm, next_comm))

The example above assumes that the traces to open are
`LTTng <https://lttng.org/>`_ Linux kernel traces.

Run this example:

.. code-block:: text

   $ python3 example.py /path/to/one/or/more/lttng/traces

Output example:

.. code-block:: text

    CPU 2: Switching process `Timer` → `swapper/2`
    CPU 0: Switching process `swapper/0` → `firefox`
    CPU 0: Switching process `firefox` → `swapper/0`
    CPU 0: Switching process `swapper/0` → `rcu_preempt`
    CPU 0: Switching process `rcu_preempt` → `swapper/0`
    CPU 3: Switching process `swapper/3` → `alsa-sink-ALC26`
    CPU 2: Switching process `swapper/2` → `Timer`
    CPU 2: Switching process `Timer` → `swapper/2`
    CPU 2: Switching process `swapper/2` → `pulseaudio`
    CPU 0: Switching process `swapper/0` → `firefox`
    CPU 1: Switching process `swapper/1` → `threaded-ml`
    CPU 2: Switching process `pulseaudio` → `Timer`

If you need to access a specific field, use:

Event payload
  :attr:`bt2._EventConst.payload_field` property.

Event specific context
  :attr:`bt2._EventConst.specific_context_field` property.

Event common context
  :attr:`bt2._EventConst.common_context_field` property.

Packet context
  :attr:`bt2._PacketConst.context_field` property.

Use Python's ``in`` operator to verify if a specific "root" field (in the list
above) contains a given field by name::

    if 'next_comm' in event.payload_field:
           # ...

The following example iterates the events of a given trace, printing the
value of the ``fd`` payload field if it's available::

    import bt2
    import sys

    # Create a trace collection message iterator from the first command-line
    # argument.
    msg_it = bt2.TraceCollectionMessageIterator(sys.argv[1])

    # Iterate the trace messages.
    for msg in msg_it:
        # `bt2._EventMessageConst` is the Python type of an event message.
        if type(msg) is bt2._EventMessageConst:
            # Check if the `fd` event payload field exists.
            if 'fd' in msg.event.payload_field:
                # Print the `fd` event payload field's value.
                print(msg.event.payload_field['fd'])

Output example:

.. code-block:: text

    14
    15
    16
    19
    30
    31
    33
    42
    0
    1
    2
    3

.. _examples_tcmi_ev_time:

Get an event's time
~~~~~~~~~~~~~~~~~~~
The time, or timestamp, of an event object belongs to its message as
a *default clock snapshot*.

An event's clock snapshot is a *snapshot* (an immutable value) of the
value of the event's stream's clock when the event occurred. As of
Babeltrace |~| |version|, a stream can only have one clock: its default
clock.

Use the :attr:`default_clock_snapshot` property of an event message
to get its default clock snapshot. A clock snapshot object offers,
amongst other things, the following properties:

:attr:`value` (:class:`int`)
  Value of the clock snapshot in clock cycles.

  A stream clock can have any frequency (Hz).

:attr:`ns_from_origin` (:class:`int`)
  Number of nanoseconds from the stream clock's origin (often the Unix
  epoch).

The following example prints, for each event, its name, its date/time,
and the difference, in seconds, since the previous event's time (if
any)::

    import bt2
    import sys
    import datetime

    # Create a trace collection message iterator from the first command-line
    # argument.
    msg_it = bt2.TraceCollectionMessageIterator(sys.argv[1])

    # Last event's time (ns from origin).
    last_event_ns_from_origin = None

    # Iterate the trace messages.
    for msg in msg_it:
        # `bt2._EventMessageConst` is the Python type of an event message.
        if type(msg) is bt2._EventMessageConst:
            # Get event message's default clock snapshot's ns from origin
            # value.
            ns_from_origin = msg.default_clock_snapshot.ns_from_origin

            # Compute the time difference since the last event message.
            diff_s = 0

            if last_event_ns_from_origin is not None:
                diff_s = (ns_from_origin - last_event_ns_from_origin) / 1e9

            # Create a `datetime.datetime` object from `ns_from_origin` for
            # presentation. Note that such an object is less accurate than
            # `ns_from_origin` as it holds microseconds, not nanoseconds.
            dt = datetime.datetime.fromtimestamp(ns_from_origin / 1e9)

            # Print line.
            fmt = '{} (+{:.6f} s): {}'
            print(fmt.format(dt, diff_s, msg.event.name))

            # Update last event's time.
            last_event_ns_from_origin = ns_from_origin

Run this example:

.. code-block:: text

   $ python3 example.py /path/to/one/or/more/traces

Output example:

.. code-block:: text

    2015-09-09 22:40:41.551451 (+0.000004 s): lttng_ust_statedump:end
    2015-09-09 22:40:43.003397 (+1.451946 s): lttng_ust_dl:dlopen
    2015-09-09 22:40:43.003412 (+0.000015 s): lttng_ust_dl:build_id
    2015-09-09 22:40:43.003861 (+0.000449 s): lttng_ust_dl:dlopen
    2015-09-09 22:40:43.003865 (+0.000004 s): lttng_ust_dl:build_id
    2015-09-09 22:40:43.003879 (+0.000014 s): my_provider:my_first_tracepoint
    2015-09-09 22:40:43.003895 (+0.000016 s): my_provider:my_first_tracepoint
    2015-09-09 22:40:43.003898 (+0.000003 s): my_provider:my_other_tracepoint
    2015-09-09 22:40:43.003922 (+0.000023 s): lttng_ust_dl:dlclose

Bonus: Print top 5 running processes using LTTng
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
As :ref:`examples_tcmi_ev_field` shows, a
:class:`bt2.TraceCollectionMessageIterator` can read
`LTTng <https://lttng.org/>`_ traces.

The following example is similar to :ref:`examples_tcmi_ev_time`: it
reads a whole LTTng Linux kernel trace, but instead of printing the time
difference for each event, it accumulates them to print the short names
of the top |~| 5 running processes on CPU |~| 0 during the whole trace.

.. code-block:: python

    import bt2
    import sys
    import collections

    # Create a trace collection message iterator from the first command-line
    # argument.
    msg_it = bt2.TraceCollectionMessageIterator(sys.argv[1])

    # This counter dictionary will hold execution times:
    #
    #     Task command name -> Total execution time (ns)
    exec_times = collections.Counter()

    # This holds the last `sched_switch` event time.
    last_ns_from_origin = None

    for msg in msg_it:
        # `bt2._EventMessageConst` is the Python type of an event message.
        # Only keep such messages.
        if type(msg) is not bt2._EventMessageConst:
            continue

        # An event message holds a trace event.
        event = msg.event

        # Only check `sched_switch` events.
        if event.name != 'sched_switch':
            continue

        # Keep only events which occurred on CPU 0.
        if event['cpu_id'] != 0:
            continue

        # Get event message's default clock snapshot's ns from origin value.
        ns_from_origin = msg.default_clock_snapshot.ns_from_origin

        if last_ns_from_origin is None:
            # We start here.
            last_ns_from_origin = ns_from_origin

        # Previous process's short name.
        prev_comm = str(event['prev_comm'])

        # Initialize an entry in our dictionary if not done yet.
        if prev_comm not in exec_times:
            exec_times[prev_comm] = 0

        # Compute previous process's execution time.
        diff_ns = ns_from_origin - last_ns_from_origin

        # Update execution time of this command.
        exec_times[prev_comm] += diff_ns

        # Update last event's time.
        last_ns_from_origin = ns_from_origin

    # Print top 5.
    for comm, ns in exec_times.most_common(5):
        print('{:20}{} s'.format(comm, ns / 1e9))

Run this example:

.. code-block:: text

   $ python3 example.py /path/to/lttng/trace

Output example:

.. code-block:: text

    swapper/0           326.294314471 s
    chromium            2.500456202 s
    Xorg.bin            0.546656895 s
    threaded-ml         0.545098185 s
    pulseaudio          0.53677713 s

Note that ``swapper/0`` is the "idle" process of CPU |~| 0 on Linux;
since we weren't using the CPU that much when tracing, its first
position in the list makes sense.

Inspect event classes
~~~~~~~~~~~~~~~~~~~~~
Each event stream is a *stream class* instance.

A stream class contains *event classes*. A stream class's event classes
describe all the possible events you can find in its instances. Stream
classes and event classes form the *metadata* of streams and events.

The following example shows how to list all the event classes of a
stream class. For each event class, the example also prints the names of
its payload field class's first-level members.

.. note::

   As of Babeltrace |~| |version|, there's no way to access a stream class
   without consuming at least one message for one of its instances
   (streams).

   A source component can add new event classes to existing stream
   classes during the trace processing task. Therefore, this example
   only lists the initial stream class's event classes.

.. code-block:: python

    import bt2
    import sys

    # Create a trace collection message iterator from the first command-line
    # argument.
    msg_it = bt2.TraceCollectionMessageIterator(sys.argv[1])

    # Get the message iterator's first stream beginning message.
    for msg in msg_it:
        # `bt2._StreamBeginningMessageConst` is the Python type of a stream
        # beginning message.
        if type(msg) is bt2._StreamBeginningMessageConst:
            break

    # A stream beginning message holds a stream.
    stream = msg.stream

    # Get the stream's class.
    stream_class = stream.cls

    # The stream class object offers a mapping interface (like a read-only
    # `dict`), where keys are event class IDs and values are
    # `bt2._EventClassConst` objects.
    for event_class in stream_class.values():
        print('{}:'.format(event_class.name))

        # The `payload_field_class` property of an event class returns a
        # `bt2._StructureFieldClassConst` object. This object offers a
        # mapping interface, where keys are member names and values are
        # `bt2._StructureFieldClassMemberConst` objects.
        for member in event_class.payload_field_class.values():
            fmt = '  {}: `{}.{}`'
            print(fmt.format(member.name, bt2.__name__,
                             member.field_class.__class__.__name__))

Run this example:

.. code-block:: text

   $ python3 example.py /path/to/trace

Output example:

.. code-block:: text

    sched_migrate_task:
      comm: `bt2._StringFieldClassConst`
      tid: `bt2._SignedIntegerFieldClassConst`
      prio: `bt2._SignedIntegerFieldClassConst`
      orig_cpu: `bt2._SignedIntegerFieldClassConst`
      dest_cpu: `bt2._SignedIntegerFieldClassConst`
    sched_switch:
      prev_comm: `bt2._StringFieldClassConst`
      prev_tid: `bt2._SignedIntegerFieldClassConst`
      prev_prio: `bt2._SignedIntegerFieldClassConst`
      prev_state: `bt2._SignedIntegerFieldClassConst`
      next_comm: `bt2._StringFieldClassConst`
      next_tid: `bt2._SignedIntegerFieldClassConst`
      next_prio: `bt2._SignedIntegerFieldClassConst`
    sched_wakeup_new:
      comm: `bt2._StringFieldClassConst`
      tid: `bt2._SignedIntegerFieldClassConst`
      prio: `bt2._SignedIntegerFieldClassConst`
      target_cpu: `bt2._SignedIntegerFieldClassConst`

.. _examples_graph:

Build and run a trace processing graph
--------------------------------------
Internally, a :class:`bt2.TraceCollectionMessageIterator` object (see
:ref:`examples_tcmi`) builds a *trace processing graph*, just like the
:bt2man:`babeltrace2-convert(1)` CLI command, and then offers a
Python iterator interface on top of it.

See the :bt2man:`babeltrace2-intro(7)` manual page to learn more about
the Babeltrace |~| 2 project and its core concepts.

The following examples shows how to manually build and then run a trace
processing graph yourself (like the :bt2man:`babeltrace2-run(1)` CLI
command does). The general steps to do so are:

#. Create an empty graph.

#. Add components to the graph.

   This process is also known as *instantiating a component class*
   because the graph must first create the component from its class
   before adding it.

   A viable graph contains at least one source component and one sink
   component.

#. Connect component ports.

   On initialization, components add input and output ports, depending
   on their type.

   You can connect component output ports to input ports within a graph.

#. Run the graph.

   This is a blocking operation which makes each sink component consume
   some messages in a round robin fashion until there are no more.

.. code-block:: python

    import bt2
    import sys

    # Create an empty graph.
    graph = bt2.Graph()

    # Add a `source.text.dmesg` component.
    #
    # graph.add_component() returns the created and added component.
    #
    # Such a component reads Linux kernel ring buffer messages (see
    # `dmesg(1)`) from the standard input and creates corresponding event
    # messages. See `babeltrace2-source.text.dmesg(7)`.
    #
    # `my source` is the unique name of this component within `graph`.
    comp_cls = bt2.find_plugin('text').source_component_classes['dmesg']
    src_comp = graph.add_component(comp_cls, 'my source')

    # Add a `sink.text.pretty` component.
    #
    # Such a component pretty-prints event messages on the standard output
    # (one message per line). See `babeltrace2-sink.text.pretty(7)`.
    #
    # The `babeltrace2 convert` CLI command uses a `sink.text.pretty`
    # sink component by default.
    comp_cls = bt2.find_plugin('text').sink_component_classes['pretty']
    sink_comp = graph.add_component(comp_cls, 'my sink')

    # Connect the `out` output port of the `source.text.dmesg` component
    # to the `in` input port of the `sink.text.pretty` component.
    graph.connect_ports(src_comp.output_ports['out'],
                        sink_comp.input_ports['in'])

    # Run the trace processing graph.
    graph.run()

Run this example:

.. code-block:: text

   $ dmesg -t | python3 example.py

Output example:

.. code-block:: text

    string: { str = "ata1.00: NCQ Send/Recv Log not supported" }
    string: { str = "ata1.00: ACPI cmd ef/02:00:00:00:00:a0 (SET FEATURES) succeeded" }
    string: { str = "ata1.00: ACPI cmd f5/00:00:00:00:00:a0 (SECURITY FREEZE LOCK) filtered out" }
    string: { str = "ata1.00: ACPI cmd ef/10:03:00:00:00:a0 (SET FEATURES) filtered out" }
    string: { str = "ata1.00: NCQ Send/Recv Log not supported" }
    string: { str = "ata1.00: configured for UDMA/133" }
    string: { str = "ata1.00: Enabling discard_zeroes_data" }
    string: { str = "OOM killer enabled." }
    string: { str = "Restarting tasks ... done." }
    string: { str = "PM: suspend exit" }

Query a component class
-----------------------
Component classes, provided by plugins, can implement a method to
support *query operations*.

A query operation is similar to a function call: the caller makes a
request (a query) with parameters and the component class's query
method returns a result object.

The query operation feature exists so that you can benefit from a
component class's implementation to get information about a trace, a
stream, a distant server, and so on. For example, the
``source.ctf.lttng-live`` component class (see
:bt2man:`babeltrace2-source.ctf.lttng-live(7)`) offers the ``sessions``
object to list the available
`LTTng live <https://lttng.org/docs/#doc-lttng-live>`_ tracing
session names and other properties.

The semantics of the query parameters and the returned object are
completely defined by the component class implementation: the library
and its Python bindings don't enforce or suggest any layout.
The best way to know which objects you can query from a component class,
what are the expected and optional parameters, and what the returned
object contains is to read this component class's documentation.

The following example queries the "standard" ``babeltrace.support-info``
query object (see
:bt2man:`babeltrace2-query-babeltrace.support-info(7)`) from the
``source.ctf.fs`` component class
(see :bt2man:`babeltrace2-source.ctf.fs(7)`) and
pretty-prints the result. The ``babeltrace.support-info`` query object
indicates whether or not a given path locates a
:abbr:`CTF (Common Trace Format)` trace directory::

    import bt2
    import sys

    # Get the `source.ctf.fs` component class from the `ctf` plugin.
    comp_cls = bt2.find_plugin('ctf').source_component_classes['fs']

    # The `babeltrace.support-info` query operation expects a `type`
    # parameter (set to `directory` here) and an `input` parameter (the
    # actual path or string to check, in this case the first command-line
    # argument).
    #
    # See `babeltrace2-query-babeltrace.support-info(7)`.
    params = {
        'type': 'directory',
        'input': sys.argv[1],
    }

    # Create a query executor.
    #
    # This is the environment in which query operations happens. The
    # queried component class has access to this executor, for example to
    # retrieve the query operation's logging level.
    query_exec = bt2.QueryExecutor(comp_cls, 'babeltrace.support-info',
                                   params)

    # Query the component class through the query executor.
    #
    # This method returns the result.
    result = query_exec.query()

    # Print the result.
    print(result)

As you can see, no trace processing graph is involved (like in
:ref:`examples_tcmi` and :ref:`examples_graph`): a query operation
is *not* a sequential trace processing task, but a simple, atomic
procedure call.

Run this example:

.. code-block:: text

   $ python3 example.py /path/to/ctf/trace

Output example:

.. code-block:: text

    {'group': '21c63a42-40bc-4c08-9761-3815ae01f43d', 'weight': 0.75}

This result indicates that the component class is 75 |~| % confident that
:file:`/path/to/ctf/trace` is a CTF trace directory path. It also shows
that this specific CTF trace belongs to the
``21c63a42-40bc-4c08-9761-3815ae01f43d`` group; a single component can
handle multiple traces which belong to the same group.

Let's try the sample example with a path that doesn't locate a CTF
trace:

.. code-block:: text

   $ python3 example.py /etc

Output:

.. code-block:: text

    {'weight': 0.0}

As expected, the zero weight indicates that ``/etc`` isn't a CTF trace
directory path.
