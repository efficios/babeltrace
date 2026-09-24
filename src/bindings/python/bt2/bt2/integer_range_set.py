# SPDX-License-Identifier: MIT
#
# Copyright (c) 2017 Philippe Proulx <pproulx@efficios.com>

import typing
import collections.abc

from bt2 import error as bt2_error
from bt2 import utils as bt2_utils
from bt2 import object as bt2_object
from bt2 import native_bt


class _IntegerRangeConst:
    def __init__(self, lower, upper=None):
        self._check_type(lower)

        if upper is None:
            upper = lower

        self._check_type(upper)

        if lower > upper:
            raise ValueError(
                f"range's lower bound ({lower}) is greater than its upper bound ({upper})"
            )

        self._lower = lower
        self._upper = upper

    @property
    def lower(self) -> int:
        return self._lower

    @property
    def upper(self) -> int:
        return self._upper

    def contains(self, value: int) -> bool:
        self._check_type(value)
        return value >= self._lower and value <= self._upper

    def __eq__(self, other: object) -> bool:
        if not isinstance(other, _IntegerRangeConst):
            return False

        return self.lower == other.lower and self.upper == other.upper


class _IntegerRange(_IntegerRangeConst):
    def __init__(self, lower: int, upper: typing.Optional[int] = None):
        super().__init__(lower, upper)


class _SignedIntegerRangeConst(_IntegerRangeConst):
    _is_type = staticmethod(bt2_utils._is_int64)
    _check_type = staticmethod(bt2_utils._check_int64)


class SignedIntegerRange(_SignedIntegerRangeConst, _IntegerRange):
    pass


class _UnsignedIntegerRangeConst(_IntegerRangeConst):
    _is_type = staticmethod(bt2_utils._is_uint64)
    _check_type = staticmethod(bt2_utils._check_uint64)


class UnsignedIntegerRange(_UnsignedIntegerRangeConst, _IntegerRange):
    pass


class _IntegerRangeSetConst(bt2_object._SharedObject, collections.abc.Set):
    def __len__(self) -> int:
        return native_bt.integer_range_set_get_range_count(
            self._as_range_set_ptr(self._ptr)
        )

    def __contains__(self, other_range: object) -> bool:
        for rg in self:
            if rg == other_range:
                return True

        return False

    def __iter__(self) -> typing.Iterator[_IntegerRangeConst]:
        for idx in range(len(self)):
            rg_ptr = self._borrow_range_ptr_by_index(self._ptr, idx)
            yield self._range_pycls(
                self._range_get_lower(rg_ptr), self._range_get_upper(rg_ptr)
            )

    def __eq__(self, other: object) -> bool:
        if not isinstance(other, _IntegerRangeSetConst):
            return False

        # a signed and an unsigned integer range set are never equal
        if self._is_equal is not other._is_equal:
            return False

        return self._is_equal(self._ptr, other._ptr)

    def contains_value(self, value: int) -> bool:
        for rg in self:
            if rg.contains(value):
                return True

        return False


class _IntegerRangeSet(_IntegerRangeSetConst, collections.abc.MutableSet):
    def __init__(
        self,
        ranges: typing.Union[
            None,
            typing.Iterable[_IntegerRange],
            typing.Iterable[typing.Union[typing.Tuple[int, int], int]],
        ] = None,
    ):
        ptr = self._create_range_set()

        if ptr is None:
            raise bt2_error._MemoryError("cannot create range set object")

        super().__init__(ptr)

        if ranges is not None:
            # will raise if not iterable
            for rg in ranges:
                self.add(rg)

    def add(
        self, rg: typing.Union[_IntegerRange, typing.Union[typing.Tuple[int, int], int]]
    ):
        if type(rg) is not self._range_pycls:
            if self._range_pycls._is_type(rg):
                rg = self._range_pycls(rg)
            else:
                # assume it's a simple pair (will raise if it's not)
                rg = self._range_pycls(rg[0], rg[1])

        bt2_utils._handle_func_status(
            self._add_range(self._ptr, rg.lower, rg.upper),
            "cannot add range to range set object",
        )

    def discard(self, rg: _IntegerRange):
        raise NotImplementedError


class _SignedIntegerRangeSetConst(_IntegerRangeSetConst):
    @staticmethod
    def _get_ref(ptr):
        native_bt.integer_range_set_signed_get_ref(ptr)

    @staticmethod
    def _put_ref(ptr):
        native_bt.integer_range_set_signed_put_ref(ptr)

    _as_range_set_ptr = staticmethod(
        native_bt.integer_range_set_signed_as_range_set_const
    )
    _borrow_range_ptr_by_index = staticmethod(
        native_bt.integer_range_set_signed_borrow_range_by_index_const
    )
    _range_get_lower = staticmethod(native_bt.integer_range_signed_get_lower)
    _range_get_upper = staticmethod(native_bt.integer_range_signed_get_upper)
    _is_equal = staticmethod(native_bt.integer_range_set_signed_is_equal)
    _range_pycls = _SignedIntegerRangeConst


class SignedIntegerRangeSet(_SignedIntegerRangeSetConst, _IntegerRangeSet):
    _create_range_set = staticmethod(native_bt.integer_range_set_signed_create)
    _add_range = staticmethod(native_bt.integer_range_set_signed_add_range)
    _range_pycls = SignedIntegerRange


class _UnsignedIntegerRangeSetConst(_IntegerRangeSetConst):
    @staticmethod
    def _get_ref(ptr):
        native_bt.integer_range_set_unsigned_get_ref(ptr)

    @staticmethod
    def _put_ref(ptr):
        native_bt.integer_range_set_unsigned_put_ref(ptr)

    _as_range_set_ptr = staticmethod(
        native_bt.integer_range_set_unsigned_as_range_set_const
    )
    _borrow_range_ptr_by_index = staticmethod(
        native_bt.integer_range_set_unsigned_borrow_range_by_index_const
    )
    _range_get_lower = staticmethod(native_bt.integer_range_unsigned_get_lower)
    _range_get_upper = staticmethod(native_bt.integer_range_unsigned_get_upper)
    _is_equal = staticmethod(native_bt.integer_range_set_unsigned_is_equal)
    _range_pycls = _UnsignedIntegerRangeConst


class UnsignedIntegerRangeSet(_UnsignedIntegerRangeSetConst, _IntegerRangeSet):
    _create_range_set = staticmethod(native_bt.integer_range_set_unsigned_create)
    _add_range = staticmethod(native_bt.integer_range_set_unsigned_add_range)
    _range_pycls = UnsignedIntegerRange
