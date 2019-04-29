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


def _create_clock_snapshot_from_ptr(ptr):
    clock_snapshot = _ClockSnapshot._create_from_ptr(ptr)
    return clock_snapshot


class _ClockSnapshot(object._UniqueObject):
    def __init__(self, clock_class_ptr, cycles):
        utils._check_uint64(cycles)
        ptr = native_bt.clock_snapshot_create(clock_class_ptr, cycles)

        if ptr is None:
            raise bt2.CreationError('cannot create clock value object')

        super().__init__(ptr)

    @property
    def clock_class(self):
        ptr = native_bt.clock_snapshot_get_class(self._ptr)
        assert(ptr)
        return bt2.ClockClass._create_from_ptr(ptr)

    @property
    def cycles(self):
        ret, cycles = native_bt.clock_snapshot_get_value(self._ptr)
        assert(ret == 0)
        return cycles

    @property
    def ns_from_epoch(self):
        ret, ns = native_bt.clock_snapshot_get_value_ns_from_epoch(self._ptr)
        utils._handle_ret(ret, "cannot get clock value object's nanoseconds from Epoch")
        return ns

    def __eq__(self, other):
        if isinstance(other, numbers.Integral):
            return int(other) == self.cycles

        if not isinstance(other, self.__class__):
            # not comparing apples to apples
            return False

        if self.addr == other.addr:
            return True

        self_props = self.clock_class, self.cycles
        other_props = other.clock_class, other.cycles
        return self_props == other_props

    def __copy__(self):
        return self.clock_class(self.cycles)

    def __deepcopy__(self, memo):
        cpy = self.__copy__()
        memo[id(self)] = cpy
        return cpy
