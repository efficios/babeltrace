from collections import OrderedDict
from bt2 import values
import unittest
import copy
import bt2


class EventTestCase(unittest.TestCase):
    def setUp(self):
        self._ec = self._create_ec()

    def tearDown(self):
        del self._ec

    def _create_ec(self, with_eh=True, with_sec=True, with_ec=True, with_ep=True):
        # event header
        if with_eh:
            eh = bt2.StructureFieldType()
            eh += OrderedDict((
                ('id', bt2.IntegerFieldType(8)),
                ('ts', bt2.IntegerFieldType(32)),
            ))
        else:
            eh = None

        # stream event context
        if with_sec:
            sec = bt2.StructureFieldType()
            sec += OrderedDict((
                ('cpu_id', bt2.IntegerFieldType(8)),
                ('stuff', bt2.FloatingPointNumberFieldType()),
            ))
        else:
            sec = None

        # packet context
        pc = bt2.StructureFieldType()
        pc += OrderedDict((
            ('something', bt2.IntegerFieldType(8)),
            ('something_else', bt2.FloatingPointNumberFieldType()),
        ))

        # stream class
        sc = bt2.StreamClass()
        sc.packet_context_field_type = pc
        sc.event_header_field_type = eh
        sc.event_context_field_type = sec

        # event context
        if with_ec:
            ec = bt2.StructureFieldType()
            ec += OrderedDict((
                ('ant', bt2.IntegerFieldType(16, is_signed=True)),
                ('msg', bt2.StringFieldType()),
            ))
        else:
            ec = None

        # event payload
        if with_ep:
            ep = bt2.StructureFieldType()
            ep += OrderedDict((
                ('giraffe', bt2.IntegerFieldType(32)),
                ('gnu', bt2.IntegerFieldType(8)),
                ('mosquito', bt2.IntegerFieldType(8)),
            ))
        else:
            ep = None

        # event class
        event_class = bt2.EventClass('ec')
        event_class.context_field_type = ec
        event_class.payload_field_type = ep
        sc.add_event_class(event_class)
        return event_class

    def test_attr_event_class(self):
        ev = self._ec()
        self.assertEqual(ev.event_class.addr, self._ec.addr)

    def test_attr_name(self):
        ev = self._ec()
        self.assertEqual(ev.name, self._ec.name)

    def test_attr_id(self):
        ev = self._ec()
        self.assertEqual(ev.id, self._ec.id)

    def test_get_event_header_field(self):
        ev = self._ec()
        ev.header_field['id'] = 23
        ev.header_field['ts'] = 1234
        self.assertEqual(ev.header_field['id'], 23)
        self.assertEqual(ev.header_field['ts'], 1234)

    def test_set_event_header_field(self):
        eh = self._ec.stream_class.event_header_field_type()
        eh['id'] = 17
        eh['ts'] = 188
        ev = self._ec()
        ev.header_field = eh
        self.assertEqual(ev.header_field['id'], 17)
        self.assertEqual(ev.header_field['ts'], 188)

    def test_get_stream_event_context_field(self):
        ev = self._ec()
        ev.stream_event_context_field['cpu_id'] = 1
        ev.stream_event_context_field['stuff'] = 13.194
        self.assertEqual(ev.stream_event_context_field['cpu_id'], 1)
        self.assertEqual(ev.stream_event_context_field['stuff'], 13.194)

    def test_set_stream_event_context_field(self):
        sec = self._ec.stream_class.event_context_field_type()
        sec['cpu_id'] = 2
        sec['stuff'] = 19.19
        ev = self._ec()
        ev.stream_event_context_field = sec
        self.assertEqual(ev.stream_event_context_field['cpu_id'], 2)
        self.assertEqual(ev.stream_event_context_field['stuff'], 19.19)

    def test_no_stream_event_context(self):
        ec = self._create_ec(with_sec=False)
        ev = ec()
        self.assertIsNone(ev.stream_event_context_field)

    def test_get_event_context_field(self):
        ev = self._ec()
        ev.context_field['ant'] = -1
        ev.context_field['msg'] = 'hellooo'
        self.assertEqual(ev.context_field['ant'], -1)
        self.assertEqual(ev.context_field['msg'], 'hellooo')

    def test_set_event_context_field(self):
        ec = self._ec.context_field_type()
        ec['ant'] = 2
        ec['msg'] = 'hi there'
        ev = self._ec()
        ev.context_field = ec
        self.assertEqual(ev.context_field['ant'], 2)
        self.assertEqual(ev.context_field['msg'], 'hi there')

    def test_no_event_context(self):
        ec = self._create_ec(with_ec=False)
        ev = ec()
        self.assertIsNone(ev.context_field)

    def test_get_event_payload_field(self):
        ev = self._ec()
        ev.payload_field['giraffe'] = 1
        ev.payload_field['gnu'] = 23
        ev.payload_field['mosquito'] = 42
        self.assertEqual(ev.payload_field['giraffe'], 1)
        self.assertEqual(ev.payload_field['gnu'], 23)
        self.assertEqual(ev.payload_field['mosquito'], 42)

    def test_set_event_payload_field(self):
        ep = self._ec.payload_field_type()
        ep['giraffe'] = 2
        ep['gnu'] = 124
        ep['mosquito'] = 17
        ev = self._ec()
        ev.payload_field = ep
        self.assertEqual(ev.payload_field['giraffe'], 2)
        self.assertEqual(ev.payload_field['gnu'], 124)
        self.assertEqual(ev.payload_field['mosquito'], 17)

    def test_clock_value(self):
        tc = bt2.Trace()
        tc.add_stream_class(self._ec.stream_class)
        cc = bt2.ClockClass('hi', 1000)
        tc.add_clock_class(cc)
        ev = self._ec()
        ev.clock_values.add(cc(177))
        self.assertEqual(ev.clock_values[cc].cycles, 177)

    def test_no_clock_value(self):
        tc = bt2.Trace()
        tc.add_stream_class(self._ec.stream_class)
        cc = bt2.ClockClass('hi', 1000)
        tc.add_clock_class(cc)
        ev = self._ec()
        self.assertIsNone(ev.clock_values[cc])

    def test_no_packet(self):
        ev = self._ec()
        self.assertIsNone(ev.packet)

    def test_packet(self):
        tc = bt2.Trace()
        tc.packet_header_field_type = bt2.StructureFieldType()
        tc.packet_header_field_type.append_field('magic', bt2.IntegerFieldType(32))
        tc.packet_header_field_type.append_field('stream_id', bt2.IntegerFieldType(16))
        tc.add_stream_class(self._ec.stream_class)
        ev = self._ec()
        self._fill_ev(ev)
        stream = self._ec.stream_class()
        packet = stream.create_packet()
        packet.header_field['magic'] = 0xc1fc1fc1
        packet.header_field['stream_id'] = 0
        packet.context_field['something'] = 154
        packet.context_field['something_else'] = 17.2
        ev.packet = packet
        self.assertEqual(ev.packet.addr, packet.addr)

    def test_no_stream(self):
        ev = self._ec()
        self.assertIsNone(ev.stream)

    def test_stream(self):
        tc = bt2.Trace()
        tc.packet_header_field_type = bt2.StructureFieldType()
        tc.packet_header_field_type.append_field('magic', bt2.IntegerFieldType(32))
        tc.packet_header_field_type.append_field('stream_id', bt2.IntegerFieldType(16))
        tc.add_stream_class(self._ec.stream_class)
        ev = self._ec()
        self._fill_ev(ev)
        stream = self._ec.stream_class()
        packet = stream.create_packet()
        packet.header_field['magic'] = 0xc1fc1fc1
        packet.header_field['stream_id'] = 0
        packet.context_field['something'] = 154
        packet.context_field['something_else'] = 17.2
        ev.packet = packet
        self.assertEqual(ev.stream.addr, stream.addr)

    def _fill_ev(self, ev):
        ev.header_field['id'] = 23
        ev.header_field['ts'] = 1234
        ev.stream_event_context_field['cpu_id'] = 1
        ev.stream_event_context_field['stuff'] = 13.194
        ev.context_field['ant'] = -1
        ev.context_field['msg'] = 'hellooo'
        ev.payload_field['giraffe'] = 1
        ev.payload_field['gnu'] = 23
        ev.payload_field['mosquito'] = 42

    def _get_full_ev(self):
        tc = bt2.Trace()
        tc.add_stream_class(self._ec.stream_class)
        cc = bt2.ClockClass('hi', 1000)
        tc.add_clock_class(cc)
        ev = self._ec()
        self._fill_ev(ev)
        ev.clock_values.add(cc(234))
        return ev

    def test_getitem(self):
        tc = bt2.Trace()
        tc.packet_header_field_type = bt2.StructureFieldType()
        tc.packet_header_field_type.append_field('magic', bt2.IntegerFieldType(32))
        tc.packet_header_field_type.append_field('stream_id', bt2.IntegerFieldType(16))
        tc.add_stream_class(self._ec.stream_class)
        ev = self._ec()
        self._fill_ev(ev)
        stream = self._ec.stream_class()
        packet = stream.create_packet()
        packet.header_field['magic'] = 0xc1fc1fc1
        packet.header_field['stream_id'] = 0
        packet.context_field['something'] = 154
        packet.context_field['something_else'] = 17.2

        with self.assertRaises(KeyError):
            ev['magic']

        ev.packet = packet
        self.assertEqual(ev['mosquito'], 42)
        self.assertEqual(ev['gnu'], 23)
        self.assertEqual(ev['giraffe'], 1)
        self.assertEqual(ev['msg'], 'hellooo')
        self.assertEqual(ev['ant'], -1)
        self.assertEqual(ev['stuff'], 13.194)
        self.assertEqual(ev['cpu_id'], 1)
        self.assertEqual(ev['ts'], 1234)
        self.assertEqual(ev['id'], 23)
        self.assertEqual(ev['something_else'], 17.2)
        self.assertEqual(ev['something'], 154)
        self.assertEqual(ev['stream_id'], 0)
        self.assertEqual(ev['magic'], 0xc1fc1fc1)

        with self.assertRaises(KeyError):
            ev['yes']

    def test_eq(self):
        tc = bt2.Trace()
        tc.add_stream_class(self._ec.stream_class)
        cc = bt2.ClockClass('hi', 1000)
        tc.add_clock_class(cc)
        ev1 = self._ec()
        self._fill_ev(ev1)
        ev1.clock_values.add(cc(234))
        ev2 = self._ec()
        self._fill_ev(ev2)
        ev2.clock_values.add(cc(234))
        self.assertEqual(ev1, ev2)

    def test_ne_header_field(self):
        tc = bt2.Trace()
        tc.add_stream_class(self._ec.stream_class)
        cc = bt2.ClockClass('hi', 1000)
        tc.add_clock_class(cc)
        ev1 = self._ec()
        self._fill_ev(ev1)
        ev1.header_field['id'] = 19
        ev1.clock_values.add(cc(234))
        ev2 = self._ec()
        self._fill_ev(ev2)
        ev2.clock_values.add(cc(234))
        self.assertNotEqual(ev1, ev2)

    def test_ne_stream_event_context_field(self):
        tc = bt2.Trace()
        tc.add_stream_class(self._ec.stream_class)
        cc = bt2.ClockClass('hi', 1000)
        tc.add_clock_class(cc)
        ev1 = self._ec()
        self._fill_ev(ev1)
        ev1.stream_event_context_field['cpu_id'] = 3
        ev1.clock_values.add(cc(234))
        ev2 = self._ec()
        self._fill_ev(ev2)
        ev2.clock_values.add(cc(234))
        self.assertNotEqual(ev1, ev2)

    def test_ne_context_field(self):
        tc = bt2.Trace()
        tc.add_stream_class(self._ec.stream_class)
        cc = bt2.ClockClass('hi', 1000)
        tc.add_clock_class(cc)
        ev1 = self._ec()
        self._fill_ev(ev1)
        ev1.context_field['ant'] = -3
        ev1.clock_values.add(cc(234))
        ev2 = self._ec()
        self._fill_ev(ev2)
        ev2.clock_values.add(cc(234))
        self.assertNotEqual(ev1, ev2)

    def test_ne_payload_field(self):
        tc = bt2.Trace()
        tc.add_stream_class(self._ec.stream_class)
        cc = bt2.ClockClass('hi', 1000)
        tc.add_clock_class(cc)
        ev1 = self._ec()
        self._fill_ev(ev1)
        ev1.payload_field['mosquito'] = 98
        ev1.clock_values.add(cc(234))
        ev2 = self._ec()
        self._fill_ev(ev2)
        ev2.clock_values.add(cc(234))
        self.assertNotEqual(ev1, ev2)

    def test_eq_invalid(self):
        ev = self._ec()
        self.assertFalse(ev == 23)

    def _test_copy(self, func):
        tc = bt2.Trace()
        tc.add_stream_class(self._ec.stream_class)
        cc = bt2.ClockClass('hi', 1000)
        tc.add_clock_class(cc)
        ev = self._ec()
        self._fill_ev(ev)
        ev.clock_values.add(cc(234))
        cpy = func(ev)
        self.assertIsNot(ev, cpy)
        self.assertNotEqual(ev.addr, cpy.addr)
        self.assertEqual(ev, cpy)

    def test_copy(self):
        self._test_copy(copy.copy)

    def test_deepcopy(self):
        self._test_copy(copy.deepcopy)
