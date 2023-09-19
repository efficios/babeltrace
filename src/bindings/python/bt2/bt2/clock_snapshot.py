# SPDX-License-Identifier: MIT
#
# Copyright (c) 2017 Philippe Proulx <pproulx@efficios.com>

import numbers
import functools

from bt2 import utils as bt2_utils
from bt2 import object as bt2_object
from bt2 import native_bt
from bt2 import clock_class as bt2_clock_class


@functools.total_ordering
class _ClockSnapshotConst(bt2_object._UniqueObject):
    @property
    def clock_class(self):
        cc_ptr = native_bt.clock_snapshot_borrow_clock_class_const(self._ptr)
        assert cc_ptr is not None
        return bt2_clock_class._ClockClassConst._create_from_ptr_and_get_ref(cc_ptr)

    @property
    def value(self):
        return native_bt.clock_snapshot_get_value(self._ptr)

    @property
    def ns_from_origin(self):
        status, ns = native_bt.clock_snapshot_get_ns_from_origin(self._ptr)
        bt2_utils._handle_func_status(
            status, "cannot get clock snapshot's nanoseconds from origin"
        )
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
