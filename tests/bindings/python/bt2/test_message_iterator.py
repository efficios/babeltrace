
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

from bt2 import value
import collections
import unittest
import copy
import bt2


class UserMessageIteratorTestCase(unittest.TestCase):
    @staticmethod
    def _create_graph(src_comp_cls):
        class MySink(bt2._UserSinkComponent):
            def __init__(self, params):
                self._add_input_port('in')

            def _consume(self):
                next(self._msg_iter)

            def _graph_is_configured(self):
                self._msg_iter = self._input_ports['in'].create_message_iterator()

        graph = bt2.Graph()
        src_comp = graph.add_component(src_comp_cls, 'src')
        sink_comp = graph.add_component(MySink, 'sink')
        graph.connect_ports(src_comp.output_ports['out'],
                            sink_comp.input_ports['in'])
        return graph

    def test_init(self):
        the_output_port_from_source = None
        the_output_port_from_iter = None

        class MyIter(bt2._UserMessageIterator):
            def __init__(self, self_port_output):
                nonlocal initialized
                nonlocal the_output_port_from_iter
                initialized = True
                the_output_port_from_iter = self_port_output

        class MySource(bt2._UserSourceComponent,
                       message_iterator_class=MyIter):
            def __init__(self, params):
                nonlocal the_output_port_from_source
                the_output_port_from_source = self._add_output_port('out', 'user data')

        initialized = False
        graph = self._create_graph(MySource)
        graph.run()
        self.assertTrue(initialized)
        self.assertEqual(the_output_port_from_source.addr, the_output_port_from_iter.addr)
        self.assertEqual(the_output_port_from_iter.user_data, 'user data')

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
        graph.run()
        del graph
        self.assertTrue(finalized)

    def test_component(self):
        class MyIter(bt2._UserMessageIterator):
            def __init__(self, self_port_output):
                nonlocal salut
                salut = self._component._salut

        class MySource(bt2._UserSourceComponent,
                       message_iterator_class=MyIter):
            def __init__(self, params):
                self._add_output_port('out')
                self._salut = 23

        salut = None
        graph = self._create_graph(MySource)
        graph.run()
        self.assertEqual(salut, 23)

    def test_addr(self):
        class MyIter(bt2._UserMessageIterator):
            def __init__(self, self_port_output):
                nonlocal addr
                addr = self.addr

        class MySource(bt2._UserSourceComponent,
                       message_iterator_class=MyIter):
            def __init__(self, params):
                self._add_output_port('out')

        addr = None
        graph = self._create_graph(MySource)
        graph.run()
        self.assertIsNotNone(addr)
        self.assertNotEqual(addr, 0)

    # Test that messages returned by _UserMessageIterator.__next__ remain valid
    # and can be re-used.
    def test_reuse_message(self):
        class MyIter(bt2._UserMessageIterator):
            def __init__(self, port):
                tc, sc, ec = port.user_data
                trace = tc()
                stream = trace.create_stream(sc)
                packet = stream.create_packet()

                # This message will be returned twice by __next__.
                event_message = self._create_event_message(ec, packet)

                self._msgs = [
                    self._create_stream_beginning_message(stream),
                    self._create_packet_beginning_message(packet),
                    event_message,
                    event_message,
                ]

            def __next__(self):
                return self._msgs.pop(0)

        class MySource(bt2._UserSourceComponent, message_iterator_class=MyIter):
            def __init__(self, params):
                tc = self._create_trace_class()
                sc = tc.create_stream_class(supports_packets=True)
                ec = sc.create_event_class()
                self._add_output_port('out', (tc, sc, ec))

        graph = bt2.Graph()
        src = graph.add_component(MySource, 'src')
        it = graph.create_output_port_message_iterator(src.output_ports['out'])

        # Skip beginning messages.
        msg = next(it)
        self.assertIsInstance(msg, bt2.message._StreamBeginningMessage)
        msg = next(it)
        self.assertIsInstance(msg, bt2.message._PacketBeginningMessage)

        msg_ev1 = next(it)
        msg_ev2 = next(it)

        self.assertIsInstance(msg_ev1, bt2.message._EventMessage)
        self.assertIsInstance(msg_ev2, bt2.message._EventMessage)
        self.assertEqual(msg_ev1.addr, msg_ev2.addr)

    @staticmethod
    def _setup_seek_beginning_test():
        # Use a source, a filter and an output port iterator.  This allows us
        # to test calling `seek_beginning` on both a _OutputPortMessageIterator
        # and a _UserComponentInputPortMessageIterator, on top of checking that
        # _UserMessageIterator._seek_beginning is properly called.

        class MySourceIter(bt2._UserMessageIterator):
            def __init__(self, port):
                tc, sc, ec = port.user_data
                trace = tc()
                stream = trace.create_stream(sc)
                packet = stream.create_packet()

                self._msgs = [
                    self._create_stream_beginning_message(stream),
                    self._create_packet_beginning_message(packet),
                    self._create_event_message(ec, packet),
                    self._create_event_message(ec, packet),
                    self._create_packet_end_message(packet),
                    self._create_stream_end_message(stream),
                ]
                self._at = 0

            def _seek_beginning(self):
                self._at = 0

            def __next__(self):
                if self._at < len(self._msgs):
                    msg = self._msgs[self._at]
                    self._at += 1
                    return msg
                else:
                    raise StopIteration

        class MySource(bt2._UserSourceComponent,
                       message_iterator_class=MySourceIter):
            def __init__(self, params):
                tc = self._create_trace_class()
                sc = tc.create_stream_class(supports_packets=True)
                ec = sc.create_event_class()

                self._add_output_port('out', (tc, sc, ec))

        class MyFilterIter(bt2._UserMessageIterator):
            def __init__(self, port):
                input_port = port.user_data
                self._upstream_iter = input_port.create_message_iterator()

            def __next__(self):
                return next(self._upstream_iter)

            def _seek_beginning(self):
                self._upstream_iter.seek_beginning()

            @property
            def _can_seek_beginning(self):
                return self._upstream_iter.can_seek_beginning

        class MyFilter(bt2._UserFilterComponent, message_iterator_class=MyFilterIter):
            def __init__(self, params):
                input_port = self._add_input_port('in')
                self._add_output_port('out', input_port)


        graph = bt2.Graph()
        src = graph.add_component(MySource, 'src')
        flt = graph.add_component(MyFilter, 'flt')
        graph.connect_ports(src.output_ports['out'], flt.input_ports['in'])
        it = graph.create_output_port_message_iterator(flt.output_ports['out'])

        return it, MySourceIter

    def test_can_seek_beginning(self):
        it, MySourceIter = self._setup_seek_beginning_test()

        def _can_seek_beginning(self):
            nonlocal can_seek_beginning
            return can_seek_beginning

        MySourceIter._can_seek_beginning = property(_can_seek_beginning)

        can_seek_beginning = True
        self.assertTrue(it.can_seek_beginning)

        can_seek_beginning = False
        self.assertFalse(it.can_seek_beginning)

        # Once can_seek_beginning returns an error, verify that it raises when
        # _can_seek_beginning has/returns the wrong type.

        # Remove the _can_seek_beginning method, we now rely on the presence of
        # a _seek_beginning method to know whether the iterator can seek to
        # beginning or not.
        del MySourceIter._can_seek_beginning
        self.assertTrue(it.can_seek_beginning)

        del MySourceIter._seek_beginning
        self.assertFalse(it.can_seek_beginning)

    def test_seek_beginning(self):
        it, MySourceIter = self._setup_seek_beginning_test()

        msg = next(it)
        self.assertIsInstance(msg, bt2.message._StreamBeginningMessage)
        msg = next(it)
        self.assertIsInstance(msg, bt2.message._PacketBeginningMessage)

        it.seek_beginning()

        msg = next(it)
        self.assertIsInstance(msg, bt2.message._StreamBeginningMessage)

        # Verify that we can seek beginning after having reached the end.
        #
        # It currently does not work to seek an output port message iterator
        # once it's ended, but we should eventually make it work and uncomment
        # the following snippet.
        #
        # try:
        #    while True:
        #        next(it)
        # except bt2.Stop:
        #    pass
        #
        # it.seek_beginning()
        # msg = next(it)
        # self.assertIsInstance(msg, bt2.message._StreamBeginningMessage)

    def test_seek_beginning_user_error(self):
        it, MySourceIter = self._setup_seek_beginning_test()

        def _seek_beginning_error(self):
           raise ValueError('ouch')

        MySourceIter._seek_beginning = _seek_beginning_error

        with self.assertRaises(bt2.Error):
            it.seek_beginning()



class OutputPortMessageIteratorTestCase(unittest.TestCase):
    def test_component(self):
        class MyIter(bt2._UserMessageIterator):
            def __init__(self, self_port_output):
                self._at = 0

            def __next__(self):
                if self._at == 7:
                    raise bt2.Stop

                if self._at == 0:
                    msg = self._create_stream_beginning_message(test_obj._stream)
                elif self._at == 1:
                    msg = self._create_packet_beginning_message(test_obj._packet)
                elif self._at == 5:
                    msg = self._create_packet_end_message(test_obj._packet)
                elif self._at == 6:
                    msg = self._create_stream_end_message(test_obj._stream)
                else:
                    msg = self._create_event_message(test_obj._event_class, test_obj._packet)
                    msg.event.payload_field['my_int'] = self._at * 3

                self._at += 1
                return msg

        class MySource(bt2._UserSourceComponent,
                       message_iterator_class=MyIter):
            def __init__(self, params):
                self._add_output_port('out')

                trace_class = self._create_trace_class()
                stream_class = trace_class.create_stream_class(supports_packets=True)

                # Create payload field class
                my_int_ft = trace_class.create_signed_integer_field_class(32)
                payload_ft = trace_class.create_structure_field_class()
                payload_ft += [
                    ('my_int', my_int_ft),
                ]

                event_class = stream_class.create_event_class(name='salut', payload_field_class=payload_ft)

                trace = trace_class()
                stream = trace.create_stream(stream_class)
                packet = stream.create_packet()

                test_obj._event_class = event_class
                test_obj._stream = stream
                test_obj._packet = packet

        test_obj = self
        graph = bt2.Graph()
        src = graph.add_component(MySource, 'src')
        msg_iter = graph.create_output_port_message_iterator(src.output_ports['out'])

        for at, msg in enumerate(msg_iter):
            if at == 0:
                self.assertIsInstance(msg, bt2.message._StreamBeginningMessage)
            elif at == 1:
                self.assertIsInstance(msg, bt2.message._PacketBeginningMessage)
            elif at == 5:
                self.assertIsInstance(msg, bt2.message._PacketEndMessage)
            elif at == 6:
                self.assertIsInstance(msg, bt2.message._StreamEndMessage)
            else:
                self.assertIsInstance(msg, bt2.message._EventMessage)
                self.assertEqual(msg.event.cls.name, 'salut')
                field = msg.event.payload_field['my_int']
                self.assertEqual(field, at * 3)

if __name__ == '__main__':
    unittest.main()
