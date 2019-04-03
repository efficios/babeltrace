from bt2 import values
import collections
import unittest
import copy
import bt2


class _MessageTestCase(unittest.TestCase):
    def setUp(self):
        self._trace = bt2.Trace()
        self._sc = bt2.StreamClass()
        self._ec = bt2.EventClass('salut')
        self._my_int_fc = bt2.IntegerFieldClass(32)
        self._ec.payload_field_class = bt2.StructureFieldClass()
        self._ec.payload_field_class += collections.OrderedDict([
            ('my_int', self._my_int_fc),
        ])
        self._sc.add_event_class(self._ec)
        self._clock_class = bt2.ClockClass('allo', 1000)
        self._trace.add_clock_class(self._clock_class)
        self._trace.packet_header_field_class = bt2.StructureFieldClass()
        self._trace.packet_header_field_class += collections.OrderedDict([
            ('hello', self._my_int_fc),
        ])
        self._trace.add_stream_class(self._sc)
        self._cc_prio_map = bt2.ClockClassPriorityMap()
        self._cc_prio_map[self._clock_class] = 231
        self._stream = self._sc()
        self._packet = self._stream.create_packet()
        self._packet.header_field['hello'] = 19487
        self._event = self._ec()
        self._event.clock_values.add(self._clock_class(1772))
        self._event.payload_field['my_int'] = 23
        self._event.packet = self._packet

    def tearDown(self):
        del self._trace
        del self._sc
        del self._ec
        del self._my_int_fc
        del self._clock_class
        del self._cc_prio_map
        del self._stream
        del self._packet
        del self._event


@unittest.skip("this is broken")
class EventMessageTestCase(_MessageTestCase):
    def test_create_no_cc_prio_map(self):
        msg = bt2.EventMessage(self._event)
        self.assertEqual(msg.event.addr, self._event.addr)
        self.assertEqual(len(msg.clock_class_priority_map), 0)

    def test_create_with_cc_prio_map(self):
        msg = bt2.EventMessage(self._event, self._cc_prio_map)
        self.assertEqual(msg.event.addr, self._event.addr)
        self.assertEqual(len(msg.clock_class_priority_map), 1)
        self.assertEqual(msg.clock_class_priority_map.highest_priority_clock_class.addr,
                         self._clock_class.addr)
        self.assertEqual(msg.clock_class_priority_map[self._clock_class], 231)

    def test_eq(self):
        msg = bt2.EventMessage(self._event, self._cc_prio_map)
        event_copy = copy.copy(self._event)
        event_copy.packet = self._packet
        cc_prio_map_copy = copy.copy(self._cc_prio_map)
        msg2 = bt2.EventMessage(event_copy, cc_prio_map_copy)
        self.assertEqual(msg, msg2)

    def test_ne_event(self):
        msg = bt2.EventMessage(self._event, self._cc_prio_map)
        event_copy = copy.copy(self._event)
        event_copy.payload_field['my_int'] = 17
        event_copy.packet = self._packet
        cc_prio_map_copy = copy.copy(self._cc_prio_map)
        msg2 = bt2.EventMessage(event_copy, cc_prio_map_copy)
        self.assertNotEqual(msg, msg2)

    def test_ne_cc_prio_map(self):
        msg = bt2.EventMessage(self._event)
        event_copy = copy.copy(self._event)
        event_copy.packet = self._packet
        cc_prio_map_copy = copy.copy(self._cc_prio_map)
        msg2 = bt2.EventMessage(event_copy, cc_prio_map_copy)
        self.assertNotEqual(msg, msg2)

    def test_eq_invalid(self):
        msg = bt2.EventMessage(self._event)
        self.assertNotEqual(msg, 23)

    def test_copy(self):
        msg = bt2.EventMessage(self._event, self._cc_prio_map)
        msg2 = copy.copy(msg)
        self.assertEqual(msg, msg2)

    def test_deepcopy(self):
        msg = bt2.EventMessage(self._event, self._cc_prio_map)
        msg2 = copy.deepcopy(msg)
        self.assertEqual(msg, msg2)


@unittest.skip("this is broken")
class PacketBeginningMessageTestCase(_MessageTestCase):
    def test_create(self):
        msg = bt2.PacketBeginningMessage(self._packet)
        self.assertEqual(msg.packet.addr, self._packet.addr)

    def test_eq(self):
        msg = bt2.PacketBeginningMessage(self._packet)
        packet_copy = copy.copy(self._packet)
        msg2 = bt2.PacketBeginningMessage(packet_copy)
        self.assertEqual(msg, msg2)

    def test_ne_packet(self):
        msg = bt2.PacketBeginningMessage(self._packet)
        packet_copy = copy.copy(self._packet)
        packet_copy.header_field['hello'] = 1847
        msg2 = bt2.PacketBeginningMessage(packet_copy)
        self.assertNotEqual(msg, msg2)

    def test_eq_invalid(self):
        msg = bt2.PacketBeginningMessage(self._packet)
        self.assertNotEqual(msg, 23)

    def test_copy(self):
        msg = bt2.PacketBeginningMessage(self._packet)
        msg2 = copy.copy(msg)
        self.assertEqual(msg, msg2)

    def test_deepcopy(self):
        msg = bt2.PacketBeginningMessage(self._packet)
        msg2 = copy.deepcopy(msg)
        self.assertEqual(msg, msg2)


@unittest.skip("this is broken")
class PacketEndMessageTestCase(_MessageTestCase):
    def test_create(self):
        msg = bt2.PacketEndMessage(self._packet)
        self.assertEqual(msg.packet.addr, self._packet.addr)

    def test_eq(self):
        msg = bt2.PacketEndMessage(self._packet)
        packet_copy = copy.copy(self._packet)
        msg2 = bt2.PacketEndMessage(packet_copy)
        self.assertEqual(msg, msg2)

    def test_ne_packet(self):
        msg = bt2.PacketEndMessage(self._packet)
        packet_copy = copy.copy(self._packet)
        packet_copy.header_field['hello'] = 1847
        msg2 = bt2.PacketEndMessage(packet_copy)
        self.assertNotEqual(msg, msg2)

    def test_eq_invalid(self):
        msg = bt2.PacketEndMessage(self._packet)
        self.assertNotEqual(msg, 23)

    def test_copy(self):
        msg = bt2.PacketEndMessage(self._packet)
        msg2 = copy.copy(msg)
        self.assertEqual(msg, msg2)

    def test_deepcopy(self):
        msg = bt2.PacketEndMessage(self._packet)
        msg2 = copy.deepcopy(msg)
        self.assertEqual(msg, msg2)


@unittest.skip("this is broken")
class StreamBeginningMessageTestCase(_MessageTestCase):
    def test_create(self):
        msg = bt2.StreamBeginningMessage(self._stream)
        self.assertEqual(msg.stream.addr, self._stream.addr)

    def test_eq(self):
        msg = bt2.StreamBeginningMessage(self._stream)
        stream_copy = copy.copy(self._stream)
        msg2 = bt2.StreamBeginningMessage(stream_copy)
        self.assertEqual(msg, msg2)

    def test_ne_stream(self):
        msg = bt2.StreamBeginningMessage(self._stream)
        stream_copy = self._sc(name='salut')
        msg2 = bt2.StreamBeginningMessage(stream_copy)
        self.assertNotEqual(msg, msg2)

    def test_eq_invalid(self):
        msg = bt2.StreamBeginningMessage(self._stream)
        self.assertNotEqual(msg, 23)

    def test_copy(self):
        msg = bt2.StreamBeginningMessage(self._stream)
        msg2 = copy.copy(msg)
        self.assertEqual(msg, msg2)

    def test_deepcopy(self):
        msg = bt2.StreamBeginningMessage(self._stream)
        msg2 = copy.deepcopy(msg)
        self.assertEqual(msg, msg2)


@unittest.skip("this is broken")
class StreamEndMessageTestCase(_MessageTestCase):
    def test_create(self):
        msg = bt2.StreamEndMessage(self._stream)
        self.assertEqual(msg.stream.addr, self._stream.addr)

    def test_eq(self):
        msg = bt2.StreamEndMessage(self._stream)
        stream_copy = copy.copy(self._stream)
        msg2 = bt2.StreamEndMessage(stream_copy)
        self.assertEqual(msg, msg2)

    def test_ne_stream(self):
        msg = bt2.StreamEndMessage(self._stream)
        stream_copy = self._sc(name='salut')
        msg2 = bt2.StreamEndMessage(stream_copy)
        self.assertNotEqual(msg, msg2)

    def test_eq_invalid(self):
        msg = bt2.StreamEndMessage(self._stream)
        self.assertNotEqual(msg, 23)

    def test_copy(self):
        msg = bt2.StreamEndMessage(self._stream)
        msg2 = copy.copy(msg)
        self.assertEqual(msg, msg2)

    def test_deepcopy(self):
        msg = bt2.StreamEndMessage(self._stream)
        msg2 = copy.deepcopy(msg)
        self.assertEqual(msg, msg2)


@unittest.skip("this is broken")
class InactivityMessageTestCase(unittest.TestCase):
    def setUp(self):
        self._cc1 = bt2.ClockClass('cc1', 1000)
        self._cc2 = bt2.ClockClass('cc2', 2000)
        self._cc_prio_map = bt2.ClockClassPriorityMap()
        self._cc_prio_map[self._cc1] = 25
        self._cc_prio_map[self._cc2] = 50

    def tearDown(self):
        del self._cc1
        del self._cc2
        del self._cc_prio_map

    def test_create_no_cc_prio_map(self):
        msg = bt2.InactivityMessage()
        self.assertEqual(len(msg.clock_class_priority_map), 0)

    def test_create_with_cc_prio_map(self):
        msg = bt2.InactivityMessage(self._cc_prio_map)
        msg.clock_values.add(self._cc1(123))
        msg.clock_values.add(self._cc2(19487))
        self.assertEqual(len(msg.clock_class_priority_map), 2)
        self.assertEqual(msg.clock_class_priority_map, self._cc_prio_map)
        self.assertEqual(msg.clock_values[self._cc1], 123)
        self.assertEqual(msg.clock_values[self._cc2], 19487)

    def test_eq(self):
        msg = bt2.InactivityMessage(self._cc_prio_map)
        msg.clock_values.add(self._cc1(123))
        msg.clock_values.add(self._cc2(19487))
        cc_prio_map_copy = copy.copy(self._cc_prio_map)
        msg2 = bt2.InactivityMessage(cc_prio_map_copy)
        msg2.clock_values.add(self._cc1(123))
        msg2.clock_values.add(self._cc2(19487))
        self.assertEqual(msg, msg2)

    def test_ne_cc_prio_map(self):
        msg = bt2.InactivityMessage(self._cc_prio_map)
        msg.clock_values.add(self._cc1(123))
        msg.clock_values.add(self._cc2(19487))
        cc_prio_map_copy = copy.copy(self._cc_prio_map)
        cc_prio_map_copy[self._cc2] = 23
        msg2 = bt2.InactivityMessage(cc_prio_map_copy)
        self.assertNotEqual(msg, msg2)

    def test_ne_clock_value(self):
        msg = bt2.InactivityMessage(self._cc_prio_map)
        msg.clock_values.add(self._cc1(123))
        msg.clock_values.add(self._cc2(19487))
        msg2 = bt2.InactivityMessage(self._cc_prio_map)
        msg.clock_values.add(self._cc1(123))
        msg.clock_values.add(self._cc2(1847))
        self.assertNotEqual(msg, msg2)

    def test_eq_invalid(self):
        msg = bt2.InactivityMessage(self._cc_prio_map)
        self.assertNotEqual(msg, 23)

    def test_copy(self):
        msg = bt2.InactivityMessage(self._cc_prio_map)
        msg.clock_values.add(self._cc1(123))
        msg.clock_values.add(self._cc2(19487))
        msg_copy = copy.copy(msg)
        self.assertEqual(msg, msg_copy)
        self.assertNotEqual(msg.addr, msg_copy.addr)
        self.assertEqual(msg.clock_class_priority_map.addr,
                         msg_copy.clock_class_priority_map.addr)
        self.assertEqual(msg_copy.clock_values[self._cc1], 123)
        self.assertEqual(msg_copy.clock_values[self._cc2], 19487)

    def test_deepcopy(self):
        msg = bt2.InactivityMessage(self._cc_prio_map)
        msg.clock_values.add(self._cc1(123))
        msg.clock_values.add(self._cc2(19487))
        msg_copy = copy.deepcopy(msg)
        self.assertEqual(msg, msg_copy)
        self.assertNotEqual(msg.addr, msg_copy.addr)
        self.assertNotEqual(msg.clock_class_priority_map.addr,
                            msg_copy.clock_class_priority_map.addr)
        self.assertEqual(msg.clock_class_priority_map,
                         msg_copy.clock_class_priority_map)
        self.assertNotEqual(list(msg.clock_class_priority_map)[0].addr,
                            list(msg_copy.clock_class_priority_map)[0].addr)
        self.assertIsNone(msg_copy.clock_values[self._cc1])
        self.assertIsNone(msg_copy.clock_values[self._cc2])
        self.assertEqual(msg_copy.clock_values[list(msg_copy.clock_class_priority_map)[0]], 123)
        self.assertEqual(msg_copy.clock_values[list(msg_copy.clock_class_priority_map)[1]], 19487)


@unittest.skip("this is broken")
class DiscardedPacketsMessageTestCase(unittest.TestCase):
    def setUp(self):
        self._trace = bt2.Trace()
        self._sc = bt2.StreamClass()
        self._ec = bt2.EventClass('salut')
        self._clock_class = bt2.ClockClass('yo', 1000)
        self._uint64_int_fc = bt2.IntegerFieldClass(64, mapped_clock_class=self._clock_class)
        self._my_int_fc = bt2.IntegerFieldClass(32)
        self._ec.payload_field_class = bt2.StructureFieldClass()
        self._ec.payload_field_class += collections.OrderedDict([
            ('my_int', self._my_int_fc),
        ])
        self._sc.add_event_class(self._ec)
        self._sc.packet_context_field_class = bt2.StructureFieldClass()
        self._sc.packet_context_field_class += collections.OrderedDict([
            ('packet_seq_num', self._my_int_fc),
            ('timestamp_begin', self._uint64_int_fc),
            ('timestamp_end', self._uint64_int_fc),
        ])
        self._trace.add_clock_class(self._clock_class)
        self._trace.add_stream_class(self._sc)
        self._stream = self._sc()

    def tearDown(self):
        del self._trace
        del self._sc
        del self._ec
        del self._clock_class
        del self._uint64_int_fc
        del self._my_int_fc
        del self._stream

    def _create_event(self, packet):
        event = self._ec()
        event.payload_field['my_int'] = 23
        event.packet = packet
        return event

    def _get_msg(self):
        class MyIter(bt2._UserMessageIterator):
            def __init__(iter_self):
                packet1 = self._stream.create_packet()
                packet1.context_field['packet_seq_num'] = 0
                packet1.context_field['timestamp_begin'] = 3
                packet1.context_field['timestamp_end'] = 6
                packet2 = self._stream.create_packet()
                packet2.context_field['packet_seq_num'] = 5
                packet2.context_field['timestamp_begin'] = 7
                packet2.context_field['timestamp_end'] = 10
                iter_self._ev1 = self._create_event(packet1)
                iter_self._ev2 = self._create_event(packet2)
                iter_self._at = 0

            def __next__(self):
                if self._at == 0:
                    msg = bt2.EventMessage(self._ev1)
                elif self._at == 1:
                    msg = bt2.EventMessage(self._ev2)
                else:
                    raise bt2.Stop

                self._at += 1
                return msg

        class MySource(bt2._UserSourceComponent,
                       message_iterator_class=MyIter):
            def __init__(self, params):
                self._add_output_port('out')

        class MySink(bt2._UserSinkComponent):
            def __init__(self, params):
                self._add_input_port('in')

            def _consume(comp_self):
                nonlocal the_msg
                msg = next(comp_self._msg_iter)

                if type(msg) is bt2._DiscardedPacketsMessage:
                    the_msg = msg
                    raise bt2.Stop

            def _port_connected(self, port, other_port):
                self._msg_iter = port.connection.create_message_iterator()

        the_msg = None
        graph = bt2.Graph()
        src = graph.add_component(MySource, 'src')
        sink = graph.add_component(MySink, 'sink')
        conn = graph.connect_ports(src.output_ports['out'],
                                   sink.input_ports['in'])
        graph.run()
        return the_msg

    def test_create(self):
        self.assertIsInstance(self._get_msg(), bt2._DiscardedPacketsMessage)

    def test_count(self):
        self.assertEqual(self._get_msg().count, 4)

    def test_stream(self):
        self.assertEqual(self._get_msg().stream.addr, self._stream.addr)

    def test_beginning_clock_value(self):
        msg = self._get_msg()
        beginning_clock_value = msg.beginning_clock_value
        self.assertEqual(beginning_clock_value.clock_class, self._clock_class)
        self.assertEqual(beginning_clock_value, 6)

    def test_end_clock_value(self):
        msg = self._get_msg()
        end_clock_value = msg.end_clock_value
        self.assertEqual(end_clock_value.clock_class, self._clock_class)
        self.assertEqual(end_clock_value, 7)

    def test_eq(self):
        msg1 = self._get_msg()
        msg2 = self._get_msg()
        self.assertEqual(msg1, msg2)

    def test_eq_invalid(self):
        msg1 = self._get_msg()
        self.assertNotEqual(msg1, 23)


@unittest.skip("this is broken")
class DiscardedEventsMessageTestCase(unittest.TestCase):
    def setUp(self):
        self._trace = bt2.Trace()
        self._sc = bt2.StreamClass()
        self._ec = bt2.EventClass('salut')
        self._clock_class = bt2.ClockClass('yo', 1000)
        self._uint64_int_fc = bt2.IntegerFieldClass(64, mapped_clock_class=self._clock_class)
        self._my_int_fc = bt2.IntegerFieldClass(32)
        self._ec.payload_field_class = bt2.StructureFieldClass()
        self._ec.payload_field_class += collections.OrderedDict([
            ('my_int', self._my_int_fc),
        ])
        self._sc.add_event_class(self._ec)
        self._sc.packet_context_field_class = bt2.StructureFieldClass()
        self._sc.packet_context_field_class += collections.OrderedDict([
            ('events_discarded', self._my_int_fc),
            ('timestamp_begin', self._uint64_int_fc),
            ('timestamp_end', self._uint64_int_fc),
        ])
        self._trace.add_clock_class(self._clock_class)
        self._trace.add_stream_class(self._sc)
        self._stream = self._sc()

    def tearDown(self):
        del self._trace
        del self._sc
        del self._ec
        del self._clock_class
        del self._uint64_int_fc
        del self._my_int_fc
        del self._stream

    def _create_event(self, packet):
        event = self._ec()
        event.payload_field['my_int'] = 23
        event.packet = packet
        return event

    def _get_msg(self):
        class MyIter(bt2._UserMessageIterator):
            def __init__(iter_self):
                packet1 = self._stream.create_packet()
                packet1.context_field['events_discarded'] = 0
                packet1.context_field['timestamp_begin'] = 3
                packet1.context_field['timestamp_end'] = 6
                packet2 = self._stream.create_packet()
                packet2.context_field['events_discarded'] = 10
                packet2.context_field['timestamp_begin'] = 7
                packet2.context_field['timestamp_end'] = 10
                iter_self._ev1 = self._create_event(packet1)
                iter_self._ev2 = self._create_event(packet2)
                iter_self._at = 0

            def __next__(self):
                if self._at == 0:
                    msg = bt2.EventMessage(self._ev1)
                elif self._at == 1:
                    msg = bt2.EventMessage(self._ev2)
                else:
                    raise bt2.Stop

                self._at += 1
                return msg

        class MySource(bt2._UserSourceComponent,
                       message_iterator_class=MyIter):
            def __init__(self, params):
                self._add_output_port('out')

        class MySink(bt2._UserSinkComponent):
            def __init__(self, params):
                self._add_input_port('in')

            def _consume(comp_self):
                nonlocal the_msg
                msg = next(comp_self._msg_iter)

                if type(msg) is bt2._DiscardedEventsMessage:
                    the_msg = msg
                    raise bt2.Stop

            def _port_connected(self, port, other_port):
                self._msg_iter = port.connection.create_message_iterator()

        the_msg = None
        graph = bt2.Graph()
        src = graph.add_component(MySource, 'src')
        sink = graph.add_component(MySink, 'sink')
        conn = graph.connect_ports(src.output_ports['out'],
                                   sink.input_ports['in'])
        graph.run()
        return the_msg

    def test_create(self):
        self.assertIsInstance(self._get_msg(), bt2._DiscardedEventsMessage)

    def test_count(self):
        self.assertEqual(self._get_msg().count, 10)

    def test_stream(self):
        self.assertEqual(self._get_msg().stream.addr, self._stream.addr)

    def test_beginning_clock_value(self):
        msg = self._get_msg()
        beginning_clock_value = msg.beginning_clock_value
        self.assertEqual(beginning_clock_value.clock_class, self._clock_class)
        self.assertEqual(beginning_clock_value, 6)

    def test_end_clock_value(self):
        msg = self._get_msg()
        end_clock_value = msg.end_clock_value
        self.assertEqual(end_clock_value.clock_class, self._clock_class)
        self.assertEqual(end_clock_value, 10)

    def test_eq(self):
        msg1 = self._get_msg()
        msg2 = self._get_msg()
        self.assertEqual(msg1, msg2)

    def test_eq_invalid(self):
        msg1 = self._get_msg()
        self.assertNotEqual(msg1, 23)
