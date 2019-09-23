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

from bt2 import native_bt, object


def _bt2_connection():
    from bt2 import connection as bt2_connection

    return bt2_connection


def _create_from_const_ptr_and_get_ref(ptr, port_type):
    cls = _PORT_TYPE_TO_PYCLS.get(port_type, None)

    if cls is None:
        raise TypeError('unknown port type: {}'.format(port_type))

    return cls._create_from_ptr_and_get_ref(ptr)


def _create_self_from_ptr_and_get_ref(ptr, port_type):
    cls = _PORT_TYPE_TO_USER_PYCLS.get(port_type, None)

    if cls is None:
        raise TypeError('unknown port type: {}'.format(port_type))

    return cls._create_from_ptr_and_get_ref(ptr)


class _PortConst(object._SharedObject):
    @classmethod
    def _get_ref(cls, ptr):
        ptr = cls._as_port_ptr(ptr)
        return native_bt.port_get_ref(ptr)

    @classmethod
    def _put_ref(cls, ptr):
        ptr = cls._as_port_ptr(ptr)
        return native_bt.port_put_ref(ptr)

    @property
    def name(self):
        ptr = self._as_port_ptr(self._ptr)
        name = native_bt.port_get_name(ptr)
        assert name is not None
        return name

    @property
    def connection(self):
        ptr = self._as_port_ptr(self._ptr)
        conn_ptr = native_bt.port_borrow_connection_const(ptr)

        if conn_ptr is None:
            return

        return _bt2_connection()._ConnectionConst._create_from_ptr_and_get_ref(conn_ptr)

    @property
    def is_connected(self):
        return self.connection is not None


class _InputPortConst(_PortConst):
    _as_port_ptr = staticmethod(native_bt.port_input_as_port_const)


class _OutputPortConst(_PortConst):
    _as_port_ptr = staticmethod(native_bt.port_output_as_port_const)


class _UserComponentPort(_PortConst):
    @classmethod
    def _as_port_ptr(cls, ptr):
        ptr = cls._as_self_port_ptr(ptr)
        return native_bt.self_component_port_as_port(ptr)

    @property
    def connection(self):
        ptr = self._as_port_ptr(self._ptr)
        conn_ptr = native_bt.port_borrow_connection_const(ptr)

        if conn_ptr is None:
            return

        return _bt2_connection()._ConnectionConst._create_from_ptr_and_get_ref(conn_ptr)

    @property
    def user_data(self):
        ptr = self._as_self_port_ptr(self._ptr)
        return native_bt.self_component_port_get_data(ptr)


class _UserComponentInputPort(_UserComponentPort, _InputPortConst):
    _as_self_port_ptr = staticmethod(
        native_bt.self_component_port_input_as_self_component_port
    )


class _UserComponentOutputPort(_UserComponentPort, _OutputPortConst):
    _as_self_port_ptr = staticmethod(
        native_bt.self_component_port_output_as_self_component_port
    )


_PORT_TYPE_TO_PYCLS = {
    native_bt.PORT_TYPE_INPUT: _InputPortConst,
    native_bt.PORT_TYPE_OUTPUT: _OutputPortConst,
}


_PORT_TYPE_TO_USER_PYCLS = {
    native_bt.PORT_TYPE_INPUT: _UserComponentInputPort,
    native_bt.PORT_TYPE_OUTPUT: _UserComponentOutputPort,
}
