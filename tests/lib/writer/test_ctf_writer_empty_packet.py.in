#!/usr/bin/env python3
#
# The MIT License (MIT)
#
# Copyright (C) 2017 - Jérémie Galarneau <jeremie.galarneau@efficios.com>
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

import bt_python_helper
import tempfile
import babeltrace.writer as btw
import babeltrace.reader as btr
import shutil
import uuid

TEST_COUNT = 4
EXPECTED_EVENT_COUNT = 10
trace_path = tempfile.mkdtemp()


def print_test_result(test_number, result, description):
        result_string = 'ok' if result else 'not ok'
        result_string += ' {} - {}'.format(test_number, description)
        print(result_string)

def create_trace(path):
        trace = btw.Writer(path)
        clock = btw.Clock('test_clock')
        trace.add_clock(clock)

        integer_field_type = btw.IntegerFieldDeclaration(32)

        event_class = btw.EventClass('simple_event')
        event_class.add_field(integer_field_type, 'int_field')

        stream_class = btw.StreamClass('empty_packet_stream')
        stream_class.add_event_class(event_class)
        stream_class.clock = clock

        stream = trace.create_stream(stream_class)

        for i in range(EXPECTED_EVENT_COUNT):
                event = btw.Event(event_class)
                event.payload('int_field').value = i
                stream.append_event(event)
        stream.flush()
        print_test_result(1, True,
                          'Flush a packet containing {} events'.format(EXPECTED_EVENT_COUNT))

        # The CTF writer will not be able to populate the packet context's
        # timestamp_begin and timestamp_end fields if it is asked to flush
        # without any queued events.
        try:
            stream.flush()
            empty_flush_succeeded = True
        except ValueError:
            empty_flush_succeeded = False

        print_test_result(2, not empty_flush_succeeded,
                          'Flushing an empty packet without explicitly setting packet time bounds should fail')
        packet_context = stream.packet_context
        packet_context.field('timestamp_begin').value = 1
        packet_context.field('timestamp_end').value = 123456
        try:
            stream.flush()
            empty_flush_succeeded = True
        except ValueError:
            empty_flush_succeeded = False

        print_test_result(3, empty_flush_succeeded,
                          'Flushing an empty packet after explicitly setting packet time bounds should succeed')


# TAP plan
print('1..{}'.format(TEST_COUNT))
print('# Creating trace at {}'.format(trace_path))
create_trace(trace_path)

traces = btr.TraceCollection()
trace_handle = traces.add_trace(trace_path, 'ctf')
if trace_handle is None:
        print('Failed to open trace at {}'.format(trace_path))

event_count = 0
for event in traces.events:
        event_count += 1

print_test_result(4, EXPECTED_EVENT_COUNT == event_count,
                  'Trace was found to contain {} events, expected {}'.format(
                   event_count, EXPECTED_EVENT_COUNT))

shutil.rmtree(trace_path)
