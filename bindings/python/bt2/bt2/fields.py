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
import bt2.field_types
import collections.abc
import functools
import numbers
import math
import abc
import bt2


def _create_from_ptr(ptr):
    # recreate the field type wrapper of this field's type (the identity
    # could be different, but the underlying address should be the
    # same)
    field_type_ptr = native_bt.ctf_field_get_type(ptr)
    utils._handle_ptr(field_type_ptr, "cannot get field object's type")
    field_type = bt2.field_types._create_from_ptr(field_type_ptr)
    typeid = native_bt.ctf_field_type_get_type_id(field_type._ptr)
    field = _TYPE_ID_TO_OBJ[typeid]._create_from_ptr(ptr)
    field._field_type = field_type
    return field


class _Field(object._Object, metaclass=abc.ABCMeta):
    def __copy__(self):
        ptr = native_bt.ctf_field_copy(self._ptr)
        utils._handle_ptr(ptr, 'cannot copy {} field object'.format(self._NAME.lower()))
        return _create_from_ptr(ptr)

    def __deepcopy__(self, memo):
        cpy = self.__copy__()
        memo[id(self)] = cpy
        return cpy

    @property
    def field_type(self):
        return self._field_type


@functools.total_ordering
class _NumericField(_Field):
    @staticmethod
    def _extract_value(other):
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
        return int(self.value)

    def __float__(self):
        return float(self.value)

    def __str__(self):
        return str(self.value)

    def __lt__(self, other):
        if not isinstance(other, numbers.Number):
            raise TypeError('unorderable types: {}() < {}()'.format(self.__class__.__name__,
                                                                    other.__class__.__name__))

        return self.value < float(other)

    def __le__(self, other):
        if not isinstance(other, numbers.Number):
            raise TypeError('unorderable types: {}() <= {}()'.format(self.__class__.__name__,
                                                                     other.__class__.__name__))

        return self.value <= float(other)

    def __eq__(self, other):
        if not isinstance(other, numbers.Number):
            return False

        return self.value == complex(other)

    def __rmod__(self, other):
        return self._extract_value(other) % self.value

    def __mod__(self, other):
        return self.value % self._extract_value(other)

    def __rfloordiv__(self, other):
        return self._extract_value(other) // self.value

    def __floordiv__(self, other):
        return self.value // self._extract_value(other)

    def __round__(self, ndigits=None):
        if ndigits is None:
            return round(self.value)
        else:
            return round(self.value, ndigits)

    def __ceil__(self):
        return math.ceil(self.value)

    def __floor__(self):
        return math.floor(self.value)

    def __trunc__(self):
        return int(self.value)

    def __abs__(self):
        return abs(self.value)

    def __add__(self, other):
        return self.value + self._extract_value(other)

    def __radd__(self, other):
        return self.__add__(other)

    def __neg__(self):
        return -self.value

    def __pos__(self):
        return +self.value

    def __mul__(self, other):
        return self.value * self._extract_value(other)

    def __rmul__(self, other):
        return self.__mul__(other)

    def __truediv__(self, other):
        return self.value / self._extract_value(other)

    def __rtruediv__(self, other):
        return self._extract_value(other) / self.value

    def __pow__(self, exponent):
        return self.value ** self._extract_value(exponent)

    def __rpow__(self, base):
        return self._extract_value(base) ** self.value

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


class _IntegralField(_NumericField, numbers.Integral):
    def __lshift__(self, other):
        return self.value << self._extract_value(other)

    def __rlshift__(self, other):
        return self._extract_value(other) << self.value

    def __rshift__(self, other):
        return self.value >> self._extract_value(other)

    def __rrshift__(self, other):
        return self._extract_value(other) >> self.value

    def __and__(self, other):
        return self.value & self._extract_value(other)

    def __rand__(self, other):
        return self._extract_value(other) & self.value

    def __xor__(self, other):
        return self.value ^ self._extract_value(other)

    def __rxor__(self, other):
        return self._extract_value(other) ^ self.value

    def __or__(self, other):
        return self.value | self._extract_value(other)

    def __ror__(self, other):
        return self._extract_value(other) | self.value

    def __invert__(self):
        return ~self.value

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


class _RealField(_NumericField, numbers.Real):
    pass


class _IntegerField(_IntegralField):
    _NAME = 'Integer'

    def _value_to_int(self, value):
        if not isinstance(value, numbers.Real):
            raise TypeError('expecting a real number object')

        value = int(value)

        if self.field_type.is_signed:
            utils._check_int64(value)
        else:
            utils._check_uint64(value)

        return value

    @property
    def value(self):
        if self.field_type.is_signed:
            ret, value = native_bt.ctf_field_signed_integer_get_value(self._ptr)
        else:
            ret, value = native_bt.ctf_field_unsigned_integer_get_value(self._ptr)

        if ret < 0:
            # field is not set
            return

        return value

    @value.setter
    def value(self, value):
        value = self._value_to_int(value)

        if self.field_type.is_signed:
            ret = native_bt.ctf_field_signed_integer_set_value(self._ptr, value)
        else:
            ret = native_bt.ctf_field_unsigned_integer_set_value(self._ptr, value)

        utils._handle_ret(ret, "cannot set integer field object's value")


class _FloatingPointNumberField(_RealField):
    _NAME = 'Floating point number'

    def _value_to_float(self, value):
        if not isinstance(value, numbers.Real):
            raise TypeError("expecting a real number object")

        return float(value)

    @property
    def value(self):
        ret, value = native_bt.ctf_field_floating_point_get_value(self._ptr)

        if ret < 0:
            # field is not set
            return

        return value

    @value.setter
    def value(self, value):
        value = self._value_to_float(value)
        ret = native_bt.ctf_field_floating_point_set_value(self._ptr, value)
        utils._handle_ret(ret, "cannot set floating point number field object's value")


class _EnumerationField(_IntegerField):
    _NAME = 'Enumeration'

    @property
    def integer_field(self):
        int_field_ptr = native_bt.ctf_field_enumeration_get_container(self._ptr)
        assert(int_field_ptr)
        return _create_from_ptr(int_field_ptr)

    @property
    def value(self):
        return self.integer_field.value

    @value.setter
    def value(self, value):
        self.integer_field.value = value

    @property
    def mappings(self):
        iter_ptr = native_bt.ctf_field_enumeration_get_mappings(self._ptr)
        assert(iter_ptr)
        return bt2.field_types._EnumerationFieldTypeMappingIterator(iter_ptr,
                                                                    self.field_type.is_signed)


@functools.total_ordering
class _StringField(_Field, collections.abc.Sequence):
    _NAME = 'String'

    def _value_to_str(self, value):
        if isinstance(value, self.__class__):
            value = value.value

        if not isinstance(value, str):
            raise TypeError("expecting a 'str' object")

        return value

    @property
    def value(self):
        value = native_bt.ctf_field_string_get_value(self._ptr)

        if value is None:
            # field is not set
            return

        return value

    @value.setter
    def value(self, value):
        value = self._value_to_str(value)
        ret = native_bt.ctf_field_string_set_value(self._ptr, value)
        utils._handle_ret(ret, "cannot set string field object's value")

    def __eq__(self, other):
        try:
            other = self._value_to_str(other)
        except:
            return False

        return self.value == other

    def __le__(self, other):
        return self.value <= self._value_to_str(other)

    def __lt__(self, other):
        return self.value < self._value_to_str(other)

    def __bool__(self):
        return bool(self.value)

    def __str__(self):
        return self.value

    def __getitem__(self, index):
        return self.value[index]

    def __len__(self):
        return len(self.value)

    def __iadd__(self, value):
        value = self._value_to_str(value)
        ret = native_bt.ctf_field_string_append(self._ptr, value)
        utils._handle_ret(ret, "cannot append to string field object's value")
        return self


class _ContainerField(_Field):
    def __bool__(self):
        return len(self) != 0

    def __len__(self):
        count = self._count()
        assert(count >= 0)
        return count

    def __delitem__(self, index):
        raise NotImplementedError


class _StructureField(_ContainerField, collections.abc.MutableMapping):
    _NAME = 'Structure'

    def _count(self):
        return len(self.field_type)

    def __getitem__(self, key):
        utils._check_str(key)
        ptr = native_bt.ctf_field_structure_get_field_by_name(self._ptr, key)

        if ptr is None:
            raise KeyError(key)

        return _create_from_ptr(ptr)

    def __setitem__(self, key, value):
        # we can only set numbers and strings
        if not isinstance(value, (numbers.Number, str)):
            raise TypeError('expecting number object or string')

        # raises if index is somehow invalid
        field = self[key]

        if not isinstance(field, (_NumericField, _StringField)):
            raise TypeError('can only set the value of a number or string field')

        # the field's property does the appropriate conversion or raises
        # the appropriate exception
        field.value = value

    def at_index(self, index):
        utils._check_uint64(index)

        if index >= len(self):
            raise IndexError

        field_ptr = native_bt.ctf_field_structure_get_field_by_index(self._ptr, index)
        assert(field_ptr)
        return _create_from_ptr(field_ptr)

    def __iter__(self):
        # same name iterator
        return iter(self.field_type)

    def __eq__(self, other):
        if not isinstance(other, collections.abc.Mapping):
            return False

        if len(self) != len(other):
            return False

        for self_key, self_value in self.items():
            if self_key not in other:
                return False

            other_value = other[self_key]

            if self_value != other_value:
                return False

        return True

    @property
    def value(self):
        return {key: field.value for key, field in self.items()}

    @value.setter
    def value(self, values):
        if not hasattr(type(values), '__getitem__'):
            raise TypeError('expecting a Mapping collection')

        for key, value in values.items():
            self[key].value = value


class _VariantField(_Field):
    _NAME = 'Variant'

    @property
    def tag_field(self):
        field_ptr = native_bt.ctf_field_variant_get_tag(self._ptr)

        if field_ptr is None:
            return

        return _create_from_ptr(field_ptr)

    @property
    def selected_field(self):
        return self.field()

    def field(self, tag_field=None):
        if tag_field is None:
            field_ptr = native_bt.ctf_field_variant_get_current_field(self._ptr)

            if field_ptr is None:
                return
        else:
            utils._check_type(tag_field, _EnumerationField)
            field_ptr = native_bt.ctf_field_variant_get_field(self._ptr, tag_field._ptr)
            utils._handle_ptr(field_ptr, "cannot select variant field object's field")

        return _create_from_ptr(field_ptr)

    def __eq__(self, other):
        if type(other) is not type(self):
            return False

        if self.addr == other.addr:
            return True

        return self.selected_field == other.selected_field

    def __bool__(self):
        return bool(self.selected_field)


class _ArraySequenceField(_ContainerField, collections.abc.MutableSequence):
    def __getitem__(self, index):
        if not isinstance(index, numbers.Integral):
            raise TypeError("'{}' is not an integral number object: invalid index".format(index.__class__.__name__))

        index = int(index)

        if index < 0 or index >= len(self):
            raise IndexError('{} field object index is out of range'.format(self._NAME))

        field_ptr = self._get_field_ptr_at_index(index)
        assert(field_ptr)
        return _create_from_ptr(field_ptr)

    def __setitem__(self, index, value):
        # we can only set numbers and strings
        if not isinstance(value, (numbers.Number, _StringField, str)):
            raise TypeError('expecting number or string object')

        # raises if index is somehow invalid
        field = self[index]

        if not isinstance(field, (_NumericField, _StringField)):
            raise TypeError('can only set the value of a number or string field')

        # the field's property does the appropriate conversion or raises
        # the appropriate exception
        field.value = value

    def insert(self, index, value):
        raise NotImplementedError

    def __eq__(self, other):
        if not isinstance(other, collections.abc.Sequence):
            return False

        if len(self) != len(other):
            return False

        for self_field, other_field in zip(self, other):
            if self_field != other_field:
                return False

        return True

    @property
    def value(self):
        return [field.value for field in self]

    @value.setter
    def value(self, values):
        if not hasattr(type(values), '__iter__'):
            raise TypeError('expecting an iterable container (Sequence)')

        if len(self) != len(values):
            raise ValueError('expected length of value and field to match')

        for index, value in enumerate(values):
            self[index].value = value


class _ArrayField(_ArraySequenceField):
    _NAME = 'Array'

    def _count(self):
        return self.field_type.length

    def _get_field_ptr_at_index(self, index):
        return native_bt.ctf_field_array_get_field(self._ptr, index)


class _SequenceField(_ArraySequenceField):
    _NAME = 'Sequence'

    def _count(self):
        return self.length_field.value

    @property
    def length_field(self):
        field_ptr = native_bt.ctf_field_sequence_get_length(self._ptr)
        utils._handle_ptr("cannot get sequence field object's length field")
        return _create_from_ptr(field_ptr)

    @length_field.setter
    def length_field(self, length_field):
        utils._check_type(length_field, _IntegerField)
        ret = native_bt.ctf_field_sequence_set_length(self._ptr, length_field._ptr)
        utils._handle_ret(ret, "cannot set sequence field object's length field")

    def _get_field_ptr_at_index(self, index):
        return native_bt.ctf_field_sequence_get_field(self._ptr, index)


_TYPE_ID_TO_OBJ = {
    native_bt.CTF_FIELD_TYPE_ID_INTEGER: _IntegerField,
    native_bt.CTF_FIELD_TYPE_ID_FLOAT: _FloatingPointNumberField,
    native_bt.CTF_FIELD_TYPE_ID_ENUM: _EnumerationField,
    native_bt.CTF_FIELD_TYPE_ID_STRING: _StringField,
    native_bt.CTF_FIELD_TYPE_ID_STRUCT: _StructureField,
    native_bt.CTF_FIELD_TYPE_ID_ARRAY: _ArrayField,
    native_bt.CTF_FIELD_TYPE_ID_SEQUENCE: _SequenceField,
    native_bt.CTF_FIELD_TYPE_ID_VARIANT: _VariantField,
}
