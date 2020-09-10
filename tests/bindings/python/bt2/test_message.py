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
import utils
from utils import TestOutputPortMessageIterator
from bt2 import clock_snapshot as bt2_clock_snapshot
from bt2 import event as bt2_event
from bt2 import event_class as bt2_event_class
from bt2 import field as bt2_field
from bt2 import packet as bt2_packet
from bt2 import stream as bt2_stream
from bt2 import stream_class as bt2_stream_class
from bt2 import trace as bt2_trace
from bt2 import trace_class as bt2_trace_class


class AllMessagesTestCase(unittest.TestCase):
    def setUp(self):
        class MyIter(bt2._UserMessageIterator):
            def __init__(self, config, self_port_output):
                self._at = 0
                self._with_stream_msgs_clock_snapshots = self_port_output.user_data.get(
                    'with_stream_msgs_clock_snapshots', False
                )

            def __next__(self):
                if test_obj._clock_class:
                    if self._at == 0:
                        if self._with_stream_msgs_clock_snapshots:
                            msg = self._create_stream_beginning_message(
                                test_obj._stream, default_clock_snapshot=self._at
                            )
                        else:
                            msg = self._create_stream_beginning_message(
                                test_obj._stream
                            )
                        test_obj.assertIs(type(msg), bt2._StreamBeginningMessage)
                    elif self._at == 1:
                        msg = self._create_packet_beginning_message(
                            test_obj._packet, self._at
                        )
                        test_obj.assertIs(type(msg), bt2._PacketBeginningMessage)
                    elif self._at == 2:
                        msg = self._create_event_message(
                            test_obj._event_class, test_obj._packet, self._at
                        )
                        test_obj.assertIs(type(msg), bt2._EventMessage)
                    elif self._at == 3:
                        msg = self._create_message_iterator_inactivity_message(
                            test_obj._clock_class, self._at
                        )
                    elif self._at == 4:
                        msg = self._create_discarded_events_message(
                            test_obj._stream, 890, self._at, self._at
                        )
                        test_obj.assertIs(type(msg), bt2._DiscardedEventsMessage)
                    elif self._at == 5:
                        msg = self._create_packet_end_message(
                            test_obj._packet, self._at
                        )
                        test_obj.assertIs(type(msg), bt2._PacketEndMessage)
                    elif self._at == 6:
                        msg = self._create_discarded_packets_message(
                            test_obj._stream, 678, self._at, self._at
                        )
                        test_obj.assertIs(type(msg), bt2._DiscardedPacketsMessage)
                    elif self._at == 7:
                        if self._with_stream_msgs_clock_snapshots:
                            msg = self._create_stream_end_message(
                                test_obj._stream, default_clock_snapshot=self._at
                            )
                        else:
                            msg = self._create_stream_end_message(test_obj._stream)
                        test_obj.assertIs(type(msg), bt2._StreamEndMessage)
                    elif self._at >= 8:
                        raise bt2.Stop
                else:
                    if self._at == 0:
                        msg = self._create_stream_beginning_message(test_obj._stream)
                    elif self._at == 1:
                        msg = self._create_packet_beginning_message(test_obj._packet)
                    elif self._at == 2:
                        msg = self._create_event_message(
                            test_obj._event_class, test_obj._packet
                        )
                    elif self._at == 3:
                        msg = self._create_discarded_events_message(
                            test_obj._stream, 890
                        )
                    elif self._at == 4:
                        msg = self._create_packet_end_message(test_obj._packet)
                    elif self._at == 5:
                        msg = self._create_discarded_packets_message(
                            test_obj._stream, 678
                        )
                    elif self._at == 6:
                        msg = self._create_stream_end_message(test_obj._stream)
                    elif self._at >= 7:
                        raise bt2.Stop

                self._at += 1
                return msg

        class MySrc(bt2._UserSourceComponent, message_iterator_class=MyIter):
            def __init__(self, config, params, obj):
                self._add_output_port('out', params)

                with_cc = bool(params['with_cc'])
                tc = self._create_trace_class()
                if with_cc:
                    cc = self._create_clock_class()
                else:
                    cc = None

                sc = tc.create_stream_class(
                    default_clock_class=cc,
                    supports_packets=True,
                    packets_have_beginning_default_clock_snapshot=with_cc,
                    packets_have_end_default_clock_snapshot=with_cc,
                    supports_discarded_events=True,
                    discarded_events_have_default_clock_snapshots=with_cc,
                    supports_discarded_packets=True,
                    discarded_packets_have_default_clock_snapshots=with_cc,
                )

                # Create payload field class
                my_int_fc = tc.create_signed_integer_field_class(32)
                payload_fc = tc.create_structure_field_class()
                payload_fc += [('my_int', my_int_fc)]

                # Create specific context field class
                my_int_fc = tc.create_signed_integer_field_class(32)
                specific_fc = tc.create_structure_field_class()
                specific_fc += [('my_int', my_int_fc)]

                ec = sc.create_event_class(
                    name='salut',
                    payload_field_class=payload_fc,
                    specific_context_field_class=specific_fc,
                )

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
        self._msg_iter = TestOutputPortMessageIterator(
            self._graph, self._src_comp.output_ports['out']
        )

        for i, msg in enumerate(self._msg_iter):
            if i == 0:
                self.assertIs(type(msg), bt2._StreamBeginningMessageConst)
                self.assertIs(type(msg.stream), bt2_stream._StreamConst)
                self.assertEqual(msg.stream.addr, self._stream.addr)
                self.assertIsInstance(
                    msg.default_clock_snapshot, bt2._UnknownClockSnapshot
                )
            elif i == 1:
                self.assertIs(type(msg), bt2._PacketBeginningMessageConst)
                self.assertIs(type(msg.packet), bt2_packet._PacketConst)
                self.assertIs(
                    type(msg.default_clock_snapshot),
                    bt2_clock_snapshot._ClockSnapshotConst,
                )
                self.assertEqual(msg.packet.addr, self._packet.addr)
                self.assertEqual(msg.default_clock_snapshot.value, i)
            elif i == 2:
                self.assertIs(type(msg), bt2._EventMessageConst)
                self.assertIs(type(msg.event), bt2_event._EventConst)
                self.assertIs(
                    type(msg.default_clock_snapshot),
                    bt2_clock_snapshot._ClockSnapshotConst,
                )
                self.assertIs(
                    type(msg.event.payload_field), bt2_field._StructureFieldConst
                )
                self.assertIs(
                    type(msg.event.payload_field['my_int']),
                    bt2_field._SignedIntegerFieldConst,
                )

                self.assertEqual(msg.event.cls.addr, self._event_class.addr)
                self.assertEqual(msg.default_clock_snapshot.value, i)
            elif i == 3:
                self.assertIs(type(msg), bt2._MessageIteratorInactivityMessageConst)
                self.assertIs(
                    type(msg.clock_snapshot), bt2_clock_snapshot._ClockSnapshotConst
                )
                self.assertEqual(msg.clock_snapshot.value, i)
            elif i == 4:
                self.assertIs(type(msg), bt2._DiscardedEventsMessageConst)
                self.assertIs(type(msg.stream), bt2_stream._StreamConst)
                self.assertIs(type(msg.stream.cls), bt2_stream_class._StreamClassConst)
                self.assertIs(
                    type(msg.beginning_default_clock_snapshot),
                    bt2_clock_snapshot._ClockSnapshotConst,
                )
                self.assertIs(
                    type(msg.end_default_clock_snapshot),
                    bt2_clock_snapshot._ClockSnapshotConst,
                )

                self.assertEqual(msg.stream.addr, self._stream.addr)
                self.assertEqual(msg.count, 890)
                self.assertEqual(
                    msg.stream.cls.default_clock_class.addr, self._clock_class.addr
                )
                self.assertEqual(msg.beginning_default_clock_snapshot.value, i)
                self.assertEqual(msg.end_default_clock_snapshot.value, i)
            elif i == 5:
                self.assertIs(type(msg), bt2._PacketEndMessageConst)
                self.assertIs(type(msg.packet), bt2_packet._PacketConst)
                self.assertIs(
                    type(msg.default_clock_snapshot),
                    bt2_clock_snapshot._ClockSnapshotConst,
                )
                self.assertEqual(msg.packet.addr, self._packet.addr)
                self.assertEqual(msg.default_clock_snapshot.value, i)
            elif i == 6:
                self.assertIs(type(msg), bt2._DiscardedPacketsMessageConst)
                self.assertIs(type(msg.stream), bt2_stream._StreamConst)
                self.assertIs(type(msg.stream.trace), bt2_trace._TraceConst)
                self.assertIs(
                    type(msg.stream.trace.cls), bt2_trace_class._TraceClassConst
                )
                self.assertIs(
                    type(msg.beginning_default_clock_snapshot),
                    bt2_clock_snapshot._ClockSnapshotConst,
                )
                self.assertIs(
                    type(msg.end_default_clock_snapshot),
                    bt2_clock_snapshot._ClockSnapshotConst,
                )
                self.assertEqual(msg.stream.addr, self._stream.addr)
                self.assertEqual(msg.count, 678)
                self.assertEqual(
                    msg.stream.cls.default_clock_class.addr, self._clock_class.addr
                )
                self.assertEqual(msg.beginning_default_clock_snapshot.value, i)
                self.assertEqual(msg.end_default_clock_snapshot.value, i)
            elif i == 7:
                self.assertIs(type(msg), bt2._StreamEndMessageConst)
                self.assertIs(type(msg.stream), bt2_stream._StreamConst)
                self.assertEqual(msg.stream.addr, self._stream.addr)
                self.assertIs(
                    type(msg.default_clock_snapshot), bt2._UnknownClockSnapshot
                )
            else:
                raise Exception

    def test_all_msg_without_cc(self):
        params = {'with_cc': False}
        self._src_comp = self._graph.add_component(self._src, 'my_source', params)
        self._msg_iter = TestOutputPortMessageIterator(
            self._graph, self._src_comp.output_ports['out']
        )

        for i, msg in enumerate(self._msg_iter):
            if i == 0:
                self.assertIsInstance(msg, bt2._StreamBeginningMessageConst)
                self.assertIs(type(msg.stream), bt2_stream._StreamConst)
                self.assertEqual(msg.stream.addr, self._stream.addr)
                with self.assertRaisesRegex(
                    ValueError, 'stream class has no default clock class'
                ):
                    msg.default_clock_snapshot
            elif i == 1:
                self.assertIsInstance(msg, bt2._PacketBeginningMessageConst)
                self.assertIs(type(msg.packet), bt2_packet._PacketConst)
                self.assertEqual(msg.packet.addr, self._packet.addr)
            elif i == 2:
                self.assertIsInstance(msg, bt2._EventMessageConst)
                self.assertIs(type(msg.event), bt2_event._EventConst)
                self.assertIs(type(msg.event.cls), bt2_event_class._EventClassConst)
                self.assertEqual(msg.event.cls.addr, self._event_class.addr)
                with self.assertRaisesRegex(
                    ValueError, 'stream class has no default clock class'
                ):
                    msg.default_clock_snapshot
            elif i == 3:
                self.assertIsInstance(msg, bt2._DiscardedEventsMessageConst)
                self.assertIs(type(msg.stream), bt2_stream._StreamConst)
                self.assertIs(type(msg.stream.cls), bt2_stream_class._StreamClassConst)
                self.assertEqual(msg.stream.addr, self._stream.addr)
                self.assertEqual(msg.count, 890)
                self.assertIsNone(msg.stream.cls.default_clock_class)
                with self.assertRaisesRegex(
                    ValueError,
                    'such a message has no clock snapshots for this stream class',
                ):
                    msg.beginning_default_clock_snapshot
                with self.assertRaisesRegex(
                    ValueError,
                    'such a message has no clock snapshots for this stream class',
                ):
                    msg.end_default_clock_snapshot
            elif i == 4:
                self.assertIsInstance(msg, bt2._PacketEndMessageConst)
                self.assertEqual(msg.packet.addr, self._packet.addr)
                self.assertIs(type(msg.packet), bt2_packet._PacketConst)
            elif i == 5:
                self.assertIsInstance(msg, bt2._DiscardedPacketsMessageConst)
                self.assertIs(type(msg.stream), bt2_stream._StreamConst)
                self.assertIs(type(msg.stream.cls), bt2_stream_class._StreamClassConst)
                self.assertIs(
                    type(msg.stream.cls.trace_class), bt2_trace_class._TraceClassConst
                )
                self.assertEqual(msg.stream.addr, self._stream.addr)
                self.assertEqual(msg.count, 678)
                self.assertIsNone(msg.stream.cls.default_clock_class)
                with self.assertRaisesRegex(
                    ValueError,
                    'such a message has no clock snapshots for this stream class',
                ):
                    msg.beginning_default_clock_snapshot
                with self.assertRaisesRegex(
                    ValueError,
                    'such a message has no clock snapshots for this stream class',
                ):
                    msg.end_default_clock_snapshot
            elif i == 6:
                self.assertIsInstance(msg, bt2._StreamEndMessageConst)
                self.assertIs(type(msg.stream), bt2_stream._StreamConst)
                self.assertEqual(msg.stream.addr, self._stream.addr)
                with self.assertRaisesRegex(
                    ValueError, 'stream class has no default clock class'
                ):
                    msg.default_clock_snapshot
            else:
                raise Exception

    def test_msg_stream_with_clock_snapshots(self):
        params = {'with_cc': True, 'with_stream_msgs_clock_snapshots': True}

        self._src_comp = self._graph.add_component(self._src, 'my_source', params)
        self._msg_iter = TestOutputPortMessageIterator(
            self._graph, self._src_comp.output_ports['out']
        )
        msgs = list(self._msg_iter)

        msg_stream_beg = msgs[0]
        self.assertIsInstance(msg_stream_beg, bt2._StreamBeginningMessageConst)
        self.assertIs(
            type(msg_stream_beg.default_clock_snapshot),
            bt2_clock_snapshot._ClockSnapshotConst,
        )
        self.assertEqual(msg_stream_beg.default_clock_snapshot.value, 0)

        msg_stream_end = msgs[7]
        self.assertIsInstance(msg_stream_end, bt2._StreamEndMessageConst)
        self.assertIs(
            type(msg_stream_end.default_clock_snapshot),
            bt2_clock_snapshot._ClockSnapshotConst,
        )
        self.assertEqual(msg_stream_end.default_clock_snapshot.value, 7)

    def test_stream_beg_msg(self):
        msg = utils.get_stream_beginning_message()
        self.assertIs(type(msg.stream), bt2_stream._Stream)

    def test_stream_end_msg(self):
        msg = utils.get_stream_end_message()
        self.assertIs(type(msg.stream), bt2_stream._Stream)

    def test_packet_beg_msg(self):
        msg = utils.get_packet_beginning_message()
        self.assertIs(type(msg.packet), bt2_packet._Packet)

    def test_packet_end_msg(self):
        msg = utils.get_packet_end_message()
        self.assertIs(type(msg.packet), bt2_packet._Packet)

    def test_event_msg(self):
        msg = utils.get_event_message()
        self.assertIs(type(msg.event), bt2_event._Event)


class CreateDiscardedEventMessageTestCase(unittest.TestCase):
    # Most basic case.
    def test_create(self):
        def create_stream_class(tc, cc):
            return tc.create_stream_class(supports_discarded_events=True)

        def msg_iter_next(msg_iter, stream):
            return msg_iter._create_discarded_events_message(stream)

        msg = utils.run_in_message_iterator_next(create_stream_class, msg_iter_next)
        self.assertIs(type(msg), bt2._DiscardedEventsMessage)
        self.assertIs(msg.count, None)

    # With event count.
    def test_create_with_count(self):
        def create_stream_class(tc, cc):
            return tc.create_stream_class(supports_discarded_events=True)

        def msg_iter_next(msg_iter, stream):
            return msg_iter._create_discarded_events_message(stream, count=242)

        msg = utils.run_in_message_iterator_next(create_stream_class, msg_iter_next)
        self.assertIs(type(msg), bt2._DiscardedEventsMessage)
        self.assertEqual(msg.count, 242)

    # With clock snapshots.
    def test_create_with_clock_snapshots(self):
        def create_stream_class(tc, cc):
            return tc.create_stream_class(
                default_clock_class=cc,
                supports_discarded_events=True,
                discarded_events_have_default_clock_snapshots=True,
            )

        def msg_iter_next(msg_iter, stream):
            return msg_iter._create_discarded_events_message(
                stream, beg_clock_snapshot=10, end_clock_snapshot=20
            )

        msg = utils.run_in_message_iterator_next(create_stream_class, msg_iter_next)
        self.assertIs(type(msg), bt2._DiscardedEventsMessage)
        self.assertEqual(msg.beginning_default_clock_snapshot, 10)
        self.assertEqual(msg.end_default_clock_snapshot, 20)

    # Trying to create when the stream does not support discarded events.
    def test_create_unsupported_raises(self):
        def create_stream_class(tc, cc):
            return tc.create_stream_class()

        def msg_iter_next(msg_iter, stream):
            with self.assertRaisesRegex(
                ValueError, 'stream class does not support discarded events'
            ):
                msg_iter._create_discarded_events_message(stream)

            return 123

        res = utils.run_in_message_iterator_next(create_stream_class, msg_iter_next)
        self.assertEqual(res, 123)

    # Trying to create with clock snapshots when the stream does not support
    # them.
    def test_create_unsupported_clock_snapshots_raises(self):
        def create_stream_class(tc, cc):
            return tc.create_stream_class(supports_discarded_events=True)

        def msg_iter_next(msg_iter, stream):
            with self.assertRaisesRegex(
                ValueError,
                'discarded events have no default clock snapshots for this stream class',
            ):
                msg_iter._create_discarded_events_message(
                    stream, beg_clock_snapshot=10, end_clock_snapshot=20
                )

            return 123

        res = utils.run_in_message_iterator_next(create_stream_class, msg_iter_next)
        self.assertEqual(res, 123)

    # Trying to create without clock snapshots when the stream requires them.
    def test_create_missing_clock_snapshots_raises(self):
        def create_stream_class(tc, cc):
            return tc.create_stream_class(
                default_clock_class=cc,
                supports_discarded_events=True,
                discarded_events_have_default_clock_snapshots=True,
            )

        def msg_iter_next(msg_iter, stream):
            with self.assertRaisesRegex(
                ValueError,
                'discarded events have default clock snapshots for this stream class',
            ):
                msg_iter._create_discarded_events_message(stream)

            return 123

        res = utils.run_in_message_iterator_next(create_stream_class, msg_iter_next)
        self.assertEqual(res, 123)


class CreateDiscardedPacketMessageTestCase(unittest.TestCase):
    # Most basic case.
    def test_create(self):
        def create_stream_class(tc, cc):
            return tc.create_stream_class(
                supports_packets=True, supports_discarded_packets=True
            )

        def msg_iter_next(msg_iter, stream):
            return msg_iter._create_discarded_packets_message(stream)

        msg = utils.run_in_message_iterator_next(create_stream_class, msg_iter_next)
        self.assertIs(type(msg), bt2._DiscardedPacketsMessage)
        self.assertIs(msg.count, None)

    # With packet count.
    def test_create_with_count(self):
        def create_stream_class(tc, cc):
            return tc.create_stream_class(
                supports_packets=True, supports_discarded_packets=True
            )

        def msg_iter_next(msg_iter, stream):
            return msg_iter._create_discarded_packets_message(stream, count=242)

        msg = utils.run_in_message_iterator_next(create_stream_class, msg_iter_next)
        self.assertIs(type(msg), bt2._DiscardedPacketsMessage)
        self.assertEqual(msg.count, 242)

    # With clock snapshots.
    def test_create_with_clock_snapshots(self):
        def create_stream_class(tc, cc):
            return tc.create_stream_class(
                default_clock_class=cc,
                supports_packets=True,
                supports_discarded_packets=True,
                discarded_packets_have_default_clock_snapshots=True,
            )

        def msg_iter_next(msg_iter, stream):
            return msg_iter._create_discarded_packets_message(
                stream, beg_clock_snapshot=10, end_clock_snapshot=20
            )

        msg = utils.run_in_message_iterator_next(create_stream_class, msg_iter_next)
        self.assertIs(type(msg), bt2._DiscardedPacketsMessage)
        self.assertEqual(msg.beginning_default_clock_snapshot, 10)
        self.assertEqual(msg.end_default_clock_snapshot, 20)

    # Trying to create when the stream does not support discarded packets.
    def test_create_unsupported_raises(self):
        def create_stream_class(tc, cc):
            return tc.create_stream_class(
                supports_packets=True,
            )

        def msg_iter_next(msg_iter, stream):
            with self.assertRaisesRegex(
                ValueError, 'stream class does not support discarded packets'
            ):
                msg_iter._create_discarded_packets_message(stream)

            return 123

        res = utils.run_in_message_iterator_next(create_stream_class, msg_iter_next)
        self.assertEqual(res, 123)

    # Trying to create with clock snapshots when the stream does not support
    # them.
    def test_create_unsupported_clock_snapshots_raises(self):
        def create_stream_class(tc, cc):
            return tc.create_stream_class(
                supports_packets=True, supports_discarded_packets=True
            )

        def msg_iter_next(msg_iter, stream):
            with self.assertRaisesRegex(
                ValueError,
                'discarded packets have no default clock snapshots for this stream class',
            ):
                msg_iter._create_discarded_packets_message(
                    stream, beg_clock_snapshot=10, end_clock_snapshot=20
                )

            return 123

        res = utils.run_in_message_iterator_next(create_stream_class, msg_iter_next)
        self.assertEqual(res, 123)

    # Trying to create without clock snapshots when the stream requires them.
    def test_create_missing_clock_snapshots_raises(self):
        def create_stream_class(tc, cc):
            return tc.create_stream_class(
                default_clock_class=cc,
                supports_packets=True,
                supports_discarded_packets=True,
                discarded_packets_have_default_clock_snapshots=True,
            )

        def msg_iter_next(msg_iter, stream):
            with self.assertRaisesRegex(
                ValueError,
                'discarded packets have default clock snapshots for this stream class',
            ):
                msg_iter._create_discarded_packets_message(stream)

            return 123

        res = utils.run_in_message_iterator_next(create_stream_class, msg_iter_next)
        self.assertEqual(res, 123)


if __name__ == '__main__':
    unittest.main()
