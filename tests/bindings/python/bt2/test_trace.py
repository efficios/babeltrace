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

import uuid
import unittest
import utils
from utils import get_default_trace_class
from bt2 import trace_class as bt2_trace_class
from bt2 import value as bt2_value
from bt2 import trace as bt2_trace
from bt2 import stream as bt2_stream
from bt2 import utils as bt2_utils


class TraceTestCase(unittest.TestCase):
    def setUp(self):
        self._tc = get_default_trace_class()

    def test_create_default(self):
        trace = self._tc()
        self.assertIsNone(trace.name)
        self.assertIsNone(trace.uuid)
        self.assertEqual(len(trace.environment), 0)
        self.assertEqual(len(trace.user_attributes), 0)

    def test_create_invalid_name(self):
        with self.assertRaises(TypeError):
            self._tc(name=17)

    def test_create_user_attributes(self):
        trace = self._tc(user_attributes={'salut': 23})
        self.assertEqual(trace.user_attributes, {'salut': 23})
        self.assertIs(type(trace.user_attributes), bt2_value.MapValue)

    def test_create_invalid_user_attributes(self):
        with self.assertRaises(TypeError):
            self._tc(user_attributes=object())

    def test_create_invalid_user_attributes_value_type(self):
        with self.assertRaises(TypeError):
            self._tc(user_attributes=23)

    def test_attr_trace_class(self):
        trace = self._tc()
        self.assertEqual(trace.cls.addr, self._tc.addr)
        self.assertIs(type(trace.cls), bt2_trace_class._TraceClass)

    def test_const_attr_trace_class(self):
        trace = utils.get_const_stream_beginning_message().stream.trace
        self.assertIs(type(trace.cls), bt2_trace_class._TraceClassConst)

    def test_attr_name(self):
        trace = self._tc(name='mein trace')
        self.assertEqual(trace.name, 'mein trace')

    def test_attr_uuid(self):
        trace = self._tc(uuid=uuid.UUID('da7d6b6f-3108-4706-89bd-ab554732611b'))
        self.assertEqual(trace.uuid, uuid.UUID('da7d6b6f-3108-4706-89bd-ab554732611b'))

    def test_env_get(self):
        trace = self._tc(environment={'hello': 'you', 'foo': -5})
        self.assertIs(type(trace.environment), bt2_trace._TraceEnvironment)
        self.assertIs(type(trace.environment['foo']), bt2_value.SignedIntegerValue)
        self.assertEqual(trace.environment['hello'], 'you')
        self.assertEqual(trace.environment['foo'], -5)

    def test_env_iter(self):
        trace = self._tc(environment={'hello': 'you', 'foo': -5})
        values = set(trace.environment)
        self.assertEqual(values, {'hello', 'foo'})

    def test_const_env_get(self):
        trace = utils.get_const_stream_beginning_message().stream.trace
        self.assertIs(type(trace.environment), bt2_trace._TraceEnvironmentConst)
        self.assertIs(
            type(trace.environment['patate']), bt2_value._SignedIntegerValueConst
        )

    def test_const_env_iter(self):
        trace = utils.get_const_stream_beginning_message().stream.trace
        values = set(trace.environment)
        self.assertEqual(values, {'patate'})

    def test_const_env_set(self):
        trace = utils.get_const_stream_beginning_message().stream.trace
        with self.assertRaises(TypeError):
            trace.environment['patate'] = 33

    def test_env_get_non_existent(self):
        trace = self._tc(environment={'hello': 'you', 'foo': -5})

        with self.assertRaises(KeyError):
            trace.environment['lel']

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
        self.assertIs(type(trace[12]), bt2_stream._Stream)

    def test_const_getitem(self):
        trace = utils.get_const_stream_beginning_message().stream.trace
        self.assertIs(type(trace[0]), bt2_stream._StreamConst)

    def test_getitem_invalid_key(self):
        trace = self._create_trace_with_some_streams()
        with self.assertRaises(KeyError):
            trace[18]

    def test_destruction_listener(self):
        def on_trace_class_destruction(trace_class):
            nonlocal num_trace_class_destroyed_calls
            num_trace_class_destroyed_calls += 1

        def on_trace_destruction(trace):
            nonlocal num_trace_destroyed_calls
            num_trace_destroyed_calls += 1

        num_trace_class_destroyed_calls = 0
        num_trace_destroyed_calls = 0

        trace_class = get_default_trace_class()
        stream_class = trace_class.create_stream_class()
        trace = trace_class()
        stream = trace.create_stream(stream_class)

        trace_class.add_destruction_listener(on_trace_class_destruction)
        td_handle1 = trace.add_destruction_listener(on_trace_destruction)
        td_handle2 = trace.add_destruction_listener(on_trace_destruction)

        self.assertIs(type(td_handle1), bt2_utils._ListenerHandle)

        trace.remove_destruction_listener(td_handle2)

        self.assertEqual(num_trace_class_destroyed_calls, 0)
        self.assertEqual(num_trace_destroyed_calls, 0)

        del trace

        self.assertEqual(num_trace_class_destroyed_calls, 0)
        self.assertEqual(num_trace_destroyed_calls, 0)

        del stream

        self.assertEqual(num_trace_class_destroyed_calls, 0)
        self.assertEqual(num_trace_destroyed_calls, 1)

        del trace_class

        self.assertEqual(num_trace_class_destroyed_calls, 0)
        self.assertEqual(num_trace_destroyed_calls, 1)

        del stream_class

        self.assertEqual(num_trace_class_destroyed_calls, 1)
        self.assertEqual(num_trace_destroyed_calls, 1)

    def test_remove_destruction_listener_wrong_type(self):
        trace_class = get_default_trace_class()
        trace = trace_class()

        with self.assertRaisesRegex(
            TypeError, r"'int' is not a '<class 'bt2.utils._ListenerHandle'>' object"
        ):
            trace.remove_destruction_listener(123)

    def test_remove_destruction_listener_wrong_object(self):
        def on_trace_destruction(trace):
            pass

        trace_class_1 = get_default_trace_class()
        trace1 = trace_class_1()
        trace_class_2 = get_default_trace_class()
        trace2 = trace_class_2()

        handle1 = trace1.add_destruction_listener(on_trace_destruction)

        with self.assertRaisesRegex(
            ValueError,
            r'This trace destruction listener does not match the trace object\.',
        ):
            trace2.remove_destruction_listener(handle1)

    def test_remove_destruction_listener_twice(self):
        def on_trace_destruction(trace_class):
            pass

        trace_class = get_default_trace_class()
        trace = trace_class()
        handle = trace.add_destruction_listener(on_trace_destruction)

        trace.remove_destruction_listener(handle)

        with self.assertRaisesRegex(
            ValueError, r'This trace destruction listener was already removed\.'
        ):
            trace.remove_destruction_listener(handle)

    def test_raise_in_destruction_listener(self):
        def on_trace_destruction(trace):
            raise ValueError('it hurts')

        trace_class = get_default_trace_class()
        trace = trace_class()
        trace.add_destruction_listener(on_trace_destruction)

        del trace


if __name__ == '__main__':
    unittest.main()
