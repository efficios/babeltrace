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
from utils import run_in_component_init
from bt2 import stream_class as bt2_stream_class
from bt2 import trace_class as bt2_trace_class
from bt2 import clock_class as bt2_clock_class
from bt2 import event_class as bt2_event_class
from bt2 import field_class as bt2_field_class


class StreamClassTestCase(unittest.TestCase):
    def setUp(self):
        def f(comp_self):
            tc = comp_self._create_trace_class(assigns_automatic_stream_class_id=True)
            cc = comp_self._create_clock_class()
            return tc, cc

        self._tc, self._cc = run_in_component_init(f)
        self._trace = self._tc()

    def test_create_default(self):
        sc = self._tc.create_stream_class()

        self.assertIs(type(sc), bt2_stream_class._StreamClass)
        self.assertIsNone(sc.name)
        self.assertIsNone(sc.packet_context_field_class)
        self.assertIsNone(sc.event_common_context_field_class)
        self.assertIsNone(sc.default_clock_class)
        self.assertTrue(sc.assigns_automatic_event_class_id)
        self.assertTrue(sc.assigns_automatic_stream_id)
        self.assertFalse(sc.supports_packets)
        self.assertFalse(sc.packets_have_beginning_default_clock_snapshot)
        self.assertFalse(sc.packets_have_end_default_clock_snapshot)
        self.assertFalse(sc.supports_discarded_events)
        self.assertFalse(sc.discarded_events_have_default_clock_snapshots)
        self.assertFalse(sc.supports_discarded_packets)
        self.assertFalse(sc.discarded_packets_have_default_clock_snapshots)
        self.assertEqual(len(sc.user_attributes), 0)

    def test_create_name(self):
        sc = self._tc.create_stream_class(name='bozo')
        self.assertEqual(sc.name, 'bozo')

    def test_create_invalid_name(self):
        with self.assertRaisesRegex(TypeError, "'int' is not a 'str' object"):
            self._tc.create_stream_class(name=17)

        self.assertEqual(len(self._tc), 0)

    def test_create_packet_context_field_class(self):
        fc = self._tc.create_structure_field_class()
        sc = self._tc.create_stream_class(
            packet_context_field_class=fc, supports_packets=True
        )
        self.assertEqual(sc.packet_context_field_class, fc)
        self.assertIs(
            type(sc.packet_context_field_class), bt2_field_class._StructureFieldClass
        )

    def test_create_invalid_packet_context_field_class(self):
        with self.assertRaisesRegex(
            TypeError,
            "'int' is not a '<class 'bt2.field_class._StructureFieldClass'>' object",
        ):
            self._tc.create_stream_class(
                packet_context_field_class=22, supports_packets=True
            )

        self.assertEqual(len(self._tc), 0)

    def test_create_invalid_packet_context_field_class_no_packets(self):
        fc = self._tc.create_structure_field_class()

        with self.assertRaisesRegex(
            ValueError,
            "cannot have a packet context field class without supporting packets",
        ):
            self._tc.create_stream_class(packet_context_field_class=fc)

        self.assertEqual(len(self._tc), 0)

    def test_create_event_common_context_field_class(self):
        fc = self._tc.create_structure_field_class()
        sc = self._tc.create_stream_class(event_common_context_field_class=fc)
        self.assertEqual(sc.event_common_context_field_class, fc)
        self.assertIs(
            type(sc.event_common_context_field_class),
            bt2_field_class._StructureFieldClass,
        )

    def test_create_invalid_event_common_context_field_class(self):
        with self.assertRaisesRegex(
            TypeError,
            "'int' is not a '<class 'bt2.field_class._StructureFieldClass'>' object",
        ):
            self._tc.create_stream_class(event_common_context_field_class=22)

        self.assertEqual(len(self._tc), 0)

    def test_create_default_clock_class(self):
        sc = self._tc.create_stream_class(default_clock_class=self._cc)
        self.assertEqual(sc.default_clock_class.addr, self._cc.addr)
        self.assertIs(type(sc.default_clock_class), bt2_clock_class._ClockClass)

    def test_create_invalid_default_clock_class(self):
        with self.assertRaisesRegex(
            TypeError, "'int' is not a '<class 'bt2.clock_class._ClockClass'>' object"
        ):
            self._tc.create_stream_class(default_clock_class=12)

        self.assertEqual(len(self._tc), 0)

    def test_create_user_attributes(self):
        sc = self._tc.create_stream_class(user_attributes={'salut': 23})
        self.assertEqual(sc.user_attributes, {'salut': 23})

    def test_create_invalid_user_attributes(self):
        with self.assertRaisesRegex(
            TypeError, "cannot create value object from 'object' object"
        ):
            self._tc.create_stream_class(user_attributes=object())

        self.assertEqual(len(self._tc), 0)

    def test_create_invalid_user_attributes_value_type(self):
        with self.assertRaisesRegex(
            TypeError,
            "'SignedIntegerValue' is not a '<class 'bt2.value.MapValue'>' object",
        ):
            self._tc.create_stream_class(user_attributes=23)

        self.assertEqual(len(self._tc), 0)

    def test_automatic_stream_ids(self):
        sc = self._tc.create_stream_class(assigns_automatic_stream_id=True)
        self.assertTrue(sc.assigns_automatic_stream_id)

        stream = self._trace.create_stream(sc)
        self.assertIsNotNone(stream.id)

    def test_automatic_stream_ids_raises(self):
        sc = self._tc.create_stream_class(assigns_automatic_stream_id=True)
        self.assertTrue(sc.assigns_automatic_stream_id)

        with self.assertRaisesRegex(
            ValueError, "id provided, but stream class assigns automatic stream ids"
        ):
            self._trace.create_stream(sc, id=123)

        self.assertEqual(len(self._trace), 0)

    def test_automatic_stream_ids_wrong_type(self):
        with self.assertRaisesRegex(TypeError, "str' is not a 'bool' object"):
            self._tc.create_stream_class(assigns_automatic_stream_id='True')

        self.assertEqual(len(self._tc), 0)

    def test_no_automatic_stream_ids(self):
        sc = self._tc.create_stream_class(assigns_automatic_stream_id=False)
        self.assertFalse(sc.assigns_automatic_stream_id)

        stream = self._trace.create_stream(sc, id=333)
        self.assertEqual(stream.id, 333)

    def test_no_automatic_stream_ids_raises(self):
        sc = self._tc.create_stream_class(assigns_automatic_stream_id=False)
        self.assertFalse(sc.assigns_automatic_stream_id)

        with self.assertRaisesRegex(
            ValueError,
            "id not provided, but stream class does not assign automatic stream ids",
        ):
            self._trace.create_stream(sc)

        self.assertEqual(len(self._trace), 0)

    def test_automatic_event_class_ids(self):
        sc = self._tc.create_stream_class(assigns_automatic_event_class_id=True)
        self.assertTrue(sc.assigns_automatic_event_class_id)

        ec = sc.create_event_class()
        self.assertIsNotNone(ec.id)

    def test_automatic_event_class_ids_raises(self):
        sc = self._tc.create_stream_class(assigns_automatic_event_class_id=True)
        self.assertTrue(sc.assigns_automatic_event_class_id)

        with self.assertRaisesRegex(
            ValueError,
            "id provided, but stream class assigns automatic event class ids",
        ):
            sc.create_event_class(id=123)

        self.assertEqual(len(sc), 0)

    def test_automatic_event_class_ids_wrong_type(self):
        with self.assertRaisesRegex(TypeError, "'str' is not a 'bool' object"):
            self._tc.create_stream_class(assigns_automatic_event_class_id='True')

        self.assertEqual(len(self._tc), 0)

    def test_no_automatic_event_class_ids(self):
        sc = self._tc.create_stream_class(assigns_automatic_event_class_id=False)
        self.assertFalse(sc.assigns_automatic_event_class_id)

        ec = sc.create_event_class(id=333)
        self.assertEqual(ec.id, 333)

    def test_no_automatic_event_class_ids_raises(self):
        sc = self._tc.create_stream_class(assigns_automatic_event_class_id=False)
        self.assertFalse(sc.assigns_automatic_event_class_id)

        with self.assertRaisesRegex(
            ValueError,
            "id not provided, but stream class does not assign automatic event class ids",
        ):
            sc.create_event_class()

        self.assertEqual(len(sc), 0)

    def test_supports_packets_without_cs(self):
        sc = self._tc.create_stream_class(
            default_clock_class=self._cc, supports_packets=True
        )
        self.assertTrue(sc.supports_packets)
        self.assertFalse(sc.packets_have_beginning_default_clock_snapshot)
        self.assertFalse(sc.packets_have_end_default_clock_snapshot)

    def test_supports_packets_with_begin_cs(self):
        sc = self._tc.create_stream_class(
            default_clock_class=self._cc,
            supports_packets=True,
            packets_have_beginning_default_clock_snapshot=True,
        )
        self.assertTrue(sc.supports_packets)
        self.assertTrue(sc.packets_have_beginning_default_clock_snapshot)
        self.assertFalse(sc.packets_have_end_default_clock_snapshot)

    def test_supports_packets_with_end_cs(self):
        sc = self._tc.create_stream_class(
            default_clock_class=self._cc,
            supports_packets=True,
            packets_have_end_default_clock_snapshot=True,
        )
        self.assertTrue(sc.supports_packets)
        self.assertFalse(sc.packets_have_beginning_default_clock_snapshot)
        self.assertTrue(sc.packets_have_end_default_clock_snapshot)

    def test_supports_packets_raises_type_error(self):
        with self.assertRaisesRegex(TypeError, "'int' is not a 'bool' object"):
            self._tc.create_stream_class(
                default_clock_class=self._cc, supports_packets=23
            )

        self.assertEqual(len(self._tc), 0)

    def test_packets_have_begin_default_cs_raises_type_error(self):
        with self.assertRaisesRegex(TypeError, "'int' is not a 'bool' object"):
            self._tc.create_stream_class(
                default_clock_class=self._cc,
                packets_have_beginning_default_clock_snapshot=23,
            )

        self.assertEqual(len(self._tc), 0)

    def test_packets_have_end_default_cs_raises_type_error(self):
        with self.assertRaisesRegex(TypeError, "'int' is not a 'bool' object"):
            self._tc.create_stream_class(
                default_clock_class=self._cc, packets_have_end_default_clock_snapshot=23
            )

        self.assertEqual(len(self._tc), 0)

    def test_does_not_support_packets_raises_with_begin_cs(self):
        with self.assertRaisesRegex(
            ValueError,
            "cannot not support packets, but have packet beginning default clock snapshot",
        ):
            self._tc.create_stream_class(
                default_clock_class=self._cc,
                packets_have_beginning_default_clock_snapshot=True,
            )

        self.assertEqual(len(self._tc), 0)

    def test_does_not_support_packets_raises_with_end_cs(self):
        with self.assertRaisesRegex(
            ValueError,
            "cannot not support packets, but have packet end default clock snapshots",
        ):
            self._tc.create_stream_class(
                default_clock_class=self._cc,
                packets_have_end_default_clock_snapshot=True,
            )

        self.assertEqual(len(self._tc), 0)

    def test_supports_discarded_events_without_cs(self):
        sc = self._tc.create_stream_class(
            default_clock_class=self._cc, supports_discarded_events=True
        )
        self.assertTrue(sc.supports_discarded_events)
        self.assertFalse(sc.discarded_events_have_default_clock_snapshots)

    def test_supports_discarded_events_with_cs(self):
        sc = self._tc.create_stream_class(
            default_clock_class=self._cc,
            supports_discarded_events=True,
            discarded_events_have_default_clock_snapshots=True,
        )
        self.assertTrue(sc.supports_discarded_events)
        self.assertTrue(sc.discarded_events_have_default_clock_snapshots)

    def test_supports_discarded_events_raises_type_error(self):
        with self.assertRaisesRegex(TypeError, "'int' is not a 'bool' object"):
            self._tc.create_stream_class(
                default_clock_class=self._cc, supports_discarded_events=23
            )

        self.assertEqual(len(self._tc), 0)

    def test_discarded_events_have_default_cs_raises_type_error(self):
        with self.assertRaisesRegex(TypeError, "'int' is not a 'bool' object"):
            self._tc.create_stream_class(
                default_clock_class=self._cc,
                discarded_events_have_default_clock_snapshots=23,
            )

        self.assertEqual(len(self._tc), 0)

    def test_does_not_support_discarded_events_raises_with_cs(self):
        with self.assertRaisesRegex(
            ValueError,
            "cannot not support discarded events, but have default clock snapshots for discarded event messages",
        ):
            self._tc.create_stream_class(
                default_clock_class=self._cc,
                discarded_events_have_default_clock_snapshots=True,
            )

        self.assertEqual(len(self._tc), 0)

    def test_supports_discarded_events_with_clock_snapshots_without_default_clock_class_raises(
        self,
    ):
        with self.assertRaisesRegex(
            ValueError,
            'cannot have no default clock class, but have default clock snapshots for discarded event messages',
        ):
            self._tc.create_stream_class(
                supports_discarded_events=True,
                discarded_events_have_default_clock_snapshots=True,
            )

        self.assertEqual(len(self._tc), 0)

    def test_supports_discarded_packets_without_cs(self):
        sc = self._tc.create_stream_class(
            default_clock_class=self._cc,
            supports_discarded_packets=True,
            supports_packets=True,
        )
        self.assertTrue(sc.supports_discarded_packets)
        self.assertFalse(sc.discarded_packets_have_default_clock_snapshots)

    def test_supports_discarded_packets_with_cs(self):
        sc = self._tc.create_stream_class(
            default_clock_class=self._cc,
            supports_discarded_packets=True,
            discarded_packets_have_default_clock_snapshots=True,
            supports_packets=True,
        )
        self.assertTrue(sc.supports_discarded_packets)
        self.assertTrue(sc.discarded_packets_have_default_clock_snapshots)

    def test_supports_discarded_packets_raises_without_packet_support(self):
        with self.assertRaisesRegex(
            ValueError, "cannot support discarded packets, but not support packets"
        ):
            self._tc.create_stream_class(
                default_clock_class=self._cc, supports_discarded_packets=True
            )

        self.assertEqual(len(self._tc), 0)

    def test_supports_discarded_packets_raises_type_error(self):
        with self.assertRaisesRegex(TypeError, "'int' is not a 'bool' object"):
            self._tc.create_stream_class(
                default_clock_class=self._cc,
                supports_discarded_packets=23,
                supports_packets=True,
            )

        self.assertEqual(len(self._tc), 0)

    def test_discarded_packets_have_default_cs_raises_type_error(self):
        with self.assertRaisesRegex(TypeError, "'int' is not a 'bool' object"):
            self._tc.create_stream_class(
                default_clock_class=self._cc,
                discarded_packets_have_default_clock_snapshots=23,
                supports_packets=True,
            )

        self.assertEqual(len(self._tc), 0)

    def test_does_not_support_discarded_packets_raises_with_cs(self):
        with self.assertRaisesRegex(
            ValueError,
            "cannot not support discarded packets, but have default clock snapshots for discarded packet messages",
        ):
            self._tc.create_stream_class(
                default_clock_class=self._cc,
                discarded_packets_have_default_clock_snapshots=True,
                supports_packets=True,
            )

        self.assertEqual(len(self._tc), 0)

    def test_supports_discarded_packets_with_clock_snapshots_without_default_clock_class_raises(
        self,
    ):
        with self.assertRaisesRegex(
            ValueError,
            'cannot have no default clock class, but have default clock snapshots for discarded packet messages',
        ):
            self._tc.create_stream_class(
                supports_packets=True,
                supports_discarded_packets=True,
                discarded_packets_have_default_clock_snapshots=True,
            )

        self.assertEqual(len(self._tc), 0)

    def test_trace_class(self):
        sc = self._tc.create_stream_class()
        self.assertEqual(sc.trace_class.addr, self._tc.addr)
        self.assertIs(type(sc.trace_class), bt2_trace_class._TraceClass)

    def _create_stream_class_with_event_classes(self):
        sc = self._tc.create_stream_class(assigns_automatic_event_class_id=False)
        ec1 = sc.create_event_class(id=23)
        ec2 = sc.create_event_class(id=17)
        return sc, ec1, ec2

    def test_getitem(self):
        sc, ec1, ec2 = self._create_stream_class_with_event_classes()

        self.assertEqual(sc[23].addr, ec1.addr)
        self.assertEqual(type(sc[23]), bt2_event_class._EventClass)
        self.assertEqual(sc[17].addr, ec2.addr)
        self.assertEqual(type(sc[17]), bt2_event_class._EventClass)

    def test_getitem_wrong_key_type(self):
        sc, _, _ = self._create_stream_class_with_event_classes()

        with self.assertRaisesRegex(TypeError, "'str' is not an 'int' object"):
            sc['event23']

    def test_getitem_wrong_key(self):
        sc, _, _ = self._create_stream_class_with_event_classes()

        with self.assertRaisesRegex(KeyError, '19'):
            sc[19]

    def test_len(self):
        sc, _, _ = self._create_stream_class_with_event_classes()

        self.assertEqual(len(sc), 2)

    def test_iter(self):
        sc, _, _ = self._create_stream_class_with_event_classes()

        ec_ids = sorted(sc)
        self.assertEqual(ec_ids, [17, 23])


if __name__ == '__main__':
    unittest.main()
