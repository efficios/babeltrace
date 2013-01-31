#!/usr/bin/env python2
# softirqtimes.py
#
# Babeltrace time of softirqs example script
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

# The script checks the trace for the amount of time
# spent from each softirq_raise to softirq_exit.
# It prints out the min, max (with timestamp),
# average times, the standard deviation and the total count.
# Using the cairoplot module, a .svg graph is also outputted
# showing the taken time in function of the time since the
# beginning of the trace.

import sys, math
from output_format_modules import cairoplot
from babeltrace import *

if len(sys.argv) < 2:
	raise TypeError("Usage: python softirqtimes.py path/to/trace")

ctx = Context()
ret = ctx.add_trace(sys.argv[1], "ctf")
if ret is None:
	raise IOError("Error adding trace")

time_taken = []
graph_data = []
max_time = (0.0, 0.0) # (val, ts)

# tmp template: {(cpu_id,  vec):TS raise}
tmp = {}
largest_val = 0

# Setting iterator
bp = IterPos(SEEK_BEGIN)
ctf_it = ctf.Iterator(ctx, bp)

# Reading events
event = ctf_it.read_event()
start_time = event.get_timestamp()
while(event is not None):

	event_name = event.get_name()
	error = True
	appendNext = False

	if event_name == 'softirq_raise' or  event_name == 'softirq_exit':
		# Recover cpu_id and vec values to make a key to tmp
		error = False
		scope = event.get_top_level_scope(ctf.scope.STREAM_PACKET_CONTEXT)
		field = event.get_field(scope, "cpu_id")
		cpu_id = field.get_uint64()
		if ctf.field_error():
			print("ERROR: Missing cpu_id info for {}".format(
				event.get_name()))
			error = True

		scope = event.get_top_level_scope(ctf.scope.EVENT_FIELDS)
		field = event.get_field(scope, "_vec")
		vec = field.get_uint64()
		if ctf.field_error():
			print("ERROR: Missing vec info for {}".format(
				event.get_name()))
			error = True
		key = (cpu_id, vec)

	if event_name == 'softirq_raise' and not error:
		# Add timestamp to tmp
		if key in tmp:
			# If key already exists
			i = 0
			while True:
				# Add index
				key = (cpu_id, vec, i)
				if key in tmp:
					i += 1
					continue
				if i > largest_val:
					largest_val = i
				break

		tmp[key] = event.get_timestamp()

	if event_name == 'softirq_exit' and not error:
		# Saving data for output
		# Key check
		if not (key in tmp):
			i = 0
			while i <= largest_val:
				key = (key[0], key[1], i)
				if key in tmp:
					break
				i += 1

		raise_timestamp = tmp[key]
		time_data = event.get_timestamp() - tmp.pop(key)
		if time_data > max_time[0]:
			# max_time = (val, ts)
			max_time = (time_data, raise_timestamp)
		time_taken.append(time_data)
		graph_data.append((raise_timestamp - start_time, time_data))

	# Next Event
	ret = ctf_it.next()
	if ret < 0:
		break
	event = ctf_it.read_event()


del ctf_it

# Standard dev. calc.
try:
	mean = sum(time_taken)/float(len(time_taken))
except ZeroDivisionError:
	raise TypeError("empty data")
deviations_squared = []
for x in time_taken:
	deviations_squared.append(math.pow((x - mean), 2))
try:
	stddev = math.sqrt(sum(deviations_squared) / (len(deviations_squared) - 1))
except ZeroDivisionError:
	stddev = '-'

# Terminal output
print("AVG TIME: {} ns".format(mean))
print("MIN TIME: {} ns".format(min(time_taken)))
print("MAX TIME: {} ns, TS: {}".format(max_time[0], max_time[1]))
print("STD DEV: {}".format(stddev))
print("TOTAL COUNT: {}".format(len(time_taken)))

# Graph output
cairoplot.scatter_plot ( 'softirqtimes.svg', data = graph_data,
	width = 5000, height = 4000, border = 20, axis = True,
	grid = True, series_colors = ["red"] )
