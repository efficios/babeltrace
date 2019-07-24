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
import collections
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
        fc = self._create_func(
            preferred_display_base=bt2.IntegerDisplayBase.HEXADECIMAL
        )
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


def enum_mapping_to_set(mapping):
    return {(x.lower, x.upper) for x in mapping.ranges}


class _EnumerationFieldClassTestCase(_TestIntegerFieldClassProps):
    def setUp(self):
        self._tc = get_default_trace_class()
        self._spec_set_up()
        self._fc = self._create_func()

    def test_create_from_invalid_type(self):
        with self.assertRaises(TypeError):
            self._create_func('coucou')

    def test_add_mapping_simple(self):
        self._fc.add_mapping('hello', self._ranges1)
        mapping = self._fc['hello']
        self.assertEqual(mapping.label, 'hello')
        self.assertEqual(mapping.ranges, self._ranges1)

    def test_add_mapping_simple_kwargs(self):
        self._fc.add_mapping(label='hello', ranges=self._ranges1)
        mapping = self._fc['hello']
        self.assertEqual(mapping.label, 'hello')
        self.assertEqual(mapping.ranges, self._ranges1)

    def test_add_mapping_invalid_name(self):
        with self.assertRaises(TypeError):
            self._fc.add_mapping(17, self._ranges1)

    def test_add_mapping_invalid_range(self):
        with self.assertRaises(TypeError):
            self._fc.add_mapping('allo', 'meow')

    def test_add_mapping_dup_label(self):
        with self.assertRaises(ValueError):
            self._fc.add_mapping('a', self._ranges1)
            self._fc.add_mapping('a', self._ranges2)

    def test_add_mapping_invalid_ranges_signedness(self):
        with self.assertRaises(TypeError):
            self._fc.add_mapping('allo', self._inval_ranges)

    def test_iadd(self):
        self._fc.add_mapping('c', self._ranges1)

        self._fc += [('d', self._ranges2), ('e', self._ranges3)]

        self.assertEqual(len(self._fc), 3)
        self.assertEqual(self._fc['c'].label, 'c')
        self.assertEqual(self._fc['c'].ranges, self._ranges1)
        self.assertEqual(self._fc['d'].label, 'd')
        self.assertEqual(self._fc['d'].ranges, self._ranges2)
        self.assertEqual(self._fc['e'].label, 'e')
        self.assertEqual(self._fc['e'].ranges, self._ranges3)

    def test_bool_op(self):
        self.assertFalse(self._fc)
        self._fc.add_mapping('a', self._ranges1)
        self.assertTrue(self._fc)

    def test_len(self):
        self._fc.add_mapping('a', self._ranges1)
        self._fc.add_mapping('b', self._ranges2)
        self._fc.add_mapping('c', self._ranges3)
        self.assertEqual(len(self._fc), 3)

    def test_getitem(self):
        self._fc.add_mapping('a', self._ranges1)
        self._fc.add_mapping('b', self._ranges2)
        self._fc.add_mapping('c', self._ranges3)
        mapping = self._fc['a']
        self.assertEqual(mapping.label, 'a')
        self.assertEqual(mapping.ranges, self._ranges1)

    def test_getitem_nonexistent(self):
        with self.assertRaises(KeyError):
            self._fc['doesnotexist']

    def test_iter(self):
        self._fc.add_mapping('a', self._ranges1)
        self._fc.add_mapping('b', self._ranges2)
        self._fc.add_mapping('c', self._ranges3)

        # This exercises iteration.
        labels = sorted(self._fc)

        self.assertEqual(labels, ['a', 'b', 'c'])

    def test_find_by_value(self):
        self._fc.add_mapping('a', self._ranges1)
        self._fc.add_mapping('b', self._ranges2)
        self._fc.add_mapping('c', self._ranges3)
        mappings = self._fc.mappings_for_value(self._value_in_range_1_and_3)
        labels = set([mapping.label for mapping in mappings])
        expected_labels = set(['a', 'c'])
        self.assertEqual(labels, expected_labels)


class UnsignedEnumerationFieldClassTestCase(
    _EnumerationFieldClassTestCase, unittest.TestCase
):
    def _spec_set_up(self):
        self._ranges1 = bt2.UnsignedIntegerRangeSet([(1, 4), (18, 47)])
        self._ranges2 = bt2.UnsignedIntegerRangeSet([(5, 5)])
        self._ranges3 = bt2.UnsignedIntegerRangeSet([(8, 22), (48, 99)])
        self._inval_ranges = bt2.SignedIntegerRangeSet([(-8, -5), (48, 1928)])
        self._value_in_range_1_and_3 = 20
        self._create_func = self._tc.create_unsigned_enumeration_field_class


class SignedEnumerationFieldClassTestCase(
    _EnumerationFieldClassTestCase, unittest.TestCase
):
    def _spec_set_up(self):
        self._ranges1 = bt2.SignedIntegerRangeSet([(-10, -4), (18, 47)])
        self._ranges2 = bt2.SignedIntegerRangeSet([(-3, -3)])
        self._ranges3 = bt2.SignedIntegerRangeSet([(-100, -1), (8, 16), (48, 99)])
        self._inval_ranges = bt2.UnsignedIntegerRangeSet([(8, 16), (48, 99)])
        self._value_in_range_1_and_3 = -7
        self._create_func = self._tc.create_signed_enumeration_field_class


class StringFieldClassTestCase(unittest.TestCase):
    def setUp(self):
        tc = get_default_trace_class()
        self._fc = tc.create_string_field_class()

    def test_create_default(self):
        self.assertIsNotNone(self._fc)


class _TestElementContainer:
    def setUp(self):
        self._tc = get_default_trace_class()
        self._fc = self._create_default_fc()

    def test_create_default(self):
        self.assertIsNotNone(self._fc)

    def test_append_element(self):
        int_field_class = self._tc.create_signed_integer_field_class(32)
        self._append_element_method(self._fc, 'int32', int_field_class)
        field_class = self._fc['int32'].field_class
        self.assertEqual(field_class.addr, int_field_class.addr)

    def test_append_element_kwargs(self):
        int_field_class = self._tc.create_signed_integer_field_class(32)
        self._append_element_method(self._fc, name='int32', field_class=int_field_class)
        field_class = self._fc['int32'].field_class
        self.assertEqual(field_class.addr, int_field_class.addr)

    def test_append_element_invalid_name(self):
        sub_fc = self._tc.create_string_field_class()

        with self.assertRaises(TypeError):
            self._append_element_method(self._fc, 23, sub_fc)

    def test_append_element_invalid_field_class(self):
        with self.assertRaises(TypeError):
            self._append_element_method(self._fc, 'yes', object())

    def test_append_element_dup_name(self):
        sub_fc1 = self._tc.create_string_field_class()
        sub_fc2 = self._tc.create_string_field_class()

        with self.assertRaises(ValueError):
            self._append_element_method(self._fc, 'yes', sub_fc1)
            self._append_element_method(self._fc, 'yes', sub_fc2)

    def test_iadd(self):
        other_fc = self._create_default_fc()
        a_field_class = self._tc.create_real_field_class()
        b_field_class = self._tc.create_signed_integer_field_class(17)
        self._append_element_method(self._fc, 'a_float', a_field_class)
        self._append_element_method(self._fc, 'b_int', b_field_class)
        c_field_class = self._tc.create_string_field_class()
        d_field_class = self._tc.create_signed_enumeration_field_class(
            field_value_range=32
        )
        e_field_class = self._tc.create_structure_field_class()
        self._fc += [
            ('c_string', c_field_class),
            ('d_enum', d_field_class),
            ('e_struct', e_field_class),
        ]
        self.assertEqual(self._fc['a_float'].field_class.addr, a_field_class.addr)
        self.assertEqual(self._fc['a_float'].name, 'a_float')
        self.assertEqual(self._fc['b_int'].field_class.addr, b_field_class.addr)
        self.assertEqual(self._fc['b_int'].name, 'b_int')
        self.assertEqual(self._fc['c_string'].field_class.addr, c_field_class.addr)
        self.assertEqual(self._fc['c_string'].name, 'c_string')
        self.assertEqual(self._fc['d_enum'].field_class.addr, d_field_class.addr)
        self.assertEqual(self._fc['d_enum'].name, 'd_enum')
        self.assertEqual(self._fc['e_struct'].field_class.addr, e_field_class.addr)
        self.assertEqual(self._fc['e_struct'].name, 'e_struct')

    def test_bool_op(self):
        self.assertFalse(self._fc)
        self._append_element_method(self._fc, 'a', self._tc.create_string_field_class())
        self.assertTrue(self._fc)

    def test_len(self):
        self._append_element_method(self._fc, 'a', self._tc.create_string_field_class())
        self._append_element_method(self._fc, 'b', self._tc.create_string_field_class())
        self._append_element_method(self._fc, 'c', self._tc.create_string_field_class())
        self.assertEqual(len(self._fc), 3)

    def test_getitem(self):
        a_fc = self._tc.create_signed_integer_field_class(32)
        b_fc = self._tc.create_string_field_class()
        c_fc = self._tc.create_real_field_class()
        self._append_element_method(self._fc, 'a', a_fc)
        self._append_element_method(self._fc, 'b', b_fc)
        self._append_element_method(self._fc, 'c', c_fc)
        self.assertEqual(self._fc['b'].field_class.addr, b_fc.addr)
        self.assertEqual(self._fc['b'].name, 'b')

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
        elements = (('a', a_fc), ('b', b_fc), ('c', c_fc))

        for elem in elements:
            self._append_element_method(self._fc, *elem)

        for (name, element), test_elem in zip(self._fc.items(), elements):
            self.assertEqual(element.name, test_elem[0])
            self.assertEqual(name, element.name)
            self.assertEqual(element.field_class.addr, test_elem[1].addr)

    def test_at_index(self):
        a_fc = self._tc.create_signed_integer_field_class(32)
        b_fc = self._tc.create_string_field_class()
        c_fc = self._tc.create_real_field_class()
        self._append_element_method(self._fc, 'c', c_fc)
        self._append_element_method(self._fc, 'a', a_fc)
        self._append_element_method(self._fc, 'b', b_fc)
        elem = self._at_index_method(self._fc, 1)
        self.assertEqual(elem.field_class.addr, a_fc.addr)
        self.assertEqual(elem.name, 'a')

    def test_at_index_invalid(self):
        self._append_element_method(
            self._fc, 'c', self._tc.create_signed_integer_field_class(32)
        )

        with self.assertRaises(TypeError):
            self._at_index_method(self._fc, 'yes')

    def test_at_index_out_of_bounds_after(self):
        self._append_element_method(
            self._fc, 'c', self._tc.create_signed_integer_field_class(32)
        )

        with self.assertRaises(IndexError):
            self._at_index_method(self._fc, len(self._fc))


class StructureFieldClassTestCase(_TestElementContainer, unittest.TestCase):
    _append_element_method = staticmethod(bt2._StructureFieldClass.append_member)
    _at_index_method = staticmethod(bt2._StructureFieldClass.member_at_index)

    def _create_default_fc(self):
        return self._tc.create_structure_field_class()


class VariantFieldClassWithoutSelectorTestCase(
    _TestElementContainer, unittest.TestCase
):
    _append_element_method = staticmethod(
        bt2._VariantFieldClassWithoutSelector.append_option
    )
    _at_index_method = staticmethod(
        bt2._VariantFieldClassWithoutSelector.option_at_index
    )

    def _create_default_fc(self):
        return self._tc.create_variant_field_class()


class _VariantFieldClassWithSelectorTestCase:
    def setUp(self):
        self._tc = get_default_trace_class()
        self._spec_set_up()
        self._fc = self._create_default_fc()

    def _create_default_fc(self):
        return self._tc.create_variant_field_class(self._selector_fc)

    def test_create_default(self):
        self.assertIsNotNone(self._fc)

    def test_append_element(self):
        str_field_class = self._tc.create_string_field_class()
        self._fc.append_option('str', str_field_class, self._ranges1)
        opt = self._fc['str']
        self.assertEqual(opt.field_class.addr, str_field_class.addr)
        self.assertEqual(opt.name, 'str')
        self.assertEqual(opt.ranges.addr, self._ranges1.addr)

    def test_append_element_kwargs(self):
        int_field_class = self._tc.create_signed_integer_field_class(32)
        self._fc.append_option(
            name='int32', field_class=int_field_class, ranges=self._ranges1
        )
        opt = self._fc['int32']
        self.assertEqual(opt.field_class.addr, int_field_class.addr)
        self.assertEqual(opt.name, 'int32')
        self.assertEqual(opt.ranges.addr, self._ranges1.addr)

    def test_append_element_invalid_name(self):
        sub_fc = self._tc.create_string_field_class()

        with self.assertRaises(TypeError):
            self._fc.append_option(self._fc, 23, sub_fc)

    def test_append_element_invalid_field_class(self):
        with self.assertRaises(TypeError):
            self._fc.append_option(self._fc, 'yes', object())

    def test_append_element_invalid_ranges(self):
        sub_fc = self._tc.create_string_field_class()

        with self.assertRaises(TypeError):
            self._fc.append_option(self._fc, sub_fc, 'lel')

    def test_append_element_dup_name(self):
        sub_fc1 = self._tc.create_string_field_class()
        sub_fc2 = self._tc.create_string_field_class()

        with self.assertRaises(ValueError):
            self._fc.append_option('yes', sub_fc1, self._ranges1)
            self._fc.append_option('yes', sub_fc2, self._ranges2)

    def test_append_element_invalid_ranges_signedness(self):
        sub_fc = self._tc.create_string_field_class()

        with self.assertRaises(TypeError):
            self._fc.append_option(self._fc, sub_fc, self._inval_ranges)

    def test_iadd(self):
        other_fc = self._create_default_fc()
        a_field_class = self._tc.create_real_field_class()
        self._fc.append_option('a_float', a_field_class, self._ranges1)
        c_field_class = self._tc.create_string_field_class()
        d_field_class = self._tc.create_signed_enumeration_field_class(
            field_value_range=32
        )
        self._fc += [
            ('c_string', c_field_class, self._ranges2),
            ('d_enum', d_field_class, self._ranges3),
        ]
        self.assertEqual(self._fc['a_float'].field_class.addr, a_field_class.addr)
        self.assertEqual(self._fc['a_float'].name, 'a_float')
        self.assertEqual(self._fc['a_float'].ranges, self._ranges1)
        self.assertEqual(self._fc['c_string'].field_class.addr, c_field_class.addr)
        self.assertEqual(self._fc['c_string'].name, 'c_string')
        self.assertEqual(self._fc['c_string'].ranges, self._ranges2)
        self.assertEqual(self._fc['d_enum'].field_class.addr, d_field_class.addr)
        self.assertEqual(self._fc['d_enum'].name, 'd_enum')
        self.assertEqual(self._fc['d_enum'].ranges, self._ranges3)

    def test_bool_op(self):
        self.assertFalse(self._fc)
        self._fc.append_option('a', self._tc.create_string_field_class(), self._ranges1)
        self.assertTrue(self._fc)

    def test_len(self):
        self._fc.append_option('a', self._tc.create_string_field_class(), self._ranges1)
        self._fc.append_option('b', self._tc.create_string_field_class(), self._ranges2)
        self._fc.append_option('c', self._tc.create_string_field_class(), self._ranges3)
        self.assertEqual(len(self._fc), 3)

    def test_getitem(self):
        a_fc = self._tc.create_signed_integer_field_class(32)
        b_fc = self._tc.create_string_field_class()
        c_fc = self._tc.create_real_field_class()
        self._fc.append_option('a', a_fc, self._ranges1)
        self._fc.append_option('b', b_fc, self._ranges2)
        self._fc.append_option('c', c_fc, self._ranges3)
        self.assertEqual(self._fc['b'].field_class.addr, b_fc.addr)
        self.assertEqual(self._fc['b'].name, 'b')
        self.assertEqual(self._fc['b'].ranges.addr, self._ranges2.addr)

    def test_getitem_invalid_key_type(self):
        with self.assertRaises(TypeError):
            self._fc[0]

    def test_getitem_invalid_key(self):
        with self.assertRaises(KeyError):
            self._fc['no way']

    def test_contains(self):
        self.assertFalse('a' in self._fc)
        self._fc.append_option('a', self._tc.create_string_field_class(), self._ranges1)
        self.assertTrue('a' in self._fc)

    def test_iter(self):
        a_fc = self._tc.create_signed_integer_field_class(32)
        b_fc = self._tc.create_string_field_class()
        c_fc = self._tc.create_real_field_class()
        opts = (
            ('a', a_fc, self._ranges1),
            ('b', b_fc, self._ranges2),
            ('c', c_fc, self._ranges3),
        )

        for opt in opts:
            self._fc.append_option(*opt)

        for (name, opt), test_opt in zip(self._fc.items(), opts):
            self.assertEqual(opt.name, test_opt[0])
            self.assertEqual(name, opt.name)
            self.assertEqual(opt.field_class.addr, test_opt[1].addr)
            self.assertEqual(opt.ranges.addr, test_opt[2].addr)

    def test_at_index(self):
        a_fc = self._tc.create_signed_integer_field_class(32)
        b_fc = self._tc.create_string_field_class()
        c_fc = self._tc.create_real_field_class()
        self._fc.append_option('c', c_fc, self._ranges1)
        self._fc.append_option('a', a_fc, self._ranges2)
        self._fc.append_option('b', b_fc, self._ranges3)
        self.assertEqual(self._fc.option_at_index(1).field_class.addr, a_fc.addr)
        self.assertEqual(self._fc.option_at_index(1).name, 'a')
        self.assertEqual(self._fc.option_at_index(1).ranges.addr, self._ranges2.addr)

    def test_at_index_invalid(self):
        self._fc.append_option(
            'c', self._tc.create_signed_integer_field_class(32), self._ranges3
        )

        with self.assertRaises(TypeError):
            self._fc.option_at_index('yes')

    def test_at_index_out_of_bounds_after(self):
        self._fc.append_option(
            'c', self._tc.create_signed_integer_field_class(32), self._ranges3
        )

        with self.assertRaises(IndexError):
            self._fc.option_at_index(len(self._fc))

    def _fill_default_fc_for_field_path_test(self):
        # Create something equivalent to:
        #
        # struct outer_struct_fc {
        #   real foo;
        #   struct inner_struct_fc {
        #     [u]int64_t selector;
        #     string bar;
        #     string baz;
        #     variant <selector> {
        #       real a;     // selected with self._ranges1
        #       int21_t b;  // selected with self._ranges2
        #       uint34_t c; // selected with self._ranges3
        #     } variant;
        #   } inner_struct[2];
        # };
        self._fc.append_option('a', self._tc.create_real_field_class(), self._ranges1)
        self._fc.append_option(
            'b', self._tc.create_signed_integer_field_class(21), self._ranges2
        )
        self._fc.append_option(
            'c', self._tc.create_unsigned_integer_field_class(34), self._ranges3
        )

        foo_fc = self._tc.create_real_field_class()
        bar_fc = self._tc.create_string_field_class()
        baz_fc = self._tc.create_string_field_class()

        inner_struct_fc = self._tc.create_structure_field_class()
        inner_struct_fc.append_member('selector', self._selector_fc)
        inner_struct_fc.append_member('bar', bar_fc)
        inner_struct_fc.append_member('baz', baz_fc)
        inner_struct_fc.append_member('variant', self._fc)

        inner_struct_array_fc = self._tc.create_static_array_field_class(
            inner_struct_fc, 2
        )

        outer_struct_fc = self._tc.create_structure_field_class()
        outer_struct_fc.append_member('foo', foo_fc)
        outer_struct_fc.append_member('inner_struct', inner_struct_array_fc)

        # The path to the selector field is resolved when the sequence is
        # actually used, for example in a packet context.
        self._tc.create_stream_class(
            supports_packets=True, packet_context_field_class=outer_struct_fc
        )

    def test_selector_field_path_length(self):
        self._fill_default_fc_for_field_path_test()
        self.assertEqual(len(self._fc.selector_field_path), 3)

    def test_selector_field_path_iter(self):
        self._fill_default_fc_for_field_path_test()
        path_items = list(self._fc.selector_field_path)

        self.assertEqual(len(path_items), 3)

        self.assertIsInstance(path_items[0], bt2._IndexFieldPathItem)
        self.assertEqual(path_items[0].index, 1)

        self.assertIsInstance(path_items[1], bt2._CurrentArrayElementFieldPathItem)

        self.assertIsInstance(path_items[2], bt2._IndexFieldPathItem)
        self.assertEqual(path_items[2].index, 0)

    def test_selector_field_path_root_scope(self):
        self._fill_default_fc_for_field_path_test()
        self.assertEqual(
            self._fc.selector_field_path.root_scope, bt2.FieldPathScope.PACKET_CONTEXT
        )


class VariantFieldClassWithUnsignedSelectorTestCase(
    _VariantFieldClassWithSelectorTestCase, unittest.TestCase
):
    def _spec_set_up(self):
        self._ranges1 = bt2.UnsignedIntegerRangeSet([(1, 4), (18, 47)])
        self._ranges2 = bt2.UnsignedIntegerRangeSet([(5, 5)])
        self._ranges3 = bt2.UnsignedIntegerRangeSet([(8, 16), (48, 99)])
        self._inval_ranges = bt2.SignedIntegerRangeSet([(-8, 16), (48, 99)])
        self._selector_fc = self._tc.create_unsigned_integer_field_class()


class VariantFieldClassWithSignedSelectorTestCase(
    _VariantFieldClassWithSelectorTestCase, unittest.TestCase
):
    def _spec_set_up(self):
        self._ranges1 = bt2.SignedIntegerRangeSet([(-10, -4), (18, 47)])
        self._ranges2 = bt2.SignedIntegerRangeSet([(-3, -3)])
        self._ranges3 = bt2.SignedIntegerRangeSet([(8, 16), (48, 99)])
        self._inval_ranges = bt2.UnsignedIntegerRangeSet([(8, 16), (48, 99)])
        self._selector_fc = self._tc.create_signed_integer_field_class()


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
            self._tc.create_static_array_field_class(
                self._tc.create_string_field_class(), -17
            )

    def test_create_invalid_length_type(self):
        with self.assertRaises(TypeError):
            self._tc.create_static_array_field_class(
                self._tc.create_string_field_class(), 'the length'
            )


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

        inner_struct_array_fc = self._tc.create_static_array_field_class(
            inner_struct_fc, 2
        )

        outer_struct_fc = self._tc.create_structure_field_class()
        outer_struct_fc.append_member('foo', foo_fc)
        outer_struct_fc.append_member('inner_struct', inner_struct_array_fc)

        # The path to the length field is resolved when the sequence is
        # actually used, for example in a packet context.
        self._tc.create_stream_class(
            packet_context_field_class=outer_struct_fc, supports_packets=True
        )

        return fc

    def test_field_path_len(self):
        fc = self._create_field_class_for_field_path_test()
        self.assertEqual(len(fc.length_field_path), 3)

    def test_field_path_iter(self):
        fc = self._create_field_class_for_field_path_test()
        path_items = list(fc.length_field_path)

        self.assertEqual(len(path_items), 3)

        self.assertIsInstance(path_items[0], bt2._IndexFieldPathItem)
        self.assertEqual(path_items[0].index, 1)

        self.assertIsInstance(path_items[1], bt2._CurrentArrayElementFieldPathItem)

        self.assertIsInstance(path_items[2], bt2._IndexFieldPathItem)
        self.assertEqual(path_items[2].index, 2)

    def test_field_path_root_scope(self):
        fc = self._create_field_class_for_field_path_test()
        self.assertEqual(
            fc.length_field_path.root_scope, bt2.FieldPathScope.PACKET_CONTEXT
        )

    def test_create_invalid_field_class(self):
        with self.assertRaises(TypeError):
            self._tc.create_dynamic_array_field_class(object())

    def test_create_invalid_length_type(self):
        with self.assertRaises(TypeError):
            self._tc.create_dynamic_array_field_class(
                self._tc.create_string_field_class(), 17
            )


if __name__ == "__main__":
    unittest.main()
