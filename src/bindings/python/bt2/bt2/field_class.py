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
import bt2.field
import bt2.field_path
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
            raise bt2.CreationError('cannot create {} field class object'.format(self._NAME.lower()))


class _IntegerFieldClass(_FieldClass):
    @property
    def field_value_range(self):
        size = native_bt.field_class_integer_get_field_value_range(self._ptr)
        assert(size >= 1)
        return size

    def _field_value_range(self, size):
        if size < 1 or size > 64:
            raise ValueError("Value is outside valid range [1, 64] ({})".format(size))
        native_bt.field_class_integer_set_field_value_range(self._ptr, size)

    _field_value_range = property(fset=_field_value_range)

    @property
    def preferred_display_base(self):
        base = native_bt.field_class_integer_get_preferred_display_base(
            self._ptr)
        assert(base >= 0)
        return base

    def _preferred_display_base(self, base):
        utils._check_uint64(base)

        if base not in (IntegerDisplayBase.BINARY,
                        IntegerDisplayBase.OCTAL,
                        IntegerDisplayBase.DECIMAL,
                        IntegerDisplayBase.HEXADECIMAL):
            raise ValueError("Display base is not a valid IntegerDisplayBase value")

        native_bt.field_class_integer_set_preferred_display_base(
            self._ptr, base)

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
            self._ptr, is_single_precision)

    _is_single_precision = property(fset=_is_single_precision)


class _EnumerationFieldClassMappingRange:
    def __init__(self, lower, upper):
        self._lower = lower
        self._upper = upper

    @property
    def lower(self):
        return self._lower

    @property
    def upper(self):
        return self._upper

    def __eq__(self, other):
        return self.lower == other.lower and self.upper == other.upper


class _EnumerationFieldClassMapping(collections.abc.Set):
    def __init__(self, mapping_ptr):
        self._mapping_ptr = mapping_ptr

    @property
    def label(self):
        mapping_ptr = self._as_enumeration_field_class_mapping_ptr(self._mapping_ptr)
        label = native_bt.field_class_enumeration_mapping_get_label(mapping_ptr)
        assert label is not None
        return label

    def __len__(self):
        mapping_ptr = self._as_enumeration_field_class_mapping_ptr(self._mapping_ptr)
        return native_bt.field_class_enumeration_mapping_get_range_count(mapping_ptr)

    def __contains__(self, other_range):
        for curr_range in self:
            if curr_range == other_range:
                return True
        return False

    def __iter__(self):
        for idx in range(len(self)):
            lower, upper = self._get_range_by_index(self._mapping_ptr, idx)
            yield _EnumerationFieldClassMappingRange(lower, upper)


class _UnsignedEnumerationFieldClassMapping(_EnumerationFieldClassMapping):
    _as_enumeration_field_class_mapping_ptr = staticmethod(native_bt.field_class_unsigned_enumeration_mapping_as_mapping_const)
    _get_range_by_index = staticmethod(native_bt.field_class_unsigned_enumeration_mapping_get_range_by_index)


class _SignedEnumerationFieldClassMapping(_EnumerationFieldClassMapping):
    _as_enumeration_field_class_mapping_ptr = staticmethod(native_bt.field_class_signed_enumeration_mapping_as_mapping_const)
    _get_range_by_index = staticmethod(native_bt.field_class_signed_enumeration_mapping_get_range_by_index)


class _EnumerationFieldClass(_IntegerFieldClass, collections.abc.Mapping):
    def __len__(self):
        count = native_bt.field_class_enumeration_get_mapping_count(self._ptr)
        assert(count >= 0)
        return count

    def map_range(self, label, lower, upper=None):
        utils._check_str(label)

        if upper is None:
            upper = lower

        status = self._map_range(self._ptr, label, lower, upper)
        utils._handle_func_status(status,
                                  "cannot add mapping to enumeration field class object")

    def labels_by_value(self, value):
        status, labels = self._get_mapping_labels_by_value(self._ptr, value)
        utils._handle_func_status(status, "cannot get mapping labels")
        return labels

    def __iter__(self):
        for idx in range(len(self)):
            mapping = self._get_mapping_by_index(self._ptr, idx)
            yield mapping.label

    def __getitem__(self, key):
        utils._check_str(key)
        for idx in range(len(self)):
            mapping = self._get_mapping_by_index(self._ptr, idx)
            if mapping.label == key:
                return mapping

        raise KeyError(key)

    def __iadd__(self, mappings):
        for mapping in mappings.values():
            for range in mapping:
                self.map_range(mapping.label, range.lower, range.upper)

        return self


class _UnsignedEnumerationFieldClass(_EnumerationFieldClass, _UnsignedIntegerFieldClass):
    _NAME = 'Unsigned enumeration'

    @staticmethod
    def _get_mapping_by_index(enum_ptr, index):
        mapping_ptr = native_bt.field_class_unsigned_enumeration_borrow_mapping_by_index_const(enum_ptr, index)
        assert mapping_ptr is not None
        return _UnsignedEnumerationFieldClassMapping(mapping_ptr)

    @staticmethod
    def _map_range(enum_ptr, label, lower, upper):
        utils._check_uint64(lower)
        utils._check_uint64(upper)
        return native_bt.field_class_unsigned_enumeration_map_range(enum_ptr, label, lower, upper)

    @staticmethod
    def _get_mapping_labels_by_value(enum_ptr, value):
        utils._check_uint64(value)
        return native_bt.field_class_unsigned_enumeration_get_mapping_labels_by_value(enum_ptr, value)


class _SignedEnumerationFieldClass(_EnumerationFieldClass, _SignedIntegerFieldClass):
    _NAME = 'Signed enumeration'

    @staticmethod
    def _get_mapping_by_index(enum_ptr, index):
        mapping_ptr = native_bt.field_class_signed_enumeration_borrow_mapping_by_index_const(enum_ptr, index)
        assert mapping_ptr is not None
        return _SignedEnumerationFieldClassMapping(mapping_ptr)

    @staticmethod
    def _map_range(enum_ptr, label, lower, upper):
        utils._check_int64(lower)
        utils._check_int64(upper)
        return native_bt.field_class_signed_enumeration_map_range(enum_ptr, label, lower, upper)

    @staticmethod
    def _get_mapping_labels_by_value(enum_ptr, value):
        utils._check_int64(value)
        return native_bt.field_class_signed_enumeration_get_mapping_labels_by_value(enum_ptr, value)


class _StringFieldClass(_FieldClass):
    _NAME = 'String'


class _FieldContainer(collections.abc.Mapping):
    def __len__(self):
        count = self._get_element_count(self._ptr)
        assert count >= 0
        return count

    def __getitem__(self, key):
        if not isinstance(key, str):
            raise TypeError("key should be a 'str' object, got {}".format(key.__class__.__name__))

        ptr = self._borrow_field_class_ptr_by_name(key)

        if ptr is None:
            raise KeyError(key)

        return _create_field_class_from_ptr_and_get_ref(ptr)

    def _borrow_field_class_ptr_by_name(self, key):
        element_ptr = self._borrow_element_by_name(self._ptr, key)
        if element_ptr is None:
            return

        return self._element_borrow_field_class(element_ptr)

    def __iter__(self):
        for idx in range(len(self)):
            element_ptr = self._borrow_element_by_index(self._ptr, idx)
            assert element_ptr is not None

            yield self._element_get_name(element_ptr)

    def _append_element_common(self, name, field_class):
        utils._check_str(name)
        utils._check_type(field_class, _FieldClass)
        status = self._append_element(self._ptr, name, field_class._ptr)
        utils._handle_func_status(status,
                                  "cannot add field to {} field class object".format(self._NAME.lower()))

    def __iadd__(self, fields):
        for name, field_class in fields.items():
            self._append_element_common(name, field_class)

        return self

    def _at_index(self, index):
        utils._check_uint64(index)

        if index < 0 or index >= len(self):
            raise IndexError

        element_ptr = self._borrow_element_by_index(self._ptr, index)
        assert element_ptr is not None

        field_class_ptr = self._element_borrow_field_class(element_ptr)

        return _create_field_class_from_ptr_and_get_ref(field_class_ptr)


class _StructureFieldClass(_FieldClass, _FieldContainer):
    _NAME = 'Structure'
    _borrow_element_by_index = staticmethod(native_bt.field_class_structure_borrow_member_by_index_const)
    _borrow_element_by_name = staticmethod(native_bt.field_class_structure_borrow_member_by_name_const)
    _element_get_name = staticmethod(native_bt.field_class_structure_member_get_name)
    _element_borrow_field_class = staticmethod(native_bt.field_class_structure_member_borrow_field_class_const)
    _get_element_count = staticmethod(native_bt.field_class_structure_get_member_count)
    _append_element = staticmethod(native_bt.field_class_structure_append_member)

    def append_member(self, name, field_class):
        return self._append_element_common(name, field_class)

    def member_at_index(self, index):
        return self._at_index(index)


class _VariantFieldClass(_FieldClass, _FieldContainer):
    _NAME = 'Variant'
    _borrow_element_by_index = staticmethod(native_bt.field_class_variant_borrow_option_by_index_const)
    _borrow_element_by_name = staticmethod(native_bt.field_class_variant_borrow_option_by_name_const)
    _element_get_name = staticmethod(native_bt.field_class_variant_option_get_name)
    _element_borrow_field_class = staticmethod(native_bt.field_class_variant_option_borrow_field_class_const)
    _get_element_count = staticmethod(native_bt.field_class_variant_get_option_count)
    _append_element = staticmethod(native_bt.field_class_variant_append_option)

    def append_option(self, name, field_class):
        return self._append_element_common(name, field_class)

    def option_at_index(self, index):
        return self._at_index(index)

    @property
    def selector_field_path(self):
        ptr = native_bt.field_class_variant_borrow_selector_field_path_const(self._ptr)
        if ptr is None:
            return

        return bt2.field_path._FieldPath._create_from_ptr_and_get_ref(ptr)

    def _set_selector_field_class(self, selector_fc):
        utils._check_type(selector_fc, bt2.field_class._EnumerationFieldClass)
        status = native_bt.field_class_variant_set_selector_field_class(self._ptr, selector_fc._ptr)
        utils._handle_func_status(status,
                                  "cannot set variant selector field type")

    _selector_field_class = property(fset=_set_selector_field_class)


class _ArrayFieldClass(_FieldClass):
    @property
    def element_field_class(self):
        elem_fc_ptr = native_bt.field_class_array_borrow_element_field_class_const(self._ptr)
        return _create_field_class_from_ptr_and_get_ref(elem_fc_ptr)


class _StaticArrayFieldClass(_ArrayFieldClass):
    @property
    def length(self):
        return native_bt.field_class_static_array_get_length(self._ptr)


class _DynamicArrayFieldClass(_ArrayFieldClass):
    @property
    def length_field_path(self):
        ptr = native_bt.field_class_dynamic_array_borrow_length_field_path_const(self._ptr)
        if ptr is None:
            return

        return bt2.field_path._FieldPath._create_from_ptr_and_get_ref(ptr)

    def _set_length_field_class(self, length_fc):
        utils._check_type(length_fc, _UnsignedIntegerFieldClass)
        status = native_bt.field_class_dynamic_array_set_length_field_class(self._ptr, length_fc._ptr)
        utils._handle_func_status(status,
                                  "cannot set dynamic array length field type")

    _length_field_class = property(fset=_set_length_field_class)


_FIELD_CLASS_TYPE_TO_OBJ = {
    native_bt.FIELD_CLASS_TYPE_UNSIGNED_INTEGER: _UnsignedIntegerFieldClass,
    native_bt.FIELD_CLASS_TYPE_SIGNED_INTEGER: _SignedIntegerFieldClass,
    native_bt.FIELD_CLASS_TYPE_REAL: _RealFieldClass,
    native_bt.FIELD_CLASS_TYPE_UNSIGNED_ENUMERATION: _UnsignedEnumerationFieldClass,
    native_bt.FIELD_CLASS_TYPE_SIGNED_ENUMERATION: _SignedEnumerationFieldClass,
    native_bt.FIELD_CLASS_TYPE_STRING: _StringFieldClass,
    native_bt.FIELD_CLASS_TYPE_STRUCTURE: _StructureFieldClass,
    native_bt.FIELD_CLASS_TYPE_STATIC_ARRAY: _StaticArrayFieldClass,
    native_bt.FIELD_CLASS_TYPE_DYNAMIC_ARRAY: _DynamicArrayFieldClass,
    native_bt.FIELD_CLASS_TYPE_VARIANT: _VariantFieldClass,
}
