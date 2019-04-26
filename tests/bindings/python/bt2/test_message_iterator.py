from bt2 import value
import collections
import unittest
import copy
import bt2


@unittest.skip("this is broken")
class UserMessageIteratorTestCase(unittest.TestCase):
    @staticmethod
    def _create_graph(src_comp_cls):
        class MySink(bt2._UserSinkComponent):
            def __init__(self, params):
                self._add_input_port('in')

            def _consume(self):
                next(self._msg_iter)

            def _port_connected(self, port, other_port):
                self._msg_iter = port.connection.create_message_iterator()

        graph = bt2.Graph()
        src_comp = graph.add_component(src_comp_cls, 'src')
        sink_comp = graph.add_component(MySink, 'sink')
        graph.connect_ports(src_comp.output_ports['out'],
                            sink_comp.input_ports['in'])
        return graph

    def test_init(self):
        class MyIter(bt2._UserMessageIterator):
            def __init__(self):
                nonlocal initialized
                initialized = True

        class MySource(bt2._UserSourceComponent,
                       message_iterator_class=MyIter):
            def __init__(self, params):
                self._add_output_port('out')

        initialized = False
        graph = self._create_graph(MySource)
        self.assertTrue(initialized)

    def test_finalize(self):
        class MyIter(bt2._UserMessageIterator):
            def _finalize(self):
                nonlocal finalized
                finalized = True

        class MySource(bt2._UserSourceComponent,
                       message_iterator_class=MyIter):
            def __init__(self, params):
                self._add_output_port('out')

        finalized = False
        graph = self._create_graph(MySource)
        del graph
        self.assertTrue(finalized)

    def test_component(self):
        class MyIter(bt2._UserMessageIterator):
            def __init__(self):
                nonlocal salut
                salut = self._component._salut

        class MySource(bt2._UserSourceComponent,
                       message_iterator_class=MyIter):
            def __init__(self, params):
                self._add_output_port('out')
                self._salut = 23

        salut = None
        graph = self._create_graph(MySource)
        self.assertEqual(salut, 23)

    def test_addr(self):
        class MyIter(bt2._UserMessageIterator):
            def __init__(self):
                nonlocal addr
                addr = self.addr

        class MySource(bt2._UserSourceComponent,
                       message_iterator_class=MyIter):
            def __init__(self, params):
                self._add_output_port('out')

        addr = None
        graph = self._create_graph(MySource)
        self.assertIsNotNone(addr)
        self.assertNotEqual(addr, 0)


@unittest.skip("this is broken")
class PrivateConnectionMessageIteratorTestCase(unittest.TestCase):
    def test_component(self):
        class MyIter(bt2._UserMessageIterator):
            pass

        class MySource(bt2._UserSourceComponent,
                       message_iterator_class=MyIter):
            def __init__(self, params):
                self._add_output_port('out')

        class MySink(bt2._UserSinkComponent):
            def __init__(self, params):
                self._add_input_port('in')

            def _consume(self):
                next(self._msg_iter)

            def _port_connected(self, port, other_port):
                nonlocal upstream_comp
                self._msg_iter = port.connection.create_message_iterator()
                upstream_comp = self._msg_iter.component

        upstream_comp = None
        graph = bt2.Graph()
        src_comp = graph.add_component(MySource, 'src')
        sink_comp = graph.add_component(MySink, 'sink')
        graph.connect_ports(src_comp.output_ports['out'],
                            sink_comp.input_ports['in'])
        self.assertEqual(src_comp, upstream_comp)
        del upstream_comp


@unittest.skip("this is broken")
class OutputPortMessageIteratorTestCase(unittest.TestCase):
    def test_component(self):
        class MyIter(bt2._UserMessageIterator):
            def __init__(self):
                self._build_meta()
                self._at = 0

            def _build_meta(self):
                self._trace = bt2.Trace()
                self._sc = bt2.StreamClass()
                self._ec = bt2.EventClass('salut')
                self._my_int_fc = bt2.IntegerFieldClass(32)
                self._ec.payload_field_class = bt2.StructureFieldClass()
                self._ec.payload_field_class += collections.OrderedDict([
                    ('my_int', self._my_int_fc),
                ])
                self._sc.add_event_class(self._ec)
                self._trace.add_stream_class(self._sc)
                self._stream = self._sc()
                self._packet = self._stream.create_packet()

            def _create_event(self, value):
                ev = self._ec()
                ev.payload_field['my_int'] = value
                ev.packet = self._packet
                return ev

            def __next__(self):
                if self._at == 5:
                    raise bt2.Stop

                msg = bt2.EventMessage(self._create_event(self._at * 3))
                self._at += 1
                return msg

        class MySource(bt2._UserSourceComponent,
                       message_iterator_class=MyIter):
            def __init__(self, params):
                self._add_output_port('out')

        graph = bt2.Graph()
        src = graph.add_component(MySource, 'src')
        types = [bt2.EventMessage]
        msg_iter = src.output_ports['out'].create_message_iterator(types)

        for at, msg in enumerate(msg_iter):
            self.assertIsInstance(msg, bt2.EventMessage)
            self.assertEqual(msg.event.event_class.name, 'salut')
            field = msg.event.payload_field['my_int']
            self.assertEqual(field, at * 3)
