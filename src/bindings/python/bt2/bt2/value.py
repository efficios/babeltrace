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
import functools
import numbers
import math
import abc
import bt2


def _create_from_ptr_template(ptr, object_map):
    if ptr is None:
        return

    # bt_value_null is translated to None.  However, we are given a reference
    # to it that we are not going to manage anymore, since we don't create a
    # Python wrapper for it.  Therefore put that reference immediately.
    if ptr == native_bt.value_null:
        _Value._put_ref(ptr)
        return

    typeid = native_bt.value_get_type(ptr)
    return object_map[typeid]._create_from_ptr(ptr)


def _create_from_ptr(ptr):
    return _create_from_ptr_template(ptr, _TYPE_TO_OBJ)


def _create_from_const_ptr(ptr):
    return _create_from_ptr_template(ptr, _TYPE_TO_CONST_OBJ)


def _create_from_ptr_and_get_ref_template(ptr, object_map):
    if ptr is None or ptr == native_bt.value_null:
        return

    typeid = native_bt.value_get_type(ptr)
    return object_map[typeid]._create_from_ptr_and_get_ref(ptr)


def _create_from_ptr_and_get_ref(ptr):
    return _create_from_ptr_and_get_ref_template(ptr, _TYPE_TO_OBJ)


def _create_from_const_ptr_and_get_ref(ptr):
    return _create_from_ptr_and_get_ref_template(ptr, _TYPE_TO_CONST_OBJ)


def create_value(value):
    if value is None:
        # null value object
        return

    if isinstance(value, _Value):
        return value

    if isinstance(value, bool):
        return BoolValue(value)

    if isinstance(value, numbers.Integral):
        return SignedIntegerValue(value)

    if isinstance(value, numbers.Real):
        return RealValue(value)

    if isinstance(value, str):
        return StringValue(value)

    if isinstance(value, collections.abc.Sequence):
        return ArrayValue(value)

    if isinstance(value, collections.abc.Mapping):
        return MapValue(value)

    raise TypeError(
        "cannot create value object from '{}' object".format(value.__class__.__name__)
    )


class _ValueConst(object._SharedObject, metaclass=abc.ABCMeta):
    _get_ref = staticmethod(native_bt.value_get_ref)
    _put_ref = staticmethod(native_bt.value_put_ref)
    _create_value_from_ptr = staticmethod(_create_from_const_ptr)
    _create_value_from_ptr_and_get_ref = staticmethod(
        _create_from_const_ptr_and_get_ref
    )

    def __ne__(self, other):
        return not (self == other)

    def _check_create_status(self, ptr):
        if ptr is None:
            raise bt2._MemoryError(
                'cannot create {} value object'.format(self._NAME.lower())
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

        raise TypeError(
            "'{}' object is not a number object".format(other.__class__.__name__)
        )

    def __int__(self):
        return int(self._value)

    def __float__(self):
        return float(self._value)

    def __repr__(self):
        return repr(self._value)

    def __lt__(self, other):
        return self._value < self._extract_value(other)

    def __eq__(self, other):
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

    def __ceil__(self):
        return math.ceil(self._value)

    def __floor__(self):
        return math.floor(self._value)

    def __trunc__(self):
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
    _NAME = 'Const boolean'

    def __bool__(self):
        return self._value

    def __repr__(self):
        return repr(self._value)

    @property
    def _value(self):
        value = native_bt.value_bool_get(self._ptr)
        return value != 0


class BoolValue(_BoolValueConst, _IntegralValue):
    _NAME = 'Boolean'

    def __init__(self, value=None):
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
                "'{}' object is not a 'bool', 'BoolValue', or '_BoolValueConst' object".format(
                    value.__class__
                )
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
    def __init__(self, value=None):
        if value is None:
            ptr = self._create_default_value()
        else:
            ptr = self._create_value(self._value_to_int(value))

        self._check_create_status(ptr)
        super().__init__(ptr)

    @classmethod
    def _value_to_int(cls, value):
        if not isinstance(value, numbers.Integral):
            raise TypeError('expecting an integral number object')

        value = int(value)
        cls._check_int_range(value)
        return value

    def _prop_set_value(self, value):
        self._set_value(self._ptr, self._value_to_int(value))

    value = property(fset=_prop_set_value)


class _UnsignedIntegerValueConst(_IntegerValueConst):
    _NAME = 'Const unsigned integer'
    _get_value = staticmethod(native_bt.value_integer_unsigned_get)


class UnsignedIntegerValue(_UnsignedIntegerValueConst, _IntegerValue):
    _NAME = 'Unsigned integer'
    _check_int_range = staticmethod(utils._check_uint64)
    _create_default_value = staticmethod(native_bt.value_integer_unsigned_create)
    _create_value = staticmethod(native_bt.value_integer_unsigned_create_init)
    _set_value = staticmethod(native_bt.value_integer_unsigned_set)


class _SignedIntegerValueConst(_IntegerValueConst):
    _NAME = 'Const signed integer'
    _get_value = staticmethod(native_bt.value_integer_signed_get)


class SignedIntegerValue(_SignedIntegerValueConst, _IntegerValue):
    _NAME = 'Signed integer'
    _check_int_range = staticmethod(utils._check_int64)
    _create_default_value = staticmethod(native_bt.value_integer_signed_create)
    _create_value = staticmethod(native_bt.value_integer_signed_create_init)
    _set_value = staticmethod(native_bt.value_integer_signed_set)


class _RealValueConst(_NumericValueConst, numbers.Real):
    _NAME = 'Const real number'

    @property
    def _value(self):
        return native_bt.value_real_get(self._ptr)


class RealValue(_RealValueConst, _NumericValue):
    _NAME = 'Real number'

    def __init__(self, value=None):
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
class _StringValueConst(collections.abc.Sequence, _Value):
    _NAME = 'Const string'

    @classmethod
    def _value_to_str(cls, value):
        if isinstance(value, _StringValueConst):
            value = value._value

        utils._check_str(value)
        return value

    @property
    def _value(self):
        return native_bt.value_string_get(self._ptr)

    def __eq__(self, other):
        try:
            return self._value == self._value_to_str(other)
        except Exception:
            return False

    def __lt__(self, other):
        return self._value < self._value_to_str(other)

    def __bool__(self):
        return bool(self._value)

    def __repr__(self):
        return repr(self._value)

    def __str__(self):
        return self._value

    def __getitem__(self, index):
        return self._value[index]

    def __len__(self):
        return len(self._value)

    def __contains__(self, item):
        return self._value_to_str(item) in self._value


class StringValue(_StringValueConst, _Value):
    _NAME = 'String'

    def __init__(self, value=None):
        if value is None:
            ptr = native_bt.value_string_create()
        else:
            ptr = native_bt.value_string_create_init(self._value_to_str(value))

        self._check_create_status(ptr)
        super().__init__(ptr)

    def _set_value(self, value):
        status = native_bt.value_string_set(self._ptr, self._value_to_str(value))
        utils._handle_func_status(status)

    value = property(fset=_set_value)

    def __iadd__(self, value):
        curvalue = self._value
        curvalue += self._value_to_str(value)
        self.value = curvalue
        return self


class _ContainerConst:
    def __bool__(self):
        return len(self) != 0


class _Container(_ContainerConst):
    def __delitem__(self, index):
        raise NotImplementedError


class _ArrayValueConst(_ContainerConst, collections.abc.Sequence, _ValueConst):
    _NAME = 'Const array'
    _borrow_element_by_index = staticmethod(
        native_bt.value_array_borrow_element_by_index_const
    )
    _is_const = True

    def __eq__(self, other):
        if not isinstance(other, collections.abc.Sequence):
            return False

        if len(self) != len(other):
            # early mismatch
            return False

        for self_elem, other_elem in zip(self, other):
            if self_elem != other_elem:
                return False

        return True

    def __len__(self):
        size = native_bt.value_array_get_length(self._ptr)
        assert size >= 0
        return size

    def _check_index(self, index):
        # TODO: support slices also
        if not isinstance(index, numbers.Integral):
            raise TypeError(
                "'{}' object is not an integral number object: invalid index".format(
                    index.__class__.__name__
                )
            )

        index = int(index)

        if index < 0 or index >= len(self):
            raise IndexError('array value object index is out of range')

    def __getitem__(self, index):
        self._check_index(index)
        ptr = self._borrow_element_by_index(self._ptr, index)
        assert ptr
        return self._create_value_from_ptr_and_get_ref(ptr)

    def __repr__(self):
        return '[{}]'.format(', '.join([repr(v) for v in self]))


class ArrayValue(_ArrayValueConst, _Container, collections.abc.MutableSequence, _Value):
    _NAME = 'Array'
    _borrow_element_by_index = staticmethod(
        native_bt.value_array_borrow_element_by_index
    )

    def __init__(self, value=None):
        ptr = native_bt.value_array_create()
        self._check_create_status(ptr)
        super().__init__(ptr)

        # Python will raise a TypeError if there's anything wrong with
        # the iterable protocol.
        if value is not None:
            for elem in value:
                self.append(elem)

    def __setitem__(self, index, value):
        self._check_index(index)
        value = create_value(value)

        if value is None:
            ptr = native_bt.value_null
        else:
            ptr = value._ptr

        status = native_bt.value_array_set_element_by_index(self._ptr, index, ptr)
        utils._handle_func_status(status)

    def append(self, value):
        value = create_value(value)

        if value is None:
            ptr = native_bt.value_null
        else:
            ptr = value._ptr

        status = native_bt.value_array_append_element(self._ptr, ptr)
        utils._handle_func_status(status)

    def __iadd__(self, iterable):
        # Python will raise a TypeError if there's anything wrong with
        # the iterable protocol.
        for elem in iterable:
            self.append(elem)

        return self

    def insert(self, value):
        raise NotImplementedError


class _MapValueKeyIterator(collections.abc.Iterator):
    def __init__(self, map_obj):
        self._map_obj = map_obj
        self._at = 0
        keys_ptr = native_bt.value_map_get_keys(map_obj._ptr)

        if keys_ptr is None:
            raise RuntimeError('unexpected error: cannot get map value object keys')

        self._keys = _create_from_ptr(keys_ptr)

    def __next__(self):
        if self._at == len(self._map_obj):
            raise StopIteration

        key = self._keys[self._at]
        self._at += 1
        return str(key)


class _MapValueConst(_ContainerConst, collections.abc.Mapping, _ValueConst):
    _NAME = 'Const map'
    _borrow_entry_value_ptr = staticmethod(native_bt.value_map_borrow_entry_value_const)

    def __ne__(self, other):
        return _Value.__ne__(self, other)

    def __eq__(self, other):
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

    def __len__(self):
        size = native_bt.value_map_get_size(self._ptr)
        assert size >= 0
        return size

    def __contains__(self, key):
        self._check_key_type(key)
        return native_bt.value_map_has_entry(self._ptr, key)

    def _check_key_type(self, key):
        utils._check_str(key)

    def _check_key(self, key):
        if key not in self:
            raise KeyError(key)

    def __getitem__(self, key):
        self._check_key(key)
        ptr = self._borrow_entry_value_ptr(self._ptr, key)
        assert ptr
        return self._create_value_from_ptr_and_get_ref(ptr)

    def __iter__(self):
        return _MapValueKeyIterator(self)

    def __repr__(self):
        items = ['{}: {}'.format(repr(k), repr(v)) for k, v in self.items()]
        return '{{{}}}'.format(', '.join(items))


class MapValue(_MapValueConst, _Container, collections.abc.MutableMapping, _Value):
    _NAME = 'Map'
    _borrow_entry_value_ptr = staticmethod(native_bt.value_map_borrow_entry_value)

    def __init__(self, value=None):
        ptr = native_bt.value_map_create()
        self._check_create_status(ptr)
        super().__init__(ptr)

        # Python will raise a TypeError if there's anything wrong with
        # the iterable/mapping protocol.
        if value is not None:
            for key, elem in value.items():
                self[key] = elem

    def __setitem__(self, key, value):
        self._check_key_type(key)
        value = create_value(value)

        if value is None:
            ptr = native_bt.value_null
        else:
            ptr = value._ptr

        status = native_bt.value_map_insert_entry(self._ptr, key, ptr)
        utils._handle_func_status(status)


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
