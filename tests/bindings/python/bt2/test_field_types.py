import bt2.fields
import unittest
import copy
import bt2


class _TestCopySimple:
    def _test_copy(self, cpy):
        self.assertIsNot(cpy, self._ft)
        self.assertNotEqual(cpy.addr, self._ft.addr)
        self.assertEqual(cpy, self._ft)

    def test_copy(self):
        cpy = copy.copy(self._ft)
        self._test_copy(cpy)

    def test_deepcopy(self):
        cpy = copy.deepcopy(self._ft)
        self._test_copy(cpy)


class _TestAlignmentProp:
    def test_assign_alignment(self):
        self._ft.alignment = 32
        self.assertEqual(self._ft.alignment, 32)

    def test_assign_invalid_alignment(self):
        with self.assertRaises(ValueError):
            self._ft.alignment = 23


class _TestByteOrderProp:
    def test_assign_byte_order(self):
        self._ft.byte_order = bt2.ByteOrder.LITTLE_ENDIAN
        self.assertEqual(self._ft.byte_order, bt2.ByteOrder.LITTLE_ENDIAN)

    def test_assign_invalid_byte_order(self):
        with self.assertRaises(TypeError):
            self._ft.byte_order = 'hey'


class _TestInvalidEq:
    def test_eq_invalid(self):
        self.assertFalse(self._ft == 23)


class _TestIntegerFieldTypeProps:
    def test_size_prop(self):
        self.assertEqual(self._ft.size, 35)

    def test_assign_signed(self):
        self._ft.is_signed = True
        self.assertTrue(self._ft.is_signed)

    def test_assign_invalid_signed(self):
        with self.assertRaises(TypeError):
            self._ft.is_signed = 23

    def test_assign_base(self):
        self._ft.base = bt2.Base.HEXADECIMAL
        self.assertEqual(self._ft.base, bt2.Base.HEXADECIMAL)

    def test_assign_invalid_base(self):
        with self.assertRaises(TypeError):
            self._ft.base = 'hey'

    def test_assign_encoding(self):
        self._ft.encoding = bt2.Encoding.UTF8
        self.assertEqual(self._ft.encoding, bt2.Encoding.UTF8)

    def test_assign_invalid_encoding(self):
        with self.assertRaises(TypeError):
            self._ft.encoding = 'hey'

    def test_assign_mapped_clock_class(self):
        cc = bt2.ClockClass('name', 1000)
        self._ft.mapped_clock_class = cc
        self.assertEqual(self._ft.mapped_clock_class, cc)

    def test_assign_invalid_mapped_clock_class(self):
        with self.assertRaises(TypeError):
            self._ft.mapped_clock_class = object()


class IntegerFieldTypeTestCase(_TestIntegerFieldTypeProps, _TestCopySimple,
                               _TestAlignmentProp, _TestByteOrderProp,
                               _TestInvalidEq, unittest.TestCase):
    def setUp(self):
        self._ft = bt2.IntegerFieldType(35)

    def tearDown(self):
        del self._ft

    def test_create_default(self):
        self.assertEqual(self._ft.size, 35)
        self.assertIsNone(self._ft.mapped_clock_class)

    def test_create_invalid_size(self):
        with self.assertRaises(TypeError):
            ft = bt2.IntegerFieldType('yes')

    def test_create_neg_size(self):
        with self.assertRaises(ValueError):
            ft = bt2.IntegerFieldType(-2)

    def test_create_neg_zero(self):
        with self.assertRaises(ValueError):
            ft = bt2.IntegerFieldType(0)

    def test_create_full(self):
        cc = bt2.ClockClass('name', 1000)
        ft = bt2.IntegerFieldType(24, alignment=16,
                                  byte_order=bt2.ByteOrder.BIG_ENDIAN,
                                  is_signed=True, base=bt2.Base.OCTAL,
                                  encoding=bt2.Encoding.NONE,
                                  mapped_clock_class=cc)
        self.assertEqual(ft.size, 24)
        self.assertEqual(ft.alignment, 16)
        self.assertEqual(ft.byte_order, bt2.ByteOrder.BIG_ENDIAN)
        self.assertTrue(ft.is_signed)
        self.assertEqual(ft.base, bt2.Base.OCTAL)
        self.assertEqual(ft.encoding, bt2.Encoding.NONE)
        self.assertEqual(ft.mapped_clock_class, cc)

    def test_create_field(self):
        field = self._ft()
        self.assertIsInstance(field, bt2.fields._IntegerField)

    def test_create_field_init(self):
        field = self._ft(23)
        self.assertEqual(field, 23)


class FloatingPointNumberFieldTypeTestCase(_TestCopySimple, _TestAlignmentProp,
                                           _TestByteOrderProp, _TestInvalidEq,
                                           unittest.TestCase):
    def setUp(self):
        self._ft = bt2.FloatingPointNumberFieldType()

    def tearDown(self):
        del self._ft

    def test_create_default(self):
        pass

    def test_create_full(self):
        ft = bt2.FloatingPointNumberFieldType(alignment=16,
                                              byte_order=bt2.ByteOrder.BIG_ENDIAN,
                                              exponent_size=11,
                                              mantissa_size=53)
        self.assertEqual(ft.alignment, 16)
        self.assertEqual(ft.byte_order, bt2.ByteOrder.BIG_ENDIAN)
        self.assertEqual(ft.exponent_size, 11)
        self.assertEqual(ft.mantissa_size, 53)

    def test_assign_exponent_size(self):
        self._ft.exponent_size = 8
        self.assertEqual(self._ft.exponent_size, 8)

    def test_assign_invalid_exponent_size(self):
        with self.assertRaises(TypeError):
            self._ft.exponent_size = 'yes'

    def test_assign_mantissa_size(self):
        self._ft.mantissa_size = 24
        self.assertEqual(self._ft.mantissa_size, 24)

    def test_assign_invalid_mantissa_size(self):
        with self.assertRaises(TypeError):
            self._ft.mantissa_size = 'no'

    def test_create_field(self):
        field = self._ft()
        self.assertIsInstance(field, bt2.fields._FloatingPointNumberField)

    def test_create_field_init(self):
        field = self._ft(17.5)
        self.assertEqual(field, 17.5)


class EnumerationFieldTypeTestCase(_TestIntegerFieldTypeProps, _TestInvalidEq,
                                   _TestCopySimple, _TestAlignmentProp,
                                   _TestByteOrderProp, unittest.TestCase):
    def setUp(self):
        self._ft = bt2.EnumerationFieldType(size=35)

    def tearDown(self):
        del self._ft

    def test_create_from_int_ft(self):
        int_ft = bt2.IntegerFieldType(23)
        self._ft = bt2.EnumerationFieldType(int_ft)

    def test_create_from_invalid_type(self):
        with self.assertRaises(TypeError):
            self._ft = bt2.EnumerationFieldType('coucou')

    def test_create_from_invalid_ft(self):
        with self.assertRaises(TypeError):
            ft = bt2.FloatingPointNumberFieldType()
            self._ft = bt2.EnumerationFieldType(ft)

    def test_create_full(self):
        ft = bt2.EnumerationFieldType(size=24, alignment=16,
                                      byte_order=bt2.ByteOrder.BIG_ENDIAN,
                                      is_signed=True, base=bt2.Base.OCTAL,
                                      encoding=bt2.Encoding.NONE,
                                      mapped_clock_class=None)
        self.assertEqual(ft.size, 24)
        self.assertEqual(ft.alignment, 16)
        self.assertEqual(ft.byte_order, bt2.ByteOrder.BIG_ENDIAN)
        self.assertTrue(ft.is_signed)
        self.assertEqual(ft.base, bt2.Base.OCTAL)
        self.assertEqual(ft.encoding, bt2.Encoding.NONE)
        #self.assertIsNone(ft.mapped_clock_class)

    def test_integer_field_type_prop(self):
        int_ft = bt2.IntegerFieldType(23)
        enum_ft = bt2.EnumerationFieldType(int_ft)
        self.assertEqual(enum_ft.integer_field_type.addr, int_ft.addr)

    def test_add_mapping_simple(self):
        self._ft.add_mapping('hello', 24)
        mapping = self._ft[0]
        self.assertEqual(mapping.name, 'hello')
        self.assertEqual(mapping.lower, 24)
        self.assertEqual(mapping.upper, 24)

    def test_add_mapping_simple_kwargs(self):
        self._ft.add_mapping(name='hello', lower=17, upper=23)
        mapping = self._ft[0]
        self.assertEqual(mapping.name, 'hello')
        self.assertEqual(mapping.lower, 17)
        self.assertEqual(mapping.upper, 23)

    def test_add_mapping_range(self):
        self._ft.add_mapping('hello', 21, 199)
        mapping = self._ft[0]
        self.assertEqual(mapping.name, 'hello')
        self.assertEqual(mapping.lower, 21)
        self.assertEqual(mapping.upper, 199)

    def test_add_mapping_invalid_name(self):
        with self.assertRaises(TypeError):
            self._ft.add_mapping(17, 21, 199)

    def test_add_mapping_invalid_signedness_lower(self):
        with self.assertRaises(ValueError):
            self._ft.add_mapping('hello', -21, 199)

    def test_add_mapping_invalid_signedness_upper(self):
        with self.assertRaises(ValueError):
            self._ft.add_mapping('hello', 21, -199)

    def test_add_mapping_simple_signed(self):
        self._ft.is_signed = True
        self._ft.add_mapping('hello', -24)
        mapping = self._ft[0]
        self.assertEqual(mapping.name, 'hello')
        self.assertEqual(mapping.lower, -24)
        self.assertEqual(mapping.upper, -24)

    def test_add_mapping_range_signed(self):
        self._ft.is_signed = True
        self._ft.add_mapping('hello', -21, 199)
        mapping = self._ft[0]
        self.assertEqual(mapping.name, 'hello')
        self.assertEqual(mapping.lower, -21)
        self.assertEqual(mapping.upper, 199)

    def test_iadd(self):
        enum_ft = bt2.EnumerationFieldType(size=16)
        enum_ft.add_mapping('c', 4, 5)
        enum_ft.add_mapping('d', 6, 18)
        enum_ft.add_mapping('e', 20, 27)
        self._ft.add_mapping('a', 0, 2)
        self._ft.add_mapping('b', 3)
        self._ft += enum_ft
        self.assertEqual(self._ft[0].name, 'a')
        self.assertEqual(self._ft[0].lower, 0)
        self.assertEqual(self._ft[0].upper, 2)
        self.assertEqual(self._ft[1].name, 'b')
        self.assertEqual(self._ft[1].lower, 3)
        self.assertEqual(self._ft[1].upper, 3)
        self.assertEqual(self._ft[2].name, 'c')
        self.assertEqual(self._ft[2].lower, 4)
        self.assertEqual(self._ft[2].upper, 5)
        self.assertEqual(self._ft[3].name, 'd')
        self.assertEqual(self._ft[3].lower, 6)
        self.assertEqual(self._ft[3].upper, 18)
        self.assertEqual(self._ft[4].name, 'e')
        self.assertEqual(self._ft[4].lower, 20)
        self.assertEqual(self._ft[4].upper, 27)

    def test_bool_op(self):
        self.assertFalse(self._ft)
        self._ft.add_mapping('a', 0)
        self.assertTrue(self._ft)

    def test_len(self):
        self._ft.add_mapping('a', 0)
        self._ft.add_mapping('b', 1)
        self._ft.add_mapping('c', 2)
        self.assertEqual(len(self._ft), 3)

    def test_getitem(self):
        self._ft.add_mapping('a', 0)
        self._ft.add_mapping('b', 1, 3)
        self._ft.add_mapping('c', 5)
        mapping = self._ft[1]
        self.assertEqual(mapping.name, 'b')
        self.assertEqual(mapping.lower, 1)
        self.assertEqual(mapping.upper, 3)

    def test_iter(self):
        mappings = (
            ('a', 1, 5),
            ('b', 10, 17),
            ('c', 20, 1504),
            ('d', 22510, 99999),
        )

        for mapping in mappings:
            self._ft.add_mapping(*mapping)

        for ft_mapping, mapping in zip(self._ft, mappings):
            self.assertEqual(ft_mapping.name, mapping[0])
            self.assertEqual(ft_mapping.lower, mapping[1])
            self.assertEqual(ft_mapping.upper, mapping[2])

    def test_mapping_eq(self):
        enum1 = bt2.EnumerationFieldType(size=32)
        enum2 = bt2.EnumerationFieldType(size=16)
        enum1.add_mapping('b', 1, 3)
        enum2.add_mapping('b', 1, 3)
        self.assertEqual(enum1[0], enum2[0])

    def test_mapping_eq_invalid(self):
        enum1 = bt2.EnumerationFieldType(size=32)
        enum1.add_mapping('b', 1, 3)
        self.assertNotEqual(enum1[0], 23)

    def _test_find_by_name(self, ft):
        ft.add_mapping('a', 0)
        ft.add_mapping('b', 1, 3)
        ft.add_mapping('a', 5)
        ft.add_mapping('a', 17, 144)
        ft.add_mapping('C', 5)
        mapping_iter = ft.mappings_by_name('a')
        mappings = list(mapping_iter)
        a0 = False
        a5 = False
        a17_144 = False
        i = 0

        for mapping in mappings:
            i += 1
            self.assertEqual(mapping.name, 'a')

            if mapping.lower == 0 and mapping.upper == 0:
                a0 = True
            elif mapping.lower == 5 and mapping.upper == 5:
                a5 = True
            elif mapping.lower == 17 and mapping.upper == 144:
                a17_144 = True

        self.assertEqual(i, 3)
        self.assertTrue(a0)
        self.assertTrue(a5)
        self.assertTrue(a17_144)

    def test_find_by_name_signed(self):
        self._test_find_by_name(bt2.EnumerationFieldType(size=8, is_signed=True))

    def test_find_by_name_unsigned(self):
        self._test_find_by_name(bt2.EnumerationFieldType(size=8))

    def _test_find_by_value(self, ft):
        ft.add_mapping('a', 0)
        ft.add_mapping('b', 1, 3)
        ft.add_mapping('c', 5, 19)
        ft.add_mapping('d', 8, 15)
        ft.add_mapping('e', 10, 21)
        ft.add_mapping('f', 0)
        ft.add_mapping('g', 14)
        mapping_iter = ft.mappings_by_value(14)
        mappings = list(mapping_iter)
        c = False
        d = False
        e = False
        g = False
        i = 0

        for mapping in mappings:
            i += 1

            if mapping.name == 'c':
                c = True
            elif mapping.name == 'd':
                d = True
            elif mapping.name == 'e':
                e = True
            elif mapping.name == 'g':
                g = True

        self.assertEqual(i, 4)
        self.assertTrue(c)
        self.assertTrue(d)
        self.assertTrue(e)
        self.assertTrue(g)

    def test_find_by_value_signed(self):
        self._test_find_by_value(bt2.EnumerationFieldType(size=8, is_signed=True))

    def test_find_by_value_unsigned(self):
        self._test_find_by_value(bt2.EnumerationFieldType(size=8))

    def test_create_field(self):
        self._ft.add_mapping('c', 4, 5)
        field = self._ft()
        self.assertIsInstance(field, bt2.fields._EnumerationField)

    def test_create_field_init(self):
        self._ft.add_mapping('c', 4, 5)
        field = self._ft(4)
        self.assertEqual(field, 4)


class StringFieldTypeTestCase(_TestCopySimple, _TestInvalidEq,
                              unittest.TestCase):
    def setUp(self):
        self._ft = bt2.StringFieldType()

    def tearDown(self):
        del self._ft

    def test_create_default(self):
        pass

    def test_create_full(self):
        ft = bt2.StringFieldType(encoding=bt2.Encoding.UTF8)
        self.assertEqual(ft.encoding, bt2.Encoding.UTF8)

    def test_assign_encoding(self):
        self._ft.encoding = bt2.Encoding.UTF8
        self.assertEqual(self._ft.encoding, bt2.Encoding.UTF8)

    def test_assign_invalid_encoding(self):
        with self.assertRaises(TypeError):
            self._ft.encoding = 'yes'

    def test_create_field(self):
        field = self._ft()
        self.assertIsInstance(field, bt2.fields._StringField)

    def test_create_field_init(self):
        field = self._ft('hola')
        self.assertEqual(field, 'hola')


class _TestFieldContainer(_TestInvalidEq, _TestCopySimple):
    def test_append_field(self):
        int_field_type = bt2.IntegerFieldType(32)
        self._ft.append_field('int32', int_field_type)
        field_type = self._ft['int32']
        self.assertEqual(field_type, int_field_type)

    def test_append_field_kwargs(self):
        int_field_type = bt2.IntegerFieldType(32)
        self._ft.append_field(name='int32', field_type=int_field_type)
        field_type = self._ft['int32']
        self.assertEqual(field_type, int_field_type)

    def test_append_field_invalid_name(self):
        with self.assertRaises(TypeError):
            self._ft.append_field(23, bt2.StringFieldType())

    def test_append_field_invalid_field_type(self):
        with self.assertRaises(TypeError):
            self._ft.append_field('yes', object())

    def test_iadd(self):
        struct_ft = bt2.StructureFieldType()
        c_field_type = bt2.StringFieldType()
        d_field_type = bt2.EnumerationFieldType(size=32)
        e_field_type = bt2.StructureFieldType()
        struct_ft.append_field('c_string', c_field_type)
        struct_ft.append_field('d_enum', d_field_type)
        struct_ft.append_field('e_struct', e_field_type)
        a_field_type = bt2.FloatingPointNumberFieldType()
        b_field_type = bt2.IntegerFieldType(17)
        self._ft.append_field('a_float', a_field_type)
        self._ft.append_field('b_int', b_field_type)
        self._ft += struct_ft
        self.assertEqual(self._ft['a_float'], a_field_type)
        self.assertEqual(self._ft['b_int'], b_field_type)
        self.assertEqual(self._ft['c_string'], c_field_type)
        self.assertEqual(self._ft['d_enum'], d_field_type)
        self.assertEqual(self._ft['e_struct'], e_field_type)

    def test_bool_op(self):
        self.assertFalse(self._ft)
        self._ft.append_field('a', bt2.StringFieldType())
        self.assertTrue(self._ft)

    def test_len(self):
        ft = bt2.StringFieldType()
        self._ft.append_field('a', ft)
        self._ft.append_field('b', ft)
        self._ft.append_field('c', ft)
        self.assertEqual(len(self._ft), 3)

    def test_getitem(self):
        a_ft = bt2.IntegerFieldType(32)
        b_ft = bt2.StringFieldType()
        c_ft = bt2.FloatingPointNumberFieldType()
        self._ft.append_field('a', a_ft)
        self._ft.append_field('b', b_ft)
        self._ft.append_field('c', c_ft)
        self.assertEqual(self._ft['b'], b_ft)

    def test_getitem_invalid_key_type(self):
        with self.assertRaises(TypeError):
            self._ft[0]

    def test_getitem_invalid_key(self):
        with self.assertRaises(KeyError):
            self._ft['no way']

    def test_contains(self):
        self.assertFalse('a' in self._ft)
        self._ft.append_field('a', bt2.StringFieldType())
        self.assertTrue('a' in self._ft)

    def test_iter(self):
        a_ft = bt2.IntegerFieldType(32)
        b_ft = bt2.StringFieldType()
        c_ft = bt2.FloatingPointNumberFieldType()
        fields = (
            ('a', a_ft),
            ('b', b_ft),
            ('c', c_ft),
        )

        for field in fields:
            self._ft.append_field(*field)

        for (name, ft_field_type), field in zip(self._ft.items(), fields):
            self.assertEqual(name, field[0])
            self.assertEqual(ft_field_type, field[1])

    def test_at_index(self):
        a_ft = bt2.IntegerFieldType(32)
        b_ft = bt2.StringFieldType()
        c_ft = bt2.FloatingPointNumberFieldType()
        self._ft.append_field('c', c_ft)
        self._ft.append_field('a', a_ft)
        self._ft.append_field('b', b_ft)
        self.assertEqual(self._ft.at_index(1), a_ft)

    def test_at_index_invalid(self):
        self._ft.append_field('c', bt2.IntegerFieldType(32))

        with self.assertRaises(TypeError):
            self._ft.at_index('yes')

    def test_at_index_out_of_bounds_after(self):
        self._ft.append_field('c', bt2.IntegerFieldType(32))

        with self.assertRaises(IndexError):
            self._ft.at_index(len(self._ft))


class StructureFieldTypeTestCase(_TestFieldContainer, unittest.TestCase):
    def setUp(self):
        self._ft = bt2.StructureFieldType()

    def tearDown(self):
        del self._ft

    def test_create_default(self):
        self.assertEqual(self._ft.alignment, 1)

    def test_create_with_min_alignment(self):
        ft = bt2.StructureFieldType(8)
        self.assertEqual(ft.alignment, 8)

    def test_assign_alignment(self):
        with self.assertRaises(AttributeError):
            self._ft.alignment = 32

    def test_assign_min_alignment(self):
        self._ft.min_alignment = 64
        self.assertTrue(self._ft.alignment >= 64)

    def test_assign_invalid_min_alignment(self):
        with self.assertRaises(ValueError):
            self._ft.min_alignment = 23

    def test_assign_get_min_alignment(self):
        with self.assertRaises(AttributeError):
            self._ft.min_alignment

    def test_create_field(self):
        field = self._ft()
        self.assertIsInstance(field, bt2.fields._StructureField)

    def test_create_field_init_invalid(self):
        with self.assertRaises(bt2.Error):
            field = self._ft(23)


class VariantFieldTypeTestCase(_TestFieldContainer, unittest.TestCase):
    def setUp(self):
        self._ft = bt2.VariantFieldType('path.to.tag')

    def tearDown(self):
        del self._ft

    def test_create_default(self):
        self.assertEqual(self._ft.tag_name, 'path.to.tag')

    def test_create_invalid_tag_name(self):
        with self.assertRaises(TypeError):
            self._ft = bt2.VariantFieldType(23)

    def test_assign_tag_name(self):
        self._ft.tag_name = 'a.different.tag'
        self.assertEqual(self._ft.tag_name, 'a.different.tag')

    def test_assign_invalid_tag_name(self):
        with self.assertRaises(TypeError):
            self._ft.tag_name = -17


class ArrayFieldTypeTestCase(_TestInvalidEq, _TestCopySimple,
                             unittest.TestCase):
    def setUp(self):
        self._elem_ft = bt2.IntegerFieldType(23)
        self._ft = bt2.ArrayFieldType(self._elem_ft, 45)

    def tearDown(self):
        del self._ft
        del self._elem_ft

    def test_create_default(self):
        self.assertEqual(self._ft.element_field_type, self._elem_ft)
        self.assertEqual(self._ft.length, 45)

    def test_create_invalid_field_type(self):
        with self.assertRaises(TypeError):
            self._ft = bt2.ArrayFieldType(object(), 45)

    def test_create_invalid_length(self):
        with self.assertRaises(ValueError):
            self._ft = bt2.ArrayFieldType(bt2.StringFieldType(), -17)

    def test_create_invalid_length_type(self):
        with self.assertRaises(TypeError):
            self._ft = bt2.ArrayFieldType(bt2.StringFieldType(), 'the length')

    def test_create_field(self):
        field = self._ft()
        self.assertIsInstance(field, bt2.fields._ArrayField)

    def test_create_field_init_invalid(self):
        with self.assertRaises(bt2.Error):
            field = self._ft(23)


class SequenceFieldTypeTestCase(_TestInvalidEq, _TestCopySimple,
                                unittest.TestCase):
    def setUp(self):
        self._elem_ft = bt2.IntegerFieldType(23)
        self._ft = bt2.SequenceFieldType(self._elem_ft, 'the.length')

    def tearDown(self):
        del self._ft
        del self._elem_ft

    def test_create_default(self):
        self.assertEqual(self._ft.element_field_type, self._elem_ft)
        self.assertEqual(self._ft.length_name, 'the.length')

    def test_create_invalid_field_type(self):
        with self.assertRaises(TypeError):
            self._ft = bt2.ArrayFieldType(object(), 'the.length')

    def test_create_invalid_length_type(self):
        with self.assertRaises(TypeError):
            self._ft = bt2.SequenceFieldType(bt2.StringFieldType(), 17)

    def test_create_field(self):
        field = self._ft()
        self.assertIsInstance(field, bt2.fields._SequenceField)

    def test_create_field_init_invalid(self):
        with self.assertRaises(bt2.Error):
            field = self._ft(23)
