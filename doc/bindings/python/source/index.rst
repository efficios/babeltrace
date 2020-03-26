.. include:: common.rst

Welcome!
========
Welcome to the `Babeltrace 2 <https://babeltrace.org/>`_ Python |~| 3
bindings documentation (version |version|).

.. note::

   This documentation (text and illustrations) is licensed under a
   `Creative Commons Attribution-ShareAlike 4.0 International
   <https://creativecommons.org/licenses/by-sa/4.0/legalcode>`_ license.

.. attention::
   This documentation is **incomplete**.

   In the meantime:

   * See :ref:`examples` to learn how to accomplish common tasks.

   * Have a look at the
     :bt2link:`Babeltrace 2 C API documentation <https://babeltrace.org/docs/v@ver@/libbabeltrace2/>`.

     The Babeltrace |~| 2 Python bindings wrap all the functionalities
     of libbabeltrace2 in a reasonably systematic manner.

**Contents**:

.. toctree::
   :maxdepth: 2

   installation
   examples

What's Babeltrace 2?
--------------------
Babeltrace 2 is an open-source software project by
`EfficiOS <https://www.efficios.com/>`_; its purpose
is to process or convert
`traces <https://en.wikipedia.org/wiki/Tracing_(software)>`_.

The Babeltrace |~| 2 project contains:

* A **library**,
  :bt2link:`libbabeltrace2 <https://babeltrace.org/docs/v@ver@/libbabeltrace2/>`,
  which all the other parts rely on.

  libbabeltrace2 offers a C99 interface.

* A **command-line program**, :bt2man:`babeltrace2(1)`, which can
  convert and manipulate traces.

* **Python 3 bindings** which offer a Pythonic interface of
  libbabeltrace2.

  This documentation is about those bindings.

* "Standard" **plugins** which ship with the project.

  `Common Trace Format <https://diamon.org/ctf/>`_ (CTF) input and
  output,
  :bt2link:`plain text <https://babeltrace.org/docs/v@ver@/man7/babeltrace2-plugin-text.7/>`
  input and output, and various
  :bt2link:`utilities <https://babeltrace.org/docs/v@ver@/man7/babeltrace2-plugin-utils.7/>`
  are part of those plugins.

With the Babeltrace |~| 2 Python bindings, you can write programs to
do everything you can do, and more, with libbabeltrace2, that is:

* Write custom source, filter, and sink *component classes* which
  you can package as Python *plugins*.

  Component classes are instantiated as components within a *trace
  processing graph* and are assembled to accomplish a trace manipulation
  or conversion job.

* Load plugins (compiled shared object or Python modules), instantiate
  their component classes within a trace processing graph, connect the
  components as needed, and run the graph to accomplish a trace
  manipulation or conversion job.

  This is what the :command:`babeltrace2` CLI tool's ``convert`` and
  ``run`` commands do, for example.

A trace processing graph contains connected components. The specific
component topology determines the trace processing task to realize.

.. figure:: images/basic-convert-graph.png

   A *conversion graph*, a specific trace processing graph.

Between the components of a trace processing graph, *messages* flow from
output ports to input ports following the configured connections through
*message iterators*. There are many types of messages, chief amongst
which is the *event message*.

With the Babeltrace |~| 2 Python bindings, you can also query some
specific object from a component class (for example, the available LTTng
live sessions of an `LTTng <https://lttng.org/>`_ relay daemon). This is
what the :command:`babeltrace2` CLI tool's ``query`` command does, for example.

Make sure to read the :bt2man:`babeltrace2-intro(7)`
manual page to learn even more about the Babeltrace |~| 2 project and
its core concepts.
