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
import uuid
import bt2
import utils
from utils import run_in_component_init, TestOutputPortMessageIterator
from bt2 import value as bt2_value
from bt2 import clock_class as bt2_clock_class


class ClockClassOffsetTestCase(unittest.TestCase):
    def test_create_default(self):
        cco = bt2.ClockClassOffset()
        self.assertEqual(cco.seconds, 0)
        self.assertEqual(cco.cycles, 0)

    def test_create(self):
        cco = bt2.ClockClassOffset(23, 4871232)
        self.assertEqual(cco.seconds, 23)
        self.assertEqual(cco.cycles, 4871232)

    def test_create_kwargs(self):
        cco = bt2.ClockClassOffset(seconds=23, cycles=4871232)
        self.assertEqual(cco.seconds, 23)
        self.assertEqual(cco.cycles, 4871232)

    def test_create_invalid_seconds(self):
        with self.assertRaises(TypeError):
            bt2.ClockClassOffset('hello', 4871232)

    def test_create_invalid_cycles(self):
        with self.assertRaises(TypeError):
            bt2.ClockClassOffset(23, 'hello')

    def test_eq(self):
        cco1 = bt2.ClockClassOffset(23, 42)
        cco2 = bt2.ClockClassOffset(23, 42)
        self.assertEqual(cco1, cco2)

    def test_ne_seconds(self):
        cco1 = bt2.ClockClassOffset(23, 42)
        cco2 = bt2.ClockClassOffset(24, 42)
        self.assertNotEqual(cco1, cco2)

    def test_ne_cycles(self):
        cco1 = bt2.ClockClassOffset(23, 42)
        cco2 = bt2.ClockClassOffset(23, 43)
        self.assertNotEqual(cco1, cco2)

    def test_eq_invalid(self):
        self.assertFalse(bt2.ClockClassOffset() == 23)


class ClockClassTestCase(unittest.TestCase):
    def assertRaisesInComponentInit(self, expected_exc_type, user_code):
        def f(comp_self):
            try:
                user_code(comp_self)
            except Exception as exc:
                return type(exc)

        exc_type = run_in_component_init(f)
        self.assertIsNotNone(exc_type)
        self.assertEqual(exc_type, expected_exc_type)

    def test_create_default(self):
        cc = run_in_component_init(lambda comp_self: comp_self._create_clock_class())

        self.assertIsNone(cc.name)
        self.assertEqual(cc.frequency, 1000000000)
        self.assertIsNone(cc.description)
        self.assertEqual(cc.precision, 0)
        self.assertEqual(cc.offset, bt2.ClockClassOffset())
        self.assertTrue(cc.origin_is_unix_epoch)
        self.assertIsNone(cc.uuid)
        self.assertEqual(len(cc.user_attributes), 0)

    def test_create_name(self):
        def f(comp_self):
            return comp_self._create_clock_class(name='the_clock')

        cc = run_in_component_init(f)
        self.assertEqual(cc.name, 'the_clock')

    def test_create_invalid_name(self):
        def f(comp_self):
            comp_self._create_clock_class(name=23)

        self.assertRaisesInComponentInit(TypeError, f)

    def test_create_description(self):
        def f(comp_self):
            return comp_self._create_clock_class(description='hi people')

        cc = run_in_component_init(f)
        self.assertEqual(cc.description, 'hi people')

    def test_create_invalid_description(self):
        def f(comp_self):
            return comp_self._create_clock_class(description=23)

        self.assertRaisesInComponentInit(TypeError, f)

    def test_create_frequency(self):
        def f(comp_self):
            return comp_self._create_clock_class(frequency=987654321)

        cc = run_in_component_init(f)
        self.assertEqual(cc.frequency, 987654321)

    def test_create_invalid_frequency(self):
        def f(comp_self):
            return comp_self._create_clock_class(frequency='lel')

        self.assertRaisesInComponentInit(TypeError, f)

    def test_create_precision(self):
        def f(comp_self):
            return comp_self._create_clock_class(precision=12)

        cc = run_in_component_init(f)
        self.assertEqual(cc.precision, 12)

    def test_create_invalid_precision(self):
        def f(comp_self):
            return comp_self._create_clock_class(precision='lel')

        self.assertRaisesInComponentInit(TypeError, f)

    def test_create_offset(self):
        def f(comp_self):
            return comp_self._create_clock_class(offset=bt2.ClockClassOffset(12, 56))

        cc = run_in_component_init(f)
        self.assertEqual(cc.offset, bt2.ClockClassOffset(12, 56))

    def test_create_invalid_offset(self):
        def f(comp_self):
            return comp_self._create_clock_class(offset=object())

        self.assertRaisesInComponentInit(TypeError, f)

    def test_create_origin_is_unix_epoch(self):
        def f(comp_self):
            return comp_self._create_clock_class(origin_is_unix_epoch=False)

        cc = run_in_component_init(f)
        self.assertEqual(cc.origin_is_unix_epoch, False)

    def test_create_invalid_origin_is_unix_epoch(self):
        def f(comp_self):
            return comp_self._create_clock_class(origin_is_unix_epoch=23)

        self.assertRaisesInComponentInit(TypeError, f)

    def test_cycles_to_ns_from_origin(self):
        def f(comp_self):
            return comp_self._create_clock_class(
                frequency=10 ** 8, origin_is_unix_epoch=True
            )

        cc = run_in_component_init(f)
        self.assertEqual(cc.cycles_to_ns_from_origin(112), 1120)

    def test_cycles_to_ns_from_origin_overflow(self):
        def f(comp_self):
            return comp_self._create_clock_class(frequency=1000)

        cc = run_in_component_init(f)
        with self.assertRaises(bt2._OverflowError):
            cc.cycles_to_ns_from_origin(2 ** 63)

    def test_create_uuid(self):
        def f(comp_self):
            return comp_self._create_clock_class(
                uuid=uuid.UUID('b43372c32ef0be28444dfc1c5cdafd33')
            )

        cc = run_in_component_init(f)
        self.assertEqual(cc.uuid, uuid.UUID('b43372c32ef0be28444dfc1c5cdafd33'))

    def test_create_invalid_uuid(self):
        def f(comp_self):
            return comp_self._create_clock_class(uuid=23)

        self.assertRaisesInComponentInit(TypeError, f)

    def test_create_user_attributes(self):
        def f(comp_self):
            return comp_self._create_clock_class(user_attributes={'salut': 23})

        cc = run_in_component_init(f)
        self.assertEqual(cc.user_attributes, {'salut': 23})
        self.assertIs(type(cc.user_attributes), bt2_value.MapValue)

    def test_create_invalid_user_attributes(self):
        def f(comp_self):
            return comp_self._create_clock_class(user_attributes=object())

        self.assertRaisesInComponentInit(TypeError, f)

    def test_create_invalid_user_attributes_value_type(self):
        def f(comp_self):
            return comp_self._create_clock_class(user_attributes=23)

        self.assertRaisesInComponentInit(TypeError, f)

    def test_const_user_attributes(self):
        cc = utils.get_const_event_message().default_clock_snapshot.clock_class
        self.assertIs(type(cc.user_attributes), bt2_value._MapValueConst)


class ClockSnapshotTestCase(unittest.TestCase):
    def setUp(self):
        def f(comp_self):
            cc = comp_self._create_clock_class(
                1000, 'my_cc', offset=bt2.ClockClassOffset(45, 354)
            )
            tc = comp_self._create_trace_class()

            return (cc, tc)

        _cc, _tc = run_in_component_init(f)
        _trace = _tc()
        _sc = _tc.create_stream_class(default_clock_class=_cc)
        _ec = _sc.create_event_class(name='salut')
        _stream = _trace.create_stream(_sc)
        self._stream = _stream
        self._ec = _ec
        self._cc = _cc

        class MyIter(bt2._UserMessageIterator):
            def __init__(self, config, self_port_output):
                self._at = 0

            def __next__(self):
                if self._at == 0:
                    notif = self._create_stream_beginning_message(_stream)
                elif self._at == 1:
                    notif = self._create_event_message(_ec, _stream, 123)
                elif self._at == 2:
                    notif = self._create_event_message(_ec, _stream, 2 ** 63)
                elif self._at == 3:
                    notif = self._create_stream_end_message(_stream)
                else:
                    raise bt2.Stop

                self._at += 1
                return notif

        class MySrc(bt2._UserSourceComponent, message_iterator_class=MyIter):
            def __init__(self, config, params, obj):
                self._add_output_port('out')

        self._graph = bt2.Graph()
        self._src_comp = self._graph.add_component(MySrc, 'my_source')
        self._msg_iter = TestOutputPortMessageIterator(
            self._graph, self._src_comp.output_ports['out']
        )

        for i, msg in enumerate(self._msg_iter):
            if i == 1:
                self._msg = msg
            elif i == 2:
                self._msg_clock_overflow = msg
                break

    def tearDown(self):
        del self._cc
        del self._msg

    def test_create_default(self):
        self.assertEqual(
            self._msg.default_clock_snapshot.clock_class.addr, self._cc.addr
        )
        self.assertEqual(self._msg.default_clock_snapshot.value, 123)

    def test_clock_class(self):
        cc = self._msg.default_clock_snapshot.clock_class
        self.assertEqual(cc.addr, self._cc.addr)
        self.assertIs(type(cc), bt2_clock_class._ClockClassConst)

    def test_ns_from_origin(self):
        s_from_origin = 45 + ((354 + 123) / 1000)
        ns_from_origin = int(s_from_origin * 1e9)
        self.assertEqual(
            self._msg.default_clock_snapshot.ns_from_origin, ns_from_origin
        )

    def test_ns_from_origin_overflow(self):
        with self.assertRaises(bt2._OverflowError):
            self._msg_clock_overflow.default_clock_snapshot.ns_from_origin

    def test_eq_int(self):
        self.assertEqual(self._msg.default_clock_snapshot, 123)

    def test_eq_invalid(self):
        self.assertFalse(self._msg.default_clock_snapshot == 23)

    def test_comparison(self):
        self.assertTrue(self._msg.default_clock_snapshot > 100)
        self.assertFalse(self._msg.default_clock_snapshot > 200)

        self.assertTrue(self._msg.default_clock_snapshot >= 123)
        self.assertFalse(self._msg.default_clock_snapshot >= 200)

        self.assertTrue(self._msg.default_clock_snapshot < 200)
        self.assertFalse(self._msg.default_clock_snapshot < 100)

        self.assertTrue(self._msg.default_clock_snapshot <= 123)
        self.assertFalse(self._msg.default_clock_snapshot <= 100)


if __name__ == '__main__':
    unittest.main()
