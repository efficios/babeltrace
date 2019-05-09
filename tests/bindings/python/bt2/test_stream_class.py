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
        self.assertFalse(sc.packets_have_default_beginning_clock_snapshot)
        self.assertFalse(sc.packets_have_default_end_clock_snapshot)

    def test_create_name(self):
        sc = self._tc.create_stream_class(name='bozo')
        self.assertEqual(sc.name, 'bozo')

    def test_create_invalid_name(self):
        with self.assertRaises(TypeError):
            self._tc.create_stream_class(name=17)

    def test_create_packet_context_field_class(self):
        fc = self._tc.create_structure_field_class()
        sc = self._tc.create_stream_class(packet_context_field_class=fc)
        self.assertEqual(sc.packet_context_field_class, fc)

    def test_create_invalid_packet_context_field_class(self):
        with self.assertRaises(TypeError):
            self._tc.create_stream_class(packet_context_field_class=22)

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

    def test_packets_have_default_beginning_clock_snapshot(self):
        sc = self._tc.create_stream_class(default_clock_class=self._cc, packets_have_default_beginning_clock_snapshot=True)
        self.assertTrue(sc.packets_have_default_beginning_clock_snapshot)

    def test_packets_have_default_beginning_clock_snapshot_raises(self):
        with self.assertRaises(TypeError):
            sc = self._tc.create_stream_class(packets_have_default_beginning_clock_snapshot="something")

    def test_packets_have_default_end_clock_snapshot(self):
        sc = self._tc.create_stream_class(default_clock_class=self._cc, packets_have_default_end_clock_snapshot=True)
        self.assertTrue(sc.packets_have_default_end_clock_snapshot)

    def test_packets_have_default_end_clock_snapshot_raises(self):
        with self.assertRaises(TypeError):
            sc = self._tc.create_stream_class(packets_have_default_end_clock_snapshot="something")

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