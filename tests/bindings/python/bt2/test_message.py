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

import collections
import unittest
import bt2


class AllMessagesTestCase(unittest.TestCase):
    def setUp(self):

        class MyIter(bt2._UserMessageIterator):
            def __init__(self, self_port_output):
                self._at = 0
                self._with_stream_msgs_clock_snapshots = self_port_output.user_data.get('with_stream_msgs_clock_snapshots', False)

            def __next__(self):
                if test_obj._clock_class:
                    if self._at == 0:
                        if self._with_stream_msgs_clock_snapshots:
                            msg = self._create_stream_beginning_message(test_obj._stream, default_clock_snapshot=self._at)
                        else:
                            msg = self._create_stream_beginning_message(test_obj._stream)
                    elif self._at == 1:
                        msg = self._create_packet_beginning_message(test_obj._packet, self._at)
                    elif self._at == 2:
                        msg = self._create_event_message(test_obj._event_class, test_obj._packet, self._at)
                    elif self._at == 3:
                        msg = self._create_message_iterator_inactivity_message(test_obj._clock_class, self._at)
                    elif self._at == 4:
                        msg = self._create_discarded_events_message(test_obj._stream, 890, self._at, self._at)
                    elif self._at == 5:
                        msg = self._create_packet_end_message(test_obj._packet, self._at)
                    elif self._at == 6:
                        msg = self._create_discarded_packets_message(test_obj._stream, 678, self._at, self._at)
                    elif self._at == 7:
                        if self._with_stream_msgs_clock_snapshots:
                            msg = self._create_stream_end_message(test_obj._stream, default_clock_snapshot=self._at)
                        else:
                            msg = self._create_stream_end_message(test_obj._stream)
                    elif self._at >= 8:
                        raise bt2.Stop
                else:
                    if self._at == 0:
                        msg = self._create_stream_beginning_message(test_obj._stream)
                    elif self._at == 1:
                        msg = self._create_packet_beginning_message(test_obj._packet)
                    elif self._at == 2:
                        msg = self._create_event_message(test_obj._event_class, test_obj._packet)
                    elif self._at == 3:
                        msg = self._create_discarded_events_message(test_obj._stream, 890)
                    elif self._at == 4:
                        msg = self._create_packet_end_message(test_obj._packet)
                    elif self._at == 5:
                        msg = self._create_discarded_packets_message(test_obj._stream, 678)
                    elif self._at == 6:
                        msg = self._create_stream_end_message(test_obj._stream)
                    elif self._at >= 7:
                        raise bt2.Stop

                self._at += 1
                return msg

        class MySrc(bt2._UserSourceComponent, message_iterator_class=MyIter):
            def __init__(self, params):
                self._add_output_port('out', params)

                with_cc = bool(params['with_cc'])
                tc = self._create_trace_class()
                if with_cc:
                    cc = self._create_clock_class()
                else:
                    cc = None

                sc = tc.create_stream_class(default_clock_class=cc,
                                            packets_have_beginning_default_clock_snapshot=with_cc,
                                            packets_have_end_default_clock_snapshot=with_cc,
                                            supports_discarded_events=True,
                                            discarded_events_have_default_clock_snapshots=with_cc,
                                            supports_discarded_packets=True,
                                            discarded_packets_have_default_clock_snapshots=with_cc)

                # Create payload field class
                my_int_fc = tc.create_signed_integer_field_class(32)
                payload_fc = tc.create_structure_field_class()
                payload_fc += collections.OrderedDict([
                    ('my_int', my_int_fc),
                ])

                ec = sc.create_event_class(name='salut', payload_field_class=payload_fc)

                trace = tc()
                stream = trace.create_stream(sc)
                packet = stream.create_packet()

                test_obj._trace = trace
                test_obj._stream = stream
                test_obj._packet = packet
                test_obj._event_class = ec
                test_obj._clock_class = cc

        test_obj = self
        self._graph = bt2.Graph()
        self._src = MySrc
        self._iter = MyIter

    def test_all_msg_with_cc(self):
        params = {'with_cc': True}
        self._src_comp = self._graph.add_component(self._src, 'my_source', params)
        self._msg_iter = self._graph.create_output_port_message_iterator(self._src_comp.output_ports['out'])

        for i, msg in enumerate(self._msg_iter):
            if i == 0:
                self.assertIsInstance(msg, bt2.message._StreamBeginningMessage)
                self.assertEqual(msg.stream.addr, self._stream.addr)
                self.assertIsInstance(msg.default_clock_snapshot, bt2.clock_snapshot._UnknownClockSnapshot)
            elif i == 1:
                self.assertIsInstance(msg, bt2.message._PacketBeginningMessage)
                self.assertEqual(msg.packet.addr, self._packet.addr)
                self.assertEqual(msg.default_clock_snapshot.value, i)
            elif i == 2:
                self.assertIsInstance(msg, bt2.message._EventMessage)
                self.assertEqual(msg.event.cls.addr, self._event_class.addr)
                self.assertEqual(msg.default_clock_snapshot.value, i)
            elif i == 3:
                self.assertIsInstance(msg, bt2.message._MessageIteratorInactivityMessage)
                self.assertEqual(msg.default_clock_snapshot.value, i)
            elif i == 4:
                self.assertIsInstance(msg, bt2.message._DiscardedEventsMessage)
                self.assertEqual(msg.stream.addr, self._stream.addr)
                self.assertEqual(msg.count, 890)
                self.assertEqual(msg.stream.cls.default_clock_class.addr, self._clock_class.addr)
                self.assertEqual(msg.beginning_default_clock_snapshot.value, i)
                self.assertEqual(msg.end_default_clock_snapshot.value, i)
            elif i == 5:
                self.assertIsInstance(msg, bt2.message._PacketEndMessage)
                self.assertEqual(msg.packet.addr, self._packet.addr)
                self.assertEqual(msg.default_clock_snapshot.value, i)
            elif i == 6:
                self.assertIsInstance(msg, bt2.message._DiscardedPacketsMessage)
                self.assertEqual(msg.stream.addr, self._stream.addr)
                self.assertEqual(msg.count, 678)
                self.assertEqual(msg.stream.cls.default_clock_class.addr, self._clock_class.addr)
                self.assertEqual(msg.beginning_default_clock_snapshot.value, i)
                self.assertEqual(msg.end_default_clock_snapshot.value, i)
            elif i == 7:
                self.assertIsInstance(msg, bt2.message._StreamEndMessage)
                self.assertEqual(msg.stream.addr, self._stream.addr)
                self.assertIsInstance(msg.default_clock_snapshot, bt2.clock_snapshot._UnknownClockSnapshot)
            else:
                raise Exception

    def test_all_msg_without_cc(self):
        params = {'with_cc': False}
        self._src_comp = self._graph.add_component(self._src, 'my_source', params)
        self._msg_iter = self._graph.create_output_port_message_iterator(self._src_comp.output_ports['out'])

        for i, msg in enumerate(self._msg_iter):
            if i == 0:
                self.assertIsInstance(msg, bt2.message._StreamBeginningMessage)
                self.assertEqual(msg.stream.addr, self._stream.addr)
                with self.assertRaises(bt2.NonexistentClockSnapshot):
                    msg.default_clock_snapshot
            elif i == 1:
                self.assertIsInstance(msg, bt2.message._PacketBeginningMessage)
                self.assertEqual(msg.packet.addr, self._packet.addr)
            elif i == 2:
                self.assertIsInstance(msg, bt2.message._EventMessage)
                self.assertEqual(msg.event.cls.addr, self._event_class.addr)
                with self.assertRaises(bt2.NonexistentClockSnapshot):
                    msg.default_clock_snapshot
            elif i == 3:
                self.assertIsInstance(msg, bt2.message._DiscardedEventsMessage)
                self.assertEqual(msg.stream.addr, self._stream.addr)
                self.assertEqual(msg.count, 890)
                self.assertIsNone(msg.stream.cls.default_clock_class)
                with self.assertRaises(bt2.NonexistentClockSnapshot):
                    msg.beginning_default_clock_snapshot
                with self.assertRaises(bt2.NonexistentClockSnapshot):
                    msg.end_default_clock_snapshot
            elif i == 4:
                self.assertIsInstance(msg, bt2.message._PacketEndMessage)
                self.assertEqual(msg.packet.addr, self._packet.addr)
            elif i == 5:
                self.assertIsInstance(msg, bt2.message._DiscardedPacketsMessage)
                self.assertEqual(msg.stream.addr, self._stream.addr)
                self.assertEqual(msg.count, 678)
                self.assertIsNone(msg.stream.cls.default_clock_class)
                with self.assertRaises(bt2.NonexistentClockSnapshot):
                    msg.beginning_default_clock_snapshot
                with self.assertRaises(bt2.NonexistentClockSnapshot):
                    msg.end_default_clock_snapshot
            elif i == 6:
                self.assertIsInstance(msg, bt2.message._StreamEndMessage)
                self.assertEqual(msg.stream.addr, self._stream.addr)
                with self.assertRaises(bt2.NonexistentClockSnapshot):
                    msg.default_clock_snapshot
            else:
                raise Exception

    def test_msg_stream_with_clock_snapshots(self):
        params = {
            'with_cc': True,
            'with_stream_msgs_clock_snapshots': True,
        }

        self._src_comp = self._graph.add_component(self._src, 'my_source', params)
        self._msg_iter = self._graph.create_output_port_message_iterator(self._src_comp.output_ports['out'])
        msgs = list(self._msg_iter)

        msg_stream_beg = msgs[0]
        self.assertIsInstance(msg_stream_beg, bt2.message._StreamBeginningMessage)
        self.assertEqual(msg_stream_beg.default_clock_snapshot.value, 0)

        msg_stream_end = msgs[7]
        self.assertIsInstance(msg_stream_end, bt2.message._StreamEndMessage)
        self.assertEqual(msg_stream_end.default_clock_snapshot.value, 7)
