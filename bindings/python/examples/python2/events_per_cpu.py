#!/usr/bin/env python2
# events_per_cpu.py
#
# Babeltrace events per cpu example script
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

# The script opens a trace and prints out CPU statistics
# for the given trace (event count per CPU, total active
# time and % of time processing events).
# It also outputs a .txt file showing each time interval
# (since the beginning of the trace) in which each CPU
# was active and the corresponding event.

import sys, multiprocessing
from output_format_modules.pprint_table import pprint_table as pprint
from babeltrace import *

if len(sys.argv) < 2:
	raise TypeError("Usage: python events_per_cpu.py path/to/trace")

# Adding trace
ctx = Context()
ret = ctx.add_trace(sys.argv[1], "ctf")
if ret is None:
	raise IOError("Error adding trace")

cpu_usage = []
nbEvents = 0
i = 0
while i < multiprocessing.cpu_count():
	cpu_usage.append([])
	i += 1

# Setting iterator
bp = IterPos(SEEK_BEGIN)
ctf_it = ctf.Iterator(ctx, bp)

# Reading events
event = ctf_it.read_event()
start_time = event.get_timestamp()

while(event is not None):

	event_name = event.get_name()
	ts = event.get_timestamp()

	# Getting cpu_id
	scope = event.get_top_level_scope(ctf.scope.STREAM_PACKET_CONTEXT)
	field = event.get_field(scope, "cpu_id")
	cpu_id = field.get_uint64()
	if ctf.field_error():
		print("ERROR: Missing cpu_id info for {}".format(event.get_name()))
	else:
		cpu_usage[cpu_id].append( (int(ts), event_name) )
		nbEvents += 1

	# Next Event
	ret = ctf_it.next()
	if ret < 0:
		break
	event = ctf_it.read_event()


# Outputting
table = []
output = open("events_per_cpu.txt", "wt")
output.write("(timestamp, event)\n")

for cpu in range(len(cpu_usage)):
	# Setting table
	event_str = str(100.0 * len(cpu_usage[cpu]) / nbEvents) + '000'
	# % is printed with 2 decimals
	table.append([cpu, len(cpu_usage[cpu]), event_str[0:event_str.find('.') + 3] + ' %'])

	# Writing to file
	output.write("\n\n\n----------------------\n")
	output.write("CPU {}\n\n".format(cpu))
	for event in cpu_usage[cpu]:
		output.write(str(event) + '\n')

# Printing table
table.insert(0, ["CPU ID", "EVENT COUNT", "TRACE EVENT %"])
pprint(table)
print("Total event count: {}".format(nbEvents))
print("Total trace time: {} ns".format(ts - start_time))

output.close()
