import unittest
import bt2


class ConnectionTestCase(unittest.TestCase):
    def test_create(self):
        class MyIter(bt2._UserMessageIterator):
            def __next__(self):
                raise bt2.Stop

        class MySource(bt2._UserSourceComponent,
                       message_iterator_class=MyIter):
            def __init__(self, params):
                self._add_output_port('out')

        class MySink(bt2._UserSinkComponent):
            def __init__(self, params):
                self._add_input_port('in')

            def _consume(self):
                raise bt2.Stop

        graph = bt2.Graph()
        src = graph.add_component(MySource, 'src')
        sink = graph.add_component(MySink, 'sink')
        conn = graph.connect_ports(src.output_ports['out'],
                                   sink.input_ports['in'])
        self.assertIsInstance(conn, bt2._Connection)

    def test_downstream_port(self):
        class MyIter(bt2._UserMessageIterator):
            def __next__(self):
                raise bt2.Stop

        class MySource(bt2._UserSourceComponent,
                       message_iterator_class=MyIter):
            def __init__(self, params):
                self._add_output_port('out')

        class MySink(bt2._UserSinkComponent):
            def __init__(self, params):
                self._add_input_port('in')

            def _consume(self):
                raise bt2.Stop

        graph = bt2.Graph()
        src = graph.add_component(MySource, 'src')
        sink = graph.add_component(MySink, 'sink')
        conn = graph.connect_ports(src.output_ports['out'],
                                   sink.input_ports['in'])
        self.assertEqual(conn.downstream_port.addr, sink.input_ports['in'].addr)

    def test_upstream_port(self):
        class MyIter(bt2._UserMessageIterator):
            def __next__(self):
                raise bt2.Stop

        class MySource(bt2._UserSourceComponent,
                       message_iterator_class=MyIter):
            def __init__(self, params):
                self._add_output_port('out')

        class MySink(bt2._UserSinkComponent):
            def __init__(self, params):
                self._add_input_port('in')

            def _consume(self):
                raise bt2.Stop

        graph = bt2.Graph()
        src = graph.add_component(MySource, 'src')
        sink = graph.add_component(MySink, 'sink')
        conn = graph.connect_ports(src.output_ports['out'],
                                   sink.input_ports['in'])
        self.assertEqual(conn.upstream_port.addr, src.output_ports['out'].addr)
