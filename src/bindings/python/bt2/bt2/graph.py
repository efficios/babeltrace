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
import bt2.logging
import bt2


def _graph_port_added_listener_from_native(user_listener,
                                           component_ptr, component_type,
                                           port_ptr, port_type):
    component = bt2.component._create_component_from_ptr_and_get_ref(component_ptr, component_type)
    port = bt2.port._create_from_ptr_and_get_ref(port_ptr, port_type)
    user_listener(component, port)


def _graph_ports_connected_listener_from_native(user_listener,
                                                upstream_component_ptr, upstream_component_type,
                                                upstream_port_ptr,
                                                downstream_component_ptr, downstream_component_type,
                                                downstream_port_ptr):
    upstream_component = bt2.component._create_component_from_ptr_and_get_ref(
        upstream_component_ptr, upstream_component_type)
    upstream_port = bt2.port._create_from_ptr_and_get_ref(
        upstream_port_ptr, native_bt.PORT_TYPE_OUTPUT)
    downstream_component = bt2.component._create_component_from_ptr_and_get_ref(
        downstream_component_ptr, downstream_component_type)
    downstream_port = bt2.port._create_from_ptr_and_get_ref(
        downstream_port_ptr, native_bt.PORT_TYPE_INPUT)
    user_listener(upstream_component, upstream_port, downstream_component, downstream_port)


class Graph(object._SharedObject):
    _get_ref = staticmethod(native_bt.graph_get_ref)
    _put_ref = staticmethod(native_bt.graph_put_ref)

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
        elif status < 0:
            raise bt2.Error(gen_error_msg)

    def add_component(self, component_class, name, params=None,
                      logging_level=bt2.logging.LoggingLevel.NONE):
        if isinstance(component_class, bt2.component._GenericSourceComponentClass):
            cc_ptr = component_class._ptr
            add_fn = native_bt.graph_add_source_component
            cc_type = native_bt.COMPONENT_CLASS_TYPE_SOURCE
        elif isinstance(component_class, bt2.component._GenericFilterComponentClass):
            cc_ptr = component_class._ptr
            add_fn = native_bt.graph_add_filter_component
            cc_type = native_bt.COMPONENT_CLASS_TYPE_FILTER
        elif isinstance(component_class, bt2.component._GenericSinkComponentClass):
            cc_ptr = component_class._ptr
            add_fn = native_bt.graph_add_sink_component
            cc_type = native_bt.COMPONENT_CLASS_TYPE_SINK
        elif issubclass(component_class, bt2.component._UserSourceComponent):
            cc_ptr = component_class._cc_ptr
            add_fn = native_bt.graph_add_source_component
            cc_type = native_bt.COMPONENT_CLASS_TYPE_SOURCE
        elif issubclass(component_class, bt2.component._UserSinkComponent):
            cc_ptr = component_class._cc_ptr
            add_fn = native_bt.graph_add_sink_component
            cc_type = native_bt.COMPONENT_CLASS_TYPE_SINK
        elif issubclass(component_class, bt2.component._UserFilterComponent):
            cc_ptr = component_class._cc_ptr
            add_fn = native_bt.graph_add_filter_component
            cc_type = native_bt.COMPONENT_CLASS_TYPE_FILTER
        else:
            raise TypeError("'{}' is not a component class".format(
                component_class.__class__.__name__))

        utils._check_str(name)
        utils._check_log_level(logging_level)
        params = bt2.create_value(params)

        params_ptr = params._ptr if params is not None else None

        status, comp_ptr = add_fn(self._ptr, cc_ptr, name,
                                  params_ptr, logging_level)
        self._handle_status(status, 'cannot add component to graph')
        assert comp_ptr
        return bt2.component._create_component_from_ptr(comp_ptr, cc_type)

    def connect_ports(self, upstream_port, downstream_port):
        utils._check_type(upstream_port, bt2.port._OutputPort)
        utils._check_type(downstream_port, bt2.port._InputPort)
        status, conn_ptr = native_bt.graph_connect_ports(self._ptr,
                                                         upstream_port._ptr,
                                                         downstream_port._ptr)
        self._handle_status(status, 'cannot connect component ports within graph')
        assert(conn_ptr)
        return bt2.connection._Connection._create_from_ptr(conn_ptr)

    def add_port_added_listener(self, listener):
        if not callable(listener):
            raise TypeError("'listener' parameter is not callable")

        fn = native_bt.py3_graph_add_port_added_listener
        listener_from_native = functools.partial(_graph_port_added_listener_from_native,
                                                 listener)

        listener_ids = fn(self._ptr, listener_from_native)
        if listener_ids is None:
            utils._raise_bt2_error('cannot add listener to graph object')
        return bt2._ListenerHandle(listener_ids, self)

    def add_ports_connected_listener(self, listener):
        if not callable(listener):
            raise TypeError("'listener' parameter is not callable")

        fn = native_bt.py3_graph_add_ports_connected_listener
        listener_from_native = functools.partial(_graph_ports_connected_listener_from_native,
                                                 listener)

        listener_ids = fn(self._ptr, listener_from_native)
        if listener_ids is None:
            utils._raise_bt2_error('cannot add listener to graph object')
        return bt2._ListenerHandle(listener_ids, self)

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

    def create_output_port_message_iterator(self, output_port):
        utils._check_type(output_port, bt2.port._OutputPort)
        msg_iter_ptr = native_bt.port_output_message_iterator_create(self._ptr, output_port._ptr)

        if msg_iter_ptr is None:
            raise bt2.CreationError('cannot create output port message iterator')

        return bt2.message_iterator._OutputPortMessageIterator(msg_iter_ptr)
