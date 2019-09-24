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
import sys
from utils import TestOutputPortMessageIterator
from bt2 import port as bt2_port
from bt2 import message_iterator as bt2_message_iterator


class SimpleSink(bt2._UserSinkComponent):
    # Straightforward sink that creates one input port (`in`) and consumes from
    # it.

    def __init__(self, config, params, obj):
        self._add_input_port('in')

    def _user_consume(self):
        next(self._msg_iter)

    def _user_graph_is_configured(self):
        self._msg_iter = self._create_input_port_message_iterator(
            self._input_ports['in']
        )


def _create_graph(src_comp_cls, sink_comp_cls, flt_comp_cls=None):
    graph = bt2.Graph()

    src_comp = graph.add_component(src_comp_cls, 'src')
    sink_comp = graph.add_component(sink_comp_cls, 'sink')

    if flt_comp_cls is not None:
        flt_comp = graph.add_component(flt_comp_cls, 'flt')
        graph.connect_ports(src_comp.output_ports['out'], flt_comp.input_ports['in'])
        graph.connect_ports(flt_comp.output_ports['out'], sink_comp.input_ports['in'])
    else:
        graph.connect_ports(src_comp.output_ports['out'], sink_comp.input_ports['in'])

    return graph


class UserMessageIteratorTestCase(unittest.TestCase):
    def test_init(self):
        the_output_port_from_source = None
        the_output_port_from_iter = None

        class MyIter(bt2._UserMessageIterator):
            def __init__(self, config, self_port_output):
                nonlocal initialized
                nonlocal the_output_port_from_iter
                initialized = True
                the_output_port_from_iter = self_port_output

        class MySource(bt2._UserSourceComponent, message_iterator_class=MyIter):
            def __init__(self, config, params, obj):
                nonlocal the_output_port_from_source
                the_output_port_from_source = self._add_output_port('out', 'user data')

        initialized = False
        graph = _create_graph(MySource, SimpleSink)
        graph.run()
        self.assertTrue(initialized)
        self.assertEqual(
            the_output_port_from_source.addr, the_output_port_from_iter.addr
        )
        self.assertEqual(the_output_port_from_iter.user_data, 'user data')

    def test_create_from_message_iterator(self):
        class MySourceIter(bt2._UserMessageIterator):
            def __init__(self, config, self_port_output):
                nonlocal src_iter_initialized
                src_iter_initialized = True

        class MySource(bt2._UserSourceComponent, message_iterator_class=MySourceIter):
            def __init__(self, config, params, obj):
                self._add_output_port('out')

        class MyFilterIter(bt2._UserMessageIterator):
            def __init__(self, config, self_port_output):
                nonlocal flt_iter_initialized
                flt_iter_initialized = True
                self._up_iter = self._create_input_port_message_iterator(
                    self._component._input_ports['in']
                )

            def __next__(self):
                return next(self._up_iter)

        class MyFilter(bt2._UserFilterComponent, message_iterator_class=MyFilterIter):
            def __init__(self, config, params, obj):
                self._add_input_port('in')
                self._add_output_port('out')

        src_iter_initialized = False
        flt_iter_initialized = False
        graph = _create_graph(MySource, SimpleSink, MyFilter)
        graph.run()
        self.assertTrue(src_iter_initialized)
        self.assertTrue(flt_iter_initialized)

    def test_create_user_error(self):
        # This tests both error handling by
        # _UserSinkComponent._create_input_port_message_iterator
        # and _UserMessageIterator._create_input_port_message_iterator, as they
        # are both used in the graph.
        class MySourceIter(bt2._UserMessageIterator):
            def __init__(self, config, self_port_output):
                raise ValueError('Very bad error')

        class MySource(bt2._UserSourceComponent, message_iterator_class=MySourceIter):
            def __init__(self, config, params, obj):
                self._add_output_port('out')

        class MyFilterIter(bt2._UserMessageIterator):
            def __init__(self, config, self_port_output):
                # This is expected to raise because of the error in
                # MySourceIter.__init__.
                self._create_input_port_message_iterator(
                    self._component._input_ports['in']
                )

        class MyFilter(bt2._UserFilterComponent, message_iterator_class=MyFilterIter):
            def __init__(self, config, params, obj):
                self._add_input_port('in')
                self._add_output_port('out')

        graph = _create_graph(MySource, SimpleSink, MyFilter)

        with self.assertRaises(bt2._Error) as ctx:
            graph.run()

        exc = ctx.exception
        cause = exc[0]

        self.assertIsInstance(cause, bt2._MessageIteratorErrorCause)
        self.assertEqual(cause.component_name, 'src')
        self.assertEqual(cause.component_output_port_name, 'out')
        self.assertIn('ValueError: Very bad error', cause.message)

    def test_finalize(self):
        class MyIter(bt2._UserMessageIterator):
            def _user_finalize(self):
                nonlocal finalized
                finalized = True

        class MySource(bt2._UserSourceComponent, message_iterator_class=MyIter):
            def __init__(self, config, params, obj):
                self._add_output_port('out')

        finalized = False
        graph = _create_graph(MySource, SimpleSink)
        graph.run()
        del graph
        self.assertTrue(finalized)

    def test_config_parameter(self):
        class MyIter(bt2._UserMessageIterator):
            def __init__(self, config, port):
                nonlocal config_type
                config_type = type(config)

        class MySource(bt2._UserSourceComponent, message_iterator_class=MyIter):
            def __init__(self, config, params, obj):
                self._add_output_port('out')

        config_type = None
        graph = _create_graph(MySource, SimpleSink)
        graph.run()
        self.assertIs(config_type, bt2_message_iterator._MessageIteratorConfiguration)

    def _test_config_can_seek_forward(self, set_can_seek_forward):
        class MyIter(bt2._UserMessageIterator):
            def __init__(self, config, port):
                if set_can_seek_forward:
                    config.can_seek_forward = True

        class MySource(bt2._UserSourceComponent, message_iterator_class=MyIter):
            def __init__(self, config, params, obj):
                self._add_output_port('out')

        class MySink(bt2._UserSinkComponent):
            def __init__(self, config, params, obj):
                self._add_input_port('in')

            def _user_graph_is_configured(self):
                self._msg_iter = self._create_input_port_message_iterator(
                    self._input_ports['in']
                )

            def _user_consume(self):
                nonlocal can_seek_forward
                can_seek_forward = self._msg_iter.can_seek_forward

        can_seek_forward = None
        graph = _create_graph(MySource, MySink)
        graph.run_once()
        self.assertIs(can_seek_forward, set_can_seek_forward)

    def test_config_can_seek_forward_default(self):
        self._test_config_can_seek_forward(False)

    def test_config_can_seek_forward(self):
        self._test_config_can_seek_forward(True)

    def test_config_can_seek_forward_wrong_type(self):
        class MyIter(bt2._UserMessageIterator):
            def __init__(self, config, port):
                config.can_seek_forward = 1

        class MySource(bt2._UserSourceComponent, message_iterator_class=MyIter):
            def __init__(self, config, params, obj):
                self._add_output_port('out')

        graph = _create_graph(MySource, SimpleSink)
        with self.assertRaises(bt2._Error) as ctx:
            graph.run()

        root_cause = ctx.exception[0]
        self.assertIn("TypeError: 'int' is not a 'bool' object", root_cause.message)

    def test_component(self):
        class MyIter(bt2._UserMessageIterator):
            def __init__(self, config, self_port_output):
                nonlocal salut
                salut = self._component._salut

        class MySource(bt2._UserSourceComponent, message_iterator_class=MyIter):
            def __init__(self, config, params, obj):
                self._add_output_port('out')
                self._salut = 23

        salut = None
        graph = _create_graph(MySource, SimpleSink)
        graph.run()
        self.assertEqual(salut, 23)

    def test_port(self):
        class MyIter(bt2._UserMessageIterator):
            def __init__(self_iter, config, self_port_output):
                nonlocal called
                called = True
                port = self_iter._port
                self.assertIs(type(self_port_output), bt2_port._UserComponentOutputPort)
                self.assertIs(type(port), bt2_port._UserComponentOutputPort)
                self.assertEqual(self_port_output.addr, port.addr)

        class MySource(bt2._UserSourceComponent, message_iterator_class=MyIter):
            def __init__(self, config, params, obj):
                self._add_output_port('out')

        called = False
        graph = _create_graph(MySource, SimpleSink)
        graph.run()
        self.assertTrue(called)

    def test_addr(self):
        class MyIter(bt2._UserMessageIterator):
            def __init__(self, config, self_port_output):
                nonlocal addr
                addr = self.addr

        class MySource(bt2._UserSourceComponent, message_iterator_class=MyIter):
            def __init__(self, config, params, obj):
                self._add_output_port('out')

        addr = None
        graph = _create_graph(MySource, SimpleSink)
        graph.run()
        self.assertIsNotNone(addr)
        self.assertNotEqual(addr, 0)

    # Test that messages returned by _UserMessageIterator.__next__ remain valid
    # and can be re-used.
    def test_reuse_message(self):
        class MyIter(bt2._UserMessageIterator):
            def __init__(self, config, port):
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
            def __init__(self, config, params, obj):
                tc = self._create_trace_class()
                sc = tc.create_stream_class(supports_packets=True)
                ec = sc.create_event_class()
                self._add_output_port('out', (tc, sc, ec))

        graph = bt2.Graph()
        src = graph.add_component(MySource, 'src')
        it = TestOutputPortMessageIterator(graph, src.output_ports['out'])

        # Skip beginning messages.
        msg = next(it)
        self.assertIs(type(msg), bt2._StreamBeginningMessageConst)
        msg = next(it)
        self.assertIs(type(msg), bt2._PacketBeginningMessageConst)

        msg_ev1 = next(it)
        msg_ev2 = next(it)

        self.assertIs(type(msg_ev1), bt2._EventMessageConst)
        self.assertIs(type(msg_ev2), bt2._EventMessageConst)
        self.assertEqual(msg_ev1.addr, msg_ev2.addr)

    # Try consuming many times from an iterator that always returns TryAgain.
    # This verifies that we are not missing an incref of Py_None, making the
    # refcount of Py_None reach 0.
    def test_try_again_many_times(self):
        class MyIter(bt2._UserMessageIterator):
            def __next__(self):
                raise bt2.TryAgain

        class MySource(bt2._UserSourceComponent, message_iterator_class=MyIter):
            def __init__(self, config, params, obj):
                self._add_output_port('out')

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

            def _user_can_seek_beginning(self):
                return self._upstream_iter.can_seek_beginning()

        class MyFilter(bt2._UserFilterComponent, message_iterator_class=MyFilterIter):
            def __init__(self, config, params, obj):
                input_port = self._add_input_port('in')
                self._add_output_port('out', input_port)

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


def _setup_seek_test(
    sink_cls,
    user_seek_beginning=None,
    user_can_seek_beginning=None,
    user_seek_ns_from_origin=None,
    user_can_seek_ns_from_origin=None,
):
    class MySourceIter(bt2._UserMessageIterator):
        def __init__(self, config, port):
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

        def __next__(self):
            if self._at < len(self._msgs):
                msg = self._msgs[self._at]
                self._at += 1
                return msg
            else:
                raise StopIteration

    if user_seek_beginning is not None:
        MySourceIter._user_seek_beginning = user_seek_beginning

    if user_can_seek_beginning is not None:
        MySourceIter._user_can_seek_beginning = user_can_seek_beginning

    if user_seek_ns_from_origin is not None:
        MySourceIter._user_seek_ns_from_origin = user_seek_ns_from_origin

    if user_can_seek_ns_from_origin is not None:
        MySourceIter._user_can_seek_ns_from_origin = user_can_seek_ns_from_origin

    class MySource(bt2._UserSourceComponent, message_iterator_class=MySourceIter):
        def __init__(self, config, params, obj):
            tc = self._create_trace_class()
            sc = tc.create_stream_class(supports_packets=True)
            ec = sc.create_event_class()

            self._add_output_port('out', (tc, sc, ec))

    class MyFilterIter(bt2._UserMessageIterator):
        def __init__(self, config, port):
            self._upstream_iter = self._create_input_port_message_iterator(
                self._component._input_ports['in']
            )

        def __next__(self):
            return next(self._upstream_iter)

        def _user_can_seek_beginning(self):
            return self._upstream_iter.can_seek_beginning()

        def _user_seek_beginning(self):
            self._upstream_iter.seek_beginning()

        def _user_can_seek_ns_from_origin(self, ns_from_origin):
            return self._upstream_iter.can_seek_ns_from_origin(ns_from_origin)

        def _user_seek_ns_from_origin(self, ns_from_origin):
            self._upstream_iter.seek_ns_from_origin(ns_from_origin)

    class MyFilter(bt2._UserFilterComponent, message_iterator_class=MyFilterIter):
        def __init__(self, config, params, obj):
            self._add_input_port('in')
            self._add_output_port('out')

    return _create_graph(MySource, sink_cls, flt_comp_cls=MyFilter)


class UserMessageIteratorSeekBeginningTestCase(unittest.TestCase):
    def test_can_seek_beginning(self):
        class MySink(bt2._UserSinkComponent):
            def __init__(self, config, params, obj):
                self._add_input_port('in')

            def _user_graph_is_configured(self):
                self._msg_iter = self._create_input_port_message_iterator(
                    self._input_ports['in']
                )

            def _user_consume(self):
                nonlocal can_seek_beginning
                can_seek_beginning = self._msg_iter.can_seek_beginning()

        def _user_can_seek_beginning(self):
            nonlocal input_port_iter_can_seek_beginning
            return input_port_iter_can_seek_beginning

        graph = _setup_seek_test(
            MySink, user_can_seek_beginning=_user_can_seek_beginning
        )

        input_port_iter_can_seek_beginning = True
        can_seek_beginning = None
        graph.run_once()
        self.assertIs(can_seek_beginning, True)

        input_port_iter_can_seek_beginning = False
        can_seek_beginning = None
        graph.run_once()
        self.assertIs(can_seek_beginning, False)

    def test_no_can_seek_beginning_with_seek_beginning(self):
        # Test an iterator without a _user_can_seek_beginning method, but with
        # a _user_seek_beginning method.
        class MySink(bt2._UserSinkComponent):
            def __init__(self, config, params, obj):
                self._add_input_port('in')

            def _user_graph_is_configured(self):
                self._msg_iter = self._create_input_port_message_iterator(
                    self._input_ports['in']
                )

            def _user_consume(self):
                nonlocal can_seek_beginning
                can_seek_beginning = self._msg_iter.can_seek_beginning()

        def _user_seek_beginning(self):
            pass

        graph = _setup_seek_test(MySink, user_seek_beginning=_user_seek_beginning)
        can_seek_beginning = None
        graph.run_once()
        self.assertIs(can_seek_beginning, True)

    def test_no_can_seek_beginning(self):
        # Test an iterator without a _user_can_seek_beginning method, without
        # a _user_seek_beginning method.
        class MySink(bt2._UserSinkComponent):
            def __init__(self, config, params, obj):
                self._add_input_port('in')

            def _user_graph_is_configured(self):
                self._msg_iter = self._create_input_port_message_iterator(
                    self._input_ports['in']
                )

            def _user_consume(self):
                nonlocal can_seek_beginning
                can_seek_beginning = self._msg_iter.can_seek_beginning()

        graph = _setup_seek_test(MySink)
        can_seek_beginning = None
        graph.run_once()
        self.assertIs(can_seek_beginning, False)

    def test_can_seek_beginning_user_error(self):
        class MySink(bt2._UserSinkComponent):
            def __init__(self, config, params, obj):
                self._add_input_port('in')

            def _user_graph_is_configured(self):
                self._msg_iter = self._create_input_port_message_iterator(
                    self._input_ports['in']
                )

            def _user_consume(self):
                # This is expected to raise.
                self._msg_iter.can_seek_beginning()

        def _user_can_seek_beginning(self):
            raise ValueError('moustiquaire')

        graph = _setup_seek_test(
            MySink, user_can_seek_beginning=_user_can_seek_beginning
        )

        with self.assertRaises(bt2._Error) as ctx:
            graph.run_once()

        cause = ctx.exception[0]
        self.assertIn('ValueError: moustiquaire', cause.message)

    def test_can_seek_beginning_wrong_return_value(self):
        class MySink(bt2._UserSinkComponent):
            def __init__(self, config, params, obj):
                self._add_input_port('in')

            def _user_graph_is_configured(self):
                self._msg_iter = self._create_input_port_message_iterator(
                    self._input_ports['in']
                )

            def _user_consume(self):
                # This is expected to raise.
                self._msg_iter.can_seek_beginning()

        def _user_can_seek_beginning(self):
            return 'Amqui'

        graph = _setup_seek_test(
            MySink, user_can_seek_beginning=_user_can_seek_beginning
        )

        with self.assertRaises(bt2._Error) as ctx:
            graph.run_once()

        cause = ctx.exception[0]
        self.assertIn("TypeError: 'str' is not a 'bool' object", cause.message)

    def test_seek_beginning(self):
        class MySink(bt2._UserSinkComponent):
            def __init__(self, config, params, obj):
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

        def _user_seek_beginning(self):
            self._at = 0

        msg = None
        graph = _setup_seek_test(MySink, user_seek_beginning=_user_seek_beginning)

        # Consume message.
        do_seek_beginning = False
        graph.run_once()
        self.assertIs(type(msg), bt2._StreamBeginningMessageConst)

        # Consume message.
        graph.run_once()
        self.assertIs(type(msg), bt2._PacketBeginningMessageConst)

        # Seek beginning.
        do_seek_beginning = True
        graph.run_once()

        # Consume message.
        do_seek_beginning = False
        graph.run_once()
        self.assertIs(type(msg), bt2._StreamBeginningMessageConst)

    def test_seek_beginning_user_error(self):
        class MySink(bt2._UserSinkComponent):
            def __init__(self, config, params, obj):
                self._add_input_port('in')

            def _user_graph_is_configured(self):
                self._msg_iter = self._create_input_port_message_iterator(
                    self._input_ports['in']
                )

            def _user_consume(self):
                self._msg_iter.seek_beginning()

        def _user_seek_beginning(self):
            raise ValueError('ouch')

        graph = _setup_seek_test(MySink, user_seek_beginning=_user_seek_beginning)

        with self.assertRaises(bt2._Error):
            graph.run_once()


class UserMessageIteratorSeekNsFromOriginTestCase(unittest.TestCase):
    def test_can_seek_ns_from_origin(self):
        class MySink(bt2._UserSinkComponent):
            def __init__(self, config, params, obj):
                self._add_input_port('in')

            def _user_graph_is_configured(self):
                self._msg_iter = self._create_input_port_message_iterator(
                    self._input_ports['in']
                )

            def _user_consume(self):
                nonlocal can_seek_ns_from_origin
                nonlocal test_ns_from_origin
                can_seek_ns_from_origin = self._msg_iter.can_seek_ns_from_origin(
                    test_ns_from_origin
                )

        def _user_can_seek_ns_from_origin(iter_self, ns_from_origin):
            nonlocal input_port_iter_can_seek_ns_from_origin
            nonlocal test_ns_from_origin
            self.assertEqual(ns_from_origin, test_ns_from_origin)
            return input_port_iter_can_seek_ns_from_origin

        graph = _setup_seek_test(
            MySink, user_can_seek_ns_from_origin=_user_can_seek_ns_from_origin
        )

        input_port_iter_can_seek_ns_from_origin = True
        can_seek_ns_from_origin = None
        test_ns_from_origin = 1
        graph.run_once()
        self.assertIs(can_seek_ns_from_origin, True)

        input_port_iter_can_seek_ns_from_origin = False
        can_seek_ns_from_origin = None
        test_ns_from_origin = 2
        graph.run_once()
        self.assertIs(can_seek_ns_from_origin, False)

    def test_no_can_seek_ns_from_origin_with_seek_ns_from_origin(self):
        # Test an iterator without a _user_can_seek_ns_from_origin method, but
        # with a _user_seek_ns_from_origin method.
        class MySink(bt2._UserSinkComponent):
            def __init__(self, config, params, obj):
                self._add_input_port('in')

            def _user_graph_is_configured(self):
                self._msg_iter = self._create_input_port_message_iterator(
                    self._input_ports['in']
                )

            def _user_consume(self):
                nonlocal can_seek_ns_from_origin
                nonlocal test_ns_from_origin
                can_seek_ns_from_origin = self._msg_iter.can_seek_ns_from_origin(
                    test_ns_from_origin
                )

        def _user_seek_ns_from_origin(self):
            pass

        graph = _setup_seek_test(
            MySink, user_seek_ns_from_origin=_user_seek_ns_from_origin
        )
        can_seek_ns_from_origin = None
        test_ns_from_origin = 2
        graph.run_once()
        self.assertIs(can_seek_ns_from_origin, True)

    def test_no_can_seek_ns_from_origin_with_seek_beginning(self):
        # Test an iterator without a _user_can_seek_ns_from_origin method, but
        # with a _user_seek_beginning method.
        class MySink(bt2._UserSinkComponent):
            def __init__(self, config, params, obj):
                self._add_input_port('in')

            def _user_graph_is_configured(self):
                self._msg_iter = self._create_input_port_message_iterator(
                    self._input_ports['in']
                )

            def _user_consume(self):
                nonlocal can_seek_ns_from_origin
                nonlocal test_ns_from_origin
                can_seek_ns_from_origin = self._msg_iter.can_seek_ns_from_origin(
                    test_ns_from_origin
                )

        def _user_seek_beginning(self):
            pass

        graph = _setup_seek_test(MySink, user_seek_beginning=_user_seek_beginning)
        can_seek_ns_from_origin = None
        test_ns_from_origin = 2
        graph.run_once()
        self.assertIs(can_seek_ns_from_origin, True)

    def test_no_can_seek_ns_from_origin(self):
        # Test an iterator without a _user_can_seek_ns_from_origin method
        # and no other related method.
        class MySink(bt2._UserSinkComponent):
            def __init__(self, config, params, obj):
                self._add_input_port('in')

            def _user_graph_is_configured(self):
                self._msg_iter = self._create_input_port_message_iterator(
                    self._input_ports['in']
                )

            def _user_consume(self):
                nonlocal can_seek_ns_from_origin
                nonlocal test_ns_from_origin
                can_seek_ns_from_origin = self._msg_iter.can_seek_ns_from_origin(
                    test_ns_from_origin
                )

        graph = _setup_seek_test(MySink)
        can_seek_ns_from_origin = None
        test_ns_from_origin = 2
        graph.run_once()
        self.assertIs(can_seek_ns_from_origin, False)

    def test_can_seek_ns_from_origin_user_error(self):
        class MySink(bt2._UserSinkComponent):
            def __init__(self, config, params, obj):
                self._add_input_port('in')

            def _user_graph_is_configured(self):
                self._msg_iter = self._create_input_port_message_iterator(
                    self._input_ports['in']
                )

            def _user_consume(self):
                # This is expected to raise.
                self._msg_iter.can_seek_ns_from_origin(2)

        def _user_can_seek_ns_from_origin(self, ns_from_origin):
            raise ValueError('Joutel')

        graph = _setup_seek_test(
            MySink, user_can_seek_ns_from_origin=_user_can_seek_ns_from_origin
        )

        with self.assertRaises(bt2._Error) as ctx:
            graph.run_once()

        cause = ctx.exception[0]
        self.assertIn('ValueError: Joutel', cause.message)

    def test_can_seek_ns_from_origin_wrong_return_value(self):
        class MySink(bt2._UserSinkComponent):
            def __init__(self, config, params, obj):
                self._add_input_port('in')

            def _user_graph_is_configured(self):
                self._msg_iter = self._create_input_port_message_iterator(
                    self._input_ports['in']
                )

            def _user_consume(self):
                # This is expected to raise.
                self._msg_iter.can_seek_ns_from_origin(2)

        def _user_can_seek_ns_from_origin(self, ns_from_origin):
            return 'Nitchequon'

        graph = _setup_seek_test(
            MySink, user_can_seek_ns_from_origin=_user_can_seek_ns_from_origin
        )

        with self.assertRaises(bt2._Error) as ctx:
            graph.run_once()

        cause = ctx.exception[0]
        self.assertIn("TypeError: 'str' is not a 'bool' object", cause.message)

    def test_seek_ns_from_origin(self):
        class MySink(bt2._UserSinkComponent):
            def __init__(self, config, params, obj):
                self._add_input_port('in')

            def _user_graph_is_configured(self):
                self._msg_iter = self._create_input_port_message_iterator(
                    self._input_ports['in']
                )

            def _user_consume(self):
                self._msg_iter.seek_ns_from_origin(17)

        def _user_seek_ns_from_origin(self, ns_from_origin):
            nonlocal actual_ns_from_origin
            actual_ns_from_origin = ns_from_origin

        graph = _setup_seek_test(
            MySink, user_seek_ns_from_origin=_user_seek_ns_from_origin
        )

        actual_ns_from_origin = None
        graph.run_once()
        self.assertEqual(actual_ns_from_origin, 17)


if __name__ == '__main__':
    unittest.main()
