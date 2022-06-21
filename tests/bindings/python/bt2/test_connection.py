# SPDX-License-Identifier: GPL-2.0-only
#
# Copyright (C) 2019 EfficiOS Inc.
#

import unittest
import bt2
from bt2 import connection as bt2_connection
from bt2 import port as bt2_port


class ConnectionTestCase(unittest.TestCase):
    def test_create(self):
        class MySource(
            bt2._UserSourceComponent, message_iterator_class=bt2._UserMessageIterator
        ):
            def __init__(self, config, params, obj):
                self._add_output_port("out")

        class MySink(bt2._UserSinkComponent):
            def __init__(self, config, params, obj):
                self._add_input_port("in")

            def _user_consume(self):
                raise bt2.Stop

        graph = bt2.Graph()
        src = graph.add_component(MySource, "src")
        sink = graph.add_component(MySink, "sink")
        conn = graph.connect_ports(src.output_ports["out"], sink.input_ports["in"])
        self.assertIs(type(conn), bt2_connection._ConnectionConst)

    def test_downstream_port(self):
        class MySource(
            bt2._UserSourceComponent, message_iterator_class=bt2._UserMessageIterator
        ):
            def __init__(self, config, params, obj):
                self._add_output_port("out")

        class MySink(bt2._UserSinkComponent):
            def __init__(self, config, params, obj):
                self._add_input_port("in")

            def _user_consume(self):
                raise bt2.Stop

        graph = bt2.Graph()
        src = graph.add_component(MySource, "src")
        sink = graph.add_component(MySink, "sink")
        conn = graph.connect_ports(src.output_ports["out"], sink.input_ports["in"])
        self.assertEqual(conn.downstream_port.addr, sink.input_ports["in"].addr)
        self.assertIs(type(conn), bt2_connection._ConnectionConst)
        self.assertIs(type(conn.downstream_port), bt2_port._InputPortConst)

    def test_upstream_port(self):
        class MySource(
            bt2._UserSourceComponent, message_iterator_class=bt2._UserMessageIterator
        ):
            def __init__(self, config, params, obj):
                self._add_output_port("out")

        class MySink(bt2._UserSinkComponent):
            def __init__(self, config, params, obj):
                self._add_input_port("in")

            def _user_consume(self):
                raise bt2.Stop

        graph = bt2.Graph()
        src = graph.add_component(MySource, "src")
        sink = graph.add_component(MySink, "sink")
        conn = graph.connect_ports(src.output_ports["out"], sink.input_ports["in"])
        self.assertEqual(conn.upstream_port.addr, src.output_ports["out"].addr)
        self.assertIs(type(conn.upstream_port), bt2_port._OutputPortConst)


if __name__ == "__main__":
    unittest.main()
