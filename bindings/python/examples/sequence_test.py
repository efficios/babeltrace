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

# Create TraceCollection and add trace:
traces = TraceCollection()
trace_handle = traces.add_trace(sys.argv[1], "ctf")
if trace_handle is None:
	raise IOError("Error adding trace")

# Listing events
lst = event_declaration_list(trace_handle, traces)
print("--- Event list ---")
for item in lst:
	print("event : {}".format(item.name))
print("--- Done ---")

for event in traces.events:
	print("TS: {}, {} : {}".format(event.timestamp,
		event.cycles, event.name))

	try:
		sequence = event["seq_int_field"]
		print("int sequence values: {}". format(sequence))
	except KeyError:
		pass

	try:
		sequence = event["seq_long_field"]
		print("long sequence values: {}". format(sequence))
	except KeyError:
		pass
