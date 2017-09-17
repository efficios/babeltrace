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

import tempfile
import babeltrace.writer as btw
import babeltrace.reader as btr
import uuid
import shutil
import unittest

class Entry(object):
    def __init__(self, stream_id, timestamp=None, end_of_packet=False):
        self.stream_id = stream_id
        self.timestamp = timestamp
        self.end_of_packet = end_of_packet


class Packet(object):
    def __init__(self, timestamps):
        self.timestamps = timestamps


class TraceIntersectionTestCase(unittest.TestCase):
    def _create_trace(self, stream_descriptions):
        trace_path = tempfile.mkdtemp()
        trace = btw.Writer(trace_path)
        clock = btw.Clock('test_clock')
        clock.uuid = self._clock_uuid
        trace.add_clock(clock)

        integer_field_type = btw.IntegerFieldDeclaration(32)

        event_class = btw.EventClass('simple_event')
        event_class.add_field(integer_field_type, 'int_field')

        stream_class = btw.StreamClass('test_stream')
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
            event = btw.Event(event_class)
            event.payload('int_field').value = entry.stream_id
            streams[entry.stream_id].append_event(event)
            if entry.end_of_packet is True:
                streams[entry.stream_id].flush()

        return trace_path

    def setUp(self):
        self._clock_uuid = uuid.uuid4()
        self._trace_path_early = self._create_trace(
            [
                [Packet(range(1, 7)), Packet(range(11, 18))],
                [Packet(range(8, 15)), Packet(range(22, 24)), Packet(range(30, 60))],
                [Packet(range(11, 14))]
            ]
        )
        self._trace_path_late = self._create_trace(
            [
                [Packet(range(100, 105)), Packet(range(109, 120))],
                [Packet(range(88, 95)), Packet(range(96, 110)), Packet(range(112, 140))],
                [Packet(range(99, 105))]
            ]
        )

        self._expected_timestamps_early = []
        for ts in range(11, 14):
            for stream in range(3):
                self._expected_timestamps_early.append(ts)

        self._expected_timestamps_late = []
        for ts in range(100, 105):
            for stream in range(3):
                self._expected_timestamps_late.append(ts)

        self._expected_timestamps_union = (self._expected_timestamps_early +
                                           self._expected_timestamps_late)

    def tearDown(self):
        shutil.rmtree(self._trace_path_early)
        shutil.rmtree(self._trace_path_late)
        pass

    @staticmethod
    def _check_trace_expected_timestamps(trace_paths, expected_timestamps):
        traces = btr.TraceCollection(intersect_mode=True)
        for trace_path in trace_paths:
            trace_handle = traces.add_trace(trace_path, 'ctf')
            if trace_handle is None:
                print('# Failed to open trace at {}'.format(trace_path))
                return False
        for event in traces.events:
            expected_timestamp = expected_timestamps.pop(0)
            if event.timestamp != expected_timestamp:
                print('# Unexpected timestamp ({}), expected {}'.format(
                    event.timestamp, expected_timestamp))
                return False
        return True

    def test_trace_early(self):
        self._check_trace_expected_timestamps([self._trace_path_early],
                                              self._expected_timestamps_early)

    def test_trace_late(self):
        self._check_trace_expected_timestamps([self._trace_path_late],
                                              self._expected_timestamps_late)

    def test_trace_intersection(self):
        self._check_trace_expected_timestamps([self._trace_path_early,
                                               self._trace_path_late],
                                              self._expected_timestamps_union)
