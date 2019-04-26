from collections import OrderedDict
from bt2 import value
import unittest
import copy
import bt2


@unittest.skip("this is broken")
class PacketTestCase(unittest.TestCase):
    def setUp(self):
        self._packet = self._create_packet()

    def tearDown(self):
        del self._packet

    def _create_packet(self, with_ph=True, with_pc=True):
        # event header
        eh = bt2.StructureFieldClass()
        eh += OrderedDict((
            ('id', bt2.IntegerFieldClass(8)),
            ('ts', bt2.IntegerFieldClass(32)),
        ))

        # stream event context
        sec = bt2.StructureFieldClass()
        sec += OrderedDict((
            ('cpu_id', bt2.IntegerFieldClass(8)),
            ('stuff', bt2.FloatingPointNumberFieldClass()),
        ))

        # packet context
        if with_pc:
            pc = bt2.StructureFieldClass()
            pc += OrderedDict((
                ('something', bt2.IntegerFieldClass(8)),
                ('something_else', bt2.FloatingPointNumberFieldClass()),
            ))
        else:
            pc = None

        # stream class
        sc = bt2.StreamClass()
        sc.packet_context_field_class = pc
        sc.event_header_field_class = eh
        sc.event_context_field_class = sec

        # event context
        ec = bt2.StructureFieldClass()
        ec += OrderedDict((
            ('ant', bt2.IntegerFieldClass(16, is_signed=True)),
            ('msg', bt2.StringFieldClass()),
        ))

        # event payload
        ep = bt2.StructureFieldClass()
        ep += OrderedDict((
            ('giraffe', bt2.IntegerFieldClass(32)),
            ('gnu', bt2.IntegerFieldClass(8)),
            ('mosquito', bt2.IntegerFieldClass(8)),
        ))

        # event class
        event_class = bt2.EventClass('ec')
        event_class.context_field_class = ec
        event_class.payload_field_class = ep
        sc.add_event_class(event_class)

        # packet header
        if with_ph:
            ph = bt2.StructureFieldClass()
            ph += OrderedDict((
                ('magic', bt2.IntegerFieldClass(32)),
                ('stream_id', bt2.IntegerFieldClass(16)),
            ))
        else:
            ph = None

        # trace c;ass
        tc = bt2.Trace()
        tc.packet_header_field_class = ph
        tc.add_stream_class(sc)

        # stream
        stream = sc()

        # packet
        return stream.create_packet()

    def test_attr_stream(self):
        self.assertIsNotNone(self._packet.stream)

    def test_get_header_field(self):
        self.assertIsNotNone(self._packet.header_field)

    def test_no_header_field(self):
        packet = self._create_packet(with_ph=False)
        self.assertIsNone(packet.header_field)

    def test_get_context_field(self):
        self.assertIsNotNone(self._packet.context_field)

    def test_no_context_field(self):
        packet = self._create_packet(with_pc=False)
        self.assertIsNone(packet.context_field)

    def _fill_packet(self, packet):
        packet.header_field['magic'] = 0xc1fc1fc1
        packet.header_field['stream_id'] = 23
        packet.context_field['something'] = 17
        packet.context_field['something_else'] = 188.88

    def test_eq(self):
        packet1 = self._create_packet()
        self._fill_packet(packet1)
        packet2 = self._create_packet()
        self._fill_packet(packet2)
        self.assertEqual(packet1, packet2)

    def test_ne_header_field(self):
        packet1 = self._create_packet()
        self._fill_packet(packet1)
        packet2 = self._create_packet()
        self._fill_packet(packet2)
        packet2.header_field['stream_id'] = 18
        self.assertNotEqual(packet1, packet2)

    def test_ne_context_field(self):
        packet1 = self._create_packet()
        self._fill_packet(packet1)
        packet2 = self._create_packet()
        self._fill_packet(packet2)
        packet2.context_field['something_else'] = 1948.11
        self.assertNotEqual(packet1, packet2)

    def test_eq_invalid(self):
        self.assertFalse(self._packet == 23)

    def _test_copy(self, func):
        packet = self._create_packet()
        self._fill_packet(packet)
        cpy = func(packet)
        self.assertIsNot(packet, cpy)
        self.assertNotEqual(packet.addr, cpy.addr)
        self.assertEqual(packet, cpy)

    def test_copy(self):
        self._test_copy(copy.copy)

    def test_deepcopy(self):
        self._test_copy(copy.deepcopy)
