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
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
# SOFTWARE.

# The script takes one optional argument (pid)
# The script will read events based on pid and
# print the scheduler switches happening with the process.
# If no arguments are passed, it displays all the scheduler switches.
# This can be used to understand which tasks schedule out the current
# process being traced, and when it gets scheduled in again.
# The trace needs PID context (lttng add-context -k -t pid)

import sys
import babeltrace.reader
import babeltrace.common


if len(sys.argv) < 2 or len(sys.argv) > 3:
    raise TypeError("Usage: python sched_switch.py [pid] path/to/trace")
elif len(sys.argv) == 3:
    filterPID = True
else:
    filterPID = False

traces = babeltrace.reader.TraceCollection()
ret = traces.add_trace(sys.argv[len(sys.argv) - 1], "ctf")
if ret is None:
    raise IOError("Error adding trace")

for event in traces.events:
    if event.name != "sched_switch":
        continue

    # Getting PID
    pid = event.field_with_scope("pid", babeltrace.common.CTFScope.STREAM_EVENT_CONTEXT)
    if pid is None:
        print("ERROR: Missing PID info for sched_switch")
        continue  # Next event

    if filterPID and (pid != long(sys.argv[1])):
        continue  # Next event

    prev_comm = event["prev_comm"]
    prev_tid = event["prev_tid"]
    prev_prio = event["prev_prio"]
    prev_state = event["prev_state"]
    next_comm = event["next_comm"]
    next_tid = event["next_tid"]
    next_prio = event["next_prio"]

    # Output
    print("sched_switch, pid = {}, TS = {}, prev_comm = {},\n\t"
          "prev_tid = {}, prev_prio = {}, prev_state = {},\n\t"
          "next_comm = {}, next_tid = {}, next_prio = {}".format(
              pid, event.timestamp, prev_comm, prev_tid,
              prev_prio, prev_state, next_comm, next_tid, next_prio))
