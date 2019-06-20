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
import bt2.clock_snapshot
import bt2.packet
import bt2.stream
import bt2.event
import bt2


def _create_from_ptr(ptr):
    msg_type = native_bt.message_get_type(ptr)

    if msg_type not in _MESSAGE_TYPE_TO_CLS:
        raise bt2.Error('unknown message type: {}'.format(msg_type))

    return _MESSAGE_TYPE_TO_CLS[msg_type]._create_from_ptr(ptr)


class _Message(object._SharedObject):
    _get_ref = staticmethod(native_bt.message_get_ref)
    _put_ref = staticmethod(native_bt.message_put_ref)

    @staticmethod
    def _check_has_default_clock_class(clock_class):
        if clock_class is None:
            raise bt2.NonexistentClockSnapshot('cannot get default clock snapshot: stream class has no default clock class')


class _MessageWithDefaultClockSnapshot:
    def _get_default_clock_snapshot(self, borrow_clock_snapshot_ptr):
        snapshot_ptr = borrow_clock_snapshot_ptr(self._ptr)

        return bt2.clock_snapshot._ClockSnapshot._create_from_ptr_and_get_ref(
            snapshot_ptr, self._ptr, self._get_ref, self._put_ref)


class _EventMessage(_Message, _MessageWithDefaultClockSnapshot):
    _borrow_default_clock_snapshot_ptr = staticmethod(native_bt.message_event_borrow_default_clock_snapshot_const)

    @property
    def default_clock_snapshot(self):
        self._check_has_default_clock_class(self.event.packet.stream.cls.default_clock_class)
        return self._get_default_clock_snapshot(self._borrow_default_clock_snapshot_ptr)

    @property
    def event(self):
        event_ptr = native_bt.message_event_borrow_event(self._ptr)
        assert event_ptr is not None
        return bt2.event._Event._create_from_ptr_and_get_ref(
            event_ptr, self._ptr, self._get_ref, self._put_ref)


class _PacketMessage(_Message, _MessageWithDefaultClockSnapshot):
    @property
    def default_clock_snapshot(self):
        self._check_has_default_clock_class(self.packet.stream.cls.default_clock_class)
        return self._get_default_clock_snapshot(self._borrow_default_clock_snapshot_ptr)

    @property
    def packet(self):
        packet_ptr = self._borrow_packet_ptr(self._ptr)
        assert packet_ptr is not None
        return bt2.packet._Packet._create_from_ptr_and_get_ref(packet_ptr)


class _PacketBeginningMessage(_PacketMessage):
    _borrow_packet_ptr = staticmethod(native_bt.message_packet_beginning_borrow_packet)
    _borrow_default_clock_snapshot_ptr = staticmethod(native_bt.message_packet_beginning_borrow_default_clock_snapshot_const)


class _PacketEndMessage(_PacketMessage):
    _borrow_packet_ptr = staticmethod(native_bt.message_packet_end_borrow_packet)
    _borrow_default_clock_snapshot_ptr = staticmethod(native_bt.message_packet_end_borrow_default_clock_snapshot_const)


class _StreamMessage(_Message, _MessageWithDefaultClockSnapshot):
    @property
    def stream(self):
        stream_ptr = self._borrow_stream_ptr(self._ptr)
        assert stream_ptr
        return bt2.stream._Stream._create_from_ptr_and_get_ref(stream_ptr)

    @property
    def default_clock_snapshot(self):
        self._check_has_default_clock_class(self.stream.cls.default_clock_class)

        status, snapshot_ptr = self._borrow_default_clock_snapshot_ptr(self._ptr)

        if status == native_bt.MESSAGE_STREAM_CLOCK_SNAPSHOT_STATE_UNKNOWN:
            return bt2.clock_snapshot._UnknownClockSnapshot()

        return bt2.clock_snapshot._ClockSnapshot._create_from_ptr_and_get_ref(
            snapshot_ptr, self._ptr, self._get_ref, self._put_ref)

    def _default_clock_snapshot(self, raw_value):
        utils._check_uint64(raw_value)
        self._set_default_clock_snapshot(self._ptr, raw_value)

    _default_clock_snapshot = property(fset=_default_clock_snapshot)


class _StreamBeginningMessage(_StreamMessage):
    _borrow_stream_ptr = staticmethod(native_bt.message_stream_beginning_borrow_stream)
    _borrow_default_clock_snapshot_ptr = staticmethod(native_bt.message_stream_beginning_borrow_default_clock_snapshot_const)
    _set_default_clock_snapshot = staticmethod(native_bt.message_stream_beginning_set_default_clock_snapshot)


class _StreamEndMessage(_StreamMessage):
    _borrow_stream_ptr = staticmethod(native_bt.message_stream_end_borrow_stream)
    _borrow_default_clock_snapshot_ptr = staticmethod(native_bt.message_stream_end_borrow_default_clock_snapshot_const)
    _set_default_clock_snapshot = staticmethod(native_bt.message_stream_end_set_default_clock_snapshot)


class _MessageIteratorInactivityMessage(_Message, _MessageWithDefaultClockSnapshot):
    _borrow_default_clock_snapshot_ptr = staticmethod(native_bt.message_message_iterator_inactivity_borrow_default_clock_snapshot_const)

    @property
    def default_clock_snapshot(self):
        # This kind of message always has a default clock class: no
        # need to call self._check_has_default_clock_class() here.
        return self._get_default_clock_snapshot(self._borrow_default_clock_snapshot_ptr)


class _DiscardedMessage(_Message, _MessageWithDefaultClockSnapshot):
    @property
    def stream(self):
        stream_ptr = self._borrow_stream_ptr(self._ptr)
        assert stream_ptr
        return bt2.stream._Stream._create_from_ptr_and_get_ref(stream_ptr)

    @property
    def count(self):
        avail, count = self._get_count(self._ptr)
        if avail is native_bt.PROPERTY_AVAILABILITY_AVAILABLE:
            return count

    def _set_count(self, count):
        utils._check_uint64(count)
        self._set_count(self._ptr, count)

    _count = property(fset=_set_count)

    def _check_has_default_clock_snapshots(self):
        if not self._has_default_clock_snapshots:
            raise bt2.NonexistentClockSnapshot('cannot get default clock snapshot: such a message has no clock snapshots for this stream class')

    @property
    def beginning_default_clock_snapshot(self):
        self._check_has_default_clock_snapshots()
        return self._get_default_clock_snapshot(self._borrow_beginning_clock_snapshot_ptr)

    @property
    def end_default_clock_snapshot(self):
        self._check_has_default_clock_snapshots()
        return self._get_default_clock_snapshot(self._borrow_end_clock_snapshot_ptr)


class _DiscardedEventsMessage(_DiscardedMessage):
    _borrow_stream_ptr = staticmethod(native_bt.message_discarded_events_borrow_stream_const)
    _get_count = staticmethod(native_bt.message_discarded_events_get_count)
    _set_count = staticmethod(native_bt.message_discarded_events_set_count)
    _borrow_beginning_clock_snapshot_ptr = staticmethod(native_bt.message_discarded_events_borrow_beginning_default_clock_snapshot_const)
    _borrow_end_clock_snapshot_ptr = staticmethod(native_bt.message_discarded_events_borrow_end_default_clock_snapshot_const)

    @property
    def _has_default_clock_snapshots(self):
        return self.stream.cls.discarded_events_have_default_clock_snapshots


class _DiscardedPacketsMessage(_DiscardedMessage):
    _borrow_stream_ptr = staticmethod(native_bt.message_discarded_packets_borrow_stream_const)
    _get_count = staticmethod(native_bt.message_discarded_packets_get_count)
    _set_count = staticmethod(native_bt.message_discarded_packets_set_count)
    _borrow_beginning_clock_snapshot_ptr = staticmethod(native_bt.message_discarded_packets_borrow_beginning_default_clock_snapshot_const)
    _borrow_end_clock_snapshot_ptr = staticmethod(native_bt.message_discarded_packets_borrow_end_default_clock_snapshot_const)

    @property
    def _has_default_clock_snapshots(self):
        return self.stream.cls.discarded_packets_have_default_clock_snapshots


_MESSAGE_TYPE_TO_CLS = {
    native_bt.MESSAGE_TYPE_EVENT: _EventMessage,
    native_bt.MESSAGE_TYPE_MESSAGE_ITERATOR_INACTIVITY: _MessageIteratorInactivityMessage,
    native_bt.MESSAGE_TYPE_STREAM_BEGINNING: _StreamBeginningMessage,
    native_bt.MESSAGE_TYPE_STREAM_END: _StreamEndMessage,
    native_bt.MESSAGE_TYPE_PACKET_BEGINNING: _PacketBeginningMessage,
    native_bt.MESSAGE_TYPE_PACKET_END: _PacketEndMessage,
    native_bt.MESSAGE_TYPE_DISCARDED_EVENTS: _DiscardedEventsMessage,
    native_bt.MESSAGE_TYPE_DISCARDED_PACKETS: _DiscardedPacketsMessage,
}
