# SPDX-License-Identifier: MIT
#
# Copyright (c) 2017 Philippe Proulx <pproulx@efficios.com>

import math
import typing
import numbers
import functools
import collections.abc

from bt2 import utils as bt2_utils
from bt2 import object as bt2_object
from bt2 import native_bt
from bt2 import field_class as bt2_field_class


def _create_field_from_ptr_template(
    object_map, ptr, owner_ptr, owner_get_ref, owner_put_ref
):
    return object_map[
        native_bt.field_class_get_type(native_bt.field_borrow_class_const(ptr))
    ]._create_from_ptr_and_get_ref(ptr, owner_ptr, owner_get_ref, owner_put_ref)


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


class _FieldConst(bt2_object._UniqueObject):
    _create_field_from_ptr = staticmethod(_create_field_from_const_ptr)
    _create_field_class_from_ptr_and_get_ref = staticmethod(
        bt2_field_class._create_field_class_from_const_ptr_and_get_ref
    )
    _borrow_class_ptr = staticmethod(native_bt.field_borrow_class_const)

    @property
    def graph_mip_version(self):
        return self.cls.graph_mip_version

    def __eq__(self, other: object) -> bool:
        return self._spec_eq(_get_leaf_field(other))

    @property
    def _cls(self):
        return self._create_field_class_from_ptr_and_get_ref(
            self._borrow_class_ptr(self._ptr)
        )

    @property
    def cls(self) -> bt2_field_class._FieldClassConst:
        return self._cls

    def _repr(self):
        raise NotImplementedError

    def __repr__(self) -> str:
        return self._repr()


class _Field(_FieldConst):
    _create_field_from_ptr = staticmethod(_create_field_from_ptr)
    _create_field_class_from_ptr_and_get_ref = staticmethod(
        bt2_field_class._create_field_class_from_ptr_and_get_ref
    )
    _borrow_class_ptr = staticmethod(native_bt.field_borrow_class)

    @property
    def cls(self) -> bt2_field_class._FieldClass:
        return self._cls


class _BitArrayFieldConst(_FieldConst):
    _NAME = "Const bit array"

    @property
    def cls(self) -> bt2_field_class._BitArrayFieldClassConst:
        return self._cls

    @property
    def value_as_integer(self) -> int:
        return native_bt.field_bit_array_get_value_as_integer(self._ptr)

    @property
    def active_flag_labels(self) -> typing.List[str]:
        bt2_utils._check_mip_ge(self, "Bit array field class flags", 1)
        return native_bt.field_bit_array_get_active_flag_labels(self._ptr)

    def _spec_eq(self, other):
        if type(other) is not type(self):
            return False

        return self.value_as_integer == other.value_as_integer

    def _repr(self):
        return repr(self.value_as_integer)

    def __str__(self) -> str:
        return str(self.value_as_integer)

    def __len__(self) -> int:
        return self.cls.length


class _BitArrayField(_BitArrayFieldConst, _Field):
    _NAME = "Bit array"

    @property
    def cls(self) -> bt2_field_class._BitArrayFieldClass:
        return self._cls

    @_BitArrayFieldConst.value_as_integer.setter
    def value_as_integer(self, value):
        bt2_utils._check_uint64(value)
        native_bt.field_bit_array_set_value_as_integer(self._ptr, value)


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

        raise TypeError(f"'{other.__class__.__name__}' object is not a number object")

    def __int__(self) -> int:
        return int(self._value)

    def __float__(self) -> float:
        return float(self._value)

    def _repr(self):
        return repr(self._value)

    def __lt__(self, other) -> bool:
        if not isinstance(other, numbers.Number):
            raise TypeError(
                f"unorderable types: {self.__class__.__name__}() < {other.__class__.__name__}()"
            )

        return self._value < self._extract_value(other)

    def _spec_eq(self, other):
        try:
            return self._value == self._extract_value(other)
        except Exception:
            return False

    def __hash__(self) -> int:
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
    def __hash__(self) -> int:
        # Non const field are not hashable as their value may be modified
        # without changing the underlying Python object.
        raise TypeError(f"unhashable type: '{self._NAME}'")


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
    _NAME = "Const boolean"

    @property
    def cls(self) -> bt2_field_class._BoolFieldClassConst:
        return self._cls

    def __bool__(self) -> bool:
        return self._value

    @classmethod
    def _value_to_bool(cls, value):
        if isinstance(value, _BoolFieldConst):
            value = value._value

        if not isinstance(value, bool):
            raise TypeError(
                f"'{value.__class__}' object is not a 'bool', '_BoolFieldConst', or '_BoolField' object"
            )

        return value

    @property
    def _value(self):
        return bool(native_bt.field_bool_get_value(self._ptr))


class _BoolField(_BoolFieldConst, _IntegralField, _Field):
    _NAME = "Boolean"

    @property
    def cls(self) -> bt2_field_class._BoolFieldClass:
        return self._cls

    def _set_value(self, value):
        native_bt.field_bool_set_value(self._ptr, self._value_to_bool(value))

    value = property(fset=_set_value)


class _IntegerFieldConst(_IntegralFieldConst, _FieldConst):
    @property
    def cls(self) -> bt2_field_class._IntegerFieldClassConst:
        return self._cls


class _IntegerField(_IntegerFieldConst, _IntegralField, _Field):
    @property
    def cls(self) -> bt2_field_class._IntegerFieldClass:
        return self._cls

    def _check_range(self, value):
        if not (value >= self._lower_bound and value <= self._upper_bound):
            raise ValueError(
                f"Value {value} is outside valid range [{self._lower_bound}, {self._upper_bound}]"
            )


class _UnsignedIntegerFieldConst(_IntegerFieldConst, _FieldConst):
    _NAME = "Const unsigned integer"

    @property
    def cls(self) -> bt2_field_class._UnsignedIntegerFieldClassConst:
        return self._cls

    @classmethod
    def _value_to_int(cls, value):
        if not isinstance(value, numbers.Integral):
            raise TypeError("expecting an integral number object")

        return int(value)

    @property
    def _value(self):
        return native_bt.field_integer_unsigned_get_value(self._ptr)


class _UnsignedIntegerField(_UnsignedIntegerFieldConst, _IntegerField, _Field):
    _NAME = "Unsigned integer"

    @property
    def cls(self) -> bt2_field_class._UnsignedIntegerFieldClass:
        return self._cls

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
        return (2**self.cls.field_value_range) - 1


class _SignedIntegerFieldConst(_IntegerFieldConst, _FieldConst):
    _NAME = "Const signed integer"

    @property
    def cls(self) -> bt2_field_class._SignedIntegerFieldClassConst:
        return self._cls

    @classmethod
    def _value_to_int(cls, value):
        if not isinstance(value, numbers.Integral):
            raise TypeError("expecting an integral number object")

        return int(value)

    @property
    def _value(self):
        return native_bt.field_integer_signed_get_value(self._ptr)


class _SignedIntegerField(_SignedIntegerFieldConst, _IntegerField, _Field):
    _NAME = "Signed integer"

    @property
    def cls(self) -> bt2_field_class._SignedIntegerFieldClass:
        return self._cls

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
    _NAME = "Const real"

    @property
    def cls(self) -> bt2_field_class._RealFieldClassConst:
        return self._cls

    @classmethod
    def _value_to_float(cls, value):
        if not isinstance(value, numbers.Real):
            raise TypeError("expecting a real number object")

        return float(value)


class _SinglePrecisionRealFieldConst(_RealFieldConst):
    _NAME = "Const single-precision real"

    @property
    def cls(self) -> bt2_field_class._SinglePrecisionRealFieldClassConst:
        return self._cls

    @property
    def _value(self):
        return native_bt.field_real_single_precision_get_value(self._ptr)


class _DoublePrecisionRealFieldConst(_RealFieldConst):
    _NAME = "Const double-precision real"

    @property
    def cls(self) -> bt2_field_class._DoublePrecisionRealFieldClassConst:
        return self._cls

    @property
    def _value(self):
        return native_bt.field_real_double_precision_get_value(self._ptr)


class _RealField(_RealFieldConst, _NumericField):
    _NAME = "Real"

    @property
    def cls(self) -> bt2_field_class._RealFieldClass:
        return self._cls


class _SinglePrecisionRealField(_SinglePrecisionRealFieldConst, _RealField):
    _NAME = "Single-precision real"

    @property
    def cls(self) -> bt2_field_class._SinglePrecisionRealFieldClass:
        return self._cls

    # Largest finite single-precision value
    _MAX = 3.4028234663852886e38

    def _set_value(self, value):
        value = self._value_to_float(value)

        if math.isfinite(value) and not (-self._MAX <= value <= self._MAX):
            raise OverflowError(
                f"Value is outside the single-precision range ({value})"
            )

        native_bt.field_real_single_precision_set_value(self._ptr, value)

    value = property(fset=_set_value)


class _DoublePrecisionRealField(_DoublePrecisionRealFieldConst, _RealField):
    _NAME = "Double-precision real"

    @property
    def cls(self) -> bt2_field_class._DoublePrecisionRealFieldClass:
        return self._cls

    def _set_value(self, value):
        native_bt.field_real_double_precision_set_value(
            self._ptr, self._value_to_float(value)
        )

    value = property(fset=_set_value)


class _EnumerationFieldConst(_IntegerFieldConst):
    @property
    def cls(self) -> bt2_field_class._EnumerationFieldClassConst:
        return self._cls

    def _repr(self):
        return f"{self._value} ({', '.join(self.labels)})"

    @property
    def labels(self) -> typing.List[str]:
        return self._get_mapping_labels(self._ptr)


class _EnumerationField(_EnumerationFieldConst, _IntegerField):
    @property
    def cls(self) -> bt2_field_class._EnumerationFieldClass:
        return self._cls


class _UnsignedEnumerationFieldConst(
    _EnumerationFieldConst, _UnsignedIntegerFieldConst
):
    _NAME = "Const unsigned Enumeration"
    _get_mapping_labels = staticmethod(
        native_bt.field_enumeration_unsigned_get_mapping_labels
    )

    @property
    def cls(self) -> bt2_field_class._UnsignedEnumerationFieldClassConst:
        return self._cls


class _UnsignedEnumerationField(
    _UnsignedEnumerationFieldConst, _EnumerationField, _UnsignedIntegerField
):
    _NAME = "Unsigned enumeration"

    @property
    def cls(self) -> bt2_field_class._UnsignedEnumerationFieldClass:
        return self._cls


class _SignedEnumerationFieldConst(_EnumerationFieldConst, _SignedIntegerFieldConst):
    _NAME = "Const signed Enumeration"
    _get_mapping_labels = staticmethod(
        native_bt.field_enumeration_signed_get_mapping_labels
    )

    @property
    def cls(self) -> bt2_field_class._SignedEnumerationFieldClassConst:
        return self._cls


class _SignedEnumerationField(
    _SignedEnumerationFieldConst, _EnumerationField, _SignedIntegerField
):
    _NAME = "Signed enumeration"

    @property
    def cls(self) -> bt2_field_class._SignedEnumerationFieldClass:
        return self._cls


@functools.total_ordering
class _StringFieldConst(_FieldConst):
    _NAME = "Const string"

    @property
    def cls(self) -> bt2_field_class._StringFieldClassConst:
        return self._cls

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

    def __lt__(self, other) -> bool:
        return self._value < self._value_to_str(other)

    def __bool__(self) -> bool:
        return bool(self._value)

    def __hash__(self) -> int:
        return hash(self._value)

    def _repr(self):
        return repr(self._value)

    def __str__(self) -> str:
        return str(self._value)

    def __getitem__(self, index) -> str:
        return self._value[index]

    def __len__(self) -> int:
        return native_bt.field_string_get_length(self._ptr)


class _StringField(_StringFieldConst, _Field):
    _NAME = "String"

    @property
    def cls(self) -> bt2_field_class._StringFieldClass:
        return self._cls

    def _set_value(self, value):
        native_bt.field_string_set_value(self._ptr, self._value_to_str(value))

    value = property(fset=_set_value)

    def __iadd__(self, value) -> "_StringField":
        bt2_utils._handle_func_status(
            native_bt.field_string_append(self._ptr, self._value_to_str(value)),
            "cannot append to string field object's value",
        )
        return self

    def __hash__(self) -> int:
        # Non const field are not hashable as their value may be modified
        # without changing the underlying Python object.
        raise TypeError(f"unhashable type: '{self._NAME}'")


class _ContainerFieldConst(_FieldConst):
    def __bool__(self) -> bool:
        return len(self) != 0

    def _count(self):
        return len(self.cls)

    def __len__(self) -> int:
        return self._count()

    def __delitem__(self, index):
        raise NotImplementedError

    def __setitem__(self, index, value):
        raise TypeError(f"'{self.__class__}' object does not support item assignment")


class _ContainerField(_ContainerFieldConst, _Field):
    pass


class _StructureFieldConst(_ContainerFieldConst, typing.Mapping[str, _FieldConst]):
    _NAME = "Const structure"
    _borrow_member_field_ptr_by_index = staticmethod(
        native_bt.field_structure_borrow_member_field_by_index_const
    )
    _borrow_member_field_ptr_by_name = staticmethod(
        native_bt.field_structure_borrow_member_field_by_name_const
    )

    @property
    def cls(self) -> bt2_field_class._StructureFieldClassConst:
        return self._cls

    def _count(self):
        return len(self.cls)

    def __iter__(self) -> typing.Iterator[str]:
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
        return f"{{{', '.join([f'{k!r}: {v!r}' for k, v in self.items()])}}}"

    def _getitem(self, key):
        bt2_utils._check_str(key)
        field_ptr = self._borrow_member_field_ptr_by_name(self._ptr, key)

        if field_ptr is None:
            raise KeyError(key)

        return self._create_field_from_ptr(
            field_ptr, self._owner_ptr, self._owner_get_ref, self._owner_put_ref
        )

    def __getitem__(self, key) -> _FieldConst:
        return self._getitem(key)

    def member_at_index(self, index) -> _FieldConst:
        bt2_utils._check_uint64(index)

        if index >= len(self):
            raise IndexError

        return self._create_field_from_ptr(
            self._borrow_member_field_ptr_by_index(self._ptr, index),
            self._owner_ptr,
            self._owner_get_ref,
            self._owner_put_ref,
        )


class _StructureField(
    _StructureFieldConst, _ContainerField, collections.abc.MutableMapping
):
    _NAME = "Structure"
    _borrow_member_field_ptr_by_index = staticmethod(
        native_bt.field_structure_borrow_member_field_by_index
    )
    _borrow_member_field_ptr_by_name = staticmethod(
        native_bt.field_structure_borrow_member_field_by_name
    )

    @property
    def cls(self) -> bt2_field_class._StructureFieldClass:
        return self._cls

    def __getitem__(self, key) -> _Field:
        return self._getitem(key)

    def __setitem__(self, key: str, value):
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
    _NAME = "Const option"
    _borrow_field_ptr = staticmethod(native_bt.field_option_borrow_field_const)

    @property
    def cls(self) -> bt2_field_class._OptionFieldClassConst:
        return self._cls

    @property
    def field(self) -> typing.Optional[_FieldConst]:
        field_ptr = self._borrow_field_ptr(self._ptr)

        if field_ptr is None:
            return

        return self._create_field_from_ptr(
            field_ptr, self._owner_ptr, self._owner_get_ref, self._owner_put_ref
        )

    @property
    def has_field(self) -> bool:
        return self.field is not None

    def _spec_eq(self, other):
        return _get_leaf_field(self) == other

    def __bool__(self) -> bool:
        return self.has_field

    def __str__(self) -> str:
        return str(self.field)

    def _repr(self):
        return repr(self.field)


class _OptionField(_OptionFieldConst, _Field):
    _NAME = "Option"
    _borrow_field_ptr = staticmethod(native_bt.field_option_borrow_field)

    @property
    def cls(self) -> bt2_field_class._OptionFieldClass:
        return self._cls

    @_OptionFieldConst.has_field.setter
    def has_field(self, value):
        bt2_utils._check_bool(value)
        native_bt.field_option_set_has_field(self._ptr, value)

    def _set_value(self, value):
        self.has_field = True
        self.field.value = value

    value = property(fset=_set_value)


class _OptionFieldWithBoolSelectorFieldConst(_OptionFieldConst):
    @property
    def cls(self) -> bt2_field_class._OptionFieldClassWithBoolSelectorFieldConst:
        return self._cls


class _OptionFieldWithBoolSelectorField(
    _OptionFieldWithBoolSelectorFieldConst, _OptionField
):
    @property
    def cls(self) -> bt2_field_class._OptionFieldClassWithBoolSelectorField:
        return self._cls


class _OptionFieldWithUnsignedIntegerSelectorFieldConst(_OptionFieldConst):
    @property
    def cls(
        self,
    ) -> bt2_field_class._OptionFieldClassWithUnsignedIntegerSelectorFieldConst:
        return self._cls


class _OptionFieldWithUnsignedIntegerSelectorField(
    _OptionFieldWithUnsignedIntegerSelectorFieldConst, _OptionField
):
    @property
    def cls(self) -> bt2_field_class._OptionFieldClassWithUnsignedIntegerSelectorField:
        return self._cls


class _OptionFieldWithSignedIntegerSelectorFieldConst(_OptionFieldConst):
    @property
    def cls(
        self,
    ) -> bt2_field_class._OptionFieldClassWithSignedIntegerSelectorFieldConst:
        return self._cls


class _OptionFieldWithSignedIntegerSelectorField(
    _OptionFieldWithSignedIntegerSelectorFieldConst, _OptionField
):
    @property
    def cls(self) -> bt2_field_class._OptionFieldClassWithSignedIntegerSelectorField:
        return self._cls


class _VariantFieldConst(_ContainerFieldConst, _FieldConst):
    _NAME = "Const variant"
    _borrow_selected_option_field_ptr = staticmethod(
        native_bt.field_variant_borrow_selected_option_field_const
    )

    @property
    def cls(self) -> bt2_field_class._VariantFieldClassConst:
        return self._cls

    def _count(self):
        return len(self.cls)

    @property
    def selected_option_index(self) -> int:
        return native_bt.field_variant_get_selected_option_index(self._ptr)

    @property
    def selected_option(self) -> _FieldConst:
        # TODO: Is there a way to check if the variant field has a selected_option,
        # so we can raise an exception instead of hitting a pre-condition check?
        # If there is something, that check should be added to selected_option_index too.
        return self._create_field_from_ptr(
            self._borrow_selected_option_field_ptr(self._ptr),
            self._owner_ptr,
            self._owner_get_ref,
            self._owner_put_ref,
        )

    def _spec_eq(self, other):
        return _get_leaf_field(self) == other

    def __bool__(self) -> bool:
        raise NotImplementedError

    def __str__(self) -> str:
        return str(self.selected_option)

    def _repr(self):
        return repr(self.selected_option)


class _VariantField(_VariantFieldConst, _ContainerField, _Field):
    _NAME = "Variant"
    _borrow_selected_option_field_ptr = staticmethod(
        native_bt.field_variant_borrow_selected_option_field
    )

    @property
    def cls(self) -> bt2_field_class._VariantFieldClass:
        return self._cls

    @_VariantFieldConst.selected_option_index.setter
    def selected_option_index(self, index: int):
        bt2_utils._check_int(index)

        if index < 0 or index >= len(self):
            raise IndexError(f"{self._NAME} field object index is out of range")

        native_bt.field_variant_select_option_by_index(self._ptr, index)

    def _set_value(self, value):
        self.selected_option.value = value

    value = property(fset=_set_value)


class _VariantFieldWithUnsignedIntegerSelectorFieldConst(_VariantFieldConst):
    @property
    def cls(
        self,
    ) -> bt2_field_class._VariantFieldClassWithUnsignedIntegerSelectorFieldConst:
        return self._cls


class _VariantFieldWithUnsignedIntegerSelectorField(
    _VariantFieldWithUnsignedIntegerSelectorFieldConst, _VariantField
):
    @property
    def cls(self) -> bt2_field_class._VariantFieldClassWithUnsignedIntegerSelector:
        return self._cls


class _VariantFieldWithSignedIntegerSelectorFieldConst(_VariantFieldConst):
    @property
    def cls(
        self,
    ) -> bt2_field_class._VariantFieldClassWithSignedIntegerSelectorFieldConst:
        return self._cls


class _VariantFieldWithSignedIntegerSelectorField(
    _VariantFieldWithSignedIntegerSelectorFieldConst, _VariantField
):
    @property
    def cls(self) -> bt2_field_class._VariantFieldClassWithSignedIntegerSelector:
        return self._cls


class _ArrayFieldConst(_ContainerFieldConst, _FieldConst, typing.Sequence[_FieldConst]):
    _borrow_element_field_ptr_by_index = staticmethod(
        native_bt.field_array_borrow_element_field_by_index_const
    )

    @property
    def cls(self) -> bt2_field_class._ArrayFieldClassConst:
        return self._cls

    @property
    def length(self):
        return native_bt.field_array_get_length(self._ptr)

    def __getitem__(self, index: int) -> _FieldConst:
        if not isinstance(index, numbers.Integral):
            raise TypeError(
                f"'{index.__class__.__name__}' is not an integral number object: invalid index"
            )

        index = int(index)

        if index < 0 or index >= len(self):
            raise IndexError(f"{self._NAME} field object index is out of range")

        return self._create_field_from_ptr(
            self._borrow_element_field_ptr_by_index(self._ptr, index),
            self._owner_ptr,
            self._owner_get_ref,
            self._owner_put_ref,
        )

    def insert(self, index: int, value):
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
        return f"[{', '.join([repr(v) for v in self])}]"


class _ArrayField(
    _ArrayFieldConst, _ContainerField, _Field, collections.abc.MutableSequence
):
    _borrow_element_field_ptr_by_index = staticmethod(
        native_bt.field_array_borrow_element_field_by_index
    )

    @property
    def cls(self) -> bt2_field_class._ArrayFieldClass:
        return self._cls

    def __setitem__(self, index: int, value):
        # raises if index is somehow invalid
        field = self[index]

        if not isinstance(field, (_NumericField, _StringField)):
            raise TypeError("can only set the value of a number or string field")

        # the field's property does the appropriate conversion or raises
        # the appropriate exception
        field.value = value


class _StaticArrayFieldConst(_ArrayFieldConst, _FieldConst):
    _NAME = "Const static array"

    @property
    def cls(self) -> bt2_field_class._StaticArrayFieldClassConst:
        return self._cls

    def _count(self):
        return native_bt.field_array_get_length(self._ptr)


class _StaticArrayField(_StaticArrayFieldConst, _ArrayField, _Field):
    _NAME = "Static array"

    @property
    def cls(self) -> bt2_field_class._StaticArrayFieldClass:
        return self._cls

    def _set_value(self, values):
        if len(self) != len(values):
            raise ValueError(
                f"expected length of value ({len(values)}) and array field ({len(self)}) to match"
            )

        for index, value in enumerate(values):
            if value is not None:
                self[index].value = value

    value = property(fset=_set_value)


class _DynamicArrayFieldConst(_ArrayFieldConst, _FieldConst):
    _NAME = "Const dynamic array"

    @property
    def cls(self) -> bt2_field_class._DynamicArrayFieldClassConst:
        return self._cls

    def _count(self):
        return self.length


class _DynamicArrayField(_DynamicArrayFieldConst, _ArrayField, _Field):
    _NAME = "Dynamic array"

    @property
    def cls(self) -> bt2_field_class._DynamicArrayFieldClass:
        return self._cls

    @_ArrayFieldConst.length.setter
    def length(self, length):
        bt2_utils._check_uint64(length)
        bt2_utils._handle_func_status(
            native_bt.field_array_dynamic_set_length(self._ptr, length),
            "cannot set dynamic array length",
        )

    def _set_value(self, values):
        if len(values) != self.length:
            self.length = len(values)

        for index, value in enumerate(values):
            if value is not None:
                self[index].value = value

    value = property(fset=_set_value)


class _DynamicArrayFieldWithLengthFieldConst(_DynamicArrayFieldConst):
    @property
    def cls(self) -> bt2_field_class._DynamicArrayFieldClassWithLengthFieldConst:
        return self._cls


class _DynamicArrayFieldWithLengthField(
    _DynamicArrayFieldWithLengthFieldConst, _DynamicArrayField
):
    @property
    def cls(self) -> bt2_field_class._DynamicArrayFieldClassWithLengthField:
        return self._cls


class _BlobFieldConst(_FieldConst):
    @property
    def cls(self) -> bt2_field_class._BlobFieldClassConst:
        return self._cls

    def _get_length(self):
        return native_bt.field_blob_get_length(self._ptr)

    length = property(fget=_get_length)

    def __len__(self) -> int:
        return self._get_length()

    def _repr(self):
        return str(self.data.tobytes())

    @property
    def data(self) -> memoryview:
        return native_bt.field_blob_get_data_const(self._ptr)


class _BlobField(_BlobFieldConst, _Field):
    @property
    def cls(self) -> bt2_field_class._BlobFieldClass:
        return self._cls

    @property
    def data(self) -> memoryview:
        return native_bt.field_blob_get_data(self._ptr)

    @data.setter
    def data(self, data: typing.Union[bytes, bytearray, memoryview]):
        native_bt.field_blob_get_data(self._ptr)[:] = data


class _StaticBlobFieldConst(_BlobFieldConst):
    @property
    def cls(self) -> bt2_field_class._StaticBlobFieldClassConst:
        return self._cls


class _StaticBlobField(_StaticBlobFieldConst, _BlobField):
    @property
    def cls(self) -> bt2_field_class._StaticBlobFieldClass:
        return self._cls


class _DynamicBlobFieldConst(_BlobFieldConst):
    @property
    def cls(self) -> bt2_field_class._DynamicBlobFieldClassConst:
        return self._cls


class _DynamicBlobField(_DynamicBlobFieldConst, _BlobField):
    @property
    def cls(self) -> bt2_field_class._DynamicBlobFieldClass:
        return self._cls

    def _set_length(self, length):
        bt2_utils._check_uint64(length)
        native_bt.field_blob_dynamic_set_length(self._ptr, length)

    length = property(fget=_BlobFieldConst.length, fset=_set_length)


class _DynamicBlobFieldWithLengthFieldConst(_DynamicBlobFieldConst):
    @property
    def cls(self) -> bt2_field_class._DynamicBlobFieldClassWithLengthFieldConst:
        return self._cls


class _DynamicBlobFieldWithLengthField(
    _DynamicBlobFieldWithLengthFieldConst, _DynamicBlobField
):
    @property
    def cls(self) -> bt2_field_class._DynamicBlobFieldClassWithLengthField:
        return self._cls


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
    native_bt.FIELD_CLASS_TYPE_DYNAMIC_ARRAY_WITH_LENGTH_FIELD: _DynamicArrayFieldWithLengthFieldConst,
    native_bt.FIELD_CLASS_TYPE_OPTION_WITHOUT_SELECTOR_FIELD: _OptionFieldConst,
    native_bt.FIELD_CLASS_TYPE_OPTION_WITH_BOOL_SELECTOR_FIELD: _OptionFieldWithBoolSelectorFieldConst,
    native_bt.FIELD_CLASS_TYPE_OPTION_WITH_UNSIGNED_INTEGER_SELECTOR_FIELD: _OptionFieldWithUnsignedIntegerSelectorFieldConst,
    native_bt.FIELD_CLASS_TYPE_OPTION_WITH_SIGNED_INTEGER_SELECTOR_FIELD: _OptionFieldWithSignedIntegerSelectorFieldConst,
    native_bt.FIELD_CLASS_TYPE_VARIANT_WITHOUT_SELECTOR_FIELD: _VariantFieldConst,
    native_bt.FIELD_CLASS_TYPE_VARIANT_WITH_UNSIGNED_INTEGER_SELECTOR_FIELD: _VariantFieldWithUnsignedIntegerSelectorFieldConst,
    native_bt.FIELD_CLASS_TYPE_VARIANT_WITH_SIGNED_INTEGER_SELECTOR_FIELD: _VariantFieldWithSignedIntegerSelectorFieldConst,
    native_bt.FIELD_CLASS_TYPE_STATIC_BLOB: _StaticBlobFieldConst,
    native_bt.FIELD_CLASS_TYPE_DYNAMIC_BLOB_WITHOUT_LENGTH_FIELD: _DynamicBlobFieldConst,
    native_bt.FIELD_CLASS_TYPE_DYNAMIC_BLOB_WITH_LENGTH_FIELD: _DynamicBlobFieldWithLengthFieldConst,
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
    native_bt.FIELD_CLASS_TYPE_DYNAMIC_ARRAY_WITH_LENGTH_FIELD: _DynamicArrayFieldWithLengthField,
    native_bt.FIELD_CLASS_TYPE_OPTION_WITHOUT_SELECTOR_FIELD: _OptionField,
    native_bt.FIELD_CLASS_TYPE_OPTION_WITH_BOOL_SELECTOR_FIELD: _OptionFieldWithBoolSelectorField,
    native_bt.FIELD_CLASS_TYPE_OPTION_WITH_UNSIGNED_INTEGER_SELECTOR_FIELD: _OptionFieldWithUnsignedIntegerSelectorField,
    native_bt.FIELD_CLASS_TYPE_OPTION_WITH_SIGNED_INTEGER_SELECTOR_FIELD: _OptionFieldWithSignedIntegerSelectorField,
    native_bt.FIELD_CLASS_TYPE_VARIANT_WITHOUT_SELECTOR_FIELD: _VariantField,
    native_bt.FIELD_CLASS_TYPE_VARIANT_WITH_UNSIGNED_INTEGER_SELECTOR_FIELD: _VariantFieldWithUnsignedIntegerSelectorField,
    native_bt.FIELD_CLASS_TYPE_VARIANT_WITH_SIGNED_INTEGER_SELECTOR_FIELD: _VariantFieldWithSignedIntegerSelectorField,
    native_bt.FIELD_CLASS_TYPE_STATIC_BLOB: _StaticBlobField,
    native_bt.FIELD_CLASS_TYPE_DYNAMIC_BLOB_WITHOUT_LENGTH_FIELD: _DynamicBlobField,
    native_bt.FIELD_CLASS_TYPE_DYNAMIC_BLOB_WITH_LENGTH_FIELD: _DynamicBlobFieldWithLengthField,
}
