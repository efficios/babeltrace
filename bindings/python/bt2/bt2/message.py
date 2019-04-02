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
import bt2.clock_class_priority_map
import bt2.clock_value
import collections
import bt2.packet
import bt2.stream
import bt2.event
import copy
import bt2


def _create_from_ptr(ptr):
    msg_type = native_bt.message_get_type(ptr)
    cls = None

    if msg_type not in _MESSAGE_TYPE_TO_CLS:
        raise bt2.Error('unknown message type: {}'.format(msg_type))

    return _MESSAGE_TYPE_TO_CLS[msg_type]._create_from_ptr(ptr)


def _msg_types_from_msg_classes(message_types):
    if message_types is None:
        msg_types = None
    else:
        for msg_cls in message_types:
            if msg_cls not in _MESSAGE_TYPE_TO_CLS.values():
                raise ValueError("'{}' is not a message class".format(msg_cls))

        msg_types = [msg_cls._TYPE for msg_cls in message_types]

    return msg_types


class _Message(object._Object):
    pass


class _CopyableMessage(_Message):
    def __copy__(self):
        return self._copy(lambda obj: obj)

    def __deepcopy__(self, memo):
        cpy = self._copy(copy.deepcopy)
        memo[id(self)] = cpy
        return cpy


class EventMessage(_CopyableMessage):
    _TYPE = native_bt.MESSAGE_TYPE_EVENT

    def __init__(self, event, cc_prio_map=None):
        utils._check_type(event, bt2.event._Event)

        if cc_prio_map is not None:
            utils._check_type(cc_prio_map, bt2.clock_class_priority_map.ClockClassPriorityMap)
            cc_prio_map_ptr = cc_prio_map._ptr
        else:
            cc_prio_map_ptr = None

        ptr = native_bt.message_event_create(event._ptr, cc_prio_map_ptr)

        if ptr is None:
            raise bt2.CreationError('cannot create event message object')

        super().__init__(ptr)

    @property
    def event(self):
        event_ptr = native_bt.message_event_get_event(self._ptr)
        assert(event_ptr)
        return bt2.event._create_from_ptr(event_ptr)

    @property
    def clock_class_priority_map(self):
        cc_prio_map_ptr = native_bt.message_event_get_clock_class_priority_map(self._ptr)
        assert(cc_prio_map_ptr)
        return bt2.clock_class_priority_map.ClockClassPriorityMap._create_from_ptr(cc_prio_map_ptr)

    def __eq__(self, other):
        if type(other) is not type(self):
            return False

        if self.addr == other.addr:
            return True

        self_props = (
            self.event,
            self.clock_class_priority_map,
        )
        other_props = (
            other.event,
            other.clock_class_priority_map,
        )
        return self_props == other_props

    def _copy(self, copy_func):
        # We can always use references here because those properties are
        # frozen anyway if they are part of a message. Since the
        # user cannot modify them after copying the message, it's
        # useless to copy/deep-copy them.
        return EventMessage(self.event, self.clock_class_priority_map)


class PacketBeginningMessage(_CopyableMessage):
    _TYPE = native_bt.MESSAGE_TYPE_PACKET_BEGINNING

    def __init__(self, packet):
        utils._check_type(packet, bt2.packet._Packet)
        ptr = native_bt.message_packet_begin_create(packet._ptr)

        if ptr is None:
            raise bt2.CreationError('cannot create packet beginning message object')

        super().__init__(ptr)

    @property
    def packet(self):
        packet_ptr = native_bt.message_packet_begin_get_packet(self._ptr)
        assert(packet_ptr)
        return bt2.packet._Packet._create_from_ptr(packet_ptr)

    def __eq__(self, other):
        if type(other) is not type(self):
            return False

        if self.addr == other.addr:
            return True

        return self.packet == other.packet

    def _copy(self, copy_func):
        # We can always use references here because those properties are
        # frozen anyway if they are part of a message. Since the
        # user cannot modify them after copying the message, it's
        # useless to copy/deep-copy them.
        return PacketBeginningMessage(self.packet)


class PacketEndMessage(_CopyableMessage):
    _TYPE = native_bt.MESSAGE_TYPE_PACKET_END

    def __init__(self, packet):
        utils._check_type(packet, bt2.packet._Packet)
        ptr = native_bt.message_packet_end_create(packet._ptr)

        if ptr is None:
            raise bt2.CreationError('cannot create packet end message object')

        super().__init__(ptr)

    @property
    def packet(self):
        packet_ptr = native_bt.message_packet_end_get_packet(self._ptr)
        assert(packet_ptr)
        return bt2.packet._Packet._create_from_ptr(packet_ptr)

    def __eq__(self, other):
        if type(other) is not type(self):
            return False

        if self.addr == other.addr:
            return True

        return self.packet == other.packet

    def _copy(self, copy_func):
        # We can always use references here because those properties are
        # frozen anyway if they are part of a message. Since the
        # user cannot modify them after copying the message, it's
        # useless to copy/deep-copy them.
        return PacketEndMessage(self.packet)


class StreamBeginningMessage(_CopyableMessage):
    _TYPE = native_bt.MESSAGE_TYPE_STREAM_BEGINNING

    def __init__(self, stream):
        utils._check_type(stream, bt2.stream._Stream)
        ptr = native_bt.message_stream_begin_create(stream._ptr)

        if ptr is None:
            raise bt2.CreationError('cannot create stream beginning message object')

        super().__init__(ptr)

    @property
    def stream(self):
        stream_ptr = native_bt.message_stream_begin_get_stream(self._ptr)
        assert(stream_ptr)
        return bt2.stream._create_from_ptr(stream_ptr)

    def __eq__(self, other):
        if type(other) is not type(self):
            return False

        if self.addr == other.addr:
            return True

        return self.stream == other.stream

    def _copy(self, copy_func):
        # We can always use references here because those properties are
        # frozen anyway if they are part of a message. Since the
        # user cannot modify them after copying the message, it's
        # useless to copy/deep-copy them.
        return StreamBeginningMessage(self.stream)


class StreamEndMessage(_CopyableMessage):
    _TYPE = native_bt.MESSAGE_TYPE_STREAM_END

    def __init__(self, stream):
        utils._check_type(stream, bt2.stream._Stream)
        ptr = native_bt.message_stream_end_create(stream._ptr)

        if ptr is None:
            raise bt2.CreationError('cannot create stream end message object')

        super().__init__(ptr)

    @property
    def stream(self):
        stream_ptr = native_bt.message_stream_end_get_stream(self._ptr)
        assert(stream_ptr)
        return bt2.stream._create_from_ptr(stream_ptr)

    def __eq__(self, other):
        if type(other) is not type(self):
            return False

        if self.addr == other.addr:
            return True

        return self.stream == other.stream

    def _copy(self, copy_func):
        # We can always use references here because those properties are
        # frozen anyway if they are part of a message. Since the
        # user cannot modify them after copying the message, it's
        # useless to copy/deep-copy them.
        return StreamEndMessage(self.stream)


class _InactivityMessageClockValuesIterator(collections.abc.Iterator):
    def __init__(self, msg_clock_values):
        self._msg_clock_values = msg_clock_values
        self._clock_classes = list(msg_clock_values._msg.clock_class_priority_map)
        self._at = 0

    def __next__(self):
        if self._at == len(self._clock_classes):
            raise StopIteration

        self._at += 1
        return self._clock_classes[at]


class _InactivityMessageClockValues(collections.abc.Mapping):
    def __init__(self, msg):
        self._msg = msg

    def __getitem__(self, clock_class):
        utils._check_type(clock_class, bt2.ClockClass)
        clock_value_ptr = native_bt.message_inactivity_get_clock_value(self._msg._ptr,
                                                                            clock_class._ptr)

        if clock_value_ptr is None:
            return

        clock_value = bt2.clock_value._create_clock_value_from_ptr(clock_value_ptr)
        return clock_value

    def add(self, clock_value):
        utils._check_type(clock_value, bt2.clock_value._ClockValue)
        ret = native_bt.message_inactivity_set_clock_value(self._msg._ptr,
                                                                clock_value._ptr)
        utils._handle_ret(ret, "cannot set inactivity message object's clock value")

    def __len__(self):
        return len(self._msg.clock_class_priority_map)

    def __iter__(self):
        return _InactivityMessageClockValuesIterator(self)


class InactivityMessage(_CopyableMessage):
    _TYPE = native_bt.MESSAGE_TYPE_MESSAGE_ITERATOR_INACTIVITY

    def __init__(self, cc_prio_map=None):
        if cc_prio_map is not None:
            utils._check_type(cc_prio_map, bt2.clock_class_priority_map.ClockClassPriorityMap)
            cc_prio_map_ptr = cc_prio_map._ptr
        else:
            cc_prio_map_ptr = None

        ptr = native_bt.message_inactivity_create(cc_prio_map_ptr)

        if ptr is None:
            raise bt2.CreationError('cannot create inactivity message object')

        super().__init__(ptr)

    @property
    def clock_class_priority_map(self):
        cc_prio_map_ptr = native_bt.message_inactivity_get_clock_class_priority_map(self._ptr)
        assert(cc_prio_map_ptr)
        return bt2.clock_class_priority_map.ClockClassPriorityMap._create_from_ptr(cc_prio_map_ptr)

    @property
    def clock_values(self):
        return _InactivityMessageClockValues(self)

    def _get_clock_values(self):
        clock_values = {}

        for clock_class, clock_value in self.clock_values.items():
            if clock_value is None:
                continue

            clock_values[clock_class] = clock_value

        return clock_values

    def __eq__(self, other):
        if type(other) is not type(self):
            return False

        if self.addr == other.addr:
            return True

        self_props = (
            self.clock_class_priority_map,
            self._get_clock_values(),
        )
        other_props = (
            other.clock_class_priority_map,
            other._get_clock_values(),
        )
        return self_props == other_props

    def __copy__(self):
        cpy = InactivityMessage(self.clock_class_priority_map)

        for clock_class, clock_value in self.clock_values.items():
            if clock_value is None:
                continue

            cpy.clock_values.add(clock_value)

        return cpy

    def __deepcopy__(self, memo):
        cc_prio_map_cpy = copy.deepcopy(self.clock_class_priority_map)
        cpy = InactivityMessage(cc_prio_map_cpy)

        # copy clock values
        for orig_clock_class in self.clock_class_priority_map:
            orig_clock_value = self.clock_value(orig_clock_class)

            if orig_clock_value is None:
                continue

            # find equivalent, copied clock class in CC priority map copy
            for cpy_clock_class in cc_prio_map_cpy:
                if cpy_clock_class == orig_clock_class:
                    break

            # create copy of clock value from copied clock class
            clock_value_cpy = cpy_clock_class(orig_clock_value.cycles)

            # set copied clock value in message copy
            cpy.clock_values.add(clock_value_cpy)

        memo[id(self)] = cpy
        return cpy


class _DiscardedElementsMessage(_Message):
    def __eq__(self, other):
        if type(other) is not type(self):
            return False

        if self.addr == other.addr:
            return True

        self_props = (
            self.count,
            self.stream,
            self.beginning_clock_value,
            self.end_clock_value,
        )
        other_props = (
            other.count,
            other.stream,
            other.beginning_clock_value,
            other.end_clock_value,
        )
        return self_props == other_props


class _DiscardedPacketsMessage(_DiscardedElementsMessage):
    _TYPE = native_bt.MESSAGE_TYPE_DISCARDED_PACKETS

    @property
    def count(self):
        count = native_bt.message_discarded_packets_get_count(self._ptr)
        assert(count >= 0)
        return count

    @property
    def stream(self):
        stream_ptr = native_bt.message_discarded_packets_get_stream(self._ptr)
        assert(stream_ptr)
        return bt2.stream._create_from_ptr(stream_ptr)

    @property
    def beginning_clock_value(self):
        clock_value_ptr = native_bt.message_discarded_packets_get_begin_clock_value(self._ptr)

        if clock_value_ptr is None:
            return

        clock_value = bt2.clock_value._create_clock_value_from_ptr(clock_value_ptr)
        return clock_value

    @property
    def end_clock_value(self):
        clock_value_ptr = native_bt.message_discarded_packets_get_end_clock_value(self._ptr)

        if clock_value_ptr is None:
            return

        clock_value = bt2.clock_value._create_clock_value_from_ptr(clock_value_ptr)
        return clock_value


class _DiscardedEventsMessage(_DiscardedElementsMessage):
    _TYPE = native_bt.MESSAGE_TYPE_DISCARDED_EVENTS

    @property
    def count(self):
        count = native_bt.message_discarded_events_get_count(self._ptr)
        assert(count >= 0)
        return count

    @property
    def stream(self):
        stream_ptr = native_bt.message_discarded_events_get_stream(self._ptr)
        assert(stream_ptr)
        return bt2.stream._create_from_ptr(stream_ptr)

    @property
    def beginning_clock_value(self):
        clock_value_ptr = native_bt.message_discarded_events_get_begin_clock_value(self._ptr)

        if clock_value_ptr is None:
            return

        clock_value = bt2.clock_value._create_clock_value_from_ptr(clock_value_ptr)
        return clock_value

    @property
    def end_clock_value(self):
        clock_value_ptr = native_bt.message_discarded_events_get_end_clock_value(self._ptr)

        if clock_value_ptr is None:
            return

        clock_value = bt2.clock_value._create_clock_value_from_ptr(clock_value_ptr)
        return clock_value


_MESSAGE_TYPE_TO_CLS = {
    native_bt.MESSAGE_TYPE_EVENT: EventMessage,
    native_bt.MESSAGE_TYPE_PACKET_BEGINNING: PacketBeginningMessage,
    native_bt.MESSAGE_TYPE_PACKET_END: PacketEndMessage,
    native_bt.MESSAGE_TYPE_STREAM_BEGINNING: StreamBeginningMessage,
    native_bt.MESSAGE_TYPE_STREAM_END: StreamEndMessage,
    native_bt.MESSAGE_TYPE_MESSAGE_ITERATOR_INACTIVITY: InactivityMessage,
    native_bt.MESSAGE_TYPE_DISCARDED_PACKETS: _DiscardedPacketsMessage,
    native_bt.MESSAGE_TYPE_DISCARDED_EVENTS: _DiscardedEventsMessage,
}
