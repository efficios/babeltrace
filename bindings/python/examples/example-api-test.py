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
lst = event_declaration_list(trace_handle, traces)
print("--- Event list ---")
for item in lst:
	print("event : {}".format(item.name))
print("--- Done ---")

for event in traces.events:
	print("TS: {}, {} : {}".format(event.timestamp,	event.cycles, event.name))

	if event.name == "sched_switch":
		prev_comm = event["prev_comm"]
		if prev_comm is None:
			print("ERROR: Missing prev_comm context info")
		else:
			print("sched_switch prev_comm: {}".format(prev_comm))

	if event.name == "exit_syscall":
		ret_code = event["ret"]
		if ret_code is None:
			print("ERROR: Unable to extract ret")
		else:
			print("exit_syscall ret: {}".format(ret_code))
