# SPDX-License-Identifier: MIT
#
# Copyright (c) 2017 Philippe Proulx <pproulx@efficios.com>

import abc
import math
import typing
import numbers
import functools
import collections.abc

from bt2 import error as bt2_error
from bt2 import utils as bt2_utils
from bt2 import object as bt2_object
from bt2 import native_bt


def _create_from_ptr_template(ptr, object_map):
    if ptr is None:
        return

    # bt_value_null is translated to None.  However, we are given a reference
    # to it that we are not going to manage anymore, since we don't create a
    # Python wrapper for it.  Therefore put that reference immediately.
    typeid = native_bt.value_get_type(ptr)

    if typeid == native_bt.VALUE_TYPE_NULL:
        _Value._put_ref(ptr)
        return

    return object_map[typeid]._create_from_ptr(ptr)


def _create_from_ptr(ptr):
    return _create_from_ptr_template(ptr, _TYPE_TO_OBJ)


def _create_from_const_ptr(ptr):
    return _create_from_ptr_template(ptr, _TYPE_TO_CONST_OBJ)


def _create_from_ptr_and_get_ref_template(ptr, object_map):
    if ptr is None:
        return

    typeid = native_bt.value_get_type(ptr)

    if typeid == native_bt.VALUE_TYPE_NULL:
        return

    return object_map[typeid]._create_from_ptr_and_get_ref(ptr)


def _create_from_ptr_and_get_ref(ptr):
    return _create_from_ptr_and_get_ref_template(ptr, _TYPE_TO_OBJ)


def _create_from_const_ptr_and_get_ref(ptr):
    return _create_from_ptr_and_get_ref_template(ptr, _TYPE_TO_CONST_OBJ)


def create_value(value: "_ConvertibleToValue") -> typing.Optional["_Value"]:
    if value is None:
        # null value object
        return

    if isinstance(value, (bool, _BoolValueConst)):
        return BoolValue(value)

    if isinstance(value, _UnsignedIntegerValueConst):
        return UnsignedIntegerValue(value)

    if isinstance(value, (numbers.Integral, _SignedIntegerValueConst)):
        return SignedIntegerValue(value)

    if isinstance(value, (numbers.Real, _RealValueConst)):
        return RealValue(value)

    if isinstance(value, (str, _StringValueConst)):
        return StringValue(value)

    if isinstance(value, (collections.abc.Sequence, _ArrayValueConst)):
        return ArrayValue(value)

    if isinstance(value, (collections.abc.Mapping, _MapValueConst)):
        return MapValue(value)

    raise TypeError(
        f"cannot create value object from '{value.__class__.__name__}' object"
    )


class _ValueConst(bt2_object._SharedObject, metaclass=abc.ABCMeta):
    @staticmethod
    def _get_ref(ptr):
        native_bt.value_get_ref(ptr)

    @staticmethod
    def _put_ref(ptr):
        native_bt.value_put_ref(ptr)

    _create_value_from_ptr = staticmethod(_create_from_const_ptr)
    _create_value_from_ptr_and_get_ref = staticmethod(
        _create_from_const_ptr_and_get_ref
    )

    def __ne__(self, other) -> bool:
        return not (self == other)

    def _check_create_status(self, ptr):
        if ptr is None:
            raise bt2_error._MemoryError(
                f"cannot create {self._NAME.lower()} value object"
            )


class _Value(_ValueConst):
    _create_value_from_ptr = staticmethod(_create_from_ptr)
    _create_value_from_ptr_and_get_ref = staticmethod(_create_from_ptr_and_get_ref)


@functools.total_ordering
class _NumericValueConst(_ValueConst):
    @staticmethod
    def _extract_value(other):
        if isinstance(other, _BoolValueConst) or isinstance(other, bool):
            return bool(other)

        if isinstance(other, numbers.Integral):
            return int(other)

        if isinstance(other, numbers.Real):
            return float(other)

        if isinstance(other, numbers.Complex):
            return complex(other)

        raise TypeError(f"'{other.__class__.__name__}' object is not a number object")

    def __int__(self) -> int:
        return int(self._value)

    def __float__(self) -> float:
        return float(self._value)

    def __repr__(self):
        return repr(self._value)

    def __lt__(self, other) -> bool:
        return self._value < self._extract_value(other)

    def __eq__(self, other: object) -> bool:
        try:
            return self._value == self._extract_value(other)
        except Exception:
            return False

    def __rmod__(self, other):
        return self._extract_value(other) % self._value

    def __mod__(self, other):
        return self._value % self._extract_value(other)

    def __rfloordiv__(self, other):
        return self._extract_value(other) // self._value

    def __floordiv__(self, other):
        return self._value // self._extract_value(other)

    def __round__(self, ndigits=None):
        if ndigits is None:
            return round(self._value)
        else:
            return round(self._value, ndigits)

    def __ceil__(self) -> int:
        return math.ceil(self._value)

    def __floor__(self) -> int:
        return math.floor(self._value)

    def __trunc__(self) -> int:
        return int(self._value)

    def __abs__(self):
        return abs(self._value)

    def __add__(self, other):
        return self._value + self._extract_value(other)

    def __radd__(self, other):
        return self.__add__(other)

    def __neg__(self):
        return -self._value

    def __pos__(self):
        return +self._value

    def __mul__(self, other):
        return self._value * self._extract_value(other)

    def __rmul__(self, other):
        return self.__mul__(other)

    def __truediv__(self, other):
        return self._value / self._extract_value(other)

    def __rtruediv__(self, other):
        return self._extract_value(other) / self._value

    def __pow__(self, exponent):
        return self._value ** self._extract_value(exponent)

    def __rpow__(self, base):
        return self._extract_value(base) ** self._value


class _NumericValue(_NumericValueConst, _Value):
    pass


class _IntegralValueConst(_NumericValueConst, numbers.Integral):
    def __lshift__(self, other):
        return self._value << self._extract_value(other)

    def __rlshift__(self, other):
        return self._extract_value(other) << self._value

    def __rshift__(self, other):
        return self._value >> self._extract_value(other)

    def __rrshift__(self, other):
        return self._extract_value(other) >> self._value

    def __and__(self, other):
        return self._value & self._extract_value(other)

    def __rand__(self, other):
        return self._extract_value(other) & self._value

    def __xor__(self, other):
        return self._value ^ self._extract_value(other)

    def __rxor__(self, other):
        return self._extract_value(other) ^ self._value

    def __or__(self, other):
        return self._value | self._extract_value(other)

    def __ror__(self, other):
        return self._extract_value(other) | self._value

    def __invert__(self):
        return ~self._value


class _IntegralValue(_IntegralValueConst, _NumericValue):
    pass


class _BoolValueConst(_IntegralValueConst):
    _NAME = "Const boolean"

    def __bool__(self) -> bool:
        return self._value

    def __repr__(self) -> str:
        return repr(self._value)

    @property
    def _value(self):
        return native_bt.value_bool_get(self._ptr) != 0


class BoolValue(_BoolValueConst, _IntegralValue):
    _NAME = "Boolean"

    def __init__(self, value: typing.Union[None, bool, _BoolValueConst] = None):
        if value is None:
            ptr = native_bt.value_bool_create()
        else:
            ptr = native_bt.value_bool_create_init(self._value_to_bool(value))

        self._check_create_status(ptr)
        super().__init__(ptr)

    @classmethod
    def _value_to_bool(cls, value):
        if isinstance(value, _BoolValueConst):
            value = value._value

        if not isinstance(value, bool):
            raise TypeError(
                f"'{value.__class__}' object is not a 'bool', 'BoolValue', or '_BoolValueConst' object"
            )

        return value

    def _set_value(self, value):
        native_bt.value_bool_set(self._ptr, self._value_to_bool(value))

    value = property(fset=_set_value)


class _IntegerValueConst(_IntegralValueConst):
    @property
    def _value(self):
        return self._get_value(self._ptr)


class _IntegerValue(_IntegerValueConst, _IntegralValue):
    def __init__(self, value: typing.Optional[typing.SupportsInt] = None):
        if value is None:
            ptr = self._create_default_value()
        else:
            ptr = self._create_value(self._value_to_int(value))

        self._check_create_status(ptr)
        super().__init__(ptr)

    @classmethod
    def _value_to_int(cls, value):
        if not isinstance(value, numbers.Integral):
            raise TypeError("expecting an integral number object")

        value = int(value)
        cls._check_int_range(value)
        return value

    def _prop_set_value(self, value):
        self._set_value(self._ptr, self._value_to_int(value))

    value = property(fset=_prop_set_value)


class _UnsignedIntegerValueConst(_IntegerValueConst):
    _NAME = "Const unsigned integer"
    _get_value = staticmethod(native_bt.value_integer_unsigned_get)


class UnsignedIntegerValue(_UnsignedIntegerValueConst, _IntegerValue):
    _NAME = "Unsigned integer"
    _check_int_range = staticmethod(bt2_utils._check_uint64)
    _create_default_value = staticmethod(native_bt.value_integer_unsigned_create)
    _create_value = staticmethod(native_bt.value_integer_unsigned_create_init)
    _set_value = staticmethod(native_bt.value_integer_unsigned_set)


class _SignedIntegerValueConst(_IntegerValueConst):
    _NAME = "Const signed integer"
    _get_value = staticmethod(native_bt.value_integer_signed_get)


class SignedIntegerValue(_SignedIntegerValueConst, _IntegerValue):
    _NAME = "Signed integer"
    _check_int_range = staticmethod(bt2_utils._check_int64)
    _create_default_value = staticmethod(native_bt.value_integer_signed_create)
    _create_value = staticmethod(native_bt.value_integer_signed_create_init)
    _set_value = staticmethod(native_bt.value_integer_signed_set)


class _RealValueConst(_NumericValueConst, numbers.Real):
    _NAME = "Const real number"

    @property
    def _value(self):
        return native_bt.value_real_get(self._ptr)


class RealValue(_RealValueConst, _NumericValue):
    _NAME = "Real number"

    def __init__(self, value: typing.Optional[typing.SupportsFloat] = None):
        if value is None:
            ptr = native_bt.value_real_create()
        else:
            value = self._value_to_float(value)
            ptr = native_bt.value_real_create_init(value)

        self._check_create_status(ptr)
        super().__init__(ptr)

    @classmethod
    def _value_to_float(cls, value):
        if not isinstance(value, numbers.Real):
            raise TypeError("expecting a real number object")

        return float(value)

    def _set_value(self, value):
        native_bt.value_real_set(self._ptr, self._value_to_float(value))

    value = property(fset=_set_value)


@functools.total_ordering
class _StringValueConst(typing.Sequence[str], _Value):
    _NAME = "Const string"

    @classmethod
    def _value_to_str(cls, value):
        if isinstance(value, _StringValueConst):
            value = value._value

        bt2_utils._check_str(value)
        return value

    @property
    def _value(self):
        return native_bt.value_string_get(self._ptr)

    def __eq__(self, other: object) -> bool:
        try:
            return self._value == self._value_to_str(other)
        except Exception:
            return False

    def __lt__(self, other) -> bool:
        return self._value < self._value_to_str(other)

    def __bool__(self) -> bool:
        return bool(self._value)

    def __repr__(self) -> str:
        return repr(self._value)

    def __str__(self) -> str:
        return self._value

    def __getitem__(self, index: int) -> str:
        return self._value[index]

    def __len__(self) -> int:
        return len(self._value)

    def __contains__(self, item: typing.Union[str, "_StringValueConst"]) -> bool:
        return self._value_to_str(item) in self._value


class StringValue(_StringValueConst, _Value):
    _NAME = "String"

    def __init__(self, value: typing.Union[None, str, _StringValueConst] = None):
        if value is None:
            ptr = native_bt.value_string_create()
        else:
            ptr = native_bt.value_string_create_init(self._value_to_str(value))

        self._check_create_status(ptr)
        super().__init__(ptr)

    def _set_value(self, value):
        bt2_utils._handle_func_status(
            native_bt.value_string_set(self._ptr, self._value_to_str(value))
        )

    value = property(fset=_set_value)

    def __iadd__(self, value: typing.Union[str, _StringValueConst]) -> "StringValue":
        curvalue = self._value
        curvalue += self._value_to_str(value)
        self.value = curvalue
        return self


class _ContainerConst:
    def __bool__(self) -> bool:
        return len(self) != 0


class _Container(_ContainerConst):
    def __delitem__(self, index):
        raise NotImplementedError


class _ArrayValueConst(
    _ContainerConst, typing.Sequence[typing.Optional[_ValueConst]], _ValueConst
):
    _NAME = "Const array"
    _borrow_element_by_index = staticmethod(
        native_bt.value_array_borrow_element_by_index_const
    )
    _is_const = True

    def __eq__(self, other: object) -> bool:
        if not isinstance(other, collections.abc.Sequence):
            return False

        if len(self) != len(other):
            # early mismatch
            return False

        for self_elem, other_elem in zip(self, other):
            if self_elem != other_elem:
                return False

        return True

    def __len__(self) -> int:
        return native_bt.value_array_get_length(self._ptr)

    def _check_index(self, index):
        # TODO: support slices also
        if not isinstance(index, numbers.Integral):
            raise TypeError(
                f"'{index.__class__.__name__}' object is not an integral number object: invalid index"
            )

        index = int(index)

        if index < 0 or index >= len(self):
            raise IndexError("array value object index is out of range")

        return index

    def __getitem__(self, index: int) -> typing.Optional[_ValueConst]:
        index = self._check_index(index)
        return self._create_value_from_ptr_and_get_ref(
            self._borrow_element_by_index(self._ptr, index)
        )

    def __repr__(self) -> str:
        return f"[{', '.join([repr(v) for v in self])}]"


class ArrayValue(_ArrayValueConst, _Container, collections.abc.MutableSequence, _Value):
    _NAME = "Array"
    _borrow_element_by_index = staticmethod(
        native_bt.value_array_borrow_element_by_index
    )

    def __init__(self, value: typing.Optional[typing.Iterable] = None):
        ptr = native_bt.value_array_create()
        self._check_create_status(ptr)
        super().__init__(ptr)

        # Python will raise a TypeError if there's anything wrong with
        # the iterable protocol.
        if value is not None:
            for elem in value:
                self.append(elem)

    def __setitem__(self, index: int, value):
        index = self._check_index(index)
        value = create_value(value)

        if value is None:
            ptr = native_bt.value_null
        else:
            ptr = value._ptr

        bt2_utils._handle_func_status(
            native_bt.value_array_set_element_by_index(self._ptr, index, ptr)
        )

    def append(self, value):
        value = create_value(value)

        if value is None:
            ptr = native_bt.value_null
        else:
            ptr = value._ptr

        bt2_utils._handle_func_status(
            native_bt.value_array_append_element(self._ptr, ptr)
        )

    def __iadd__(self, iterable: typing.Iterable):
        # Python will raise a TypeError if there's anything wrong with
        # the iterable protocol.
        for elem in iterable:
            self.append(elem)

        return self

    def insert(self, value):
        raise NotImplementedError


class _MapValueKeyIterator(typing.Iterator[str]):
    def __init__(self, map_obj):
        self._map_obj = map_obj
        self._at = 0
        self._keys = native_bt.value_map_get_keys(map_obj._ptr)

    def __next__(self) -> str:
        if self._at == len(self._map_obj):
            raise StopIteration

        key = self._keys[self._at]
        self._at += 1
        return key


class _MapValueConst(
    _ContainerConst, typing.Mapping[str, typing.Optional[_ValueConst]], _ValueConst
):
    _NAME = "Const map"
    _borrow_entry_value_ptr = staticmethod(native_bt.value_map_borrow_entry_value_const)

    def __ne__(self, other) -> bool:
        return _Value.__ne__(self, other)

    def __eq__(self, other: object) -> bool:
        if not isinstance(other, collections.abc.Mapping):
            return False

        if len(self) != len(other):
            # early mismatch
            return False

        for self_key in self:
            if self_key not in other:
                return False

            if self[self_key] != other[self_key]:
                return False

        return True

    def __len__(self) -> int:
        return native_bt.value_map_get_size(self._ptr)

    def __contains__(self, key: str) -> bool:
        self._check_key_type(key)
        return native_bt.value_map_has_entry(self._ptr, key)

    def _check_key_type(self, key):
        bt2_utils._check_str(key)

    def _check_key(self, key):
        if key not in self:
            raise KeyError(key)

    def __getitem__(self, key: str) -> typing.Optional[_ValueConst]:
        self._check_key(key)
        return self._create_value_from_ptr_and_get_ref(
            self._borrow_entry_value_ptr(self._ptr, key)
        )

    def __iter__(self) -> typing.Iterator[str]:
        return _MapValueKeyIterator(self)

    def __repr__(self) -> str:
        return f"{{{', '.join([f'{k!r}: {v!r}' for k, v in self.items()])}}}"


class MapValue(_MapValueConst, _Container, collections.abc.MutableMapping, _Value):
    _NAME = "Map"
    _borrow_entry_value_ptr = staticmethod(native_bt.value_map_borrow_entry_value)

    def __init__(self, value: typing.Optional[typing.Iterable] = None):
        ptr = native_bt.value_map_create()
        self._check_create_status(ptr)
        super().__init__(ptr)

        # Python will raise a TypeError if there's anything wrong with
        # the iterable/mapping protocol.
        if value is not None:
            for key, elem in value.items():
                self[key] = elem

    def __setitem__(self, key: str, value):
        self._check_key_type(key)
        value = create_value(value)

        if value is None:
            ptr = native_bt.value_null
        else:
            ptr = value._ptr

        status = native_bt.value_map_insert_entry(self._ptr, key, ptr)
        bt2_utils._handle_func_status(status)


_ConvertibleToBoolValue = typing.Union[bool, _BoolValueConst]
_ConvertibleToSignedIntegerValue = typing.Union[int, _SignedIntegerValueConst]
_ConvertibleToUnsignedIntegerValue = _UnsignedIntegerValueConst
_ConvertibleToRealValue = typing.Union[float, _RealValueConst]
_ConvertibleToStringValue = typing.Union[str, _StringValueConst]
_ConvertibleToArrayValue = typing.Union[
    typing.Sequence["_ConvertibleToValue"], _ArrayValueConst
]
_ConvertibleToMapValue = typing.Union[
    typing.Mapping[str, "_ConvertibleToValue"], _MapValueConst
]
_ConvertibleToValue = typing.Union[
    None,
    _ConvertibleToBoolValue,
    _ConvertibleToSignedIntegerValue,
    _ConvertibleToUnsignedIntegerValue,
    _ConvertibleToRealValue,
    _ConvertibleToStringValue,
    _ConvertibleToArrayValue,
    _ConvertibleToMapValue,
]

_TYPE_TO_OBJ = {
    native_bt.VALUE_TYPE_BOOL: BoolValue,
    native_bt.VALUE_TYPE_UNSIGNED_INTEGER: UnsignedIntegerValue,
    native_bt.VALUE_TYPE_SIGNED_INTEGER: SignedIntegerValue,
    native_bt.VALUE_TYPE_REAL: RealValue,
    native_bt.VALUE_TYPE_STRING: StringValue,
    native_bt.VALUE_TYPE_ARRAY: ArrayValue,
    native_bt.VALUE_TYPE_MAP: MapValue,
}

_TYPE_TO_CONST_OBJ = {
    native_bt.VALUE_TYPE_BOOL: _BoolValueConst,
    native_bt.VALUE_TYPE_UNSIGNED_INTEGER: _UnsignedIntegerValueConst,
    native_bt.VALUE_TYPE_SIGNED_INTEGER: _SignedIntegerValueConst,
    native_bt.VALUE_TYPE_REAL: _RealValueConst,
    native_bt.VALUE_TYPE_STRING: _StringValueConst,
    native_bt.VALUE_TYPE_ARRAY: _ArrayValueConst,
    native_bt.VALUE_TYPE_MAP: _MapValueConst,
}
