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
import collections.abc
import bt2.component
import bt2.connection
import bt2.message_iterator
import bt2.message
import copy
import bt2


def _create_from_ptr(ptr):
    port_type = native_bt.port_get_type(ptr)

    if port_type == native_bt.PORT_TYPE_INPUT:
        cls = _InputPort
    elif port_type == native_bt.PORT_TYPE_OUTPUT:
        cls = _OutputPort
    else:
        raise bt2.Error('unknown port type: {}'.format(port_type))

    return cls._create_from_ptr(ptr)


def _create_private_from_ptr(ptr):
    pub_ptr = native_bt.port_from_private(ptr)
    utils._handle_ptr(pub_ptr, 'cannot get port object from private port object')
    port_type = native_bt.port_get_type(pub_ptr)
    assert(port_type == native_bt.PORT_TYPE_INPUT or port_type == native_bt.PORT_TYPE_OUTPUT)

    if port_type == native_bt.PORT_TYPE_INPUT:
        cls = _PrivateInputPort
    elif port_type == native_bt.PORT_TYPE_OUTPUT:
        cls = _PrivateOutputPort

    obj = cls._create_from_ptr(ptr)
    obj._pub_ptr = pub_ptr
    return obj


class _Port(object._SharedObject):
    @staticmethod
    def _name(ptr):
        name = native_bt.port_get_name(ptr)
        assert(name is not None)
        return name

    @staticmethod
    def _disconnect(ptr):
        status = native_bt.port_disconnect(ptr)

        if status < 0:
            raise bt2.Error('cannot disconnect port')

    @property
    def name(self):
        return self._name(self._ptr)

    @property
    def component(self):
        comp_ptr = native_bt.port_get_component(self._ptr)

        if comp_ptr is None:
            return

        return bt2.component._create_generic_component_from_ptr(comp_ptr)

    @property
    def connection(self):
        conn_ptr = native_bt.port_get_connection(self._ptr)

        if conn_ptr is None:
            return

        return bt2.connection._Connection._create_from_ptr(conn_ptr)

    @property
    def is_connected(self):
        return self.connection is not None

    def disconnect(self):
        self._disconnect(self._ptr)

    def __eq__(self, other):
        if type(other) is not type(self):
            return False

        return self.addr == other.addr


class _InputPort(_Port):
    pass


class _OutputPort(_Port):
    def create_message_iterator(self, message_types=None,
                                     colander_component_name=None):
        msg_types = bt2.message._msg_types_from_msg_classes(message_types)

        if colander_component_name is not None:
            utils._check_str(colander_component_name)

        msg_iter_ptr = native_bt.py3_create_output_port_msg_iter(int(self._ptr),
                                                                     colander_component_name,
                                                                     msg_types)

        if msg_iter_ptr is None:
            raise bt2.CreationError('cannot create output port message iterator')

        return bt2.message_iterator._OutputPortMessageIterator._create_from_ptr(msg_iter_ptr)


class _PrivatePort(_Port):
    @property
    def name(self):
        return self._name(self._pub_ptr)

    @property
    def component(self):
        comp_ptr = native_bt.private_port_get_private_component(self._ptr)

        if comp_ptr is None:
            return

        pub_comp_ptr = native_bt.component_from_private(comp_ptr)
        assert(pub_comp_ptr)
        comp = bt2.component._create_generic_component_from_ptr(pub_comp_ptr)
        native_bt.put(comp_ptr)
        return comp

    @property
    def connection(self):
        conn_ptr = native_bt.private_port_get_private_connection(self._ptr)

        if conn_ptr is None:
            return

        return bt2.connection._create_private_from_ptr(conn_ptr)

    def remove_from_component(self):
        status = native_bt.private_port_remove_from_component(self._ptr)

        if status < 0:
            raise bt2.Error("cannot remove port from component")

    def disconnect(self):
        self._disconnect(self._pub_ptr)


class _PrivateInputPort(_PrivatePort, _InputPort):
    pass


class _PrivateOutputPort(_PrivatePort, _OutputPort):
    pass
