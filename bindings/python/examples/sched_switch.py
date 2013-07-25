#!/usr/bin/env python3
# sched_switch.py
#
# Babeltrace example script with sched_switch events
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

# The script takes one optional argument (pid)
# The script will read events based on pid and
# print the scheduler switches happening with the process.
# If no arguments are passed, it displays all the scheduler switches.
# This can be used to understand which tasks schedule out the current
# process being traced, and when it gets scheduled in again.
# The trace needs PID context (lttng add-context -k -t pid)

import sys
from babeltrace import *

if len(sys.argv) < 2 or len(sys.argv) > 3:
	raise TypeError("Usage: python sched_switch.py [pid] path/to/trace")
elif len(sys.argv) == 3:
	usePID = True
else:
	usePID = False


ctx = Context()
ret = ctx.add_trace(sys.argv[len(sys.argv)-1], "ctf")
if ret is None:
	raise IOError("Error adding trace")

# Setting iterator
bp = IterPos(SEEK_BEGIN)
ctf_it = ctf.Iterator(ctx, bp)

# Reading events
event = ctf_it.read_event()
while event is not None:
	while True:
		if event.get_name() == "sched_switch":
			# Getting scope definition
			sco = event.get_top_level_scope(ctf.scope.STREAM_EVENT_CONTEXT)
			if sco is None:
				print("ERROR: Cannot get definition scope for sched_switch")
				break # Next event

			# Getting PID
			pid_field = event.get_field_with_scope(sco, "_pid")
			pid = pid_field.get_int64()

			if ctf.field_error():
				print("ERROR: Missing PID info for sched_switch")
				break # Next event

			if usePID and (pid != long(sys.argv[1])):
				break # Next event

			sco = event.get_top_level_scope(ctf.scope.EVENT_FIELDS)

			# prev_comm
			field = event.get_field_with_scope(sco, "_prev_comm")
			prev_comm = field.get_char_array()
			if ctf.field_error():
				print("ERROR: Missing prev_comm context info")

			# prev_tid
			field = event.get_field_with_scope(sco, "_prev_tid")
			prev_tid = field.get_int64()
			if ctf.field_error():
				print("ERROR: Missing prev_tid context info")

			# prev_prio
			field = event.get_field_with_scope(sco, "_prev_prio")
			prev_prio = field.get_int64()
			if ctf.field_error():
				print("ERROR: Missing prev_prio context info")

			# prev_state
			field = event.get_field_with_scope(sco, "_prev_state")
			prev_state = field.get_int64()
			if ctf.field_error():
				print("ERROR: Missing prev_state context info")

			# next_comm
			field = event.get_field_with_scope(sco, "_next_comm")
			next_comm = field.get_char_array()
			if ctf.field_error():
				print("ERROR: Missing next_comm context info")

			# next_tid
			field = event.get_field_with_scope(sco, "_next_tid")
			next_tid = field.get_int64()
			if ctf.field_error():
				print("ERROR: Missing next_tid context info")

			# next_prio
			field = event.get_field_with_scope(sco, "_next_prio")
			next_prio = field.get_int64()
			if ctf.field_error():
				print("ERROR: Missing next_prio context info")

			# Output
			print("sched_switch, pid = {}, TS = {}, prev_comm = {},\n\t"
				"prev_tid = {}, prev_prio = {}, prev_state = {},\n\t"
				"next_comm = {}, next_tid = {}, next_prio = {}".format(
				pid, event.get_timestamp(), prev_comm, prev_tid,
				prev_prio, prev_state, next_comm, next_tid, next_prio))

		break # Next event

	# Next event
	ret = ctf_it.next()
	if ret < 0:
		break
	event = ctf_it.read_event()

del ctf_it
