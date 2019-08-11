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

from bt2 import native_bt, object, utils
from bt2 import interrupter as bt2_interrupter
from bt2 import connection as bt2_connection
from bt2 import component as bt2_component
import functools
from bt2 import port as bt2_port
from bt2 import logging as bt2_logging
import bt2


def _graph_port_added_listener_from_native(
    user_listener, component_ptr, component_type, port_ptr, port_type
):
    component = bt2_component._create_component_from_ptr_and_get_ref(
        component_ptr, component_type
    )
    port = bt2_port._create_from_ptr_and_get_ref(port_ptr, port_type)
    user_listener(component, port)


def _graph_ports_connected_listener_from_native(
    user_listener,
    upstream_component_ptr,
    upstream_component_type,
    upstream_port_ptr,
    downstream_component_ptr,
    downstream_component_type,
    downstream_port_ptr,
):
    upstream_component = bt2_component._create_component_from_ptr_and_get_ref(
        upstream_component_ptr, upstream_component_type
    )
    upstream_port = bt2_port._create_from_ptr_and_get_ref(
        upstream_port_ptr, native_bt.PORT_TYPE_OUTPUT
    )
    downstream_component = bt2_component._create_component_from_ptr_and_get_ref(
        downstream_component_ptr, downstream_component_type
    )
    downstream_port = bt2_port._create_from_ptr_and_get_ref(
        downstream_port_ptr, native_bt.PORT_TYPE_INPUT
    )
    user_listener(
        upstream_component, upstream_port, downstream_component, downstream_port
    )


class Graph(object._SharedObject):
    _get_ref = staticmethod(native_bt.graph_get_ref)
    _put_ref = staticmethod(native_bt.graph_put_ref)

    def __init__(self, mip_version=0):
        utils._check_uint64(mip_version)

        if mip_version > bt2.get_maximal_mip_version():
            raise ValueError('unknown MIP version {}'.format(mip_version))

        ptr = native_bt.graph_create(mip_version)

        if ptr is None:
            raise bt2._MemoryError('cannot create graph object')

        super().__init__(ptr)

    def add_component(
        self,
        component_class,
        name,
        params=None,
        obj=None,
        logging_level=bt2_logging.LoggingLevel.NONE,
    ):
        if isinstance(component_class, bt2_component._SourceComponentClass):
            cc_ptr = component_class._ptr
            add_fn = native_bt.bt2_graph_add_source_component
            cc_type = native_bt.COMPONENT_CLASS_TYPE_SOURCE
        elif isinstance(component_class, bt2_component._FilterComponentClass):
            cc_ptr = component_class._ptr
            add_fn = native_bt.bt2_graph_add_filter_component
            cc_type = native_bt.COMPONENT_CLASS_TYPE_FILTER
        elif isinstance(component_class, bt2_component._SinkComponentClass):
            cc_ptr = component_class._ptr
            add_fn = native_bt.bt2_graph_add_sink_component
            cc_type = native_bt.COMPONENT_CLASS_TYPE_SINK
        elif issubclass(component_class, bt2_component._UserSourceComponent):
            cc_ptr = component_class._bt_cc_ptr
            add_fn = native_bt.bt2_graph_add_source_component
            cc_type = native_bt.COMPONENT_CLASS_TYPE_SOURCE
        elif issubclass(component_class, bt2_component._UserSinkComponent):
            cc_ptr = component_class._bt_cc_ptr
            add_fn = native_bt.bt2_graph_add_sink_component
            cc_type = native_bt.COMPONENT_CLASS_TYPE_SINK
        elif issubclass(component_class, bt2_component._UserFilterComponent):
            cc_ptr = component_class._bt_cc_ptr
            add_fn = native_bt.bt2_graph_add_filter_component
            cc_type = native_bt.COMPONENT_CLASS_TYPE_FILTER
        else:
            raise TypeError(
                "'{}' is not a component class".format(
                    component_class.__class__.__name__
                )
            )

        utils._check_str(name)
        utils._check_log_level(logging_level)
        base_cc_ptr = component_class._bt_component_class_ptr()

        if obj is not None and not native_bt.bt2_is_python_component_class(base_cc_ptr):
            raise ValueError('cannot pass a Python object to a non-Python component')

        params = bt2.create_value(params)
        params_ptr = params._ptr if params is not None else None

        status, comp_ptr = add_fn(
            self._ptr, cc_ptr, name, params_ptr, obj, logging_level
        )
        utils._handle_func_status(status, 'cannot add component to graph')
        assert comp_ptr
        return bt2_component._create_component_from_ptr(comp_ptr, cc_type)

    def connect_ports(self, upstream_port, downstream_port):
        utils._check_type(upstream_port, bt2_port._OutputPort)
        utils._check_type(downstream_port, bt2_port._InputPort)
        status, conn_ptr = native_bt.graph_connect_ports(
            self._ptr, upstream_port._ptr, downstream_port._ptr
        )
        utils._handle_func_status(status, 'cannot connect component ports within graph')
        assert conn_ptr
        return bt2_connection._Connection._create_from_ptr(conn_ptr)

    def add_port_added_listener(self, listener):
        if not callable(listener):
            raise TypeError("'listener' parameter is not callable")

        fn = native_bt.bt2_graph_add_port_added_listener
        listener_from_native = functools.partial(
            _graph_port_added_listener_from_native, listener
        )

        listener_ids = fn(self._ptr, listener_from_native)
        if listener_ids is None:
            raise bt2._Error('cannot add listener to graph object')

        return utils._ListenerHandle(listener_ids, self)

    def add_ports_connected_listener(self, listener):
        if not callable(listener):
            raise TypeError("'listener' parameter is not callable")

        fn = native_bt.bt2_graph_add_ports_connected_listener
        listener_from_native = functools.partial(
            _graph_ports_connected_listener_from_native, listener
        )

        listener_ids = fn(self._ptr, listener_from_native)
        if listener_ids is None:
            raise bt2._Error('cannot add listener to graph object')

        return utils._ListenerHandle(listener_ids, self)

    def run_once(self):
        status = native_bt.graph_run_once(self._ptr)
        utils._handle_func_status(status, 'graph object could not run once')

    def run(self):
        status = native_bt.graph_run(self._ptr)

        try:
            utils._handle_func_status(status, 'graph object stopped running')
        except bt2.Stop:
            # done
            return
        except Exception:
            raise

    def add_interrupter(self, interrupter):
        utils._check_type(interrupter, bt2_interrupter.Interrupter)
        native_bt.graph_add_interrupter(self._ptr, interrupter._ptr)

    def interrupt(self):
        native_bt.graph_interrupt(self._ptr)
