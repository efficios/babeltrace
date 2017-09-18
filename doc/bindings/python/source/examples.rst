.. _examples:

********
Examples
********

This section presents a few short and straightforward examples
of Babeltrace legacy Python bindings usage.

The examples are divided into two categories: those which demonstrate
the :ref:`reader API <reader-api>`, and those which demonstrate
the :ref:`CTF writer API <ctf-writer-api>`.


.. _reader-api-examples:

Reader API examples
===================

The :ref:`reader API <reader-api>` includes everything needed to open
traces and iterate on events in order.


Open one trace and print all event names
----------------------------------------

This example shows how to open a single CTF trace, iterate on all the
events, and print their names.

.. code-block:: python

   import babeltrace.reader
   import sys


   # get the trace path from the first command line argument
   trace_path = sys.argv[1]

   trace_collection = babeltrace.reader.TraceCollection()

   trace_collection.add_trace(trace_path, 'ctf')

   for event in trace_collection.events:
       print(event.name)


Open multiple traces and print all event field names
----------------------------------------------------

This example opens multiple CTF traces (their paths are provided as
command line arguments), iterates on all their correlated events in
order, and prints a list of their field names.

.. code-block:: python

   import babeltrace.reader
   import sys


   trace_collection = babeltrace.reader.TraceCollection()

   for path in sys.argv[1:]:
       trace_collection.add_trace(path, 'ctf')

   for event in trace_collection.events:
       print(', '.join(event.keys()))


Print a specific event field
----------------------------

Reading the field value of an :class:`babeltrace.reader.Event` object
is done by using its :class:`dict`-like interface:

.. code-block:: python

   field_value = event['field_name']

As such, you can use Python's ``in`` keyword to verify if a given
event contains a given field name:

.. code-block:: python

   if 'field_name' in event:
       # ...

The following example iterates on the events of a trace, and prints the
value of the ``fd`` field if it's available.

.. code-block:: python

   import babeltrace.reader
   import sys

   # get the trace path from the first command line argument
   trace_path = sys.argv[1]

   trace_collection = babeltrace.reader.TraceCollection()

   trace_collection.add_trace(trace_path, 'ctf')

   for event in trace_collection.events:
       if 'fd' in event:
           print(event['fd'])

Beware that different fields of the same event may share the same name
if they are in different scopes. In this case, the :class:`dict`-like
interface prioritizes event payload fields before event context fields,
event context fields before stream event context fields, and so on
(see :class:`babeltrace.reader.Event` for this exact list of
priorities). It is possible to get the value of an event's field
within a specific scope using
:meth:`babeltrace.reader.Event.field_with_scope`:

.. code-block:: python

   import babeltrace.reader
   import babeltrace.common

   # ...

   field_value = event.field_with_scope('field_name',
                                        babeltrace.common.CTFScope.EVENT_CONTEXT)


Bonus: top 5 running processes using LTTng
------------------------------------------

Since `LTTng <http://lttng.org/>`_ produces CTF traces, the
Babeltrace Python binding can read LTTng traces.

This somewhat more complex example reads a whole LTTng Linux kernel
trace, and outputs the short names of the top 5 running processes on
CPU 0 during the whole trace.

.. code-block:: python

   from collections import Counter
   import babeltrace.reader
   import sys


   # a trace collection holds one or more traces
   col = babeltrace.reader.TraceCollection()

   # add the trace provided by the user (first command line argument)
   # (LTTng traces always have the 'ctf' format)
   if col.add_trace(sys.argv[1], 'ctf') is None:
       raise RuntimeError('Cannot add trace')

   # this counter dict will hold execution times:
   #
   #   task command name -> total execution time (ns)
   exec_times = Counter()

   # this holds the last `sched_switch` timestamp
   last_ts = None

   # iterate on events
   for event in col.events:
       # keep only `sched_switch` events
       if event.name != 'sched_switch':
           continue

       # keep only events which happened on CPU 0
       if event['cpu_id'] != 0:
           continue

       # event timestamp
       cur_ts = event.timestamp

       if last_ts is None:
           # we start here
           last_ts = cur_ts

       # previous task command (short) name
       prev_comm = event['prev_comm']

       # initialize entry in our dict if not yet done
       if prev_comm not in exec_times:
           exec_times[prev_comm] = 0

       # compute previous command execution time
       diff = cur_ts - last_ts

       # update execution time of this command
       exec_times[prev_comm] += diff

       # update last timestamp
       last_ts = cur_ts

   # print top 5
   for name, ns in exec_times.most_common(5):
       s = ns / 1000000000
       print('{:20}{} s'.format(name, s))


Inspect event declarations and their field declarations
-------------------------------------------------------

When :meth:`babeltrace.reader.TraceCollection.add_trace` is called
and a trace is successfully opened and added, a corresponding
:class:`babeltrace.reader.TraceHandle` object for this trace is
returned. It is then possible to iterate on the event declarations of
this trace handle using :attr:`babeltrace.reader.TraceHandle.events`.
Each generated :class:`babeltrace.reader.EventDeclaration` object
contains common properties for this type of event, including its
field declarations. This is useful for inspecting the available
events of a trace, and their "signature" in terms of fields, before
iterating its actual events.

This example adds a trace to a trace collection, and uses the returned
trace handle to iterate on its event declarations. The goal here is to
make sure the ``sched_switch`` event exists, and that it contains
at least the following fields:

* ``prev_comm``, which should be an array of 8-bit integers
* ``prev_tid``, which should be an integer

.. code-block:: python

   import babeltrace.reader as btr
   import sys


   def validate_sched_switch_fields(event_decl):
       found_prev_comm = False
       found_prev_tid = False

       for field_decl in event_decl.fields:
           if field_decl.name == 'prev_comm':
               if isinstance(field_decl, btr.ArrayFieldDeclaration):
                   elem_decl = field_decl.element_declaration

                   if isinstance(elem_decl, btr.IntegerFieldDeclaration):
                       if elem_decl.size == 8:
                           found_prev_comm = True
           elif field_decl.name == 'prev_tid':
               if isinstance(field_decl, btr.IntegerFieldDeclaration):
                   found_prev_tid = True

       return found_prev_comm and found_prev_tid


   # get the trace path from the first command line argument
   trace_path = sys.argv[1]

   trace_collection = btr.TraceCollection()
   trace_handle = trace_collection.add_trace(trace_path, 'ctf')
   sched_switch_found = False

   for event_decl in trace_handle.events:
       if event_decl.name == 'sched_switch':
           if validate_sched_switch_fields(event_decl):
               sched_switch_found = True
               break

   print('trace path: {}'.format(trace_handle.path))

   if sched_switch_found:
       print('found sched_switch!')
   else:
       print('could not find sched_switch')


.. _ctf-writer-api-examples:

CTF writer API examples
=======================

The :ref:`CTF writer API <ctf-writer-api>` is a set of classes which
allows a Python script to write complete
`CTF <http://www.efficios.com/ctf>`_ (Common Trace Format) traces.


One trace, one stream, one event, one field
-------------------------------------------

This is the most simple example of using the CTF writer API. It creates
one writer (responsible for writing one trace), then uses it to create
one stream. One event with a single field is appended to this single
stream, and everything is flushed.

The trace is written in a temporary directory (its path is printed
at the beginning of the script).

.. code-block:: python

   import babeltrace.writer as btw
   import tempfile


   # temporary directory holding the CTF trace
   trace_path = tempfile.mkdtemp()

   print('trace path: {}'.format(trace_path))

   # our writer
   writer = btw.Writer(trace_path)

   # create one default clock and register it to the writer
   clock = btw.Clock('my_clock')
   clock.description = 'this is my clock'
   writer.add_clock(clock)

   # create one default stream class and assign our clock to it
   stream_class = btw.StreamClass('my_stream')
   stream_class.clock = clock

   # create one default event class
   event_class = btw.EventClass('my_event')

   # create one 32-bit signed integer field
   int32_field_decl = btw.IntegerFieldDeclaration(32)
   int32_field_decl.signed = True

   # add this field declaration to our event class
   event_class.add_field(int32_field_decl, 'my_field')

   # register our event class to our stream class
   stream_class.add_event_class(event_class)

   # create our single event, based on our event class
   event = btw.Event(event_class)

   # assign an integer value to our single field
   event.payload('my_field').value = -23

   # create our single stream
   stream = writer.create_stream(stream_class)

   # append our single event to our single stream
   stream.append_event(event)

   # flush the stream
   stream.flush()


Basic CTF fields
----------------

This example writes a few events with basic CTF fields: integers,
floating point numbers, enumerations and strings.

The trace is written in a temporary directory (its path is printed
at the beginning of the script).

.. code-block:: python

   import babeltrace.writer as btw
   import babeltrace.common
   import tempfile
   import math


   trace_path = tempfile.mkdtemp()

   print('trace path: {}'.format(trace_path))


   writer = btw.Writer(trace_path)

   clock = btw.Clock('my_clock')
   clock.description = 'this is my clock'
   writer.add_clock(clock)

   stream_class = btw.StreamClass('my_stream')
   stream_class.clock = clock

   event_class = btw.EventClass('my_event')

   # 32-bit signed integer field declaration
   int32_field_decl = btw.IntegerFieldDeclaration(32)
   int32_field_decl.signed = True
   int32_field_decl.base = btw.IntegerBase.HEX

   # 5-bit unsigned integer field declaration
   uint5_field_decl = btw.IntegerFieldDeclaration(5)
   uint5_field_decl.signed = False

   # IEEE 754 single precision floating point number field declaration
   float_field_decl = btw.FloatingPointFieldDeclaration()
   float_field_decl.exponent_digits = btw.FloatingPointFieldDeclaration.FLT_EXP_DIG
   float_field_decl.mantissa_digits = btw.FloatingPointFieldDeclaration.FLT_MANT_DIG

   # enumeration field declaration (based on the 5-bit unsigned integer above)
   enum_field_decl = btw.EnumerationFieldDeclaration(uint5_field_decl)
   enum_field_decl.add_mapping('DAZED', 3, 11)
   enum_field_decl.add_mapping('AND', 13, 13)
   enum_field_decl.add_mapping('CONFUSED', 17, 30)

   # string field declaration
   string_field_decl = btw.StringFieldDeclaration()
   string_field_decl.encoding = babeltrace.common.CTFStringEncoding.UTF8

   event_class.add_field(int32_field_decl, 'my_int32_field')
   event_class.add_field(uint5_field_decl, 'my_uint5_field')
   event_class.add_field(float_field_decl, 'my_float_field')
   event_class.add_field(enum_field_decl, 'my_enum_field')
   event_class.add_field(int32_field_decl, 'another_int32_field')
   event_class.add_field(string_field_decl, 'my_string_field')

   stream_class.add_event_class(event_class)

   stream = writer.create_stream(stream_class)

   # create and append first event
   event = btw.Event(event_class)
   event.payload('my_int32_field').value = 0xbeef
   event.payload('my_uint5_field').value = 17
   event.payload('my_float_field').value = -math.pi
   event.payload('my_enum_field').value = 8  # label: 'DAZED'
   event.payload('another_int32_field').value = 0x20141210
   event.payload('my_string_field').value = 'Hello, World!'
   stream.append_event(event)

   # create and append second event
   event = btw.Event(event_class)
   event.payload('my_int32_field').value = 0x12345678
   event.payload('my_uint5_field').value = 31
   event.payload('my_float_field').value = math.e
   event.payload('my_enum_field').value = 28  # label: 'CONFUSED'
   event.payload('another_int32_field').value = -1
   event.payload('my_string_field').value = trace_path
   stream.append_event(event)

   stream.flush()


Static array and sequence fields
--------------------------------

This example demonstrates how to write static array and sequence
fields. A static array has a fixed length, whereas a sequence reads
its length dynamically from another (integer) field.

In this example, an event is appended to a single stream, in which
three fields are present:

* ``seqlen``, the dynamic length of the sequence ``seq`` (set to the
  number of command line arguments)
* ``array``, a static array of 23 16-bit unsigned integers
* ``seq``, a sequence of ``seqlen`` strings, where the strings are
  the command line arguments

The trace is written in a temporary directory (its path is printed
at the beginning of the script).

.. code-block:: python

   import babeltrace.writer as btw
   import babeltrace.common
   import tempfile
   import sys


   trace_path = tempfile.mkdtemp()

   print('trace path: {}'.format(trace_path))


   writer = btw.Writer(trace_path)

   clock = btw.Clock('my_clock')
   clock.description = 'this is my clock'
   writer.add_clock(clock)

   stream_class = btw.StreamClass('my_stream')
   stream_class.clock = clock

   event_class = btw.EventClass('my_event')

   # 16-bit unsigned integer field declaration
   uint16_field_decl = btw.IntegerFieldDeclaration(16)
   uint16_field_decl.signed = False

   # array field declaration (23 16-bit unsigned integers)
   array_field_decl = btw.ArrayFieldDeclaration(uint16_field_decl, 23)

   # string field declaration
   string_field_decl = btw.StringFieldDeclaration()
   string_field_decl.encoding = babeltrace.common.CTFStringEncoding.UTF8

   # sequence field declaration of strings (length will be the `seqlen` field)
   seq_field_decl = btw.SequenceFieldDeclaration(string_field_decl, 'seqlen')

   event_class.add_field(uint16_field_decl, 'seqlen')
   event_class.add_field(array_field_decl, 'array')
   event_class.add_field(seq_field_decl, 'seq')

   stream_class.add_event_class(event_class)

   stream = writer.create_stream(stream_class)

   # create event
   event = btw.Event(event_class)

   # set sequence length field
   event.payload('seqlen').value = len(sys.argv)

   # get array field
   array_field = event.payload('array')

   # populate array field
   for i in range(array_field_decl.length):
       array_field.field(i).value = i * i

   # get sequence field
   seq_field = event.payload('seq')

   # assign sequence field's length field
   seq_field.length = event.payload('seqlen')

   # populate sequence field
   for i in range(seq_field.length.value):
       seq_field.field(i).value = sys.argv[i]

   # append event
   stream.append_event(event)

   stream.flush()


Structure fields
----------------

A CTF structure is an ordered map of field names to actual fields, just
like C structures. In fact, an event's payload is a structure field,
so structure fields may contain other structure fields, and so on.

This examples shows how to create a structure field from a structure
field declaration, populate it, and write an event containing it as
a payload field.

The trace is written in a temporary directory (its path is printed
at the beginning of the script).

.. code-block:: python

   import babeltrace.writer as btw
   import babeltrace.common
   import tempfile


   trace_path = tempfile.mkdtemp()

   print('trace path: {}'.format(trace_path))


   writer = btw.Writer(trace_path)

   clock = btw.Clock('my_clock')
   clock.description = 'this is my clock'
   writer.add_clock(clock)

   stream_class = btw.StreamClass('my_stream')
   stream_class.clock = clock

   event_class = btw.EventClass('my_event')

   # 32-bit signed integer field declaration
   int32_field_decl = btw.IntegerFieldDeclaration(32)
   int32_field_decl.signed = True

   # string field declaration
   string_field_decl = btw.StringFieldDeclaration()
   string_field_decl.encoding = babeltrace.common.CTFStringEncoding.UTF8

   # structure field declaration
   struct_field_decl = btw.StructureFieldDeclaration()

   # add field declarations to our structure field declaration
   struct_field_decl.add_field(int32_field_decl, 'field_one')
   struct_field_decl.add_field(string_field_decl, 'field_two')
   struct_field_decl.add_field(int32_field_decl, 'field_three')

   event_class.add_field(struct_field_decl, 'my_struct')
   event_class.add_field(string_field_decl, 'my_string')

   stream_class.add_event_class(event_class)

   stream = writer.create_stream(stream_class)

   # create event
   event = btw.Event(event_class)

   # get event's structure field
   struct_field = event.payload('my_struct')

   # populate this structure field
   struct_field.field('field_one').value = 23
   struct_field.field('field_two').value = 'Achilles Last Stand'
   struct_field.field('field_three').value = -1534

   # set event's string field
   event.payload('my_string').value = 'Tangerine'

   # append event
   stream.append_event(event)

   stream.flush()


Variant fields
--------------

The CTF variant is the most versatile field type. It acts as a
placeholder for any other type. Which type is selected depends on the
current value of an outer enumeration field, known as a *tag* from the
variant's point of view.

Variants are typical constructs in communication protocols with
dynamic types. For example, `BSON <http://bsonspec.org/spec.html>`_,
the protocol used by `MongoDB <http://www.mongodb.org/>`_, has specific
numeric IDs for each element type.

This examples shows how to create a CTF variant field. The tag, an
enumeration field, must also be created and associated with the
variant. In this case, the tag selects between three types: a
32-bit signed integer, a string, or a floating point number.

The trace is written in a temporary directory (its path is printed
at the beginning of the script).

.. code-block:: python

   import babeltrace.writer as btw
   import babeltrace.common
   import tempfile


   trace_path = tempfile.mkdtemp()

   print('trace path: {}'.format(trace_path))


   writer = btw.Writer(trace_path)

   clock = btw.Clock('my_clock')
   clock.description = 'this is my clock'
   writer.add_clock(clock)

   stream_class = btw.StreamClass('my_stream')
   stream_class.clock = clock

   event_class = btw.EventClass('my_event')

   # 32-bit signed integer field declaration
   int32_field_decl = btw.IntegerFieldDeclaration(32)
   int32_field_decl.signed = True

   # string field declaration
   string_field_decl = btw.StringFieldDeclaration()
   string_field_decl.encoding = babeltrace.common.CTFStringEncoding.UTF8

   # IEEE 754 single precision floating point number field declaration
   float_field_decl = btw.FloatingPointFieldDeclaration()
   float_field_decl.exponent_digits = btw.FloatingPointFieldDeclaration.FLT_EXP_DIG
   float_field_decl.mantissa_digits = btw.FloatingPointFieldDeclaration.FLT_MANT_DIG

   # enumeration field declaration (variant's tag)
   enum_field_decl = btw.EnumerationFieldDeclaration(int32_field_decl)
   enum_field_decl.add_mapping('INT', 0, 0)
   enum_field_decl.add_mapping('STRING', 1, 1)
   enum_field_decl.add_mapping('FLOAT', 2, 2)

   # variant field declaration (variant's tag field will be named `vartag`)
   variant_field_decl = btw.VariantFieldDeclaration(enum_field_decl, 'vartag')

   # register selectable fields to variant
   variant_field_decl.add_field(int32_field_decl, 'INT')
   variant_field_decl.add_field(string_field_decl, 'STRING')
   variant_field_decl.add_field(float_field_decl, 'FLOAT')

   event_class.add_field(enum_field_decl, 'vartag')
   event_class.add_field(variant_field_decl, 'var')

   stream_class.add_event_class(event_class)

   stream = writer.create_stream(stream_class)

   # first event: integer is selected
   event = btw.Event(event_class)
   tag_field = event.payload('vartag')
   tag_field.value = 0
   event.payload('var').field(tag_field).value = 23
   stream.append_event(event)

   # second event: string is selected
   event = btw.Event(event_class)
   tag_field = event.payload('vartag')
   tag_field.value = 1
   event.payload('var').field(tag_field).value = 'The Battle of Evermore'
   stream.append_event(event)

   # third event: floating point number is selected
   event = btw.Event(event_class)
   tag_field = event.payload('vartag')
   tag_field.value = 2
   event.payload('var').field(tag_field).value = -15.34
   stream.append_event(event)

   stream.flush()
