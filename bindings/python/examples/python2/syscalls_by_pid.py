#!/usr/bin/env python2
# syscall_by_pid.py
#
# Babeltrace syscall by pid example script
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

# The script checks the number of events in the trace
# and outputs a table and a .svg histogram for the specified
# range (microseconds) or the total trace if no range specified.
# The graph is generated using the cairoplot module.

# The script checks all syscall in the trace and prints a list
# showing the number of systemcalls executed by each PID
# ordered from greatest to least number of syscalls.
# The trace needs PID context (lttng add-context -k -t pid)

import sys
from babeltrace import *
from output_format_modules.pprint_table import pprint_table as pprint

if len(sys.argv) < 2 :
	raise TypeError("Usage: python syscalls_by_pid.py path/to/trace")

ctx = Context()
ret = ctx.add_trace(sys.argv[1], "ctf")
if ret is None:
	raise IOError("Error adding trace")

data = {}

# Setting iterator
bp = IterPos(SEEK_BEGIN)
ctf_it = ctf.Iterator(ctx, bp)

# Reading events
event = ctf_it.read_event()
while event is not None:
	if event.get_name().find("sys") >= 0:
		# Getting scope definition
		sco = event.get_top_level_scope(ctf.scope.STREAM_EVENT_CONTEXT)
		if sco is None:
			print("ERROR: Cannot get definition scope for {}".format(
				event.get_name()))
		else:
			# Getting PID
			pid_field = event.get_field(sco, "_pid")
			pid = pid_field.get_int64()

			if ctf.field_error():
				print("ERROR: Missing PID info for sched_switch".format(
					event.get_name()))
			elif pid in data:
				data[pid] += 1
			else:
				data[pid] = 1
	# Next event
	ret = ctf_it.next()
	if ret < 0:
		break
	event = ctf_it.read_event()

del ctf_it

# Setting table for output
table = []
for item in data:
	table.append([data[item], item])  # [count, pid]
table.sort(reverse = True)	# [big count first, pid]
for i in range(len(table)):
	table[i].reverse()	# [pid, big count first]
table.insert(0, ["PID", "SYSCALL COUNT"])
pprint(table)
