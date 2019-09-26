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
import utils
from utils import run_in_component_init
from bt2 import stream as bt2_stream
from bt2 import field as bt2_field


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
        sec += [
            ('cpu_id', tc.create_signed_integer_field_class(8)),
            ('stuff', tc.create_double_precision_real_field_class()),
        ]

        # packet context
        pc = None
        if with_pc:
            pc = tc.create_structure_field_class()
            pc += [
                ('something', tc.create_signed_integer_field_class(8)),
                ('something_else', tc.create_double_precision_real_field_class()),
                ('events_discarded', tc.create_unsigned_integer_field_class(64)),
                ('packet_seq_num', tc.create_unsigned_integer_field_class(64)),
            ]

        # stream class
        sc = tc.create_stream_class(
            default_clock_class=clock_class,
            event_common_context_field_class=sec,
            packet_context_field_class=pc,
            supports_packets=True,
        )

        # event context
        ec = tc.create_structure_field_class()
        ec += [
            ('ant', tc.create_signed_integer_field_class(16)),
            ('msg', tc.create_string_field_class()),
        ]

        # event payload
        ep = tc.create_structure_field_class()
        ep += [
            ('giraffe', tc.create_signed_integer_field_class(32)),
            ('gnu', tc.create_signed_integer_field_class(8)),
            ('mosquito', tc.create_signed_integer_field_class(8)),
        ]

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
        self.assertIs(type(packet.stream), bt2_stream._Stream)

    def test_const_attr_stream(self):
        packet = utils.get_const_packet_beginning_message().packet
        self.assertIs(type(packet.stream), bt2_stream._StreamConst)

    def test_context_field(self):
        packet, stream, pc_fc = self._create_packet(with_pc=True)
        self.assertEqual(packet.context_field.cls.addr, pc_fc.addr)
        self.assertIs(type(packet.context_field), bt2_field._StructureField)

    def test_const_context_field(self):
        packet = utils.get_const_packet_beginning_message().packet
        self.assertIs(type(packet.context_field), bt2_field._StructureFieldConst)

    def test_no_context_field(self):
        packet, _, _ = self._create_packet(with_pc=False)
        self.assertIsNone(packet.context_field)


if __name__ == '__main__':
    unittest.main()
