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

import sys
import tempfile
from babeltrace import *

trace_path = tempfile.mkdtemp()

print("Writing trace at {}".format(trace_path))
writer = CTFWriter.Writer(trace_path)

clock = CTFWriter.Clock("A_clock")
clock.description = "Simple clock"

writer.add_clock(clock)
writer.add_environment_field("Python_version", str(sys.version_info))

stream_class = CTFWriter.StreamClass("test_stream")
stream_class.clock = clock

event_class = CTFWriter.EventClass("SimpleEvent")

# Create a int32_t equivalent type
int32_type = CTFWriter.IntegerFieldDeclaration(32)
int32_type.signed = True

# Create a string type
string_type = CTFWriter.StringFieldDeclaration()

# Create a structure type containing both an integer and a string
structure_type = CTFWriter.StructureFieldDeclaration()
structure_type.add_field(int32_type, "an_integer")
structure_type.add_field(string_type, "a_string_field")
event_class.add_field(structure_type, "structure_field")

# Create a floating point type
floating_point_type = CTFWriter.FloatFieldDeclaration()
floating_point_type.exponent_digits = CTFWriter.FloatFieldDeclaration.FLT_EXP_DIG
floating_point_type.mantissa_digits = CTFWriter.FloatFieldDeclaration.FLT_MANT_DIG
event_class.add_field(floating_point_type, "float_field")

# Create an enumeration type
int10_type = CTFWriter.IntegerFieldDeclaration(10)
enumeration_type = CTFWriter.EnumerationFieldDeclaration(int10_type)
enumeration_type.add_mapping("FIRST_ENTRY", 0, 4)
enumeration_type.add_mapping("SECOND_ENTRY", 5, 5)
enumeration_type.add_mapping("THIRD_ENTRY", 6, 10)
event_class.add_field(enumeration_type, "enum_field")

# Create an array type
array_type = CTFWriter.ArrayFieldDeclaration(int10_type, 5)
event_class.add_field(array_type, "array_field")

stream_class.add_event_class(event_class)
stream = writer.create_stream(stream_class)

for i in range(100):
	event = CTFWriter.Event(event_class)

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

	enumeration_field = event.payload("enum_field")
	integer_field = enumeration_field.container
	integer_field.value = i % 10

	stream.append_event(event)

stream.flush()
