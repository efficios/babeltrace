# SPDX-License-Identifier: MIT
#
# Copyright (c) 2017 Philippe Proulx <pproulx@efficios.com>

import os
import typing
import numbers
import weakref
import datetime
import threading
from collections import namedtuple

from bt2 import mip as bt2_mip
from bt2 import port as bt2_port
from bt2 import graph as bt2_graph
from bt2 import utils as bt2_utils
from bt2 import value as bt2_value
from bt2 import plugin as bt2_plugin
from bt2 import logging as bt2_logging
from bt2 import message as bt2_message
from bt2 import component as bt2_component
from bt2 import native_bt
from bt2 import query_executor as bt2_query_executor
from bt2 import message_iterator as bt2_message_iterator
from bt2 import component_descriptor as bt2_component_descriptor

# a pair of component and ComponentSpec
_ComponentAndSpec = namedtuple("_ComponentAndSpec", ["comp", "spec"])

# Lazily-loaded, cached `utils` plugin (needed for its `muxer` and
# `trimmer` filter component classes).
#
# Protected by `_cached_utils_plugin_lock`.
_cached_utils_plugin = None
_cached_utils_plugin_lock = threading.Lock()


def _should_find_plugins_in_std_dirs():
    disable_std_plugin_dirs = os.environ.get("BABELTRACE_DISABLE_STD_PLUGIN_DIRS")

    return disable_std_plugin_dirs is None or disable_std_plugin_dirs != "1"


# bt2.find_plugins() wrapper that honors the
# `BABELTRACE_DISABLE_STD_PLUGIN_DIRS` env var.
def _find_plugins():
    should_find_plugins_in_std_dirs = _should_find_plugins_in_std_dirs()

    return bt2_plugin.find_plugins(
        find_in_user_dir=should_find_plugins_in_std_dirs,
        find_in_sys_dir=should_find_plugins_in_std_dirs,
    )


# bt2.find_plugin() wrapper that honors the
# `BABELTRACE_DISABLE_STD_PLUGIN_DIRS` env var.
def _find_plugin(name):
    should_find_plugins_in_std_dirs = _should_find_plugins_in_std_dirs()

    return bt2_plugin.find_plugin(
        name,
        find_in_user_dir=should_find_plugins_in_std_dirs,
        find_in_sys_dir=should_find_plugins_in_std_dirs,
    )


# Returns the cached `utils` plugin, loading it on first use.
#
# Thread-safe.
def _utils_plugin():
    global _cached_utils_plugin

    if _cached_utils_plugin is None:
        with _cached_utils_plugin_lock:
            # Another thread may have loaded the plugin while we were
            # waiting for the lock: check again.
            if _cached_utils_plugin is None:
                plugin = _find_plugin("utils")

                if plugin is None:
                    raise RuntimeError('cannot find "utils" plugin')

                _cached_utils_plugin = plugin

    return _cached_utils_plugin


# Returns the filter component class named `name` from the cached
# `utils` plugin.
def _utils_filter_comp_cls(name):
    plugin = _utils_plugin()

    if name not in plugin.filter_component_classes:
        raise RuntimeError(
            f'cannot find "{name}" filter component class in "utils" plugin'
        )

    return plugin.filter_component_classes[name]


class _BaseComponentSpec:
    # Base for any component spec that can be passed to
    # TraceCollectionMessageIterator.
    def __init__(
        self, params, obj, logging_level: typing.Optional[bt2_logging.LoggingLevel]
    ):
        if logging_level is not None:
            bt2_utils._check_type(logging_level, bt2_logging.LoggingLevel)

        self._params = bt2_value.create_value(params)
        self._obj = obj
        self._logging_level = logging_level

    @property
    def params(self) -> typing.Optional[bt2_value._Value]:
        return self._params

    @property
    def obj(self) -> object:
        return self._obj

    @property
    def logging_level(self) -> typing.Optional[bt2_logging.LoggingLevel]:
        return self._logging_level


class ComponentSpec(_BaseComponentSpec):
    # A component spec with a specific component class.
    def __init__(
        self,
        component_class: typing.Union[
            bt2_component._SourceComponentClassConst,
            bt2_component._FilterComponentClassConst,
            typing.Type[bt2_component._UserSourceComponent],
            typing.Type[bt2_component._UserFilterComponent],
        ],
        params: bt2_component._ComponentParams = None,
        obj=None,
        logging_level: bt2_logging.LoggingLevel = bt2_logging.LoggingLevel.NONE,
    ):
        if type(params) is str:
            params = {"inputs": [params]}

        super().__init__(params, obj, logging_level)

        is_cc_object = isinstance(
            component_class,
            (
                bt2_component._SourceComponentClassConst,
                bt2_component._FilterComponentClassConst,
            ),
        )
        is_user_cc_type = isinstance(
            component_class, bt2_component._UserComponentType
        ) and issubclass(
            component_class,
            (bt2_component._UserSourceComponent, bt2_component._UserFilterComponent),
        )

        if not is_cc_object and not is_user_cc_type:
            raise TypeError(
                f"'{component_class.__class__.__name__}' is not a source or filter component class"
            )

        self._component_class = component_class

    @property
    def component_class(
        self,
    ) -> typing.Union[
        bt2_component._SourceComponentClassConst,
        bt2_component._FilterComponentClassConst,
        typing.Type[bt2_component._UserSourceComponent],
        typing.Type[bt2_component._UserFilterComponent],
    ]:
        return self._component_class

    @classmethod
    def from_named_plugin_and_component_class(
        cls,
        plugin_name: str,
        component_class_name: str,
        params: bt2_component._ComponentParams = None,
        obj: object = None,
        logging_level: bt2_logging.LoggingLevel = bt2_logging.LoggingLevel.NONE,
        plugin_set: typing.Optional[bt2_plugin._PluginSet] = None,
    ):
        bt2_utils._check_str(plugin_name)

        if plugin_set is None:
            plugin_set = _find_plugins()
        else:
            bt2_utils._check_type(plugin_set, bt2_plugin._PluginSet)

        plugin = None

        if plugin_set is not None:
            for candidate in plugin_set:
                if candidate.name == plugin_name:
                    plugin = candidate
                    break

        if plugin is None:
            raise ValueError(f"no such plugin: {plugin_name}")

        if component_class_name in plugin.source_component_classes:
            comp_class = plugin.source_component_classes[component_class_name]
        elif component_class_name in plugin.filter_component_classes:
            comp_class = plugin.filter_component_classes[component_class_name]
        else:
            raise KeyError(
                f"source or filter component class `{component_class_name}` not found in plugin `{plugin_name}`"
            )

        return cls(comp_class, params, obj, logging_level)


class AutoSourceComponentSpec(_BaseComponentSpec):
    # A component spec that does automatic source discovery.
    _no_obj = object()

    def __init__(
        self,
        input: str,
        params: bt2_component._ComponentParams = None,
        obj: object = _no_obj,
        logging_level: typing.Optional[bt2_logging.LoggingLevel] = None,
    ):
        super().__init__(params, obj, logging_level)
        self._input = input

    @property
    def input(self) -> str:
        return self._input


# Find the source component class named `class_name` of the plugin named
# `plugin_name` in `plugin_set`, or `None` if not found.
def _src_comp_cls_from_plugin_set(plugin_set, plugin_name, class_name):
    for plugin in plugin_set:
        if plugin.name != plugin_name:
            continue

        if class_name in plugin.source_component_classes:
            return plugin.source_component_classes[class_name]

    return None


# Transform a list of `AutoSourceComponentSpec` into a list of
# `ComponentSpec` using the automatic source component
# discovery mechanism.
#
# If `plugin_set` is specified, perform the automatic source component
# discovery mechanism using the plugins in `plugin_set`. Otherwise, look for
# plugins using bt2.find_plugins().
def source_component_specs_from_auto_source_component_specs(
    auto_source_comp_specs: typing.Sequence["AutoSourceComponentSpec"],
    plugin_set: typing.Optional[bt2_plugin._PluginSet] = None,
) -> typing.List["ComponentSpec"]:
    for spec in auto_source_comp_specs:
        bt2_utils._check_type(spec, AutoSourceComponentSpec)

    inputs = bt2_value.ArrayValue([spec.input for spec in auto_source_comp_specs])
    log_levels = bt2_value.ArrayValue(
        [
            (
                bt2_value.UnsignedIntegerValue(spec.logging_level.value)
                if spec.logging_level is not None
                else None
            )
            for spec in auto_source_comp_specs
        ]
    )

    if plugin_set is None:
        plugin_set = _find_plugins()
    else:
        bt2_utils._check_type(plugin_set, bt2_plugin._PluginSet)

    comp_specs = []
    used_input_indices = set()

    for (
        plugin_name,
        class_name,
        comp_inputs,
        comp_orig_indices,
    ) in native_bt.auto_discover_source_components(
        inputs._ptr, log_levels._ptr, plugin_set._ptr
    ):
        params = bt2_value.MapValue()
        logging_level = bt2_logging.LoggingLevel.NONE
        obj = None

        # Compute `params` for this component by piling up params given
        # to all `AutoSourceComponentSpec` objects that contributed in
        # the instantiation of this component.
        #
        # The effective log level for a component is the last one
        # specified across the `AutoSourceComponentSpec` that
        # contributed in its instantiation.
        for idx in comp_orig_indices:
            orig_spec = auto_source_comp_specs[idx]

            if orig_spec.params is not None:
                params.update(orig_spec.params)

            if orig_spec.logging_level is not None:
                logging_level = orig_spec.logging_level

            if orig_spec.obj is not AutoSourceComponentSpec._no_obj:
                obj = orig_spec.obj

            used_input_indices.add(int(idx))

        params["inputs"] = comp_inputs

        comp_cls = _src_comp_cls_from_plugin_set(plugin_set, plugin_name, class_name)

        # The automatic source component discovery mechanism returned this
        # plugin/class name from `plugin_set`, so it's expected to be found.
        assert comp_cls is not None

        comp_specs.append(
            ComponentSpec(comp_cls, params=params, obj=obj, logging_level=logging_level)
        )

    if len(used_input_indices) != len(inputs):
        unused_input_indices = set(range(len(inputs))) - used_input_indices
        unused_input_indices = sorted(unused_input_indices)
        unused_inputs = [str(inputs[x]) for x in unused_input_indices]

        msg = f"Some auto source component specs did not produce any component: {', '.join(unused_inputs)}"
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
            f'"{obj}" is not an integral number or a datetime.datetime object'
        )

    return int(s * 1e9)


class _TraceCollectionMessageIteratorProxySink(bt2_component._UserSinkComponent):
    def __init__(self, config, params, msg_list):
        assert type(msg_list) is list
        self._msg_list = msg_list
        self._add_input_port("in")

    def _user_graph_is_configured(self):
        self._msg_iter = self._create_message_iterator(self._input_ports["in"])

    def _user_consume(self):
        assert self._msg_list[0] is None
        self._msg_list[0] = next(self._msg_iter)


class TraceCollectionMessageIterator(bt2_message_iterator._MessageIterator):
    def __init__(
        self,
        source_component_specs: typing.Iterable[ComponentSpec],
        filter_component_specs: typing.Optional[typing.Iterable[ComponentSpec]] = None,
        stream_intersection_mode: bool = False,
        begin: typing.Union[None, numbers.Real, datetime.datetime] = None,
        end: typing.Union[None, numbers.Real, datetime.datetime] = None,
        plugin_set: typing.Optional[bt2_plugin._PluginSet] = None,
    ):
        bt2_utils._check_bool(stream_intersection_mode)
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
        self._src_comp_specs += source_component_specs_from_auto_source_component_specs(
            auto_src_comp_specs, plugin_set
        )

        self._flt_comp_specs = filter_component_specs
        self._next_suffix = 1
        self._connect_ports = False
        self._src_comps_and_specs: typing.List[_ComponentAndSpec] = []

        self._build_graph()

    def _compute_stream_intersections(self):
        # Pre-compute the trimmer range to use for each port in the graph, when
        # stream intersection mode is enabled.
        self._stream_inter_port_to_range = {}

        for src_comp_and_spec in self._src_comps_and_specs:
            # Query the port's component for the `babeltrace.trace-infos`
            # object which contains the range for each stream, from which we can
            # compute the intersection of the streams in each trace.
            query_exec = bt2_query_executor.QueryExecutor(
                src_comp_and_spec.spec.component_class,
                "babeltrace.trace-infos",
                src_comp_and_spec.spec.params,
            )
            trace_infos = query_exec.query()

            for trace_info in trace_infos:
                begin = max(
                    [
                        stream["range-ns"]["begin"]
                        for stream in trace_info["stream-infos"]
                    ]
                )
                end = min(
                    [stream["range-ns"]["end"] for stream in trace_info["stream-infos"]]
                )

                # Each port associated to this trace will have this computed
                # range.
                for stream in trace_info["stream-infos"]:
                    # A port name is unique within a component, but not
                    # necessarily across all components.  Use a component
                    # and port name pair to make it unique across the graph.
                    port_name = str(stream["port-name"])
                    key = (src_comp_and_spec.comp.addr, port_name)
                    self._stream_inter_port_to_range[key] = (begin, end)

    def _validate_source_component_specs(self, comp_specs):
        for comp_spec in comp_specs:
            if (
                type(comp_spec) is not ComponentSpec
                and type(comp_spec) is not AutoSourceComponentSpec
            ):
                raise TypeError(
                    f'"{type(comp_spec)}" object is not a ComponentSpec or AutoSourceComponentSpec'
                )

    def _validate_filter_component_specs(self, comp_specs):
        for comp_spec in comp_specs:
            if type(comp_spec) is not ComponentSpec:
                raise TypeError(f'"{type(comp_spec)}" object is not a ComponentSpec')

    def __next__(self) -> bt2_message._MessageConst:
        assert self._msg_list[0] is None
        self._graph.run_once()
        msg = self._msg_list[0]
        assert msg is not None
        self._msg_list[0] = None
        return msg

    def _create_stream_intersection_trimmer(self, component, port):
        key = (component.addr, port.name)
        begin, end = self._stream_inter_port_to_range[key]
        name = f"trimmer-{component.name}-{port.name}"
        return self._create_trimmer(begin, end, name)

    def _create_muxer(self):
        return self._add_component(_utils_filter_comp_cls("muxer"), "muxer")

    def _create_trimmer(self, begin_ns, end_ns, name):
        params = {}

        def ns_to_string(ns):
            s_part = ns // 1000000000
            ns_part = ns % 1000000000
            return f"{s_part}.{ns_part:09d}"

        if begin_ns is not None:
            params["begin"] = ns_to_string(begin_ns)

        if end_ns is not None:
            params["end"] = ns_to_string(end_ns)

        return self._add_component(_utils_filter_comp_cls("trimmer"), name, params)

    # Make `name` unique within the graph by appending a suffix. All
    # components added to the graph go through this so that an internal
    # component (muxer, trimmer, proxy sink) can't clash with a user source or
    # filter component class that happens to have the same name.
    def _get_unique_comp_name(self, name):
        name += f"-{self._next_suffix}"
        self._next_suffix += 1
        return name

    # Add a component to the graph, giving it a unique name derived from
    # `name`.
    def _add_component(
        self,
        comp_cls,
        name,
        params=None,
        obj=None,
        logging_level=bt2_logging.LoggingLevel.NONE,
    ):
        return self._graph.add_component(
            comp_cls, self._get_unique_comp_name(name), params, obj, logging_level
        )

    def _create_comp(self, comp_spec):
        return self._add_component(
            comp_spec.component_class,
            comp_spec.component_class.name,
            comp_spec.params,
            comp_spec.obj,
            comp_spec.logging_level,
        )

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
            self._graph.connect_ports(port, trimmer_comp.input_ports["in"])
            port_to_muxer = trimmer_comp.output_ports["out"]
        else:
            port_to_muxer = port

        self._graph.connect_ports(port_to_muxer, self._get_free_muxer_input_port())

    def _add_port_added_listener(self):
        # This prevents the circular reference between
        # `TraceCollectionMessageIterator` and `Graph` that would
        # otherwise prevent garbage collection due to __del__() methods
        # in the `_SharedObject` inheritance chain.
        weak_method = weakref.WeakMethod(self._graph_port_added)

        def port_added_listener(component, port):
            method = weak_method()

            if method is None:
                # Object was garbage-collected: nothing to do
                return

            return method(component, port)

        self._graph.add_port_added_listener(port_added_listener)

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
                    bt2_component_descriptor.ComponentDescriptor(
                        comp_spec.component_class, comp_spec.params, comp_spec.obj
                    )
                )

        descriptors = []
        append_comp_specs_descriptors(descriptors, self._src_comp_specs)
        append_comp_specs_descriptors(descriptors, self._flt_comp_specs)

        if self._stream_intersection_mode:
            # we also need at least one `flt.utils.trimmer` component
            append_comp_specs_descriptors(
                descriptors, [ComponentSpec(_utils_filter_comp_cls("trimmer"))]
            )

        mip_version = bt2_mip.get_greatest_operative_mip_version(descriptors)

        if mip_version is None:
            msg = "failed to find an operative message interchange protocol version (components are not interoperable)"
            raise RuntimeError(msg)

        return mip_version

    def _build_graph(self):
        self._graph = bt2_graph.Graph(self._get_greatest_operative_mip_version())
        self._add_port_added_listener()
        self._muxer_comp = self._create_muxer()
        last_flt_out_port = self._muxer_comp.output_ports["out"]

        if self._begin_ns is not None or self._end_ns is not None:
            trimmer_comp = self._create_trimmer(self._begin_ns, self._end_ns, "trimmer")
            self._graph.connect_ports(last_flt_out_port, trimmer_comp.input_ports["in"])
            last_flt_out_port = trimmer_comp.output_ports["out"]

        # Create and connect extra filter components (chained)
        for spec in self._flt_comp_specs:
            comp = self._create_comp(spec)
            self._graph.connect_ports(
                last_flt_out_port, list(comp.input_ports.values())[0]
            )
            last_flt_out_port = list(comp.output_ports.values())[0]

        # Here we create the components, self._graph_port_added() is
        # called when they add ports, but the callback returns early
        # because self._connect_ports is False. This is because the
        # self._graph_port_added() could not find the associated source
        # component specification in self._src_comps_and_specs because
        # it does not exist yet (it needs the created component to
        # exist).
        for comp_spec in self._src_comp_specs:
            self._src_comps_and_specs.append(
                _ComponentAndSpec(self._create_comp(comp_spec), comp_spec)
            )

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
        sink = self._add_component(
            _TraceCollectionMessageIteratorProxySink, "proxy-sink", obj=self._msg_list
        )
        sink_in_port = sink.input_ports["in"]

        # connect last filter to proxy sink
        self._graph.connect_ports(last_flt_out_port, sink_in_port)
