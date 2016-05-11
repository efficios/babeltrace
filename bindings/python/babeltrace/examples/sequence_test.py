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
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
# SOFTWARE.

# This example uses the babeltrace python module
# to partially test the sequence API

import sys
import babeltrace.reader


# Check for path arg:
if len(sys.argv) < 2:
    raise TypeError("Usage: sequence_test.py path/to/file")

# Create TraceCollection and add trace:
traces = babeltrace.reader.TraceCollection()
trace_handle = traces.add_trace(sys.argv[1], "ctf")
if trace_handle is None:
    raise IOError("Error adding trace")

# Listing events
print("--- Event list ---")
for event_declaration in trace_handle.events:
    print("event : {}".format(event_declaration.name))
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
