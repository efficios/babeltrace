.. include:: common.rst

Installation
============
Linux distributions
-------------------
Linux distributions typically provide the Babeltrace |~| 2 Python
bindings as the ``python3-bt2`` package.

Build and install from source
-----------------------------
When you
`build Babeltrace 2 from source <https://babeltrace.org/#bt2-build-from-src>`_,
specify the ``--enable-python-bindings`` option at configuration time,
for example:

.. code-block:: text

   $ ./configure --enable-python-bindings

If you also need the Babeltrace |~| 2 library (libbabeltrace2) to
load Python plugins, specify the ``--enable-python-plugins`` option
at configuration time:

.. code-block:: text

   $ ./configure --enable-python-bindings --enable-python-plugins

See the project's
:bt2link:`README <https://github.com/efficios/babeltrace/blob/stable-@ver@/README.adoc>`
for build-time requirements and detailed build instructions.

.. note::

   The Babeltrace |~| 2 Python bindings only work with Python |~| 3.
