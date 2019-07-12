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
import bt2


class _IntegerRange:
    def __init__(self, lower, upper):
        self._check_type(lower)
        self._check_type(upper)

        if lower > upper:
            raise ValueError("range's lower bound ({}) is greater than its upper bound ({})".format(lower, upper))

        self._lower = lower
        self._upper = upper

    @property
    def lower(self):
        return self._lower

    @property
    def upper(self):
        return self._upper

    def contains(self, value):
        self._check_type(value)
        return value >= self._lower and value <= self._upper

    def __eq__(self, other):
        if type(other) is not type(self):
            return False

        return self.lower == other.lower and self.upper == other.upper


class SignedIntegerRange(_IntegerRange):
    _check_type = staticmethod(utils._check_int64)


class UnsignedIntegerRange(_IntegerRange):
    _check_type = staticmethod(utils._check_uint64)


class _IntegerRangeSet(object._SharedObject, collections.abc.MutableSet):
    def __init__(self, ranges=None):
        ptr = self._create_range_set()

        if ptr is None:
            raise bt2.CreationError('cannot create range set object')

        super().__init__(ptr)

        if ranges is not None:
            # will raise if not iterable
            for rg in ranges:
                self.add(rg)

    def __len__(self):
        range_set_ptr = self._as_range_set_ptr(self._ptr)
        count = native_bt.integer_range_set_get_range_count(range_set_ptr)
        assert count >= 0
        return count

    def __contains__(self, other_range):
        for rg in self:
            if rg == other_range:
                return True

        return False

    def __iter__(self):
        for idx in range(len(self)):
            rg_ptr = self._borrow_range_by_index_ptr(self._ptr, idx)
            assert rg_ptr is not None
            lower = self._range_get_lower(rg_ptr)
            upper = self._range_get_upper(rg_ptr)
            yield self._range_type(lower, upper)

    def __eq__(self, other):
        if type(other) is not type(self):
            return False

        return self._compare(self._ptr, other._ptr)

    def contains_value(self, value):
        for rg in self:
            if rg.contains(value):
                return True

        return False

    def add(self, rg):
        if type(rg) is not self._range_type:
            # assume it's a simple pair (will raise if it's not)
            rg = self._range_type(rg[0], rg[1])

        status = self._add_range(self._ptr, rg.lower, rg.upper)
        utils._handle_func_status(status,
                                  'cannot add range to range set object')

    def discard(self, rg):
        raise NotImplementedError


class SignedIntegerRangeSet(_IntegerRangeSet):
    _get_ref = staticmethod(native_bt.integer_range_set_signed_get_ref)
    _put_ref = staticmethod(native_bt.integer_range_set_signed_put_ref)
    _as_range_set_ptr = staticmethod(native_bt.integer_range_set_signed_as_range_set_const)
    _create_range_set = staticmethod(native_bt.integer_range_set_signed_create)
    _borrow_range_by_index_ptr = staticmethod(native_bt.integer_range_set_signed_borrow_range_by_index_const)
    _range_get_lower = staticmethod(native_bt.integer_range_signed_get_lower)
    _range_get_upper = staticmethod(native_bt.integer_range_signed_get_upper)
    _add_range = staticmethod(native_bt.integer_range_set_signed_add_range)
    _compare = staticmethod(native_bt.integer_range_set_signed_compare)
    _range_type = SignedIntegerRange


class UnsignedIntegerRangeSet(_IntegerRangeSet):
    _get_ref = staticmethod(native_bt.integer_range_set_unsigned_get_ref)
    _put_ref = staticmethod(native_bt.integer_range_set_unsigned_put_ref)
    _as_range_set_ptr = staticmethod(native_bt.integer_range_set_unsigned_as_range_set_const)
    _create_range_set = staticmethod(native_bt.integer_range_set_unsigned_create)
    _borrow_range_by_index_ptr = staticmethod(native_bt.integer_range_set_unsigned_borrow_range_by_index_const)
    _range_get_lower = staticmethod(native_bt.integer_range_unsigned_get_lower)
    _range_get_upper = staticmethod(native_bt.integer_range_unsigned_get_upper)
    _add_range = staticmethod(native_bt.integer_range_set_unsigned_add_range)
    _compare = staticmethod(native_bt.integer_range_set_unsigned_compare)
    _range_type = UnsignedIntegerRange
