.. _ctf-writer-api:

**************
CTF writer API
**************

.. currentmodule:: babeltrace.writer

The **CTF writer API** allows to write native
`CTF <http://www.efficios.com/ctf>`_ traces from a Python environment.

A CTF trace is made of one or more CTF streams. Streams contain
packets, which in turn contain serialized events. CTF readers, such as
Babeltrace, are able to merge streams and iterate on events in order
when reading a CTF trace.

The CTF writer API is exposed as a set of classes available in the
:mod:`babeltrace.writer` module.

.. seealso::

   :ref:`ctf-writer-api-examples`

There is a clear distinction to be made between field declaration
and field objects. A field declaration describes the properties, or
metadata, of a field. It does not contain a value. For example,
an integer *field declaration* contains a size in bits, a byte order,
a display base, whether it's signed or not, and so on. On the other
hand, an integer *field* is linked with a specific integer field
declaration, but contains an actual integer value. Thus, the layout and
properties of fields are described once in field declarations, and then
all concrete fields carry a value and a field declaration reference.
The same applies to event classes vs. events, as well as to stream
classes vs. streams.

The main interface to write a CTF trace is the :class:`Writer` class:

.. class:: Writer
   :noindex:

   Writing object in which clocks and streams may be registered, and
   other common trace properties may be set.

A concrete :class:`Stream` object may be created from a
:class:`StreamClass` using :meth:`Writer.create_stream` method. A
stream class is also associated with a clock (:class:`Clock`), which
must also be registered to the writer object.

.. class:: StreamClass
   :noindex:

   Stream class.

.. class:: Stream
   :noindex:

   Stream, based on a stream class.

.. class:: Clock
   :noindex:

   Reference clock of one to many streams.

Events, before being created, must be described in an event class
(:class:`EventClass`). An event class must be added to a stream class
using :meth:`StreamClass.add_event_class` before appending an event
to an actual stream linked with this stream class.

.. class:: EventClass
   :noindex:

   Event class.

.. class:: Event
   :noindex:

   Event, based on an event class.

An event class is created by instantiating an :class:`EventClass`
object and using its :meth:`~EventClass.add_field` method to add an
instance of one of the following field declarations:

.. class:: IntegerFieldDeclaration
   :noindex:

   Integer field declaration.

.. class:: FloatingPointFieldDeclaration
   :noindex:

   Floating point number field declaration.

.. class:: EnumerationFieldDeclaration
   :noindex:

   Enumeration field declaration (mapping from labels to ranges of
   integers).

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

   Structure (ordered map of field names to field declarations) field
   declaration.

.. class:: VariantFieldDeclaration
   :noindex:

   Variant (dynamic selection between different field declarations)
   field declaration.

Once an event class is created, a concrete event may be created by
instantiating an :class:`Event` object. Its :meth:`~Event.payload`
method may be used to access the actual event's fields and set their
values. Those fields are instances of the following types:

.. class:: IntegerField
   :noindex:

   Integer field.

.. class:: FloatingPointField
   :noindex:

   Floating point number field.

.. class:: EnumerationField
   :noindex:

   Enumeration field (mapping from labels to ranges of integers).

.. class:: StringField
   :noindex:

   String (NULL-terminated array of bytes) field.

.. class:: ArrayField
   :noindex:

   Static array field.

.. class:: SequenceField
   :noindex:

   Sequence (dynamic array) field.

.. class:: StructureField
   :noindex:

   Structure (ordered map of field names to field declarations) field.

.. class:: VariantField
   :noindex:

   Variant (dynamic selection between different field declarations)
   field.

The subclass relationship for the CTF writer API is the following::

    object
        FieldDeclaration
            IntegerFieldDeclaration
            FloatingPointFieldDeclaration
            EnumerationFieldDeclaration
            StringFieldDeclaration
            ArrayFieldDeclaration
            SequenceFieldDeclaration
            StructureFieldDeclaration
            VariantFieldDeclaration
        Field
            IntegerField
            FloatingPointField
            EnumerationField
            StringField
            ArrayField
            SequenceField
            StructureField
            VariantField
        EnumerationMapping
        EventClass
        Event
        Clock
        StreamClass
        Stream
        Writer

Most of these classes' methods and properties raise :exc:`ValueError`
on error.


:class:`Writer`
===============

.. autoclass:: Writer
   :members:

   .. automethod:: __init__


:class:`StreamClass`
====================

.. autoclass:: StreamClass
   :members:

   .. automethod:: __init__


:class:`Stream`
===============

.. autoclass:: Stream
   :members:


:class:`Clock`
==============

.. autoclass:: Clock
   :members:

   .. automethod:: __init__


:class:`EventClass`
===================

.. autoclass:: EventClass
   :members:

   .. automethod:: __init__


:class:`Event`
==============

.. autoclass:: Event
   :members:

   .. automethod:: __init__


:class:`FieldDeclaration`
=========================

.. autoclass:: FieldDeclaration
   :members:


:class:`IntegerBase`
====================

.. autoclass:: IntegerBase
   :members:


:class:`IntegerFieldDeclaration`
================================

.. autoclass:: IntegerFieldDeclaration
   :members:
   :show-inheritance:

   .. automethod:: __init__


:class:`FloatingPointFieldDeclaration`
======================================

.. autoclass:: FloatingPointFieldDeclaration
   :members:
   :show-inheritance:

   .. automethod:: __init__


:class:`EnumerationMapping`
===========================

.. autoclass:: EnumerationMapping
   :members:

   .. automethod:: __init__


:class:`EnumerationFieldDeclaration`
====================================

.. autoclass:: EnumerationFieldDeclaration
   :members:
   :show-inheritance:

   .. automethod:: __init__


:class:`StringFieldDeclaration`
===============================

.. autoclass:: StringFieldDeclaration
   :members:
   :show-inheritance:

   .. automethod:: __init__


:class:`ArrayFieldDeclaration`
==============================

.. autoclass:: ArrayFieldDeclaration
   :members:
   :show-inheritance:

   .. automethod:: __init__


:class:`SequenceFieldDeclaration`
=================================

.. autoclass:: SequenceFieldDeclaration
   :members:
   :show-inheritance:

   .. automethod:: __init__


:class:`StructureFieldDeclaration`
==================================

.. autoclass:: StructureFieldDeclaration
   :members:
   :show-inheritance:

   .. automethod:: __init__


:class:`VariantFieldDeclaration`
================================

.. autoclass:: VariantFieldDeclaration
   :members:
   :show-inheritance:

   .. automethod:: __init__


:class:`Field`
==============

.. autoclass:: Field
   :members:


:class:`IntegerField`
=====================

.. autoclass:: IntegerField
   :members:
   :show-inheritance:


:class:`FloatingPointField`
===========================

.. autoclass:: FloatingPointField
   :members:
   :show-inheritance:


:class:`EnumerationField`
=========================

.. autoclass:: EnumerationField
   :members:
   :show-inheritance:


:class:`StringField`
====================

.. autoclass:: StringField
   :members:
   :show-inheritance:


:class:`ArrayField`
===================

.. autoclass:: ArrayField
   :members:
   :show-inheritance:


:class:`SequenceField`
======================

.. autoclass:: SequenceField
   :members:
   :show-inheritance:


:class:`StructureField`
=======================

.. autoclass:: StructureField
   :members:
   :show-inheritance:


:class:`VariantField`
=====================

.. autoclass:: VariantField
   :members:
   :show-inheritance:
