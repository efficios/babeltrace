# SPDX-License-Identifier: MIT
#
# Copyright (c) 2016-2017 Philippe Proulx <pproulx@efficios.com>

from bt2 import native_bt, object, utils
from bt2 import event_class as bt2_event_class
from bt2 import packet as bt2_packet
from bt2 import stream as bt2_stream
from bt2 import field as bt2_field
import collections.abc


class _EventConst(object._UniqueObject, collections.abc.Mapping):
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

    def __iter__(self):
        # To only yield unique keys, keep a set of member names that are
        # already yielded. Two root structure fields (for example,
        # payload and common context) can contain immediate members
        # which share the same name.
        member_names = set()

        if self.payload_field is not None:
            for field_name in self.payload_field:
                yield field_name
                member_names.add(field_name)

        if self.specific_context_field is not None:
            for field_name in self.specific_context_field:
                if field_name not in member_names:
                    yield field_name
                    member_names.add(field_name)

        if self.common_context_field is not None:
            for field_name in self.common_context_field:
                if field_name not in member_names:
                    yield field_name
                    member_names.add(field_name)

        if self.packet and self.packet.context_field is not None:
            for field_name in self.packet.context_field:
                if field_name not in member_names:
                    yield field_name
                    member_names.add(field_name)

    def __len__(self):
        return sum(1 for _ in self)


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
