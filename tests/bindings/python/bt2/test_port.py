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
from bt2 import port as bt2_port


class PortTestCase(unittest.TestCase):
    @staticmethod
    def _create_comp(comp_cls, name=None):
        graph = bt2.Graph()

        if name is None:
            name = 'comp'

        return graph.add_component(comp_cls, name)

    def test_src_add_output_port(self):
        class MySource(
            bt2._UserSourceComponent, message_iterator_class=bt2._UserMessageIterator
        ):
            def __init__(comp_self, config, params, obj):
                port = comp_self._add_output_port('out')
                self.assertEqual(port.name, 'out')

        comp = self._create_comp(MySource)
        self.assertEqual(len(comp.output_ports), 1)
        self.assertIs(type(comp.output_ports['out']), bt2_port._OutputPortConst)

    def test_flt_add_output_port(self):
        class MyFilter(
            bt2._UserFilterComponent, message_iterator_class=bt2._UserMessageIterator
        ):
            def __init__(comp_self, config, params, obj):
                port = comp_self._add_output_port('out')
                self.assertEqual(port.name, 'out')

        comp = self._create_comp(MyFilter)
        self.assertEqual(len(comp.output_ports), 1)

    def test_flt_add_input_port(self):
        class MyFilter(
            bt2._UserFilterComponent, message_iterator_class=bt2._UserMessageIterator
        ):
            def __init__(comp_self, config, params, obj):
                port = comp_self._add_input_port('in')
                self.assertEqual(port.name, 'in')

        comp = self._create_comp(MyFilter)
        self.assertEqual(len(comp.input_ports), 1)
        self.assertIs(type(comp.input_ports['in']), bt2_port._InputPortConst)

    def test_sink_add_input_port(self):
        class MySink(bt2._UserSinkComponent):
            def __init__(comp_self, config, params, obj):
                port = comp_self._add_input_port('in')
                self.assertEqual(port.name, 'in')

            def _user_consume(self):
                pass

        comp = self._create_comp(MySink)
        self.assertEqual(len(comp.input_ports), 1)

    def test_user_src_output_ports_getitem(self):
        class MySource(
            bt2._UserSourceComponent, message_iterator_class=bt2._UserMessageIterator
        ):
            def __init__(comp_self, config, params, obj):
                port1 = comp_self._add_output_port('clear')
                port2 = comp_self._add_output_port('print')
                port3 = comp_self._add_output_port('insert')
                self.assertEqual(port3.addr, comp_self._output_ports['insert'].addr)
                self.assertEqual(port2.addr, comp_self._output_ports['print'].addr)
                self.assertEqual(port1.addr, comp_self._output_ports['clear'].addr)

        self._create_comp(MySource)

    def test_user_flt_output_ports_getitem(self):
        class MyFilter(
            bt2._UserFilterComponent, message_iterator_class=bt2._UserMessageIterator
        ):
            def __init__(comp_self, config, params, obj):
                port1 = comp_self._add_output_port('clear')
                port2 = comp_self._add_output_port('print')
                port3 = comp_self._add_output_port('insert')
                self.assertEqual(port3.addr, comp_self._output_ports['insert'].addr)
                self.assertEqual(port2.addr, comp_self._output_ports['print'].addr)
                self.assertEqual(port1.addr, comp_self._output_ports['clear'].addr)

        self._create_comp(MyFilter)

    def test_user_flt_input_ports_getitem(self):
        class MyFilter(
            bt2._UserFilterComponent, message_iterator_class=bt2._UserMessageIterator
        ):
            def __init__(comp_self, config, params, obj):
                port1 = comp_self._add_input_port('clear')
                port2 = comp_self._add_input_port('print')
                port3 = comp_self._add_input_port('insert')
                self.assertEqual(port3.addr, comp_self._input_ports['insert'].addr)
                self.assertEqual(port2.addr, comp_self._input_ports['print'].addr)
                self.assertEqual(port1.addr, comp_self._input_ports['clear'].addr)

        self._create_comp(MyFilter)

    def test_user_sink_input_ports_getitem(self):
        class MySink(bt2._UserSinkComponent):
            def __init__(comp_self, config, params, obj):
                port1 = comp_self._add_input_port('clear')
                port2 = comp_self._add_input_port('print')
                port3 = comp_self._add_input_port('insert')
                self.assertEqual(port3.addr, comp_self._input_ports['insert'].addr)
                self.assertEqual(port2.addr, comp_self._input_ports['print'].addr)
                self.assertEqual(port1.addr, comp_self._input_ports['clear'].addr)

            def _user_consume(self):
                pass

        self._create_comp(MySink)

    def test_user_src_output_ports_getitem_invalid_key(self):
        class MySource(
            bt2._UserSourceComponent, message_iterator_class=bt2._UserMessageIterator
        ):
            def __init__(comp_self, config, params, obj):
                comp_self._add_output_port('clear')
                comp_self._add_output_port('print')
                comp_self._add_output_port('insert')

                with self.assertRaises(KeyError):
                    comp_self._output_ports['hello']

        self._create_comp(MySource)

    def test_user_flt_output_ports_getitem_invalid_key(self):
        class MyFilter(
            bt2._UserFilterComponent, message_iterator_class=bt2._UserMessageIterator
        ):
            def __init__(comp_self, config, params, obj):
                comp_self._add_output_port('clear')
                comp_self._add_output_port('print')
                comp_self._add_output_port('insert')

                with self.assertRaises(KeyError):
                    comp_self._output_ports['hello']

        self._create_comp(MyFilter)

    def test_user_flt_input_ports_getitem_invalid_key(self):
        class MyFilter(
            bt2._UserFilterComponent, message_iterator_class=bt2._UserMessageIterator
        ):
            def __init__(comp_self, config, params, obj):
                comp_self._add_input_port('clear')
                comp_self._add_input_port('print')
                comp_self._add_input_port('insert')

                with self.assertRaises(KeyError):
                    comp_self._input_ports['hello']

        self._create_comp(MyFilter)

    def test_user_sink_input_ports_getitem_invalid_key(self):
        class MySink(bt2._UserSinkComponent):
            def __init__(comp_self, config, params, obj):
                comp_self._add_input_port('clear')
                comp_self._add_input_port('print')
                comp_self._add_input_port('insert')

                with self.assertRaises(KeyError):
                    comp_self._input_ports['hello']

            def _user_consume(self):
                pass

        self._create_comp(MySink)

    def test_user_src_output_ports_len(self):
        class MySource(
            bt2._UserSourceComponent, message_iterator_class=bt2._UserMessageIterator
        ):
            def __init__(comp_self, config, params, obj):
                comp_self._add_output_port('clear')
                comp_self._add_output_port('print')
                comp_self._add_output_port('insert')
                self.assertEqual(len(comp_self._output_ports), 3)

        self._create_comp(MySource)

    def test_user_flt_output_ports_len(self):
        class MyFilter(
            bt2._UserFilterComponent, message_iterator_class=bt2._UserMessageIterator
        ):
            def __init__(comp_self, config, params, obj):
                comp_self._add_output_port('clear')
                comp_self._add_output_port('print')
                comp_self._add_output_port('insert')
                self.assertEqual(len(comp_self._output_ports), 3)

        self._create_comp(MyFilter)

    def test_user_flt_input_ports_len(self):
        class MyFilter(
            bt2._UserFilterComponent, message_iterator_class=bt2._UserMessageIterator
        ):
            def __init__(comp_self, config, params, obj):
                comp_self._add_input_port('clear')
                comp_self._add_input_port('print')
                comp_self._add_input_port('insert')
                self.assertEqual(len(comp_self._input_ports), 3)

        self._create_comp(MyFilter)

    def test_user_sink_input_ports_len(self):
        class MySink(bt2._UserSinkComponent):
            def __init__(comp_self, config, params, obj):
                comp_self._add_input_port('clear')
                comp_self._add_input_port('print')
                comp_self._add_input_port('insert')
                self.assertEqual(len(comp_self._input_ports), 3)

            def _user_consume(self):
                pass

        self._create_comp(MySink)

    def test_user_src_output_ports_iter(self):
        class MySource(
            bt2._UserSourceComponent, message_iterator_class=bt2._UserMessageIterator
        ):
            def __init__(comp_self, config, params, obj):
                port1 = comp_self._add_output_port('clear')
                port2 = comp_self._add_output_port('print')
                port3 = comp_self._add_output_port('insert')
                ports = []

                for port_name, port in comp_self._output_ports.items():
                    ports.append((port_name, port))

                self.assertEqual(ports[0][0], 'clear')
                self.assertEqual(ports[0][1].addr, port1.addr)
                self.assertEqual(ports[1][0], 'print')
                self.assertEqual(ports[1][1].addr, port2.addr)
                self.assertEqual(ports[2][0], 'insert')
                self.assertEqual(ports[2][1].addr, port3.addr)

        self._create_comp(MySource)

    def test_user_flt_output_ports_iter(self):
        class MyFilter(
            bt2._UserFilterComponent, message_iterator_class=bt2._UserMessageIterator
        ):
            def __init__(comp_self, config, params, obj):
                port1 = comp_self._add_output_port('clear')
                port2 = comp_self._add_output_port('print')
                port3 = comp_self._add_output_port('insert')
                ports = []

                for port_name, port in comp_self._output_ports.items():
                    ports.append((port_name, port))

                self.assertEqual(ports[0][0], 'clear')
                self.assertEqual(ports[0][1].addr, port1.addr)
                self.assertEqual(ports[1][0], 'print')
                self.assertEqual(ports[1][1].addr, port2.addr)
                self.assertEqual(ports[2][0], 'insert')
                self.assertEqual(ports[2][1].addr, port3.addr)

        self._create_comp(MyFilter)

    def test_user_flt_input_ports_iter(self):
        class MyFilter(
            bt2._UserFilterComponent, message_iterator_class=bt2._UserMessageIterator
        ):
            def __init__(comp_self, config, params, obj):
                port1 = comp_self._add_input_port('clear')
                port2 = comp_self._add_input_port('print')
                port3 = comp_self._add_input_port('insert')
                ports = []

                for port_name, port in comp_self._input_ports.items():
                    ports.append((port_name, port))

                self.assertEqual(ports[0][0], 'clear')
                self.assertEqual(ports[0][1].addr, port1.addr)
                self.assertEqual(ports[1][0], 'print')
                self.assertEqual(ports[1][1].addr, port2.addr)
                self.assertEqual(ports[2][0], 'insert')
                self.assertEqual(ports[2][1].addr, port3.addr)

        self._create_comp(MyFilter)

    def test_user_sink_input_ports_iter(self):
        class MySink(bt2._UserSinkComponent):
            def __init__(comp_self, config, params, obj):
                port1 = comp_self._add_input_port('clear')
                port2 = comp_self._add_input_port('print')
                port3 = comp_self._add_input_port('insert')
                ports = []

                for port_name, port in comp_self._input_ports.items():
                    ports.append((port_name, port))

                self.assertEqual(ports[0][0], 'clear')
                self.assertEqual(ports[0][1].addr, port1.addr)
                self.assertEqual(ports[1][0], 'print')
                self.assertEqual(ports[1][1].addr, port2.addr)
                self.assertEqual(ports[2][0], 'insert')
                self.assertEqual(ports[2][1].addr, port3.addr)

            def _user_consume(self):
                pass

        self._create_comp(MySink)

    def test_gen_src_output_ports_getitem(self):
        port1 = None
        port2 = None
        port3 = None

        class MySource(
            bt2._UserSourceComponent, message_iterator_class=bt2._UserMessageIterator
        ):
            def __init__(comp_self, config, params, obj):
                nonlocal port1, port2, port3
                port1 = comp_self._add_output_port('clear')
                port2 = comp_self._add_output_port('print')
                port3 = comp_self._add_output_port('insert')

        comp = self._create_comp(MySource)
        self.assertEqual(port3.addr, comp.output_ports['insert'].addr)
        self.assertEqual(port2.addr, comp.output_ports['print'].addr)
        self.assertEqual(port1.addr, comp.output_ports['clear'].addr)
        del port1
        del port2
        del port3

    def test_gen_flt_output_ports_getitem(self):
        port1 = None
        port2 = None
        port3 = None

        class MyFilter(
            bt2._UserFilterComponent, message_iterator_class=bt2._UserMessageIterator
        ):
            def __init__(comp_self, config, params, obj):
                nonlocal port1, port2, port3
                port1 = comp_self._add_output_port('clear')
                port2 = comp_self._add_output_port('print')
                port3 = comp_self._add_output_port('insert')

        comp = self._create_comp(MyFilter)
        self.assertEqual(port3.addr, comp.output_ports['insert'].addr)
        self.assertEqual(port2.addr, comp.output_ports['print'].addr)
        self.assertEqual(port1.addr, comp.output_ports['clear'].addr)
        del port1
        del port2
        del port3

    def test_gen_flt_input_ports_getitem(self):
        port1 = None
        port2 = None
        port3 = None

        class MyFilter(
            bt2._UserFilterComponent, message_iterator_class=bt2._UserMessageIterator
        ):
            def __init__(comp_self, config, params, obj):
                nonlocal port1, port2, port3
                port1 = comp_self._add_input_port('clear')
                port2 = comp_self._add_input_port('print')
                port3 = comp_self._add_input_port('insert')

        comp = self._create_comp(MyFilter)
        self.assertEqual(port3.addr, comp.input_ports['insert'].addr)
        self.assertEqual(port2.addr, comp.input_ports['print'].addr)
        self.assertEqual(port1.addr, comp.input_ports['clear'].addr)
        del port1
        del port2
        del port3

    def test_gen_sink_input_ports_getitem(self):
        port1 = None
        port2 = None
        port3 = None

        class MySink(bt2._UserSinkComponent):
            def __init__(comp_self, config, params, obj):
                nonlocal port1, port2, port3
                port1 = comp_self._add_input_port('clear')
                port2 = comp_self._add_input_port('print')
                port3 = comp_self._add_input_port('insert')

            def _user_consume(self):
                pass

        comp = self._create_comp(MySink)
        self.assertEqual(port3.addr, comp.input_ports['insert'].addr)
        self.assertEqual(port2.addr, comp.input_ports['print'].addr)
        self.assertEqual(port1.addr, comp.input_ports['clear'].addr)
        del port1
        del port2
        del port3

    def test_gen_src_output_ports_getitem_invalid_key(self):
        class MySource(
            bt2._UserSourceComponent, message_iterator_class=bt2._UserMessageIterator
        ):
            def __init__(comp_self, config, params, obj):
                comp_self._add_output_port('clear')
                comp_self._add_output_port('print')
                comp_self._add_output_port('insert')

        comp = self._create_comp(MySource)

        with self.assertRaises(KeyError):
            comp.output_ports['hello']

    def test_gen_flt_output_ports_getitem_invalid_key(self):
        class MyFilter(
            bt2._UserFilterComponent, message_iterator_class=bt2._UserMessageIterator
        ):
            def __init__(comp_self, config, params, obj):
                comp_self._add_output_port('clear')
                comp_self._add_output_port('print')
                comp_self._add_output_port('insert')

        comp = self._create_comp(MyFilter)

        with self.assertRaises(KeyError):
            comp.output_ports['hello']

    def test_gen_flt_input_ports_getitem_invalid_key(self):
        class MyFilter(
            bt2._UserFilterComponent, message_iterator_class=bt2._UserMessageIterator
        ):
            def __init__(comp_self, config, params, obj):
                comp_self._add_input_port('clear')
                comp_self._add_input_port('print')
                comp_self._add_input_port('insert')

        comp = self._create_comp(MyFilter)

        with self.assertRaises(KeyError):
            comp.input_ports['hello']

    def test_gen_sink_input_ports_getitem_invalid_key(self):
        class MySink(bt2._UserSinkComponent):
            def __init__(comp_self, config, params, obj):
                comp_self._add_input_port('clear')
                comp_self._add_input_port('print')
                comp_self._add_input_port('insert')

                with self.assertRaises(KeyError):
                    comp_self._input_ports['hello']

            def _user_consume(self):
                pass

        comp = self._create_comp(MySink)

        with self.assertRaises(KeyError):
            comp.input_ports['hello']

    def test_gen_src_output_ports_len(self):
        class MySource(
            bt2._UserSourceComponent, message_iterator_class=bt2._UserMessageIterator
        ):
            def __init__(comp_self, config, params, obj):
                comp_self._add_output_port('clear')
                comp_self._add_output_port('print')
                comp_self._add_output_port('insert')

        comp = self._create_comp(MySource)
        self.assertEqual(len(comp.output_ports), 3)

    def test_gen_flt_output_ports_len(self):
        class MyFilter(
            bt2._UserFilterComponent, message_iterator_class=bt2._UserMessageIterator
        ):
            def __init__(comp_self, config, params, obj):
                comp_self._add_output_port('clear')
                comp_self._add_output_port('print')
                comp_self._add_output_port('insert')

        comp = self._create_comp(MyFilter)
        self.assertEqual(len(comp.output_ports), 3)

    def test_gen_flt_input_ports_len(self):
        class MyFilter(
            bt2._UserFilterComponent, message_iterator_class=bt2._UserMessageIterator
        ):
            def __init__(comp_self, config, params, obj):
                comp_self._add_input_port('clear')
                comp_self._add_input_port('print')
                comp_self._add_input_port('insert')

        comp = self._create_comp(MyFilter)
        self.assertEqual(len(comp.input_ports), 3)

    def test_gen_sink_input_ports_len(self):
        class MySink(bt2._UserSinkComponent):
            def __init__(comp_self, config, params, obj):
                comp_self._add_input_port('clear')
                comp_self._add_input_port('print')
                comp_self._add_input_port('insert')

            def _user_consume(self):
                pass

        comp = self._create_comp(MySink)
        self.assertEqual(len(comp.input_ports), 3)

    def test_gen_src_output_ports_iter(self):
        port1 = None
        port2 = None
        port3 = None

        class MySource(
            bt2._UserSourceComponent, message_iterator_class=bt2._UserMessageIterator
        ):
            def __init__(comp_self, config, params, obj):
                nonlocal port1, port2, port3
                port1 = comp_self._add_output_port('clear')
                port2 = comp_self._add_output_port('print')
                port3 = comp_self._add_output_port('insert')

        comp = self._create_comp(MySource)
        ports = []

        for port_name, port in comp.output_ports.items():
            ports.append((port_name, port))

        self.assertEqual(ports[0][0], 'clear')
        self.assertEqual(ports[0][1].addr, port1.addr)
        self.assertEqual(ports[1][0], 'print')
        self.assertEqual(ports[1][1].addr, port2.addr)
        self.assertEqual(ports[2][0], 'insert')
        self.assertEqual(ports[2][1].addr, port3.addr)
        del port1
        del port2
        del port3

    def test_gen_flt_output_ports_iter(self):
        port1 = None
        port2 = None
        port3 = None

        class MyFilter(
            bt2._UserFilterComponent, message_iterator_class=bt2._UserMessageIterator
        ):
            def __init__(comp_self, config, params, obj):
                nonlocal port1, port2, port3
                port1 = comp_self._add_output_port('clear')
                port2 = comp_self._add_output_port('print')
                port3 = comp_self._add_output_port('insert')

        comp = self._create_comp(MyFilter)
        ports = []

        for port_name, port in comp.output_ports.items():
            ports.append((port_name, port))

        self.assertEqual(ports[0][0], 'clear')
        self.assertEqual(ports[0][1].addr, port1.addr)
        self.assertEqual(ports[1][0], 'print')
        self.assertEqual(ports[1][1].addr, port2.addr)
        self.assertEqual(ports[2][0], 'insert')
        self.assertEqual(ports[2][1].addr, port3.addr)
        del port1
        del port2
        del port3

    def test_gen_flt_input_ports_iter(self):
        port1 = None
        port2 = None
        port3 = None

        class MyFilter(
            bt2._UserFilterComponent, message_iterator_class=bt2._UserMessageIterator
        ):
            def __init__(comp_self, config, params, obj):
                nonlocal port1, port2, port3
                port1 = comp_self._add_input_port('clear')
                port2 = comp_self._add_input_port('print')
                port3 = comp_self._add_input_port('insert')

        comp = self._create_comp(MyFilter)
        ports = []

        for port_name, port in comp.input_ports.items():
            ports.append((port_name, port))

        self.assertEqual(ports[0][0], 'clear')
        self.assertEqual(ports[0][1].addr, port1.addr)
        self.assertEqual(ports[1][0], 'print')
        self.assertEqual(ports[1][1].addr, port2.addr)
        self.assertEqual(ports[2][0], 'insert')
        self.assertEqual(ports[2][1].addr, port3.addr)
        del port1
        del port2
        del port3

    def test_gen_sink_input_ports_iter(self):
        port1 = None
        port2 = None
        port3 = None

        class MySink(bt2._UserSinkComponent):
            def __init__(comp_self, config, params, obj):
                nonlocal port1, port2, port3
                port1 = comp_self._add_input_port('clear')
                port2 = comp_self._add_input_port('print')
                port3 = comp_self._add_input_port('insert')

            def _user_consume(self):
                pass

        comp = self._create_comp(MySink)
        ports = []

        for port_name, port in comp.input_ports.items():
            ports.append((port_name, port))

        self.assertEqual(ports[0][0], 'clear')
        self.assertEqual(ports[0][1].addr, port1.addr)
        self.assertEqual(ports[1][0], 'print')
        self.assertEqual(ports[1][1].addr, port2.addr)
        self.assertEqual(ports[2][0], 'insert')
        self.assertEqual(ports[2][1].addr, port3.addr)
        del port1
        del port2
        del port3

    def test_name(self):
        class MySink(bt2._UserSinkComponent):
            def __init__(comp_self, config, params, obj):
                comp_self._add_input_port('clear')

            def _user_consume(self):
                pass

        comp = self._create_comp(MySink)
        self.assertEqual(comp.input_ports['clear'].name, 'clear')

    def test_connection_none(self):
        class MySink(bt2._UserSinkComponent):
            def __init__(comp_self, config, params, obj):
                comp_self._add_input_port('clear')

            def _user_consume(self):
                pass

        comp = self._create_comp(MySink)
        self.assertIsNone(comp.input_ports['clear'].connection)

    def test_is_connected_false(self):
        class MySink(bt2._UserSinkComponent):
            def __init__(comp_self, config, params, obj):
                comp_self._add_input_port('clear')

            def _user_consume(self):
                pass

        comp = self._create_comp(MySink)
        self.assertFalse(comp.input_ports['clear'].is_connected)

    def test_self_name(self):
        class MySink(bt2._UserSinkComponent):
            def __init__(comp_self, config, params, obj):
                port = comp_self._add_input_port('clear')
                self.assertEqual(port.name, 'clear')

            def _user_consume(self):
                pass

        self._create_comp(MySink)

    def test_self_connection_none(self):
        class MySink(bt2._UserSinkComponent):
            def __init__(comp_self, config, params, obj):
                port = comp_self._add_input_port('clear')
                self.assertIsNone(port.connection)

            def _user_consume(self):
                pass

        self._create_comp(MySink)

    def test_self_is_connected_false(self):
        class MySink(bt2._UserSinkComponent):
            def __init__(comp_self, config, params, obj):
                port = comp_self._add_input_port('clear')
                self.assertFalse(port.is_connected)

            def _user_consume(self):
                pass

        self._create_comp(MySink)

    def test_source_self_port_user_data(self):
        class MyUserData:
            def __del__(self):
                nonlocal objects_deleted
                objects_deleted += 1

        class MySource(
            bt2._UserFilterComponent, message_iterator_class=bt2._UserMessageIterator
        ):
            def __init__(comp_self, config, params, obj):
                nonlocal user_datas

                p = comp_self._add_output_port('port1')
                user_datas.append(p.user_data)
                p = comp_self._add_output_port('port2', 2)
                user_datas.append(p.user_data)
                p = comp_self._add_output_port('port3', MyUserData())
                user_datas.append(p.user_data)

        user_datas = []
        objects_deleted = 0

        comp = self._create_comp(MySource)
        self.assertEqual(len(user_datas), 3)
        self.assertIs(user_datas[0], None)
        self.assertEqual(user_datas[1], 2)
        self.assertIs(type(user_datas[2]), MyUserData)

        # Verify that the user data gets freed.
        self.assertEqual(objects_deleted, 0)
        del user_datas
        del comp
        self.assertEqual(objects_deleted, 1)

    def test_filter_self_port_user_data(self):
        class MyUserData:
            def __del__(self):
                nonlocal objects_deleted
                objects_deleted += 1

        class MyFilter(
            bt2._UserFilterComponent, message_iterator_class=bt2._UserMessageIterator
        ):
            def __init__(comp_self, config, params, obj):
                nonlocal user_datas

                p = comp_self._add_output_port('port1')
                user_datas.append(p.user_data)
                p = comp_self._add_output_port('port2', 'user data string')
                user_datas.append(p.user_data)
                p = comp_self._add_output_port('port3', MyUserData())
                user_datas.append(p.user_data)

                p = comp_self._add_input_port('port4')
                user_datas.append(p.user_data)
                p = comp_self._add_input_port('port5', user_data={'user data': 'dict'})
                user_datas.append(p.user_data)
                p = comp_self._add_input_port('port6', MyUserData())
                user_datas.append(p.user_data)

        user_datas = []
        objects_deleted = 0

        comp = self._create_comp(MyFilter)
        self.assertEqual(len(user_datas), 6)
        self.assertIs(user_datas[0], None)
        self.assertEqual(user_datas[1], 'user data string')
        self.assertIs(type(user_datas[2]), MyUserData)
        self.assertIs(user_datas[3], None)
        self.assertEqual(user_datas[4], {'user data': 'dict'})
        self.assertIs(type(user_datas[5]), MyUserData)

        # Verify that the user data gets freed.
        self.assertEqual(objects_deleted, 0)
        del user_datas
        del comp
        self.assertEqual(objects_deleted, 2)

    def test_sink_self_port_user_data(self):
        class MyUserData:
            def __del__(self):
                nonlocal objects_deleted
                objects_deleted += 1

        class MySink(bt2._UserSinkComponent):
            def __init__(comp_self, config, params, obj):
                nonlocal user_datas

                p = comp_self._add_input_port('port1')
                user_datas.append(p.user_data)
                p = comp_self._add_input_port('port2', MyUserData())
                user_datas.append(p.user_data)

            def _user_consume(self):
                pass

        user_datas = []
        objects_deleted = 0

        comp = self._create_comp(MySink)
        self.assertEqual(len(user_datas), 2)
        self.assertIs(user_datas[0], None)
        self.assertIs(type(user_datas[1]), MyUserData)

        # Verify that the user data gets freed.
        self.assertEqual(objects_deleted, 0)
        del user_datas
        del comp
        self.assertEqual(objects_deleted, 1)


if __name__ == '__main__':
    unittest.main()
