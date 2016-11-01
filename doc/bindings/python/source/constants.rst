.. _constants:

****************
Common constants
****************

.. currentmodule:: babeltrace.common

.. IMPORTANT::
   The bindings documented here are the ones of Babeltrace 2, which
   is not yet released. If you're using a Babeltrace 1.x release, all
   the names are in the same module named ``babeltrace``.

   In this case, the names found in the ``babeltrace.common`` module
   documented here can be found in the ``babeltrace`` module directly.

   For example, instead of:

   .. code-block:: python

      import babeltrace.common

      scope = babeltrace.common.CTFScope.EVENT_CONTEXT

   do:

   .. code-block:: python

      import babeltrace

      scope = babeltrace.CTFScope.EVENT_CONTEXT

The following constants are used as arguments and return values in
several methods of both the :ref:`reader <reader-api>` and
:ref:`CTF writer <ctf-writer-api>` APIs.


:class:`CTFStringEncoding`
==========================

.. autoclass:: CTFStringEncoding
   :members:


:class:`ByteOrder`
==================

.. autoclass:: ByteOrder
   :members:


:class:`CTFTypeId`
==================

.. autoclass:: CTFTypeId
   :members:


:class:`CTFScope`
=================

.. autoclass:: CTFScope
   :members:
