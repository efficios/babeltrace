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

import tempfile
#import babeltrace.writer as btw
#import babeltrace.reader as btr
import shutil
import uuid
import unittest

@unittest.skip("this is broken")
class CtfWriterTestCase(unittest.TestCase):
    def setUp(self):
        self._expected_event_count = 100
        self._trace_path = tempfile.mkdtemp()

    def tearDown(self):
        shutil.rmtree(self._trace_path)

    def _create_trace(self):
        trace = btw.Writer(self._trace_path)
        clock = btw.Clock('test_clock')
        trace.add_clock(clock)

        integer_field_type = btw.IntegerFieldDeclaration(32)

        event_class = btw.EventClass('simple_event')
        event_class.add_field(integer_field_type, 'int_field')

        stream_class = btw.StreamClass('empty_packet_stream')
        stream_class.add_event_class(event_class)
        stream_class.clock = clock

        stream = trace.create_stream(stream_class)

        for j in range(2):
            for i in range(self._expected_event_count // 2):
                val = i * (j + 1)
                clock.time = val * 200
                event = btw.Event(event_class)
                event.payload('int_field').value = val
                stream.append_event(event)

            stream.flush()

    def test_writer(self):
        self._create_trace()

        traces = btr.TraceCollection()
        trace_handle = traces.add_trace(self._trace_path, 'ctf')
        self.assertIsNotNone(trace_handle)

        event_count = sum(1 for event in traces.events)
        self.assertEqual(self._expected_event_count, event_count)

        self.assertEqual(traces.timestamp_begin, 0)
        self.assertEqual(traces.timestamp_end, 19600)
