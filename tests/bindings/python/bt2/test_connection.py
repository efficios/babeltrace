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


class ConnectionTestCase(unittest.TestCase):
    def test_create(self):
        class MyIter(bt2._UserMessageIterator):
            def __next__(self):
                raise bt2.Stop

        class MySource(bt2._UserSourceComponent, message_iterator_class=MyIter):
            def __init__(self, params, obj):
                self._add_output_port('out')

        class MySink(bt2._UserSinkComponent):
            def __init__(self, params, obj):
                self._add_input_port('in')

            def _user_consume(self):
                raise bt2.Stop

        graph = bt2.Graph()
        src = graph.add_component(MySource, 'src')
        sink = graph.add_component(MySink, 'sink')
        conn = graph.connect_ports(src.output_ports['out'], sink.input_ports['in'])

    def test_downstream_port(self):
        class MyIter(bt2._UserMessageIterator):
            def __next__(self):
                raise bt2.Stop

        class MySource(bt2._UserSourceComponent, message_iterator_class=MyIter):
            def __init__(self, params, obj):
                self._add_output_port('out')

        class MySink(bt2._UserSinkComponent):
            def __init__(self, params, obj):
                self._add_input_port('in')

            def _user_consume(self):
                raise bt2.Stop

        graph = bt2.Graph()
        src = graph.add_component(MySource, 'src')
        sink = graph.add_component(MySink, 'sink')
        conn = graph.connect_ports(src.output_ports['out'], sink.input_ports['in'])
        self.assertEqual(conn.downstream_port.addr, sink.input_ports['in'].addr)

    def test_upstream_port(self):
        class MyIter(bt2._UserMessageIterator):
            def __next__(self):
                raise bt2.Stop

        class MySource(bt2._UserSourceComponent, message_iterator_class=MyIter):
            def __init__(self, params, obj):
                self._add_output_port('out')

        class MySink(bt2._UserSinkComponent):
            def __init__(self, params, obj):
                self._add_input_port('in')

            def _user_consume(self):
                raise bt2.Stop

        graph = bt2.Graph()
        src = graph.add_component(MySource, 'src')
        sink = graph.add_component(MySink, 'sink')
        conn = graph.connect_ports(src.output_ports['out'], sink.input_ports['in'])
        self.assertEqual(conn.upstream_port.addr, src.output_ports['out'].addr)
