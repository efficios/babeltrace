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

import bt2
import collections.abc


# Run callable `func` in the context of a component's __init__ method.  The
# callable is passed the Component being instantiated.
#
# The value returned by the callable is returned by run_in_component_init.
def run_in_component_init(func):
    class MySink(bt2._UserSinkComponent):
        def __init__(self, config, params, obj):
            nonlocal res_bound
            res_bound = func(self)

        def _user_consume(self):
            pass

    g = bt2.Graph()
    res_bound = None
    g.add_component(MySink, 'comp')

    # We deliberately use a different variable for returning the result than
    # the variable bound to the MySink.__init__ context and delete res_bound.
    # The MySink.__init__ context stays alive until the end of the program, so
    # if res_bound were to still point to our result, it would contribute an
    # unexpected reference to the refcount of the result, from the point of view
    # of the user of this function.  It would then affect destruction tests,
    # for example, which want to test what happens when the refcount of a Python
    # object reaches 0.

    res = res_bound
    del res_bound
    return res


# Create an empty trace class with default values.
def get_default_trace_class():
    def f(comp_self):
        return comp_self._create_trace_class()

    return run_in_component_init(f)


# Create a pair of list, one containing non-const messages and the other
# containing const messages
def _get_all_message_types(with_packet=True):
    _msgs = None

    class MyIter(bt2._UserMessageIterator):
        def __init__(self, config, self_output_port):

            nonlocal _msgs
            self._at = 0
            self._msgs = [
                self._create_stream_beginning_message(
                    self_output_port.user_data['stream']
                )
            ]

            if with_packet:
                assert self_output_port.user_data['packet']
                self._msgs.append(
                    self._create_packet_beginning_message(
                        self_output_port.user_data['packet']
                    )
                )

            default_clock_snapshot = 789

            if with_packet:
                assert self_output_port.user_data['packet']
                ev_parent = self_output_port.user_data['packet']
            else:
                assert self_output_port.user_data['stream']
                ev_parent = self_output_port.user_data['stream']

            msg = self._create_event_message(
                self_output_port.user_data['event_class'],
                ev_parent,
                default_clock_snapshot,
            )

            msg.event.payload_field['giraffe'] = 1
            msg.event.specific_context_field['ant'] = -1
            msg.event.common_context_field['cpu_id'] = 1
            self._msgs.append(msg)

            if with_packet:
                self._msgs.append(
                    self._create_packet_end_message(
                        self_output_port.user_data['packet']
                    )
                )

            self._msgs.append(
                self._create_stream_end_message(self_output_port.user_data['stream'])
            )

            _msgs = self._msgs

        def __next__(self):
            if self._at == len(self._msgs):
                raise bt2.Stop

            msg = self._msgs[self._at]
            self._at += 1
            return msg

    class MySrc(bt2._UserSourceComponent, message_iterator_class=MyIter):
        def __init__(self, config, params, obj):
            tc = self._create_trace_class()
            clock_class = self._create_clock_class(frequency=1000)

            # event common context (stream-class-defined)
            cc = tc.create_structure_field_class()
            cc += [('cpu_id', tc.create_signed_integer_field_class(8))]

            # packet context (stream-class-defined)
            pc = None

            if with_packet:
                pc = tc.create_structure_field_class()
                pc += [('something', tc.create_unsigned_integer_field_class(8))]

            stream_class = tc.create_stream_class(
                default_clock_class=clock_class,
                event_common_context_field_class=cc,
                packet_context_field_class=pc,
                supports_packets=with_packet,
            )

            # specific context (event-class-defined)
            sc = tc.create_structure_field_class()
            sc += [('ant', tc.create_signed_integer_field_class(16))]

            # event payload
            ep = tc.create_structure_field_class()
            ep += [('giraffe', tc.create_signed_integer_field_class(32))]

            event_class = stream_class.create_event_class(
                name='garou', specific_context_field_class=sc, payload_field_class=ep
            )

            trace = tc(environment={'patate': 12})
            stream = trace.create_stream(stream_class, user_attributes={'salut': 23})

            if with_packet:
                packet = stream.create_packet()
                packet.context_field['something'] = 154
            else:
                packet = None

            self._add_output_port(
                'out',
                {
                    'tc': tc,
                    'stream': stream,
                    'event_class': event_class,
                    'trace': trace,
                    'packet': packet,
                },
            )

    _graph = bt2.Graph()
    _src_comp = _graph.add_component(MySrc, 'my_source')
    _msg_iter = TestOutputPortMessageIterator(_graph, _src_comp.output_ports['out'])

    const_msgs = list(_msg_iter)

    return _msgs, const_msgs


def get_stream_beginning_message():
    msgs, _ = _get_all_message_types()
    for m in msgs:
        if type(m) is bt2._StreamBeginningMessage:
            return m


def get_const_stream_beginning_message():
    _, const_msgs = _get_all_message_types()
    for m in const_msgs:
        if type(m) is bt2._StreamBeginningMessageConst:
            return m


def get_stream_end_message():
    msgs, _ = _get_all_message_types()
    for m in msgs:
        if type(m) is bt2._StreamEndMessage:
            return m


def get_packet_beginning_message():
    msgs, _ = _get_all_message_types(with_packet=True)
    for m in msgs:
        if type(m) is bt2._PacketBeginningMessage:
            return m


def get_const_packet_beginning_message():
    _, const_msgs = _get_all_message_types(with_packet=True)
    for m in const_msgs:
        if type(m) is bt2._PacketBeginningMessageConst:
            return m


def get_packet_end_message():
    msgs, _ = _get_all_message_types(with_packet=True)
    for m in msgs:
        if type(m) is bt2._PacketEndMessage:
            return m


def get_event_message():
    msgs, _ = _get_all_message_types()
    for m in msgs:
        if type(m) is bt2._EventMessage:
            return m


def get_const_event_message():
    _, const_msgs = _get_all_message_types()
    for m in const_msgs:
        if type(m) is bt2._EventMessageConst:
            return m


# Proxy sink component class.
#
# This sink accepts a list of a single item as its initialization
# object. This sink creates a single input port `in`. When it consumes
# from this port, it puts the returned message in the initialization
# list as the first item.
class TestProxySink(bt2._UserSinkComponent):
    def __init__(self, config, params, msg_list):
        assert msg_list is not None
        self._msg_list = msg_list
        self._add_input_port('in')

    def _user_graph_is_configured(self):
        self._msg_iter = self._create_message_iterator(self._input_ports['in'])

    def _user_consume(self):
        assert self._msg_list[0] is None
        self._msg_list[0] = next(self._msg_iter)


# This is a helper message iterator for tests.
#
# The constructor accepts a graph and an output port.
#
# Internally, it adds a proxy sink to the graph and connects the
# received output port to the proxy sink's input port. Its __next__()
# method then uses the proxy sink to transfer the consumed message to
# the output port message iterator's user.
#
# This message iterator cannot seek.
class TestOutputPortMessageIterator(collections.abc.Iterator):
    def __init__(self, graph, output_port):
        self._graph = graph
        self._msg_list = [None]
        sink = graph.add_component(TestProxySink, 'test-proxy-sink', obj=self._msg_list)
        graph.connect_ports(output_port, sink.input_ports['in'])

    def __next__(self):
        assert self._msg_list[0] is None
        self._graph.run_once()
        msg = self._msg_list[0]
        assert msg is not None
        self._msg_list[0] = None
        return msg


# Create a const field of the given field class.
#
# The field is part of a dummy stream, itself part of a dummy trace created
# from trace class `tc`.
def create_const_field(tc, field_class, field_value_setter_fn):
    field_name = 'const field'

    class MyIter(bt2._UserMessageIterator):
        def __init__(self, config, self_port_output):
            nonlocal field_class
            nonlocal field_value_setter_fn
            trace = tc()
            packet_context_fc = tc.create_structure_field_class()
            packet_context_fc.append_member(field_name, field_class)
            sc = tc.create_stream_class(
                packet_context_field_class=packet_context_fc, supports_packets=True
            )
            stream = trace.create_stream(sc)
            packet = stream.create_packet()

            field_value_setter_fn(packet.context_field[field_name])

            self._msgs = [
                self._create_stream_beginning_message(stream),
                self._create_packet_beginning_message(packet),
            ]

        def __next__(self):
            if len(self._msgs) == 0:
                raise StopIteration

            return self._msgs.pop(0)

    class MySrc(bt2._UserSourceComponent, message_iterator_class=MyIter):
        def __init__(self, config, params, obj):
            self._add_output_port('out', params)

    graph = bt2.Graph()
    src_comp = graph.add_component(MySrc, 'my_source', None)
    msg_iter = TestOutputPortMessageIterator(graph, src_comp.output_ports['out'])

    # Ignore first message, stream beginning
    _ = next(msg_iter)
    packet_beg_msg = next(msg_iter)

    return packet_beg_msg.packet.context_field[field_name]


# Run `msg_iter_next_func` in a bt2._UserMessageIterator.__next__ context.
#
# For convenience, a trace and a stream are created.  To allow the caller to
# customize the created stream class, the `create_stream_class_func` callback
# is invoked during the component initialization.  It gets passed a trace class
# and a clock class, and must return a stream class.
#
# The `msg_iter_next_func` callback receives two arguments, the message iterator
# and the created stream.
#
# The value returned by `msg_iter_next_func` is returned by this function.
def run_in_message_iterator_next(create_stream_class_func, msg_iter_next_func):
    class MyIter(bt2._UserMessageIterator):
        def __init__(self, config, port):
            tc, sc = port.user_data
            trace = tc()
            self._stream = trace.create_stream(sc)

        def __next__(self):
            nonlocal res_bound
            res_bound = msg_iter_next_func(self, self._stream)
            raise bt2.Stop

    class MySrc(bt2._UserSourceComponent, message_iterator_class=MyIter):
        def __init__(self, config, params, obj):
            tc = self._create_trace_class()
            cc = self._create_clock_class()
            sc = create_stream_class_func(tc, cc)

            self._add_output_port('out', (tc, sc))

    class MySink(bt2._UserSinkComponent):
        def __init__(self, config, params, obj):
            self._input_port = self._add_input_port('in')

        def _user_graph_is_configured(self):
            self._input_iter = self._create_message_iterator(self._input_port)

        def _user_consume(self):
            next(self._input_iter)

    graph = bt2.Graph()
    res_bound = None
    src = graph.add_component(MySrc, 'ze source')
    snk = graph.add_component(MySink, 'ze sink')
    graph.connect_ports(src.output_ports['out'], snk.input_ports['in'])
    graph.run()

    # We deliberately use a different variable for returning the result than
    # the variable bound to the MyIter.__next__ context.  See the big comment
    # about that in `run_in_component_init`.

    res = res_bound
    del res_bound
    return res
