#!/usr/bin/env python3
# babeltrace_and_lttng.py
# 
# Babeltrace and LTTng example script
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


# This script uses both lttng-tools and babeltrace
# python modules.  It creates a session, enables
# events, starts tracing for 2 seconds, stops tracing,
# destroys the session and outputs the trace in the
# specified output file.
#
# WARNING: will destroy any existing trace having
#          the same name as ses_name


# ------------------------------------------------------
ses_name =	"babeltrace-lttng-test"
trace_path =	"/lttng-traces/babeltrace-lttng-trace/"
out_file =	"babeltrace-lttng-trace-text-output.txt"
# ------------------------------------------------------


import time
try:
	import babeltrace, lttng
except ImportError:
	raise ImportError(	"both babeltrace and lttng-tools "
				"python modules must be installed" )


# Errors to raise if something goes wrong
class LTTngError(Exception):
	pass
class BabeltraceError(Exception):
	pass


# LTTNG-TOOLS

# Making sure session does not already exist
lttng.destroy(ses_name)

# Creating a new session and handle
ret = lttng.create(ses_name,trace_path)
if ret < 0:
	raise LTTngError(lttng.strerror(ret))

domain = lttng.Domain()
domain.type = lttng.DOMAIN_KERNEL

han = None
han = lttng.Handle(ses_name, domain)
if han is None:
	raise LTTngError("Handle not created")


# Enabling all events
event = lttng.Event()
event.type = lttng.EVENT_ALL
event.loglevel_type = lttng.EVENT_LOGLEVEL_ALL
ret = lttng.enable_event(han, event, None)
if ret < 0:
	raise LTTngError(lttng.strerror(ret))

# Start, wait, stop
ret = lttng.start(ses_name)
if ret < 0:
	raise LTTngError(lttng.strerror(ret))
print("Tracing...")
time.sleep(2)
print("Stopped.")
ret = lttng.stop(ses_name)
if ret < 0:
	raise LTTngError(lttng.strerror(ret))


# Destroying tracing session
ret = lttng.destroy(ses_name)
if ret < 0:
	raise LTTngError(lttng.strerror(ret))


# BABELTRACE

# Create TraceCollecion and add trace:
traces = babeltrace.TraceCollection()
ret = traces.add_trace(trace_path + "/kernel", "ctf")
if ret is None:
	raise BabeltraceError("Error adding trace")

# Reading events from trace
# and outputting timestamps and event names
# in out_file
print("Writing trace file...")
output = open(out_file, "wt")

for event in traces.events:
	output.write("TS: {}, {} : {}\n".format(event.get_timestamp(),
		event.get_cycles(), event.get_name()))

# Closing file
output.close()

print("Done.")
