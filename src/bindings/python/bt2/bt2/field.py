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
import bt2.field_class
import collections.abc
import functools
import numbers
import math
import bt2


def _create_field_from_ptr(ptr, owner_ptr, owner_get_ref, owner_put_ref):
    field_class_ptr = native_bt.field_borrow_class_const(ptr)
    utils._handle_ptr(field_class_ptr, "cannot get field object's class")
    typeid = native_bt.field_class_get_type(field_class_ptr)
    field = _TYPE_ID_TO_OBJ[typeid]._create_from_ptr_and_get_ref(
        ptr, owner_ptr, owner_get_ref, owner_put_ref)
    return field


# Get the "effective" field of `field`.  If `field` is a variant, return the
# currently selected field.  If `field` is of any other type, return `field`
# directly.

def _get_leaf_field(field):
    if not isinstance(field, _VariantField):
        return field

    return _get_leaf_field(field.selected_option)


class _Field(object._UniqueObject):
    def __eq__(self, other):
        other = _get_leaf_field(other)
        return self._spec_eq(other)

    @property
    def field_class(self):
        field_class_ptr = native_bt.field_borrow_class_const(self._ptr)
        assert field_class_ptr is not None
        return bt2.field_class._create_field_class_from_ptr_and_get_ref(field_class_ptr)

    def _repr(self):
        raise NotImplementedError

    def __repr__(self):
        return self._repr()


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
        return int(self._value)

    def __float__(self):
        return float(self._value)

    def _repr(self):
        return repr(self._value)

    def __lt__(self, other):
        if not isinstance(other, numbers.Number):
            raise TypeError('unorderable types: {}() < {}()'.format(self.__class__.__name__,
                                                                    other.__class__.__name__))

        return self._value < float(other)

    def _spec_eq(self, other):
        if not isinstance(other, numbers.Number):
            return NotImplemented

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


class _IntegralField(_NumericField, numbers.Integral):
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


class _IntegerField(_IntegralField, _Field):
    pass


class _UnsignedIntegerField(_IntegerField, _Field):
    _NAME = 'Unsigned integer'

    def _value_to_int(self, value):
        if not isinstance(value, numbers.Real):
            raise TypeError('expecting a real number object')

        value = int(value)
        utils._check_uint64(value)

        return value

    @property
    def _value(self):
        return native_bt.field_unsigned_integer_get_value(self._ptr)

    def _set_value(self, value):
        value = self._value_to_int(value)
        native_bt.field_unsigned_integer_set_value(self._ptr, value)

    value = property(fset=_set_value)


class _SignedIntegerField(_IntegerField, _Field):
    _NAME = 'Signed integer'

    def _value_to_int(self, value):
        if not isinstance(value, numbers.Real):
            raise TypeError('expecting a real number object')

        value = int(value)
        utils._check_int64(value)

        return value

    @property
    def _value(self):
        return native_bt.field_signed_integer_get_value(self._ptr)

    def _set_value(self, value):
        value = self._value_to_int(value)
        native_bt.field_signed_integer_set_value(self._ptr, value)

    value = property(fset=_set_value)


class _RealField(_NumericField, numbers.Real):
    _NAME = 'Real'

    def _value_to_float(self, value):
        if not isinstance(value, numbers.Real):
            raise TypeError("expecting a real number object")

        return float(value)

    @property
    def _value(self):
        return native_bt.field_real_get_value(self._ptr)

    def _set_value(self, value):
        value = self._value_to_float(value)
        native_bt.field_real_set_value(self._ptr, value)

    value = property(fset=_set_value)


class _EnumerationField(_IntegerField):
    def _repr(self):
        return '{} ({})'.format(self._value, ', '.join(self.labels))

    @property
    def labels(self):
        ret, labels = self._get_mapping_labels(self._ptr)
        utils._handle_ret(ret, "cannot get label for enumeration field")

        assert labels is not None
        return labels


class _UnsignedEnumerationField(_EnumerationField, _UnsignedIntegerField):
    _NAME = 'Unsigned Enumeration'
    _get_mapping_labels = staticmethod(native_bt.field_unsigned_enumeration_get_mapping_labels)


class _SignedEnumerationField(_EnumerationField, _SignedIntegerField):
    _NAME = 'Signed Enumeration'
    _get_mapping_labels = staticmethod(native_bt.field_signed_enumeration_get_mapping_labels)


@functools.total_ordering
class _StringField(_Field):
    _NAME = 'String'

    def _value_to_str(self, value):
        if isinstance(value, self.__class__):
            value = value._value

        if not isinstance(value, str):
            raise TypeError("expecting a 'str' object")

        return value

    @property
    def _value(self):
        return native_bt.field_string_get_value(self._ptr)

    def _set_value(self, value):
        value = self._value_to_str(value)
        native_bt.field_string_set_value(self._ptr, value)

    value = property(fset=_set_value)

    def _spec_eq(self, other):
        try:
            other = self._value_to_str(other)
        except Exception:
            return False

        return self._value == other

    def __lt__(self, other):
        return self._value < self._value_to_str(other)

    def __bool__(self):
        return bool(self._value)

    def _repr(self):
        return repr(self._value)

    def __str__(self):
        return str(self._value)

    def __getitem__(self, index):
        return self._value[index]

    def __len__(self):
        return native_bt.field_string_get_length(self._ptr)

    def __iadd__(self, value):
        value = self._value_to_str(value)
        ret = native_bt.field_string_append(self._ptr, value)
        utils._handle_ret(ret, "cannot append to string field object's value")
        return self


class _ContainerField(_Field):
    def __bool__(self):
        return len(self) != 0

    def __len__(self):
        count = self._count()
        assert count >= 0
        return count

    def __delitem__(self, index):
        raise NotImplementedError


class _StructureField(_ContainerField, collections.abc.MutableMapping):
    _NAME = 'Structure'

    def _count(self):
        return len(self.field_class)

    def __setitem__(self, key, value):
        # raises if key is somehow invalid
        field = self[key]

        # the field's property does the appropriate conversion or raises
        # the appropriate exception
        field.value = value

    def __iter__(self):
        # same name iterator
        return iter(self.field_class)

    def _spec_eq(self, other):
        try:
            if len(self) != len(other):
                return False

            for self_key, self_value in self.items():
                if self_key not in other:
                    return False

                other_value = other[self_key]

                if self_value != other_value:
                    return False

            return True
        except Exception:
            return False

    def _set_value(self, values):
        try:
            for key, value in values.items():
                self[key].value = value
        except Exception:
            raise

    value = property(fset=_set_value)

    def _repr(self):
        items = ['{}: {}'.format(repr(k), repr(v)) for k, v in self.items()]
        return '{{{}}}'.format(', '.join(items))

    def __getitem__(self, key):
        utils._check_str(key)
        field_ptr = native_bt.field_structure_borrow_member_field_by_name(self._ptr, key)

        if field_ptr is None:
            raise KeyError(key)

        return _create_field_from_ptr(field_ptr, self._owner_ptr,
                                      self._owner_get_ref,
                                      self._owner_put_ref)

    def member_at_index(self, index):
        utils._check_uint64(index)

        if index >= len(self):
            raise IndexError

        field_ptr = native_bt.field_structure_borrow_member_field_by_index(self._ptr, index)
        assert field_ptr is not None
        return _create_field_from_ptr(field_ptr, self._owner_ptr,
                                      self._owner_get_ref,
                                      self._owner_put_ref)


class _VariantField(_ContainerField, _Field):
    _NAME = 'Variant'

    @property
    def selected_option_index(self):
        return native_bt.field_variant_get_selected_option_field_index(self._ptr)

    @selected_option_index.setter
    def selected_option_index(self, index):
        native_bt.field_variant_select_option_field(self._ptr, index)

    @property
    def selected_option(self):
        field_ptr = native_bt.field_variant_borrow_selected_option_field(self._ptr)
        utils._handle_ptr(field_ptr, "cannot get variant field's selected option")

        return _create_field_from_ptr(field_ptr, self._owner_ptr,
                                      self._owner_get_ref,
                                      self._owner_put_ref)

    def _spec_eq(self, other):
        new_self = _get_leaf_field(self)
        return new_self == other

    def __bool__(self):
        raise NotImplementedError

    def __str__(self):
        return str(self.selected_option)

    def _repr(self):
        return repr(self.selected_option)

    def _set_value(self, value):
        self.selected_option.value = value

    value = property(fset=_set_value)


class _ArrayField(_ContainerField, _Field):

    def _get_length(self):
        return native_bt.field_array_get_length(self._ptr)

    length = property(fget=_get_length)

    def __getitem__(self, index):
        if not isinstance(index, numbers.Integral):
            raise TypeError("'{}' is not an integral number object: invalid index".format(index.__class__.__name__))

        index = int(index)

        if index < 0 or index >= len(self):
            raise IndexError('{} field object index is out of range'.format(self._NAME))

        field_ptr = native_bt.field_array_borrow_element_field_by_index(self._ptr, index)
        assert(field_ptr)
        return _create_field_from_ptr(field_ptr, self._owner_ptr,
                                      self._owner_get_ref,
                                      self._owner_put_ref)

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

    def _spec_eq(self, other):
        try:
            if len(self) != len(other):
                return False

            for self_field, other_field in zip(self, other):
                if self_field != other_field:
                    return False

            return True
        except Exception:
            return False

    def _repr(self):
        return '[{}]'.format(', '.join([repr(v) for v in self]))


class _StaticArrayField(_ArrayField, _Field):
    _NAME = 'Static array'

    def _count(self):
        return native_bt.field_array_get_length(self._ptr)

    def _set_value(self, values):
        if len(self) != len(values):
            raise ValueError(
                'expected length of value and array field to match')

        for index, value in enumerate(values):
            if value is not None:
                self[index].value = value

    value = property(fset=_set_value)


class _DynamicArrayField(_ArrayField, _Field):
    _NAME = 'Dynamic array'

    def _count(self):
        return self.length

    def _set_length(self, length):
        utils._check_uint64(length)
        ret = native_bt.field_dynamic_array_set_length(self._ptr, length)
        utils._handle_ret(ret, "cannot set dynamic array length")

    length = property(fget=_ArrayField._get_length, fset=_set_length)

    def _set_value(self, values):
        if len(values) != self.length:
            self.length = len(values)

        for index, value in enumerate(values):
            if value is not None:
                self[index].value = value

    value = property(fset=_set_value)


_TYPE_ID_TO_OBJ = {
    native_bt.FIELD_CLASS_TYPE_UNSIGNED_INTEGER: _UnsignedIntegerField,
    native_bt.FIELD_CLASS_TYPE_SIGNED_INTEGER: _SignedIntegerField,
    native_bt.FIELD_CLASS_TYPE_REAL: _RealField,
    native_bt.FIELD_CLASS_TYPE_UNSIGNED_ENUMERATION: _UnsignedEnumerationField,
    native_bt.FIELD_CLASS_TYPE_SIGNED_ENUMERATION: _SignedEnumerationField,
    native_bt.FIELD_CLASS_TYPE_STRING: _StringField,
    native_bt.FIELD_CLASS_TYPE_STRUCTURE: _StructureField,
    native_bt.FIELD_CLASS_TYPE_STATIC_ARRAY: _StaticArrayField,
    native_bt.FIELD_CLASS_TYPE_DYNAMIC_ARRAY: _DynamicArrayField,
    native_bt.FIELD_CLASS_TYPE_VARIANT: _VariantField,
}
