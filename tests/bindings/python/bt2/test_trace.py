#
# Copyright (C) 2019 EfficiOS Inc.
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; only version 2
# of the License.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
#

import unittest
from utils import get_default_trace_class


class TraceTestCase(unittest.TestCase):
    def setUp(self):
        self._tc = get_default_trace_class()

    def test_create_default(self):
        trace = self._tc()
        self.assertEqual(trace.name, None)

    def test_create_full(self):
        trace = self._tc(name='my name')
        self.assertEqual(trace.name, 'my name')

    def test_create_invalid_name(self):
        with self.assertRaises(TypeError):
            self._tc(name=17)

    def test_attr_trace_class(self):
        trace = self._tc(name='my name')
        self.assertEqual(trace.cls.addr, self._tc.addr)

    def test_len(self):
        trace = self._tc()
        sc = self._tc.create_stream_class()
        self.assertEqual(len(trace), 0)

        trace.create_stream(sc)
        self.assertEqual(len(trace), 1)

    def _create_trace_with_some_streams(self):
        sc = self._tc.create_stream_class(assigns_automatic_stream_id=False)
        trace = self._tc()
        trace.create_stream(sc, id=12)
        trace.create_stream(sc, id=15)
        trace.create_stream(sc, id=17)

        return trace

    def test_iter(self):
        trace = self._create_trace_with_some_streams()
        stream_ids = set(trace)
        self.assertEqual(stream_ids, {12, 15, 17})

    def test_getitem(self):
        trace = self._create_trace_with_some_streams()

        self.assertEqual(trace[12].id, 12)

    def test_getitem_invalid_key(self):
        trace = self._create_trace_with_some_streams()
        with self.assertRaises(KeyError):
            trace[18]

    def test_destruction_listener(self):
        def on_trace_class_destruction(trace_class):
            nonlocal trace_class_destroyed
            trace_class_destroyed = True

        def on_trace_destruction(trace):
            nonlocal trace_destroyed
            trace_destroyed = True

        trace_destroyed = False
        trace_class_destroyed = False

        trace_class = get_default_trace_class()
        stream_class = trace_class.create_stream_class()
        trace = trace_class()
        stream = trace.create_stream(stream_class)

        trace_class.add_destruction_listener(on_trace_class_destruction)
        trace.add_destruction_listener(on_trace_destruction)

        self.assertFalse(trace_class_destroyed)
        self.assertFalse(trace_destroyed)

        del trace

        self.assertFalse(trace_class_destroyed)
        self.assertFalse(trace_destroyed)

        del stream

        self.assertFalse(trace_class_destroyed)
        self.assertTrue(trace_destroyed)

        del trace_class

        self.assertFalse(trace_class_destroyed)
        self.assertTrue(trace_destroyed)

        del stream_class

        self.assertTrue(trace_class_destroyed)
        self.assertTrue(trace_destroyed)
