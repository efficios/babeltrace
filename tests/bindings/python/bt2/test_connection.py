from bt2 import value
import unittest
import copy
import bt2


@unittest.skip("this is broken")
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
        self.assertNotIsInstance(conn, bt2._PrivateConnection)

    def test_is_ended_false(self):
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
        self.assertFalse(conn.is_ended)

    def test_is_ended_true(self):
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
        src.output_ports['out'].disconnect()
        self.assertTrue(conn.is_ended)

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

    def test_eq(self):
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
        self.assertEqual(conn, conn)

    def test_eq_invalid(self):
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
        self.assertNotEqual(conn, 23)


@unittest.skip("this is broken")
class PrivateConnectionTestCase(unittest.TestCase):
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

            def _port_connected(self, port, other_port):
                nonlocal priv_conn
                priv_conn = port.connection

        priv_conn = None
        graph = bt2.Graph()
        src = graph.add_component(MySource, 'src')
        sink = graph.add_component(MySink, 'sink')
        conn = graph.connect_ports(src.output_ports['out'],
                                   sink.input_ports['in'])
        self.assertIsInstance(priv_conn, bt2._PrivateConnection)
        self.assertEqual(conn, priv_conn)
        del priv_conn

    def test_is_ended_false(self):
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

            def _port_connected(self, port, other_port):
                nonlocal priv_conn
                priv_conn = port.connection

        priv_conn = None
        graph = bt2.Graph()
        src = graph.add_component(MySource, 'src')
        sink = graph.add_component(MySink, 'sink')
        conn = graph.connect_ports(src.output_ports['out'],
                                   sink.input_ports['in'])
        self.assertFalse(priv_conn.is_ended)
        del priv_conn

    def test_is_ended_true(self):
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

            def _port_connected(self, port, other_port):
                nonlocal priv_conn
                priv_conn = port.connection

        priv_conn = None
        graph = bt2.Graph()
        src = graph.add_component(MySource, 'src')
        sink = graph.add_component(MySink, 'sink')
        conn = graph.connect_ports(src.output_ports['out'],
                                   sink.input_ports['in'])
        sink.input_ports['in'].disconnect()
        self.assertTrue(priv_conn.is_ended)
        del priv_conn

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

            def _port_connected(self, port, other_port):
                nonlocal priv_port
                priv_conn = port.connection
                priv_port = priv_conn.downstream_port

        priv_port = None
        graph = bt2.Graph()
        src = graph.add_component(MySource, 'src')
        sink = graph.add_component(MySink, 'sink')
        conn = graph.connect_ports(src.output_ports['out'],
                                   sink.input_ports['in'])
        self.assertEqual(priv_port.addr, sink.input_ports['in'].addr)
        del priv_port

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

            def _port_connected(self, port, other_port):
                nonlocal priv_port
                priv_conn = port.connection
                priv_port = priv_conn.upstream_port

        priv_port = None
        graph = bt2.Graph()
        src = graph.add_component(MySource, 'src')
        sink = graph.add_component(MySink, 'sink')
        conn = graph.connect_ports(src.output_ports['out'],
                                   sink.input_ports['in'])
        self.assertEqual(priv_port.addr, src.output_ports['out'].addr)
        del priv_port

    def test_eq(self):
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

            def _port_connected(self, port, other_port):
                nonlocal priv_conn
                priv_conn = port.connection

        priv_conn = None
        graph = bt2.Graph()
        src = graph.add_component(MySource, 'src')
        sink = graph.add_component(MySink, 'sink')
        conn = graph.connect_ports(src.output_ports['out'],
                                   sink.input_ports['in'])
        self.assertEqual(priv_conn, conn)
        del priv_conn

    def test_eq_invalid(self):
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

            def _port_connected(self, port, other_port):
                nonlocal priv_conn
                priv_conn = port.connection

        priv_conn = None
        graph = bt2.Graph()
        src = graph.add_component(MySource, 'src')
        sink = graph.add_component(MySink, 'sink')
        conn = graph.connect_ports(src.output_ports['out'],
                                   sink.input_ports['in'])
        self.assertNotEqual(priv_conn, 23)
        del priv_conn
