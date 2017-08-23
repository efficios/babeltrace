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
import collections.abc
import bt2.clock_class
import copy
import bt2


class _ClockClassIterator(collections.abc.Iterator):
    def __init__(self, cc_prio_map):
        self._cc_prio_map = cc_prio_map
        self._at = 0

    def __next__(self):
        if self._at == len(self._cc_prio_map):
            raise StopIteration

        cc_ptr = native_bt.clock_class_priority_map_get_clock_class_by_index(self._cc_prio_map._ptr,
                                                                             self._at)
        assert(cc_ptr)
        clock_class = bt2.ClockClass._create_from_ptr(cc_ptr)
        self._at += 1
        return clock_class


class ClockClassPriorityMap(object._Object, collections.abc.MutableMapping):
    def __init__(self, clock_class_priorities=None):
        ptr = native_bt.clock_class_priority_map_create()

        if ptr is None:
            raise bt2.CreationError('cannot create clock class priority map object')

        super().__init__(ptr)

        if clock_class_priorities is not None:
            for clock_class, priority in clock_class_priorities.items():
                self[clock_class] = priority

    def __getitem__(self, key):
        utils._check_type(key, bt2.ClockClass)
        ret, prio = native_bt.clock_class_priority_map_get_clock_class_priority(self._ptr,
                                                                                key._ptr)

        if ret != 0:
            raise KeyError(key)

        return prio

    def __len__(self):
        count = native_bt.clock_class_priority_map_get_clock_class_count(self._ptr)
        assert(count >= 0)
        return count

    def __delitem__(self):
        raise NotImplementedError

    def __setitem__(self, key, value):
        utils._check_type(key, bt2.ClockClass)
        utils._check_uint64(value)
        ret = native_bt.clock_class_priority_map_add_clock_class(self._ptr,
                                                                 key._ptr,
                                                                 value)
        utils._handle_ret(ret, "cannot set clock class's priority in clock class priority map object")

    def __iter__(self):
        return _ClockClassIterator(self)

    @property
    def highest_priority_clock_class(self):
        cc_ptr = native_bt.clock_class_priority_map_get_highest_priority_clock_class(self._ptr)

        if cc_ptr is None:
            return

        return bt2.ClockClass._create_from_ptr(cc_ptr)

    def _get_prios(self):
        prios = {}

        for clock_class, prio in self.items():
            prios[clock_class] = prio

        return prios

    def __eq__(self, other):
        if type(other) is not type(self):
            return False

        if self.addr == other.addr:
            return True

        return self._get_prios() == other._get_prios()

    def _copy(self, cc_copy_func):
        cpy = ClockClassPriorityMap()

        for clock_class, prio in self.items():
            cpy[cc_copy_func(clock_class)] = prio

        return cpy

    def __copy__(self):
        return self._copy(lambda obj: obj)

    def __deepcopy__(self, memo):
        cpy = self._copy(copy.deepcopy)
        memo[id(self)] = cpy
        return cpy
