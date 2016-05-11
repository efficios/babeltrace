#!/usr/bin/env python3
#
# The MIT License (MIT)
#
# Copyright (C) 2016 - Jérémie Galarneau <jeremie.galarneau@efficios.com>
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
import babeltrace
import uuid
import shutil


class Entry(object):
    def __init__(self, stream_id, timestamp=None, end_of_packet=False):
        self.stream_id = stream_id
        self.timestamp = timestamp
        self.end_of_packet = end_of_packet


class Packet(object):
    def __init__(self, timestamps):
        self.timestamps = timestamps

clock_uuid = uuid.uuid4()

# stream_descriptions is a list of lists of Packets.
# Each stream description list is mapped to a single stream instance.
def create_trace(path, clock_uuid, stream_descriptions):
    trace = babeltrace.CTFWriter.Writer(path)
    clock = babeltrace.CTFWriter.Clock('test_clock')
    clock.uuid = clock_uuid
    trace.add_clock(clock)

    integer_field_type = babeltrace.CTFWriter.IntegerFieldDeclaration(32)

    event_class = babeltrace.CTFWriter.EventClass('simple_event')
    event_class.add_field(integer_field_type, 'int_field')

    stream_class = babeltrace.CTFWriter.StreamClass('test_stream')
    stream_class.add_event_class(event_class)
    stream_class.clock = clock

    streams = []
    stream_entries = []
    for stream_id, stream_packets in enumerate(stream_descriptions):
        stream = trace.create_stream(stream_class)
        streams.append(stream)

        for packet in stream_packets:
            for timestamp in packet.timestamps:
                stream_entries.append(Entry(stream_id, timestamp))
            # Mark the last inserted entry as the end of packet
            stream_entries[len(stream_entries) - 1].end_of_packet = True

    # Sort stream entries which will provide us with a time-ordered list of
    # events to insert in the streams.
    for entry in sorted(stream_entries, key=lambda entry: entry.timestamp):
        clock.time = entry.timestamp
        event = babeltrace.CTFWriter.Event(event_class)
        event.payload('int_field').value = entry.stream_id
        streams[entry.stream_id].append_event(event)
        if entry.end_of_packet is True:
            streams[entry.stream_id].flush()


def check_trace_expected_timestamps(trace_paths, expected_timestamps):
    traces = babeltrace.TraceCollection(intersect_mode=True)
    for trace_path in trace_paths:
        trace_handle = traces.add_trace(trace_path, 'ctf')
        if trace_handle is None:
            print('Failed to open trace at {}'.format(trace_path))
            return False
    for event in traces.events:
        expected_timestamp = expected_timestamps.pop(0)
        if event.timestamp != expected_timestamp:
            print('# Unexpected timestamp ({}), expected {}'.format(
                event.timestamp, expected_timestamp))
            return False
    return True


def print_test_result(test_number, result, description):
    result_string = 'ok' if result else 'not ok'
    result_string += ' {} - {}'.format(test_number, description)
    print(result_string)

TEST_COUNT = 3
# TAP plan
print('1..{}'.format(TEST_COUNT))

trace_path_early = tempfile.mkdtemp()

# The stream intersection of this trace is event timestamps 11, 12 and 13,
# accounting for 9 events in stream-intersection mode
print('# Creating early trace at {}'.format(trace_path_early))
create_trace(trace_path_early, clock_uuid, [
    [Packet(range(1, 7)), Packet(range(11, 18))],
    [Packet(range(8, 15)), Packet(range(22, 24)), Packet(range(30, 60))],
    [Packet(range(11, 14))]
    ])

trace_path_late = tempfile.mkdtemp()
print('# Creating late trace at {}'.format(trace_path_late))
# The stream intersection of this trace is event timestamps 100, 101, 102, 103
# and 104 accounting for 15 events in stream-intersection mode
create_trace(trace_path_late, clock_uuid, [
    [Packet(range(100, 105)), Packet(range(109, 120))],
    [Packet(range(88, 95)), Packet(range(96, 110)), Packet(range(112, 140))],
    [Packet(range(99, 105))]
    ])

expected_timestamps_early = []
for ts in range(11, 14):
    for stream in range(3):
        expected_timestamps_early.append(ts)

expected_timestamps_late = []
for ts in range(100, 105):
    for stream in range(3):
        expected_timestamps_late.append(ts)

expected_timestamps_union = (expected_timestamps_early +
                            expected_timestamps_late)

# Validate that the stream-intersection mode on a single trace returns only
# the events that are in overlapping packets across all of the trace's streams.
result = check_trace_expected_timestamps([trace_path_early],
                                            expected_timestamps_early)
print_test_result(1, result, 'Validating expected event timestamps of "early" trace')

result = check_trace_expected_timestamps([trace_path_late],
                                            expected_timestamps_late)
print_test_result(2, result, 'Validating expected event timestamps of "late" trace')

result = check_trace_expected_timestamps([trace_path_early, trace_path_late],
                                            expected_timestamps_union)
print_test_result(3, result, 'Validating expected event timestamps of the union of "early" and "late" traces')

shutil.rmtree(trace_path_early)
shutil.rmtree(trace_path_late)
