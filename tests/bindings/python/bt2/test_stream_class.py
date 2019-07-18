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
from utils import run_in_component_init


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

    def test_create_name(self):
        sc = self._tc.create_stream_class(name='bozo')
        self.assertEqual(sc.name, 'bozo')

    def test_create_invalid_name(self):
        with self.assertRaises(TypeError):
            self._tc.create_stream_class(name=17)

    def test_create_packet_context_field_class(self):
        fc = self._tc.create_structure_field_class()
        sc = self._tc.create_stream_class(
            packet_context_field_class=fc, supports_packets=True
        )
        self.assertEqual(sc.packet_context_field_class, fc)

    def test_create_invalid_packet_context_field_class(self):
        with self.assertRaises(TypeError):
            self._tc.create_stream_class(packet_context_field_class=22)

    def test_create_invalid_packet_context_field_class_no_packets(self):
        fc = self._tc.create_structure_field_class()

        with self.assertRaises(ValueError):
            self._tc.create_stream_class(packet_context_field_class=fc)

    def test_create_event_common_context_field_class(self):
        fc = self._tc.create_structure_field_class()
        sc = self._tc.create_stream_class(event_common_context_field_class=fc)
        self.assertEqual(sc.event_common_context_field_class, fc)

    def test_create_invalid_event_common_context_field_class(self):
        with self.assertRaises(TypeError):
            self._tc.create_stream_class(event_common_context_field_class=22)

    def test_create_default_clock_class(self):
        sc = self._tc.create_stream_class(default_clock_class=self._cc)
        self.assertEqual(sc.default_clock_class.addr, self._cc.addr)

    def test_create_invalid_default_clock_class(self):
        with self.assertRaises(TypeError):
            self._tc.create_stream_class(default_clock_class=12)

    def test_automatic_stream_ids(self):
        sc = self._tc.create_stream_class(assigns_automatic_stream_id=True)
        self.assertTrue(sc.assigns_automatic_stream_id)

        stream = self._trace.create_stream(sc)
        self.assertIsNotNone(stream.id)

    def test_automatic_stream_ids_raises(self):
        sc = self._tc.create_stream_class(assigns_automatic_stream_id=True)
        self.assertTrue(sc.assigns_automatic_stream_id)

        with self.assertRaises(ValueError):
            self._trace.create_stream(sc, id=123)

    def test_no_automatic_stream_ids(self):
        sc = self._tc.create_stream_class(assigns_automatic_stream_id=False)
        self.assertFalse(sc.assigns_automatic_stream_id)

        stream = self._trace.create_stream(sc, id=333)
        self.assertEqual(stream.id, 333)

    def test_no_automatic_stream_ids_raises(self):
        sc = self._tc.create_stream_class(assigns_automatic_stream_id=False)
        self.assertFalse(sc.assigns_automatic_stream_id)

        with self.assertRaises(ValueError):
            self._trace.create_stream(sc)

    def test_automatic_event_class_ids(self):
        sc = self._tc.create_stream_class(assigns_automatic_event_class_id=True)
        self.assertTrue(sc.assigns_automatic_event_class_id)

        ec = sc.create_event_class()
        self.assertIsNotNone(ec.id)

    def test_automatic_event_class_ids_raises(self):
        sc = self._tc.create_stream_class(assigns_automatic_event_class_id=True)
        self.assertTrue(sc.assigns_automatic_event_class_id)

        with self.assertRaises(ValueError):
            sc.create_event_class(id=123)

    def test_no_automatic_event_class_ids(self):
        sc = self._tc.create_stream_class(assigns_automatic_event_class_id=False)
        self.assertFalse(sc.assigns_automatic_event_class_id)

        ec = sc.create_event_class(id=333)
        self.assertEqual(ec.id, 333)

    def test_no_automatic_event_class_ids_raises(self):
        sc = self._tc.create_stream_class(assigns_automatic_event_class_id=False)
        self.assertFalse(sc.assigns_automatic_event_class_id)

        with self.assertRaises(ValueError):
            sc.create_event_class()

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
        with self.assertRaises(TypeError):
            sc = self._tc.create_stream_class(
                default_clock_class=self._cc, supports_packets=23
            )

    def test_packets_have_begin_default_cs_raises_type_error(self):
        with self.assertRaises(TypeError):
            sc = self._tc.create_stream_class(
                default_clock_class=self._cc,
                packets_have_beginning_default_clock_snapshot=23,
            )

    def test_packets_have_end_default_cs_raises_type_error(self):
        with self.assertRaises(TypeError):
            sc = self._tc.create_stream_class(
                default_clock_class=self._cc, packets_have_end_default_clock_snapshot=23
            )

    def test_does_not_support_packets_raises_with_begin_cs(self):
        with self.assertRaises(ValueError):
            sc = self._tc.create_stream_class(
                default_clock_class=self._cc,
                packets_have_beginning_default_clock_snapshot=True,
            )

    def test_does_not_support_packets_raises_with_end_cs(self):
        with self.assertRaises(ValueError):
            sc = self._tc.create_stream_class(
                default_clock_class=self._cc,
                packets_have_end_default_clock_snapshot=True,
            )

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
        with self.assertRaises(TypeError):
            sc = self._tc.create_stream_class(
                default_clock_class=self._cc, supports_discarded_events=23
            )

    def test_discarded_events_have_default_cs_raises_type_error(self):
        with self.assertRaises(TypeError):
            sc = self._tc.create_stream_class(
                default_clock_class=self._cc,
                discarded_events_have_default_clock_snapshots=23,
            )

    def test_does_not_support_discarded_events_raises_with_cs(self):
        with self.assertRaises(ValueError):
            sc = self._tc.create_stream_class(
                default_clock_class=self._cc,
                discarded_events_have_default_clock_snapshots=True,
            )

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
        with self.assertRaises(ValueError):
            sc = self._tc.create_stream_class(
                default_clock_class=self._cc, supports_discarded_packets=True
            )

    def test_supports_discarded_packets_raises_type_error(self):
        with self.assertRaises(TypeError):
            sc = self._tc.create_stream_class(
                default_clock_class=self._cc,
                supports_discarded_packets=23,
                supports_packets=True,
            )

    def test_discarded_packets_have_default_cs_raises_type_error(self):
        with self.assertRaises(TypeError):
            sc = self._tc.create_stream_class(
                default_clock_class=self._cc,
                discarded_packets_have_default_clock_snapshots=23,
                supports_packets=True,
            )

    def test_does_not_support_discarded_packets_raises_with_cs(self):
        with self.assertRaises(ValueError):
            sc = self._tc.create_stream_class(
                default_clock_class=self._cc,
                discarded_packets_have_default_clock_snapshots=True,
                supports_packets=True,
            )

    def test_trace_class(self):
        sc = self._tc.create_stream_class()
        self.assertEqual(sc.trace_class.addr, self._tc.addr)

    def _create_stream_class_with_event_classes(self):
        sc = self._tc.create_stream_class(assigns_automatic_event_class_id=False)
        ec1 = sc.create_event_class(id=23)
        ec2 = sc.create_event_class(id=17)
        return sc, ec1, ec2

    def test_getitem(self):
        sc, ec1, ec2 = self._create_stream_class_with_event_classes()

        self.assertEqual(sc[23].addr, ec1.addr)
        self.assertEqual(sc[17].addr, ec2.addr)

    def test_getitem_wrong_key_type(self):
        sc, _, _ = self._create_stream_class_with_event_classes()

        with self.assertRaises(TypeError):
            sc['event23']

    def test_getitem_wrong_key(self):
        sc, _, _ = self._create_stream_class_with_event_classes()

        with self.assertRaises(KeyError):
            sc[19]

    def test_len(self):
        sc, _, _ = self._create_stream_class_with_event_classes()

        self.assertEqual(len(sc), 2)

    def test_iter(self):
        sc, _, _ = self._create_stream_class_with_event_classes()

        ec_ids = sorted(sc)
        self.assertEqual(ec_ids, [17, 23])
