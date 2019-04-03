import tempfile
import unittest
import bt2
import shutil


@unittest.skip("this is broken")
class AutoPopulatePacketContextTimestampsTestCase(unittest.TestCase):
    def setUp(self):
        self._trace_path = tempfile.mkdtemp()
        self._writer = bt2.CtfWriter(self._trace_path)
        self._cc = bt2.ClockClass('default', int(1e9))
        self._writer.trace.add_clock_class(self._cc)
        self._sc = bt2.StreamClass()
        pcft = bt2.StructureFieldType()
        pcft.append_field('packet_size', bt2.IntegerFieldType(32))
        pcft.append_field('content_size', bt2.IntegerFieldType(32))
        pcft.append_field('timestamp_begin', bt2.IntegerFieldType(64, mapped_clock_class=self._cc))
        pcft.append_field('timestamp_end', bt2.IntegerFieldType(64, mapped_clock_class=self._cc))
        self._sc.packet_context_field_type = pcft

    def tearDown(self):
        shutil.rmtree(self._trace_path)

    def _get_trace_notifs(self):
        del self._cc
        del self._sc
        del self._ec
        del self._stream
        del self._writer
        specs = [bt2.ComponentSpec('ctf', 'fs', self._trace_path)]
        notif_iter = bt2.TraceCollectionNotificationIterator(specs)
        return list(notif_iter)

    def _complete_sc(self):
        self._sc.add_event_class(self._ec)
        self._writer.trace.add_stream_class(self._sc)
        self._stream = self._sc()

    def _complete_sc_event_simple_ts(self):
        ehft = bt2.StructureFieldType()
        ehft.append_field('id', bt2.IntegerFieldType(32))
        ehft.append_field('timestamp', bt2.IntegerFieldType(16, mapped_clock_class=self._cc))
        self._sc.event_header_field_type = ehft
        self._ec = bt2.EventClass('evt')
        payloadft = bt2.StructureFieldType()
        payloadft.append_field('value', bt2.IntegerFieldType(32))
        self._ec.payload_field_type = payloadft
        self._complete_sc()

    def _complete_sc_event_ts_in_payload(self):
        ehft = bt2.StructureFieldType()
        ehft.append_field('id', bt2.IntegerFieldType(32))
        ehft.append_field('timestamp', bt2.IntegerFieldType(16, mapped_clock_class=self._cc))
        self._sc.event_header_field_type = ehft
        self._ec = bt2.EventClass('evt')
        payloadft = bt2.StructureFieldType()
        payloadft.append_field('value', bt2.IntegerFieldType(32))
        payloadft.append_field('a_ts', bt2.IntegerFieldType(8, mapped_clock_class=self._cc))
        self._ec.payload_field_type = payloadft
        self._complete_sc()

    def _append_event_simple_ts(self, ts):
        evt = self._ec()
        evt.header_field['timestamp'] = ts
        evt.payload_field['value'] = 23
        self._stream.append_event(evt)

    def _append_event_ts_in_payload(self, header_ts, payload_ts):
        evt = self._ec()
        evt.header_field['timestamp'] = header_ts
        evt.payload_field['value'] = 23
        evt.payload_field['a_ts'] = payload_ts
        self._stream.append_event(evt)

    def _set_ts_begin_end(self, ts_begin=None, ts_end=None):
        if ts_begin is not None:
            self._stream.packet_context_field['timestamp_begin'] = ts_begin

        if ts_end is not None:
            self._stream.packet_context_field['timestamp_end'] = ts_end

    def _get_packet_ts_begin_end(self, notifs):
        ts_begin_end_list = []

        for notif in notifs:
            if type(notif) is bt2.PacketBeginningNotification:
                ts_begin_end_list.append((
                    int(notif.packet.context_field['timestamp_begin']),
                    int(notif.packet.context_field['timestamp_end']),
                ))

        return ts_begin_end_list

    def test_ts_inc(self):
        self._complete_sc_event_simple_ts()
        self._append_event_simple_ts(12)
        self._append_event_simple_ts(144)
        self._stream.flush()
        ts_begin_end_list = self._get_packet_ts_begin_end(self._get_trace_notifs())
        self.assertEqual(ts_begin_end_list[0][0], 0)
        self.assertEqual(ts_begin_end_list[0][1], 144)

    def test_ts_equal(self):
        self._complete_sc_event_simple_ts()
        self._append_event_simple_ts(12)
        self._append_event_simple_ts(144)
        self._append_event_simple_ts(144)
        self._stream.flush()
        ts_begin_end_list = self._get_packet_ts_begin_end(self._get_trace_notifs())
        self.assertEqual(ts_begin_end_list[0][0], 0)
        self.assertEqual(ts_begin_end_list[0][1], 144)

    def test_ts_wraps(self):
        self._complete_sc_event_simple_ts()
        self._append_event_simple_ts(12)
        self._append_event_simple_ts(144)
        self._append_event_simple_ts(11)
        self._stream.flush()
        ts_begin_end_list = self._get_packet_ts_begin_end(self._get_trace_notifs())
        self.assertEqual(ts_begin_end_list[0][0], 0)
        self.assertEqual(ts_begin_end_list[0][1], 65547)

    def test_ts_begin_from_0(self):
        self._complete_sc_event_simple_ts()
        self._append_event_simple_ts(12)
        self._append_event_simple_ts(144)
        self._stream.flush()
        ts_begin_end_list = self._get_packet_ts_begin_end(self._get_trace_notifs())
        self.assertEqual(ts_begin_end_list[0][0], 0)
        self.assertEqual(ts_begin_end_list[0][1], 144)

    def test_ts_begin_from_last_ts_end(self):
        self._complete_sc_event_simple_ts()
        self._append_event_simple_ts(12)
        self._append_event_simple_ts(144)
        self._stream.flush()
        self._append_event_simple_ts(2001)
        self._append_event_simple_ts(3500)
        self._stream.flush()
        ts_begin_end_list = self._get_packet_ts_begin_end(self._get_trace_notifs())
        self.assertEqual(ts_begin_end_list[0][0], 0)
        self.assertEqual(ts_begin_end_list[0][1], 144)
        self.assertEqual(ts_begin_end_list[1][0], 144)
        self.assertEqual(ts_begin_end_list[1][1], 3500)

    def test_ts_begin_from_provided(self):
        self._complete_sc_event_simple_ts()
        self._set_ts_begin_end(ts_begin=17)
        self._append_event_simple_ts(11)
        self._append_event_simple_ts(15)
        self._stream.flush()
        ts_begin_end_list = self._get_packet_ts_begin_end(self._get_trace_notifs())
        self.assertEqual(ts_begin_end_list[0][0], 17)
        self.assertEqual(ts_begin_end_list[0][1], 65551)

    def test_ts_end_from_provided(self):
        self._complete_sc_event_simple_ts()
        self._set_ts_begin_end(ts_end=1001)
        self._append_event_simple_ts(11)
        self._append_event_simple_ts(15)
        self._stream.flush()
        ts_begin_end_list = self._get_packet_ts_begin_end(self._get_trace_notifs())
        self.assertEqual(ts_begin_end_list[0][0], 0)
        self.assertEqual(ts_begin_end_list[0][1], 1001)

    def test_ts_end_from_provided_equal(self):
        self._complete_sc_event_simple_ts()
        self._set_ts_begin_end(ts_end=1001)
        self._append_event_simple_ts(11)
        self._append_event_simple_ts(1001)
        self._stream.flush()
        ts_begin_end_list = self._get_packet_ts_begin_end(self._get_trace_notifs())
        self.assertEqual(ts_begin_end_list[0][0], 0)
        self.assertEqual(ts_begin_end_list[0][1], 1001)

    def test_ts_end_from_provided_too_small(self):
        self._complete_sc_event_simple_ts()
        self._set_ts_begin_end(ts_end=1001)
        self._append_event_simple_ts(11)
        self._append_event_simple_ts(1002)

        with self.assertRaises(bt2.Error):
            self._stream.flush()

    def test_ts_begin_provided_equal_last_ts_end_two_packets(self):
        self._complete_sc_event_simple_ts()
        self._append_event_simple_ts(12)
        self._append_event_simple_ts(1001)
        self._stream.flush()
        self._set_ts_begin_end(ts_begin=1001)
        self._append_event_simple_ts(2001)
        self._append_event_simple_ts(3500)
        self._stream.flush()
        ts_begin_end_list = self._get_packet_ts_begin_end(self._get_trace_notifs())
        self.assertEqual(ts_begin_end_list[0][0], 0)
        self.assertEqual(ts_begin_end_list[0][1], 1001)
        self.assertEqual(ts_begin_end_list[1][0], 1001)
        self.assertEqual(ts_begin_end_list[1][1], 3500)

    def test_ts_begin_provided_less_than_last_ts_end_two_packets(self):
        self._complete_sc_event_simple_ts()
        self._append_event_simple_ts(12)
        self._append_event_simple_ts(1001)
        self._stream.flush()
        self._set_ts_begin_end(ts_begin=1000)
        self._append_event_simple_ts(2001)
        self._append_event_simple_ts(3500)

        with self.assertRaises(bt2.Error):
            self._stream.flush()

    def test_ts_end_from_value_event_payload(self):
        self._complete_sc_event_ts_in_payload()
        self._append_event_ts_in_payload(11, 15)
        self._append_event_ts_in_payload(5, 10)
        self._append_event_ts_in_payload(18, 10)
        self._stream.flush()
        ts_begin_end_list = self._get_packet_ts_begin_end(self._get_trace_notifs())
        self.assertEqual(ts_begin_end_list[0][0], 0)
        self.assertEqual(ts_begin_end_list[0][1], 65802)

    def test_empty_packet_auto_ts_begin_end(self):
        self._complete_sc_event_simple_ts()
        self._stream.flush()
        ts_begin_end_list = self._get_packet_ts_begin_end(self._get_trace_notifs())
        self.assertEqual(ts_begin_end_list[0][0], 0)
        self.assertEqual(ts_begin_end_list[0][1], 0)

    def test_empty_packet_provided_ts_begin(self):
        self._complete_sc_event_simple_ts()
        self._set_ts_begin_end(ts_begin=1001)
        self._stream.flush()
        ts_begin_end_list = self._get_packet_ts_begin_end(self._get_trace_notifs())
        self.assertEqual(ts_begin_end_list[0][0], 1001)
        self.assertEqual(ts_begin_end_list[0][1], 1001)

    def test_empty_packet_provided_ts_end(self):
        self._complete_sc_event_simple_ts()
        self._set_ts_begin_end(ts_end=1001)
        self._stream.flush()
        ts_begin_end_list = self._get_packet_ts_begin_end(self._get_trace_notifs())
        self.assertEqual(ts_begin_end_list[0][0], 0)
        self.assertEqual(ts_begin_end_list[0][1], 1001)

    def test_empty_packet_provided_ts_begin_end(self):
        self._complete_sc_event_simple_ts()
        self._set_ts_begin_end(1001, 3003)
        self._stream.flush()
        ts_begin_end_list = self._get_packet_ts_begin_end(self._get_trace_notifs())
        self.assertEqual(ts_begin_end_list[0][0], 1001)
        self.assertEqual(ts_begin_end_list[0][1], 3003)

    def test_empty_packet_ts_begin_from_last_ts_end(self):
        self._complete_sc_event_simple_ts()
        self._set_ts_begin_end(1001, 3003)
        self._stream.flush()
        self._set_ts_begin_end(ts_end=6000)
        self._stream.flush()
        ts_begin_end_list = self._get_packet_ts_begin_end(self._get_trace_notifs())
        self.assertEqual(ts_begin_end_list[0][0], 1001)
        self.assertEqual(ts_begin_end_list[0][1], 3003)
        self.assertEqual(ts_begin_end_list[1][0], 3003)
        self.assertEqual(ts_begin_end_list[1][1], 6000)
