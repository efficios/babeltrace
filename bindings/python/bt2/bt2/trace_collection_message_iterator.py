# The MIT License (MIT)
#
# Copyright (c) 2017 Philippe Proulx <pproulx@efficios.com>
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
# THE SOFTWARE.

from bt2 import utils
import bt2
import itertools
import bt2.message_iterator
import datetime
import collections.abc
from collections import namedtuple
import numbers


# a pair of component and ComponentSpec
_ComponentAndSpec = namedtuple('_ComponentAndSpec', ['comp', 'spec'])


class ComponentSpec:
    def __init__(self, plugin_name, component_class_name, params=None):
        utils._check_str(plugin_name)
        utils._check_str(component_class_name)
        self._plugin_name = plugin_name
        self._component_class_name = component_class_name

        if type(params) is str:
            self._params = bt2.create_value({'path': params})
        else:
            self._params = bt2.create_value(params)

    @property
    def plugin_name(self):
        return self._plugin_name

    @property
    def component_class_name(self):
        return self._component_class_name

    @property
    def params(self):
        return self._params


# datetime.datetime or integral to nanoseconds
def _get_ns(obj):
    if obj is None:
        return

    if isinstance(obj, numbers.Real):
        # consider that it's already in seconds
        s = obj
    elif isinstance(obj, datetime.datetime):
        # s -> ns
        s = obj.timestamp()
    else:
        raise TypeError('"{}" is not an integral number or a datetime.datetime object'.format(obj))

    return int(s * 1e9)


class _CompClsType:
    SOURCE = 0
    FILTER = 1


class TraceCollectionMessageIterator(bt2.message_iterator._MessageIterator):
    def __init__(self, source_component_specs, filter_component_specs=None,
                 message_types=None, stream_intersection_mode=False,
                 begin=None, end=None):
        utils._check_bool(stream_intersection_mode)
        self._stream_intersection_mode = stream_intersection_mode
        self._begin_ns = _get_ns(begin)
        self._end_ns = _get_ns(end)
        self._message_types = message_types

        if type(source_component_specs) is ComponentSpec:
            source_component_specs = [source_component_specs]

        if type(filter_component_specs) is ComponentSpec:
            filter_component_specs = [filter_component_specs]
        elif filter_component_specs is None:
            filter_component_specs = []

        self._src_comp_specs = source_component_specs
        self._flt_comp_specs = filter_component_specs
        self._next_suffix = 1
        self._connect_ports = False

        # lists of _ComponentAndSpec
        self._src_comps_and_specs = []
        self._flt_comps_and_specs = []

        self._validate_component_specs(source_component_specs)
        self._validate_component_specs(filter_component_specs)
        self._build_graph()

    def _validate_component_specs(self, comp_specs):
        for comp_spec in comp_specs:
            if type(comp_spec) is not ComponentSpec:
                raise TypeError('"{}" object is not a ComponentSpec'.format(type(comp_spec)))

    def __next__(self):
        return next(self._msg_iter)

    def _create_stream_intersection_trimmer(self, port):
        # find the original parameters specified by the user to create
        # this port's component to get the `path` parameter
        for src_comp_and_spec in self._src_comps_and_specs:
            if port.component == src_comp_and_spec.comp:
                params = src_comp_and_spec.spec.params
                break

        try:
            path = params['path']
        except:
            raise bt2.Error('all source components must be created with a "path" parameter in stream intersection mode')

        params = {'path': str(path)}

        # query the port's component for the `trace-info` object which
        # contains the stream intersection range for each exposed
        # trace
        query_exec = bt2.QueryExecutor()
        trace_info_res = query_exec.query(port.component.component_class,
                                          'trace-info', params)
        begin = None
        end = None

        # find the trace info for this port's trace by name's prefix
        try:
            for trace_info in trace_info_res:
                if port.name.startswith(str(trace_info['path'])):
                    range_ns = trace_info['intersection-range-ns']
                    begin = range_ns['begin']
                    end = range_ns['end']
                    break
        except:
            pass

        if begin is None or end is None:
            raise bt2.Error('cannot find stream intersection range for port "{}"'.format(port.name))

        name = 'trimmer-{}-{}'.format(port.component.name, port.name)
        return self._create_trimmer(begin, end, name)

    def _create_muxer(self):
        plugin = bt2.find_plugin('utils')

        if plugin is None:
            raise bt2.Error('cannot find "utils" plugin (needed for the muxer)')

        if 'muxer' not in plugin.filter_component_classes:
            raise bt2.Error('cannot find "muxer" filter component class in "utils" plugin')

        comp_cls = plugin.filter_component_classes['muxer']
        return self._graph.add_component(comp_cls, 'muxer')

    def _create_trimmer(self, begin, end, name):
        plugin = bt2.find_plugin('utils')

        if plugin is None:
            raise bt2.Error('cannot find "utils" plugin (needed for the trimmer)')

        if 'trimmer' not in plugin.filter_component_classes:
            raise bt2.Error('cannot find "trimmer" filter component class in "utils" plugin')

        params = {}

        if begin is not None:
            params['begin'] = begin

        if end is not None:
            params['end'] = end

        comp_cls = plugin.filter_component_classes['trimmer']
        return self._graph.add_component(comp_cls, name, params)

    def _get_unique_comp_name(self, comp_spec):
        name = '{}-{}'.format(comp_spec.plugin_name,
                              comp_spec.component_class_name)
        comps_and_specs = itertools.chain(self._src_comps_and_specs,
                                          self._flt_comps_and_specs)

        if name in [comp_and_spec.comp.name for comp_and_spec in comps_and_specs]:
            name += '-{}'.format(self._next_suffix)
            self._next_suffix += 1

        return name

    def _create_comp(self, comp_spec, comp_cls_type):
        plugin = bt2.find_plugin(comp_spec.plugin_name)

        if plugin is None:
            raise bt2.Error('no such plugin: {}'.format(comp_spec.plugin_name))

        if comp_cls_type == _CompClsType.SOURCE:
            comp_classes = plugin.source_component_classes
        else:
            comp_classes = plugin.filter_component_classes

        if comp_spec.component_class_name not in comp_classes:
            cc_type = 'source' if comp_cls_type == _CompClsType.SOURCE else 'filter'
            raise bt2.Error('no such {} component class in "{}" plugin: {}'.format(cc_type,
                                                                                   comp_spec.plugin_name,
                                                                                   comp_spec.component_class_name))

        comp_cls = comp_classes[comp_spec.component_class_name]
        name = self._get_unique_comp_name(comp_spec)
        comp = self._graph.add_component(comp_cls, name, comp_spec.params)
        return comp

    def _get_free_muxer_input_port(self):
        for port in self._muxer_comp.input_ports.values():
            if not port.is_connected:
                return port

    def _connect_src_comp_port(self, port):
        # if this trace collection iterator is in stream intersection
        # mode, we need this connection:
        #
        #     port -> trimmer -> muxer
        #
        # otherwise, simply:
        #
        #     port -> muxer
        if self._stream_intersection_mode:
            trimmer_comp = self._create_stream_intersection_trimmer(port)
            self._graph.connect_ports(port, trimmer_comp.input_ports['in'])
            port_to_muxer = trimmer_comp.output_ports['out']
        else:
            port_to_muxer = port

        self._graph.connect_ports(port_to_muxer, self._get_free_muxer_input_port())

    def _graph_port_added(self, port):
        if not self._connect_ports:
            return

        if type(port) is bt2._InputPort:
            return

        if port.component not in [comp.comp for comp in self._src_comps_and_specs]:
            # do not care about non-source components (muxer, trimmer, etc.)
            return

        self._connect_src_comp_port(port)

    def _build_graph(self):
        self._graph = bt2.Graph()
        self._graph.add_listener(bt2.GraphListenerType.PORT_ADDED,
                                 self._graph_port_added)
        self._muxer_comp = self._create_muxer()

        if self._begin_ns is not None or self._end_ns is not None:
            trimmer_comp = self._create_trimmer(self._begin_ns,
                                                self._end_ns, 'trimmer')
            self._graph.connect_ports(self._muxer_comp.output_ports['out'],
                                      trimmer_comp.input_ports['in'])
            msg_iter_port = trimmer_comp.output_ports['out']
        else:
            msg_iter_port = self._muxer_comp.output_ports['out']

        # create extra filter components (chained)
        for comp_spec in self._flt_comp_specs:
            comp = self._create_comp(comp_spec, _CompClsType.FILTER)
            self._flt_comps_and_specs.append(_ComponentAndSpec(comp, comp_spec))

        # connect the extra filter chain
        for comp_and_spec in self._flt_comps_and_specs:
            in_port = list(comp_and_spec.comp.input_ports.values())[0]
            out_port = list(comp_and_spec.comp.output_ports.values())[0]
            self._graph.connect_ports(msg_iter_port, in_port)
            msg_iter_port = out_port

        # Here we create the components, self._graph_port_added() is
        # called when they add ports, but the callback returns early
        # because self._connect_ports is False. This is because the
        # self._graph_port_added() could not find the associated source
        # component specification in self._src_comps_and_specs because
        # it does not exist yet (it needs the created component to
        # exist).
        for comp_spec in self._src_comp_specs:
            comp = self._create_comp(comp_spec, _CompClsType.SOURCE)
            self._src_comps_and_specs.append(_ComponentAndSpec(comp, comp_spec))

        # Now we connect the ports which exist at this point. We allow
        # self._graph_port_added() to automatically connect _new_ ports.
        self._connect_ports = True

        for comp_and_spec in self._src_comps_and_specs:
            # Keep a separate list because comp_and_spec.output_ports
            # could change during the connection of one of its ports.
            # Any new port is handled by self._graph_port_added().
            out_ports = [port for port in comp_and_spec.comp.output_ports.values()]

            for out_port in out_ports:
                if not out_port.component or out_port.is_connected:
                    continue

                self._connect_src_comp_port(out_port)

        # create this trace collection iterator's message iterator
        self._msg_iter = msg_iter_port.create_message_iterator(self._message_types)
