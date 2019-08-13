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
from bt2 import field_path as bt2_field_path
from bt2 import integer_range_set as bt2_integer_range_set
import bt2


def _create_field_class_from_ptr_and_get_ref(ptr):
    typeid = native_bt.field_class_get_type(ptr)
    return _FIELD_CLASS_TYPE_TO_OBJ[typeid]._create_from_ptr_and_get_ref(ptr)


class IntegerDisplayBase:
    BINARY = native_bt.FIELD_CLASS_INTEGER_PREFERRED_DISPLAY_BASE_BINARY
    OCTAL = native_bt.FIELD_CLASS_INTEGER_PREFERRED_DISPLAY_BASE_OCTAL
    DECIMAL = native_bt.FIELD_CLASS_INTEGER_PREFERRED_DISPLAY_BASE_DECIMAL
    HEXADECIMAL = native_bt.FIELD_CLASS_INTEGER_PREFERRED_DISPLAY_BASE_HEXADECIMAL


class _FieldClass(object._SharedObject):
    _get_ref = staticmethod(native_bt.field_class_get_ref)
    _put_ref = staticmethod(native_bt.field_class_put_ref)

    def _check_create_status(self, ptr):
        if ptr is None:
            raise bt2._MemoryError(
                'cannot create {} field class object'.format(self._NAME.lower())
            )


class _BoolFieldClass(_FieldClass):
    _NAME = 'Boolean'


class _IntegerFieldClass(_FieldClass):
    @property
    def field_value_range(self):
        size = native_bt.field_class_integer_get_field_value_range(self._ptr)
        assert size >= 1
        return size

    def _field_value_range(self, size):
        if size < 1 or size > 64:
            raise ValueError("Value is outside valid range [1, 64] ({})".format(size))
        native_bt.field_class_integer_set_field_value_range(self._ptr, size)

    _field_value_range = property(fset=_field_value_range)

    @property
    def preferred_display_base(self):
        base = native_bt.field_class_integer_get_preferred_display_base(self._ptr)
        assert base >= 0
        return base

    def _preferred_display_base(self, base):
        utils._check_uint64(base)

        if base not in (
            IntegerDisplayBase.BINARY,
            IntegerDisplayBase.OCTAL,
            IntegerDisplayBase.DECIMAL,
            IntegerDisplayBase.HEXADECIMAL,
        ):
            raise ValueError("Display base is not a valid IntegerDisplayBase value")

        native_bt.field_class_integer_set_preferred_display_base(self._ptr, base)

    _preferred_display_base = property(fset=_preferred_display_base)


class _UnsignedIntegerFieldClass(_IntegerFieldClass):
    _NAME = 'Unsigned integer'


class _SignedIntegerFieldClass(_IntegerFieldClass):
    _NAME = 'Signed integer'


class _RealFieldClass(_FieldClass):
    _NAME = 'Real'

    @property
    def is_single_precision(self):
        return native_bt.field_class_real_is_single_precision(self._ptr)

    def _is_single_precision(self, is_single_precision):
        utils._check_bool(is_single_precision)
        native_bt.field_class_real_set_is_single_precision(
            self._ptr, is_single_precision
        )

    _is_single_precision = property(fset=_is_single_precision)


# an enumeration field class mapping does not have a reference count, so
# we copy the properties here to avoid eventual memory access errors.
class _EnumerationFieldClassMapping:
    def __init__(self, mapping_ptr):
        base_mapping_ptr = self._as_enumeration_field_class_mapping_ptr(mapping_ptr)
        self._label = native_bt.field_class_enumeration_mapping_get_label(
            base_mapping_ptr
        )
        assert self._label is not None
        ranges_ptr = self._mapping_borrow_ranges_ptr(mapping_ptr)
        assert ranges_ptr is not None
        self._ranges = self._ranges_type._create_from_ptr_and_get_ref(ranges_ptr)

    @property
    def label(self):
        return self._label

    @property
    def ranges(self):
        return self._ranges


class _UnsignedEnumerationFieldClassMapping(_EnumerationFieldClassMapping):
    _ranges_type = bt2_integer_range_set.UnsignedIntegerRangeSet
    _as_enumeration_field_class_mapping_ptr = staticmethod(
        native_bt.field_class_enumeration_unsigned_mapping_as_mapping_const
    )
    _mapping_borrow_ranges_ptr = staticmethod(
        native_bt.field_class_enumeration_unsigned_mapping_borrow_ranges_const
    )


class _SignedEnumerationFieldClassMapping(_EnumerationFieldClassMapping):
    _ranges_type = bt2_integer_range_set.SignedIntegerRangeSet
    _as_enumeration_field_class_mapping_ptr = staticmethod(
        native_bt.field_class_enumeration_signed_mapping_as_mapping_const
    )
    _mapping_borrow_ranges_ptr = staticmethod(
        native_bt.field_class_enumeration_signed_mapping_borrow_ranges_const
    )


class _EnumerationFieldClass(_IntegerFieldClass, collections.abc.Mapping):
    def __len__(self):
        count = native_bt.field_class_enumeration_get_mapping_count(self._ptr)
        assert count >= 0
        return count

    def add_mapping(self, label, ranges):
        utils._check_str(label)
        utils._check_type(ranges, self._range_set_type)

        if label in self:
            raise ValueError("duplicate mapping label '{}'".format(label))

        status = self._add_mapping(self._ptr, label, ranges._ptr)
        utils._handle_func_status(
            status, 'cannot add mapping to enumeration field class object'
        )

    def mappings_for_value(self, value):
        status, labels = self._get_mapping_labels_for_value(self._ptr, value)
        utils._handle_func_status(
            status, 'cannot get mapping labels for value {}'.format(value)
        )
        return [self[label] for label in labels]

    def __iter__(self):
        for idx in range(len(self)):
            mapping = self._get_mapping_by_index(self._ptr, idx)
            yield mapping.label

    def __getitem__(self, label):
        utils._check_str(label)
        mapping = self._get_mapping_by_label(self._ptr, label)

        if mapping is None:
            raise KeyError(label)

        return mapping

    def __iadd__(self, mappings):
        for label, ranges in mappings:
            self.add_mapping(label, ranges)

        return self


class _UnsignedEnumerationFieldClass(
    _EnumerationFieldClass, _UnsignedIntegerFieldClass
):
    _NAME = 'Unsigned enumeration'
    _range_set_type = bt2_integer_range_set.UnsignedIntegerRangeSet
    _add_mapping = staticmethod(native_bt.field_class_enumeration_unsigned_add_mapping)

    @staticmethod
    def _get_mapping_by_index(enum_ptr, index):
        mapping_ptr = native_bt.field_class_enumeration_unsigned_borrow_mapping_by_index_const(
            enum_ptr, index
        )
        assert mapping_ptr is not None
        return _UnsignedEnumerationFieldClassMapping(mapping_ptr)

    @staticmethod
    def _get_mapping_by_label(enum_ptr, label):
        mapping_ptr = native_bt.field_class_enumeration_unsigned_borrow_mapping_by_label_const(
            enum_ptr, label
        )

        if mapping_ptr is None:
            return

        return _UnsignedEnumerationFieldClassMapping(mapping_ptr)

    @staticmethod
    def _get_mapping_labels_for_value(enum_ptr, value):
        utils._check_uint64(value)
        return native_bt.field_class_enumeration_unsigned_get_mapping_labels_for_value(
            enum_ptr, value
        )


class _SignedEnumerationFieldClass(_EnumerationFieldClass, _SignedIntegerFieldClass):
    _NAME = 'Signed enumeration'
    _range_set_type = bt2_integer_range_set.SignedIntegerRangeSet
    _add_mapping = staticmethod(native_bt.field_class_enumeration_signed_add_mapping)

    @staticmethod
    def _get_mapping_by_index(enum_ptr, index):
        mapping_ptr = native_bt.field_class_enumeration_signed_borrow_mapping_by_index_const(
            enum_ptr, index
        )
        assert mapping_ptr is not None
        return _SignedEnumerationFieldClassMapping(mapping_ptr)

    @staticmethod
    def _get_mapping_by_label(enum_ptr, label):
        mapping_ptr = native_bt.field_class_enumeration_signed_borrow_mapping_by_label_const(
            enum_ptr, label
        )

        if mapping_ptr is None:
            return

        return _SignedEnumerationFieldClassMapping(mapping_ptr)

    @staticmethod
    def _get_mapping_labels_for_value(enum_ptr, value):
        utils._check_int64(value)
        return native_bt.field_class_enumeration_signed_get_mapping_labels_for_value(
            enum_ptr, value
        )


class _StringFieldClass(_FieldClass):
    _NAME = 'String'


class _StructureFieldClassMember:
    def __init__(self, name, field_class):
        self._name = name
        self._field_class = field_class

    @property
    def name(self):
        return self._name

    @property
    def field_class(self):
        return self._field_class


class _StructureFieldClass(_FieldClass, collections.abc.Mapping):
    _NAME = 'Structure'

    def append_member(self, name, field_class):
        utils._check_str(name)
        utils._check_type(field_class, _FieldClass)

        if name in self:
            raise ValueError("duplicate member name '{}'".format(name))

        status = native_bt.field_class_structure_append_member(
            self._ptr, name, field_class._ptr
        )
        utils._handle_func_status(
            status, 'cannot append member to structure field class object'
        )

    def __len__(self):
        count = native_bt.field_class_structure_get_member_count(self._ptr)
        assert count >= 0
        return count

    @staticmethod
    def _create_member_from_ptr(member_ptr):
        name = native_bt.field_class_structure_member_get_name(member_ptr)
        assert name is not None
        fc_ptr = native_bt.field_class_structure_member_borrow_field_class_const(
            member_ptr
        )
        assert fc_ptr is not None
        fc = _create_field_class_from_ptr_and_get_ref(fc_ptr)
        return _StructureFieldClassMember(name, fc)

    def __getitem__(self, key):
        if not isinstance(key, str):
            raise TypeError(
                "key must be a 'str' object, got '{}'".format(key.__class__.__name__)
            )

        member_ptr = native_bt.field_class_structure_borrow_member_by_name_const(
            self._ptr, key
        )

        if member_ptr is None:
            raise KeyError(key)

        return self._create_member_from_ptr(member_ptr)

    def __iter__(self):
        for idx in range(len(self)):
            member_ptr = native_bt.field_class_structure_borrow_member_by_index_const(
                self._ptr, idx
            )
            assert member_ptr is not None
            yield native_bt.field_class_structure_member_get_name(member_ptr)

    def __iadd__(self, members):
        for name, field_class in members:
            self.append_member(name, field_class)

        return self

    def member_at_index(self, index):
        utils._check_uint64(index)

        if index >= len(self):
            raise IndexError

        member_ptr = native_bt.field_class_structure_borrow_member_by_index_const(
            self._ptr, index
        )
        assert member_ptr is not None
        return self._create_member_from_ptr(member_ptr)


class _OptionFieldClass(_FieldClass):
    @property
    def field_class(self):
        elem_fc_ptr = native_bt.field_class_option_borrow_field_class_const(self._ptr)
        return _create_field_class_from_ptr_and_get_ref(elem_fc_ptr)

    @property
    def selector_field_path(self):
        ptr = native_bt.field_class_option_borrow_selector_field_path_const(self._ptr)
        if ptr is None:
            return

        return bt2_field_path._FieldPath._create_from_ptr_and_get_ref(ptr)


class _VariantFieldClassOption:
    def __init__(self, name, field_class):
        self._name = name
        self._field_class = field_class

    @property
    def name(self):
        return self._name

    @property
    def field_class(self):
        return self._field_class


class _VariantFieldClassWithSelectorOption(_VariantFieldClassOption):
    def __init__(self, name, field_class, ranges):
        super().__init__(name, field_class)
        self._ranges = ranges

    @property
    def ranges(self):
        return self._ranges


class _VariantFieldClass(_FieldClass, collections.abc.Mapping):
    _NAME = 'Variant'
    _borrow_option_by_name_ptr = staticmethod(
        native_bt.field_class_variant_borrow_option_by_name_const
    )
    _borrow_member_by_index_ptr = staticmethod(
        native_bt.field_class_variant_borrow_option_by_index_const
    )

    @staticmethod
    def _as_option_ptr(opt_ptr):
        return opt_ptr

    def _create_option_from_ptr(self, opt_ptr):
        name = native_bt.field_class_variant_option_get_name(opt_ptr)
        assert name is not None
        fc_ptr = native_bt.field_class_variant_option_borrow_field_class_const(opt_ptr)
        assert fc_ptr is not None
        fc = _create_field_class_from_ptr_and_get_ref(fc_ptr)
        return _VariantFieldClassOption(name, fc)

    def __len__(self):
        count = native_bt.field_class_variant_get_option_count(self._ptr)
        assert count >= 0
        return count

    def __getitem__(self, key):
        if not isinstance(key, str):
            raise TypeError(
                "key must be a 'str' object, got '{}'".format(key.__class__.__name__)
            )

        opt_ptr = self._borrow_option_by_name_ptr(self._ptr, key)

        if opt_ptr is None:
            raise KeyError(key)

        return self._create_option_from_ptr(opt_ptr)

    def __iter__(self):
        for idx in range(len(self)):
            opt_ptr = self._borrow_member_by_index_ptr(self._ptr, idx)
            assert opt_ptr is not None
            base_opt_ptr = self._as_option_ptr(opt_ptr)
            yield native_bt.field_class_variant_option_get_name(base_opt_ptr)

    def option_at_index(self, index):
        utils._check_uint64(index)

        if index >= len(self):
            raise IndexError

        opt_ptr = self._borrow_member_by_index_ptr(self._ptr, index)
        assert opt_ptr is not None
        return self._create_option_from_ptr(opt_ptr)


class _VariantFieldClassWithoutSelector(_VariantFieldClass):
    _NAME = 'Variant (without selector)'

    def append_option(self, name, field_class):
        utils._check_str(name)
        utils._check_type(field_class, _FieldClass)

        if name in self:
            raise ValueError("duplicate option name '{}'".format(name))

        status = native_bt.field_class_variant_without_selector_append_option(
            self._ptr, name, field_class._ptr
        )
        utils._handle_func_status(
            status, 'cannot append option to variant field class object'
        )

    def __iadd__(self, options):
        for name, field_class in options:
            self.append_option(name, field_class)

        return self


class _VariantFieldClassWithSelector(_VariantFieldClass):
    _NAME = 'Variant (with selector)'

    def _create_option_from_ptr(self, opt_ptr):
        base_opt_ptr = self._as_option_ptr(opt_ptr)
        name = native_bt.field_class_variant_option_get_name(base_opt_ptr)
        assert name is not None
        fc_ptr = native_bt.field_class_variant_option_borrow_field_class_const(
            base_opt_ptr
        )
        assert fc_ptr is not None
        fc = _create_field_class_from_ptr_and_get_ref(fc_ptr)
        range_set_ptr = self._option_borrow_ranges_ptr(opt_ptr)
        assert range_set_ptr is not None
        range_set = self._range_set_type._create_from_ptr_and_get_ref(range_set_ptr)
        return _VariantFieldClassWithSelectorOption(name, fc, range_set)

    @property
    def selector_field_path(self):
        ptr = native_bt.field_class_variant_with_selector_borrow_selector_field_path_const(
            self._ptr
        )

        if ptr is None:
            return

        return bt2_field_path._FieldPath._create_from_ptr_and_get_ref(ptr)

    def append_option(self, name, field_class, ranges):
        utils._check_str(name)
        utils._check_type(field_class, _FieldClass)
        utils._check_type(ranges, self._range_set_type)

        if name in self:
            raise ValueError("duplicate option name '{}'".format(name))

        if len(ranges) == 0:
            raise ValueError('range set is empty')

        # TODO: check overlaps (precondition of self._append_option())

        status = self._append_option(self._ptr, name, field_class._ptr, ranges._ptr)
        utils._handle_func_status(
            status, 'cannot append option to variant field class object'
        )

    def __iadd__(self, options):
        for name, field_class, ranges in options:
            self.append_option(name, field_class, ranges)

        return self


class _VariantFieldClassWithUnsignedSelector(_VariantFieldClassWithSelector):
    _NAME = 'Variant (with unsigned selector)'
    _borrow_option_by_name_ptr = staticmethod(
        native_bt.field_class_variant_with_selector_unsigned_borrow_option_by_name_const
    )
    _borrow_member_by_index_ptr = staticmethod(
        native_bt.field_class_variant_with_selector_unsigned_borrow_option_by_index_const
    )
    _as_option_ptr = staticmethod(
        native_bt.field_class_variant_with_selector_unsigned_option_as_option_const
    )
    _append_option = staticmethod(
        native_bt.field_class_variant_with_selector_unsigned_append_option
    )
    _option_borrow_ranges_ptr = staticmethod(
        native_bt.field_class_variant_with_selector_unsigned_option_borrow_ranges_const
    )
    _range_set_type = bt2_integer_range_set.UnsignedIntegerRangeSet


class _VariantFieldClassWithSignedSelector(_VariantFieldClassWithSelector):
    _NAME = 'Variant (with signed selector)'
    _borrow_option_by_name_ptr = staticmethod(
        native_bt.field_class_variant_with_selector_signed_borrow_option_by_name_const
    )
    _borrow_member_by_index_ptr = staticmethod(
        native_bt.field_class_variant_with_selector_signed_borrow_option_by_index_const
    )
    _as_option_ptr = staticmethod(
        native_bt.field_class_variant_with_selector_signed_option_as_option_const
    )
    _append_option = staticmethod(
        native_bt.field_class_variant_with_selector_signed_append_option
    )
    _option_borrow_ranges_ptr = staticmethod(
        native_bt.field_class_variant_with_selector_signed_option_borrow_ranges_const
    )
    _range_set_type = bt2_integer_range_set.SignedIntegerRangeSet


class _ArrayFieldClass(_FieldClass):
    @property
    def element_field_class(self):
        elem_fc_ptr = native_bt.field_class_array_borrow_element_field_class_const(
            self._ptr
        )
        return _create_field_class_from_ptr_and_get_ref(elem_fc_ptr)


class _StaticArrayFieldClass(_ArrayFieldClass):
    @property
    def length(self):
        return native_bt.field_class_array_static_get_length(self._ptr)


class _DynamicArrayFieldClass(_ArrayFieldClass):
    @property
    def length_field_path(self):
        ptr = native_bt.field_class_array_dynamic_borrow_length_field_path_const(
            self._ptr
        )
        if ptr is None:
            return

        return bt2_field_path._FieldPath._create_from_ptr_and_get_ref(ptr)


_FIELD_CLASS_TYPE_TO_OBJ = {
    native_bt.FIELD_CLASS_TYPE_BOOL: _BoolFieldClass,
    native_bt.FIELD_CLASS_TYPE_UNSIGNED_INTEGER: _UnsignedIntegerFieldClass,
    native_bt.FIELD_CLASS_TYPE_SIGNED_INTEGER: _SignedIntegerFieldClass,
    native_bt.FIELD_CLASS_TYPE_REAL: _RealFieldClass,
    native_bt.FIELD_CLASS_TYPE_UNSIGNED_ENUMERATION: _UnsignedEnumerationFieldClass,
    native_bt.FIELD_CLASS_TYPE_SIGNED_ENUMERATION: _SignedEnumerationFieldClass,
    native_bt.FIELD_CLASS_TYPE_STRING: _StringFieldClass,
    native_bt.FIELD_CLASS_TYPE_STRUCTURE: _StructureFieldClass,
    native_bt.FIELD_CLASS_TYPE_STATIC_ARRAY: _StaticArrayFieldClass,
    native_bt.FIELD_CLASS_TYPE_DYNAMIC_ARRAY: _DynamicArrayFieldClass,
    native_bt.FIELD_CLASS_TYPE_OPTION: _OptionFieldClass,
    native_bt.FIELD_CLASS_TYPE_VARIANT_WITHOUT_SELECTOR: _VariantFieldClassWithoutSelector,
    native_bt.FIELD_CLASS_TYPE_VARIANT_WITH_UNSIGNED_SELECTOR: _VariantFieldClassWithUnsignedSelector,
    native_bt.FIELD_CLASS_TYPE_VARIANT_WITH_SIGNED_SELECTOR: _VariantFieldClassWithSignedSelector,
}
