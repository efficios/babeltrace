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
import bt2.connection
import bt2.component
import functools
import bt2.port
import bt2


class GraphListenerType:
    PORT_ADDED = 0
    PORT_REMOVED = 1
    PORTS_CONNECTED = 2
    PORTS_DISCONNECTED = 3


def _graph_port_added_listener_from_native(user_listener, port_ptr):
    try:
        port = bt2.port._create_from_ptr(port_ptr)
        port._get()
        user_listener(port)
    except:
        pass


def _graph_port_removed_listener_from_native(user_listener, port_ptr):
    try:
        port = bt2.port._create_from_ptr(port_ptr)
        port._get()
        user_listener(port)
    except:
        pass


def _graph_ports_connected_listener_from_native(user_listener,
                                                upstream_port_ptr,
                                                downstream_port_ptr):
    try:
        upstream_port = bt2.port._create_from_ptr(upstream_port_ptr)
        upstream_port._get()
        downstream_port = bt2.port._create_from_ptr(downstream_port_ptr)
        downstream_port._get()
        user_listener(upstream_port, downstream_port)
    except:
        pass


def _graph_ports_disconnected_listener_from_native(user_listener,
                                                   upstream_comp_ptr,
                                                   downstream_comp_ptr,
                                                   upstream_port_ptr,
                                                   downstream_port_ptr):
    try:
        upstream_comp = bt2.component._create_generic_component_from_ptr(upstream_comp_ptr)
        upstream_comp._get()
        downstream_comp = bt2.component._create_generic_component_from_ptr(downstream_comp_ptr)
        downstream_comp._get()
        upstream_port = bt2.port._create_from_ptr(upstream_port_ptr)
        upstream_port._get()
        downstream_port = bt2.port._create_from_ptr(downstream_port_ptr)
        downstream_port._get()
        user_listener(upstream_comp, downstream_comp, upstream_port,
                      downstream_port)
    except:
        pass


class Graph(object._SharedObject):
    def __init__(self):
        ptr = native_bt.graph_create()

        if ptr is None:
            raise bt2.CreationError('cannot create graph object')

        super().__init__(ptr)

    def _handle_status(self, status, gen_error_msg):
        if status == native_bt.GRAPH_STATUS_COMPONENT_REFUSES_PORT_CONNECTION:
            raise bt2.PortConnectionRefused
        elif status == native_bt.GRAPH_STATUS_CANCELED:
            raise bt2.GraphCanceled
        elif status == native_bt.GRAPH_STATUS_END:
            raise bt2.Stop
        elif status == native_bt.GRAPH_STATUS_AGAIN:
            raise bt2.TryAgain
        elif status == native_bt.GRAPH_STATUS_NO_SINK:
            raise bt2.NoSinkComponent
        elif status == native_bt.GRAPH_STATUS_CANNOT_CONSUME:
            raise bt2.CannotConsumeGraph
        elif status < 0:
            raise bt2.Error(gen_error_msg)

    def add_component(self, component_class, name, params=None):
        if isinstance(component_class, bt2.component._GenericComponentClass):
            cc_ptr = component_class._ptr
        elif issubclass(component_class, bt2.component._UserComponent):
            cc_ptr = component_class._cc_ptr
        else:
            raise TypeError("'{}' is not a component class".format(component_class.__class__.__name__))

        utils._check_str(name)
        params = bt2.create_value(params)

        if params is None:
            params_ptr = None
        else:
            params_ptr = params._ptr

        status, comp_ptr = native_bt.graph_add_component(self._ptr, cc_ptr,
                                                         name, params_ptr)
        self._handle_status(status, 'cannot add component to graph')
        assert(comp_ptr)
        return bt2.component._create_generic_component_from_ptr(comp_ptr)

    def connect_ports(self, upstream_port, downstream_port):
        utils._check_type(upstream_port, bt2.port._OutputPort)
        utils._check_type(downstream_port, bt2.port._InputPort)
        status, conn_ptr = native_bt.graph_connect_ports(self._ptr,
                                                         upstream_port._ptr,
                                                         downstream_port._ptr)
        self._handle_status(status, 'cannot connect component ports within graph')
        assert(conn_ptr)
        return bt2.connection._Connection._create_from_ptr(conn_ptr)

    def add_listener(self, listener_type, listener):
        if not hasattr(listener, '__call__'):
            raise TypeError("'listener' parameter is not callable")

        if listener_type == GraphListenerType.PORT_ADDED:
            fn = native_bt.py3_graph_add_port_added_listener
            listener_from_native = functools.partial(_graph_port_added_listener_from_native,
                                                     listener)
        elif listener_type == GraphListenerType.PORT_REMOVED:
            fn = native_bt.py3_graph_add_port_removed_listener
            listener_from_native = functools.partial(_graph_port_removed_listener_from_native,
                                                     listener)
        elif listener_type == GraphListenerType.PORTS_CONNECTED:
            fn = native_bt.py3_graph_add_ports_connected_listener
            listener_from_native = functools.partial(_graph_ports_connected_listener_from_native,
                                                     listener)
        elif listener_type == GraphListenerType.PORTS_DISCONNECTED:
            fn = native_bt.py3_graph_add_ports_disconnected_listener
            listener_from_native = functools.partial(_graph_ports_disconnected_listener_from_native,
                                                     listener)
        else:
            raise TypeError

        listener_id = fn(self._ptr, listener_from_native)
        utils._handle_ret(listener_id, 'cannot add listener to graph object')
        return bt2._ListenerHandle(listener_id, self)

    def run(self):
        status = native_bt.graph_run(self._ptr)

        if status == native_bt.GRAPH_STATUS_END:
            return

        self._handle_status(status, 'graph object stopped running because of an unexpected error')

    def cancel(self):
        status = native_bt.graph_cancel(self._ptr)
        self._handle_status(status, 'cannot cancel graph object')

    @property
    def is_canceled(self):
        is_canceled = native_bt.graph_is_canceled(self._ptr)
        assert(is_canceled >= 0)
        return is_canceled > 0

    def __eq__(self, other):
        if type(other) is not type(self):
            return False

        return self.addr == other.addr
