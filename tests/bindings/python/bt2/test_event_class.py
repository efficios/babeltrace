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
import bt2
from utils import get_default_trace_class
from bt2 import stream_class as bt2_stream_class
from bt2 import event_class as bt2_event_class
from bt2 import field_class as bt2_field_class
from bt2 import value as bt2_value
from utils import TestOutputPortMessageIterator


def _create_const_event_class(tc, stream_class):
    fc1 = tc.create_structure_field_class()
    fc2 = tc.create_structure_field_class()
    event_class = stream_class.create_event_class(
        payload_field_class=fc1, specific_context_field_class=fc2
    )

    class MyIter(bt2._UserMessageIterator):
        def __init__(self, config, self_port_output):

            trace = tc()
            stream = trace.create_stream(stream_class)
            self._msgs = [
                self._create_stream_beginning_message(stream),
                self._create_event_message(event_class, stream),
            ]

        def __next__(self):
            if len(self._msgs) == 0:
                raise StopIteration

            return self._msgs.pop(0)

    class MySrc(bt2._UserSourceComponent, message_iterator_class=MyIter):
        def __init__(self, config, params, obj):
            self._add_output_port('out', params)

    graph = bt2.Graph()
    src_comp = graph.add_component(MySrc, 'my_source', None)
    msg_iter = TestOutputPortMessageIterator(graph, src_comp.output_ports['out'])

    # Ignore first message, stream beginning
    _ = next(msg_iter)

    event_msg = next(msg_iter)

    return event_msg.event.cls


class EventClassTestCase(unittest.TestCase):
    def setUp(self):
        self._tc = get_default_trace_class()

        self._context_fc = self._tc.create_structure_field_class()
        self._context_fc.append_member('allo', self._tc.create_string_field_class())
        self._context_fc.append_member(
            'zola', self._tc.create_signed_integer_field_class(18)
        )

        self._payload_fc = self._tc.create_structure_field_class()
        self._payload_fc.append_member('zoom', self._tc.create_string_field_class())

        self._stream_class = self._tc.create_stream_class(
            assigns_automatic_event_class_id=True
        )

    def test_create_default(self):
        ec = self._stream_class.create_event_class()

        self.assertIs(type(ec), bt2_event_class._EventClass)
        self.assertIsNone(ec.name, 'my_event')
        self.assertTrue(type(ec.id), int)
        self.assertIsNone(ec.specific_context_field_class)
        self.assertIsNone(ec.payload_field_class)
        self.assertIsNone(ec.emf_uri)
        self.assertIsNone(ec.log_level)
        self.assertEqual(len(ec.user_attributes), 0)

    def test_create_invalid_id(self):
        sc = self._tc.create_stream_class(assigns_automatic_event_class_id=False)
        with self.assertRaises(TypeError):
            sc.create_event_class(id='lel')

        self.assertEqual(len(sc), 0)

    def test_create_specific_context_field_class(self):
        fc = self._tc.create_structure_field_class()
        ec = self._stream_class.create_event_class(specific_context_field_class=fc)
        self.assertEqual(ec.specific_context_field_class.addr, fc.addr)
        self.assertIs(
            type(ec.specific_context_field_class), bt2_field_class._StructureFieldClass
        )

    def test_const_create_specific_context_field_class(self):
        ec_const = _create_const_event_class(self._tc, self._stream_class)
        self.assertIs(
            type(ec_const.specific_context_field_class),
            bt2_field_class._StructureFieldClassConst,
        )

    def test_create_invalid_specific_context_field_class(self):
        with self.assertRaises(TypeError):
            self._stream_class.create_event_class(specific_context_field_class='lel')

        self.assertEqual(len(self._stream_class), 0)

    def test_create_payload_field_class(self):
        fc = self._tc.create_structure_field_class()
        ec = self._stream_class.create_event_class(payload_field_class=fc)
        self.assertEqual(ec.payload_field_class.addr, fc.addr)
        self.assertIs(
            type(ec.payload_field_class), bt2_field_class._StructureFieldClass
        )

    def test_const_create_payload_field_class(self):
        ec_const = _create_const_event_class(self._tc, self._stream_class)
        self.assertIs(
            type(ec_const.payload_field_class),
            bt2_field_class._StructureFieldClassConst,
        )

    def test_create_invalid_payload_field_class(self):
        with self.assertRaises(TypeError):
            self._stream_class.create_event_class(payload_field_class='lel')

        self.assertEqual(len(self._stream_class), 0)

    def test_create_name(self):
        ec = self._stream_class.create_event_class(name='viande à chien')
        self.assertEqual(ec.name, 'viande à chien')

    def test_create_invalid_name(self):
        with self.assertRaises(TypeError):
            self._stream_class.create_event_class(name=2)

        self.assertEqual(len(self._stream_class), 0)

    def test_emf_uri(self):
        ec = self._stream_class.create_event_class(emf_uri='salut')
        self.assertEqual(ec.emf_uri, 'salut')

    def test_create_invalid_emf_uri(self):
        with self.assertRaises(TypeError):
            self._stream_class.create_event_class(emf_uri=23)

        self.assertEqual(len(self._stream_class), 0)

    def test_create_log_level(self):
        ec = self._stream_class.create_event_class(
            log_level=bt2.EventClassLogLevel.EMERGENCY
        )
        self.assertEqual(ec.log_level, bt2.EventClassLogLevel.EMERGENCY)

    def test_create_invalid_log_level(self):
        with self.assertRaises(ValueError):
            self._stream_class.create_event_class(log_level='zoom')

        self.assertEqual(len(self._stream_class), 0)

    def test_create_user_attributes(self):
        ec = self._stream_class.create_event_class(user_attributes={'salut': 23})
        self.assertEqual(ec.user_attributes, {'salut': 23})
        self.assertIs(type(ec.user_attributes), bt2_value.MapValue)

    def test_const_create_user_attributes(self):
        ec_const = _create_const_event_class(self._tc, self._stream_class)
        self.assertIs(type(ec_const.user_attributes), bt2_value._MapValueConst)

    def test_create_invalid_user_attributes(self):
        with self.assertRaises(TypeError):
            self._stream_class.create_event_class(user_attributes=object())

        self.assertEqual(len(self._stream_class), 0)

    def test_create_invalid_user_attributes_value_type(self):
        with self.assertRaises(TypeError):
            self._stream_class.create_event_class(user_attributes=23)

        self.assertEqual(len(self._stream_class), 0)

    def test_stream_class(self):
        ec = self._stream_class.create_event_class()
        self.assertEqual(ec.stream_class.addr, self._stream_class.addr)
        self.assertIs(type(ec.stream_class), bt2_stream_class._StreamClass)

    def test_const_stream_class(self):
        ec_const = _create_const_event_class(self._tc, self._stream_class)
        self.assertIs(type(ec_const.stream_class), bt2_stream_class._StreamClassConst)


if __name__ == '__main__':
    unittest.main()
