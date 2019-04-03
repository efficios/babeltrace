from bt2 import values
import collections
import unittest
import copy
import bt2


@unittest.skip("this is broken")
class UserNotificationIteratorTestCase(unittest.TestCase):
    @staticmethod
    def _create_graph(src_comp_cls):
        class MySink(bt2._UserSinkComponent):
            def __init__(self, params):
                self._add_input_port('in')

            def _consume(self):
                next(self._notif_iter)

            def _port_connected(self, port, other_port):
                self._notif_iter = port.connection.create_notification_iterator()

        graph = bt2.Graph()
        src_comp = graph.add_component(src_comp_cls, 'src')
        sink_comp = graph.add_component(MySink, 'sink')
        graph.connect_ports(src_comp.output_ports['out'],
                            sink_comp.input_ports['in'])
        return graph

    def test_init(self):
        class MyIter(bt2._UserNotificationIterator):
            def __init__(self):
                nonlocal initialized
                initialized = True

        class MySource(bt2._UserSourceComponent,
                       notification_iterator_class=MyIter):
            def __init__(self, params):
                self._add_output_port('out')

        initialized = False
        graph = self._create_graph(MySource)
        self.assertTrue(initialized)

    def test_finalize(self):
        class MyIter(bt2._UserNotificationIterator):
            def _finalize(self):
                nonlocal finalized
                finalized = True

        class MySource(bt2._UserSourceComponent,
                       notification_iterator_class=MyIter):
            def __init__(self, params):
                self._add_output_port('out')

        finalized = False
        graph = self._create_graph(MySource)
        del graph
        self.assertTrue(finalized)

    def test_component(self):
        class MyIter(bt2._UserNotificationIterator):
            def __init__(self):
                nonlocal salut
                salut = self._component._salut

        class MySource(bt2._UserSourceComponent,
                       notification_iterator_class=MyIter):
            def __init__(self, params):
                self._add_output_port('out')
                self._salut = 23

        salut = None
        graph = self._create_graph(MySource)
        self.assertEqual(salut, 23)

    def test_addr(self):
        class MyIter(bt2._UserNotificationIterator):
            def __init__(self):
                nonlocal addr
                addr = self.addr

        class MySource(bt2._UserSourceComponent,
                       notification_iterator_class=MyIter):
            def __init__(self, params):
                self._add_output_port('out')

        addr = None
        graph = self._create_graph(MySource)
        self.assertIsNotNone(addr)
        self.assertNotEqual(addr, 0)


@unittest.skip("this is broken")
class PrivateConnectionNotificationIteratorTestCase(unittest.TestCase):
    def test_component(self):
        class MyIter(bt2._UserNotificationIterator):
            pass

        class MySource(bt2._UserSourceComponent,
                       notification_iterator_class=MyIter):
            def __init__(self, params):
                self._add_output_port('out')

        class MySink(bt2._UserSinkComponent):
            def __init__(self, params):
                self._add_input_port('in')

            def _consume(self):
                next(self._notif_iter)

            def _port_connected(self, port, other_port):
                nonlocal upstream_comp
                self._notif_iter = port.connection.create_notification_iterator()
                upstream_comp = self._notif_iter.component

        upstream_comp = None
        graph = bt2.Graph()
        src_comp = graph.add_component(MySource, 'src')
        sink_comp = graph.add_component(MySink, 'sink')
        graph.connect_ports(src_comp.output_ports['out'],
                            sink_comp.input_ports['in'])
        self.assertEqual(src_comp, upstream_comp)
        del upstream_comp


@unittest.skip("this is broken")
class OutputPortNotificationIteratorTestCase(unittest.TestCase):
    def test_component(self):
        class MyIter(bt2._UserNotificationIterator):
            def __init__(self):
                self._build_meta()
                self._at = 0

            def _build_meta(self):
                self._trace = bt2.Trace()
                self._sc = bt2.StreamClass()
                self._ec = bt2.EventClass('salut')
                self._my_int_ft = bt2.IntegerFieldType(32)
                self._ec.payload_field_type = bt2.StructureFieldType()
                self._ec.payload_field_type += collections.OrderedDict([
                    ('my_int', self._my_int_ft),
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

                notif = bt2.EventNotification(self._create_event(self._at * 3))
                self._at += 1
                return notif

        class MySource(bt2._UserSourceComponent,
                       notification_iterator_class=MyIter):
            def __init__(self, params):
                self._add_output_port('out')

        graph = bt2.Graph()
        src = graph.add_component(MySource, 'src')
        types = [bt2.EventNotification]
        notif_iter = src.output_ports['out'].create_notification_iterator(types)

        for at, notif in enumerate(notif_iter):
            self.assertIsInstance(notif, bt2.EventNotification)
            self.assertEqual(notif.event.event_class.name, 'salut')
            field = notif.event.payload_field['my_int']
            self.assertEqual(field, at * 3)
