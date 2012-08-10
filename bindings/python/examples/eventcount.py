# eventcount.py
# 
# Babeltrace event count example script
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

# The script prints a count of specified events and
# their related tid's in a given trace.
# The trace needs TID context (lttng add-context -k -t tid)

import sys
from babeltrace import *
from output_format_modules.pprint_table import pprint_table as pprint

if len(sys.argv) < 3:
	raise TypeError("Usage: python eventcount.py event1 [event2 ...] path/to/trace")

ctx = Context()
ret = ctx.add_trace(sys.argv[len(sys.argv)-1], "ctf")
if ret is None:
	raise IOError("Error adding trace")

counts = {}

# Setting iterator
bp = IterPos(SEEK_BEGIN)
ctf_it = ctf.Iterator(ctx, bp)

# Reading events
event = ctf_it.read_event()
while(event is not None):
	for event_type in sys.argv[1:len(sys.argv)-1]:
		if event_type == event.get_name():

			# Getting scope definition
			sco = event.get_top_level_scope(ctf.scope.STREAM_EVENT_CONTEXT)
			if sco is None:
				print("ERROR: Cannot get definition scope for {}".format(
					event.get_name()))
				continue

			# Getting TID
			tid_field = event.get_field(sco, "_tid")
			tid = tid_field.get_int64()

			if ctf.field_error():
				print("ERROR: Missing TID info for {}".format(
					event.get_name()))
				continue

			tmp = (tid, event.get_name())

			if tmp in counts:
				counts[tmp] += 1
			else:
				counts[tmp] = 1

	# Next event
	ret = ctf_it.next()
	if ret < 0:
		break
	event = ctf_it.read_event()

del ctf_it

# Appending data to table for output
table = []
for item in counts:
	table.append([item[0], item[1], counts[item]])
table = sorted(table)
table.insert(0,["TID", "EVENT", "COUNT"])
pprint(table, 2)
