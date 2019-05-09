from collections import OrderedDict
import unittest
from utils import run_in_component_init


class PacketTestCase(unittest.TestCase):
    @staticmethod
    def _create_packet(with_pc):
        def create_tc_cc(comp_self):
            cc = comp_self._create_clock_class(frequency=1000, name='my_cc')
            tc = comp_self._create_trace_class()
            return cc, tc

        clock_class, tc = run_in_component_init(create_tc_cc)

        # stream event context
        sec = tc.create_structure_field_class()
        sec += OrderedDict((
            ('cpu_id', tc.create_signed_integer_field_class(8)),
            ('stuff', tc.create_real_field_class()),
        ))

        # packet context
        pc = None
        if with_pc:
            pc = tc.create_structure_field_class()
            pc += OrderedDict((
                ('something', tc.create_signed_integer_field_class(8)),
                ('something_else', tc.create_real_field_class()),
                ('events_discarded', tc.create_unsigned_integer_field_class(64)),
                ('packet_seq_num', tc.create_unsigned_integer_field_class(64)),
            ))

        # stream class
        sc = tc.create_stream_class(default_clock_class=clock_class,
                                    event_common_context_field_class=sec,
                                    packet_context_field_class=pc)

        # event context
        ec = tc.create_structure_field_class()
        ec += OrderedDict((
            ('ant', tc.create_signed_integer_field_class(16)),
            ('msg', tc.create_string_field_class()),
        ))

        # event payload
        ep = tc.create_structure_field_class()
        ep += OrderedDict((
            ('giraffe', tc.create_signed_integer_field_class(32)),
            ('gnu', tc.create_signed_integer_field_class(8)),
            ('mosquito', tc.create_signed_integer_field_class(8)),
        ))

        # event class
        event_class = sc.create_event_class(name='ec', payload_field_class=ep)
        event_class.common_context_field_class = ec

        # trace
        trace = tc()

        # stream
        stream = trace.create_stream(sc)

        # packet
        return stream.create_packet(), stream, pc

    def test_attr_stream(self):
        packet, stream, _ = self._create_packet(with_pc=True)
        self.assertEqual(packet.stream.addr, stream.addr)

    def test_context_field(self):
        packet, stream, pc_fc = self._create_packet(with_pc=True)
        self.assertEqual(packet.context_field.field_class.addr, pc_fc.addr)

    def test_no_context_field(self):
        packet, _, _ = self._create_packet(with_pc=False)
        self.assertIsNone(packet.context_field)