from bt2 import values
import collections
import unittest
import copy
import bt2


class _NotificationTestCase(unittest.TestCase):
    def setUp(self):
        self._trace = bt2.Trace()
        self._sc = bt2.StreamClass()
        self._ec = bt2.EventClass('salut')
        self._my_int_ft = bt2.IntegerFieldType(32)
        self._ec.payload_field_type = bt2.StructureFieldType()
        self._ec.payload_field_type += collections.OrderedDict([
            ('my_int', self._my_int_ft),
        ])
        self._sc.add_event_class(self._ec)
        self._clock_class = bt2.ClockClass('allo', 1000)
        self._trace.add_clock_class(self._clock_class)
        self._trace.packet_header_field_type = bt2.StructureFieldType()
        self._trace.packet_header_field_type += collections.OrderedDict([
            ('hello', self._my_int_ft),
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
        del self._my_int_ft
        del self._clock_class
        del self._cc_prio_map
        del self._stream
        del self._packet
        del self._event


class EventNotificationTestCase(_NotificationTestCase):
    def test_create_no_cc_prio_map(self):
        notif = bt2.EventNotification(self._event)
        self.assertEqual(notif.event.addr, self._event.addr)
        self.assertEqual(len(notif.clock_class_priority_map), 0)

    def test_create_with_cc_prio_map(self):
        notif = bt2.EventNotification(self._event, self._cc_prio_map)
        self.assertEqual(notif.event.addr, self._event.addr)
        self.assertEqual(len(notif.clock_class_priority_map), 1)
        self.assertEqual(notif.clock_class_priority_map.highest_priority_clock_class.addr,
                         self._clock_class.addr)
        self.assertEqual(notif.clock_class_priority_map[self._clock_class], 231)

    def test_eq(self):
        notif = bt2.EventNotification(self._event, self._cc_prio_map)
        event_copy = copy.copy(self._event)
        event_copy.packet = self._packet
        cc_prio_map_copy = copy.copy(self._cc_prio_map)
        notif2 = bt2.EventNotification(event_copy, cc_prio_map_copy)
        self.assertEqual(notif, notif2)

    def test_ne_event(self):
        notif = bt2.EventNotification(self._event, self._cc_prio_map)
        event_copy = copy.copy(self._event)
        event_copy.payload_field['my_int'] = 17
        event_copy.packet = self._packet
        cc_prio_map_copy = copy.copy(self._cc_prio_map)
        notif2 = bt2.EventNotification(event_copy, cc_prio_map_copy)
        self.assertNotEqual(notif, notif2)

    def test_ne_cc_prio_map(self):
        notif = bt2.EventNotification(self._event)
        event_copy = copy.copy(self._event)
        event_copy.packet = self._packet
        cc_prio_map_copy = copy.copy(self._cc_prio_map)
        notif2 = bt2.EventNotification(event_copy, cc_prio_map_copy)
        self.assertNotEqual(notif, notif2)

    def test_eq_invalid(self):
        notif = bt2.EventNotification(self._event)
        self.assertNotEqual(notif, 23)

    def test_copy(self):
        notif = bt2.EventNotification(self._event, self._cc_prio_map)
        notif2 = copy.copy(notif)
        self.assertEqual(notif, notif2)

    def test_deepcopy(self):
        notif = bt2.EventNotification(self._event, self._cc_prio_map)
        notif2 = copy.deepcopy(notif)
        self.assertEqual(notif, notif2)


class PacketBeginningNotificationTestCase(_NotificationTestCase):
    def test_create(self):
        notif = bt2.PacketBeginningNotification(self._packet)
        self.assertEqual(notif.packet.addr, self._packet.addr)

    def test_eq(self):
        notif = bt2.PacketBeginningNotification(self._packet)
        packet_copy = copy.copy(self._packet)
        notif2 = bt2.PacketBeginningNotification(packet_copy)
        self.assertEqual(notif, notif2)

    def test_ne_packet(self):
        notif = bt2.PacketBeginningNotification(self._packet)
        packet_copy = copy.copy(self._packet)
        packet_copy.header_field['hello'] = 1847
        notif2 = bt2.PacketBeginningNotification(packet_copy)
        self.assertNotEqual(notif, notif2)

    def test_eq_invalid(self):
        notif = bt2.PacketBeginningNotification(self._packet)
        self.assertNotEqual(notif, 23)

    def test_copy(self):
        notif = bt2.PacketBeginningNotification(self._packet)
        notif2 = copy.copy(notif)
        self.assertEqual(notif, notif2)

    def test_deepcopy(self):
        notif = bt2.PacketBeginningNotification(self._packet)
        notif2 = copy.deepcopy(notif)
        self.assertEqual(notif, notif2)


class PacketEndNotificationTestCase(_NotificationTestCase):
    def test_create(self):
        notif = bt2.PacketEndNotification(self._packet)
        self.assertEqual(notif.packet.addr, self._packet.addr)

    def test_eq(self):
        notif = bt2.PacketEndNotification(self._packet)
        packet_copy = copy.copy(self._packet)
        notif2 = bt2.PacketEndNotification(packet_copy)
        self.assertEqual(notif, notif2)

    def test_ne_packet(self):
        notif = bt2.PacketEndNotification(self._packet)
        packet_copy = copy.copy(self._packet)
        packet_copy.header_field['hello'] = 1847
        notif2 = bt2.PacketEndNotification(packet_copy)
        self.assertNotEqual(notif, notif2)

    def test_eq_invalid(self):
        notif = bt2.PacketEndNotification(self._packet)
        self.assertNotEqual(notif, 23)

    def test_copy(self):
        notif = bt2.PacketEndNotification(self._packet)
        notif2 = copy.copy(notif)
        self.assertEqual(notif, notif2)

    def test_deepcopy(self):
        notif = bt2.PacketEndNotification(self._packet)
        notif2 = copy.deepcopy(notif)
        self.assertEqual(notif, notif2)


class StreamBeginningNotificationTestCase(_NotificationTestCase):
    def test_create(self):
        notif = bt2.StreamBeginningNotification(self._stream)
        self.assertEqual(notif.stream.addr, self._stream.addr)

    def test_eq(self):
        notif = bt2.StreamBeginningNotification(self._stream)
        stream_copy = copy.copy(self._stream)
        notif2 = bt2.StreamBeginningNotification(stream_copy)
        self.assertEqual(notif, notif2)

    def test_ne_stream(self):
        notif = bt2.StreamBeginningNotification(self._stream)
        stream_copy = self._sc(name='salut')
        notif2 = bt2.StreamBeginningNotification(stream_copy)
        self.assertNotEqual(notif, notif2)

    def test_eq_invalid(self):
        notif = bt2.StreamBeginningNotification(self._stream)
        self.assertNotEqual(notif, 23)

    def test_copy(self):
        notif = bt2.StreamBeginningNotification(self._stream)
        notif2 = copy.copy(notif)
        self.assertEqual(notif, notif2)

    def test_deepcopy(self):
        notif = bt2.StreamBeginningNotification(self._stream)
        notif2 = copy.deepcopy(notif)
        self.assertEqual(notif, notif2)


class StreamEndNotificationTestCase(_NotificationTestCase):
    def test_create(self):
        notif = bt2.StreamEndNotification(self._stream)
        self.assertEqual(notif.stream.addr, self._stream.addr)

    def test_eq(self):
        notif = bt2.StreamEndNotification(self._stream)
        stream_copy = copy.copy(self._stream)
        notif2 = bt2.StreamEndNotification(stream_copy)
        self.assertEqual(notif, notif2)

    def test_ne_stream(self):
        notif = bt2.StreamEndNotification(self._stream)
        stream_copy = self._sc(name='salut')
        notif2 = bt2.StreamEndNotification(stream_copy)
        self.assertNotEqual(notif, notif2)

    def test_eq_invalid(self):
        notif = bt2.StreamEndNotification(self._stream)
        self.assertNotEqual(notif, 23)

    def test_copy(self):
        notif = bt2.StreamEndNotification(self._stream)
        notif2 = copy.copy(notif)
        self.assertEqual(notif, notif2)

    def test_deepcopy(self):
        notif = bt2.StreamEndNotification(self._stream)
        notif2 = copy.deepcopy(notif)
        self.assertEqual(notif, notif2)


class InactivityNotificationTestCase(unittest.TestCase):
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
        notif = bt2.InactivityNotification()
        self.assertEqual(len(notif.clock_class_priority_map), 0)

    def test_create_with_cc_prio_map(self):
        notif = bt2.InactivityNotification(self._cc_prio_map)
        notif.clock_values.add(self._cc1(123))
        notif.clock_values.add(self._cc2(19487))
        self.assertEqual(len(notif.clock_class_priority_map), 2)
        self.assertEqual(notif.clock_class_priority_map, self._cc_prio_map)
        self.assertEqual(notif.clock_values[self._cc1], 123)
        self.assertEqual(notif.clock_values[self._cc2], 19487)

    def test_eq(self):
        notif = bt2.InactivityNotification(self._cc_prio_map)
        notif.clock_values.add(self._cc1(123))
        notif.clock_values.add(self._cc2(19487))
        cc_prio_map_copy = copy.copy(self._cc_prio_map)
        notif2 = bt2.InactivityNotification(cc_prio_map_copy)
        notif2.clock_values.add(self._cc1(123))
        notif2.clock_values.add(self._cc2(19487))
        self.assertEqual(notif, notif2)

    def test_ne_cc_prio_map(self):
        notif = bt2.InactivityNotification(self._cc_prio_map)
        notif.clock_values.add(self._cc1(123))
        notif.clock_values.add(self._cc2(19487))
        cc_prio_map_copy = copy.copy(self._cc_prio_map)
        cc_prio_map_copy[self._cc2] = 23
        notif2 = bt2.InactivityNotification(cc_prio_map_copy)
        self.assertNotEqual(notif, notif2)

    def test_ne_clock_value(self):
        notif = bt2.InactivityNotification(self._cc_prio_map)
        notif.clock_values.add(self._cc1(123))
        notif.clock_values.add(self._cc2(19487))
        notif2 = bt2.InactivityNotification(self._cc_prio_map)
        notif.clock_values.add(self._cc1(123))
        notif.clock_values.add(self._cc2(1847))
        self.assertNotEqual(notif, notif2)

    def test_eq_invalid(self):
        notif = bt2.InactivityNotification(self._cc_prio_map)
        self.assertNotEqual(notif, 23)

    def test_copy(self):
        notif = bt2.InactivityNotification(self._cc_prio_map)
        notif.clock_values.add(self._cc1(123))
        notif.clock_values.add(self._cc2(19487))
        notif_copy = copy.copy(notif)
        self.assertEqual(notif, notif_copy)
        self.assertNotEqual(notif.addr, notif_copy.addr)
        self.assertEqual(notif.clock_class_priority_map.addr,
                         notif_copy.clock_class_priority_map.addr)
        self.assertEqual(notif_copy.clock_values[self._cc1], 123)
        self.assertEqual(notif_copy.clock_values[self._cc2], 19487)

    def test_deepcopy(self):
        notif = bt2.InactivityNotification(self._cc_prio_map)
        notif.clock_values.add(self._cc1(123))
        notif.clock_values.add(self._cc2(19487))
        notif_copy = copy.deepcopy(notif)
        self.assertEqual(notif, notif_copy)
        self.assertNotEqual(notif.addr, notif_copy.addr)
        self.assertNotEqual(notif.clock_class_priority_map.addr,
                            notif_copy.clock_class_priority_map.addr)
        self.assertEqual(notif.clock_class_priority_map,
                         notif_copy.clock_class_priority_map)
        self.assertNotEqual(list(notif.clock_class_priority_map)[0].addr,
                            list(notif_copy.clock_class_priority_map)[0].addr)
        self.assertIsNone(notif_copy.clock_values[self._cc1])
        self.assertIsNone(notif_copy.clock_values[self._cc2])
        self.assertEqual(notif_copy.clock_values[list(notif_copy.clock_class_priority_map)[0]], 123)
        self.assertEqual(notif_copy.clock_values[list(notif_copy.clock_class_priority_map)[1]], 19487)


class DiscardedPacketsNotificationTestCase(unittest.TestCase):
    def setUp(self):
        self._trace = bt2.Trace()
        self._sc = bt2.StreamClass()
        self._ec = bt2.EventClass('salut')
        self._clock_class = bt2.ClockClass('yo', 1000)
        self._uint64_int_ft = bt2.IntegerFieldType(64, mapped_clock_class=self._clock_class)
        self._my_int_ft = bt2.IntegerFieldType(32)
        self._ec.payload_field_type = bt2.StructureFieldType()
        self._ec.payload_field_type += collections.OrderedDict([
            ('my_int', self._my_int_ft),
        ])
        self._sc.add_event_class(self._ec)
        self._sc.packet_context_field_type = bt2.StructureFieldType()
        self._sc.packet_context_field_type += collections.OrderedDict([
            ('packet_seq_num', self._my_int_ft),
            ('timestamp_begin', self._uint64_int_ft),
            ('timestamp_end', self._uint64_int_ft),
        ])
        self._trace.add_clock_class(self._clock_class)
        self._trace.add_stream_class(self._sc)
        self._stream = self._sc()

    def tearDown(self):
        del self._trace
        del self._sc
        del self._ec
        del self._clock_class
        del self._uint64_int_ft
        del self._my_int_ft
        del self._stream

    def _create_event(self, packet):
        event = self._ec()
        event.payload_field['my_int'] = 23
        event.packet = packet
        return event

    def _get_notif(self):
        class MyIter(bt2._UserNotificationIterator):
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
                    notif = bt2.EventNotification(self._ev1)
                elif self._at == 1:
                    notif = bt2.EventNotification(self._ev2)
                else:
                    raise bt2.Stop

                self._at += 1
                return notif

        class MySource(bt2._UserSourceComponent,
                       notification_iterator_class=MyIter):
            def __init__(self, params):
                self._add_output_port('out')

        class MySink(bt2._UserSinkComponent):
            def __init__(self, params):
                self._add_input_port('in')

            def _consume(comp_self):
                nonlocal the_notif
                notif = next(comp_self._notif_iter)

                if type(notif) is bt2._DiscardedPacketsNotification:
                    the_notif = notif
                    raise bt2.Stop

            def _port_connected(self, port, other_port):
                self._notif_iter = port.connection.create_notification_iterator()

        the_notif = None
        graph = bt2.Graph()
        src = graph.add_component(MySource, 'src')
        sink = graph.add_component(MySink, 'sink')
        conn = graph.connect_ports(src.output_ports['out'],
                                   sink.input_ports['in'])
        graph.run()
        return the_notif

    def test_create(self):
        self.assertIsInstance(self._get_notif(), bt2._DiscardedPacketsNotification)

    def test_count(self):
        self.assertEqual(self._get_notif().count, 4)

    def test_stream(self):
        self.assertEqual(self._get_notif().stream.addr, self._stream.addr)

    def test_beginning_clock_value(self):
        notif = self._get_notif()
        beginning_clock_value = notif.beginning_clock_value
        self.assertEqual(beginning_clock_value.clock_class, self._clock_class)
        self.assertEqual(beginning_clock_value, 6)

    def test_end_clock_value(self):
        notif = self._get_notif()
        end_clock_value = notif.end_clock_value
        self.assertEqual(end_clock_value.clock_class, self._clock_class)
        self.assertEqual(end_clock_value, 7)

    def test_eq(self):
        notif1 = self._get_notif()
        notif2 = self._get_notif()
        self.assertEqual(notif1, notif2)

    def test_eq_invalid(self):
        notif1 = self._get_notif()
        self.assertNotEqual(notif1, 23)


class DiscardedEventsNotificationTestCase(unittest.TestCase):
    def setUp(self):
        self._trace = bt2.Trace()
        self._sc = bt2.StreamClass()
        self._ec = bt2.EventClass('salut')
        self._clock_class = bt2.ClockClass('yo', 1000)
        self._uint64_int_ft = bt2.IntegerFieldType(64, mapped_clock_class=self._clock_class)
        self._my_int_ft = bt2.IntegerFieldType(32)
        self._ec.payload_field_type = bt2.StructureFieldType()
        self._ec.payload_field_type += collections.OrderedDict([
            ('my_int', self._my_int_ft),
        ])
        self._sc.add_event_class(self._ec)
        self._sc.packet_context_field_type = bt2.StructureFieldType()
        self._sc.packet_context_field_type += collections.OrderedDict([
            ('events_discarded', self._my_int_ft),
            ('timestamp_begin', self._uint64_int_ft),
            ('timestamp_end', self._uint64_int_ft),
        ])
        self._trace.add_clock_class(self._clock_class)
        self._trace.add_stream_class(self._sc)
        self._stream = self._sc()

    def tearDown(self):
        del self._trace
        del self._sc
        del self._ec
        del self._clock_class
        del self._uint64_int_ft
        del self._my_int_ft
        del self._stream

    def _create_event(self, packet):
        event = self._ec()
        event.payload_field['my_int'] = 23
        event.packet = packet
        return event

    def _get_notif(self):
        class MyIter(bt2._UserNotificationIterator):
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
                    notif = bt2.EventNotification(self._ev1)
                elif self._at == 1:
                    notif = bt2.EventNotification(self._ev2)
                else:
                    raise bt2.Stop

                self._at += 1
                return notif

        class MySource(bt2._UserSourceComponent,
                       notification_iterator_class=MyIter):
            def __init__(self, params):
                self._add_output_port('out')

        class MySink(bt2._UserSinkComponent):
            def __init__(self, params):
                self._add_input_port('in')

            def _consume(comp_self):
                nonlocal the_notif
                notif = next(comp_self._notif_iter)

                if type(notif) is bt2._DiscardedEventsNotification:
                    the_notif = notif
                    raise bt2.Stop

            def _port_connected(self, port, other_port):
                self._notif_iter = port.connection.create_notification_iterator()

        the_notif = None
        graph = bt2.Graph()
        src = graph.add_component(MySource, 'src')
        sink = graph.add_component(MySink, 'sink')
        conn = graph.connect_ports(src.output_ports['out'],
                                   sink.input_ports['in'])
        graph.run()
        return the_notif

    def test_create(self):
        self.assertIsInstance(self._get_notif(), bt2._DiscardedEventsNotification)

    def test_count(self):
        self.assertEqual(self._get_notif().count, 10)

    def test_stream(self):
        self.assertEqual(self._get_notif().stream.addr, self._stream.addr)

    def test_beginning_clock_value(self):
        notif = self._get_notif()
        beginning_clock_value = notif.beginning_clock_value
        self.assertEqual(beginning_clock_value.clock_class, self._clock_class)
        self.assertEqual(beginning_clock_value, 6)

    def test_end_clock_value(self):
        notif = self._get_notif()
        end_clock_value = notif.end_clock_value
        self.assertEqual(end_clock_value.clock_class, self._clock_class)
        self.assertEqual(end_clock_value, 10)

    def test_eq(self):
        notif1 = self._get_notif()
        notif2 = self._get_notif()
        self.assertEqual(notif1, notif2)

    def test_eq_invalid(self):
        notif1 = self._get_notif()
        self.assertNotEqual(notif1, 23)
