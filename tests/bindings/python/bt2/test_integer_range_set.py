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

import bt2
import unittest

from utils import get_default_trace_class, create_const_field


def get_const_signed_integer_range(int_ranges):
    def range_setter(field):
        field.value = 12

    tc = get_default_trace_class()
    fc = tc.create_signed_enumeration_field_class(32)
    fc.add_mapping('something', bt2.SignedIntegerRangeSet(int_ranges))

    return create_const_field(tc, fc, range_setter).cls['something'].ranges


def get_const_unsigned_integer_range(int_ranges):
    def range_setter(field):
        field.value = 12

    tc = get_default_trace_class()
    fc = tc.create_unsigned_enumeration_field_class(32)
    fc.add_mapping('something', bt2.UnsignedIntegerRangeSet(int_ranges))

    return create_const_field(tc, fc, range_setter).cls['something'].ranges


class _IntegerRangeTestCase:
    def setUp(self):
        self._rg = self._CLS(self._def_lower, self._def_upper)
        self._const_rg = list(
            self._GET_CONST_RANGE_SET([(self._def_lower, self._def_upper)])
        )[0]

    def test_create(self):
        self.assertEqual(self._rg.lower, self._def_lower)
        self.assertEqual(self._rg.upper, self._def_upper)
        self.assertIs(type(self._rg), self._CLS)

    def test_const_create(self):
        self.assertEqual(self._const_rg.lower, self._def_lower)
        self.assertEqual(self._const_rg.upper, self._def_upper)
        self.assertIs(type(self._const_rg), self._CONST_CLS)

    def test_create_same(self):
        rg = self._CLS(self._def_lower, self._def_lower)
        self.assertEqual(rg.lower, self._def_lower)
        self.assertEqual(rg.upper, self._def_lower)

    def test_create_single(self):
        rg = self._CLS(self._def_lower)
        self.assertEqual(rg.lower, self._def_lower)
        self.assertEqual(rg.upper, self._def_lower)

    def test_create_wrong_type_lower(self):
        with self.assertRaises(TypeError):
            self._CLS(19.3, self._def_upper)

    def test_create_wrong_type_upper(self):
        with self.assertRaises(TypeError):
            self._CLS(self._def_lower, 19.3)

    def test_create_out_of_bound_lower(self):
        with self.assertRaises(ValueError):
            self._CLS(self._oob_lower, self._def_upper)

    def test_create_out_of_bound_upper(self):
        with self.assertRaises(ValueError):
            self._CLS(self._def_lower, self._oob_upper)

    def test_create_lower_gt_upper(self):
        with self.assertRaises(ValueError):
            self._CLS(self._def_lower, self._def_lower - 1)

    def test_contains_lower(self):
        self.assertTrue(self._rg.contains(self._def_lower))

    def test_contains_upper(self):
        self.assertTrue(self._rg.contains(self._def_upper))

    def test_contains_avg(self):
        avg = (self._def_lower + self._def_upper) // 2
        self.assertTrue(self._rg.contains(avg))

    def test_contains_wrong_type(self):
        with self.assertRaises(TypeError):
            self._rg.contains('allo')

    def test_contains_out_of_bound(self):
        with self.assertRaises(ValueError):
            self._rg.contains(self._oob_upper)

    def test_eq(self):
        rg = self._CLS(self._def_lower, self._def_upper)
        self.assertEqual(rg, self._rg)

    def test_const_eq(self):
        rg = list(self._GET_CONST_RANGE_SET([(self._def_lower, self._def_upper)]))[0]
        self.assertEqual(rg, self._const_rg)

    def test_const_nonconst_eq(self):
        self.assertEqual(self._rg, self._const_rg)

    def test_ne(self):
        rg = self._CLS(self._def_lower, self._def_upper - 1)
        self.assertNotEqual(rg, self._rg)

    def test_const_ne(self):
        rg = list(self._GET_CONST_RANGE_SET([(self._def_lower, self._def_upper - 1)]))[
            0
        ]
        self.assertNotEqual(rg, self._const_rg)

    def test_ne_other_type(self):
        self.assertNotEqual(self._rg, 48)


class UnsignedIntegerRangeTestCase(_IntegerRangeTestCase, unittest.TestCase):
    _CLS = bt2.UnsignedIntegerRange
    _CONST_CLS = bt2._UnsignedIntegerRangeConst
    _GET_CONST_RANGE_SET = staticmethod(get_const_unsigned_integer_range)
    _def_lower = 23
    _def_upper = 18293
    _oob_lower = -1
    _oob_upper = 1 << 64


class SignedIntegerRangeTestCase(_IntegerRangeTestCase, unittest.TestCase):
    _CLS = bt2.SignedIntegerRange
    _CONST_CLS = bt2._SignedIntegerRangeConst
    _GET_CONST_RANGE_SET = staticmethod(get_const_signed_integer_range)
    _def_lower = -184
    _def_upper = 11547
    _oob_lower = -(1 << 63) - 1
    _oob_upper = 1 << 63


class _IntegerRangeSetTestCase:
    def setUp(self):
        self._rs = self._CLS((self._range1, self._range2, self._range3))
        self._const_rs = self._GET_CONST_RANGE_SET(
            [self._range1, self._range2, self._range3]
        )

    def test_create(self):
        self.assertEqual(len(self._rs), 3)
        self.assertIn(self._range1, self._rs)
        self.assertIn(self._range2, self._rs)
        self.assertIn(self._range3, self._rs)
        self.assertIs(type(self._range1), self._RANGE_CLS)

    def test_const_create(self):
        self.assertEqual(len(self._const_rs), 3)
        self.assertIn(self._range1, self._const_rs)
        self.assertIn(self._range2, self._const_rs)
        self.assertIn(self._range3, self._const_rs)
        self.assertIs(type(self._range1), self._RANGE_CLS)

    def test_create_tuples(self):
        rs = self._CLS(
            (
                (self._range1.lower, self._range1.upper),
                (self._range2.lower, self._range2.upper),
                (self._range3.lower, self._range3.upper),
            )
        )
        self.assertEqual(len(rs), 3)
        self.assertIn(self._range1, rs)
        self.assertIn(self._range2, rs)
        self.assertIn(self._range3, rs)

    def test_create_single(self):
        rs = self._CLS((self._range_same.lower,))
        self.assertEqual(len(rs), 1)
        self.assertIn(self._range_same, rs)

    def test_create_non_iter(self):
        with self.assertRaises(TypeError):
            self._rs = self._CLS(23)

    def test_create_wrong_elem_type(self):
        with self.assertRaises(TypeError):
            self._rs = self._CLS((self._range1, self._range2, 'lel'))

    def test_len(self):
        self.assertEqual(len(self._rs), 3)

    def test_contains(self):
        rs = self._CLS((self._range1, self._range2))
        self.assertIn(self._range1, rs)
        self.assertIn(self._range2, rs)
        self.assertNotIn(self._range3, rs)

    def test_contains_value(self):
        rs = self._CLS((self._range1, self._range2))
        self.assertTrue(rs.contains_value(self._range1.upper))
        self.assertTrue(rs.contains_value(self._range2.lower))
        self.assertFalse(rs.contains_value(self._range3.upper))

    def test_contains_value_wrong_type(self):
        with self.assertRaises(TypeError):
            self._rs.contains_value('meow')

    def test_iter(self):
        range_list = list(self._rs)
        self.assertIn(self._range1, range_list)
        self.assertIn(self._range2, range_list)
        self.assertIn(self._range3, range_list)

        for rg in range_list:
            self.assertIs(type(rg), self._RANGE_CLS)

    def test_const_iter(self):
        range_list = list(self._const_rs)
        self.assertIn(self._range1, range_list)
        self.assertIn(self._range2, range_list)
        self.assertIn(self._range3, range_list)

        for rg in range_list:
            self.assertIs(type(rg), self._CONST_RANGE_CLS)

    def test_empty(self):
        rs = self._CLS()
        self.assertEqual(len(rs), 0)
        self.assertEqual(len(list(rs)), 0)

    def test_add_range_obj(self):
        rs = self._CLS((self._range1,))
        self.assertEqual(len(rs), 1)
        rs.add(self._range2)
        self.assertEqual(len(rs), 2)
        self.assertIn(self._range2, rs)

    def test_const_add_range_obj(self):
        with self.assertRaises(AttributeError):
            self._const_rs.add((12, 4434))

    def test_discard_not_implemented(self):
        with self.assertRaises(NotImplementedError):
            self._rs.discard(self._range2)

    def test_eq_same_order(self):
        rs = self._CLS((self._range1, self._range2, self._range3))
        self.assertEqual(self._rs, rs)

    def test_eq_diff_order(self):
        rs = self._CLS((self._range1, self._range3, self._range2))
        self.assertEqual(self._rs, rs)

    def test_eq_same_addr(self):
        self.assertEqual(self._rs, self._rs)

    def test_ne_diff_len(self):
        rs = self._CLS((self._range1, self._range2))
        self.assertNotEqual(self._rs, rs)

    def test_ne_diff_values(self):
        rs1 = self._CLS((self._range1, self._range2))
        rs2 = self._CLS((self._range1, self._range3))
        self.assertNotEqual(rs1, rs2)

    def test_ne_incompatible_type(self):
        self.assertNotEqual(self._rs, object())


class UnsignedIntegerRangeSetTestCase(_IntegerRangeSetTestCase, unittest.TestCase):
    _CLS = bt2.UnsignedIntegerRangeSet
    _CONST_CLS = bt2._UnsignedIntegerRangeSetConst
    _RANGE_CLS = bt2.UnsignedIntegerRange
    _CONST_RANGE_CLS = bt2._UnsignedIntegerRangeConst
    _GET_CONST_RANGE_SET = staticmethod(get_const_unsigned_integer_range)

    def setUp(self):
        self._range1 = bt2.UnsignedIntegerRange(4, 192)
        self._range2 = bt2.UnsignedIntegerRange(17, 228)
        self._range3 = bt2.UnsignedIntegerRange(1000, 2000)
        self._range_same = bt2.UnsignedIntegerRange(1300, 1300)
        super().setUp()


class SignedIntegerRangeSetTestCase(_IntegerRangeSetTestCase, unittest.TestCase):
    _CLS = bt2.SignedIntegerRangeSet
    _CONST_CLS = bt2._SignedIntegerRangeSetConst
    _RANGE_CLS = bt2.SignedIntegerRange
    _CONST_RANGE_CLS = bt2._SignedIntegerRangeConst
    _GET_CONST_RANGE_SET = staticmethod(get_const_signed_integer_range)

    def setUp(self):
        self._range1 = bt2.SignedIntegerRange(-1484, -17)
        self._range2 = bt2.SignedIntegerRange(-101, 1500)
        self._range3 = bt2.SignedIntegerRange(1948, 2019)
        self._range_same = bt2.SignedIntegerRange(-1300, -1300)
        super().setUp()


if __name__ == '__main__':
    unittest.main()
