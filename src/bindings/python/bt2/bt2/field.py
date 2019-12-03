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
from bt2 import field_class as bt2_field_class
import collections.abc
import functools
import numbers
import math


def _create_field_from_ptr_template(
    object_map, ptr, owner_ptr, owner_get_ref, owner_put_ref
):

    field_class_ptr = native_bt.field_borrow_class_const(ptr)
    typeid = native_bt.field_class_get_type(field_class_ptr)
    field = object_map[typeid]._create_from_ptr_and_get_ref(
        ptr, owner_ptr, owner_get_ref, owner_put_ref
    )
    return field


def _create_field_from_ptr(ptr, owner_ptr, owner_get_ref, owner_put_ref):
    return _create_field_from_ptr_template(
        _TYPE_ID_TO_OBJ, ptr, owner_ptr, owner_get_ref, owner_put_ref
    )


def _create_field_from_const_ptr(ptr, owner_ptr, owner_get_ref, owner_put_ref):
    return _create_field_from_ptr_template(
        _TYPE_ID_TO_CONST_OBJ, ptr, owner_ptr, owner_get_ref, owner_put_ref
    )


# Get the "effective" field of `field`.  If `field` is a variant, return
# the currently selected field.  If `field` is an option, return the
# content field.  If `field` is of any other type, return `field`
# directly.


def _get_leaf_field(field):
    if isinstance(field, _VariantFieldConst):
        return _get_leaf_field(field.selected_option)

    if isinstance(field, _OptionFieldConst):
        return _get_leaf_field(field.field)

    return field


class _FieldConst(object._UniqueObject):
    _create_field_from_ptr = staticmethod(_create_field_from_const_ptr)
    _create_field_class_from_ptr_and_get_ref = staticmethod(
        bt2_field_class._create_field_class_from_const_ptr_and_get_ref
    )
    _borrow_class_ptr = staticmethod(native_bt.field_borrow_class_const)

    def __eq__(self, other):
        other = _get_leaf_field(other)
        return self._spec_eq(other)

    @property
    def cls(self):
        field_class_ptr = self._borrow_class_ptr(self._ptr)
        assert field_class_ptr is not None
        return self._create_field_class_from_ptr_and_get_ref(field_class_ptr)

    def _repr(self):
        raise NotImplementedError

    def __repr__(self):
        return self._repr()


class _Field(_FieldConst):
    _create_field_from_ptr = staticmethod(_create_field_from_ptr)
    _create_field_class_from_ptr_and_get_ref = staticmethod(
        bt2_field_class._create_field_class_from_ptr_and_get_ref
    )
    _borrow_class_ptr = staticmethod(native_bt.field_borrow_class)


class _BitArrayFieldConst(_FieldConst):
    _NAME = 'Const bit array'

    @property
    def value_as_integer(self):
        return native_bt.field_bit_array_get_value_as_integer(self._ptr)

    def _spec_eq(self, other):
        if type(other) is not type(self):
            return False

        return self.value_as_integer == other.value_as_integer

    def _repr(self):
        return repr(self.value_as_integer)

    def __str__(self):
        return str(self.value_as_integer)

    def __len__(self):
        return self.cls.length


class _BitArrayField(_BitArrayFieldConst, _Field):
    _NAME = 'Bit array'

    def _value_as_integer(self, value):
        utils._check_uint64(value)
        native_bt.field_bit_array_set_value_as_integer(self._ptr, value)

    value_as_integer = property(
        fget=_BitArrayFieldConst.value_as_integer.fget, fset=_value_as_integer
    )


@functools.total_ordering
class _NumericFieldConst(_FieldConst):
    @staticmethod
    def _extract_value(other):
        if isinstance(other, _BoolFieldConst) or isinstance(other, bool):
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

    def _repr(self):
        return repr(self._value)

    def __lt__(self, other):
        if not isinstance(other, numbers.Number):
            raise TypeError(
                'unorderable types: {}() < {}()'.format(
                    self.__class__.__name__, other.__class__.__name__
                )
            )

        return self._value < self._extract_value(other)

    def _spec_eq(self, other):
        try:
            return self._value == self._extract_value(other)
        except Exception:
            return False

    def __hash__(self):
        return hash(self._value)

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


class _NumericField(_NumericFieldConst, _Field):
    def __hash__(self):
        # Non const field are not hashable as their value may be modified
        # without changing the underlying Python object.
        raise TypeError('unhashable type: \'{}\''.format(self._NAME))


class _IntegralFieldConst(_NumericFieldConst, numbers.Integral):
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


class _IntegralField(_IntegralFieldConst, _NumericField):
    pass


class _BoolFieldConst(_IntegralFieldConst, _FieldConst):
    _NAME = 'Const boolean'

    def __bool__(self):
        return self._value

    @classmethod
    def _value_to_bool(cls, value):
        if isinstance(value, _BoolFieldConst):
            value = value._value

        if not isinstance(value, bool):
            raise TypeError(
                "'{}' object is not a 'bool', '_BoolFieldConst', or '_BoolField' object".format(
                    value.__class__
                )
            )

        return value

    @property
    def _value(self):
        return bool(native_bt.field_bool_get_value(self._ptr))


class _BoolField(_BoolFieldConst, _IntegralField, _Field):
    _NAME = 'Boolean'

    def _set_value(self, value):
        value = self._value_to_bool(value)
        native_bt.field_bool_set_value(self._ptr, value)

    value = property(fset=_set_value)


class _IntegerFieldConst(_IntegralFieldConst, _FieldConst):
    pass


class _IntegerField(_IntegerFieldConst, _IntegralField, _Field):
    def _check_range(self, value):
        if not (value >= self._lower_bound and value <= self._upper_bound):
            raise ValueError(
                "Value {} is outside valid range [{}, {}]".format(
                    value, self._lower_bound, self._upper_bound
                )
            )


class _UnsignedIntegerFieldConst(_IntegerFieldConst, _FieldConst):
    _NAME = 'Const unsigned integer'

    @classmethod
    def _value_to_int(cls, value):
        if not isinstance(value, numbers.Integral):
            raise TypeError('expecting an integral number object')

        return int(value)

    @property
    def _value(self):
        return native_bt.field_integer_unsigned_get_value(self._ptr)


class _UnsignedIntegerField(_UnsignedIntegerFieldConst, _IntegerField, _Field):
    _NAME = 'Unsigned integer'

    def _set_value(self, value):
        value = self._value_to_int(value)

        self._check_range(value)

        native_bt.field_integer_unsigned_set_value(self._ptr, value)

    value = property(fset=_set_value)

    @property
    def _lower_bound(self):
        return 0

    @property
    def _upper_bound(self):
        return (2 ** self.cls.field_value_range) - 1


class _SignedIntegerFieldConst(_IntegerFieldConst, _FieldConst):
    _NAME = 'Const signed integer'

    @classmethod
    def _value_to_int(cls, value):
        if not isinstance(value, numbers.Integral):
            raise TypeError('expecting an integral number object')

        return int(value)

    @property
    def _value(self):
        return native_bt.field_integer_signed_get_value(self._ptr)


class _SignedIntegerField(_SignedIntegerFieldConst, _IntegerField, _Field):
    _NAME = 'Signed integer'

    def _set_value(self, value):
        value = self._value_to_int(value)

        self._check_range(value)

        native_bt.field_integer_signed_set_value(self._ptr, value)

    value = property(fset=_set_value)

    @property
    def _lower_bound(self):
        return -1 * (2 ** (self.cls.field_value_range - 1))

    @property
    def _upper_bound(self):
        return (2 ** (self.cls.field_value_range - 1)) - 1


class _RealFieldConst(_NumericFieldConst, numbers.Real):
    _NAME = 'Const real'

    @classmethod
    def _value_to_float(cls, value):
        if not isinstance(value, numbers.Real):
            raise TypeError("expecting a real number object")

        return float(value)


class _SinglePrecisionRealFieldConst(_RealFieldConst):
    _NAME = 'Const single-precision real'

    @property
    def _value(self):
        return native_bt.field_real_single_precision_get_value(self._ptr)


class _DoublePrecisionRealFieldConst(_RealFieldConst):
    _NAME = 'Const double-precision real'

    @property
    def _value(self):
        return native_bt.field_real_double_precision_get_value(self._ptr)


class _RealField(_RealFieldConst, _NumericField):
    _NAME = 'Real'


class _SinglePrecisionRealField(_SinglePrecisionRealFieldConst, _RealField):
    _NAME = 'Single-precision real'

    def _set_value(self, value):
        value = self._value_to_float(value)
        native_bt.field_real_single_precision_set_value(self._ptr, value)

    value = property(fset=_set_value)


class _DoublePrecisionRealField(_DoublePrecisionRealFieldConst, _RealField):
    _NAME = 'Double-precision real'

    def _set_value(self, value):
        value = self._value_to_float(value)
        native_bt.field_real_double_precision_set_value(self._ptr, value)

    value = property(fset=_set_value)


class _EnumerationFieldConst(_IntegerFieldConst):
    def _repr(self):
        return '{} ({})'.format(self._value, ', '.join(self.labels))

    @property
    def labels(self):
        status, labels = self._get_mapping_labels(self._ptr)
        utils._handle_func_status(status, "cannot get label for enumeration field")

        assert labels is not None
        return labels


class _EnumerationField(_EnumerationFieldConst, _IntegerField):
    pass


class _UnsignedEnumerationFieldConst(
    _EnumerationFieldConst, _UnsignedIntegerFieldConst
):
    _NAME = 'Const unsigned Enumeration'
    _get_mapping_labels = staticmethod(
        native_bt.field_enumeration_unsigned_get_mapping_labels
    )


class _UnsignedEnumerationField(
    _UnsignedEnumerationFieldConst, _EnumerationField, _UnsignedIntegerField
):
    _NAME = 'Unsigned enumeration'


class _SignedEnumerationFieldConst(_EnumerationFieldConst, _SignedIntegerFieldConst):
    _NAME = 'Const signed Enumeration'
    _get_mapping_labels = staticmethod(
        native_bt.field_enumeration_signed_get_mapping_labels
    )


class _SignedEnumerationField(
    _SignedEnumerationFieldConst, _EnumerationField, _SignedIntegerField
):
    _NAME = 'Signed enumeration'


@functools.total_ordering
class _StringFieldConst(_FieldConst):
    _NAME = 'Const string'

    @classmethod
    def _value_to_str(cls, value):
        if isinstance(value, _StringFieldConst):
            value = value._value

        if not isinstance(value, str):
            raise TypeError("expecting a 'str' object")

        return value

    @property
    def _value(self):
        return native_bt.field_string_get_value(self._ptr)

    def _spec_eq(self, other):
        try:
            return self._value == self._value_to_str(other)
        except Exception:
            return False

    def __lt__(self, other):
        return self._value < self._value_to_str(other)

    def __bool__(self):
        return bool(self._value)

    def __hash__(self):
        return hash(self._value)

    def _repr(self):
        return repr(self._value)

    def __str__(self):
        return str(self._value)

    def __getitem__(self, index):
        return self._value[index]

    def __len__(self):
        return native_bt.field_string_get_length(self._ptr)


class _StringField(_StringFieldConst, _Field):
    _NAME = 'String'

    def _set_value(self, value):
        value = self._value_to_str(value)
        native_bt.field_string_set_value(self._ptr, value)

    value = property(fset=_set_value)

    def __iadd__(self, value):
        value = self._value_to_str(value)
        status = native_bt.field_string_append(self._ptr, value)
        utils._handle_func_status(
            status, "cannot append to string field object's value"
        )
        return self

    def __hash__(self):
        # Non const field are not hashable as their value may be modified
        # without changing the underlying Python object.
        raise TypeError('unhashable type: \'{}\''.format(self._NAME))


class _ContainerFieldConst(_FieldConst):
    def __bool__(self):
        return len(self) != 0

    def _count(self):
        return len(self.cls)

    def __len__(self):
        count = self._count()
        assert count >= 0
        return count

    def __delitem__(self, index):
        raise NotImplementedError

    def __setitem__(self, index, value):
        raise TypeError(
            '\'{}\' object does not support item assignment'.format(self.__class__)
        )


class _ContainerField(_ContainerFieldConst, _Field):
    pass


class _StructureFieldConst(_ContainerFieldConst, collections.abc.Mapping):
    _NAME = 'Const structure'
    _borrow_member_field_ptr_by_index = staticmethod(
        native_bt.field_structure_borrow_member_field_by_index_const
    )
    _borrow_member_field_ptr_by_name = staticmethod(
        native_bt.field_structure_borrow_member_field_by_name_const
    )

    def _count(self):
        return len(self.cls)

    def __iter__(self):
        # same name iterator
        return iter(self.cls)

    def _spec_eq(self, other):
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

    def _repr(self):
        items = ['{}: {}'.format(repr(k), repr(v)) for k, v in self.items()]
        return '{{{}}}'.format(', '.join(items))

    def __getitem__(self, key):
        utils._check_str(key)
        field_ptr = self._borrow_member_field_ptr_by_name(self._ptr, key)

        if field_ptr is None:
            raise KeyError(key)

        return self._create_field_from_ptr(
            field_ptr, self._owner_ptr, self._owner_get_ref, self._owner_put_ref
        )

    def member_at_index(self, index):
        utils._check_uint64(index)

        if index >= len(self):
            raise IndexError
        field_ptr = self._borrow_member_field_ptr_by_index(self._ptr, index)
        assert field_ptr is not None
        return self._create_field_from_ptr(
            field_ptr, self._owner_ptr, self._owner_get_ref, self._owner_put_ref
        )


class _StructureField(
    _StructureFieldConst, _ContainerField, collections.abc.MutableMapping
):
    _NAME = 'Structure'
    _borrow_member_field_ptr_by_index = staticmethod(
        native_bt.field_structure_borrow_member_field_by_index
    )
    _borrow_member_field_ptr_by_name = staticmethod(
        native_bt.field_structure_borrow_member_field_by_name
    )

    def __setitem__(self, key, value):
        # raises if key is somehow invalid
        field = self[key]

        # the field's property does the appropriate conversion or raises
        # the appropriate exception
        field.value = value

    def _set_value(self, values):
        try:
            for key, value in values.items():
                self[key].value = value
        except Exception:
            raise

    value = property(fset=_set_value)


class _OptionFieldConst(_FieldConst):
    _NAME = 'Const option'
    _borrow_field_ptr = staticmethod(native_bt.field_option_borrow_field_const)

    @property
    def field(self):
        field_ptr = self._borrow_field_ptr(self._ptr)

        if field_ptr is None:
            return

        return self._create_field_from_ptr(
            field_ptr, self._owner_ptr, self._owner_get_ref, self._owner_put_ref
        )

    @property
    def has_field(self):
        return self.field is not None

    def _spec_eq(self, other):
        return _get_leaf_field(self) == other

    def __bool__(self):
        return self.has_field

    def __str__(self):
        return str(self.field)

    def _repr(self):
        return repr(self.field)


class _OptionField(_OptionFieldConst, _Field):
    _NAME = 'Option'
    _borrow_field_ptr = staticmethod(native_bt.field_option_borrow_field)

    def _has_field(self, value):
        utils._check_bool(value)
        native_bt.field_option_set_has_field(self._ptr, value)

    has_field = property(fget=_OptionFieldConst.has_field.fget, fset=_has_field)

    def _set_value(self, value):
        self.has_field = True
        field = self.field
        assert field is not None
        field.value = value

    value = property(fset=_set_value)


class _VariantFieldConst(_ContainerFieldConst, _FieldConst):
    _NAME = 'Const variant'
    _borrow_selected_option_field_ptr = staticmethod(
        native_bt.field_variant_borrow_selected_option_field_const
    )

    def _count(self):
        return len(self.cls)

    @property
    def selected_option_index(self):
        return native_bt.field_variant_get_selected_option_index(self._ptr)

    @property
    def selected_option(self):
        # TODO: Is there a way to check if the variant field has a selected_option,
        # so we can raise an exception instead of hitting a pre-condition check?
        # If there is something, that check should be added to selected_option_index too.
        field_ptr = self._borrow_selected_option_field_ptr(self._ptr)

        return self._create_field_from_ptr(
            field_ptr, self._owner_ptr, self._owner_get_ref, self._owner_put_ref
        )

    def _spec_eq(self, other):
        return _get_leaf_field(self) == other

    def __bool__(self):
        raise NotImplementedError

    def __str__(self):
        return str(self.selected_option)

    def _repr(self):
        return repr(self.selected_option)


class _VariantField(_VariantFieldConst, _ContainerField, _Field):
    _NAME = 'Variant'
    _borrow_selected_option_field_ptr = staticmethod(
        native_bt.field_variant_borrow_selected_option_field
    )

    def _selected_option_index(self, index):
        if index < 0 or index >= len(self):
            raise IndexError('{} field object index is out of range'.format(self._NAME))

        native_bt.field_variant_select_option_by_index(self._ptr, index)

    selected_option_index = property(
        fget=_VariantFieldConst.selected_option_index.fget, fset=_selected_option_index
    )

    def _set_value(self, value):
        self.selected_option.value = value

    value = property(fset=_set_value)


class _ArrayFieldConst(_ContainerFieldConst, _FieldConst, collections.abc.Sequence):
    _borrow_element_field_ptr_by_index = staticmethod(
        native_bt.field_array_borrow_element_field_by_index_const
    )

    def _get_length(self):
        return native_bt.field_array_get_length(self._ptr)

    length = property(fget=_get_length)

    def __getitem__(self, index):
        if not isinstance(index, numbers.Integral):
            raise TypeError(
                "'{}' is not an integral number object: invalid index".format(
                    index.__class__.__name__
                )
            )

        index = int(index)

        if index < 0 or index >= len(self):
            raise IndexError('{} field object index is out of range'.format(self._NAME))

        field_ptr = self._borrow_element_field_ptr_by_index(self._ptr, index)
        assert field_ptr
        return self._create_field_from_ptr(
            field_ptr, self._owner_ptr, self._owner_get_ref, self._owner_put_ref
        )

    def insert(self, index, value):
        raise NotImplementedError

    def _spec_eq(self, other):
        if not isinstance(other, collections.abc.Sequence):
            return False

        if len(self) != len(other):
            # early mismatch
            return False

        for self_elem, other_elem in zip(self, other):
            if self_elem != other_elem:
                return False

        return True

    def _repr(self):
        return '[{}]'.format(', '.join([repr(v) for v in self]))


class _ArrayField(
    _ArrayFieldConst, _ContainerField, _Field, collections.abc.MutableSequence
):
    _borrow_element_field_ptr_by_index = staticmethod(
        native_bt.field_array_borrow_element_field_by_index
    )

    def __setitem__(self, index, value):
        # raises if index is somehow invalid
        field = self[index]

        if not isinstance(field, (_NumericField, _StringField)):
            raise TypeError('can only set the value of a number or string field')

        # the field's property does the appropriate conversion or raises
        # the appropriate exception
        field.value = value


class _StaticArrayFieldConst(_ArrayFieldConst, _FieldConst):
    _NAME = 'Const static array'

    def _count(self):
        return native_bt.field_array_get_length(self._ptr)


class _StaticArrayField(_StaticArrayFieldConst, _ArrayField, _Field):
    _NAME = 'Static array'

    def _set_value(self, values):
        if len(self) != len(values):
            raise ValueError(
                'expected length of value ({}) and array field ({}) to match'.format(
                    len(values), len(self)
                )
            )

        for index, value in enumerate(values):
            if value is not None:
                self[index].value = value

    value = property(fset=_set_value)


class _DynamicArrayFieldConst(_ArrayFieldConst, _FieldConst):
    _NAME = 'Const dynamic array'

    def _count(self):
        return self.length


class _DynamicArrayField(_DynamicArrayFieldConst, _ArrayField, _Field):
    _NAME = 'Dynamic array'

    def _set_length(self, length):
        utils._check_uint64(length)
        status = native_bt.field_array_dynamic_set_length(self._ptr, length)
        utils._handle_func_status(status, "cannot set dynamic array length")

    length = property(fget=_ArrayField._get_length, fset=_set_length)

    def _set_value(self, values):
        if len(values) != self.length:
            self.length = len(values)

        for index, value in enumerate(values):
            if value is not None:
                self[index].value = value

    value = property(fset=_set_value)


_TYPE_ID_TO_CONST_OBJ = {
    native_bt.FIELD_CLASS_TYPE_BOOL: _BoolFieldConst,
    native_bt.FIELD_CLASS_TYPE_BIT_ARRAY: _BitArrayFieldConst,
    native_bt.FIELD_CLASS_TYPE_UNSIGNED_INTEGER: _UnsignedIntegerFieldConst,
    native_bt.FIELD_CLASS_TYPE_SIGNED_INTEGER: _SignedIntegerFieldConst,
    native_bt.FIELD_CLASS_TYPE_SINGLE_PRECISION_REAL: _SinglePrecisionRealFieldConst,
    native_bt.FIELD_CLASS_TYPE_DOUBLE_PRECISION_REAL: _DoublePrecisionRealFieldConst,
    native_bt.FIELD_CLASS_TYPE_UNSIGNED_ENUMERATION: _UnsignedEnumerationFieldConst,
    native_bt.FIELD_CLASS_TYPE_SIGNED_ENUMERATION: _SignedEnumerationFieldConst,
    native_bt.FIELD_CLASS_TYPE_STRING: _StringFieldConst,
    native_bt.FIELD_CLASS_TYPE_STRUCTURE: _StructureFieldConst,
    native_bt.FIELD_CLASS_TYPE_STATIC_ARRAY: _StaticArrayFieldConst,
    native_bt.FIELD_CLASS_TYPE_DYNAMIC_ARRAY_WITHOUT_LENGTH_FIELD: _DynamicArrayFieldConst,
    native_bt.FIELD_CLASS_TYPE_DYNAMIC_ARRAY_WITH_LENGTH_FIELD: _DynamicArrayFieldConst,
    native_bt.FIELD_CLASS_TYPE_OPTION_WITHOUT_SELECTOR_FIELD: _OptionFieldConst,
    native_bt.FIELD_CLASS_TYPE_OPTION_WITH_BOOL_SELECTOR_FIELD: _OptionFieldConst,
    native_bt.FIELD_CLASS_TYPE_OPTION_WITH_UNSIGNED_INTEGER_SELECTOR_FIELD: _OptionFieldConst,
    native_bt.FIELD_CLASS_TYPE_OPTION_WITH_SIGNED_INTEGER_SELECTOR_FIELD: _OptionFieldConst,
    native_bt.FIELD_CLASS_TYPE_VARIANT_WITHOUT_SELECTOR_FIELD: _VariantFieldConst,
    native_bt.FIELD_CLASS_TYPE_VARIANT_WITH_UNSIGNED_INTEGER_SELECTOR_FIELD: _VariantFieldConst,
    native_bt.FIELD_CLASS_TYPE_VARIANT_WITH_SIGNED_INTEGER_SELECTOR_FIELD: _VariantFieldConst,
}

_TYPE_ID_TO_OBJ = {
    native_bt.FIELD_CLASS_TYPE_BOOL: _BoolField,
    native_bt.FIELD_CLASS_TYPE_BIT_ARRAY: _BitArrayField,
    native_bt.FIELD_CLASS_TYPE_UNSIGNED_INTEGER: _UnsignedIntegerField,
    native_bt.FIELD_CLASS_TYPE_SIGNED_INTEGER: _SignedIntegerField,
    native_bt.FIELD_CLASS_TYPE_SINGLE_PRECISION_REAL: _SinglePrecisionRealField,
    native_bt.FIELD_CLASS_TYPE_DOUBLE_PRECISION_REAL: _DoublePrecisionRealField,
    native_bt.FIELD_CLASS_TYPE_UNSIGNED_ENUMERATION: _UnsignedEnumerationField,
    native_bt.FIELD_CLASS_TYPE_SIGNED_ENUMERATION: _SignedEnumerationField,
    native_bt.FIELD_CLASS_TYPE_STRING: _StringField,
    native_bt.FIELD_CLASS_TYPE_STRUCTURE: _StructureField,
    native_bt.FIELD_CLASS_TYPE_STATIC_ARRAY: _StaticArrayField,
    native_bt.FIELD_CLASS_TYPE_DYNAMIC_ARRAY_WITHOUT_LENGTH_FIELD: _DynamicArrayField,
    native_bt.FIELD_CLASS_TYPE_DYNAMIC_ARRAY_WITH_LENGTH_FIELD: _DynamicArrayField,
    native_bt.FIELD_CLASS_TYPE_OPTION_WITHOUT_SELECTOR_FIELD: _OptionField,
    native_bt.FIELD_CLASS_TYPE_OPTION_WITH_BOOL_SELECTOR_FIELD: _OptionField,
    native_bt.FIELD_CLASS_TYPE_OPTION_WITH_UNSIGNED_INTEGER_SELECTOR_FIELD: _OptionField,
    native_bt.FIELD_CLASS_TYPE_OPTION_WITH_SIGNED_INTEGER_SELECTOR_FIELD: _OptionField,
    native_bt.FIELD_CLASS_TYPE_VARIANT_WITHOUT_SELECTOR_FIELD: _VariantField,
    native_bt.FIELD_CLASS_TYPE_VARIANT_WITH_UNSIGNED_INTEGER_SELECTOR_FIELD: _VariantField,
    native_bt.FIELD_CLASS_TYPE_VARIANT_WITH_SIGNED_INTEGER_SELECTOR_FIELD: _VariantField,
}
