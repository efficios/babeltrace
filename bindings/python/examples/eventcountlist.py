# eventcountlist.py
# 
# Babeltrace event count list example script
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

# The script prints a count and rate of events.
# It also outputs a bar graph of count per event, using the cairoplot module.

import sys
from babeltrace import *
from output_format_modules import cairoplot
from output_format_modules.pprint_table import pprint_table as pprint

# Check for path arg:
if len(sys.argv) < 2:
	raise TypeError("Usage: python eventcountlist.py path/to/trace")

ctx = Context()
ret = ctx.add_trace(sys.argv[1], "ctf")
if ret is None:
	raise IOError("Error adding trace")

# Events and their assossiated count
# will be stored as a dict:
events_count = {}

# Setting iterator:
bp = IterPos(SEEK_BEGIN)
ctf_it = ctf.Iterator(ctx,bp)

prev_event = None
event = ctf_it.read_event()

start_time = event.get_timestamp()

# Reading events:
while(event is not None):
	if event.get_name() in events_count:
		events_count[event.get_name()] += 1
	else:
		events_count[event.get_name()] = 1

	ret = ctf_it.next()
	if ret < 0:
		break
	else:
		prev_event = event
		event = ctf_it.read_event()

if event:
	total_time = event.get_timestamp() - start_time
else:
	total_time = prev_event.get_timestamp() - start_time

del ctf_it

# Printing encountered events with respective count and rate:
print("Total time: {} ns".format(total_time))
table = [["EVENT", "COUNT", "RATE (Hz)"]]
for item in sorted(events_count.iterkeys()):
	tmp = [item, events_count[item],
		events_count[item]/(total_time/1000000000.0)]
	table.append(tmp)
pprint(table)

# Exporting data as bar graph
cairoplot.vertical_bar_plot ( 'eventcountlist.svg', events_count, 50+85*len(events_count),
	800, border = 20, display_values = True, grid = True,
	rounded_corners = True,
	x_labels = sorted(events_count.keys()) )
