####################################
Babeltrace |version| Python bindings
####################################

Generated on |today|.


Welcome!
========

Welcome to `Babeltrace <http://www.efficios.com/babeltrace>`_'s
Python bindings' documentation!

Babeltrace is a trace format converter. It is able to read and write
different trace formats, such as the
`Common Trace Format <http://www.efficios.com/ctf>`_ (CTF).
Babeltrace also acts as the CTF reference implementation.

The Babeltrace Python bindings rely on the ``libbabeltrace`` library,
the current public C API of Babeltrace.


Installing
==========

The Python bindings may be enabled when configuring Babeltrace's build::

    ./configure --enable-python-bindings

On Debian and Ubuntu, the Python bindings are available in the
``python3-babeltrace`` package.

.. note::

   Currently, the Babeltrace Python bindings only works with Python 3.


Bindings
========

.. IMPORTANT::
   The bindings documented here are the ones of Babeltrace 2, which
   is not yet released. If you're using a Babeltrace 1.x release, all
   the names are in the same module named ``babeltrace``. In this case:

   * The names found in the ``babeltrace.common`` module
     (:ref:`documented here <constants>`) can be found in the
     ``babeltrace`` module directly.

     For example, instead of:

     .. code-block:: python

        import babeltrace.common

        scope = babeltrace.common.CTFScope.EVENT_CONTEXT

     do:

     .. code-block:: python

        import babeltrace

        scope = babeltrace.CTFScope.EVENT_CONTEXT

   * The names found in the ``babeltrace.reader`` module
     (:ref:`documented here <reader-api>`) can be found in the
     ``babeltrace`` module directly.

     For example, instead of:

     .. code-block:: python

        import babeltrace.reader

        trace_collection = babeltrace.reader.TraceCollection()

     do:

     .. code-block:: python

        import babeltrace

        trace_collection = babeltrace.TraceCollection()

   * The names found in the ``babeltrace.writer`` module
     (:ref:`documented here <ctf-writer-api>`) can be found in the
     ``CTFWriter`` class in the ``babeltrace`` module.

     For example, instead of:

     .. code-block:: python

        import babeltrace.writer

        event_class = babeltrace.writer.EventClass('my_event')

     do:

     .. code-block:: python

        from babeltrace import CTFWriter

        event_class = CTFWriter.EventClass('my_event')

The Babeltrace Python bindings are available as a single Python package,
:py:mod:`babeltrace`.

The Babeltrace Python bindings' API is divided into two parts:

* The :ref:`reader API <reader-api>` is exposed by the
  :mod:`babeltrace.reader` module, a set of classes used to
  open a collection of different traces and iterate on their events.
* The :ref:`CTF writer API <ctf-writer-api>` is exposed by the
  :mod:`babeltrace.writer` module, which makes it possible to
  write a complete `CTF <http://www.efficios.com/ctf>`_ trace from
  scratch.

.. note::

   For backward compatibility reasons, the reader API is imported in the
   package itself. The CTF writer API is imported in the package itself
   too, as :mod:`~babeltrace.CTFWriter`. It is, however, strongly
   recommended to import and use the three modules
   (:mod:`babeltrace.common`, :mod:`babeltrace.reader`, and
   :mod:`babeltrace.writer`) explicitly, since there is no long-term
   plan to maintain the compatibility layer.

**Contents**:

.. toctree::
   :numbered:

   constants
   reader
   writer
   examples
