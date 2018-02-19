# writer.py
#
# Babeltrace writer interface Python module
#
# Copyright 2012-2017 EfficiOS Inc.
#
# Author: Jérémie Galarneau <jeremie.galarneau@efficios.com>
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
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
# SOFTWARE.

import babeltrace.common as common
import bt2


class EnumerationMapping:
    """
    Mapping from an enumeration label to a range of integers.
    """

    def __init__(self, name, start, end):
        """
        Creates an enumeration mapping, where label *name* is mapped to
        the [*start*, *end*] range of integers (*end* is included).

        Set *start* and *end* to the same value to create an enumeration
        mapping to a single value.
        """

        self._enum_mapping = bt2._EnumerationFieldTypeMapping(self, start, end)

    @property
    def name(self):
        return self._enum_mapping.name

    @property
    def start(self):
        return self._enum_mapping.lower

    @property
    def end(self):
        return self._enum_mapping.upper


class Clock:
    """
    A CTF clock allows the description of the system's clock topology, as
    well as the definition of each clock's parameters.

    :class:`Clock` objects must be registered to a :class:`Writer`
    object (see :meth:`Writer.add_clock`), as well as be registered to
    a :class:`StreamClass` object (see :attr:`StreamClass.clock`).
    """

    def __init__(self, name):
        """
        Creates a default CTF clock named *name*.

        :exc:`ValueError` is raised on error.
        """

        try:
            self._clock = bt2.CtfWriterClock(name)
        except:
            raise ValueError("Invalid clock name.")
        assert self._clock

    @property
    def name(self):
        """
        Clock name.

        Set this attribute to change the clock's name.

        :exc:`ValueError` is raised on error.
        """

        try:
            return self._clock.name
        except:
            raise ValueError("Invalid clock instance.")

    @property
    def description(self):
        """
        Clock description (string).

        Set this attribute to change the clock's description.

        :exc:`ValueError` is raised on error.
        """

        try:
            return self._clock.description
        except:
            raise ValueError("Invalid clock instance.")

    @description.setter
    def description(self, desc):
        try:
            self._clock.description = desc
        except:
            raise ValueError("Invalid clock description.")

    @property
    def frequency(self):
        """
        Clock frequency in Hz (integer).

        Set this attribute to change the clock's frequency.

        :exc:`ValueError` is raised on error.
        """

        try:
            return self._clock.frequency
        except:
            raise ValueError("Invalid clock instance.")

    @frequency.setter
    def frequency(self, freq):
        try:
            self._clock.frequency = freq
        except:
            raise ValueError("Invalid frequency value.")

    @property
    def precision(self):
        """
        Clock precision in clock ticks (integer).

        Set this attribute to change the clock's precision.

        :exc:`ValueError` is raised on error.
        """

        try:
            return self._clock.precision
        except:
            raise ValueError("Invalid clock instance.")

    @precision.setter
    def precision(self, precision):
        try:
            self._clock.precision = precision
        except:
            raise ValueError("Invalid precision value.")

    @property
    def offset_seconds(self):
        """
        Clock offset in seconds since POSIX.1 Epoch (integer).

        Set this attribute to change the clock's offset in seconds.

        :exc:`ValueError` is raised on error.
        """

        try:
            return self._clock.offset.seconds
        except:
            raise ValueError("Invalid clock instance.")

    @offset_seconds.setter
    def offset_seconds(self, offset_s):
        try:
            self._clock.offset = bt2.ClockClassOffset(offset_s,
                                                      self._clock.offset.cycles)
        except:
            raise ValueError("Invalid offset value.")

    @property
    def offset(self):
        """
        Clock offset in ticks since (POSIX.1 Epoch +
        :attr:`offset_seconds`).

        Set this attribute to change the clock's offset.

        :exc:`ValueError` is raised on error.
        """

        try:
            return self._clock.offset.cycles
        except:
            raise ValueError("Invalid clock instance.")

    @offset.setter
    def offset(self, offset):
        try:
            self._clock.offset = bt2.ClockClassOffset(self._clock.offset.seconds,
                                                      offset)
        except:
            raise ValueError("Invalid offset value.")

    @property
    def absolute(self):
        """
        ``True`` if this clock is absolute, i.e. if the clock is a
        global reference across the other clocks of the trace.

        Set this attribute to change the clock's absolute state
        (boolean).

        :exc:`ValueError` is raised on error.
        """

        try:
            return self._clock.is_absolute
        except:
            raise ValueError("Invalid clock instance.")

    @absolute.setter
    def absolute(self, is_absolute):
        try:
            self._clock.is_absolute = is_absolute
        except:
            raise ValueError("Could not set the clock absolute attribute.")

    @property
    def uuid(self):
        """
        Clock UUID (an :class:`uuid.UUID` object).

        Set this attribute to change the clock's UUID.

        :exc:`ValueError` is raised on error.
        """

        try:
            return self._clock.uuid
        except:
            raise ValueError("Invalid clock instance.")

    @uuid.setter
    def uuid(self, uuid):
        uuid_bytes = uuid.bytes

        if len(uuid_bytes) != 16:
            raise ValueError(
                "Invalid UUID provided. UUID length must be 16 bytes")

        try:
            self._clock.uuid = uuid
        except:
            raise ValueError("Invalid clock instance.")

    @property
    def time(self):
        """
        Clock current time; nanoseconds (integer) since clock origin
        (POSIX.1 Epoch + :attr:`offset_seconds` + :attr:`offset`).

        Set this attribute to change the clock's current time.

        :exc:`ValueError` is raised on error.
        """

        raise NotImplementedError("Getter not implemented.")

    @time.setter
    def time(self, time):
        try:
            self._clock.time = time
        except:
            raise ValueError("Invalid time value.")


class IntegerBase:
    """
    Display base of an integer.
    """

    #: Unknown
    UNKNOWN = -1

    #: Binary
    BIN = 2

    #: Octal
    OCT = 8

    #: Decimal
    DEC = 10

    #: Hexadecimal
    HEX = 16

    # keep this for backward compatibility
    INTEGER_BASE_UNKNOWN = -1
    INTEGER_BASE_BINARY = 2
    INTEGER_BASE_OCTAL = 8
    INTEGER_BASE_DECIMAL = 10
    INTEGER_BASE_HEXADECIMAL = 16


_BT2_BYTE_ORDER_TO_BYTE_ORDER = {
    bt2.ByteOrder.NATIVE: common.ByteOrder.BYTE_ORDER_NATIVE,
    bt2.ByteOrder.LITTLE_ENDIAN: common.ByteOrder.BYTE_ORDER_LITTLE_ENDIAN,
    bt2.ByteOrder.BIG_ENDIAN: common.ByteOrder.BYTE_ORDER_BIG_ENDIAN,
    bt2.ByteOrder.NETWORK: common.ByteOrder.BYTE_ORDER_NETWORK,
}

_BYTE_ORDER_TO_BT2_BYTE_ORDER = {
    common.ByteOrder.BYTE_ORDER_NATIVE: bt2.ByteOrder.NATIVE,
    common.ByteOrder.BYTE_ORDER_LITTLE_ENDIAN: bt2.ByteOrder.LITTLE_ENDIAN,
    common.ByteOrder.BYTE_ORDER_BIG_ENDIAN: bt2.ByteOrder.BIG_ENDIAN,
    common.ByteOrder.BYTE_ORDER_NETWORK: bt2.ByteOrder.NETWORK,
}

_BT2_ENCODING_TO_ENCODING = {
    bt2.Encoding.NONE: common.CTFStringEncoding.NONE,
    bt2.Encoding.ASCII: common.CTFStringEncoding.ASCII,
    bt2.Encoding.UTF8: common.CTFStringEncoding.UTF8,
}

_ENCODING_TO_BT2_ENCODING = {
    common.CTFStringEncoding.NONE: bt2.Encoding.NONE,
    common.CTFStringEncoding.ASCII: bt2.Encoding.ASCII,
    common.CTFStringEncoding.UTF8: bt2.Encoding.UTF8,
}


class FieldDeclaration:
    """
    Base class of all field declarations. This class is not meant to
    be instantiated by the user; use one of the concrete field
    declaration subclasses instead.
    """

    class IntegerBase(IntegerBase):
        pass

    def __init__(self):
        if self._field_type is None:
            raise ValueError("FieldDeclaration creation failed.")

    @staticmethod
    def _create_field_declaration(field_type):

        if type(field_type) not in _BT2_FIELD_TYPE_TO_BT_DECLARATION:
            raise TypeError("Invalid field declaration instance.")

        declaration = Field.__new__(Field)
        declaration._field_type = field_type
        declaration.__class__ = _BT2_FIELD_TYPE_TO_BT_DECLARATION[
            type(field_type)]
        return declaration

    @property
    def alignment(self):
        """
        Field alignment in bits (integer).

        Set this attribute to change this field's alignment.

        :exc:`ValueError` is raised on error.
        """

        try:
            return self._field_type.alignment
        except:
            raise ValueError(
                "Could not get alignment field declaration attribute.")

    @alignment.setter
    def alignment(self, alignment):
        try:
            self._field_type.alignment = alignment
        except:
            raise ValueError("Invalid alignment value.")

    @property
    def byte_order(self):
        """
        Field byte order (one of :class:`babeltrace.common.ByteOrder`
        constants).

        Set this attribute to change this field's byte order.

        :exc:`ValueError` is raised on error.
        """

        try:
            return _BT2_BYTE_ORDER_TO_BYTE_ORDER[self._field_type.byte_order]
        except:
            raise ValueError(
                "Could not get byte order field declaration attribute.")

    @byte_order.setter
    def byte_order(self, byte_order):
        try:
            self._field_type.byte_order = _BYTE_ORDER_TO_BT2_BYTE_ORDER[byte_order]
        except:
            raise ValueError("Could not set byte order value.")


class _EncodingProp:
    @property
    def encoding(self):
        """
        Integer encoding (one of
        :class:`babeltrace.common.CTFStringEncoding` constants).

        Set this attribute to change this field's encoding.

        :exc:`ValueError` is raised on error.
        """

        try:
            return _BT2_ENCODING_TO_ENCODING[self._field_type.encoding]
        except:
            raise ValueError("Could not get field encoding.")

    @encoding.setter
    def encoding(self, encoding):
        try:
            self._field_type.encoding = _ENCODING_TO_BT2_ENCODING[encoding]
        except:
            raise ValueError("Could not set field encoding.")


class IntegerFieldDeclaration(FieldDeclaration, _EncodingProp):
    """
    Integer field declaration.
    """

    def __init__(self, size):
        """
        Creates an integer field declaration of size *size* bits.

        :exc:`ValueError` is raised on error.
        """

        self._field_type = bt2.IntegerFieldType(size)
        super().__init__()

    @property
    def size(self):
        """
        Integer size in bits (integer).

        Set this attribute to change this integer's size.

        :exc:`ValueError` is raised on error.
        """

        try:
            return self._field_type.size
        except:
            raise ValueError("Could not get Integer size attribute.")

    @property
    def signed(self):
        """
        ``True`` if this integer is signed.

        Set this attribute to change this integer's signedness
        (boolean).

        :exc:`ValueError` is raised on error.
        """

        try:
            return self._field_type.is_signed
        except:
            raise ValueError("Could not get Integer signed attribute.")

    @signed.setter
    def signed(self, signed):
        try:
            self._field_type.is_signed = signed
        except:
            raise ValueError("Could not set Integer signed attribute.")

    @property
    def base(self):
        """
        Integer display base (one of :class:`IntegerBase` constants).

        Set this attribute to change this integer's display base.

        :exc:`ValueError` is raised on error.
        """

        try:
            return self._field_type.base
        except:
            raise ValueError("Could not get Integer base attribute.")

    @base.setter
    def base(self, base):
        try:
            self._field_type.base = base
        except:
            raise ValueError("Could not set Integer base.")


class EnumerationFieldDeclaration(FieldDeclaration):
    """
    Enumeration field declaration. A CTF enumeration maps labels to
    ranges of integers.
    """

    def __init__(self, integer_type):
        """
        Creates an enumeration field declaration, with *integer_type*
        being the underlying :class:`IntegerFieldDeclaration` for storing
        the integer.

        :exc:`ValueError` is raised on error.
        """

        isinst = isinstance(integer_type, IntegerFieldDeclaration)

        if integer_type is None or not isinst:
            raise TypeError("Invalid integer container.")

        self._field_type = bt2.EnumerationFieldType(integer_type._field_type)
        super().__init__()

    @property
    def container(self):
        """
        Underlying container (:class:`IntegerFieldDeclaration`).

        :exc:`TypeError` is raised on error.
        """

        try:
            return FieldDeclaration._create_field_declaration(
                self._field_type.integer_field_type)
        except:
            raise TypeError("Invalid enumeration declaration")

    def add_mapping(self, name, range_start, range_end):
        """
        Adds a mapping to the enumeration field declaration, from the
        label named *name* to range [*range_start*, *range_end*], where
        *range_start* and *range_end* are integers included in the
        range.

        :exc:`ValueError` is raised on error.
        """

        try:
            self._field_type.add_mapping(name, range_start, range_end)
        except:
            raise ValueError(
                "Could not add mapping to enumeration declaration.")

    @property
    def mappings(self):
        """
        Generates the mappings of this enumeration field declaration
        (:class:`EnumerationMapping` objects).

        :exc:`TypeError` is raised on error.
        """

        for mapping in self._field_type:
            yield EnumerationMapping(mapping.name, mapping.lower,
                                     mapping.upper)

    def get_mapping_by_name(self, name):
        """
        Returns the :class:`EnumerationMapping` object for the label
        named *name*.

        :exc:`TypeError` is raised on error.
        """

        try:
            mappings = list(self._field_type.mappings_by_name(name))
        except:
            raise TypeError(
                'Could not get enumeration mappings by name \'{}\''.format(
                    name))

        if not mappings:
            return None

        mapping = mappings[0]
        return EnumerationMapping(mapping.name, mapping.lower, mapping.upper)

    def get_mapping_by_value(self, value):
        """
        Returns the :class:`EnumerationMapping` object for the value
        *value* (integer).

        :exc:`TypeError` is raised on error.
        """

        try:
            mappings = list(self._field_type.mappings_by_value(value))
        except:
            raise TypeError(
                'Could not get enumeration mappings by value \'{}\''.format(
                    value))

        if not mappings:
            return None

        mapping = mappings[0]
        return EnumerationMapping(mapping.name, mapping.lower, mapping.upper)


class FloatingPointFieldDeclaration(FieldDeclaration):
    """
    Floating point number field declaration.

    A CTF floating point number is a made of three sections: the sign
    bit, the exponent bits, and the mantissa bits. The most significant
    bit of the resulting binary word is the sign bit, and is included
    in the number of mantissa bits.

    For example, the
    `IEEE 754 <http://en.wikipedia.org/wiki/IEEE_floating_point>`_
    single precision floating point number is represented on a 32-bit
    word using an 8-bit exponent (``e``) and a 24-bit mantissa (``m``),
    the latter count including the sign bit (``s``)::

        s eeeeeeee mmmmmmmmmmmmmmmmmmmmmmm

    The IEEE 754 double precision floating point number uses an
    11-bit exponent and a 53-bit mantissa.
    """

    #: IEEE 754 single precision floating point number exponent size
    FLT_EXP_DIG = 8

    #: IEEE 754 double precision floating point number exponent size
    DBL_EXP_DIG = 11

    #: IEEE 754 single precision floating point number mantissa size
    FLT_MANT_DIG = 24

    #: IEEE 754 double precision floating point number mantissa size
    DBL_MANT_DIG = 53

    def __init__(self):
        """
        Creates a floating point number field declaration.

        :exc:`ValueError` is raised on error.
        """

        self._field_type = bt2.FloatingPointNumberFieldType()
        super().__init__()

    @property
    def exponent_digits(self):
        """
        Floating point number exponent section size in bits (integer).

        Set this attribute to change the floating point number's
        exponent section's size. You may use :attr:`FLT_EXP_DIG` or
        :attr:`DBL_EXP_DIG` for IEEE 754 floating point numbers.

        :exc:`ValueError` is raised on error.
        """

        try:
            return self._field_type.exponent_size
        except:
            raise TypeError(
                "Could not get Floating point exponent digit count")

    @exponent_digits.setter
    def exponent_digits(self, exponent_digits):
        try:
            self._field_type.exponent_size = exponent_digits
        except:
            raise ValueError("Could not set exponent digit count.")

    @property
    def mantissa_digits(self):
        """
        Floating point number mantissa section size in bits (integer).

        Set this attribute to change the floating point number's
        mantissa section's size. You may use :attr:`FLT_MANT_DIG` or
        :attr:`DBL_MANT_DIG` for IEEE 754 floating point numbers.

        :exc:`ValueError` is raised on error.
        """

        try:
            return self._field_type.mantissa_size
        except:
            raise TypeError(
                "Could not get Floating point mantissa digit count")

    @mantissa_digits.setter
    def mantissa_digits(self, mantissa_digits):
        try:
            self._field_type.mantissa_size = mantissa_digits
        except:
            raise ValueError("Could not set mantissa digit count.")


class FloatFieldDeclaration(FloatingPointFieldDeclaration):
    pass


class StructureFieldDeclaration(FieldDeclaration):
    """
    Structure field declaration, i.e. an ordered mapping from field
    names to field declarations.
    """

    def __init__(self):
        """
        Creates an empty structure field declaration.

        :exc:`ValueError` is raised on error.
        """

        self._field_type = bt2.StructureFieldType()
        super().__init__()

    def add_field(self, field_type, field_name):
        """
        Appends one :class:`FieldDeclaration` *field_type* named
        *field_name* to the structure's ordered map.

        :exc:`ValueError` is raised on error.
        """

        try:
            self._field_type.append_field(field_name, field_type._field_type)
        except:
            raise ValueError("Could not add field to structure.")

    @property
    def fields(self):
        """
        Generates the (field name, :class:`FieldDeclaration`) pairs
        of this structure.

        :exc:`TypeError` is raised on error.
        """

        for name, field_type in self._field_type.items():
            yield (name,
                   FieldDeclaration._create_field_declaration(
                       field_type))

    def get_field_by_name(self, name):
        """
        Returns the :class:`FieldDeclaration` mapped to the field name
        *name* in this structure.

        :exc:`TypeError` is raised on error.
        """

        if name not in self._field_type:
            msg = "Could not find Structure field with name {}".format(name)
            raise TypeError(msg)

        field_type = self._field_type[name]
        return FieldDeclaration._create_field_declaration(
            field_type)


class VariantFieldDeclaration(FieldDeclaration):
    """
    Variant field declaration.

    A CTF variant is a dynamic selection between different fields.
    The value of a *tag* (a CTF enumeration) determines what is the
    current selected field. All the possible fields must be added to
    its field declaration before using an actual variant field.
    """

    def __init__(self, enum_tag, tag_name):
        """
        Creates an empty variant field declaration with tag field
        declaration *enum_tag* (instance of
        :class:`EnumerationFieldDeclaration`) named *tag_name*
        (string).

        :exc:`ValueError` is raised on error.
        """

        isinst = isinstance(enum_tag, EnumerationFieldDeclaration)
        if enum_tag is None or not isinst:
            raise TypeError("Invalid tag type; must be of type EnumerationFieldDeclaration.")

        self._field_type = bt2.VariantFieldType(tag_name=tag_name,
                                                tag_field_type=enum_tag._field_type)
        super().__init__()

    @property
    def tag_name(self):
        """
        Variant field declaration tag name.

        :exc:`TypeError` is raised on error.
        """

        try:
            self._field_type.tag_name
        except:
            raise TypeError("Could not get Variant tag name")

    @property
    def tag_type(self):
        """
        Variant field declaration tag field declaration
        (:class:`EnumerationFieldDeclaration` object).

        :exc:`TypeError` is raised on error.
        """

        try:
            return FieldDeclaration._create_field_declaration(
                self._field_type.tag_field_type)
        except:
            raise TypeError("Could not get Variant tag type")

    def add_field(self, field_type, field_name):
        """
        Registers the :class:`FieldDeclaration` object *field_type*
        as the variant's selected type when the variant's tag's current
        label is *field_name*.

        :exc:`ValueError` is raised on error.
        """

        try:
            self._field_type.append_field(name=field_name, field_type=field_type._field_type)
        except:
            raise ValueError("Could not add field to variant.")

    @property
    def fields(self):
        """
        Generates the (field name, :class:`FieldDeclaration`) pairs
        of this variant field declaration.

        :exc:`TypeError` is raised on error.
        """

        for name, member in self._field_type.items():
            yield (name, FieldDeclaration._create_field_declaration(member))

    def get_field_by_name(self, name):
        """
        Returns the :class:`FieldDeclaration` selected when the
        variant's tag's current label is *name*.

        :exc:`TypeError` is raised on error.
        """

        if name not in self._field_type:
            raise TypeError(
                'Could not find Variant field with name {}'.format(name))

        field_type = self._field_type[name]
        return FieldDeclaration._create_field_declaration(field_type)

    def get_field_from_tag(self, tag):
        """
        Returns the :class:`FieldDeclaration` selected by the current
        label of the :class:`EnumerationField` *tag*.

        :exc:`TypeError` is raised on error.
        """

        try:
            return create_field(self).field(tag).declaration
        except:
            raise TypeError('Could not get Variant field declaration.')


class ArrayFieldDeclaration(FieldDeclaration):
    """
    Static array field declaration.
    """

    def __init__(self, element_type, length):
        """
        Creates a static array field declaration of *length*
        elements of type *element_type* (:class:`FieldDeclaration`).

        :exc:`ValueError` is raised on error.
        """

        try:
            self._field_type = bt2.ArrayFieldType(element_type._field_type, length)
        except:
            raise ValueError('Failed to create ArrayFieldDeclaration.')
        super().__init__()

    @property
    def element_type(self):
        """
        Type of the elements of this this static array (subclass of
        :class:`FieldDeclaration`).

        :exc:`TypeError` is raised on error.
        """

        try:
            return FieldDeclaration._create_field_declaration(
                self._field_type.element_field_type)
        except:
            raise TypeError("Could not get Array element type")

    @property
    def length(self):
        """
        Length of this static array (integer).

        :exc:`TypeError` is raised on error.
        """

        try:
            return self._field_type.length
        except:
            raise TypeError("Could not get Array length")


class SequenceFieldDeclaration(FieldDeclaration):
    """
    Sequence (dynamic array) field declaration.
    """

    def __init__(self, element_type, length_field_name):
        """
        Creates a sequence field declaration of
        elements of type *element_type* (:class:`FieldDeclaration`).
        The length of a sequence field based on this sequence field
        declaration is obtained by retrieving the dynamic integer
        value of the field named *length_field_name*.

        :exc:`ValueError` is raised on error.
        """

        try:
            self._field_type = bt2.SequenceFieldType(element_type, length_field_name)
        except:
            raise ValueError('Failed to create SequenceFieldDeclaration.')
        super().__init__()

    @property
    def element_type(self):
        """
        Type of the elements of this sequence (subclass of
        :class:`FieldDeclaration`).

        :exc:`TypeError` is raised on error.
        """

        try:
            return FieldDeclaration._create_field_declaration(
                self._field_type.element_field_type)
        except:
            raise TypeError("Could not get Sequence element type")

    @property
    def length_field_name(self):
        """
        Name of the integer field defining the dynamic length of
        sequence fields based on this sequence field declaration.

        :exc:`TypeError` is raised on error.
        """

        try:
            return self._field_type.length_name
        except:
            raise TypeError("Could not get Sequence element type")


class StringFieldDeclaration(FieldDeclaration, _EncodingProp):
    """
    String (NULL-terminated array of bytes) field declaration.
    """

    def __init__(self):
        """
        Creates a string field declaration.

        :exc:`ValueError` is raised on error.
        """

        self._field_type = bt2.StringFieldType()
        super().__init__()


@staticmethod
def create_field(field_type):
    """
    Create an instance of a field.
    """
    isinst = isinstance(field_type, FieldDeclaration)

    if field_type is None or not isinst:
        raise TypeError("Invalid field_type. Type must be a FieldDeclaration-derived class.")

    if isinstance(field_type, IntegerFieldDeclaration):
        return IntegerField(field_type)
    elif isinstance(field_type, EnumerationFieldDeclaration):
        return EnumerationField(field_type)
    elif isinstance(field_type, FloatFieldDeclaration):
        return FloatingPointField(field_type)
    elif isinstance(field_type, StructureFieldDeclaration):
        return StructureField(field_type)
    elif isinstance(field_type, VariantFieldDeclaration):
        return VariantField(field_type)
    elif isinstance(field_type, ArrayFieldDeclaration):
        return ArrayField(field_type)
    elif isinstance(field_type, SequenceFieldDeclaration):
        return SequenceField(field_type)
    elif isinstance(field_type, StringFieldDeclaration):
        return StringField(field_type)


class Field:
    """
    Base class of all fields. This class is not meant to be
    instantiated by the user, and neither are its subclasses. Use
    :meth:`Event.payload` to access specific, concrete fields of
    an event.
    """

    def __init__(self, field_type):
        if not isinstance(field_type, FieldDeclaration):
            raise TypeError("Invalid field_type argument.")

        try:
            self._f = field_type._field_type()
        except:
            raise ValueError("Field creation failed.")

    @staticmethod
    def _create_field(bt2_field):
        type_dict = {
            bt2._IntegerField: IntegerField,
            bt2._FloatingPointNumberField: FloatingPointField,
            bt2._EnumerationField: EnumerationField,
            bt2._StringField: StringField,
            bt2._StructureField: StructureField,
            bt2._VariantField: VariantField,
            bt2._ArrayField: ArrayField,
            bt2._SequenceField: SequenceField
        }

        if type(bt2_field) not in type_dict:
            raise TypeError("Invalid field instance.")

        field = Field.__new__(Field)
        field._f = bt2_field
        field.__class__ = type_dict[type(bt2_field)]

        return field

    @property
    def declaration(self):
        """
        Field declaration (subclass of :class:`FieldDeclaration`).

        :exc:`TypeError` is raised on error.
        """

        return FieldDeclaration._create_field_declaration(self._f.field_type)


class IntegerField(Field):
    """
    Integer field, based on an :class:`IntegerFieldDeclaration` object.
    """

    @property
    def value(self):
        """
        Integer value (:class:`int`).

        Set this attribute to change the integer field's value.

        :exc:`ValueError` or :exc:`TypeError` are raised on error.
        """

        try:
            return int(self._f)
        except:
            ValueError('Could not get integer field value.')

    @value.setter
    def value(self, value):
        try:
            self._f.value = value
        except:
            raise ValueError("Could not set integer field value.")


class EnumerationField(Field):
    """
    Enumeration field, based on an
    :class:`EnumerationFieldDeclaration` object.
    """

    @property
    def container(self):
        """
        Underlying container (:class:`IntegerField`).

        :exc:`TypeError` is raised on error.
        """

        try:
            return Field._create_field(self._f.integer_field)
        except:
            raise TypeError("Invalid enumeration field type.")

    @property
    def value(self):
        """
        Current label of this enumeration field (:class:`str`).

        Set this attribute to an integer (:class:`int`) to change the
        enumeration field's value.

        :exc:`ValueError` is raised on error.
        """

        try:
            bt2_enum_ft = self._f.field_type
            mappings = list(bt2_enum_ft.mappings_by_value(self._f.value))
            return mappings[0].name
        except:
            raise ValueError("Could not get enumeration mapping name.")

    @value.setter
    def value(self, value):
        if not isinstance(value, int):
            raise TypeError("EnumerationField value must be an int")

        self._f.value = value


class FloatingPointField(Field):
    """
    Floating point number field, based on a
    :class:`FloatingPointFieldDeclaration` object.
    """

    @property
    def value(self):
        """
        Floating point number value (:class:`float`).

        Set this attribute to change the floating point number field's
        value.

        :exc:`ValueError` or :exc:`TypeError` are raised on error.
        """

        try:
            float(self._f)
        except:
            raise ValueError("Could not get floating point field value.")

    @value.setter
    def value(self, value):
        try:
            self._f.value = value
        except:
            raise ValueError("Could not set floating point field value.")


# This class is provided to ensure backward-compatibility since a stable
# release publicly exposed this... abomination.
class FloatFieldingPoint(FloatingPointField):
    pass


class StructureField(Field):
    """
    Structure field, based on a
    :class:`StructureFieldDeclaration` object.
    """

    def field(self, field_name):
        """
        Returns the structure :class:`Field` named *field_name*.

        :exc:`ValueError` is raised on error.
        """

        try:
            return Field._create_field(self._f[field_name])
        except:
            raise ValueError("Invalid field_name provided.")


class VariantField(Field):
    """
    Variant field, based on a
    :class:`VariantFieldDeclaration` object.
    """

    def field(self, tag):
        """
        Returns the :class:`Field` selected by the current label of
        *tag* (:class:`EnumerationField`).

        :exc:`ValueError` is raised on error.
        """

        try:
            return Field._create_field(self._f.field(tag._f))
        except:
            raise ValueError("Invalid tag provided.")


class ArrayField(Field):
    """
    Static array field, based on an
    :class:`ArrayFieldDeclaration` object.
    """

    def field(self, index):
        """
        Returns the :class:`Field` at index *index* in this static
        array.

        :exc:`IndexError` is raised on error.
        """

        try:
            return Field._create_field(self._f[index])
        except:
            raise IndexError("Invalid index provided.")


class SequenceField(Field):
    """
    Sequence (dynamic array) field, based on a
    :class:`SequenceFieldDeclaration` object.
    """

    @property
    def length(self):
        """
        Sequence length (:class:`IntegerField`).

        Set this attribute to change the sequence length's integer
        field (integer must be unsigned).

        :exc:`ValueError` or :exc:`TypeError` are raised on error.
        """

        try:
            length_field = self._f.length_field
            if length_field is None:
                return
            return Field._create_field(length_field)
        except:
            raise ValueError('Invalid sequence field.')

    @length.setter
    def length(self, length_field):
        if not isinstance(length_field, IntegerField):
            raise TypeError("Invalid length field.")

        if length_field.declaration.signed:
            raise TypeError("Sequence field length must be unsigned")

        try:
            self._f.length_field = length_field._f
        except:
            raise ValueError("Could not set sequence length.")

    def field(self, index):
        """
        Returns the :class:`Field` at index *index* in this sequence.

        :exc:`ValueError` is raised on error.
        """

        try:
            return Field._create_field(self._f[index])
        except:
            raise IndexError("Invalid index provided.")


class StringField(Field):
    """
    String (NULL-terminated array of bytes) field.
    """

    @property
    def value(self):
        """
        String value (:class:`str`).

        Set this attribute to change the string's value.

        :exc:`ValueError` or :exc:`TypeError` are raised on error.
        """

        try:
            str(self._f)
        except:
            raise ValueError('Could not get string field value.')

    @value.setter
    def value(self, value):
        try:
            self._f.value = value
        except:
            raise ValueError("Could not set string field value.")


class EventClass:
    """
    An event class contains the properties of specific
    events (:class:`Event`). Any concrete event must be linked with an
    :class:`EventClass`.

    Some attributes are automatically set when creating an event class.
    For example, if no numeric ID is explicitly set using the
    :attr:`id` attribute, a default, unique ID within the stream class
    containing this event class will be created when needed.
    """

    def __init__(self, name):
        """
        Creates an event class named *name*.

        :exc:`ValueError` is raised on error.
        """

        self._ec = bt2.EventClass(name)

        if self._ec is None:
            raise ValueError("Event class creation failed.")

    def add_field(self, field_type, field_name):
        """
        Adds a field declaration *field_type* named *field_name* to
        this event class.

        *field_type* must be one of:

        * :class:`IntegerFieldDeclaration`
        * :class:`FloatingPointFieldDeclaration`
        * :class:`EnumerationFieldDeclaration`
        * :class:`StringFieldDeclaration`
        * :class:`ArrayFieldDeclaration`
        * :class:`SequenceFieldDeclaration`
        * :class:`StructureFieldDeclaration`
        * :class:`VariantFieldDeclaration`

        :exc:`ValueError` is raised on error.
        """

        try:
            self._ec.payload_field_type.append_field(field_name,
                                                     field_type._field_type)
        except:
            raise ValueError("Could not add field to event class.")

    @property
    def name(self):
        """
        Event class' name.
        """

        try:
            return self._ec.name
        except:
            raise TypeError("Could not get EventClass name")

    @property
    def id(self):
        """
        Event class' numeric ID.

        Set this attribute to assign a numeric ID to this event class.
        This ID must be unique amongst all the event class IDs of a
        given stream class.

        :exc:`TypeError` is raised on error.
        """

        try:
            return self._ec.id
        except:
            raise TypeError("Could not get EventClass id")

    @id.setter
    def id(self, id):
        try:
            self._ec.id = id
        except:
            raise TypeError("Can't change an EventClass id after it has been assigned to a stream class.")

    @property
    def stream_class(self):
        """
        :class:`StreamClass` object containing this event class,
        or ``None`` if not set.
        """

        bt2_stream_class = self._ec.stream_class
        if bt2_stream_class is None:
            return

        stream_class = StreamClass.__new__(StreamClass)
        stream_class._stream_class = bt2_stream_class

        return stream_class

    @property
    def fields(self):
        """
        Generates the (field name, :class:`FieldDeclaration`) pairs of
        this event class.

        :exc:`TypeError` is raised on error.
        """

        return FieldDeclaration._create_field_declaration(
            self._ec.payload_field_type).fields

    def get_field_by_name(self, name):
        """
        Returns the :class:`FieldDeclaration` object named *name* in
        this event class.

        :exc:`TypeError` is raised on error.
        """

        return FieldDeclaration._create_field_declaration(
            self._ec.payload_field_type)[name]


class Event:
    """
    Events are specific instances of event classes
    (:class:`EventClass`), which means they may contain actual,
    concrete field values.
    """

    def __init__(self, event_class):
        """
        Creates an event linked with the :class:`EventClass`
        *event_class*.

        :exc:`ValueError` is raised on error.
        """

        if not isinstance(event_class, EventClass):
            raise TypeError("Invalid event_class argument.")

        try:
            self._e = event_class._ec()
        except:
            raise ValueError("Event creation failed.")

    @property
    def event_class(self):
        """
        :class:`EventClass` object to which this event is linked.
        """

        event_class = EventClass.__new__(EventClass)
        event_class._ec = self._e.event_class
        return event_class

    def clock(self):
        """
        :class:`Clock` object used by this object, or ``None`` if
        the event class is not registered to a stream class.
        """

        try:
            bt2_clock = self._e.event_class.stream_class.clock
        except:
            return

        clock = Clock.__new__(Clock)
        clock._c = bt2_clock
        return clock

    def payload(self, field_name):
        """
        Returns the :class:`Field` object named *field_name* in this
        event.

        The returned field object is created using the event class'
        field declaration named *field_name*.

        The return type is one of:

        * :class:`IntegerField`
        * :class:`FloatingPointField`
        * :class:`EnumerationField`
        * :class:`StringField`
        * :class:`ArrayField`
        * :class:`SequenceField`
        * :class:`StructureField`
        * :class:`VariantField`

        :exc:`TypeError` is raised on error.
        """

        try:
            return Field._create_field(self._e.payload_field[field_name])
        except:
            raise TypeError('Could not get field from event.')

    def set_payload(self, field_name, value_field):
        """
        Set the event's field named *field_name* to the manually
        created :class:`Field` object *value_field*.

        *value_field*'s type must be one of:

        * :class:`IntegerField`
        * :class:`FloatingPointField`
        * :class:`EnumerationField`
        * :class:`StringField`
        * :class:`ArrayField`
        * :class:`SequenceField`
        * :class:`StructureField`
        * :class:`VariantField`

        :exc:`ValueError` is raised on error.
        """

        if not isinstance(value_field, Field):
            raise TypeError("Invalid value type.")

        try:
            self._e.payload_field[field_name] = value_field._f
        except:
            raise ValueError("Could not set event field payload.")

    @property
    def stream_context(self):
        """
        Stream event context field (instance of
        :class:`StructureField`).

        Set this attribute to assign a stream event context field
        to this stream.

        :exc:`ValueError` is raised on error.
        """

        try:
            return Field._create_field(self._e.context_field)
        except:
            raise ValueError("Invalid Stream.")

    @stream_context.setter
    def stream_context(self, field):
        if not isinstance(field, StructureField):
            raise TypeError("Argument field must be of type StructureField")

        try:
            self._e.context_field = field._f
        except:
            raise ValueError("Invalid stream context field.")


class StreamClass:
    """
    A stream class contains the properties of specific
    streams (:class:`Stream`). Any concrete stream must be linked with
    a :class:`StreamClass`, usually by calling
    :meth:`Writer.create_stream`.

    Some attributes are automatically set when creating a stream class.
    For example, if no clock is explicitly set using the
    :attr:`clock` attribute, a default clock will be created
    when needed.
    """

    def __init__(self, name):
        """
        Creates a stream class named *name*.

        :exc:`ValueError` is raised on error.
        """

        try:
            # Set default event header and packet context.
            event_header_type = bt2.StructureFieldType()
            uint32_ft = bt2.IntegerFieldType(32)
            uint64_ft = bt2.IntegerFieldType(64)
            event_header_type.append_field('id', uint32_ft)
            event_header_type.append_field('timestamp', uint64_ft)

            packet_context_type = bt2.StructureFieldType()
            packet_context_type.append_field('timestamp_begin', uint64_ft)
            packet_context_type.append_field('timestamp_end', uint64_ft)
            packet_context_type.append_field('content_size', uint64_ft)
            packet_context_type.append_field('packet_size', uint64_ft)
            packet_context_type.append_field('events_discarded', uint64_ft)
            sc = bt2.StreamClass(name,
                                 event_header_field_type=event_header_type,
                                 packet_context_field_type=packet_context_type)
            self._stream_class = sc
        except:
            raise ValueError("Stream class creation failed.")

    @property
    def name(self):
        """
        Stream class' name.

        :exc:`TypeError` is raised on error.
        """

        try:
            return self._stream_class.name
        except:
            raise TypeError("Could not get StreamClass name")

    @property
    def clock(self):
        """
        Stream class' clock (:class:`Clock` object).

        Set this attribute to change the clock of this stream class.

        :exc:`ValueError` is raised on error.
        """

        if self._stream_class.clock is None:
            return

        clock = Clock.__new__(Clock)
        clock._c = self._stream_class.clock
        return clock

    @clock.setter
    def clock(self, clock):
        if not isinstance(clock, Clock):
            raise TypeError("Invalid clock type.")

        try:
            self._stream_class.clock = clock._clock
        except:
            raise ValueError("Could not set stream class clock.")

    @property
    def id(self):
        """
        Stream class' numeric ID.

        Set this attribute to change the ID of this stream class.

        :exc:`ValueError` is raised on error.
        """

        try:
            return self._stream_class.id
        except:
            raise TypeError("Could not get StreamClass id")

    @id.setter
    def id(self, id):
        try:
            self._stream_class.id = id
        except:
            raise TypeError("Could not set stream class id.")

    @property
    def event_classes(self):
        """
        Generates the event classes (:class:`EventClass` objects) of
        this stream class.

        :exc:`TypeError` is raised on error.
        """

        for bt2_ec in self._stream_class.values():
            event_class = EventClass.__new__(EventClass)
            event_class._ec = bt2_ec
            yield event_class

    def add_event_class(self, event_class):
        """
        Registers the :class:`EventClass` *event_class* to this stream
        class.

        Once the event class is registered, it will be generated as one
        of the event classes generated by :attr:`event_classes`.

        :exc:`ValueError` is raised on error.
        """

        if not isinstance(event_class, EventClass):
            raise TypeError("Invalid event_class type.")

        try:
            self._stream_class.add_event_class(event_class._ec)
        except:
            raise ValueError("Could not add event class.")

    @property
    def packet_context_type(self):
        """
        Stream packet context declaration.

        Set this attribute to change the stream packet context
        declaration (must be an instance of
        :class:`StructureFieldDeclaration`).

        :exc:`ValueError` is raised on error.

        """

        try:
            bt2_field_type = self._stream_class.packet_context_field_type
            if bt2_field_type is None:
                raise ValueError("Invalid StreamClass")

            field_type = FieldDeclaration._create_field_declaration(
                bt2_field_type)
            return field_type
        except:
            raise ValueError("Invalid StreamClass")

    @packet_context_type.setter
    def packet_context_type(self, field_type):
        if field_type is not None and not isinstance(field_type,
                                                     StructureFieldDeclaration):
            raise TypeError("field_type argument must be of type StructureFieldDeclaration.")

        bt2_field_type = None if field_type is None else field_type._field_type
        try:
            self._stream_class.packet_context_field_type = bt2_field_type
        except:
            raise ValueError("Failed to set packet context type.")

    @property
    def event_context_type(self):
        """
        Stream event context declaration.

        Set this attribute to change the stream event context
        declaration (must be an instance of
        :class:`StructureFieldDeclaration`).

        :exc:`ValueError` is raised on error.

        """

        try:
            bt2_field_type = self._stream_class.event_context_field_type
            if bt2_field_type is None:
                raise ValueError("Invalid StreamClass")

            field_type = FieldDeclaration._create_field_declaration(
                bt2_field_type)
            return field_type
        except:
            raise ValueError("Invalid StreamClass")

    @event_context_type.setter
    def event_context_type(self, field_type):
        if field_type is not None and not isinstance(field_type,
                                                     StructureFieldDeclaration):
            raise TypeError("field_type argument must be of type StructureFieldDeclaration.")

        bt2_field_type = None if field_type is None else field_type._field_type
        try:
            self._stream_class.event_context_field_type = bt2_field_type
        except:
            raise ValueError("Failed to set event context type.")


class Stream:
    """
    Streams are specific instances of stream classes, which means they
    may contain actual, concrete events.

    :class:`Stream` objects are returned by
    :meth:`Writer.create_stream`; they are not meant to be
    instantiated by the user.

    Concrete :class:`Event` objects are appended to
    :class:`Stream` objects using :meth:`append_event`.

    When :meth:`flush` is called, a CTF packet is created, containing
    all the appended events since the last flush. Although the stream
    is flushed on object destruction, it is **strongly recommended**
    that the user call :meth:`flush` manually before exiting the
    script, as :meth:`__del__` is not always reliable.
    """

    def __init__(self):
        raise NotImplementedError("Stream cannot be instantiated; use Writer.create_stream()")

    @property
    def discarded_events(self):
        """
        Number of discarded (lost) events in this stream so far.

        :exc:`ValueError` is raised on error.
        """

        try:
            return self._s.discarded_events_count
        except:
            raise ValueError("Could not get the stream discarded events count")

    def append_discarded_events(self, event_count):
        """
        Appends *event_count* discarded events to this stream.
        """

        self._s.append_discarded_events(event_count)

    def append_event(self, event):
        """
        Appends event *event* (:class:`Event` object) to this stream.

        The stream's associated clock will be sampled during this call.
        *event* **shall not** be modified after being appended to this
        stream.

        :exc:`ValueError` is raised on error.
        """

        try:
            self._s.append_event(event._e)
        except:
            raise ValueError("Could not append event to stream.")

    @property
    def packet_context(self):
        """
        Stream packet context field (instance of
        :class:`StructureField`).

        Set this attribute to assign a stream packet context field
        to this stream.

        :exc:`ValueError` is raised on error.
        """

        bt2_field = self._s.packet_context_field
        if bt2_field is None:
            raise ValueError("Invalid Stream.")

        return Field._create_field(bt2_field)

    @packet_context.setter
    def packet_context(self, field):
        if not isinstance(field, StructureField):
            raise TypeError("Argument field must be of type StructureField")

        try:
            self._s.packet_context_field = field._f
        except:
            raise ValueError("Invalid packet context field.")

    def flush(self):
        """
        Flushes the current packet of this stream to disk. Events
        subsequently appended to the stream will be added to a new
        packet.

        :exc:`ValueError` is raised on error.
        """

        try:
            self._s.flush()
        except:
            raise ValueError('Failed to flush CTF writer stream')


class Writer:
    """
    This object is the CTF writer API context. It oversees its streams
    and clocks, and is responsible for writing one CTF trace.
    """

    def __init__(self, path):
        """
        Creates a CTF writer, initializing a new CTF trace at path
        *path*.

        *path* must be an existing directory, since a CTF trace is
        made of multiple files.

        :exc:`ValueError` is raised if the creation fails.
        """

        try:
            self._w = bt2.CtfWriter(path)
        except:
            raise ValueError("Writer creation failed.")

    def create_stream(self, stream_class):
        """
        Creates and registers a new stream based on stream class
        *stream_class*.

        This is the standard way of creating a :class:`Stream` object:
        the user is not allowed to instantiate this class.

        Returns a new :class:`Stream` object.
        """

        if not isinstance(stream_class, StreamClass):
            raise TypeError("Invalid stream_class type.")

        if stream_class._stream_class.trace is None:
            self._w.trace.add_stream_class(stream_class._stream_class)

        stream = Stream.__new__(Stream)
        stream._s = stream_class._stream_class()
        stream.__class__ = Stream
        return stream

    def add_environment_field(self, name, value):
        """
        Sets the CTF environment variable named *name* to value *value*
        (converted to a string).

        :exc:`ValueError` or `TypeError` is raised on error.
        """

        if type(name) != str:
            raise TypeError("Field name must be a string.")

        t = type(value)

        if type(value) != str and type(value) != int:
            raise TypeError("Value type is not supported.")

        try:
            self._w.trace.env[name] = value
        except:
            raise ValueError("Could not add environment field to trace.")

    def add_clock(self, clock):
        """
        Registers :class:`Clock` object *clock* to the writer.

        You *must* register CTF clocks assigned to stream classes
        to the writer.

        :exc:`ValueError` is raised if the creation fails.
        """

        try:
            self._w.add_clock(clock._clock)
        except:
            raise ValueError("Could not add clock to Writer.")

    @property
    def metadata(self):
        """
        Current metadata of this trace (:class:`str`).
        """

        return self._w.metadata_string

    def flush_metadata(self):
        """
        Flushes the trace's metadata to the metadata file.
        """

        self._w.flush_metadata()

    @property
    def byte_order(self):
        """
        Native byte order of this trace (one of
        :class:`babeltrace.common.ByteOrder` constants).

        This is the actual byte order that is used when a field
        declaration has the
        :attr:`babeltrace.common.ByteOrder.BYTE_ORDER_NATIVE`
        value.

        Set this attribute to change the trace's native byte order.

        Defaults to the host machine's endianness.

        :exc:`ValueError` is raised on error.
        """
        raise NotImplementedError("Getter not implemented.")

    @byte_order.setter
    def byte_order(self, byte_order):
        try:
            self._w.trace.native_byte_order = _BYTE_ORDER_TO_BT2_BYTE_ORDER[byte_order]
        except:
            raise ValueError("Could not set trace byte order.")


_BT2_FIELD_TYPE_TO_BT_DECLARATION = {
    bt2.IntegerFieldType: IntegerFieldDeclaration,
    bt2.FloatingPointNumberFieldType: FloatFieldDeclaration,
    bt2.EnumerationFieldType: EnumerationFieldDeclaration,
    bt2.StringFieldType: StringFieldDeclaration,
    bt2.StructureFieldType: StructureFieldDeclaration,
    bt2.ArrayFieldType: ArrayFieldDeclaration,
    bt2.SequenceFieldType: SequenceFieldDeclaration,
    bt2.VariantFieldType: VariantFieldDeclaration,
}
