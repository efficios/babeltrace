# histogram.py
# 
# Babeltrace histogram example script
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

import sys
from babeltrace import *
from output_format_modules import cairoplot
from output_format_modules.pprint_table import pprint_table as pprint

# ------------------------------------------------
# 		Output settings

# number of intervals:
nbDiv = 25	# Should not be over 150
		# for usable graph output

# table output stream (file-like object):
out = sys.stdout
# -------------------------------------------------

if len(sys.argv) < 2 or len(sys.argv) > 4:
	raise TypeError("Usage: python histogram.py [ start_time [end_time] ] path/to/trace")

ctx = Context()
ret = ctx.add_trace(sys.argv[len(sys.argv)-1], "ctf")
if ret is None:
	raise IOError("Error adding trace")

# Check when to start/stop graphing
sinceBegin = True
beginTime = 0.0
if len(sys.argv) > 2:
	sinceBegin = False
	beginTime = float(sys.argv[1])
untilEnd = True
if len(sys.argv) == 4:
	untilEnd = False

# Setting iterator
bp = IterPos(SEEK_BEGIN)
ctf_it = ctf.Iterator(ctx, bp)

# Reading events
event = ctf_it.read_event()
start_time = event.get_timestamp()
time = 0
count = {}

while(event is not None):
	# Microsec.
	time = (event.get_timestamp() - start_time)/1000.0

	# Check if in range
	if not sinceBegin:
		if time < beginTime:
			# Next Event
			ret = ctf_it.next()
			if ret < 0:
				break
			event = ctf_it.read_event()
			continue
	if not untilEnd:
		if time > float(sys.argv[2]):
			break

	# Counting events per timestamp:
	if time in count:
		count[time] += 1
	else:
		count[time] = 1

	# Next Event
	ret = ctf_it.next()
	if ret < 0:
		break
	event = ctf_it.read_event()

del ctf_it

# Setting data for output
interval = (time - beginTime)/nbDiv
div_begin_time = beginTime
div_end_time = beginTime + interval
data = {}

# Prefix for string sorting, considering
# there should not be over 150 intervals.
# This would work up to 9999 intervals.
# If needed, add zeros.
prefix = 0.0001

while div_end_time <= time:
	key = str(prefix) + '[' + str(div_begin_time) + ';' + str(div_end_time) + '['
	for tmp in count:
		if tmp >= div_begin_time and tmp < div_end_time:
			if key in data:
				data[key] += count[tmp]
			else:
				data[key] = count[tmp]
	if not key in data:
		data[key] = 0
	div_begin_time = div_end_time
	div_end_time += interval
	# Prefix increment
	prefix += 0.001

table = []
x_labels = []
for key in sorted(data):
	table.append([key[key.find('['):], data[key]])
	x_labels.append(key[key.find('['):])

# Table output
table.insert(0, ["INTERVAL (us)", "COUNT"])
pprint(table, 1, out)

# Graph output
cairoplot.vertical_bar_plot ( 'histogram.svg', data, 50 + 150*nbDiv, 50*nbDiv,
	border = 20, display_values = True, grid = True,
	x_labels = x_labels, rounded_corners = True )
