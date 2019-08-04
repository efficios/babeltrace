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
import sys
from utils import TestOutputPortMessageIterator


class UserMessageIteratorTestCase(unittest.TestCase):
    @staticmethod
    def _create_graph(src_comp_cls, flt_comp_cls=None):
        class MySink(bt2._UserSinkComponent):
            def __init__(self, params, obj):
                self._add_input_port('in')

            def _user_consume(self):
                next(self._msg_iter)

            def _user_graph_is_configured(self):
                self._msg_iter = self._create_input_port_message_iterator(
                    self._input_ports['in']
                )

        graph = bt2.Graph()
        src_comp = graph.add_component(src_comp_cls, 'src')

        if flt_comp_cls is not None:
            flt_comp = graph.add_component(flt_comp_cls, 'flt')

        sink_comp = graph.add_component(MySink, 'sink')

        if flt_comp_cls is not None:
            assert flt_comp is not None
            graph.connect_ports(
                src_comp.output_ports['out'], flt_comp.input_ports['in']
            )
            out_port = flt_comp.output_ports['out']
        else:
            out_port = src_comp.output_ports['out']

        graph.connect_ports(out_port, sink_comp.input_ports['in'])
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

        class MySource(bt2._UserSourceComponent, message_iterator_class=MyIter):
            def __init__(self, params, obj):
                nonlocal the_output_port_from_source
                the_output_port_from_source = self._add_output_port('out', 'user data')

        initialized = False
        graph = self._create_graph(MySource)
        graph.run()
        self.assertTrue(initialized)
        self.assertEqual(
            the_output_port_from_source.addr, the_output_port_from_iter.addr
        )
        self.assertEqual(the_output_port_from_iter.user_data, 'user data')

    def test_create_from_message_iterator(self):
        class MySourceIter(bt2._UserMessageIterator):
            def __init__(self, self_port_output):
                nonlocal src_iter_initialized
                src_iter_initialized = True

        class MySource(bt2._UserSourceComponent, message_iterator_class=MySourceIter):
            def __init__(self, params, obj):
                self._add_output_port('out')

        class MyFilterIter(bt2._UserMessageIterator):
            def __init__(self, self_port_output):
                nonlocal flt_iter_initialized
                flt_iter_initialized = True
                self._up_iter = self._create_input_port_message_iterator(
                    self._component._input_ports['in']
                )

            def __next__(self):
                return next(self._up_iter)

        class MyFilter(bt2._UserFilterComponent, message_iterator_class=MyFilterIter):
            def __init__(self, params, obj):
                self._add_input_port('in')
                self._add_output_port('out')

        src_iter_initialized = False
        flt_iter_initialized = False
        graph = self._create_graph(MySource, MyFilter)
        graph.run()
        self.assertTrue(src_iter_initialized)
        self.assertTrue(flt_iter_initialized)

    def test_finalize(self):
        class MyIter(bt2._UserMessageIterator):
            def _user_finalize(self):
                nonlocal finalized
                finalized = True

        class MySource(bt2._UserSourceComponent, message_iterator_class=MyIter):
            def __init__(self, params, obj):
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

        class MySource(bt2._UserSourceComponent, message_iterator_class=MyIter):
            def __init__(self, params, obj):
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

        class MySource(bt2._UserSourceComponent, message_iterator_class=MyIter):
            def __init__(self, params, obj):
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
            def __init__(self, params, obj):
                tc = self._create_trace_class()
                sc = tc.create_stream_class(supports_packets=True)
                ec = sc.create_event_class()
                self._add_output_port('out', (tc, sc, ec))

        graph = bt2.Graph()
        src = graph.add_component(MySource, 'src')
        it = TestOutputPortMessageIterator(graph, src.output_ports['out'])

        # Skip beginning messages.
        msg = next(it)
        self.assertIsInstance(msg, bt2._StreamBeginningMessage)
        msg = next(it)
        self.assertIsInstance(msg, bt2._PacketBeginningMessage)

        msg_ev1 = next(it)
        msg_ev2 = next(it)

        self.assertIsInstance(msg_ev1, bt2._EventMessage)
        self.assertIsInstance(msg_ev2, bt2._EventMessage)
        self.assertEqual(msg_ev1.addr, msg_ev2.addr)

    @staticmethod
    def _setup_seek_beginning_test(sink_cls):
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

            def _user_seek_beginning(self):
                self._at = 0

            def __next__(self):
                if self._at < len(self._msgs):
                    msg = self._msgs[self._at]
                    self._at += 1
                    return msg
                else:
                    raise StopIteration

        class MySource(bt2._UserSourceComponent, message_iterator_class=MySourceIter):
            def __init__(self, params, obj):
                tc = self._create_trace_class()
                sc = tc.create_stream_class(supports_packets=True)
                ec = sc.create_event_class()

                self._add_output_port('out', (tc, sc, ec))

        class MyFilterIter(bt2._UserMessageIterator):
            def __init__(self, port):
                input_port = port.user_data
                self._upstream_iter = self._create_input_port_message_iterator(
                    input_port
                )

            def __next__(self):
                return next(self._upstream_iter)

            def _user_seek_beginning(self):
                self._upstream_iter.seek_beginning()

            @property
            def _user_can_seek_beginning(self):
                return self._upstream_iter.can_seek_beginning

        class MyFilter(bt2._UserFilterComponent, message_iterator_class=MyFilterIter):
            def __init__(self, params, obj):
                input_port = self._add_input_port('in')
                self._add_output_port('out', input_port)

        graph = bt2.Graph()
        src = graph.add_component(MySource, 'src')
        flt = graph.add_component(MyFilter, 'flt')
        sink = graph.add_component(sink_cls, 'sink')
        graph.connect_ports(src.output_ports['out'], flt.input_ports['in'])
        graph.connect_ports(flt.output_ports['out'], sink.input_ports['in'])
        return MySourceIter, graph

    def test_can_seek_beginning(self):
        class MySink(bt2._UserSinkComponent):
            def __init__(self, params, obj):
                self._add_input_port('in')

            def _user_graph_is_configured(self):
                self._msg_iter = self._create_input_port_message_iterator(
                    self._input_ports['in']
                )

            def _user_consume(self):
                nonlocal can_seek_beginning
                can_seek_beginning = self._msg_iter.can_seek_beginning

        MySourceIter, graph = self._setup_seek_beginning_test(MySink)

        def _user_can_seek_beginning(self):
            nonlocal input_port_iter_can_seek_beginning
            return input_port_iter_can_seek_beginning

        MySourceIter._user_can_seek_beginning = property(_user_can_seek_beginning)

        input_port_iter_can_seek_beginning = True
        can_seek_beginning = None
        graph.run_once()
        self.assertTrue(can_seek_beginning)

        input_port_iter_can_seek_beginning = False
        can_seek_beginning = None
        graph.run_once()
        self.assertFalse(can_seek_beginning)

        # Once can_seek_beginning returns an error, verify that it raises when
        # _can_seek_beginning has/returns the wrong type.

        # Remove the _can_seek_beginning method, we now rely on the presence of
        # a _seek_beginning method to know whether the iterator can seek to
        # beginning or not.
        del MySourceIter._user_can_seek_beginning
        can_seek_beginning = None
        graph.run_once()
        self.assertTrue(can_seek_beginning)

        del MySourceIter._user_seek_beginning
        can_seek_beginning = None
        graph.run_once()
        self.assertFalse(can_seek_beginning)

    def test_seek_beginning(self):
        class MySink(bt2._UserSinkComponent):
            def __init__(self, params, obj):
                self._add_input_port('in')

            def _user_graph_is_configured(self):
                self._msg_iter = self._create_input_port_message_iterator(
                    self._input_ports['in']
                )

            def _user_consume(self):
                nonlocal do_seek_beginning
                nonlocal msg

                if do_seek_beginning:
                    self._msg_iter.seek_beginning()
                    return

                msg = next(self._msg_iter)

        do_seek_beginning = False
        msg = None
        MySourceIter, graph = self._setup_seek_beginning_test(MySink)
        graph.run_once()
        self.assertIsInstance(msg, bt2._StreamBeginningMessage)
        graph.run_once()
        self.assertIsInstance(msg, bt2._PacketBeginningMessage)
        do_seek_beginning = True
        graph.run_once()
        do_seek_beginning = False
        graph.run_once()
        self.assertIsInstance(msg, bt2._StreamBeginningMessage)

    def test_seek_beginning_user_error(self):
        class MySink(bt2._UserSinkComponent):
            def __init__(self, params, obj):
                self._add_input_port('in')

            def _user_graph_is_configured(self):
                self._msg_iter = self._create_input_port_message_iterator(
                    self._input_ports['in']
                )

            def _user_consume(self):
                self._msg_iter.seek_beginning()

        MySourceIter, graph = self._setup_seek_beginning_test(MySink)

        def _user_seek_beginning_error(self):
            raise ValueError('ouch')

        MySourceIter._user_seek_beginning = _user_seek_beginning_error

        with self.assertRaises(bt2._Error):
            graph.run_once()

    # Try consuming many times from an iterator that always returns TryAgain.
    # This verifies that we are not missing an incref of Py_None, making the
    # refcount of Py_None reach 0.
    def test_try_again_many_times(self):
        class MyIter(bt2._UserMessageIterator):
            def __next__(self):
                raise bt2.TryAgain

        class MySource(bt2._UserSourceComponent, message_iterator_class=MyIter):
            def __init__(self, params, obj):
                self._add_output_port('out')

        graph = bt2.Graph()
        src = graph.add_component(MySource, 'src')
        it = TestOutputPortMessageIterator(graph, src.output_ports['out'])

        # Three times the initial ref count of `None` iterations should
        # be enough to catch the bug even if there are small differences
        # between configurations.
        none_ref_count = sys.getrefcount(None) * 3

        for i in range(none_ref_count):
            with self.assertRaises(bt2.TryAgain):
                next(it)


if __name__ == '__main__':
    unittest.main()
