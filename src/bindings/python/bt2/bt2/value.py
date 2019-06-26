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


def _handle_status(status, obj_name):
    if status >= 0:
        return
    else:
        raise RuntimeError('unexpected error')


def _create_from_ptr(ptr):
    if ptr is None or ptr == native_bt.value_null:
        return

    typeid = native_bt.value_get_type(ptr)
    return _TYPE_TO_OBJ[typeid]._create_from_ptr(ptr)


def _create_from_ptr_and_get_ref(ptr):
    if ptr is None or ptr == native_bt.value_null:
        return

    typeid = native_bt.value_get_type(ptr)
    return _TYPE_TO_OBJ[typeid]._create_from_ptr_and_get_ref(ptr)


def create_value(value):
    if value is None:
        # null value object
        return

    if isinstance(value, _Value):
        return value

    if isinstance(value, bool):
        return BoolValue(value)

    if isinstance(value, int):
        return SignedIntegerValue(value)

    if isinstance(value, float):
        return RealValue(value)

    if isinstance(value, str):
        return StringValue(value)

    try:
        return MapValue(value)
    except:
        pass

    try:
        return ArrayValue(value)
    except:
        pass

    raise TypeError("cannot create value object from '{}' object".format(value.__class__.__name__))


class _Value(object._SharedObject, metaclass=abc.ABCMeta):
    _get_ref = staticmethod(native_bt.value_get_ref)
    _put_ref = staticmethod(native_bt.value_put_ref)

    def __eq__(self, other):
        if other is None:
            # self is never the null value object
            return False

        # try type-specific comparison first
        spec_eq = self._spec_eq(other)

        if spec_eq is not None:
            return spec_eq

        if not isinstance(other, _Value):
            # not comparing apples to apples
            return False

        # fall back to native comparison function
        return native_bt.value_compare(self._ptr, other._ptr)

    def __ne__(self, other):
        return not (self == other)

    @abc.abstractmethod
    def _spec_eq(self, other):
        pass

    def _handle_status(self, status):
        _handle_status(status, self._NAME)

    def _check_create_status(self, ptr):
        if ptr is None:
            raise bt2.CreationError(
                'cannot create {} value object'.format(self._NAME.lower()))


@functools.total_ordering
class _NumericValue(_Value):
    @staticmethod
    def _extract_value(other):
        if isinstance(other, _NumericValue):
            return other._value

        if other is True or other is False:
            return other

        if isinstance(other, numbers.Integral):
            return int(other)

        if isinstance(other, numbers.Real):
            return float(other)

        if isinstance(other, numbers.Complex):
            return complex(other)

        raise TypeError("'{}' object is not a number object".format(other.__class__.__name__))

    def __int__(self):
        return int(self._value)

    def __float__(self):
        return float(self._value)

    def __repr__(self):
        return repr(self._value)

    def __lt__(self, other):
        if not isinstance(other, numbers.Number):
            raise TypeError('unorderable types: {}() < {}()'.format(self.__class__.__name__,
                                                                    other.__class__.__name__))

        return self._value < float(other)

    def _spec_eq(self, other):
        pass

    def __eq__(self, other):
        if not isinstance(other, numbers.Number):
            return False

        return self._value == complex(other)

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

    def __iadd__(self, other):
        self.value = self + other
        return self

    def __isub__(self, other):
        self.value = self - other
        return self

    def __imul__(self, other):
        self.value = self * other
        return self

    def __itruediv__(self, other):
        self.value = self / other
        return self

    def __ifloordiv__(self, other):
        self.value = self // other
        return self

    def __imod__(self, other):
        self.value = self % other
        return self

    def __ipow__(self, other):
        self.value = self ** other
        return self


class _IntegralValue(_NumericValue, numbers.Integral):
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

    def __ilshift__(self, other):
        self.value = self << other
        return self

    def __irshift__(self, other):
        self.value = self >> other
        return self

    def __iand__(self, other):
        self.value = self & other
        return self

    def __ixor__(self, other):
        self.value = self ^ other
        return self

    def __ior__(self, other):
        self.value = self | other
        return self


class _RealValue(_NumericValue, numbers.Real):
    pass


class BoolValue(_Value):
    _NAME = 'Boolean'

    def __init__(self, value=None):
        if value is None:
            ptr = native_bt.value_bool_create()
        else:
            ptr = native_bt.value_bool_create_init(self._value_to_bool(value))

        self._check_create_status(ptr)
        super().__init__(ptr)

    def _spec_eq(self, other):
        if isinstance(other, numbers.Number):
            return self._value == bool(other)

    def __bool__(self):
        return self._value

    def __repr__(self):
        return repr(self._value)

    def _value_to_bool(self, value):
        if isinstance(value, BoolValue):
            value = value._value

        if not isinstance(value, bool):
            raise TypeError("'{}' object is not a 'bool' or 'BoolValue' object".format(value.__class__))

        return int(value)

    @property
    def _value(self):
        value = native_bt.value_bool_get(self._ptr)
        return value != 0

    def _set_value(self, value):
        native_bt.value_bool_set(self._ptr, self._value_to_bool(value))

    value = property(fset=_set_value)


class _IntegerValue(_IntegralValue):
    def __init__(self, value=None):
        if value is None:
            ptr = self._create_default_value()
        else:
            ptr = self._create_value(self._value_to_int(value))

        self._check_create_status(ptr)
        super().__init__(ptr)

    def _value_to_int(self, value):
        if not isinstance(value, numbers.Real):
            raise TypeError('expecting a number object')

        value = int(value)
        self._check_int_range(value)
        return value

    @property
    def _value(self):
        return self._get_value(self._ptr)

    def _prop_set_value(self, value):
        self._set_value(self._ptr, self._value_to_int(value))

    value = property(fset=_prop_set_value)


class UnsignedIntegerValue(_IntegerValue):
    _check_int_range = staticmethod(utils._check_uint64)
    _create_default_value = staticmethod(native_bt.value_unsigned_integer_create)
    _create_value = staticmethod(native_bt.value_unsigned_integer_create_init)
    _set_value = staticmethod(native_bt.value_unsigned_integer_set)
    _get_value = staticmethod(native_bt.value_unsigned_integer_get)


class SignedIntegerValue(_IntegerValue):
    _check_int_range = staticmethod(utils._check_int64)
    _create_default_value = staticmethod(native_bt.value_signed_integer_create)
    _create_value = staticmethod(native_bt.value_signed_integer_create_init)
    _set_value = staticmethod(native_bt.value_signed_integer_set)
    _get_value = staticmethod(native_bt.value_signed_integer_get)


class RealValue(_RealValue):
    _NAME = 'Real number'

    def __init__(self, value=None):
        if value is None:
            ptr = native_bt.value_real_create()
        else:
            value = self._value_to_float(value)
            ptr = native_bt.value_real_create_init(value)

        self._check_create_status(ptr)
        super().__init__(ptr)

    def _value_to_float(self, value):
        if not isinstance(value, numbers.Real):
            raise TypeError("expecting a real number object")

        return float(value)

    @property
    def _value(self):
        return native_bt.value_real_get(self._ptr)

    def _set_value(self, value):
        native_bt.value_real_set(self._ptr, self._value_to_float(value))

    value = property(fset=_set_value)


@functools.total_ordering
class StringValue(collections.abc.Sequence, _Value):
    _NAME = 'String'

    def __init__(self, value=None):
        if value is None:
            ptr = native_bt.value_string_create()
        else:
            ptr = native_bt.value_string_create_init(self._value_to_str(value))

        self._check_create_status(ptr)
        super().__init__(ptr)

    def _value_to_str(self, value):
        if isinstance(value, self.__class__):
            value = value._value

        utils._check_str(value)
        return value

    @property
    def _value(self):
        return native_bt.value_string_get(self._ptr)

    def _set_value(self, value):
        status = native_bt.value_string_set(self._ptr, self._value_to_str(value))
        self._handle_status(status)

    value = property(fset=_set_value)

    def _spec_eq(self, other):
        try:
            return self._value == self._value_to_str(other)
        except:
            return

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

    def __iadd__(self, value):
        curvalue = self._value
        curvalue += self._value_to_str(value)
        self.value = curvalue
        return self


class _Container:
    def __bool__(self):
        return len(self) != 0

    def __delitem__(self, index):
        raise NotImplementedError


class ArrayValue(_Container, collections.abc.MutableSequence, _Value):
    _NAME = 'Array'

    def __init__(self, value=None):
        ptr = native_bt.value_array_create()
        self._check_create_status(ptr)
        super().__init__(ptr)

        # Python will raise a TypeError if there's anything wrong with
        # the iterable protocol.
        if value is not None:
            for elem in value:
                self.append(elem)

    def _spec_eq(self, other):
        try:
            if len(self) != len(other):
                # early mismatch
                return False

            for self_elem, other_elem in zip(self, other):
                if self_elem != other_elem:
                    return False

            return True
        except:
            return

    def __len__(self):
        size = native_bt.value_array_get_size(self._ptr)
        assert(size >= 0)
        return size

    def _check_index(self, index):
        # TODO: support slices also
        if not isinstance(index, numbers.Integral):
            raise TypeError("'{}' object is not an integral number object: invalid index".format(index.__class__.__name__))

        index = int(index)

        if index < 0 or index >= len(self):
            raise IndexError('array value object index is out of range')

    def __getitem__(self, index):
        self._check_index(index)
        ptr = native_bt.value_array_borrow_element_by_index(self._ptr, index)
        assert(ptr)
        return _create_from_ptr_and_get_ref(ptr)

    def __setitem__(self, index, value):
        self._check_index(index)
        value = create_value(value)

        if value is None:
            ptr = native_bt.value_null
        else:
            ptr = value._ptr

        status = native_bt.value_array_set_element_by_index(
            self._ptr, index, ptr)
        self._handle_status(status)

    def append(self, value):
        value = create_value(value)

        if value is None:
            ptr = native_bt.value_null
        else:
            ptr = value._ptr

        status = native_bt.value_array_append_element(self._ptr, ptr)
        self._handle_status(status)

    def __iadd__(self, iterable):
        # Python will raise a TypeError if there's anything wrong with
        # the iterable protocol.
        for elem in iterable:
            self.append(elem)

        return self

    def __repr__(self):
        return '[{}]'.format(', '.join([repr(v) for v in self]))

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


class MapValue(_Container, collections.abc.MutableMapping, _Value):
    _NAME = 'Map'

    def __init__(self, value=None):
        ptr = native_bt.value_map_create()
        self._check_create_status(ptr)
        super().__init__(ptr)

        # Python will raise a TypeError if there's anything wrong with
        # the iterable/mapping protocol.
        if value is not None:
            for key, elem in value.items():
                self[key] = elem

    def __eq__(self, other):
        return _Value.__eq__(self, other)

    def __ne__(self, other):
        return _Value.__ne__(self, other)

    def _spec_eq(self, other):
        try:
            if len(self) != len(other):
                # early mismatch
                return False

            for self_key in self:
                if self_key not in other:
                    return False

                self_value = self[self_key]
                other_value = other[self_key]

                if self_value != other_value:
                    return False

            return True
        except:
            return

    def __len__(self):
        size = native_bt.value_map_get_size(self._ptr)
        assert(size >= 0)
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
        ptr = native_bt.value_map_borrow_entry_value(self._ptr, key)
        assert(ptr)
        return _create_from_ptr_and_get_ref(ptr)

    def __iter__(self):
        return _MapValueKeyIterator(self)

    def __setitem__(self, key, value):
        self._check_key_type(key)
        value = create_value(value)

        if value is None:
            ptr = native_bt.value_null
        else:
            ptr = value._ptr

        status = native_bt.value_map_insert_entry(self._ptr, key, ptr)
        self._handle_status(status)

    def __repr__(self):
        items = ['{}: {}'.format(repr(k), repr(v)) for k, v in self.items()]
        return '{{{}}}'.format(', '.join(items))


_TYPE_TO_OBJ = {
    native_bt.VALUE_TYPE_BOOL: BoolValue,
    native_bt.VALUE_TYPE_UNSIGNED_INTEGER: UnsignedIntegerValue,
    native_bt.VALUE_TYPE_SIGNED_INTEGER: SignedIntegerValue,
    native_bt.VALUE_TYPE_REAL: RealValue,
    native_bt.VALUE_TYPE_STRING: StringValue,
    native_bt.VALUE_TYPE_ARRAY: ArrayValue,
    native_bt.VALUE_TYPE_MAP: MapValue,
}
