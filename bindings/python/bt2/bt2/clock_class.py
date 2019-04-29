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
import uuid as uuidp
import numbers
import bt2
import bt2.clock_snapshot as clock_snapshot


class ClockClassOffset:
    def __init__(self, seconds=0, cycles=0):
        utils._check_int64(seconds)
        utils._check_int64(cycles)
        self._seconds = seconds
        self._cycles = cycles

    @property
    def seconds(self):
        return self._seconds

    @property
    def cycles(self):
        return self._cycles

    def __hash__(self):
        return hash((self.seconds, self.cycles))

    def __eq__(self, other):
        if not isinstance(other, self.__class__):
            # not comparing apples to apples
            return False

        return (self.seconds, self.cycles) == (other.seconds, other.cycles)


class ClockClass(object._SharedObject):
    def __init__(self, name, frequency, description=None, precision=None,
                 offset=None, is_absolute=None, uuid=None):
        utils._check_str(name)
        utils._check_uint64(frequency)
        ptr = native_bt.clock_class_create(name, frequency)

        if ptr is None:
            raise bt2.CreationError('cannot create clock class object')

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
        return ClockClass(name=self.name, description=self.description,
                          frequency=self.frequency, precision=self.precision,
                          offset=self.offset, is_absolute=self.is_absolute,
                          uuid=self.uuid)

    def __deepcopy__(self, memo):
        cpy = self.__copy__()
        memo[id(self)] = cpy
        return cpy

    def __hash__(self):
        return hash((
            self.name,
            self.description,
            self.frequency,
            self.precision,
            self.offset.seconds,
            self.offset.cycles,
            self.is_absolute,
            self.uuid))

    @property
    def name(self):
        name = native_bt.clock_class_get_name(self._ptr)
        assert(name is not None)
        return name

    @name.setter
    def name(self, name):
        utils._check_str(name)
        ret = native_bt.clock_class_set_name(self._ptr, name)
        utils._handle_ret(ret, "cannot set clock class object's name")

    @property
    def description(self):
        return native_bt.clock_class_get_description(self._ptr)

    @description.setter
    def description(self, description):
        utils._check_str(description)
        ret = native_bt.clock_class_set_description(self._ptr, description)
        utils._handle_ret(ret, "cannot set clock class object's description")

    @property
    def frequency(self):
        frequency = native_bt.clock_class_get_frequency(self._ptr)
        assert(frequency >= 1)
        return frequency

    @frequency.setter
    def frequency(self, frequency):
        utils._check_uint64(frequency)
        ret = native_bt.clock_class_set_frequency(self._ptr, frequency)
        utils._handle_ret(ret, "cannot set clock class object's frequency")

    @property
    def precision(self):
        precision = native_bt.clock_class_get_precision(self._ptr)
        assert(precision >= 0)
        return precision

    @precision.setter
    def precision(self, precision):
        utils._check_uint64(precision)
        ret = native_bt.clock_class_set_precision(self._ptr, precision)
        utils._handle_ret(ret, "cannot set clock class object's precision")

    @property
    def offset(self):
        ret, offset_s = native_bt.clock_class_get_offset_s(self._ptr)
        assert(ret == 0)
        ret, offset_cycles = native_bt.clock_class_get_offset_cycles(self._ptr)
        assert(ret == 0)
        return ClockClassOffset(offset_s, offset_cycles)

    @offset.setter
    def offset(self, offset):
        utils._check_type(offset, ClockClassOffset)
        ret = native_bt.clock_class_set_offset_s(self._ptr, offset.seconds)
        utils._handle_ret(ret, "cannot set clock class object's offset (seconds)")
        ret = native_bt.clock_class_set_offset_cycles(self._ptr, offset.cycles)
        utils._handle_ret(ret, "cannot set clock class object's offset (cycles)")

    @property
    def is_absolute(self):
        is_absolute = native_bt.clock_class_is_absolute(self._ptr)
        assert(is_absolute >= 0)
        return is_absolute > 0

    @is_absolute.setter
    def is_absolute(self, is_absolute):
        utils._check_bool(is_absolute)
        ret = native_bt.clock_class_set_is_absolute(self._ptr, int(is_absolute))
        utils._handle_ret(ret, "cannot set clock class object's absoluteness")

    @property
    def uuid(self):
        uuid_bytes = native_bt.clock_class_get_uuid(self._ptr)

        if uuid_bytes is None:
            return

        return uuidp.UUID(bytes=uuid_bytes)

    @uuid.setter
    def uuid(self, uuid):
        utils._check_type(uuid, uuidp.UUID)
        ret = native_bt.clock_class_set_uuid(self._ptr, uuid.bytes)
        utils._handle_ret(ret, "cannot set clock class object's UUID")

    def __call__(self, cycles):
        return clock_snapshot._ClockSnapshot(self._ptr, cycles)
