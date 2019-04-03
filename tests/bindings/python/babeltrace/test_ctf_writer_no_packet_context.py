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
class CtfWriterNoPacketContextTestCase(unittest.TestCase):
    def setUp(self):
        self._expected_event_count = 100
        self._trace_path = tempfile.mkdtemp()

    def tearDown(self):
        shutil.rmtree(self._trace_path)

    def _write_trace(self):
        trace = btw.Writer(self._trace_path)
        clock = btw.Clock('test_clock')
        trace.add_clock(clock)

        integer_field_type = btw.IntegerFieldDeclaration(32)

        event_class = btw.EventClass('simple_event')
        event_class.add_field(integer_field_type, 'int_field')

        stream_class = btw.StreamClass('test_stream')
        stream_class.add_event_class(event_class)
        stream_class.clock = clock

        stream_class.packet_context_type = None
        stream = trace.create_stream(stream_class)

        for i in range(self._expected_event_count):
            event = btw.Event(event_class)
            event.payload('int_field').value = i
            stream.append_event(event)
        stream.flush()

        # It is not valid for a stream to contain more than one packet
        # if it does not have content_size/packet_size info in its packet
        # context
        event = btw.Event(event_class)
        event.payload('int_field').value = 42
        stream.append_event(event)

        flush_raises = False
        try:
            stream.flush()
        except ValueError:
            flush_raises = True
        self.assertTrue(flush_raises)
        trace.flush_metadata()

    def test_trace_no_packet_context(self):
        self._write_trace()
        traces = btr.TraceCollection()
        trace_handle = traces.add_trace(self._trace_path, 'ctf')
        self.assertIsNotNone(trace_handle)

        event_count = sum(1 for event in traces.events)
        self.assertEqual(self._expected_event_count, event_count)
