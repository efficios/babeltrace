# The MIT License (MIT)
#
# Copyright (c) 2016-2017 Philippe Proulx <pproulx@efficios.com>
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

from bt2 import native_bt, utils
from bt2 import object as bt2_object
from bt2 import packet as bt2_packet
from bt2 import stream_class as bt2_stream_class
from bt2 import value as bt2_value
import bt2


def _bt2_trace():
    from bt2 import trace as bt2_trace

    return bt2_trace


class _StreamConst(bt2_object._SharedObject):
    _get_ref = staticmethod(native_bt.stream_get_ref)
    _put_ref = staticmethod(native_bt.stream_put_ref)
    _borrow_class_ptr = staticmethod(native_bt.stream_borrow_class_const)
    _borrow_user_attributes_ptr = staticmethod(
        native_bt.stream_borrow_user_attributes_const
    )
    _create_value_from_ptr_and_get_ref = staticmethod(
        bt2_value._create_from_const_ptr_and_get_ref
    )
    _borrow_trace_ptr = staticmethod(native_bt.stream_borrow_trace_const)
    _stream_class_pycls = bt2_stream_class._StreamClassConst
    _trace_pycls = property(lambda _: _bt2_trace()._TraceConst)

    @property
    def cls(self):
        stream_class_ptr = self._borrow_class_ptr(self._ptr)
        assert stream_class_ptr is not None
        return self._stream_class_pycls._create_from_ptr_and_get_ref(stream_class_ptr)

    @property
    def name(self):
        return native_bt.stream_get_name(self._ptr)

    @property
    def user_attributes(self):
        ptr = self._borrow_user_attributes_ptr(self._ptr)
        assert ptr is not None
        return self._create_value_from_ptr_and_get_ref(ptr)

    @property
    def id(self):
        id = native_bt.stream_get_id(self._ptr)
        return id if id >= 0 else None

    @property
    def trace(self):
        trace_ptr = self._borrow_trace_ptr(self._ptr)
        assert trace_ptr is not None
        return self._trace_pycls._create_from_ptr_and_get_ref(trace_ptr)


class _Stream(_StreamConst):
    _borrow_class_ptr = staticmethod(native_bt.stream_borrow_class)
    _borrow_user_attributes_ptr = staticmethod(native_bt.stream_borrow_user_attributes)
    _create_value_from_ptr_and_get_ref = staticmethod(
        bt2_value._create_from_ptr_and_get_ref
    )
    _borrow_trace_ptr = staticmethod(native_bt.stream_borrow_trace)
    _stream_class_pycls = bt2_stream_class._StreamClass
    _trace_pycls = property(lambda _: _bt2_trace()._Trace)

    def create_packet(self):
        if not self.cls.supports_packets:
            raise ValueError(
                'cannot create packet: stream class does not support packets'
            )

        packet_ptr = native_bt.packet_create(self._ptr)

        if packet_ptr is None:
            raise bt2._MemoryError('cannot create packet object')

        return bt2_packet._Packet._create_from_ptr(packet_ptr)

    def _user_attributes(self, user_attributes):
        value = bt2_value.create_value(user_attributes)
        utils._check_type(value, bt2_value.MapValue)
        native_bt.stream_set_user_attributes(self._ptr, value._ptr)

    _user_attributes = property(
        fget=_StreamConst.user_attributes.fget, fset=_user_attributes
    )

    def _name(self, name):
        utils._check_str(name)
        native_bt.stream_set_name(self._ptr, name)

    _name = property(fget=_StreamConst.name.fget, fset=_name)
