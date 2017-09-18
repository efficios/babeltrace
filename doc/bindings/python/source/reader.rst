.. _reader-api:

**********
Reader API
**********

.. currentmodule:: babeltrace.reader

The classes documented below are related to **reading traces**
operations. Common operations such as adding traces to a trace
collection, iterating on their events, reading event names,
timestamps, contexts and payloads can be accomplished by using these
classes.

The Babeltrace Python legacy bindings' reader API exposes two categories
of classes. The first category is used to open traces and iterate on
theirs events. The typical procedure for reading traces is:

1. Create a :class:`TraceCollection` instance.
2. Add traces to this collection using
   :meth:`TraceCollection.add_trace`.
3. Iterate on :attr:`TraceCollection.events` to get :class:`Event`
   objects and perform the desired computation on event data.

.. seealso::

   :ref:`reader-api-examples`

.. class:: TraceCollection
   :noindex:

   A collection of opened traces.

.. class:: TraceHandle
   :noindex:

   A trace handle. Allows the user to manipulate a specific trace
   directly.

.. class:: Event
   :noindex:

   An event.

.. exception:: FieldError
   :noindex:

   A field error. Raised when a field's value cannot be accessed.

The second category of classes is a set of declaration classes that
are returned when inspecting the layout of a trace's events, through
:attr:`TraceHandle.events`. The following classes are not meant to be
instantiated by the user:

.. class:: EventDeclaration
   :noindex:

   Event declaration.

.. class:: FieldDeclaration
   :noindex:

   Event field declaration (base class for all the following types).

.. class:: IntegerFieldDeclaration
   :noindex:

   Integer field declaration.

.. class:: FloatFieldDeclaration
   :noindex:

   Floating point number field declaration.

.. class:: EnumerationFieldDeclaration
   :noindex:

   Enumeration field declaration.

.. class:: StringFieldDeclaration
   :noindex:

   String (NULL-terminated array of bytes) field declaration.

.. class:: ArrayFieldDeclaration
   :noindex:

   Static array field declaration.

.. class:: SequenceFieldDeclaration
   :noindex:

   Sequence (dynamic array) field declaration.

.. class:: StructureFieldDeclaration
   :noindex:

   Structure field declaration. A structure is an ordered map of field
   names to field values.

.. class:: VariantFieldDeclaration
   :noindex:

   Variant field declaration. A variant is dynamic selection between
   different types.

Here is the subclass relationship for the reader API::

    object
        TraceCollection
        TraceHandle
        Event
        FieldError
        EventDeclaration
        FieldDeclaration
            IntegerFieldDeclaration
            FloatFieldDeclaration
            EnumerationFieldDeclaration
            StringFieldDeclaration
            ArrayFieldDeclaration
            SequenceFieldDeclaration
            StructureFieldDeclaration
            VariantFieldDeclaration

:class:`TraceCollection`
========================

.. autoclass:: TraceCollection
   :members:

   .. automethod:: __init__


:class:`TraceHandle`
====================

.. autoclass:: TraceHandle
   :members:


:class:`Event`
==============

.. autoclass:: Event
   :members:


:exc:`FieldError`
=================

.. autoexception:: FieldError
   :members:


:class:`EventDeclaration`
=========================

.. autoclass:: EventDeclaration
   :members:


:class:`FieldDeclaration`
=========================

.. autoclass:: FieldDeclaration
   :members:


:class:`IntegerFieldDeclaration`
================================

.. autoclass:: IntegerFieldDeclaration
   :members:
   :show-inheritance:


:class:`FloatFieldDeclaration`
==============================

.. autoclass:: FloatFieldDeclaration
   :members:
   :show-inheritance:


:class:`EnumerationFieldDeclaration`
====================================

.. autoclass:: EnumerationFieldDeclaration
   :members:
   :show-inheritance:


:class:`StringFieldDeclaration`
===============================

.. autoclass:: StringFieldDeclaration
   :members:
   :show-inheritance:


:class:`ArrayFieldDeclaration`
==============================

.. autoclass:: ArrayFieldDeclaration
   :members:
   :show-inheritance:


:class:`SequenceFieldDeclaration`
=================================

.. autoclass:: SequenceFieldDeclaration
   :members:
   :show-inheritance:


:class:`StructureFieldDeclaration`
==================================

.. autoclass:: StructureFieldDeclaration
   :members:
   :show-inheritance:


:class:`VariantFieldDeclaration`
================================

.. autoclass:: VariantFieldDeclaration
   :members:
