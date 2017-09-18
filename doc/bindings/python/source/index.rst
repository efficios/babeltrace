###########################################
Babeltrace |version| legacy Python bindings
###########################################

Generated on |today|.


Welcome!
========

Welcome to `Babeltrace <http://www.efficios.com/babeltrace>`_'s
*legacy* Python bindings' documentation!

Babeltrace is a trace format converter. It is able to read and write
different trace formats, such as the
`Common Trace Format <http://www.efficios.com/ctf>`_ (CTF).
Babeltrace also acts as the CTF reference implementation.

The Babeltrace legacy Python bindings rely on the ``libbabeltrace``
library, the public C API of Babeltrace.


Installing
==========

The bindings may be enabled when configuring Babeltrace's build::

    ./configure --enable-python-bindings

On Debian and Ubuntu, the bindings are available in the
``python3-babeltrace`` package.

.. note::

   Currently, the Babeltrace legacy Python bindings only works
   with Python 3.


Bindings
========

The Babeltrace legacy Python bindings are available as a single Python
package, :py:mod:`babeltrace`.

The Babeltrace legacy Python bindings' API is divided into two parts:

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
