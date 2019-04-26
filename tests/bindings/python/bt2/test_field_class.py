import bt2.field
import unittest
import copy
import bt2


class _TestCopySimple:
    def _test_copy(self, cpy):
        self.assertIsNot(cpy, self._fc)
        self.assertNotEqual(cpy.addr, self._fc.addr)
        self.assertEqual(cpy, self._fc)

    def test_copy(self):
        cpy = copy.copy(self._fc)
        self._test_copy(cpy)

    def test_deepcopy(self):
        cpy = copy.deepcopy(self._fc)
        self._test_copy(cpy)


class _TestAlignmentProp:
    def test_assign_alignment(self):
        self._fc.alignment = 32
        self.assertEqual(self._fc.alignment, 32)

    def test_assign_invalid_alignment(self):
        with self.assertRaises(ValueError):
            self._fc.alignment = 23


class _TestByteOrderProp:
    def test_assign_byte_order(self):
        self._fc.byte_order = bt2.ByteOrder.LITTLE_ENDIAN
        self.assertEqual(self._fc.byte_order, bt2.ByteOrder.LITTLE_ENDIAN)

    def test_assign_invalid_byte_order(self):
        with self.assertRaises(TypeError):
            self._fc.byte_order = 'hey'


class _TestInvalidEq:
    def test_eq_invalid(self):
        self.assertFalse(self._fc == 23)


class _TestIntegerFieldClassProps:
    def test_size_prop(self):
        self.assertEqual(self._fc.size, 35)

    def test_assign_signed(self):
        self._fc.is_signed = True
        self.assertTrue(self._fc.is_signed)

    def test_assign_invalid_signed(self):
        with self.assertRaises(TypeError):
            self._fc.is_signed = 23

    def test_assign_base(self):
        self._fc.base = bt2.Base.HEXADECIMAL
        self.assertEqual(self._fc.base, bt2.Base.HEXADECIMAL)

    def test_assign_invalid_base(self):
        with self.assertRaises(TypeError):
            self._fc.base = 'hey'

    def test_assign_encoding(self):
        self._fc.encoding = bt2.Encoding.UTF8
        self.assertEqual(self._fc.encoding, bt2.Encoding.UTF8)

    def test_assign_invalid_encoding(self):
        with self.assertRaises(TypeError):
            self._fc.encoding = 'hey'

    def test_assign_mapped_clock_class(self):
        cc = bt2.ClockClass('name', 1000)
        self._fc.mapped_clock_class = cc
        self.assertEqual(self._fc.mapped_clock_class, cc)

    def test_assign_invalid_mapped_clock_class(self):
        with self.assertRaises(TypeError):
            self._fc.mapped_clock_class = object()


@unittest.skip("this is broken")
class IntegerFieldClassTestCase(_TestIntegerFieldClassProps, _TestCopySimple,
                               _TestAlignmentProp, _TestByteOrderProp,
                               _TestInvalidEq, unittest.TestCase):
    def setUp(self):
        self._fc = bt2.IntegerFieldClass(35)

    def tearDown(self):
        del self._fc

    def test_create_default(self):
        self.assertEqual(self._fc.size, 35)
        self.assertIsNone(self._fc.mapped_clock_class)

    def test_create_invalid_size(self):
        with self.assertRaises(TypeError):
            fc = bt2.IntegerFieldClass('yes')

    def test_create_neg_size(self):
        with self.assertRaises(ValueError):
            fc = bt2.IntegerFieldClass(-2)

    def test_create_neg_zero(self):
        with self.assertRaises(ValueError):
            fc = bt2.IntegerFieldClass(0)

    def test_create_full(self):
        cc = bt2.ClockClass('name', 1000)
        fc = bt2.IntegerFieldClass(24, alignment=16,
                                  byte_order=bt2.ByteOrder.BIG_ENDIAN,
                                  is_signed=True, base=bt2.Base.OCTAL,
                                  encoding=bt2.Encoding.NONE,
                                  mapped_clock_class=cc)
        self.assertEqual(fc.size, 24)
        self.assertEqual(fc.alignment, 16)
        self.assertEqual(fc.byte_order, bt2.ByteOrder.BIG_ENDIAN)
        self.assertTrue(fc.is_signed)
        self.assertEqual(fc.base, bt2.Base.OCTAL)
        self.assertEqual(fc.encoding, bt2.Encoding.NONE)
        self.assertEqual(fc.mapped_clock_class, cc)

    def test_create_field(self):
        field = self._fc()
        self.assertIsInstance(field, bt2.field._IntegerField)

    def test_create_field_init(self):
        field = self._fc(23)
        self.assertEqual(field, 23)


@unittest.skip("this is broken")
class FloatingPointNumberFieldClassTestCase(_TestCopySimple, _TestAlignmentProp,
                                           _TestByteOrderProp, _TestInvalidEq,
                                           unittest.TestCase):
    def setUp(self):
        self._fc = bt2.FloatingPointNumberFieldClass()

    def tearDown(self):
        del self._fc

    def test_create_default(self):
        pass

    def test_create_full(self):
        fc = bt2.FloatingPointNumberFieldClass(alignment=16,
                                              byte_order=bt2.ByteOrder.BIG_ENDIAN,
                                              exponent_size=11,
                                              mantissa_size=53)
        self.assertEqual(fc.alignment, 16)
        self.assertEqual(fc.byte_order, bt2.ByteOrder.BIG_ENDIAN)
        self.assertEqual(fc.exponent_size, 11)
        self.assertEqual(fc.mantissa_size, 53)

    def test_assign_exponent_size(self):
        self._fc.exponent_size = 8
        self.assertEqual(self._fc.exponent_size, 8)

    def test_assign_invalid_exponent_size(self):
        with self.assertRaises(TypeError):
            self._fc.exponent_size = 'yes'

    def test_assign_mantissa_size(self):
        self._fc.mantissa_size = 24
        self.assertEqual(self._fc.mantissa_size, 24)

    def test_assign_invalid_mantissa_size(self):
        with self.assertRaises(TypeError):
            self._fc.mantissa_size = 'no'

    def test_create_field(self):
        field = self._fc()
        self.assertIsInstance(field, bt2.field._FloatingPointNumberField)

    def test_create_field_init(self):
        field = self._fc(17.5)
        self.assertEqual(field, 17.5)


@unittest.skip("this is broken")
class EnumerationFieldClassTestCase(_TestIntegerFieldClassProps, _TestInvalidEq,
                                   _TestCopySimple, _TestAlignmentProp,
                                   _TestByteOrderProp, unittest.TestCase):
    def setUp(self):
        self._fc = bt2.EnumerationFieldClass(size=35)

    def tearDown(self):
        del self._fc

    def test_create_from_int_fc(self):
        int_fc = bt2.IntegerFieldClass(23)
        self._fc = bt2.EnumerationFieldClass(int_fc)

    def test_create_from_invalid_type(self):
        with self.assertRaises(TypeError):
            self._fc = bt2.EnumerationFieldClass('coucou')

    def test_create_from_invalid_fc(self):
        with self.assertRaises(TypeError):
            fc = bt2.FloatingPointNumberFieldClass()
            self._fc = bt2.EnumerationFieldClass(fc)

    def test_create_full(self):
        fc = bt2.EnumerationFieldClass(size=24, alignment=16,
                                      byte_order=bt2.ByteOrder.BIG_ENDIAN,
                                      is_signed=True, base=bt2.Base.OCTAL,
                                      encoding=bt2.Encoding.NONE,
                                      mapped_clock_class=None)
        self.assertEqual(fc.size, 24)
        self.assertEqual(fc.alignment, 16)
        self.assertEqual(fc.byte_order, bt2.ByteOrder.BIG_ENDIAN)
        self.assertTrue(fc.is_signed)
        self.assertEqual(fc.base, bt2.Base.OCTAL)
        self.assertEqual(fc.encoding, bt2.Encoding.NONE)
        #self.assertIsNone(fc.mapped_clock_class)

    def test_integer_field_class_prop(self):
        int_fc = bt2.IntegerFieldClass(23)
        enum_fc = bt2.EnumerationFieldClass(int_fc)
        self.assertEqual(enum_fc.integer_field_class.addr, int_fc.addr)

    def test_add_mapping_simple(self):
        self._fc.add_mapping('hello', 24)
        mapping = self._fc[0]
        self.assertEqual(mapping.name, 'hello')
        self.assertEqual(mapping.lower, 24)
        self.assertEqual(mapping.upper, 24)

    def test_add_mapping_simple_kwargs(self):
        self._fc.add_mapping(name='hello', lower=17, upper=23)
        mapping = self._fc[0]
        self.assertEqual(mapping.name, 'hello')
        self.assertEqual(mapping.lower, 17)
        self.assertEqual(mapping.upper, 23)

    def test_add_mapping_range(self):
        self._fc.add_mapping('hello', 21, 199)
        mapping = self._fc[0]
        self.assertEqual(mapping.name, 'hello')
        self.assertEqual(mapping.lower, 21)
        self.assertEqual(mapping.upper, 199)

    def test_add_mapping_invalid_name(self):
        with self.assertRaises(TypeError):
            self._fc.add_mapping(17, 21, 199)

    def test_add_mapping_invalid_signedness_lower(self):
        with self.assertRaises(ValueError):
            self._fc.add_mapping('hello', -21, 199)

    def test_add_mapping_invalid_signedness_upper(self):
        with self.assertRaises(ValueError):
            self._fc.add_mapping('hello', 21, -199)

    def test_add_mapping_simple_signed(self):
        self._fc.is_signed = True
        self._fc.add_mapping('hello', -24)
        mapping = self._fc[0]
        self.assertEqual(mapping.name, 'hello')
        self.assertEqual(mapping.lower, -24)
        self.assertEqual(mapping.upper, -24)

    def test_add_mapping_range_signed(self):
        self._fc.is_signed = True
        self._fc.add_mapping('hello', -21, 199)
        mapping = self._fc[0]
        self.assertEqual(mapping.name, 'hello')
        self.assertEqual(mapping.lower, -21)
        self.assertEqual(mapping.upper, 199)

    def test_iadd(self):
        enum_fc = bt2.EnumerationFieldClass(size=16)
        enum_fc.add_mapping('c', 4, 5)
        enum_fc.add_mapping('d', 6, 18)
        enum_fc.add_mapping('e', 20, 27)
        self._fc.add_mapping('a', 0, 2)
        self._fc.add_mapping('b', 3)
        self._fc += enum_fc
        self.assertEqual(self._fc[0].name, 'a')
        self.assertEqual(self._fc[0].lower, 0)
        self.assertEqual(self._fc[0].upper, 2)
        self.assertEqual(self._fc[1].name, 'b')
        self.assertEqual(self._fc[1].lower, 3)
        self.assertEqual(self._fc[1].upper, 3)
        self.assertEqual(self._fc[2].name, 'c')
        self.assertEqual(self._fc[2].lower, 4)
        self.assertEqual(self._fc[2].upper, 5)
        self.assertEqual(self._fc[3].name, 'd')
        self.assertEqual(self._fc[3].lower, 6)
        self.assertEqual(self._fc[3].upper, 18)
        self.assertEqual(self._fc[4].name, 'e')
        self.assertEqual(self._fc[4].lower, 20)
        self.assertEqual(self._fc[4].upper, 27)

    def test_bool_op(self):
        self.assertFalse(self._fc)
        self._fc.add_mapping('a', 0)
        self.assertTrue(self._fc)

    def test_len(self):
        self._fc.add_mapping('a', 0)
        self._fc.add_mapping('b', 1)
        self._fc.add_mapping('c', 2)
        self.assertEqual(len(self._fc), 3)

    def test_getitem(self):
        self._fc.add_mapping('a', 0)
        self._fc.add_mapping('b', 1, 3)
        self._fc.add_mapping('c', 5)
        mapping = self._fc[1]
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
            self._fc.add_mapping(*mapping)

        for fc_mapping, mapping in zip(self._fc, mappings):
            self.assertEqual(fc_mapping.name, mapping[0])
            self.assertEqual(fc_mapping.lower, mapping[1])
            self.assertEqual(fc_mapping.upper, mapping[2])

    def test_mapping_eq(self):
        enum1 = bt2.EnumerationFieldClass(size=32)
        enum2 = bt2.EnumerationFieldClass(size=16)
        enum1.add_mapping('b', 1, 3)
        enum2.add_mapping('b', 1, 3)
        self.assertEqual(enum1[0], enum2[0])

    def test_mapping_eq_invalid(self):
        enum1 = bt2.EnumerationFieldClass(size=32)
        enum1.add_mapping('b', 1, 3)
        self.assertNotEqual(enum1[0], 23)

    def _test_find_by_name(self, fc):
        fc.add_mapping('a', 0)
        fc.add_mapping('b', 1, 3)
        fc.add_mapping('a', 5)
        fc.add_mapping('a', 17, 144)
        fc.add_mapping('C', 5)
        mapping_iter = fc.mappings_by_name('a')
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
        self._test_find_by_name(bt2.EnumerationFieldClass(size=8, is_signed=True))

    def test_find_by_name_unsigned(self):
        self._test_find_by_name(bt2.EnumerationFieldClass(size=8))

    def _test_find_by_value(self, fc):
        fc.add_mapping('a', 0)
        fc.add_mapping('b', 1, 3)
        fc.add_mapping('c', 5, 19)
        fc.add_mapping('d', 8, 15)
        fc.add_mapping('e', 10, 21)
        fc.add_mapping('f', 0)
        fc.add_mapping('g', 14)
        mapping_iter = fc.mappings_by_value(14)
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
        self._test_find_by_value(bt2.EnumerationFieldClass(size=8, is_signed=True))

    def test_find_by_value_unsigned(self):
        self._test_find_by_value(bt2.EnumerationFieldClass(size=8))

    def test_create_field(self):
        self._fc.add_mapping('c', 4, 5)
        field = self._fc()
        self.assertIsInstance(field, bt2.field._EnumerationField)

    def test_create_field_init(self):
        self._fc.add_mapping('c', 4, 5)
        field = self._fc(4)
        self.assertEqual(field, 4)


@unittest.skip("this is broken")
class StringFieldClassTestCase(_TestCopySimple, _TestInvalidEq,
                              unittest.TestCase):
    def setUp(self):
        self._fc = bt2.StringFieldClass()

    def tearDown(self):
        del self._fc

    def test_create_default(self):
        pass

    def test_create_full(self):
        fc = bt2.StringFieldClass(encoding=bt2.Encoding.UTF8)
        self.assertEqual(fc.encoding, bt2.Encoding.UTF8)

    def test_assign_encoding(self):
        self._fc.encoding = bt2.Encoding.UTF8
        self.assertEqual(self._fc.encoding, bt2.Encoding.UTF8)

    def test_assign_invalid_encoding(self):
        with self.assertRaises(TypeError):
            self._fc.encoding = 'yes'

    def test_create_field(self):
        field = self._fc()
        self.assertIsInstance(field, bt2.field._StringField)

    def test_create_field_init(self):
        field = self._fc('hola')
        self.assertEqual(field, 'hola')


class _TestFieldContainer(_TestInvalidEq, _TestCopySimple):
    def test_append_field(self):
        int_field_class = bt2.IntegerFieldClass(32)
        self._fc.append_field('int32', int_field_class)
        field_class = self._fc['int32']
        self.assertEqual(field_class, int_field_class)

    def test_append_field_kwargs(self):
        int_field_class = bt2.IntegerFieldClass(32)
        self._fc.append_field(name='int32', field_class=int_field_class)
        field_class = self._fc['int32']
        self.assertEqual(field_class, int_field_class)

    def test_append_field_invalid_name(self):
        with self.assertRaises(TypeError):
            self._fc.append_field(23, bt2.StringFieldClass())

    def test_append_field_invalid_field_class(self):
        with self.assertRaises(TypeError):
            self._fc.append_field('yes', object())

    def test_iadd(self):
        struct_fc = bt2.StructureFieldClass()
        c_field_class = bt2.StringFieldClass()
        d_field_class = bt2.EnumerationFieldClass(size=32)
        e_field_class = bt2.StructureFieldClass()
        struct_fc.append_field('c_string', c_field_class)
        struct_fc.append_field('d_enum', d_field_class)
        struct_fc.append_field('e_struct', e_field_class)
        a_field_class = bt2.FloatingPointNumberFieldClass()
        b_field_class = bt2.IntegerFieldClass(17)
        self._fc.append_field('a_float', a_field_class)
        self._fc.append_field('b_int', b_field_class)
        self._fc += struct_fc
        self.assertEqual(self._fc['a_float'], a_field_class)
        self.assertEqual(self._fc['b_int'], b_field_class)
        self.assertEqual(self._fc['c_string'], c_field_class)
        self.assertEqual(self._fc['d_enum'], d_field_class)
        self.assertEqual(self._fc['e_struct'], e_field_class)

    def test_bool_op(self):
        self.assertFalse(self._fc)
        self._fc.append_field('a', bt2.StringFieldClass())
        self.assertTrue(self._fc)

    def test_len(self):
        fc = bt2.StringFieldClass()
        self._fc.append_field('a', fc)
        self._fc.append_field('b', fc)
        self._fc.append_field('c', fc)
        self.assertEqual(len(self._fc), 3)

    def test_getitem(self):
        a_fc = bt2.IntegerFieldClass(32)
        b_fc = bt2.StringFieldClass()
        c_fc = bt2.FloatingPointNumberFieldClass()
        self._fc.append_field('a', a_fc)
        self._fc.append_field('b', b_fc)
        self._fc.append_field('c', c_fc)
        self.assertEqual(self._fc['b'], b_fc)

    def test_getitem_invalid_key_type(self):
        with self.assertRaises(TypeError):
            self._fc[0]

    def test_getitem_invalid_key(self):
        with self.assertRaises(KeyError):
            self._fc['no way']

    def test_contains(self):
        self.assertFalse('a' in self._fc)
        self._fc.append_field('a', bt2.StringFieldClass())
        self.assertTrue('a' in self._fc)

    def test_iter(self):
        a_fc = bt2.IntegerFieldClass(32)
        b_fc = bt2.StringFieldClass()
        c_fc = bt2.FloatingPointNumberFieldClass()
        fields = (
            ('a', a_fc),
            ('b', b_fc),
            ('c', c_fc),
        )

        for field in fields:
            self._fc.append_field(*field)

        for (name, fc_field_class), field in zip(self._fc.items(), fields):
            self.assertEqual(name, field[0])
            self.assertEqual(fc_field_class, field[1])

    def test_at_index(self):
        a_fc = bt2.IntegerFieldClass(32)
        b_fc = bt2.StringFieldClass()
        c_fc = bt2.FloatingPointNumberFieldClass()
        self._fc.append_field('c', c_fc)
        self._fc.append_field('a', a_fc)
        self._fc.append_field('b', b_fc)
        self.assertEqual(self._fc.at_index(1), a_fc)

    def test_at_index_invalid(self):
        self._fc.append_field('c', bt2.IntegerFieldClass(32))

        with self.assertRaises(TypeError):
            self._fc.at_index('yes')

    def test_at_index_out_of_bounds_after(self):
        self._fc.append_field('c', bt2.IntegerFieldClass(32))

        with self.assertRaises(IndexError):
            self._fc.at_index(len(self._fc))


@unittest.skip("this is broken")
class StructureFieldClassTestCase(_TestFieldContainer, unittest.TestCase):
    def setUp(self):
        self._fc = bt2.StructureFieldClass()

    def tearDown(self):
        del self._fc

    def test_create_default(self):
        self.assertEqual(self._fc.alignment, 1)

    def test_create_with_min_alignment(self):
        fc = bt2.StructureFieldClass(8)
        self.assertEqual(fc.alignment, 8)

    def test_assign_alignment(self):
        with self.assertRaises(AttributeError):
            self._fc.alignment = 32

    def test_assign_min_alignment(self):
        self._fc.min_alignment = 64
        self.assertTrue(self._fc.alignment >= 64)

    def test_assign_invalid_min_alignment(self):
        with self.assertRaises(ValueError):
            self._fc.min_alignment = 23

    def test_assign_get_min_alignment(self):
        with self.assertRaises(AttributeError):
            self._fc.min_alignment

    def test_create_field(self):
        field = self._fc()
        self.assertIsInstance(field, bt2.field._StructureField)

    def test_create_field_init_invalid(self):
        with self.assertRaises(bt2.Error):
            field = self._fc(23)


@unittest.skip("this is broken")
class VariantFieldClassTestCase(_TestFieldContainer, unittest.TestCase):
    def setUp(self):
        self._fc = bt2.VariantFieldClass('path.to.tag')

    def tearDown(self):
        del self._fc

    def test_create_default(self):
        self.assertEqual(self._fc.tag_name, 'path.to.tag')

    def test_create_invalid_tag_name(self):
        with self.assertRaises(TypeError):
            self._fc = bt2.VariantFieldClass(23)

    def test_assign_tag_name(self):
        self._fc.tag_name = 'a.different.tag'
        self.assertEqual(self._fc.tag_name, 'a.different.tag')

    def test_assign_invalid_tag_name(self):
        with self.assertRaises(TypeError):
            self._fc.tag_name = -17


@unittest.skip("this is broken")
class ArrayFieldClassTestCase(_TestInvalidEq, _TestCopySimple,
                             unittest.TestCase):
    def setUp(self):
        self._elem_fc = bt2.IntegerFieldClass(23)
        self._fc = bt2.ArrayFieldClass(self._elem_fc, 45)

    def tearDown(self):
        del self._fc
        del self._elem_fc

    def test_create_default(self):
        self.assertEqual(self._fc.element_field_class, self._elem_fc)
        self.assertEqual(self._fc.length, 45)

    def test_create_invalid_field_class(self):
        with self.assertRaises(TypeError):
            self._fc = bt2.ArrayFieldClass(object(), 45)

    def test_create_invalid_length(self):
        with self.assertRaises(ValueError):
            self._fc = bt2.ArrayFieldClass(bt2.StringFieldClass(), -17)

    def test_create_invalid_length_type(self):
        with self.assertRaises(TypeError):
            self._fc = bt2.ArrayFieldClass(bt2.StringFieldClass(), 'the length')

    def test_create_field(self):
        field = self._fc()
        self.assertIsInstance(field, bt2.field._ArrayField)

    def test_create_field_init_invalid(self):
        with self.assertRaises(bt2.Error):
            field = self._fc(23)


@unittest.skip("this is broken")
class SequenceFieldClassTestCase(_TestInvalidEq, _TestCopySimple,
                                unittest.TestCase):
    def setUp(self):
        self._elem_fc = bt2.IntegerFieldClass(23)
        self._fc = bt2.SequenceFieldClass(self._elem_fc, 'the.length')

    def tearDown(self):
        del self._fc
        del self._elem_fc

    def test_create_default(self):
        self.assertEqual(self._fc.element_field_class, self._elem_fc)
        self.assertEqual(self._fc.length_name, 'the.length')

    def test_create_invalid_field_class(self):
        with self.assertRaises(TypeError):
            self._fc = bt2.ArrayFieldClass(object(), 'the.length')

    def test_create_invalid_length_type(self):
        with self.assertRaises(TypeError):
            self._fc = bt2.SequenceFieldClass(bt2.StringFieldClass(), 17)

    def test_create_field(self):
        field = self._fc()
        self.assertIsInstance(field, bt2.field._SequenceField)

    def test_create_field_init_invalid(self):
        with self.assertRaises(bt2.Error):
            field = self._fc(23)
