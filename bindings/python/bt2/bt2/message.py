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


class _MessageWithDefaultClockSnapshot:
    def _get_default_clock_snapshot(self, borrow_clock_snapshot_ptr):
        if not self._has_default_clock_class:
            raise bt2.NoDefaultClockClass('cannot get default clock snapshot, stream class has no default clock class')

        snapshot_ptr = borrow_clock_snapshot_ptr(self._ptr)

        return bt2.clock_snapshot._ClockSnapshot._create_from_ptr_and_get_ref(
            snapshot_ptr, self._ptr, self._get_ref, self._put_ref)


class _EventMessage(_Message, _MessageWithDefaultClockSnapshot):
    _borrow_default_clock_snapshot_ptr = staticmethod(native_bt.message_event_borrow_default_clock_snapshot_const)

    @property
    def _has_default_clock_class(self):
        return self.event.packet.stream.stream_class.default_clock_class is not None

    @property
    def default_clock_snapshot(self):
        return self._get_default_clock_snapshot(self._borrow_default_clock_snapshot_ptr)

    @property
    def event(self):
        event_ptr = native_bt.message_event_borrow_event(self._ptr)
        assert event_ptr is not None
        return bt2.event._Event._create_from_ptr_and_get_ref(
            event_ptr, self._ptr, self._get_ref, self._put_ref)


class _PacketMessage(_Message, _MessageWithDefaultClockSnapshot):
    @property
    def _has_default_clock_class(self):
        return self.packet.stream.stream_class.default_clock_class is not None

    @property
    def default_clock_snapshot(self):
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


class _StreamMessage(_Message):
    @property
    def stream(self):
        stream_ptr = self._borrow_stream_ptr(self._ptr)
        assert stream_ptr
        return bt2.stream._Stream._create_from_ptr_and_get_ref(stream_ptr)


class _StreamBeginningMessage(_StreamMessage):
    _borrow_stream_ptr = staticmethod(native_bt.message_stream_beginning_borrow_stream)


class _StreamEndMessage(_StreamMessage):
    _borrow_stream_ptr = staticmethod(native_bt.message_stream_end_borrow_stream)


class _StreamActivityMessage(_Message):
    @property
    def default_clock_snapshot(self):
        if self.stream.stream_class.default_clock_class is None:
            raise bt2.NoDefaultClockClass('cannot get default clock snapshot, stream class has no default clock class')

        status, snapshot_ptr = self._borrow_default_clock_snapshot_ptr(self._ptr)

        if status == native_bt.MESSAGE_STREAM_ACTIVITY_CLOCK_SNAPSHOT_STATE_KNOWN:
            snapshot_type = bt2.clock_snapshot._ClockSnapshot
        elif status == native_bt.MESSAGE_STREAM_ACTIVITY_CLOCK_SNAPSHOT_STATE_UNKNOWN:
            snapshot_type = bt2.clock_snapshot._UnknownClockSnapshot
        elif status == native_bt.MESSAGE_STREAM_ACTIVITY_CLOCK_SNAPSHOT_STATE_INFINITE:
            snapshot_type = bt2.clock_snapshot._InfiniteClockSnapshot
        else:
            raise bt2.Error('cannot borrow default clock snapshot from message')

        assert snapshot_ptr is not None

        return snapshot_type._create_from_ptr_and_get_ref(
            snapshot_ptr, self._ptr, self._get_ref, self._put_ref)

    def _default_clock_snapshot(self, value):
        self._set_default_clock_snapshot_ptr(self._ptr, value)

    _default_clock_snapshot = property(fset=_default_clock_snapshot)

    @property
    def stream(self):
        stream_ptr = self._borrow_stream_ptr(self._ptr)
        assert stream_ptr
        return bt2.stream._Stream._create_from_ptr_and_get_ref(stream_ptr)


class _StreamActivityBeginningMessage(_StreamActivityMessage):
    _borrow_default_clock_snapshot_ptr = staticmethod(native_bt.message_stream_activity_beginning_borrow_default_clock_snapshot_const)
    _set_default_clock_snapshot_ptr = staticmethod(native_bt.message_stream_activity_beginning_set_default_clock_snapshot)
    _borrow_stream_ptr = staticmethod(native_bt.message_stream_activity_beginning_borrow_stream)


class _StreamActivityEndMessage(_StreamActivityMessage):
    _borrow_default_clock_snapshot_ptr = staticmethod(native_bt.message_stream_activity_end_borrow_default_clock_snapshot_const)
    _set_default_clock_snapshot_ptr = staticmethod(native_bt.message_stream_activity_end_set_default_clock_snapshot)
    _borrow_stream_ptr = staticmethod(native_bt.message_stream_activity_end_borrow_stream)


class _MessageIteratorInactivityMessage(_Message, _MessageWithDefaultClockSnapshot):
    # This kind of message always has a default clock class.
    _has_default_clock_class = True
    _borrow_default_clock_snapshot_ptr = staticmethod(native_bt.message_message_iterator_inactivity_borrow_default_clock_snapshot_const)

    @property
    def default_clock_snapshot(self):
        return self._get_default_clock_snapshot(self._borrow_default_clock_snapshot_ptr)


class _DiscardedMessage(_Message, _MessageWithDefaultClockSnapshot):
    @property
    def stream(self):
        stream_ptr = self._borrow_stream_ptr(self._ptr)
        assert stream_ptr
        return bt2.stream._Stream._create_from_ptr_and_get_ref(stream_ptr)

    @property
    def _has_default_clock_class(self):
        return self.default_clock_class is not None

    @property
    def default_clock_class(self):
        cc_ptr = self._borrow_clock_class_ptr(self._ptr)
        if cc_ptr is not None:
            return bt2.clock_class._ClockClass._create_from_ptr_and_get_ref(cc_ptr)

    @property
    def count(self):
        avail, count = self._get_count(self._ptr)
        if avail is native_bt.PROPERTY_AVAILABILITY_AVAILABLE:
            return count

    def _set_count(self, count):
        utils._check_uint64(count)
        self._set_count(self._ptr, count)

    _count = property(fset=_set_count)

    @property
    def beginning_default_clock_snapshot(self):
        return self._get_default_clock_snapshot(self._borrow_beginning_clock_snapshot_ptr)

    @property
    def end_default_clock_snapshot(self):
        return self._get_default_clock_snapshot(self._borrow_end_clock_snapshot_ptr)


class _DiscardedEventsMessage(_DiscardedMessage):
    _borrow_stream_ptr = staticmethod(native_bt.message_discarded_events_borrow_stream_const)
    _get_count = staticmethod(native_bt.message_discarded_events_get_count)
    _set_count = staticmethod(native_bt.message_discarded_events_set_count)
    _borrow_clock_class_ptr = staticmethod(native_bt.message_discarded_events_borrow_stream_class_default_clock_class_const)
    _borrow_beginning_clock_snapshot_ptr = staticmethod(native_bt.message_discarded_events_borrow_default_beginning_clock_snapshot_const)
    _borrow_end_clock_snapshot_ptr = staticmethod(native_bt.message_discarded_events_borrow_default_end_clock_snapshot_const)


class _DiscardedPacketsMessage(_DiscardedMessage):
    _borrow_stream_ptr = staticmethod(native_bt.message_discarded_packets_borrow_stream_const)
    _get_count = staticmethod(native_bt.message_discarded_packets_get_count)
    _set_count = staticmethod(native_bt.message_discarded_packets_set_count)
    _borrow_clock_class_ptr = staticmethod(native_bt.message_discarded_packets_borrow_stream_class_default_clock_class_const)
    _borrow_beginning_clock_snapshot_ptr = staticmethod(native_bt.message_discarded_packets_borrow_default_beginning_clock_snapshot_const)
    _borrow_end_clock_snapshot_ptr = staticmethod(native_bt.message_discarded_packets_borrow_default_end_clock_snapshot_const)


_MESSAGE_TYPE_TO_CLS = {
    native_bt.MESSAGE_TYPE_EVENT: _EventMessage,
    native_bt.MESSAGE_TYPE_MESSAGE_ITERATOR_INACTIVITY: _MessageIteratorInactivityMessage,
    native_bt.MESSAGE_TYPE_STREAM_BEGINNING: _StreamBeginningMessage,
    native_bt.MESSAGE_TYPE_STREAM_END: _StreamEndMessage,
    native_bt.MESSAGE_TYPE_PACKET_BEGINNING: _PacketBeginningMessage,
    native_bt.MESSAGE_TYPE_PACKET_END: _PacketEndMessage,
    native_bt.MESSAGE_TYPE_STREAM_ACTIVITY_BEGINNING: _StreamActivityBeginningMessage,
    native_bt.MESSAGE_TYPE_STREAM_ACTIVITY_END: _StreamActivityEndMessage,
    native_bt.MESSAGE_TYPE_DISCARDED_EVENTS: _DiscardedEventsMessage,
    native_bt.MESSAGE_TYPE_DISCARDED_PACKETS: _DiscardedPacketsMessage,
}
