#!/usr/bin/env python3
# sequence_test.py
#
# Babeltrace example script based on the Babeltrace API test script
#
# Copyright 2013 Xiaona Han
# Copyright 2012 EfficiOS Inc.
#
# Author: Danny Serres <danny.serres@efficios.com>
# Author: Xiaona Han <xiaonahappy13@163.com>
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

# This example uses the babeltrace python module
# to partially test the sequence API

import sys
from babeltrace import *

# Check for path arg:
if len(sys.argv) < 2:
	raise TypeError("Usage: sequence_test.py path/to/file")

# Create context and add trace:
ctx = Context()
trace_handle = ctx.add_trace(sys.argv[1], "ctf")
if trace_handle is None:
	raise IOError("Error adding trace")

# Listing events
lst = ctf.get_event_decl_list(trace_handle, ctx)
print("--- Event list ---")
for item in lst:
	print("event : {}".format(item.get_name()))
print("--- Done ---")

# Iter trace
bp = IterPos(SEEK_BEGIN)
ctf_it = ctf.Iterator(ctx,bp)
event = ctf_it.read_event()

while(event is not None):
	print("TS: {}, {} : {}".format(event.get_timestamp(),
		event.get_cycles(), event.get_name()))
	field = event.get_field("seq_int_field")
	if field is not None:
		print("int sequence values: {}". format(field[0].get_value()))
	field = event.get_field("seq_long_field")
	if field is not None:
		print("long sequence values: {}". format(field[0].get_value()))

	ret = ctf_it.next()
	if ret < 0:
		break
	else:
		event = ctf_it.read_event()

del ctf_it
