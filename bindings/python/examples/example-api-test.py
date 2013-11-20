#!/usr/bin/env python3
# example_api_test.py
#
# Babeltrace example script based on the Babeltrace API test script
#
# Copyright 2012 EfficiOS Inc.
#
# Author: Danny Serres <danny.serres@efficios.com>
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
# to partially test the api.

import sys
from babeltrace import *

# Check for path arg:
if len(sys.argv) < 2:
	raise TypeError("Usage: python example-api-test.py path/to/file")

# Create TraceCollection and add trace:
traces = TraceCollection()
trace_handle = traces.add_trace(sys.argv[1], "ctf")
if trace_handle is None:
	raise IOError("Error adding trace")

# Listing events
lst = CTFReader.get_event_decl_list(trace_handle, traces)
print("--- Event list ---")
for item in lst:
	print("event : {}".format(item.get_name()))
print("--- Done ---")

for event in traces.events:
	print("TS: {}, {} : {}".format(event.get_timestamp(),
		event.get_cycles(), event.get_name()))

	if event.get_name() == "sched_switch":
		prev_field = event.get_field("prev_comm")
		if prev_field is None:
			print("ERROR: Missing prev_comm context info")
		else:
			prev_comm = prev_field[0].get_value()
			if prev_comm is not None:
				print("sched_switch prev_comm: {}".format(prev_comm))

	if event.get_name() == "exit_syscall":
		ret_field = event.get_field("ret")
		if ret_field is None:
			print("ERROR: Unable to extract ret")
		else:
			ret_code = ret_field[0].get_value()
			if ret_code is not None:
				print("exit_syscall ret: {}".format(ret_code))

