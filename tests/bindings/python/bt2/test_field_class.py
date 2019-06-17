#
# Copyright (C) 2019 EfficiOS Inc.
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; only version 2
# of the License.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
#

import bt2.field
import unittest
import bt2
from utils import get_default_trace_class


class _TestIntegerFieldClassProps:
    def test_create_default(self):
        fc = self._create_func()
        self.assertEqual(fc.field_value_range, 64)
        self.assertEqual(fc.preferred_display_base, bt2.IntegerDisplayBase.DECIMAL)

    def test_create_range(self):
        fc = self._create_func(field_value_range=35)
        self.assertEqual(fc.field_value_range, 35)

        fc = self._create_func(36)
        self.assertEqual(fc.field_value_range, 36)

    def test_create_invalid_range(self):
        with self.assertRaises(TypeError):
            self._create_func('yes')

        with self.assertRaises(TypeError):
            self._create_func(field_value_range='yes')

        with self.assertRaises(ValueError):
            self._create_func(field_value_range=-2)

        with self.assertRaises(ValueError):
            self._create_func(field_value_range=0)

    def test_create_base(self):
        fc = self._create_func(preferred_display_base=bt2.IntegerDisplayBase.HEXADECIMAL)
        self.assertEqual(fc.preferred_display_base, bt2.IntegerDisplayBase.HEXADECIMAL)

    def test_create_invalid_base_type(self):
        with self.assertRaises(TypeError):
            self._create_func(preferred_display_base='yes')

    def test_create_invalid_base_value(self):
        with self.assertRaises(ValueError):
            self._create_func(preferred_display_base=444)

    def test_create_full(self):
        fc = self._create_func(24, preferred_display_base=bt2.IntegerDisplayBase.OCTAL)
        self.assertEqual(fc.field_value_range, 24)
        self.assertEqual(fc.preferred_display_base, bt2.IntegerDisplayBase.OCTAL)


class IntegerFieldClassTestCase(_TestIntegerFieldClassProps, unittest.TestCase):
    def setUp(self):
        self._tc = get_default_trace_class()
        self._create_func = self._tc.create_signed_integer_field_class


class RealFieldClassTestCase(unittest.TestCase):
    def setUp(self):
        self._tc = get_default_trace_class()

    def test_create_default(self):
        fc = self._tc.create_real_field_class()
        self.assertFalse(fc.is_single_precision)

    def test_create_is_single_precision(self):
        fc = self._tc.create_real_field_class(is_single_precision=True)
        self.assertTrue(fc.is_single_precision)

    def test_create_invalid_is_single_precision(self):
        with self.assertRaises(TypeError):
            self._tc.create_real_field_class(is_single_precision='hohoho')


# Converts an _EnumerationFieldClassMapping to a list of ranges:
#
#    [(lower0, upper0), (lower1, upper1), ...]

def enum_mapping_to_list(mapping):
    return sorted([(x.lower, x.upper) for x in mapping])


class EnumerationFieldClassTestCase(_TestIntegerFieldClassProps):
    def setUp(self):
        self._tc = get_default_trace_class()

    def test_create_from_invalid_type(self):
        with self.assertRaises(TypeError):
            self._create_func('coucou')

    def test_add_mapping_simple(self):
        self._fc.map_range('hello', 24)
        mapping = self._fc['hello']
        self.assertEqual(mapping.label, 'hello')

        ranges = enum_mapping_to_list(mapping)
        self.assertEqual(ranges, [(24, 24)])

    def test_add_mapping_simple_kwargs(self):
        self._fc.map_range(label='hello', lower=17, upper=23)
        mapping = self._fc['hello']
        self.assertEqual(mapping.label, 'hello')

        ranges = enum_mapping_to_list(mapping)
        self.assertEqual(ranges, [(17, 23)])

    def test_add_mapping_range(self):
        self._fc.map_range('hello', 21, 199)
        mapping = self._fc['hello']
        self.assertEqual(mapping.label, 'hello')

        ranges = enum_mapping_to_list(mapping)
        self.assertEqual(ranges, [(21, 199)])

    def test_add_mapping_invalid_name(self):
        with self.assertRaises(TypeError):
            self._fc.map_range(17, 21, 199)

    def test_iadd(self):
        enum_fc = self._tc.create_signed_enumeration_field_class(field_value_range=16)
        enum_fc.map_range('c', 4, 5)
        enum_fc.map_range('d', 6, 18)
        enum_fc.map_range('e', 20, 27)
        self._fc.map_range('a', 0, 2)
        self._fc.map_range('b', 3)
        self._fc += enum_fc

        self.assertEqual(self._fc['a'].label, 'a')
        self.assertEqual(enum_mapping_to_list(self._fc['a']), [(0, 2)])

        self.assertEqual(self._fc['b'].label, 'b')
        self.assertEqual(enum_mapping_to_list(self._fc['b']), [(3, 3)])

        self.assertEqual(self._fc['c'].label, 'c')
        self.assertEqual(enum_mapping_to_list(self._fc['c']), [(4, 5)])

        self.assertEqual(self._fc['d'].label, 'd')
        self.assertEqual(enum_mapping_to_list(self._fc['d']), [(6, 18)])

        self.assertEqual(self._fc['e'].label, 'e')
        self.assertEqual(enum_mapping_to_list(self._fc['e']), [(20, 27)])

    def test_bool_op(self):
        self.assertFalse(self._fc)
        self._fc.map_range('a', 0)
        self.assertTrue(self._fc)

    def test_len(self):
        self._fc.map_range('a', 0)
        self._fc.map_range('b', 1)
        self._fc.map_range('c', 2)
        self.assertEqual(len(self._fc), 3)

    def test_getitem(self):
        self._fc.map_range('a', 0)
        self._fc.map_range('b', 1, 3)
        self._fc.map_range('a', 5)
        self._fc.map_range('a', 17, 123)
        self._fc.map_range('C', 5)
        mapping = self._fc['a']

        self.assertEqual(mapping.label, 'a')
        ranges = enum_mapping_to_list(mapping)
        self.assertEqual(ranges, [(0, 0), (5, 5), (17, 123)])

        with self.assertRaises(KeyError):
            self._fc['doesnotexist']

    def test_contains(self):
        self._fc.map_range('a', 0)
        self._fc.map_range('a', 2, 23)
        self._fc.map_range('b', 2)
        self._fc.map_range('c', 5)

        a_mapping = self._fc['a']
        b_mapping = self._fc['b']
        first_range = next(iter(a_mapping))

        self.assertIn(first_range, a_mapping)
        self.assertNotIn(first_range, b_mapping)

    def test_iter(self):
        self._fc.map_range('a', 1, 5)
        self._fc.map_range('b', 10, 17)
        self._fc.map_range('c', 20, 1504)

        self._fc.map_range('d', 22510, 99999)

        # This exercises iteration.
        labels = sorted(self._fc)

        self.assertEqual(labels, ['a', 'b', 'c', 'd'])

    def test_find_by_value(self):
        self._fc.map_range('a', 0)
        self._fc.map_range('b', 1, 3)
        self._fc.map_range('c', 5, 19)
        self._fc.map_range('d', 8, 15)
        self._fc.map_range('e', 10, 21)
        self._fc.map_range('f', 0)
        self._fc.map_range('g', 14)

        labels = self._fc.labels_by_value(14)

        expected_labels = ['c', 'd', 'e', 'g']

        self.assertTrue(all(label in labels for label in expected_labels))


class UnsignedEnumerationFieldClassTestCase(EnumerationFieldClassTestCase, unittest.TestCase):
    def setUp(self):
        super().setUp()
        self._create_func = self._tc.create_unsigned_enumeration_field_class
        self._fc = self._tc.create_unsigned_enumeration_field_class()

    def test_add_mapping_invalid_signedness_lower(self):
        with self.assertRaises(ValueError):
            self._fc.map_range('hello', -21, 199)

    def test_add_mapping_invalid_signedness_upper(self):
        with self.assertRaises(ValueError):
            self._fc.map_range('hello', 21, -199)


class SignedEnumerationFieldClassTestCase(EnumerationFieldClassTestCase, unittest.TestCase):
    def setUp(self):
        super().setUp()
        self._create_func = self._tc.create_signed_enumeration_field_class
        self._fc = self._tc.create_signed_enumeration_field_class()

    def test_add_mapping_simple_signed(self):
        self._fc.map_range('hello', -24)
        mapping = self._fc['hello']
        self.assertEqual(mapping.label, 'hello')

        ranges = enum_mapping_to_list(mapping)
        self.assertEqual(ranges, [(-24, -24)])

    def test_add_mapping_range_signed(self):
        self._fc.map_range('hello', -21, 199)
        mapping = self._fc['hello']
        self.assertEqual(mapping.label, 'hello')
        ranges = enum_mapping_to_list(mapping)
        self.assertEqual(ranges, [(-21, 199)])


class StringFieldClassTestCase(unittest.TestCase):
    def setUp(self):
        tc = get_default_trace_class()
        self._fc = tc.create_string_field_class()

    def test_create_default(self):
        self.assertIsNotNone(self._fc)


class _TestFieldContainer():
    def test_append_element(self):
        int_field_class = self._tc.create_signed_integer_field_class(32)
        self._append_element_method(self._fc, 'int32', int_field_class)
        field_class = self._fc['int32']
        self.assertEqual(field_class.addr, int_field_class.addr)

    def test_append_elemenbt_kwargs(self):
        int_field_class = self._tc.create_signed_integer_field_class(32)
        self._append_element_method(self._fc, name='int32', field_class=int_field_class)
        field_class = self._fc['int32']
        self.assertEqual(field_class.addr, int_field_class.addr)

    def test_append_element_invalid_name(self):
        sub_fc = self._tc.create_string_field_class()

        with self.assertRaises(TypeError):
            self._append_element_method(self._fc, 23, sub_fc)

    def test_append_element_invalid_field_class(self):
        with self.assertRaises(TypeError):
            self._append_element_method(self._fc, 'yes', object())

    def test_iadd(self):
        struct_fc = self._tc.create_structure_field_class()
        c_field_class = self._tc.create_string_field_class()
        d_field_class = self._tc.create_signed_enumeration_field_class(field_value_range=32)
        e_field_class = self._tc.create_structure_field_class()
        self._append_element_method(struct_fc, 'c_string', c_field_class)
        self._append_element_method(struct_fc, 'd_enum', d_field_class)
        self._append_element_method(struct_fc, 'e_struct', e_field_class)
        a_field_class = self._tc.create_real_field_class()
        b_field_class = self._tc.create_signed_integer_field_class(17)
        self._append_element_method(self._fc, 'a_float', a_field_class)
        self._append_element_method(self._fc, 'b_int', b_field_class)
        self._fc += struct_fc
        self.assertEqual(self._fc['a_float'].addr, a_field_class.addr)
        self.assertEqual(self._fc['b_int'].addr, b_field_class.addr)
        self.assertEqual(self._fc['c_string'].addr, c_field_class.addr)
        self.assertEqual(self._fc['d_enum'].addr, d_field_class.addr)
        self.assertEqual(self._fc['e_struct'].addr, e_field_class.addr)

    def test_bool_op(self):
        self.assertFalse(self._fc)
        self._append_element_method(self._fc, 'a', self._tc.create_string_field_class())
        self.assertTrue(self._fc)

    def test_len(self):
        fc = self._tc.create_string_field_class()
        self._append_element_method(self._fc, 'a', fc)
        self._append_element_method(self._fc, 'b', fc)
        self._append_element_method(self._fc, 'c', fc)
        self.assertEqual(len(self._fc), 3)

    def test_getitem(self):
        a_fc = self._tc.create_signed_integer_field_class(32)
        b_fc = self._tc.create_string_field_class()
        c_fc = self._tc.create_real_field_class()
        self._append_element_method(self._fc, 'a', a_fc)
        self._append_element_method(self._fc, 'b', b_fc)
        self._append_element_method(self._fc, 'c', c_fc)
        self.assertEqual(self._fc['b'].addr, b_fc.addr)

    def test_getitem_invalid_key_type(self):
        with self.assertRaises(TypeError):
            self._fc[0]

    def test_getitem_invalid_key(self):
        with self.assertRaises(KeyError):
            self._fc['no way']

    def test_contains(self):
        self.assertFalse('a' in self._fc)
        self._append_element_method(self._fc, 'a', self._tc.create_string_field_class())
        self.assertTrue('a' in self._fc)

    def test_iter(self):
        a_fc = self._tc.create_signed_integer_field_class(32)
        b_fc = self._tc.create_string_field_class()
        c_fc = self._tc.create_real_field_class()
        fields = (
            ('a', a_fc),
            ('b', b_fc),
            ('c', c_fc),
        )

        for field in fields:
            self._append_element_method(self._fc, *field)

        for (name, fc_field_class), field in zip(self._fc.items(), fields):
            self.assertEqual(name, field[0])
            self.assertEqual(fc_field_class.addr, field[1].addr)

    def test_at_index(self):
        a_fc = self._tc.create_signed_integer_field_class(32)
        b_fc = self._tc.create_string_field_class()
        c_fc = self._tc.create_real_field_class()
        self._append_element_method(self._fc, 'c', c_fc)
        self._append_element_method(self._fc, 'a', a_fc)
        self._append_element_method(self._fc, 'b', b_fc)
        self.assertEqual(self._at_index_method(self._fc, 1).addr, a_fc.addr)

    def test_at_index_invalid(self):
        self._append_element_method(self._fc, 'c', self._tc.create_signed_integer_field_class(32))

        with self.assertRaises(TypeError):
            self._at_index_method(self._fc, 'yes')

    def test_at_index_out_of_bounds_after(self):
        self._append_element_method(self._fc, 'c', self._tc.create_signed_integer_field_class(32))

        with self.assertRaises(IndexError):
            self._at_index_method(self._fc, len(self._fc))


class StructureFieldClassTestCase(_TestFieldContainer, unittest.TestCase):
    def setUp(self):
        self._append_element_method = bt2.field_class._StructureFieldClass.append_member
        self._at_index_method = bt2.field_class._StructureFieldClass.member_at_index
        self._tc = get_default_trace_class()
        self._fc = self._tc.create_structure_field_class()

    def test_create_default(self):
        self.assertIsNotNone(self._fc)


class VariantFieldClassTestCase(_TestFieldContainer, unittest.TestCase):
    def setUp(self):
        self._append_element_method = bt2.field_class._VariantFieldClass.append_option
        self._at_index_method = bt2.field_class._VariantFieldClass.option_at_index
        self._tc = get_default_trace_class()
        self._fc = self._tc.create_variant_field_class()

    def test_create_default(self):
        fc = self._tc.create_variant_field_class()

        self.assertIsNone(fc.selector_field_path)

    def _create_field_class_for_field_path_test(self):
        # Create something equivalent to:
        #
        # struct outer_struct_fc {
        #   real foo;
        #   struct inner_struct_fc {
        #     enum { first = 1, second = 2..434 } selector;
        #     string bar;
        #     string baz;
        #     variant<selector> {
        #       real a;
        #       int21_t b;
        #       uint34_t c;
        #     } variant;
        #   } inner_struct[2];
        # };
        selector_fc = self._tc.create_unsigned_enumeration_field_class(field_value_range=42)
        selector_fc.map_range('first', 1)
        selector_fc.map_range('second', 2, 434)

        fc = self._tc.create_variant_field_class(selector_fc)
        fc.append_option('a', self._tc.create_real_field_class())
        fc.append_option('b', self._tc.create_signed_integer_field_class(21))
        fc.append_option('c', self._tc.create_unsigned_integer_field_class(34))

        foo_fc = self._tc.create_real_field_class()
        bar_fc = self._tc.create_string_field_class()
        baz_fc = self._tc.create_string_field_class()

        inner_struct_fc = self._tc.create_structure_field_class()
        inner_struct_fc.append_member('selector', selector_fc)
        inner_struct_fc.append_member('bar', bar_fc)
        inner_struct_fc.append_member('baz', baz_fc)
        inner_struct_fc.append_member('variant', fc)

        inner_struct_array_fc = self._tc.create_static_array_field_class(inner_struct_fc, 2)

        outer_struct_fc = self._tc.create_structure_field_class()
        outer_struct_fc.append_member('foo', foo_fc)
        outer_struct_fc.append_member('inner_struct', inner_struct_array_fc)

        # The path to the selector field is resolved when the sequence is
        # actually used, for example in a packet context.
        self._tc.create_stream_class(packet_context_field_class=outer_struct_fc)

        return fc

    def test_selector_field_path_length(self):
        fc = self._create_field_class_for_field_path_test()
        self.assertEqual(len(fc.selector_field_path), 3)

    def test_selector_field_path_iter(self):
        fc = self._create_field_class_for_field_path_test()
        path_items = list(fc.selector_field_path)

        self.assertEqual(len(path_items), 3)

        self.assertIsInstance(path_items[0], bt2.field_path._IndexFieldPathItem)
        self.assertEqual(path_items[0].index, 1)

        self.assertIsInstance(path_items[1], bt2.field_path._CurrentArrayElementFieldPathItem)

        self.assertIsInstance(path_items[2], bt2.field_path._IndexFieldPathItem)
        self.assertEqual(path_items[2].index, 0)

    def test_selector_field_path_root_scope(self):
        fc = self._create_field_class_for_field_path_test()
        self.assertEqual(fc.selector_field_path.root_scope, bt2.field_path.Scope.PACKET_CONTEXT)


class StaticArrayFieldClassTestCase(unittest.TestCase):
    def setUp(self):
        self._tc = get_default_trace_class()
        self._elem_fc = self._tc.create_signed_integer_field_class(23)

    def test_create_default(self):
        fc = self._tc.create_static_array_field_class(self._elem_fc, 45)
        self.assertEqual(fc.element_field_class.addr, self._elem_fc.addr)
        self.assertEqual(fc.length, 45)

    def test_create_invalid_elem_field_class(self):
        with self.assertRaises(TypeError):
            self._tc.create_static_array_field_class(object(), 45)

    def test_create_invalid_length(self):
        with self.assertRaises(ValueError):
            self._tc.create_static_array_field_class(self._tc.create_string_field_class(), -17)

    def test_create_invalid_length_type(self):
        with self.assertRaises(TypeError):
            self._tc.create_static_array_field_class(self._tc.create_string_field_class(), 'the length')


class DynamicArrayFieldClassTestCase(unittest.TestCase):
    def setUp(self):
        self._tc = get_default_trace_class()
        self._elem_fc = self._tc.create_signed_integer_field_class(23)
        self._len_fc = self._tc.create_unsigned_integer_field_class(12)

    def test_create_default(self):
        fc = self._tc.create_dynamic_array_field_class(self._elem_fc)
        self.assertEqual(fc.element_field_class.addr, self._elem_fc.addr)
        self.assertIsNone(fc.length_field_path, None)

    def _create_field_class_for_field_path_test(self):
        # Create something a field class that is equivalent to:
        #
        # struct outer_struct_fc {
        #   real foo;
        #   struct inner_struct_fc {
        #     string bar;
        #     string baz;
        #     uint12_t len;
        #     uint23_t dyn_array[len];
        #   } inner_struct[2];
        # };

        fc = self._tc.create_dynamic_array_field_class(self._elem_fc, self._len_fc)

        foo_fc = self._tc.create_real_field_class()
        bar_fc = self._tc.create_string_field_class()
        baz_fc = self._tc.create_string_field_class()

        inner_struct_fc = self._tc.create_structure_field_class()
        inner_struct_fc.append_member('bar', bar_fc)
        inner_struct_fc.append_member('baz', baz_fc)
        inner_struct_fc.append_member('len', self._len_fc)
        inner_struct_fc.append_member('dyn_array', fc)

        inner_struct_array_fc = self._tc.create_static_array_field_class(inner_struct_fc, 2)

        outer_struct_fc = self._tc.create_structure_field_class()
        outer_struct_fc.append_member('foo', foo_fc)
        outer_struct_fc.append_member('inner_struct', inner_struct_array_fc)

        # The path to the length field is resolved when the sequence is
        # actually used, for example in a packet context.
        self._tc.create_stream_class(packet_context_field_class=outer_struct_fc)

        return fc

    def test_field_path_len(self):
        fc = self._create_field_class_for_field_path_test()
        self.assertEqual(len(fc.length_field_path), 3)

    def test_field_path_iter(self):
        fc = self._create_field_class_for_field_path_test()
        path_items = list(fc.length_field_path)

        self.assertEqual(len(path_items), 3)

        self.assertIsInstance(path_items[0], bt2.field_path._IndexFieldPathItem)
        self.assertEqual(path_items[0].index, 1)

        self.assertIsInstance(path_items[1], bt2.field_path._CurrentArrayElementFieldPathItem)

        self.assertIsInstance(path_items[2], bt2.field_path._IndexFieldPathItem)
        self.assertEqual(path_items[2].index, 2)

    def test_field_path_root_scope(self):
        fc = self._create_field_class_for_field_path_test()
        self.assertEqual(fc.length_field_path.root_scope, bt2.field_path.Scope.PACKET_CONTEXT)

    def test_create_invalid_field_class(self):
        with self.assertRaises(TypeError):
            self._tc.create_dynamic_array_field_class(object())

    def test_create_invalid_length_type(self):
        with self.assertRaises(TypeError):
            self._tc.create_dynamic_array_field_class(self._tc.create_string_field_class(), 17)


if __name__ == "__main__":
    unittest.main()
