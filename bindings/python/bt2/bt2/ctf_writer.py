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

from bt2 import native_bt, object, stream, utils
import uuid as uuidp
import bt2.event
import bt2.field
import abc
import bt2


class CtfWriterClock(object._Object):
    def __init__(self, name, description=None, frequency=None, precision=None,
                 offset=None, is_absolute=None, uuid=None):
        utils._check_str(name)
        ptr = native_bt.ctf_clock_create(name)

        if ptr is None:
            raise bt2.CreationError('cannot create CTF writer clock object')

        super().__init__(ptr)

        if description is not None:
            self.description = description

        if frequency is not None:
            self.frequency = frequency

        if precision is not None:
            self.precision = precision

        if offset is not None:
            self.offset = offset

        if is_absolute is not None:
            self.is_absolute = is_absolute

        if uuid is not None:
            self.uuid = uuid

    def __eq__(self, other):
        if type(self) is not type(other):
            # not comparing apples to apples
            return False

        if self.addr == other.addr:
            return True

        self_props = (
            self.name,
            self.description,
            self.frequency,
            self.precision,
            self.offset,
            self.is_absolute,
            self.uuid
        )
        other_props = (
            other.name,
            other.description,
            other.frequency,
            other.precision,
            other.offset,
            other.is_absolute,
            other.uuid
        )
        return self_props == other_props

    def __copy__(self):
        return CtfWriterClock(name=self.name, description=self.description,
                              frequency=self.frequency,
                              precision=self.precision, offset=self.offset,
                              is_absolute=self.is_absolute, uuid=self.uuid)

    def __deepcopy__(self, memo):
        cpy = self.__copy__()
        memo[id(self)] = cpy
        return cpy

    @property
    def name(self):
        name = native_bt.ctf_clock_get_name(self._ptr)
        assert(name is not None)
        return name

    @property
    def description(self):
        description = native_bt.ctf_clock_get_description(self._ptr)
        return description

    @description.setter
    def description(self, description):
        utils._check_str(description)
        ret = native_bt.ctf_clock_set_description(self._ptr, description)
        utils._handle_ret(ret, "cannot set CTF writer clock object's description")

    @property
    def frequency(self):
        frequency = native_bt.ctf_clock_get_frequency(self._ptr)
        assert(frequency >= 1)
        return frequency

    @frequency.setter
    def frequency(self, frequency):
        utils._check_uint64(frequency)
        ret = native_bt.ctf_clock_set_frequency(self._ptr, frequency)
        utils._handle_ret(ret, "cannot set CTF writer clock object's frequency")

    @property
    def precision(self):
        precision = native_bt.ctf_clock_get_precision(self._ptr)
        assert(precision >= 0)
        return precision

    @precision.setter
    def precision(self, precision):
        utils._check_uint64(precision)
        ret = native_bt.ctf_clock_set_precision(self._ptr, precision)
        utils._handle_ret(ret, "cannot set CTF writer clock object's precision")

    @property
    def offset(self):
        ret, offset_s = native_bt.ctf_clock_get_offset_s(self._ptr)
        assert(ret == 0)
        ret, offset_cycles = native_bt.ctf_clock_get_offset(self._ptr)
        assert(ret == 0)
        return bt2.ClockClassOffset(offset_s, offset_cycles)

    @offset.setter
    def offset(self, offset):
        utils._check_type(offset, bt2.ClockClassOffset)
        ret = native_bt.ctf_clock_set_offset_s(self._ptr, offset.seconds)
        utils._handle_ret(ret, "cannot set CTF writer clock object's offset (seconds)")
        ret = native_bt.ctf_clock_set_offset(self._ptr, offset.cycles)
        utils._handle_ret(ret, "cannot set CTF writer clock object's offset (cycles)")

    @property
    def is_absolute(self):
        is_absolute = native_bt.ctf_clock_get_is_absolute(self._ptr)
        assert(is_absolute >= 0)
        return is_absolute > 0

    @is_absolute.setter
    def is_absolute(self, is_absolute):
        utils._check_bool(is_absolute)
        ret = native_bt.ctf_clock_set_is_absolute(self._ptr, int(is_absolute))
        utils._handle_ret(ret, "cannot set CTF writer clock object's absoluteness")

    @property
    def uuid(self):
        uuid_bytes = native_bt.ctf_clock_get_uuid(self._ptr)
        assert(uuid_bytes is not None)
        return uuidp.UUID(bytes=uuid_bytes)

    @uuid.setter
    def uuid(self, uuid):
        utils._check_type(uuid, uuidp.UUID)
        ret = native_bt.ctf_clock_set_uuid(self._ptr, uuid.bytes)
        utils._handle_ret(ret, "cannot set CTF writer clock object's UUID")

    def _time(self, time):
        utils._check_int64(time)
        ret = native_bt.ctf_clock_set_time(self._ptr, time)

    time = property(fset=_time)


class _CtfWriterStream(stream._StreamBase):
    @property
    def discarded_events_count(self):
        ret, count = native_bt.stream_get_discarded_events_count(self._ptr)
        utils._handle_ret(ret, "cannot get CTF writer stream object's discarded events count")
        return count

    def append_discarded_events(self, count):
        utils._check_uint64(count)
        native_bt.stream_append_discarded_events(self._ptr, count)

    def append_event(self, event):
        utils._check_type(event, bt2.event._Event)
        ret = native_bt.stream_append_event(self._ptr, event._ptr)
        utils._handle_ret(ret, 'cannot append event object to CTF writer stream object')

    def flush(self):
        ret = native_bt.stream_flush(self._ptr)
        utils._handle_ret(ret, 'cannot flush CTF writer stream object')

    @property
    def packet_header_field(self):
        field_ptr = native_bt.stream_get_packet_header(self._ptr)

        if field_ptr is None:
            return

        return bt2.field._create_from_ptr(field_ptr)

    @packet_header_field.setter
    def packet_header_field(self, packet_header_field):
        packet_header_field_ptr = None

        if packet_header_field is not None:
            utils._check_type(packet_header_field, bt2.field._Field)
            packet_header_field_ptr = packet_header_field._ptr

        ret = native_bt.stream_set_packet_header(self._ptr,
                                                 packet_header_field_ptr)
        utils._handle_ret(ret, "cannot set CTF writer stream object's packet header field")

    @property
    def packet_context_field(self):
        field_ptr = native_bt.stream_get_packet_context(self._ptr)

        if field_ptr is None:
            return

        return bt2.field._create_from_ptr(field_ptr)

    @packet_context_field.setter
    def packet_context_field(self, packet_context_field):
        packet_context_field_ptr = None

        if packet_context_field is not None:
            utils._check_type(packet_context_field, bt2.field._Field)
            packet_context_field_ptr = packet_context_field._ptr

        ret = native_bt.stream_set_packet_context(self._ptr,
                                                  packet_context_field_ptr)
        utils._handle_ret(ret, "cannot set CTF writer stream object's packet context field")

    def __eq__(self, other):
        if type(other) is not type(self):
            return False

        if self.addr == other.addr:
            return True

        if not _StreamBase.__eq__(self, other):
            return False

        self_props = (
            self.discarded_events_count,
            self.packet_header_field,
            self.packet_context_field,
        )
        other_props = (
            other.discarded_events_count,
            other.packet_header_field,
            other.packet_context_field,
        )
        return self_props == other_props

    def _copy(self, copy_func):
        cpy = self.stream_class(self.name)
        cpy.append_discarded_events(self.discarded_events_count)
        cpy.packet_header_field = copy_func(self.packet_header_field)
        cpy.packet_context_field = copy_func(self.packet_context_field)
        return cpy

    def __copy__(self):
        return self._copy(copy.copy)

    def __deepcopy__(self, memo):
        cpy = self._copy(copy.deepcopy)
        memo[id(self)] = cpy
        return cpy


class CtfWriter(object._Object):
    def __init__(self, path):
        utils._check_str(path)
        ptr = native_bt.ctf_writer_create(path)

        if ptr is None:
            raise bt2.CreationError('cannot create CTF writer object')

        super().__init__(ptr)

    @property
    def trace(self):
        trace_ptr = native_bt.ctf_writer_get_trace(self._ptr)
        assert(trace_ptr)
        return bt2.Trace._create_from_ptr(trace_ptr)

    @property
    def metadata_string(self):
        metadata_string = native_bt.ctf_writer_get_metadata_string(self._ptr)
        assert(metadata_string is not None)
        return metadata_string

    def flush_metadata(self):
        native_bt.ctf_writer_flush_metadata(self._ptr)

    def add_clock(self, clock):
        utils._check_type(clock, CtfWriterClock)
        ret = native_bt.ctf_writer_add_clock(self._ptr, clock._ptr)
        utils._handle_ret(ret, 'cannot add CTF writer clock object to CTF writer object')
