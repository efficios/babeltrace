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
import bt2.message_iterator
import collections.abc
import bt2.port
import copy
import bt2


def _handle_status(status, gen_error_msg):
    if status == native_bt.CONNECTION_STATUS_GRAPH_IS_CANCELED:
        raise bt2.GraphCanceled
    elif status == native_bt.CONNECTION_STATUS_IS_ENDED:
        raise bt2.ConnectionEnded
    elif status < 0:
        raise bt2.Error(gen_error_msg)


def _create_private_from_ptr(ptr):
    obj = _PrivateConnection._create_from_ptr(ptr)
    obj._pub_ptr = native_bt.connection_from_private(ptr)
    assert(obj._pub_ptr)
    return obj


class _Connection(object._Object):
    @staticmethod
    def _downstream_port(ptr):
        port_ptr = native_bt.connection_get_downstream_port(ptr)
        utils._handle_ptr(port_ptr, "cannot get connection object's downstream port object")
        return bt2.port._create_from_ptr(port_ptr)

    @staticmethod
    def _upstream_port(ptr):
        port_ptr = native_bt.connection_get_upstream_port(ptr)
        utils._handle_ptr(port_ptr, "cannot get connection object's upstream port object")
        return bt2.port._create_from_ptr(port_ptr)

    @property
    def downstream_port(self):
        return self._downstream_port(self._ptr)

    @property
    def upstream_port(self):
        return self._upstream_port(self._ptr)

    @staticmethod
    def _is_ended(ptr):
        return native_bt.connection_is_ended(ptr) == 1

    @property
    def is_ended(self):
        return self._is_ended(self._ptr)

    def __eq__(self, other):
        if type(other) not in (_Connection, _PrivateConnection):
            return False

        return self.addr == other.addr


class _PrivateConnection(object._PrivateObject, _Connection):
    def create_message_iterator(self, message_types=None):
        msg_types = bt2.message._msg_types_from_msg_classes(message_types)
        status, msg_iter_ptr = native_bt.py3_create_priv_conn_msg_iter(int(self._ptr),
                                                                           msg_types)
        _handle_status(status, 'cannot create message iterator object')
        assert(msg_iter_ptr)
        return bt2.message_iterator._PrivateConnectionMessageIterator._create_from_ptr(msg_iter_ptr)

    @property
    def is_ended(self):
        return self._is_ended(self._pub_ptr)

    @property
    def downstream_port(self):
        return self._downstream_port(self._pub_ptr)

    @property
    def upstream_port(self):
        return self._upstream_port(self._pub_ptr)
