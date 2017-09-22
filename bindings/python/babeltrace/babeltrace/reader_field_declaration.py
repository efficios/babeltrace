# The MIT License (MIT)
#
# Copyright (c) 2013-2017 Jérémie Galarneau <jeremie.galarneau@efficios.com>
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

import bt2
import babeltrace.common as common


def _create_field_declaration(field_type, name, scope):
    try:
        if type(field_type) == bt2.IntegerFieldType:
            declaration = IntegerFieldDeclaration.__new__(
                IntegerFieldDeclaration)
        elif type(field_type) == bt2.EnumerationFieldType:
            declaration = EnumerationFieldDeclaration.__new__(
                EnumerationFieldDeclaration)
        elif type(field_type) == bt2.ArrayFieldType:
            declaration = ArrayFieldDeclaration.__new__(
                ArrayFieldDeclaration)
        elif type(field_type) == bt2.SequenceFieldType:
            declaration = SequenceFieldDeclaration.__new__(
                SequenceFieldDeclaration)
        elif type(field_type) == bt2.FloatingPointNumberFieldType:
            declaration = FloatFieldDeclaration.__new__(
                FloatFieldDeclaration)
        elif type(field_type) == bt2.StructureFieldType:
            declaration = StructureFieldDeclaration.__new__(
                StructureFieldDeclaration)
        elif type(field_type) == bt2.StringFieldType:
            declaration = StringFieldDeclaration.__new__(
                StringFieldDeclaration)
        elif type(field_type) == bt2.VariantFieldType:
            declaration = VariantFieldDeclaration.__new__(
                VariantFieldDeclaration)
        else:
            return
    except bt2.Error:
        return

    declaration._field_type = field_type
    declaration._name = name
    declaration._scope = scope
    return declaration


class FieldDeclaration:
    """
    Base class for concrete field declarations.

    This class is not meant to be instantiated by the user.
    """

    def __init__(self):
        raise NotImplementedError("FieldDeclaration cannot be instantiated")

    def __repr__(self):
        return "({0}) {1} {2}".format(common.CTFScope.scope_name(self.scope),
                                      common.CTFTypeId.type_name(self.type),
                                      self.name)

    @property
    def name(self):
        """
        Field name, or ``None`` on error.
        """

        return self._name

    @property
    def type(self):
        """
        Field type (one of :class:`babeltrace.common.CTFTypeId`
        constants).
        """

        return _OBJ_TO_TYPE_ID[type(self)]

    @property
    def scope(self):
        """
        Field scope (one of:class:`babeltrace.common.CTFScope`
        constants).
        """

        return self._scope


class IntegerFieldDeclaration(FieldDeclaration):
    """
    Integer field declaration.
    """

    def __init__(self):
        raise NotImplementedError("IntegerFieldDeclaration cannot be instantiated")

    @property
    def signedness(self):
        """
        0 if this integer is unsigned, 1 if signed, or -1 on error.
        """

        try:
            if self._field_type.is_signed:
                return 1
            else:
                return 0
        except bt2.Error:
            return -1

    @property
    def base(self):
        """
        Integer base (:class:`int`), or a negative value on error.
        """

        try:
            return self._field_type.base
        except AssertionError:
            return -1

    @property
    def byte_order(self):
        """
        Integer byte order (one of
        :class:`babeltrace.common.ByteOrder` constants).
        """

        try:
            byte_order = self._field_type.byte_order
        except AssertionError:
            return common.ByteOrder.BYTE_ORDER_UNKNOWN

        try:
            return _BT2_BYTE_ORDER_TO_BYTE_ORDER[byte_order]
        except KeyError:
            return common.ByteOrder.BYTE_ORDER_UNKNOWN

    @property
    def size(self):
        """
        Integer size in bits, or a negative value on error.
        """

        try:
            return self._field_type.size
        except AssertionError:
            return -1

    @property
    def length(self):
        """
        Integer size in bits, or a negative value on error.
        """

        return self.size

    @property
    def encoding(self):
        """
        Integer encoding (one of
        :class:`babeltrace.common.CTFStringEncoding` constants).
        """

        try:
            encoding = self._field_type.encoding
        except bt2.Error:
            return common.CTFStringEncoding.UNKNOWN

        try:
            return _BT2_ENCODING_TO_ENCODING[encoding]
        except KeyError:
            return common.CTFStringEncoding.UNKNOWN


class EnumerationFieldDeclaration(FieldDeclaration):
    """
    Enumeration field declaration.

    .. note::

       As of this version, this class is missing some properties.
    """

    def __init__(self):
        raise NotImplementedError("EnumerationFieldDeclaration cannot be instantiated")


class ArrayFieldDeclaration(FieldDeclaration):
    """
    Static array field declaration.
    """

    def __init__(self):
        raise NotImplementedError("ArrayFieldDeclaration cannot be instantiated")

    @property
    def length(self):
        """
        Fixed length of this static array (number of contained
        elements), or a negative value on error.
        """

        try:
            return self._field_type.length
        except AssertionError:
            return -1

    @property
    def element_declaration(self):
        """
        Field declaration of the underlying element.
        """

        try:
            return _create_field_declaration(
                self._field_type.element_field_type, name=None,
                scope=self._scope)
        except bt2.Error:
            return


class SequenceFieldDeclaration(FieldDeclaration):
    """
    Sequence (dynamic array) field declaration.

    .. note::

       As of this version, this class is missing some properties.
    """

    def __init__(self):
        raise NotImplementedError("SequenceFieldDeclaration cannot be instantiated")

    @property
    def element_declaration(self):
        """
        Field declaration of the underlying element.
        """

        try:
            return _create_field_declaration(
                self._field_type.element_field_type, name=None,
                scope=self._scope)
        except bt2.Error:
            return


class FloatFieldDeclaration(FieldDeclaration):
    """
    Floating point number field declaration.

    .. note::

       As of this version, this class is missing some properties.
    """

    def __init__(self):
        raise NotImplementedError("FloatFieldDeclaration cannot be instantiated")


class StructureFieldDeclaration(FieldDeclaration):
    """
    Structure (ordered map of field names to field declarations) field
    declaration.

    .. note::

       As of this version, this class is missing some properties.
    """

    def __init__(self):
        raise NotImplementedError("StructureFieldDeclaration cannot be instantiated")


class StringFieldDeclaration(FieldDeclaration):
    """
    String (NULL-terminated array of bytes) field declaration.

    .. note::

       As of this version, this class is missing some properties.
    """

    def __init__(self):
        raise NotImplementedError("StringFieldDeclaration cannot be instantiated")


class VariantFieldDeclaration(FieldDeclaration):
    """
    Variant (dynamic selection between different types) field declaration.

    .. note::

       As of this version, this class is missing some properties.
    """

    def __init__(self):
        raise NotImplementedError("VariantFieldDeclaration cannot be instantiated")


_OBJ_TO_TYPE_ID = {
    IntegerFieldDeclaration: common.CTFTypeId.INTEGER,
    FloatFieldDeclaration: common.CTFTypeId.FLOAT,
    EnumerationFieldDeclaration: common.CTFTypeId.ENUM,
    StringFieldDeclaration: common.CTFTypeId.STRING,
    StructureFieldDeclaration: common.CTFTypeId.STRUCT,
    ArrayFieldDeclaration: common.CTFTypeId.ARRAY,
    SequenceFieldDeclaration: common.CTFTypeId.SEQUENCE,
    VariantFieldDeclaration: common.CTFTypeId.VARIANT,
}

_BT2_BYTE_ORDER_TO_BYTE_ORDER = {
    bt2.ByteOrder.NATIVE: common.ByteOrder.BYTE_ORDER_NATIVE,
    bt2.ByteOrder.LITTLE_ENDIAN: common.ByteOrder.BYTE_ORDER_LITTLE_ENDIAN,
    bt2.ByteOrder.BIG_ENDIAN: common.ByteOrder.BYTE_ORDER_BIG_ENDIAN,
    bt2.ByteOrder.NETWORK: common.ByteOrder.BYTE_ORDER_NETWORK,
}

_BT2_ENCODING_TO_ENCODING = {
    bt2.Encoding.NONE: common.CTFStringEncoding.NONE,
    bt2.Encoding.ASCII: common.CTFStringEncoding.ASCII,
    bt2.Encoding.UTF8: common.CTFStringEncoding.UTF8,
}
