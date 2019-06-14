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
import numbers
import bt2
import functools


@functools.total_ordering
class _ClockSnapshot(object._UniqueObject):
    @property
    def clock_class(self):
        cc_ptr = native_bt.clock_snapshot_borrow_clock_class_const(self._ptr)
        assert cc_ptr is not None
        return bt2.clock_class._ClockClass._create_from_ptr_and_get_ref(cc_ptr)

    @property
    def value(self):
        return native_bt.clock_snapshot_get_value(self._ptr)

    @property
    def ns_from_origin(self):
        ret, ns = native_bt.clock_snapshot_get_ns_from_origin(self._ptr)

        if ret == native_bt.CLOCK_SNAPSHOT_STATUS_OVERFLOW:
            raise OverflowError("cannot get clock snapshot's nanoseconds from origin")

        return ns

    def __eq__(self, other):
        if not isinstance(other, numbers.Integral):
            return NotImplemented

        return self.value == int(other)

    def __lt__(self, other):
        if not isinstance(other, numbers.Integral):
            return NotImplemented

        return self.value < int(other)


class _UnknownClockSnapshot:
    pass


class _InfiniteClockSnapshot:
    pass
