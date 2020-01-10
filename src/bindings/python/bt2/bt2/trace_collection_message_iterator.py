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

from bt2 import utils, native_bt
import bt2
import itertools
from bt2 import message_iterator as bt2_message_iterator
from bt2 import port as bt2_port
from bt2 import component as bt2_component
from bt2 import value as bt2_value
from bt2 import plugin as bt2_plugin
import datetime
from collections import namedtuple
import numbers


# a pair of component and ComponentSpec
_ComponentAndSpec = namedtuple('_ComponentAndSpec', ['comp', 'spec'])


class _BaseComponentSpec:
    # Base for any component spec that can be passed to
    # TraceCollectionMessageIterator.
    def __init__(self, params, obj, logging_level):
        if logging_level is not None:
            utils._check_log_level(logging_level)

        self._params = bt2.create_value(params)
        self._obj = obj
        self._logging_level = logging_level

    @property
    def params(self):
        return self._params

    @property
    def obj(self):
        return self._obj

    @property
    def logging_level(self):
        return self._logging_level


class ComponentSpec(_BaseComponentSpec):
    # A component spec with a specific component class.
    def __init__(
        self,
        component_class,
        params=None,
        obj=None,
        logging_level=bt2.LoggingLevel.NONE,
    ):
        if type(params) is str:
            params = {'inputs': [params]}

        super().__init__(params, obj, logging_level)

        is_cc_object = isinstance(
            component_class,
            (bt2._SourceComponentClassConst, bt2._FilterComponentClassConst),
        )
        is_user_cc_type = isinstance(
            component_class, bt2_component._UserComponentType
        ) and issubclass(
            component_class, (bt2._UserSourceComponent, bt2._UserFilterComponent)
        )

        if not is_cc_object and not is_user_cc_type:
            raise TypeError(
                "'{}' is not a source or filter component class".format(
                    component_class.__class__.__name__
                )
            )

        self._component_class = component_class

    @property
    def component_class(self):
        return self._component_class

    @classmethod
    def from_named_plugin_and_component_class(
        cls,
        plugin_name,
        component_class_name,
        params=None,
        obj=None,
        logging_level=bt2.LoggingLevel.NONE,
    ):
        plugin = bt2.find_plugin(plugin_name)

        if plugin is None:
            raise ValueError('no such plugin: {}'.format(plugin_name))

        if component_class_name in plugin.source_component_classes:
            comp_class = plugin.source_component_classes[component_class_name]
        elif component_class_name in plugin.filter_component_classes:
            comp_class = plugin.filter_component_classes[component_class_name]
        else:
            raise KeyError(
                'source or filter component class `{}` not found in plugin `{}`'.format(
                    component_class_name, plugin_name
                )
            )

        return cls(comp_class, params, obj, logging_level)


class AutoSourceComponentSpec(_BaseComponentSpec):
    # A component spec that does automatic source discovery.
    _no_obj = object()

    def __init__(self, input, params=None, obj=_no_obj, logging_level=None):
        super().__init__(params, obj, logging_level)
        self._input = input

    @property
    def input(self):
        return self._input


def _auto_discover_source_component_specs(auto_source_comp_specs, plugin_set):
    # Transform a list of `AutoSourceComponentSpec` in a list of `ComponentSpec`
    # using the automatic source discovery mechanism.
    inputs = bt2.ArrayValue([spec.input for spec in auto_source_comp_specs])

    if plugin_set is None:
        plugin_set = bt2.find_plugins()
    else:
        utils._check_type(plugin_set, bt2_plugin._PluginSet)

    res_ptr = native_bt.bt2_auto_discover_source_components(
        inputs._ptr, plugin_set._ptr
    )

    if res_ptr is None:
        raise bt2._MemoryError('cannot auto discover source components')

    res = bt2_value._create_from_ptr(res_ptr)

    assert type(res) == bt2.MapValue
    assert 'status' in res

    status = res['status']
    utils._handle_func_status(status, 'cannot auto-discover source components')

    comp_specs = []
    comp_specs_raw = res['results']
    assert type(comp_specs_raw) == bt2.ArrayValue

    used_input_indices = set()

    for comp_spec_raw in comp_specs_raw:
        assert type(comp_spec_raw) == bt2.ArrayValue
        assert len(comp_spec_raw) == 4

        plugin_name = comp_spec_raw[0]
        assert type(plugin_name) == bt2.StringValue
        plugin_name = str(plugin_name)

        class_name = comp_spec_raw[1]
        assert type(class_name) == bt2.StringValue
        class_name = str(class_name)

        comp_inputs = comp_spec_raw[2]
        assert type(comp_inputs) == bt2.ArrayValue

        comp_orig_indices = comp_spec_raw[3]
        assert type(comp_orig_indices)

        params = bt2.MapValue()
        logging_level = bt2.LoggingLevel.NONE
        obj = None

        # Compute `params` for this component by piling up params given to all
        # AutoSourceComponentSpec objects that contributed in the instantiation
        # of this component.
        #
        # The effective log level for a component is the last one specified
        # across the AutoSourceComponentSpec that contributed in its
        # instantiation.
        for idx in comp_orig_indices:
            orig_spec = auto_source_comp_specs[idx]

            if orig_spec.params is not None:
                params.update(orig_spec.params)

            if orig_spec.logging_level is not None:
                logging_level = orig_spec.logging_level

            if orig_spec.obj is not AutoSourceComponentSpec._no_obj:
                obj = orig_spec.obj

            used_input_indices.add(int(idx))

        params['inputs'] = comp_inputs

        comp_specs.append(
            ComponentSpec.from_named_plugin_and_component_class(
                plugin_name,
                class_name,
                params=params,
                obj=obj,
                logging_level=logging_level,
            )
        )

    if len(used_input_indices) != len(inputs):
        unused_input_indices = set(range(len(inputs))) - used_input_indices
        unused_input_indices = sorted(unused_input_indices)
        unused_inputs = [str(inputs[x]) for x in unused_input_indices]

        msg = (
            'Some auto source component specs did not produce any component: '
            + ', '.join(unused_inputs)
        )
        raise RuntimeError(msg)

    return comp_specs


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
        raise TypeError(
            '"{}" is not an integral number or a datetime.datetime object'.format(obj)
        )

    return int(s * 1e9)


class _TraceCollectionMessageIteratorProxySink(bt2_component._UserSinkComponent):
    def __init__(self, config, params, msg_list):
        assert type(msg_list) is list
        self._msg_list = msg_list
        self._add_input_port('in')

    def _user_graph_is_configured(self):
        self._msg_iter = self._create_message_iterator(self._input_ports['in'])

    def _user_consume(self):
        assert self._msg_list[0] is None
        self._msg_list[0] = next(self._msg_iter)


class TraceCollectionMessageIterator(bt2_message_iterator._MessageIterator):
    def __init__(
        self,
        source_component_specs,
        filter_component_specs=None,
        stream_intersection_mode=False,
        begin=None,
        end=None,
        plugin_set=None,
    ):
        utils._check_bool(stream_intersection_mode)
        self._stream_intersection_mode = stream_intersection_mode
        self._begin_ns = _get_ns(begin)
        self._end_ns = _get_ns(end)
        self._msg_list = [None]

        # If a single item is provided, convert to a list.
        if type(source_component_specs) in (
            ComponentSpec,
            AutoSourceComponentSpec,
            str,
        ):
            source_component_specs = [source_component_specs]

        # Convert any string to an AutoSourceComponentSpec.
        def str_to_auto(item):
            if type(item) is str:
                item = AutoSourceComponentSpec(item)

            return item

        source_component_specs = [str_to_auto(s) for s in source_component_specs]

        if type(filter_component_specs) is ComponentSpec:
            filter_component_specs = [filter_component_specs]
        elif filter_component_specs is None:
            filter_component_specs = []

        self._validate_source_component_specs(source_component_specs)
        self._validate_filter_component_specs(filter_component_specs)

        # Pass any `ComponentSpec` instance as-is.
        self._src_comp_specs = [
            spec for spec in source_component_specs if type(spec) is ComponentSpec
        ]

        # Convert any `AutoSourceComponentSpec` in concrete `ComponentSpec` instances.
        auto_src_comp_specs = [
            spec
            for spec in source_component_specs
            if type(spec) is AutoSourceComponentSpec
        ]
        self._src_comp_specs += _auto_discover_source_component_specs(
            auto_src_comp_specs, plugin_set
        )

        self._flt_comp_specs = filter_component_specs
        self._next_suffix = 1
        self._connect_ports = False

        # lists of _ComponentAndSpec
        self._src_comps_and_specs = []
        self._flt_comps_and_specs = []

        self._build_graph()

    def _compute_stream_intersections(self):
        # Pre-compute the trimmer range to use for each port in the graph, when
        # stream intersection mode is enabled.
        self._stream_inter_port_to_range = {}

        for src_comp_and_spec in self._src_comps_and_specs:
            # Query the port's component for the `babeltrace.trace-infos`
            # object which contains the range for each stream, from which we can
            # compute the intersection of the streams in each trace.
            query_exec = bt2.QueryExecutor(
                src_comp_and_spec.spec.component_class,
                'babeltrace.trace-infos',
                src_comp_and_spec.spec.params,
            )
            trace_infos = query_exec.query()

            for trace_info in trace_infos:
                begin = max(
                    [
                        stream['range-ns']['begin']
                        for stream in trace_info['stream-infos']
                    ]
                )
                end = min(
                    [stream['range-ns']['end'] for stream in trace_info['stream-infos']]
                )

                # Each port associated to this trace will have this computed
                # range.
                for stream in trace_info['stream-infos']:
                    # A port name is unique within a component, but not
                    # necessarily across all components.  Use a component
                    # and port name pair to make it unique across the graph.
                    port_name = str(stream['port-name'])
                    key = (src_comp_and_spec.comp.addr, port_name)
                    self._stream_inter_port_to_range[key] = (begin, end)

    def _validate_source_component_specs(self, comp_specs):
        for comp_spec in comp_specs:
            if (
                type(comp_spec) is not ComponentSpec
                and type(comp_spec) is not AutoSourceComponentSpec
            ):
                raise TypeError(
                    '"{}" object is not a ComponentSpec or AutoSourceComponentSpec'.format(
                        type(comp_spec)
                    )
                )

    def _validate_filter_component_specs(self, comp_specs):
        for comp_spec in comp_specs:
            if type(comp_spec) is not ComponentSpec:
                raise TypeError(
                    '"{}" object is not a ComponentSpec'.format(type(comp_spec))
                )

    def __next__(self):
        assert self._msg_list[0] is None
        self._graph.run_once()
        msg = self._msg_list[0]
        assert msg is not None
        self._msg_list[0] = None
        return msg

    def _create_stream_intersection_trimmer(self, component, port):
        key = (component.addr, port.name)
        begin, end = self._stream_inter_port_to_range[key]
        name = 'trimmer-{}-{}'.format(component.name, port.name)
        return self._create_trimmer(begin, end, name)

    def _create_muxer(self):
        plugin = bt2.find_plugin('utils')

        if plugin is None:
            raise RuntimeError('cannot find "utils" plugin (needed for the muxer)')

        if 'muxer' not in plugin.filter_component_classes:
            raise RuntimeError(
                'cannot find "muxer" filter component class in "utils" plugin'
            )

        comp_cls = plugin.filter_component_classes['muxer']
        return self._graph.add_component(comp_cls, 'muxer')

    def _create_trimmer(self, begin_ns, end_ns, name):
        plugin = bt2.find_plugin('utils')

        if plugin is None:
            raise RuntimeError('cannot find "utils" plugin (needed for the trimmer)')

        if 'trimmer' not in plugin.filter_component_classes:
            raise RuntimeError(
                'cannot find "trimmer" filter component class in "utils" plugin'
            )

        params = {}

        def ns_to_string(ns):
            s_part = ns // 1000000000
            ns_part = ns % 1000000000
            return '{}.{:09d}'.format(s_part, ns_part)

        if begin_ns is not None:
            params['begin'] = ns_to_string(begin_ns)

        if end_ns is not None:
            params['end'] = ns_to_string(end_ns)

        comp_cls = plugin.filter_component_classes['trimmer']
        return self._graph.add_component(comp_cls, name, params)

    def _get_unique_comp_name(self, comp_cls):
        name = comp_cls.name
        comps_and_specs = itertools.chain(
            self._src_comps_and_specs, self._flt_comps_and_specs
        )

        if name in [comp_and_spec.comp.name for comp_and_spec in comps_and_specs]:
            name += '-{}'.format(self._next_suffix)
            self._next_suffix += 1

        return name

    def _create_comp(self, comp_spec):
        comp_cls = comp_spec.component_class
        name = self._get_unique_comp_name(comp_cls)
        comp = self._graph.add_component(
            comp_cls, name, comp_spec.params, comp_spec.obj, comp_spec.logging_level
        )
        return comp

    def _get_free_muxer_input_port(self):
        for port in self._muxer_comp.input_ports.values():
            if not port.is_connected:
                return port

    def _connect_src_comp_port(self, component, port):
        # if this trace collection iterator is in stream intersection
        # mode, we need this connection:
        #
        #     port -> trimmer -> muxer
        #
        # otherwise, simply:
        #
        #     port -> muxer
        if self._stream_intersection_mode:
            trimmer_comp = self._create_stream_intersection_trimmer(component, port)
            self._graph.connect_ports(port, trimmer_comp.input_ports['in'])
            port_to_muxer = trimmer_comp.output_ports['out']
        else:
            port_to_muxer = port

        self._graph.connect_ports(port_to_muxer, self._get_free_muxer_input_port())

    def _graph_port_added(self, component, port):
        if not self._connect_ports:
            return

        if type(port) is bt2_port._InputPortConst:
            return

        if component not in [comp.comp for comp in self._src_comps_and_specs]:
            # do not care about non-source components (muxer, trimmer, etc.)
            return

        self._connect_src_comp_port(component, port)

    def _get_greatest_operative_mip_version(self):
        def append_comp_specs_descriptors(descriptors, comp_specs):
            for comp_spec in comp_specs:
                descriptors.append(
                    bt2.ComponentDescriptor(
                        comp_spec.component_class, comp_spec.params, comp_spec.obj
                    )
                )

        descriptors = []
        append_comp_specs_descriptors(descriptors, self._src_comp_specs)
        append_comp_specs_descriptors(descriptors, self._flt_comp_specs)

        if self._stream_intersection_mode:
            # we also need at least one `flt.utils.trimmer` component
            comp_spec = ComponentSpec.from_named_plugin_and_component_class(
                'utils', 'trimmer'
            )
            append_comp_specs_descriptors(descriptors, [comp_spec])

        mip_version = bt2.get_greatest_operative_mip_version(descriptors)

        if mip_version is None:
            msg = 'failed to find an operative message interchange protocol version (components are not interoperable)'
            raise RuntimeError(msg)

        return mip_version

    def _build_graph(self):
        self._graph = bt2.Graph(self._get_greatest_operative_mip_version())
        self._graph.add_port_added_listener(self._graph_port_added)
        self._muxer_comp = self._create_muxer()

        if self._begin_ns is not None or self._end_ns is not None:
            trimmer_comp = self._create_trimmer(self._begin_ns, self._end_ns, 'trimmer')
            self._graph.connect_ports(
                self._muxer_comp.output_ports['out'], trimmer_comp.input_ports['in']
            )
            last_flt_out_port = trimmer_comp.output_ports['out']
        else:
            last_flt_out_port = self._muxer_comp.output_ports['out']

        # create extra filter components (chained)
        for comp_spec in self._flt_comp_specs:
            comp = self._create_comp(comp_spec)
            self._flt_comps_and_specs.append(_ComponentAndSpec(comp, comp_spec))

        # connect the extra filter chain
        for comp_and_spec in self._flt_comps_and_specs:
            in_port = list(comp_and_spec.comp.input_ports.values())[0]
            out_port = list(comp_and_spec.comp.output_ports.values())[0]
            self._graph.connect_ports(last_flt_out_port, in_port)
            last_flt_out_port = out_port

        # Here we create the components, self._graph_port_added() is
        # called when they add ports, but the callback returns early
        # because self._connect_ports is False. This is because the
        # self._graph_port_added() could not find the associated source
        # component specification in self._src_comps_and_specs because
        # it does not exist yet (it needs the created component to
        # exist).
        for comp_spec in self._src_comp_specs:
            comp = self._create_comp(comp_spec)
            self._src_comps_and_specs.append(_ComponentAndSpec(comp, comp_spec))

        if self._stream_intersection_mode:
            self._compute_stream_intersections()

        # Now we connect the ports which exist at this point. We allow
        # self._graph_port_added() to automatically connect _new_ ports.
        self._connect_ports = True

        for comp_and_spec in self._src_comps_and_specs:
            # Keep a separate list because comp_and_spec.output_ports
            # could change during the connection of one of its ports.
            # Any new port is handled by self._graph_port_added().
            out_ports = [port for port in comp_and_spec.comp.output_ports.values()]

            for out_port in out_ports:
                if out_port.is_connected:
                    continue

                self._connect_src_comp_port(comp_and_spec.comp, out_port)

        # Add the proxy sink, passing our message list to share consumed
        # messages with this trace collection message iterator.
        sink = self._graph.add_component(
            _TraceCollectionMessageIteratorProxySink, 'proxy-sink', obj=self._msg_list
        )
        sink_in_port = sink.input_ports['in']

        # connect last filter to proxy sink
        self._graph.connect_ports(last_flt_out_port, sink_in_port)
