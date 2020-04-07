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

from bt2 import native_bt, object, utils
from bt2 import event_class as bt2_event_class
from bt2 import packet as bt2_packet
from bt2 import stream as bt2_stream
from bt2 import field as bt2_field


class _EventConst(object._UniqueObject):
    _borrow_class_ptr = staticmethod(native_bt.event_borrow_class_const)
    _borrow_packet_ptr = staticmethod(native_bt.event_borrow_packet_const)
    _borrow_stream_ptr = staticmethod(native_bt.event_borrow_stream_const)
    _borrow_common_context_field_ptr = staticmethod(
        native_bt.event_borrow_common_context_field_const
    )
    _borrow_specific_context_field_ptr = staticmethod(
        native_bt.event_borrow_specific_context_field_const
    )
    _borrow_payload_field_ptr = staticmethod(native_bt.event_borrow_payload_field_const)
    _create_field_from_ptr = staticmethod(bt2_field._create_field_from_const_ptr)

    _event_class_pycls = property(lambda _: bt2_event_class._EventClassConst)
    _packet_pycls = property(lambda _: bt2_packet._PacketConst)
    _stream_pycls = property(lambda _: bt2_stream._StreamConst)

    @property
    def cls(self):
        event_class_ptr = self._borrow_class_ptr(self._ptr)
        assert event_class_ptr is not None
        return self._event_class_pycls._create_from_ptr_and_get_ref(event_class_ptr)

    @property
    def name(self):
        return self.cls.name

    @property
    def id(self):
        return self.cls.id

    @property
    def packet(self):
        packet_ptr = self._borrow_packet_ptr(self._ptr)

        if packet_ptr is None:
            return

        return self._packet_pycls._create_from_ptr_and_get_ref(packet_ptr)

    @property
    def stream(self):
        stream_ptr = self._borrow_stream_ptr(self._ptr)
        assert stream_ptr is not None
        return self._stream_pycls._create_from_ptr_and_get_ref(stream_ptr)

    @property
    def common_context_field(self):
        field_ptr = self._borrow_common_context_field_ptr(self._ptr)

        if field_ptr is None:
            return

        return self._create_field_from_ptr(
            field_ptr, self._owner_ptr, self._owner_get_ref, self._owner_put_ref
        )

    @property
    def specific_context_field(self):
        field_ptr = self._borrow_specific_context_field_ptr(self._ptr)

        if field_ptr is None:
            return

        return self._create_field_from_ptr(
            field_ptr, self._owner_ptr, self._owner_get_ref, self._owner_put_ref
        )

    @property
    def payload_field(self):
        field_ptr = self._borrow_payload_field_ptr(self._ptr)

        if field_ptr is None:
            return

        return self._create_field_from_ptr(
            field_ptr, self._owner_ptr, self._owner_get_ref, self._owner_put_ref
        )

    def __getitem__(self, key):
        utils._check_str(key)
        payload_field = self.payload_field

        if payload_field is not None and key in payload_field:
            return payload_field[key]

        specific_context_field = self.specific_context_field

        if specific_context_field is not None and key in specific_context_field:
            return specific_context_field[key]

        common_context_field = self.common_context_field

        if common_context_field is not None and key in common_context_field:
            return common_context_field[key]

        if self.packet:
            packet_context_field = self.packet.context_field

            if packet_context_field is not None and key in packet_context_field:
                return packet_context_field[key]

        raise KeyError(key)


class _Event(_EventConst):
    _borrow_class_ptr = staticmethod(native_bt.event_borrow_class)
    _borrow_packet_ptr = staticmethod(native_bt.event_borrow_packet)
    _borrow_stream_ptr = staticmethod(native_bt.event_borrow_stream)
    _borrow_common_context_field_ptr = staticmethod(
        native_bt.event_borrow_common_context_field
    )
    _borrow_specific_context_field_ptr = staticmethod(
        native_bt.event_borrow_specific_context_field
    )
    _borrow_payload_field_ptr = staticmethod(native_bt.event_borrow_payload_field)
    _create_field_from_ptr = staticmethod(bt2_field._create_field_from_ptr)

    _event_class_pycls = property(lambda _: bt2_event_class._EventClass)
    _packet_pycls = property(lambda _: bt2_packet._Packet)
    _stream_pycls = property(lambda _: bt2_stream._Stream)
