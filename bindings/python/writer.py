import babeltrace.nativebt as nbt
import babeltrace.common as common
from uuid import UUID


# Used to compare to -1ULL in error checks
_MAX_UINT64 = 0xFFFFFFFFFFFFFFFF


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

        self.name = name
        self.start = start
        self.end = end


class Clock:
    def __init__(self, name):
        self._c = nbt._bt_ctf_clock_create(name)

        if self._c is None:
            raise ValueError("Invalid clock name.")

    def __del__(self):
        nbt._bt_ctf_clock_put(self._c)

    @property
    def name(self):
        """
        Get the clock's name.
        """

        name = nbt._bt_ctf_clock_get_name(self._c)

        if name is None:
            raise ValueError("Invalid clock instance.")

        return name

    @property
    def description(self):
        """
        Get the clock's description. None if unset.
        """

        return nbt._bt_ctf_clock_get_description(self._c)

    @description.setter
    def description(self, desc):
        """
        Set the clock's description. The description appears in the clock's TSDL
        meta-data.
        """

        ret = nbt._bt_ctf_clock_set_description(self._c, str(desc))

        if ret < 0:
            raise ValueError("Invalid clock description.")

    @property
    def frequency(self):
        """
        Get the clock's frequency (Hz).
        """

        freq = nbt._bt_ctf_clock_get_frequency(self._c)

        if freq == _MAX_UINT64:
            raise ValueError("Invalid clock instance")

        return freq

    @frequency.setter
    def frequency(self, freq):
        """
        Set the clock's frequency (Hz).
        """

        ret = nbt._bt_ctf_clock_set_frequency(self._c, freq)

        if ret < 0:
            raise ValueError("Invalid frequency value.")

    @property
    def precision(self):
        """
        Get the clock's precision (in clock ticks).
        """

        precision = nbt._bt_ctf_clock_get_precision(self._c)

        if precision == _MAX_UINT64:
            raise ValueError("Invalid clock instance")

        return precision

    @precision.setter
    def precision(self, precision):
        """
        Set the clock's precision (in clock ticks).
        """

        ret = nbt._bt_ctf_clock_set_precision(self._c, precision)

    @property
    def offset_seconds(self):
        """
        Get the clock's offset in seconds from POSIX.1 Epoch.
        """

        offset_s = nbt._bt_ctf_clock_get_offset_s(self._c)

        if offset_s == _MAX_UINT64:
            raise ValueError("Invalid clock instance")

        return offset_s

    @offset_seconds.setter
    def offset_seconds(self, offset_s):
        """
        Set the clock's offset in seconds from POSIX.1 Epoch.
        """

        ret = nbt._bt_ctf_clock_set_offset_s(self._c, offset_s)

        if ret < 0:
            raise ValueError("Invalid offset value.")

    @property
    def offset(self):
        """
        Get the clock's offset in ticks from POSIX.1 Epoch + offset in seconds.
        """

        offset = nbt._bt_ctf_clock_get_offset(self._c)

        if offset == _MAX_UINT64:
            raise ValueError("Invalid clock instance")

        return offset

    @offset.setter
    def offset(self, offset):
        """
        Set the clock's offset in ticks from POSIX.1 Epoch + offset in seconds.
        """

        ret = nbt._bt_ctf_clock_set_offset(self._c, offset)

        if ret < 0:
            raise ValueError("Invalid offset value.")

    @property
    def absolute(self):
        """
        Get a clock's absolute attribute. A clock is absolute if the clock
        is a global reference across the trace's other clocks.
        """

        is_absolute = nbt._bt_ctf_clock_get_is_absolute(self._c)

        if is_absolute == -1:
            raise ValueError("Invalid clock instance")

        return False if is_absolute == 0 else True

    @absolute.setter
    def absolute(self, is_absolute):
        """
        Set a clock's absolute attribute. A clock is absolute if the clock
        is a global reference across the trace's other clocks.
        """

        ret = nbt._bt_ctf_clock_set_is_absolute(self._c, int(is_absolute))

        if ret < 0:
            raise ValueError("Could not set the clock's absolute attribute.")

    @property
    def uuid(self):
        """
        Get a clock's UUID (an object of type UUID).
        """

        uuid_list = []

        for i in range(16):
            ret, value = nbt._bt_python_ctf_clock_get_uuid_index(self._c, i)

            if ret < 0:
                raise ValueError("Invalid clock instance")

            uuid_list.append(value)

        return UUID(bytes=bytes(uuid_list))

    @uuid.setter
    def uuid(self, uuid):
        """
        Set a clock's UUID (an object of type UUID).
        """

        uuid_bytes = uuid.bytes

        if len(uuid_bytes) != 16:
            raise ValueError("Invalid UUID provided. UUID length must be 16 bytes")

        for i in range(len(uuid_bytes)):
            ret = nbt._bt_python_ctf_clock_set_uuid_index(self._c, i,
                                                          uuid_bytes[i])

            if ret < 0:
                raise ValueError("Invalid clock instance")

    @property
    def time(self):
        """
        Get the current time in nanoseconds since the clock's origin (offset and
        offset_s attributes).
        """

        time = nbt._bt_ctf_clock_get_time(self._c)

        if time == _MAX_UINT64:
            raise ValueError("Invalid clock instance")

        return time

    @time.setter
    def time(self, time):
        """
        Set the current time in nanoseconds since the clock's origin (offset and
        offset_s attributes). The clock's value will be sampled as events are
        appended to a stream.
        """

        ret = nbt._bt_ctf_clock_set_time(self._c, time)

        if ret < 0:
            raise ValueError("Invalid time value.")


class FieldDeclaration:
    """
    FieldDeclaration should not be instantiated directly. Instantiate
    one of the concrete FieldDeclaration classes.
    """

    class IntegerBase:
        # These values are based on the bt_ctf_integer_base enum
        # declared in event-types.h.
        INTEGER_BASE_UNKNOWN = -1
        INTEGER_BASE_BINARY = 2
        INTEGER_BASE_OCTAL = 8
        INTEGER_BASE_DECIMAL = 10
        INTEGER_BASE_HEXADECIMAL = 16

    def __init__(self):
        if self._ft is None:
            raise ValueError("FieldDeclaration creation failed.")

    def __del__(self):
        nbt._bt_ctf_field_type_put(self._ft)

    @staticmethod
    def _create_field_declaration_from_native_instance(
            native_field_declaration):
        type_dict = {
            common.CTFTypeId.INTEGER: IntegerFieldDeclaration,
            common.CTFTypeId.FLOAT: FloatFieldDeclaration,
            common.CTFTypeId.ENUM: EnumerationFieldDeclaration,
            common.CTFTypeId.STRING: StringFieldDeclaration,
            common.CTFTypeId.STRUCT: StructureFieldDeclaration,
            common.CTFTypeId.VARIANT: VariantFieldDeclaration,
            common.CTFTypeId.ARRAY: ArrayFieldDeclaration,
            common.CTFTypeId.SEQUENCE: SequenceFieldDeclaration
        }

        field_type_id = nbt._bt_ctf_field_type_get_type_id(native_field_declaration)

        if field_type_id == common.CTFTypeId.UNKNOWN:
            raise TypeError("Invalid field instance")

        declaration = Field.__new__(Field)
        declaration._ft = native_field_declaration
        declaration.__class__ = type_dict[field_type_id]

        return declaration

    @property
    def alignment(self):
        """
        Get the field declaration's alignment. Returns -1 on error.
        """

        return nbt._bt_ctf_field_type_get_alignment(self._ft)

    @alignment.setter
    def alignment(self, alignment):
        """
        Set the field declaration's alignment. Defaults to 1 (bit-aligned). However,
        some types, such as structures and string, may impose other alignment
        constraints.
        """

        ret = nbt._bt_ctf_field_type_set_alignment(self._ft, alignment)

        if ret < 0:
            raise ValueError("Invalid alignment value.")

    @property
    def byte_order(self):
        """
        Get the field declaration's byte order. One of the ByteOrder's constant.
        """

        return nbt._bt_ctf_field_type_get_byte_order(self._ft)

    @byte_order.setter
    def byte_order(self, byte_order):
        """
        Set the field declaration's byte order. Use constants defined in the ByteOrder
        class.
        """

        ret = nbt._bt_ctf_field_type_set_byte_order(self._ft, byte_order)

        if ret < 0:
            raise ValueError("Could not set byte order value.")


class IntegerFieldDeclaration(FieldDeclaration):
    def __init__(self, size):
        """
        Create a new integer field declaration of the given size.
        """
        self._ft = nbt._bt_ctf_field_type_integer_create(size)
        super().__init__()

    @property
    def size(self):
        """
        Get an integer's size.
        """

        ret = nbt._bt_ctf_field_type_integer_get_size(self._ft)

        if ret < 0:
            raise ValueError("Could not get Integer's size attribute.")
        else:
            return ret

    @property
    def signed(self):
        """
        Get an integer's signedness attribute.
        """

        ret = nbt._bt_ctf_field_type_integer_get_signed(self._ft)

        if ret < 0:
            raise ValueError("Could not get Integer's signed attribute.")
        elif ret > 0:
            return True
        else:
            return False

    @signed.setter
    def signed(self, signed):
        """
        Set an integer's signedness attribute.
        """

        ret = nbt._bt_ctf_field_type_integer_set_signed(self._ft, signed)

        if ret < 0:
            raise ValueError("Could not set Integer's signed attribute.")

    @property
    def base(self):
        """
        Get the integer's base used to pretty-print the resulting trace.
        Returns a constant from the FieldDeclaration.IntegerBase class.
        """

        return nbt._bt_ctf_field_type_integer_get_base(self._ft)

    @base.setter
    def base(self, base):
        """
        Set the integer's base used to pretty-print the resulting trace.
        The base must be a constant of the FieldDeclarationIntegerBase class.
        """

        ret = nbt._bt_ctf_field_type_integer_set_base(self._ft, base)

        if ret < 0:
            raise ValueError("Could not set Integer's base.")

    @property
    def encoding(self):
        """
        Get the integer's encoding (one of the constants of the
        CTFStringEncoding class).
        Returns a constant from the CTFStringEncoding class.
        """

        return nbt._bt_ctf_field_type_integer_get_encoding(self._ft)

    @encoding.setter
    def encoding(self, encoding):
        """
        An integer encoding may be set to signal that the integer must be printed
        as a text character. Must be a constant from the CTFStringEncoding class.
        """

        ret = nbt._bt_ctf_field_type_integer_set_encoding(self._ft, encoding)

        if ret < 0:
            raise ValueError("Could not set Integer's encoding.")


class EnumerationFieldDeclaration(FieldDeclaration):
    def __init__(self, integer_type):
        """
        Create a new enumeration field declaration with the given underlying container type.
        """
        isinst = isinstance(integer_type, IntegerFieldDeclaration)

        if integer_type is None or not isinst:
            raise TypeError("Invalid integer container.")

        self._ft = nbt._bt_ctf_field_type_enumeration_create(integer_type._ft)
        super().__init__()

    @property
    def container(self):
        """
        Get the enumeration's underlying container type.
        """

        ret = nbt._bt_ctf_field_type_enumeration_get_container_type(self._ft)

        if ret is None:
            raise TypeError("Invalid enumeration declaration")

        return FieldDeclaration._create_field_declaration_from_native_instance(ret)

    def add_mapping(self, name, range_start, range_end):
        """
        Add a mapping to the enumeration. The range's values are inclusive.
        """

        if range_start < 0 or range_end < 0:
            ret = nbt._bt_ctf_field_type_enumeration_add_mapping(self._ft,
                                                                 str(name),
                                                                 range_start,
                                                                 range_end)
        else:
            ret = nbt._bt_ctf_field_type_enumeration_add_mapping_unsigned(self._ft,
                                                                          str(name),
                                                                          range_start,
                                                                          range_end)

        if ret < 0:
            raise ValueError("Could not add mapping to enumeration declaration.")

    @property
    def mappings(self):
        """
        Generator returning instances of EnumerationMapping.
        """

        signed = self.container.signed

        count = nbt._bt_ctf_field_type_enumeration_get_mapping_count(self._ft)

        for i in range(count):
            if signed:
                ret = nbt._bt_python_ctf_field_type_enumeration_get_mapping(self._ft, i)
            else:
                ret = nbt._bt_python_ctf_field_type_enumeration_get_mapping_unsigned(self._ft, i)

            if len(ret) != 3:
                msg = "Could not get Enumeration mapping at index {}".format(i)
                raise TypeError(msg)

            name, range_start, range_end = ret
            yield EnumerationMapping(name, range_start, range_end)

    def get_mapping_by_name(self, name):
        """
        Get a mapping by name (EnumerationMapping).
        """

        index = nbt._bt_ctf_field_type_enumeration_get_mapping_index_by_name(self._ft, name)

        if index < 0:
            return None

        if self.container.signed:
            ret = nbt._bt_python_ctf_field_type_enumeration_get_mapping(self._ft, index)
        else:
            ret = nbt._bt_python_ctf_field_type_enumeration_get_mapping_unsigned(self._ft, index)

        if len(ret) != 3:
            msg = "Could not get Enumeration mapping at index {}".format(i)
            raise TypeError(msg)

        name, range_start, range_end = ret

        return EnumerationMapping(name, range_start, range_end)

    def get_mapping_by_value(self, value):
        """
        Get a mapping by value (EnumerationMapping).
        """

        if value < 0:
            index = nbt._bt_ctf_field_type_enumeration_get_mapping_index_by_value(self._ft, value)
        else:
            index = nbt._bt_ctf_field_type_enumeration_get_mapping_index_by_unsigned_value(self._ft, value)

        if index < 0:
            return None

        if self.container.signed:
            ret = nbt._bt_python_ctf_field_type_enumeration_get_mapping(self._ft, index)
        else:
            ret = nbt._bt_python_ctf_field_type_enumeration_get_mapping_unsigned(self._ft, index)

        if len(ret) != 3:
            msg = "Could not get Enumeration mapping at index {}".format(i)
            raise TypeError(msg)

        name, range_start, range_end = ret

        return EnumerationMapping(name, range_start, range_end)


class FloatFieldDeclaration(FieldDeclaration):
    FLT_EXP_DIG = 8
    DBL_EXP_DIG = 11
    FLT_MANT_DIG = 24
    DBL_MANT_DIG = 53

    def __init__(self):
        """
        Create a new floating point field declaration.
        """

        self._ft = nbt._bt_ctf_field_type_floating_point_create()
        super().__init__()

    @property
    def exponent_digits(self):
        """
        Get the number of exponent digits used to store the floating point field.
        """

        ret = nbt._bt_ctf_field_type_floating_point_get_exponent_digits(self._ft)

        if ret < 0:
            raise TypeError(
                "Could not get Floating point exponent digit count")

        return ret

    @exponent_digits.setter
    def exponent_digits(self, exponent_digits):
        """
        Set the number of exponent digits to use to store the floating point field.
        The only values currently supported are FLT_EXP_DIG and DBL_EXP_DIG which
        are defined as constants of this class.
        """

        ret = nbt._bt_ctf_field_type_floating_point_set_exponent_digits(self._ft,
                                                                        exponent_digits)

        if ret < 0:
            raise ValueError("Could not set exponent digit count.")

    @property
    def mantissa_digits(self):
        """
        Get the number of mantissa digits used to store the floating point field.
        """

        ret = nbt._bt_ctf_field_type_floating_point_get_mantissa_digits(self._ft)

        if ret < 0:
            raise TypeError("Could not get Floating point mantissa digit count")

        return ret

    @mantissa_digits.setter
    def mantissa_digits(self, mantissa_digits):
        """
        Set the number of mantissa digits to use to store the floating point field.
        The only values currently supported are FLT_MANT_DIG and DBL_MANT_DIG which
        are defined as constants of this class.
        """

        ret = nbt._bt_ctf_field_type_floating_point_set_mantissa_digits(self._ft,
                                                                        mantissa_digits)

        if ret < 0:
            raise ValueError("Could not set mantissa digit count.")


class FloatingPointFieldDeclaration(FloatFieldDeclaration):
    pass


class StructureFieldDeclaration(FieldDeclaration):
    def __init__(self):
        """
        Create a new structure field declaration.
        """

        self._ft = nbt._bt_ctf_field_type_structure_create()
        super().__init__()

    def add_field(self, field_type, field_name):
        """
        Add a field of type "field_type" to the structure.
        """

        ret = nbt._bt_ctf_field_type_structure_add_field(self._ft,
                                                         field_type._ft,
                                                         str(field_name))

        if ret < 0:
            raise ValueError("Could not add field to structure.")

    @property
    def fields(self):
        """
        Generator returning the structure's field as tuples of (field name, field declaration).
        """

        count = nbt._bt_ctf_field_type_structure_get_field_count(self._ft)

        if count < 0:
            raise TypeError("Could not get Structure field count")

        for i in range(count):
            field_name = nbt._bt_python_ctf_field_type_structure_get_field_name(self._ft, i)

            if field_name is None:
                msg = "Could not get Structure field name at index {}".format(i)
                raise TypeError(msg)

            field_type_native = nbt._bt_python_ctf_field_type_structure_get_field_type(self._ft, i)

            if field_type_native is None:
                msg = "Could not get Structure field type at index {}".format(i)
                raise TypeError(msg)

            field_type = FieldDeclaration._create_field_declaration_from_native_instance(field_type_native)
            yield (field_name, field_type)

    def get_field_by_name(self, name):
        """
        Get a field declaration by name (FieldDeclaration).
        """

        field_type_native = nbt._bt_ctf_field_type_structure_get_field_type_by_name(self._ft, name)

        if field_type_native is None:
            msg = "Could not find Structure field with name {}".format(name)
            raise TypeError(msg)

        return FieldDeclaration._create_field_declaration_from_native_instance(field_type_native)


class VariantFieldDeclaration(FieldDeclaration):
    def __init__(self, enum_tag, tag_name):
        """
        Create a new variant field declaration.
        """

        isinst = isinstance(enum_tag, EnumerationFieldDeclaration)
        if enum_tag is None or not isinst:
            raise TypeError("Invalid tag type; must be of type EnumerationFieldDeclaration.")

        self._ft = nbt._bt_ctf_field_type_variant_create(enum_tag._ft,
                                                         str(tag_name))
        super().__init__()

    @property
    def tag_name(self):
        """
        Get the variant's tag name.
        """

        ret = nbt._bt_ctf_field_type_variant_get_tag_name(self._ft)

        if ret is None:
            raise TypeError("Could not get Variant tag name")

        return ret

    @property
    def tag_type(self):
        """
        Get the variant's tag type.
        """

        ret = nbt._bt_ctf_field_type_variant_get_tag_type(self._ft)

        if ret is None:
            raise TypeError("Could not get Variant tag type")

        return FieldDeclaration._create_field_declaration_from_native_instance(ret)

    def add_field(self, field_type, field_name):
        """
        Add a field of type "field_type" to the variant.
        """

        ret = nbt._bt_ctf_field_type_variant_add_field(self._ft,
                                                       field_type._ft,
                                                       str(field_name))

        if ret < 0:
            raise ValueError("Could not add field to variant.")

    @property
    def fields(self):
        """
        Generator returning the variant's field as tuples of (field name, field declaration).
        """

        count = nbt._bt_ctf_field_type_variant_get_field_count(self._ft)

        if count < 0:
            raise TypeError("Could not get Variant field count")

        for i in range(count):
            field_name = nbt._bt_python_ctf_field_type_variant_get_field_name(self._ft, i)

            if field_name is None:
                msg = "Could not get Variant field name at index {}".format(i)
                raise TypeError(msg)

            field_type_native = nbt._bt_python_ctf_field_type_variant_get_field_type(self._ft, i)

            if field_type_native is None:
                msg = "Could not get Variant field type at index {}".format(i)
                raise TypeError(msg)

            field_type = FieldDeclaration._create_field_declaration_from_native_instance(field_type_native)
            yield (field_name, field_type)

    def get_field_by_name(self, name):
        """
        Get a field declaration by name (FieldDeclaration).
        """

        field_type_native = nbt._bt_ctf_field_type_variant_get_field_type_by_name(self._ft,
                                                                                  name)

        if field_type_native is None:
            msg = "Could not find Variant field with name {}".format(name)
            raise TypeError(msg)

        return FieldDeclaration._create_field_declaration_from_native_instance(field_type_native)

    def get_field_from_tag(self, tag):
        """
        Get a field declaration from tag (EnumerationField).
        """

        field_type_native = nbt._bt_ctf_field_type_variant_get_field_type_from_tag(self._ft, tag._f)

        if field_type_native is None:
            msg = "Could not find Variant field with tag value {}".format(tag.value)
            raise TypeError(msg)

        return FieldDeclaration._create_field_declaration_from_native_instance(field_type_native)


class ArrayFieldDeclaration(FieldDeclaration):
    def __init__(self, element_type, length):
        """
        Create a new array field declaration.
        """

        self._ft = nbt._bt_ctf_field_type_array_create(element_type._ft,
                                                       length)
        super().__init__()

    @property
    def element_type(self):
        """
        Get the array's element type.
        """

        ret = nbt._bt_ctf_field_type_array_get_element_type(self._ft)

        if ret is None:
            raise TypeError("Could not get Array element type")

        return FieldDeclaration._create_field_declaration_from_native_instance(ret)

    @property
    def length(self):
        """
        Get the array's length.
        """

        ret = nbt._bt_ctf_field_type_array_get_length(self._ft)

        if ret < 0:
            raise TypeError("Could not get Array length")

        return ret


class SequenceFieldDeclaration(FieldDeclaration):
    def __init__(self, element_type, length_field_name):
        """
        Create a new sequence field declaration.
        """

        self._ft = nbt._bt_ctf_field_type_sequence_create(element_type._ft,
                                                          str(length_field_name))
        super().__init__()

    @property
    def element_type(self):
        """
        Get the sequence's element type.
        """

        ret = nbt._bt_ctf_field_type_sequence_get_element_type(self._ft)

        if ret is None:
            raise TypeError("Could not get Sequence element type")

        return FieldDeclaration._create_field_declaration_from_native_instance(ret)

    @property
    def length_field_name(self):
        """
        Get the sequence's length field name.
        """

        ret = nbt._bt_ctf_field_type_sequence_get_length_field_name(self._ft)

        if ret is None:
            raise TypeError("Could not get Sequence length field name")

        return ret


class StringFieldDeclaration(FieldDeclaration):
    def __init__(self):
        """
        Create a new string field declaration.
        """

        self._ft = nbt._bt_ctf_field_type_string_create()
        super().__init__()

    @property
    def encoding(self):
        """
        Get a string declaration's encoding (a constant from the CTFStringEncoding class).
        """

        return nbt._bt_ctf_field_type_string_get_encoding(self._ft)

    @encoding.setter
    def encoding(self, encoding):
        """
        Set a string declaration's encoding. Must be a constant from the CTFStringEncoding class.
        """

        ret = nbt._bt_ctf_field_type_string_set_encoding(self._ft, encoding)
        if ret < 0:
            raise ValueError("Could not set string encoding.")


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
    Base class, do not instantiate.
    """

    def __init__(self, field_type):
        if not isinstance(field_type, FieldDeclaration):
            raise TypeError("Invalid field_type argument.")

        self._f = nbt._bt_ctf_field_create(field_type._ft)

        if self._f is None:
            raise ValueError("Field creation failed.")

    def __del__(self):
        nbt._bt_ctf_field_put(self._f)

    @staticmethod
    def _create_field_from_native_instance(native_field_instance):
        type_dict = {
            common.CTFTypeId.INTEGER: IntegerField,
            common.CTFTypeId.FLOAT: FloatingPointField,
            common.CTFTypeId.ENUM: EnumerationField,
            common.CTFTypeId.STRING: StringField,
            common.CTFTypeId.STRUCT: StructureField,
            common.CTFTypeId.VARIANT: VariantField,
            common.CTFTypeId.ARRAY: ArrayField,
            common.CTFTypeId.SEQUENCE: SequenceField
        }

        field_type = nbt._bt_python_get_field_type(native_field_instance)

        if field_type == common.CTFTypeId.UNKNOWN:
            raise TypeError("Invalid field instance")

        field = Field.__new__(Field)
        field._f = native_field_instance
        field.__class__ = type_dict[field_type]

        return field

    @property
    def declaration(self):
        native_field_type = nbt._bt_ctf_field_get_type(self._f)

        if native_field_type is None:
            raise TypeError("Invalid field instance")
        return FieldDeclaration._create_field_declaration_from_native_instance(
            native_field_type)


class IntegerField(Field):
    @property
    def value(self):
        """
        Get an integer field's value.
        """

        signedness = nbt._bt_python_field_integer_get_signedness(self._f)

        if signedness < 0:
            raise TypeError("Invalid integer instance.")

        if signedness == 0:
            ret, value = nbt._bt_ctf_field_unsigned_integer_get_value(self._f)
        else:
            ret, value = nbt._bt_ctf_field_signed_integer_get_value(self._f)

        if ret < 0:
            raise ValueError("Could not get integer field value.")

        return value

    @value.setter
    def value(self, value):
        """
        Set an integer field's value.
        """

        if not isinstance(value, int):
            raise TypeError("IntegerField's value must be an int")

        signedness = nbt._bt_python_field_integer_get_signedness(self._f)
        if signedness < 0:
            raise TypeError("Invalid integer instance.")

        if signedness == 0:
            ret = nbt._bt_ctf_field_unsigned_integer_set_value(self._f, value)
        else:
            ret = nbt._bt_ctf_field_signed_integer_set_value(self._f, value)

        if ret < 0:
            raise ValueError("Could not set integer field value.")


class EnumerationField(Field):
    @property
    def container(self):
        """
        Return the enumeration's underlying container field (an integer field).
        """

        container = IntegerField.__new__(IntegerField)
        container._f = nbt._bt_ctf_field_enumeration_get_container(self._f)

        if container._f is None:
            raise TypeError("Invalid enumeration field type.")

        return container

    @property
    def value(self):
        """
        Get the enumeration field's mapping name.
        """

        value = nbt._bt_ctf_field_enumeration_get_mapping_name(self._f)

        if value is None:
            raise ValueError("Could not get enumeration's mapping name.")

        return value

    @value.setter
    def value(self, value):
        """
        Set the enumeration field's value. Must be an integer as mapping names
        may be ambiguous.
        """

        if not isinstance(value, int):
            raise TypeError("EnumerationField value must be an int")

        self.container.value = value


class FloatingPointField(Field):
    @property
    def value(self):
        """
        Get a floating point field's value.
        """

        ret, value = nbt._bt_ctf_field_floating_point_get_value(self._f)

        if ret < 0:
            raise ValueError("Could not get floating point field value.")

        return value

    @value.setter
    def value(self, value):
        """
        Set a floating point field's value.
        """

        if not isinstance(value, int) and not isinstance(value, float):
            raise TypeError("Value must be either a float or an int")

        ret = nbt._bt_ctf_field_floating_point_set_value(self._f, float(value))

        if ret < 0:
            raise ValueError("Could not set floating point field value.")


# oops!!
class FloatFieldingPoint(FloatingPointField):
    pass


class StructureField(Field):
    def field(self, field_name):
        """
        Get the structure's field corresponding to the provided field name.
        """

        native_instance = nbt._bt_ctf_field_structure_get_field(self._f,
                                                                str(field_name))

        if native_instance is None:
            raise ValueError("Invalid field_name provided.")

        return Field._create_field_from_native_instance(native_instance)


class VariantField(Field):
    def field(self, tag):
        """
        Return the variant's selected field. The "tag" field is the selector enum field.
        """

        native_instance = nbt._bt_ctf_field_variant_get_field(self._f, tag._f)

        if native_instance is None:
            raise ValueError("Invalid tag provided.")

        return Field._create_field_from_native_instance(native_instance)


class ArrayField(Field):
    def field(self, index):
        """
        Return the array's field at position "index".
        """

        native_instance = nbt._bt_ctf_field_array_get_field(self._f, index)

        if native_instance is None:
            raise IndexError("Invalid index provided.")

        return Field._create_field_from_native_instance(native_instance)


class SequenceField(Field):
    @property
    def length(self):
        """
        Get the sequence's length field (IntegerField).
        """

        native_instance = nbt._bt_ctf_field_sequence_get_length(self._f)

        if native_instance is None:
            length = -1

        return Field._create_field_from_native_instance(native_instance)

    @length.setter
    def length(self, length_field):
        """
        Set the sequence's length field (IntegerField).
        """

        if not isinstance(length_field, IntegerField):
            raise TypeError("Invalid length field.")

        if length_field.declaration.signed:
            raise TypeError("Sequence field length must be unsigned")

        ret = nbt._bt_ctf_field_sequence_set_length(self._f, length_field._f)

        if ret < 0:
            raise ValueError("Could not set sequence length.")

    def field(self, index):
        """
        Return the sequence's field at position "index".
        """

        native_instance = nbt._bt_ctf_field_sequence_get_field(self._f, index)

        if native_instance is None:
            raise ValueError("Could not get sequence element at index.")

        return Field._create_field_from_native_instance(native_instance)


class StringField(Field):
    @property
    def value(self):
        """
        Get a string field's value.
        """

        return nbt._bt_ctf_field_string_get_value(self._f)

    @value.setter
    def value(self, value):
        """
        Set a string field's value.
        """

        ret = nbt._bt_ctf_field_string_set_value(self._f, str(value))

        if ret < 0:
            raise ValueError("Could not set string field value.")


class EventClass:
    def __init__(self, name):
        """
        Create a new event class of the given name.
        """

        self._ec = nbt._bt_ctf_event_class_create(name)

        if self._ec is None:
            raise ValueError("Event class creation failed.")

    def __del__(self):
        nbt._bt_ctf_event_class_put(self._ec)

    def add_field(self, field_type, field_name):
        """
        Add a field of type "field_type" to the event class.
        """

        ret = nbt._bt_ctf_event_class_add_field(self._ec, field_type._ft,
                                                str(field_name))

        if ret < 0:
            raise ValueError("Could not add field to event class.")

    @property
    def name(self):
        """
        Get the event class' name.
        """

        name = nbt._bt_ctf_event_class_get_name(self._ec)

        if name is None:
            raise TypeError("Could not get EventClass name")

        return name

    @property
    def id(self):
        """
        Get the event class' id. Returns a negative value if unset.
        """

        id = nbt._bt_ctf_event_class_get_id(self._ec)

        if id < 0:
            raise TypeError("Could not get EventClass id")

        return id

    @id.setter
    def id(self, id):
        """
        Set the event class' id. Throws a TypeError if the event class
        is already registered to a stream class.
        """

        ret = nbt._bt_ctf_event_class_set_id(self._ec, id)

        if ret < 0:
            raise TypeError("Can't change an Event Class's id after it has been assigned to a stream class")

    @property
    def stream_class(self):
        """
        Get the event class' stream class. Returns None if unset.
        """
        stream_class_native = nbt._bt_ctf_event_class_get_stream_class(self._ec)

        if stream_class_native is None:
            return None

        stream_class = StreamClass.__new__(StreamClass)
        stream_class._sc = stream_class_native

        return stream_class

    @property
    def fields(self):
        """
        Generator returning the event class' fields as tuples of (field name, field declaration).
        """

        count = nbt._bt_ctf_event_class_get_field_count(self._ec)

        if count < 0:
            raise TypeError("Could not get EventClass' field count")

        for i in range(count):
            field_name = nbt._bt_python_ctf_event_class_get_field_name(self._ec, i)

            if field_name is None:
                msg = "Could not get EventClass' field name at index {}".format(i)
                raise TypeError(msg)

            field_type_native = nbt._bt_python_ctf_event_class_get_field_type(self._ec, i)

            if field_type_native is None:
                msg = "Could not get EventClass' field type at index {}".format(i)
                raise TypeError(msg)

            field_type = FieldDeclaration._create_field_declaration_from_native_instance(field_type_native)
            yield (field_name, field_type)

    def get_field_by_name(self, name):
        """
        Get a field declaration by name (FieldDeclaration).
        """

        field_type_native = nbt._bt_ctf_event_class_get_field_by_name(self._ec, name)

        if field_type_native is None:
            msg = "Could not find EventClass field with name {}".format(name)
            raise TypeError(msg)

        return FieldDeclaration._create_field_declaration_from_native_instance(field_type_native)


class Event:
    def __init__(self, event_class):
        """
        Create a new event of the given event class.
        """

        if not isinstance(event_class, EventClass):
            raise TypeError("Invalid event_class argument.")

        self._e = nbt._bt_ctf_event_create(event_class._ec)

        if self._e is None:
            raise ValueError("Event creation failed.")

    def __del__(self):
        nbt._bt_ctf_event_put(self._e)

    @property
    def event_class(self):
        """
        Get the event's class.
        """

        event_class_native = nbt._bt_ctf_event_get_class(self._e)

        if event_class_native is None:
            return None

        event_class = EventClass.__new__(EventClass)
        event_class._ec = event_class_native

        return event_class

    def clock(self):
        """
        Get a clock from event. Returns None if the event's class
        is not registered to a stream class.
        """

        clock_instance = nbt._bt_ctf_event_get_clock(self._e)

        if clock_instance is None:
            return None

        clock = Clock.__new__(Clock)
        clock._c = clock_instance

        return clock

    def payload(self, field_name):
        """
        Get a field from event.
        """

        native_instance = nbt._bt_ctf_event_get_payload(self._e,
                                                        str(field_name))

        if native_instance is None:
            raise ValueError("Could not get event payload.")

        return Field._create_field_from_native_instance(native_instance)

    def set_payload(self, field_name, value_field):
        """
        Set a manually created field as an event's payload.
        """

        if not isinstance(value, Field):
            raise TypeError("Invalid value type.")

        ret = nbt._bt_ctf_event_set_payload(self._e, str(field_name),
                                            value_field._f)

        if ret < 0:
            raise ValueError("Could not set event field payload.")


class StreamClass:
    def __init__(self, name):
        """
        Create a new stream class of the given name.
        """

        self._sc = nbt._bt_ctf_stream_class_create(name)

        if self._sc is None:
            raise ValueError("Stream class creation failed.")

    def __del__(self):
        nbt._bt_ctf_stream_class_put(self._sc)

    @property
    def name(self):
        """
        Get a stream class' name.
        """

        name = nbt._bt_ctf_stream_class_get_name(self._sc)

        if name is None:
            raise TypeError("Could not get StreamClass name")

        return name

    @property
    def clock(self):
        """
        Get a stream class' clock.
        """

        clock_instance = nbt._bt_ctf_stream_class_get_clock(self._sc)

        if clock_instance is None:
            return None

        clock = Clock.__new__(Clock)
        clock._c = clock_instance

        return clock

    @clock.setter
    def clock(self, clock):
        """
        Assign a clock to a stream class.
        """

        if not isinstance(clock, Clock):
            raise TypeError("Invalid clock type.")

        ret = nbt._bt_ctf_stream_class_set_clock(self._sc, clock._c)

        if ret < 0:
            raise ValueError("Could not set stream class clock.")

    @property
    def id(self):
        """
        Get a stream class' id.
        """

        ret = nbt._bt_ctf_stream_class_get_id(self._sc)

        if ret < 0:
            raise TypeError("Could not get StreamClass id")

        return ret

    @id.setter
    def id(self, id):
        """
        Assign an id to a stream class.
        """

        ret = nbt._bt_ctf_stream_class_set_id(self._sc, id)

        if ret < 0:
            raise TypeError("Could not set stream class id.")

    @property
    def event_classes(self):
        """
        Generator returning the stream class' event classes.
        """

        count = nbt._bt_ctf_stream_class_get_event_class_count(self._sc)

        if count < 0:
            raise TypeError("Could not get StreamClass' event class count")

        for i in range(count):
            event_class_native = nbt._bt_ctf_stream_class_get_event_class(self._sc, i)

            if event_class_native is None:
                msg = "Could not get StreamClass' event class at index {}".format(i)
                raise TypeError(msg)

            event_class = EventClass.__new__(EventClass)
            event_class._ec = event_class_native
            yield event_class

    def add_event_class(self, event_class):
        """
        Add an event class to a stream class. New events can be added even after a
        stream has been instantiated and events have been appended. However, a stream
        will not accept events of a class that has not been added to the stream
        class beforehand.
        """

        if not isinstance(event_class, EventClass):
            raise TypeError("Invalid event_class type.")

        ret = nbt._bt_ctf_stream_class_add_event_class(self._sc,
                                                       event_class._ec)

        if ret < 0:
            raise ValueError("Could not add event class.")

    @property
    def packet_context_type(self):
        """
        Get the StreamClass' packet context type (StructureFieldDeclaration)
        """

        field_type_native = nbt._bt_ctf_stream_class_get_packet_context_type(self._sc)

        if field_type_native is None:
            raise ValueError("Invalid StreamClass")

        field_type = FieldDeclaration._create_field_declaration_from_native_instance(field_type_native)

        return field_type

    @packet_context_type.setter
    def packet_context_type(self, field_type):
        """
        Set a StreamClass' packet context type. Must be of type
        StructureFieldDeclaration.
        """

        if not isinstance(field_type, StructureFieldDeclaration):
            raise TypeError("field_type argument must be of type StructureFieldDeclaration.")

        ret = nbt._bt_ctf_stream_class_set_packet_context_type(self._sc,
                                                               field_type._ft)

        if ret < 0:
            raise ValueError("Failed to set packet context type.")


class Stream:
    def __init__(self):
        raise NotImplementedError("Stream cannot be instantiated; use Writer.create_stream()")

    def __del__(self):
        nbt._bt_ctf_stream_put(self._s)

    @property
    def discarded_events(self):
        """
        Get a stream's discarded event count.
        """

        ret, count = nbt._bt_ctf_stream_get_discarded_events_count(self._s)

        if ret < 0:
            raise ValueError("Could not get the stream's discarded events count")

        return count

    def append_discarded_events(self, event_count):
        """
        Increase the current packet's discarded event count.
        """

        nbt._bt_ctf_stream_append_discarded_events(self._s, event_count)

    def append_event(self, event):
        """
        Append "event" to the stream's current packet. The stream's associated clock
        will be sampled during this call. The event shall not be modified after
        being appended to a stream.
        """

        ret = nbt._bt_ctf_stream_append_event(self._s, event._e)

        if ret < 0:
            raise ValueError("Could not append event to stream.")

    @property
    def packet_context(self):
        """
        Get a Stream's packet context field (a StructureField).
        """

        native_field = nbt._bt_ctf_stream_get_packet_context(self._s)

        if native_field is None:
            raise ValueError("Invalid Stream.")

        return Field._create_field_from_native_instance(native_field)

    @packet_context.setter
    def packet_context(self, field):
        """
        Set a Stream's packet context field (must be a StructureField).
        """

        if not isinstance(field, StructureField):
            raise TypeError("Argument field must be of type StructureField")

        ret = nbt._bt_ctf_stream_set_packet_context(self._s, field._f)

        if ret < 0:
            raise ValueError("Invalid packet context field.")

    def flush(self):
        """
        The stream's current packet's events will be flushed to disk. Events
        subsequently appended to the stream will be added to a new packet.
        """

        ret = nbt._bt_ctf_stream_flush(self._s)

        if ret < 0:
            raise ValueError("Could not flush stream.")


class Writer:
    def __init__(self, path):
        """
        Create a new writer that will produce a trace in the given path.
        """

        self._w = nbt._bt_ctf_writer_create(path)

        if self._w is None:
            raise ValueError("Writer creation failed.")

    def __del__(self):
        nbt._bt_ctf_writer_put(self._w)

    def create_stream(self, stream_class):
        """
        Create a new stream instance and register it to the writer.
        """

        if not isinstance(stream_class, StreamClass):
            raise TypeError("Invalid stream_class type.")

        stream = Stream.__new__(Stream)
        stream._s = nbt._bt_ctf_writer_create_stream(self._w, stream_class._sc)

        return stream

    def add_environment_field(self, name, value):
        """
        Add an environment field to the trace.
        """

        ret = nbt._bt_ctf_writer_add_environment_field(self._w, str(name),
                                                       str(value))

        if ret < 0:
            raise ValueError("Could not add environment field to trace.")

    def add_clock(self, clock):
        """
        Add a clock to the trace. Clocks assigned to stream classes must be
        registered to the writer.
        """

        ret = nbt._bt_ctf_writer_add_clock(self._w, clock._c)

        if ret < 0:
            raise ValueError("Could not add clock to Writer.")

    @property
    def metadata(self):
        """
        Get the trace's TSDL meta-data.
        """

        return nbt._bt_ctf_writer_get_metadata_string(self._w)

    def flush_metadata(self):
        """
        Flush the trace's metadata to the metadata file.
        """

        nbt._bt_ctf_writer_flush_metadata(self._w)

    @property
    def byte_order(self):
        """
        Get the trace's byte order. Must be a constant from the ByteOrder
        class.
        """

        raise NotImplementedError("Getter not implemented.")

    @byte_order.setter
    def byte_order(self, byte_order):
        """
        Set the trace's byte order. Must be a constant from the ByteOrder
        class. Defaults to the host machine's endianness
        """

        ret = nbt._bt_ctf_writer_set_byte_order(self._w, byte_order)

        if ret < 0:
            raise ValueError("Could not set trace's byte order.")
