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
import utils
from utils import run_in_component_init
from bt2 import trace as bt2_trace
from bt2 import stream as bt2_stream
from bt2 import value as bt2_value
from bt2 import stream_class as bt2_stream_class


class StreamTestCase(unittest.TestCase):
    def setUp(self):
        def f(comp_self):
            return comp_self._create_trace_class()

        self._tc = run_in_component_init(f)
        self._sc = self._tc.create_stream_class(assigns_automatic_stream_id=True)
        self._tr = self._tc()

    def test_create_default(self):
        stream = self._tr.create_stream(self._sc)
        self.assertIsNone(stream.name)
        self.assertIs(type(stream), bt2_stream._Stream)
        self.assertEqual(len(stream.user_attributes), 0)

    def test_name(self):
        stream = self._tr.create_stream(self._sc, name='équidistant')
        self.assertEqual(stream.name, 'équidistant')

    def test_invalid_name(self):
        with self.assertRaises(TypeError):
            self._tr.create_stream(self._sc, name=22)

    def test_create_user_attributes(self):
        stream = self._tr.create_stream(self._sc, user_attributes={'salut': 23})
        self.assertEqual(stream.user_attributes, {'salut': 23})
        self.assertIs(type(stream.user_attributes), bt2_value.MapValue)

    def test_const_user_attributes(self):
        stream = utils.get_const_stream_beginning_message().stream
        self.assertEqual(stream.user_attributes, {'salut': 23})
        self.assertIs(type(stream.user_attributes), bt2_value._MapValueConst)

    def test_create_invalid_user_attributes(self):
        with self.assertRaises(TypeError):
            self._tr.create_stream(self._sc, user_attributes=object())

    def test_create_invalid_user_attributes_value_type(self):
        with self.assertRaises(TypeError):
            self._tr.create_stream(self._sc, user_attributes=23)

    def test_stream_class(self):
        stream = self._tr.create_stream(self._sc)
        self.assertEqual(stream.cls, self._sc)
        self.assertIs(type(stream.cls), bt2_stream_class._StreamClass)

    def test_const_stream_class(self):
        stream = utils.get_const_stream_beginning_message().stream
        self.assertIs(type(stream.cls), bt2_stream_class._StreamClassConst)

    def test_trace(self):
        stream = self._tr.create_stream(self._sc)
        self.assertEqual(stream.trace.addr, self._tr.addr)
        self.assertIs(type(stream.trace), bt2_trace._Trace)

    def test_const_trace(self):
        stream = utils.get_const_stream_beginning_message().stream
        self.assertIs(type(stream.trace), bt2_trace._TraceConst)

    def test_invalid_id(self):
        sc = self._tc.create_stream_class(assigns_automatic_stream_id=False)

        with self.assertRaises(TypeError):
            self._tr.create_stream(sc, id='string')


if __name__ == '__main__':
    unittest.main()
