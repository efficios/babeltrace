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
        def __init__(self, params, obj):
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


# Proxy sink component class.
#
# This sink accepts a list of a single item as its initialization
# object. This sink creates a single input port `in`. When it consumes
# from this port, it puts the returned message in the initialization
# list as the first item.
class TestProxySink(bt2._UserSinkComponent):
    def __init__(self, params, msg_list):
        assert msg_list is not None
        self._msg_list = msg_list
        self._add_input_port('in')

    def _user_graph_is_configured(self):
        self._msg_iter = self._create_input_port_message_iterator(
            self._input_ports['in']
        )

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
