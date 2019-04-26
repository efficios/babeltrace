from bt2 import value
import unittest
import copy
import bt2


@unittest.skip("this is broken")
class PortTestCase(unittest.TestCase):
    @staticmethod
    def _create_comp(comp_cls, name=None):
        graph = bt2.Graph()

        if name is None:
            name = 'comp'

        return graph.add_component(comp_cls, name)

    def test_src_add_output_port(self):
        class MyIter(bt2._UserMessageIterator):
            def __next__(self):
                raise bt2.Stop

        class MySource(bt2._UserSourceComponent,
                       message_iterator_class=MyIter):
            def __init__(comp_self, params):
                port = comp_self._add_output_port('out')
                self.assertEqual(port.name, 'out')

        comp = self._create_comp(MySource)
        self.assertEqual(len(comp.output_ports), 1)


    def test_flt_add_output_port(self):
        class MyIter(bt2._UserMessageIterator):
            def __next__(self):
                raise bt2.Stop

        class MyFilter(bt2._UserFilterComponent,
                       message_iterator_class=MyIter):
            def __init__(comp_self, params):
                port = comp_self._add_output_port('out')
                self.assertEqual(port.name, 'out')

        comp = self._create_comp(MyFilter)
        self.assertEqual(len(comp.output_ports), 1)

    def test_flt_add_input_port(self):
        class MyIter(bt2._UserMessageIterator):
            def __next__(self):
                raise bt2.Stop

        class MyFilter(bt2._UserFilterComponent,
                       message_iterator_class=MyIter):
            def __init__(comp_self, params):
                port = comp_self._add_input_port('in')
                self.assertEqual(port.name, 'in')

        comp = self._create_comp(MyFilter)
        self.assertEqual(len(comp.input_ports), 1)

    def test_sink_add_input_port(self):
        class MySink(bt2._UserSinkComponent):
            def __init__(comp_self, params):
                port = comp_self._add_input_port('in')
                self.assertEqual(port.name, 'in')

            def _consume(self):
                pass

        comp = self._create_comp(MySink)
        self.assertEqual(len(comp.input_ports), 1)

    def test_user_src_output_ports_getitem(self):
        class MyIter(bt2._UserMessageIterator):
            def __next__(self):
                raise bt2.Stop

        class MySource(bt2._UserSourceComponent,
                       message_iterator_class=MyIter):
            def __init__(comp_self, params):
                port1 = comp_self._add_output_port('clear')
                port2 = comp_self._add_output_port('print')
                port3 = comp_self._add_output_port('insert')
                self.assertEqual(port3.addr, comp_self._output_ports['insert'].addr)
                self.assertEqual(port2.addr, comp_self._output_ports['print'].addr)
                self.assertEqual(port1.addr, comp_self._output_ports['clear'].addr)

        comp = self._create_comp(MySource)

    def test_user_flt_output_ports_getitem(self):
        class MyIter(bt2._UserMessageIterator):
            def __next__(self):
                raise bt2.Stop

        class MyFilter(bt2._UserFilterComponent,
                       message_iterator_class=MyIter):
            def __init__(comp_self, params):
                port1 = comp_self._add_output_port('clear')
                port2 = comp_self._add_output_port('print')
                port3 = comp_self._add_output_port('insert')
                self.assertEqual(port3.addr, comp_self._output_ports['insert'].addr)
                self.assertEqual(port2.addr, comp_self._output_ports['print'].addr)
                self.assertEqual(port1.addr, comp_self._output_ports['clear'].addr)

        comp = self._create_comp(MyFilter)

    def test_user_flt_input_ports_getitem(self):
        class MyIter(bt2._UserMessageIterator):
            def __next__(self):
                raise bt2.Stop

        class MyFilter(bt2._UserFilterComponent,
                       message_iterator_class=MyIter):
            def __init__(comp_self, params):
                port1 = comp_self._add_input_port('clear')
                port2 = comp_self._add_input_port('print')
                port3 = comp_self._add_input_port('insert')
                self.assertEqual(port3.addr, comp_self._input_ports['insert'].addr)
                self.assertEqual(port2.addr, comp_self._input_ports['print'].addr)
                self.assertEqual(port1.addr, comp_self._input_ports['clear'].addr)

        comp = self._create_comp(MyFilter)

    def test_user_sink_input_ports_getitem(self):
        class MySink(bt2._UserSinkComponent):
            def __init__(comp_self, params):
                port1 = comp_self._add_input_port('clear')
                port2 = comp_self._add_input_port('print')
                port3 = comp_self._add_input_port('insert')
                self.assertEqual(port3.addr, comp_self._input_ports['insert'].addr)
                self.assertEqual(port2.addr, comp_self._input_ports['print'].addr)
                self.assertEqual(port1.addr, comp_self._input_ports['clear'].addr)

            def _consume(self):
                pass

        comp = self._create_comp(MySink)

    def test_user_src_output_ports_getitem_invalid_key(self):
        class MyIter(bt2._UserMessageIterator):
            def __next__(self):
                raise bt2.Stop

        class MySource(bt2._UserSourceComponent,
                       message_iterator_class=MyIter):
            def __init__(comp_self, params):
                comp_self._add_output_port('clear')
                comp_self._add_output_port('print')
                comp_self._add_output_port('insert')

                with self.assertRaises(KeyError):
                    comp_self._output_ports['hello']

        comp = self._create_comp(MySource)

    def test_user_flt_output_ports_getitem_invalid_key(self):
        class MyIter(bt2._UserMessageIterator):
            def __next__(self):
                raise bt2.Stop

        class MyFilter(bt2._UserFilterComponent,
                       message_iterator_class=MyIter):
            def __init__(comp_self, params):
                comp_self._add_output_port('clear')
                comp_self._add_output_port('print')
                comp_self._add_output_port('insert')

                with self.assertRaises(KeyError):
                    comp_self._output_ports['hello']

        comp = self._create_comp(MyFilter)

    def test_user_flt_input_ports_getitem_invalid_key(self):
        class MyIter(bt2._UserMessageIterator):
            def __next__(self):
                raise bt2.Stop

        class MyFilter(bt2._UserFilterComponent,
                       message_iterator_class=MyIter):
            def __init__(comp_self, params):
                comp_self._add_input_port('clear')
                comp_self._add_input_port('print')
                comp_self._add_input_port('insert')

                with self.assertRaises(KeyError):
                    comp_self._input_ports['hello']

        comp = self._create_comp(MyFilter)

    def test_user_sink_input_ports_getitem_invalid_key(self):
        class MySink(bt2._UserSinkComponent):
            def __init__(comp_self, params):
                comp_self._add_input_port('clear')
                comp_self._add_input_port('print')
                comp_self._add_input_port('insert')

                with self.assertRaises(KeyError):
                    comp_self._input_ports['hello']

            def _consume(self):
                pass

        comp = self._create_comp(MySink)

    def test_user_src_output_ports_len(self):
        class MyIter(bt2._UserMessageIterator):
            def __next__(self):
                raise bt2.Stop

        class MySource(bt2._UserSourceComponent,
                       message_iterator_class=MyIter):
            def __init__(comp_self, params):
                comp_self._add_output_port('clear')
                comp_self._add_output_port('print')
                comp_self._add_output_port('insert')
                self.assertEqual(len(comp_self._output_ports), 3)

        comp = self._create_comp(MySource)

    def test_user_flt_output_ports_len(self):
        class MyIter(bt2._UserMessageIterator):
            def __next__(self):
                raise bt2.Stop

        class MyFilter(bt2._UserFilterComponent,
                       message_iterator_class=MyIter):
            def __init__(comp_self, params):
                comp_self._add_output_port('clear')
                comp_self._add_output_port('print')
                comp_self._add_output_port('insert')
                self.assertEqual(len(comp_self._output_ports), 3)

        comp = self._create_comp(MyFilter)

    def test_user_flt_input_ports_len(self):
        class MyIter(bt2._UserMessageIterator):
            def __next__(self):
                raise bt2.Stop

        class MyFilter(bt2._UserFilterComponent,
                       message_iterator_class=MyIter):
            def __init__(comp_self, params):
                comp_self._add_input_port('clear')
                comp_self._add_input_port('print')
                comp_self._add_input_port('insert')
                self.assertEqual(len(comp_self._input_ports), 3)

        comp = self._create_comp(MyFilter)

    def test_user_sink_input_ports_len(self):
        class MySink(bt2._UserSinkComponent):
            def __init__(comp_self, params):
                comp_self._add_input_port('clear')
                comp_self._add_input_port('print')
                comp_self._add_input_port('insert')
                self.assertEqual(len(comp_self._input_ports), 3)

            def _consume(self):
                pass

        comp = self._create_comp(MySink)

    def test_user_src_output_ports_iter(self):
        class MyIter(bt2._UserMessageIterator):
            def __next__(self):
                raise bt2.Stop

        class MySource(bt2._UserSourceComponent,
                       message_iterator_class=MyIter):
            def __init__(comp_self, params):
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

        comp = self._create_comp(MySource)

    def test_user_flt_output_ports_iter(self):
        class MyIter(bt2._UserMessageIterator):
            def __next__(self):
                raise bt2.Stop

        class MyFilter(bt2._UserFilterComponent,
                       message_iterator_class=MyIter):
            def __init__(comp_self, params):
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

        comp = self._create_comp(MyFilter)

    def test_user_flt_input_ports_iter(self):
        class MyIter(bt2._UserMessageIterator):
            def __next__(self):
                raise bt2.Stop

        class MyFilter(bt2._UserFilterComponent,
                       message_iterator_class=MyIter):
            def __init__(comp_self, params):
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

        comp = self._create_comp(MyFilter)

    def test_user_sink_input_ports_iter(self):
        class MySink(bt2._UserSinkComponent):
            def __init__(comp_self, params):
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

            def _consume(self):
                pass

        comp = self._create_comp(MySink)

    def test_gen_src_output_ports_getitem(self):
        class MyIter(bt2._UserMessageIterator):
            def __next__(self):
                raise bt2.Stop

        port1 = None
        port2 = None
        port3 = None

        class MySource(bt2._UserSourceComponent,
                       message_iterator_class=MyIter):
            def __init__(comp_self, params):
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
        class MyIter(bt2._UserMessageIterator):
            def __next__(self):
                raise bt2.Stop

        port1 = None
        port2 = None
        port3 = None

        class MyFilter(bt2._UserFilterComponent,
                       message_iterator_class=MyIter):
            def __init__(comp_self, params):
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
        class MyIter(bt2._UserMessageIterator):
            def __next__(self):
                raise bt2.Stop

        port1 = None
        port2 = None
        port3 = None

        class MyFilter(bt2._UserFilterComponent,
                       message_iterator_class=MyIter):
            def __init__(comp_self, params):
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
            def __init__(comp_self, params):
                nonlocal port1, port2, port3
                port1 = comp_self._add_input_port('clear')
                port2 = comp_self._add_input_port('print')
                port3 = comp_self._add_input_port('insert')

            def _consume(self):
                pass

        comp = self._create_comp(MySink)
        self.assertEqual(port3.addr, comp.input_ports['insert'].addr)
        self.assertEqual(port2.addr, comp.input_ports['print'].addr)
        self.assertEqual(port1.addr, comp.input_ports['clear'].addr)
        del port1
        del port2
        del port3

    def test_gen_src_output_ports_getitem_invalid_key(self):
        class MyIter(bt2._UserMessageIterator):
            def __next__(self):
                raise bt2.Stop

        class MySource(bt2._UserSourceComponent,
                       message_iterator_class=MyIter):
            def __init__(comp_self, params):
                comp_self._add_output_port('clear')
                comp_self._add_output_port('print')
                comp_self._add_output_port('insert')

        comp = self._create_comp(MySource)

        with self.assertRaises(KeyError):
            comp.output_ports['hello']

    def test_gen_flt_output_ports_getitem_invalid_key(self):
        class MyIter(bt2._UserMessageIterator):
            def __next__(self):
                raise bt2.Stop

        class MyFilter(bt2._UserFilterComponent,
                       message_iterator_class=MyIter):
            def __init__(comp_self, params):
                comp_self._add_output_port('clear')
                comp_self._add_output_port('print')
                comp_self._add_output_port('insert')

        comp = self._create_comp(MyFilter)

        with self.assertRaises(KeyError):
            comp.output_ports['hello']

    def test_gen_flt_input_ports_getitem_invalid_key(self):
        class MyIter(bt2._UserMessageIterator):
            def __next__(self):
                raise bt2.Stop

        class MyFilter(bt2._UserFilterComponent,
                       message_iterator_class=MyIter):
            def __init__(comp_self, params):
                comp_self._add_input_port('clear')
                comp_self._add_input_port('print')
                comp_self._add_input_port('insert')

        comp = self._create_comp(MyFilter)

        with self.assertRaises(KeyError):
            comp.input_ports['hello']

    def test_gen_sink_input_ports_getitem_invalid_key(self):
        class MySink(bt2._UserSinkComponent):
            def __init__(comp_self, params):
                comp_self._add_input_port('clear')
                comp_self._add_input_port('print')
                comp_self._add_input_port('insert')

                with self.assertRaises(KeyError):
                    comp_self._input_ports['hello']

            def _consume(self):
                pass

        comp = self._create_comp(MySink)

        with self.assertRaises(KeyError):
            comp.input_ports['hello']

    def test_gen_src_output_ports_len(self):
        class MyIter(bt2._UserMessageIterator):
            def __next__(self):
                raise bt2.Stop

        class MySource(bt2._UserSourceComponent,
                       message_iterator_class=MyIter):
            def __init__(comp_self, params):
                comp_self._add_output_port('clear')
                comp_self._add_output_port('print')
                comp_self._add_output_port('insert')

        comp = self._create_comp(MySource)
        self.assertEqual(len(comp.output_ports), 3)

    def test_gen_flt_output_ports_len(self):
        class MyIter(bt2._UserMessageIterator):
            def __next__(self):
                raise bt2.Stop

        class MyFilter(bt2._UserFilterComponent,
                       message_iterator_class=MyIter):
            def __init__(comp_self, params):
                comp_self._add_output_port('clear')
                comp_self._add_output_port('print')
                comp_self._add_output_port('insert')

        comp = self._create_comp(MyFilter)
        self.assertEqual(len(comp.output_ports), 3)

    def test_gen_flt_input_ports_len(self):
        class MyIter(bt2._UserMessageIterator):
            def __next__(self):
                raise bt2.Stop

        class MyFilter(bt2._UserFilterComponent,
                       message_iterator_class=MyIter):
            def __init__(comp_self, params):
                comp_self._add_input_port('clear')
                comp_self._add_input_port('print')
                comp_self._add_input_port('insert')

        comp = self._create_comp(MyFilter)
        self.assertEqual(len(comp.input_ports), 3)

    def test_gen_sink_input_ports_len(self):
        class MySink(bt2._UserSinkComponent):
            def __init__(comp_self, params):
                comp_self._add_input_port('clear')
                comp_self._add_input_port('print')
                comp_self._add_input_port('insert')

            def _consume(self):
                pass

        comp = self._create_comp(MySink)
        self.assertEqual(len(comp.input_ports), 3)

    def test_gen_src_output_ports_iter(self):
        class MyIter(bt2._UserMessageIterator):
            def __next__(self):
                raise bt2.Stop

        port1 = None
        port2 = None
        port3 = None

        class MySource(bt2._UserSourceComponent,
                       message_iterator_class=MyIter):
            def __init__(comp_self, params):
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
        class MyIter(bt2._UserMessageIterator):
            def __next__(self):
                raise bt2.Stop

        port1 = None
        port2 = None
        port3 = None

        class MyFilter(bt2._UserFilterComponent,
                       message_iterator_class=MyIter):
            def __init__(comp_self, params):
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
        class MyIter(bt2._UserMessageIterator):
            def __next__(self):
                raise bt2.Stop

        port1 = None
        port2 = None
        port3 = None

        class MyFilter(bt2._UserFilterComponent,
                       message_iterator_class=MyIter):
            def __init__(comp_self, params):
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
            def __init__(comp_self, params):
                nonlocal port1, port2, port3
                port1 = comp_self._add_input_port('clear')
                port2 = comp_self._add_input_port('print')
                port3 = comp_self._add_input_port('insert')

            def _consume(self):
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
            def __init__(comp_self, params):
                comp_self._add_input_port('clear')

            def _consume(self):
                pass

        comp = self._create_comp(MySink)
        self.assertEqual(comp.input_ports['clear'].name, 'clear')

    def test_component(self):
        class MySink(bt2._UserSinkComponent):
            def __init__(comp_self, params):
                comp_self._add_input_port('clear')

            def _consume(self):
                pass

        comp = self._create_comp(MySink)
        self.assertEqual(comp.input_ports['clear'].component.addr, comp.addr)

    def test_connection_none(self):
        class MySink(bt2._UserSinkComponent):
            def __init__(comp_self, params):
                comp_self._add_input_port('clear')

            def _consume(self):
                pass

        comp = self._create_comp(MySink)
        self.assertIsNone(comp.input_ports['clear'].connection)

    def test_is_connected_false(self):
        class MySink(bt2._UserSinkComponent):
            def __init__(comp_self, params):
                comp_self._add_input_port('clear')

            def _consume(self):
                pass

        comp = self._create_comp(MySink)
        self.assertFalse(comp.input_ports['clear'].is_connected)

    def test_eq(self):
        class MySink(bt2._UserSinkComponent):
            def __init__(comp_self, params):
                comp_self._add_input_port('clear')

            def _consume(self):
                pass

        comp = self._create_comp(MySink)
        self.assertEqual(comp.input_ports['clear'],
                         comp.input_ports['clear'])

    def test_eq_invalid(self):
        class MySink(bt2._UserSinkComponent):
            def __init__(comp_self, params):
                comp_self._add_input_port('clear')

            def _consume(self):
                pass

        comp = self._create_comp(MySink)
        self.assertNotEqual(comp.input_ports['clear'], 23)

    def test_disconnect_no_connection(self):
        class MySink(bt2._UserSinkComponent):
            def __init__(comp_self, params):
                port = comp_self._add_input_port('clear')

            def _consume(self):
                pass

        comp = self._create_comp(MySink)
        comp.input_ports['clear'].disconnect()

    def test_priv_name(self):
        class MySink(bt2._UserSinkComponent):
            def __init__(comp_self, params):
                port = comp_self._add_input_port('clear')
                self.assertEqual(port.name, 'clear')

            def _consume(self):
                pass

        comp = self._create_comp(MySink)

    def test_priv_component(self):
        class MySink(bt2._UserSinkComponent):
            def __init__(comp_self, params):
                port = comp_self._add_input_port('clear')
                self.assertEqual(port.component, comp_self)

            def _consume(self):
                pass

        comp = self._create_comp(MySink)

    def test_priv_connection_none(self):
        class MySink(bt2._UserSinkComponent):
            def __init__(comp_self, params):
                port = comp_self._add_input_port('clear')
                self.assertIsNone(port.connection)

            def _consume(self):
                pass

        comp = self._create_comp(MySink)

    def test_priv_is_connected_false(self):
        class MySink(bt2._UserSinkComponent):
            def __init__(comp_self, params):
                port = comp_self._add_input_port('clear')
                self.assertFalse(port.is_connected)

            def _consume(self):
                pass

        comp = self._create_comp(MySink)

    def test_priv_eq(self):
        class MySink(bt2._UserSinkComponent):
            def __init__(comp_self, params):
                port = comp_self._add_input_port('clear')
                self.assertEqual(port, port)

            def _consume(self):
                pass

        comp = self._create_comp(MySink)

    def test_priv_eq_invalid(self):
        class MySink(bt2._UserSinkComponent):
            def __init__(comp_self, params):
                port = comp_self._add_input_port('clear')
                self.assertNotEqual(port, 23)

            def _consume(self):
                pass

        comp = self._create_comp(MySink)

    def test_priv_disconnect_no_connection(self):
        class MySink(bt2._UserSinkComponent):
            def __init__(comp_self, params):
                port = comp_self._add_input_port('clear')
                port.disconnect()

            def _consume(self):
                pass

        comp = self._create_comp(MySink)

    def test_priv_remove_from_component(self):
        class MySink(bt2._UserSinkComponent):
            def __init__(comp_self, params):
                port = comp_self._add_input_port('clear')
                self.assertEqual(len(comp_self._input_ports), 1)

                try:
                    port.remove_from_component()
                except:
                    import traceback
                    traceback.print_exc()

                self.assertEqual(len(comp_self._input_ports), 0)
                self.assertIsNone(port.component)

            def _consume(self):
                pass

        comp = self._create_comp(MySink)
