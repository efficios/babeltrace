#!/usr/bin/env python3
# ctf_writer.py
#
# Babeltrace CTF Writer example script.
#
# Copyright 2013 EfficiOS Inc.
#
# Author: Jérémie Galarneau <jeremie.galarneau@efficios.com>
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
# SOFTWARE.

import sys
import tempfile
import babeltrace.writer as btw


trace_path = tempfile.mkdtemp()

print("Writing trace at {}".format(trace_path))
writer = btw.Writer(trace_path)

clock = btw.Clock("A_clock")
print("Clock name is \"{}\"".format(clock.name))
clock.description = "Simple clock"
print("Clock description is \"{}\"".format(clock.description))
print("Clock frequency is {}".format(clock.frequency))
print("Clock precision is {}".format(clock.precision))
print("Clock offset_seconds is {}".format(clock.offset_seconds))
print("Clock offset is {}".format(clock.offset))
print("Clock is absolute: {}".format(clock.absolute))
print("Clock time is {}".format(clock.time))
print("Clock UUID is {}".format(clock.uuid))

writer.add_clock(clock)
writer.add_environment_field("Python_version", str(sys.version_info))

stream_class = btw.StreamClass("test_stream")
stream_class.clock = clock

event_class = btw.EventClass("SimpleEvent")

# Create a int32_t equivalent type
int32_type = btw.IntegerFieldDeclaration(32)
int32_type.signed = True

# Create a uint16_t equivalent type
uint16_type = btw.IntegerFieldDeclaration(16)
uint16_type.signed = False

# Add a custom uint16_t field in the stream's packet context
packet_context_type = stream_class.packet_context_type
print("\nFields in default packet context:")
for field in packet_context_type.fields:
    print(str(type(field[1])) + " " + field[0])
packet_context_type.add_field(uint16_type, "a_custom_packet_context_field")
stream_class.packet_context_type = packet_context_type

# Create a string type
string_type = btw.StringFieldDeclaration()

# Create a structure type containing both an integer and a string
structure_type = btw.StructureFieldDeclaration()
structure_type.add_field(int32_type, "an_integer")
structure_type.add_field(string_type, "a_string_field")
event_class.add_field(structure_type, "structure_field")

# Create a floating point type
floating_point_type = btw.FloatFieldDeclaration()
floating_point_type.exponent_digits = btw.FloatFieldDeclaration.FLT_EXP_DIG
floating_point_type.mantissa_digits = btw.FloatFieldDeclaration.FLT_MANT_DIG
event_class.add_field(floating_point_type, "float_field")

# Create an enumeration type
int10_type = btw.IntegerFieldDeclaration(10)
enumeration_type = btw.EnumerationFieldDeclaration(int10_type)
enumeration_type.add_mapping("FIRST_ENTRY", 0, 4)
enumeration_type.add_mapping("SECOND_ENTRY", 5, 5)
enumeration_type.add_mapping("THIRD_ENTRY", 6, 10)
event_class.add_field(enumeration_type, "enum_field")

# Create an array type
array_type = btw.ArrayFieldDeclaration(int10_type, 5)
event_class.add_field(array_type, "array_field")

# Create a sequence type
sequence_type = btw.SequenceFieldDeclaration(int32_type, "sequence_len")
event_class.add_field(uint16_type, "sequence_len")
event_class.add_field(sequence_type, "sequence_field")

stream_class.add_event_class(event_class)
stream = writer.create_stream(stream_class)

for i in range(100):
    event = btw.Event(event_class)

    clock.time = i * 1000
    structure_field = event.payload("structure_field")
    integer_field = structure_field.field("an_integer")
    integer_field.value = i

    string_field = structure_field.field("a_string_field")
    string_field.value = "Test string."

    float_field = event.payload("float_field")
    float_field.value = float(i) + (float(i) / 100.0)

    array_field = event.payload("array_field")
    for j in range(5):
        element = array_field.field(j)
        element.value = i + j

    event.payload("sequence_len").value = i % 10
    sequence_field = event.payload("sequence_field")
    sequence_field.length = event.payload("sequence_len")
    for j in range(event.payload("sequence_len").value):
        sequence_field.field(j).value = i + j

    enumeration_field = event.payload("enum_field")
    integer_field = enumeration_field.container
    enumeration_field.value = i % 10

    stream.append_event(event)

# Populate custom packet context field before flushing
packet_context = stream.packet_context
field = packet_context.field("a_custom_packet_context_field")
field.value = 42

stream.flush()
