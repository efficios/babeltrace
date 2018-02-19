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
import bt2.clock_class
import bt2.packet
import bt2.stream
import bt2.fields
import bt2.clock_value
import collections
import numbers
import copy
import abc
import bt2


def _create_from_ptr(ptr):
    # recreate the event class wrapper of this event's class (the
    # identity could be different, but the underlying address should be
    # the same)
    event_class_ptr = native_bt.event_get_class(ptr)
    utils._handle_ptr(event_class_ptr, "cannot get event object's class")
    event_class = bt2.EventClass._create_from_ptr(event_class_ptr)
    event = _Event._create_from_ptr(ptr)
    event._event_class = event_class
    return event


class _EventClockValuesIterator(collections.abc.Iterator):
    def __init__(self, event_clock_values):
        self._event_clock_values = event_clock_values
        self._clock_classes = event_clock_values._event._clock_classes
        self._at = 0

    def __next__(self):
        if self._at == len(self._clock_classes):
            raise StopIteration

        self._at += 1
        return self._clock_classes[at]


class _EventClockValues(collections.abc.Mapping):
    def __init__(self, event):
        self._event = event

    def __getitem__(self, clock_class):
        utils._check_type(clock_class, bt2.ClockClass)
        clock_value_ptr = native_bt.event_get_clock_value(self._event._ptr,
                                                          clock_class._ptr)

        if clock_value_ptr is None:
            return

        clock_value = bt2.clock_value._create_clock_value_from_ptr(clock_value_ptr)
        return clock_value

    def add(self, clock_value):
        utils._check_type(clock_value, bt2.clock_value._ClockValue)
        ret = native_bt.event_set_clock_value(self._ptr,
                                              clock_value._ptr)
        utils._handle_ret(ret, "cannot set event object's clock value")

    def __len__(self):
        count = len(self._event._clock_classes)
        assert(count >= 0)
        return count

    def __iter__(self):
        return _EventClockValuesIterator(self)


class _Event(object._Object):
    @property
    def event_class(self):
        return self._event_class

    @property
    def name(self):
        return self._event_class.name

    @property
    def id(self):
        return self._event_class.id

    @property
    def packet(self):
        packet_ptr = native_bt.event_get_packet(self._ptr)

        if packet_ptr is None:
            return packet_ptr

        return bt2.packet._Packet._create_from_ptr(packet_ptr)

    @packet.setter
    def packet(self, packet):
        utils._check_type(packet, bt2.packet._Packet)
        ret = native_bt.event_set_packet(self._ptr, packet._ptr)
        utils._handle_ret(ret, "cannot set event object's packet object")

    @property
    def stream(self):
        stream_ptr = native_bt.event_get_stream(self._ptr)

        if stream_ptr is None:
            return stream_ptr

        return bt2.stream._Stream._create_from_ptr(stream_ptr)

    @property
    def header_field(self):
        field_ptr = native_bt.event_get_header(self._ptr)

        if field_ptr is None:
            return

        return bt2.fields._create_from_ptr(field_ptr)

    @header_field.setter
    def header_field(self, header_field):
        header_field_ptr = None

        if header_field is not None:
            utils._check_type(header_field, bt2.fields._Field)
            header_field_ptr = header_field._ptr

        ret = native_bt.event_set_header(self._ptr, header_field_ptr)
        utils._handle_ret(ret, "cannot set event object's header field")

    @property
    def stream_event_context_field(self):
        field_ptr = native_bt.event_get_stream_event_context(self._ptr)

        if field_ptr is None:
            return

        return bt2.fields._create_from_ptr(field_ptr)

    @stream_event_context_field.setter
    def stream_event_context_field(self, stream_event_context):
        stream_event_context_ptr = None

        if stream_event_context is not None:
            utils._check_type(stream_event_context, bt2.fields._Field)
            stream_event_context_ptr = stream_event_context._ptr

        ret = native_bt.event_set_stream_event_context(self._ptr,
                                                       stream_event_context_ptr)
        utils._handle_ret(ret, "cannot set event object's stream event context field")

    @property
    def context_field(self):
        field_ptr = native_bt.event_get_event_context(self._ptr)

        if field_ptr is None:
            return

        return bt2.fields._create_from_ptr(field_ptr)

    @context_field.setter
    def context_field(self, context):
        context_ptr = None

        if context is not None:
            utils._check_type(context, bt2.fields._Field)
            context_ptr = context._ptr

        ret = native_bt.event_set_event_context(self._ptr, context_ptr)
        utils._handle_ret(ret, "cannot set event object's context field")

    @property
    def payload_field(self):
        field_ptr = native_bt.event_get_event_payload(self._ptr)

        if field_ptr is None:
            return

        return bt2.fields._create_from_ptr(field_ptr)

    @payload_field.setter
    def payload_field(self, payload):
        payload_ptr = None

        if payload is not None:
            utils._check_type(payload, bt2.fields._Field)
            payload_ptr = payload._ptr

        ret = native_bt.event_set_event_payload(self._ptr, payload_ptr)
        utils._handle_ret(ret, "cannot set event object's payload field")

    def _get_clock_value_cycles(self, clock_class_ptr):
        clock_value_ptr = native_bt.event_get_clock_value(self._ptr,
                                                          clock_class_ptr)

        if clock_value_ptr is None:
            return

        ret, cycles = native_bt.clock_value_get_value(clock_value_ptr)
        native_bt.put(clock_value_ptr)
        utils._handle_ret(ret, "cannot get clock value object's cycles")
        return cycles

    @property
    def clock_values(self):
        return _EventClockValues(self)

    def __getitem__(self, key):
        utils._check_str(key)
        payload_field = self.payload_field

        if payload_field is not None and key in payload_field:
            return payload_field[key]

        context_field = self.context_field

        if context_field is not None and key in context_field:
            return context_field[key]

        sec_field = self.stream_event_context_field

        if sec_field is not None and key in sec_field:
            return sec_field[key]

        header_field = self.header_field

        if header_field is not None and key in header_field:
            return header_field[key]

        packet = self.packet

        if packet is None:
            raise KeyError(key)

        pkt_context_field = packet.context_field

        if pkt_context_field is not None and key in pkt_context_field:
            return pkt_context_field[key]

        pkt_header_field = packet.header_field

        if pkt_header_field is not None and key in pkt_header_field:
            return pkt_header_field[key]

        raise KeyError(key)

    @property
    def _clock_classes(self):
        stream_class = self.event_class.stream_class

        if stream_class is None:
            return []

        trace = stream_class.trace

        if trace is None:
            return []

        clock_classes = []

        for clock_class in trace.clock_classes.values():
            clock_classes.append(clock_class)

        return clock_classes

    @property
    def _clock_class_ptrs(self):
        return [cc._ptr for cc in self._clock_classes]

    def __eq__(self, other):
        if type(other) is not type(self):
            return False

        if self.addr == other.addr:
            return True

        self_clock_values = {}
        other_clock_values = {}

        for clock_class_ptr in self._clock_class_ptrs:
            self_clock_values[int(clock_class_ptr)] = self._get_clock_value_cycles(clock_class_ptr)

        for clock_class_ptr in other._clock_class_ptrs:
            other_clock_values[int(clock_class_ptr)] = self._get_clock_value_cycles(clock_class_ptr)

        self_props = (
            self.header_field,
            self.stream_event_context_field,
            self.context_field,
            self.payload_field,
            self_clock_values,
        )
        other_props = (
            other.header_field,
            other.stream_event_context_field,
            other.context_field,
            other.payload_field,
            other_clock_values,
        )
        return self_props == other_props

    def _copy(self, copy_func):
        cpy = self.event_class()

        # copy fields
        cpy.header_field = copy_func(self.header_field)
        cpy.stream_event_context_field = copy_func(self.stream_event_context_field)
        cpy.context_field = copy_func(self.context_field)
        cpy.payload_field = copy_func(self.payload_field)

        # Copy known clock value references. It's not necessary to copy
        # clock class or clock value objects because once a clock value
        # is created from a clock class, the clock class is frozen.
        # Thus even if we copy the clock class, the user cannot modify
        # it, therefore it's useless to copy it.
        for clock_class in self._clock_classes:
            clock_value = self.clock_values[clock_class]

            if clock_value is not None:
                cpy.clock_values.add(clock_value)

        return cpy

    def __copy__(self):
        return self._copy(copy.copy)

    def __deepcopy__(self, memo):
        cpy = self._copy(copy.deepcopy)
        memo[id(self)] = cpy
        return cpy
