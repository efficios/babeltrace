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

from functools import partial, partialmethod
import operator
import collections
import unittest
import math
import copy
import bt2


# The value object classes explicitly do not implement the copy methods,
# raising `NotImplementedError`, just in case we decide to implement
# them someday.
class _TestCopySimple:
    def test_copy(self):
        with self.assertRaises(NotImplementedError):
            copy.copy(self._def)

    def test_deepcopy(self):
        with self.assertRaises(NotImplementedError):
            copy.deepcopy(self._def)


_COMP_BINOPS = (operator.eq, operator.ne)


# Base class for numeric value test cases.
#
# To be compatible with this base class, a derived class must, in its
# setUp() method:
#
# * Set `self._def` to a value object with an arbitrary raw value.
# * Set `self._def_value` to the equivalent raw value of `self._def`.
class _TestNumericValue(_TestCopySimple):
    # Tries the binary operation `op`:
    #
    # 1. Between `self._def`, which is a value object, and `rhs`.
    # 2. Between `self._def_value`, which is the raw value of
    #    `self._def`, and `rhs`.
    #
    # Returns the results of 1. and 2.
    #
    # If there's an exception while performing 1. or 2., asserts that
    # both operations raised exceptions, that both exceptions have the
    # same type, and returns `None` for both results.
    def _binop(self, op, rhs):
        type_rexc = None
        type_rvexc = None
        comp_value = rhs

        # try with value object
        try:
            r = op(self._def, rhs)
        except Exception as e:
            type_rexc = type(e)

        # try with raw value
        try:
            rv = op(self._def_value, comp_value)
        except Exception as e:
            type_rvexc = type(e)

        if type_rexc is not None or type_rvexc is not None:
            # at least one of the operations raised an exception: in
            # this case both operations should have raised the same
            # type of exception (division by zero, bit shift with a
            # floating point number operand, etc.)
            self.assertIs(type_rexc, type_rvexc)
            return None, None

        return r, rv

    # Tries the unary operation `op`:
    #
    # 1. On `self._def`, which is a value object.
    # 2. On `self._def_value`, which is the raw value of `self._def`.
    #
    # Returns the results of 1. and 2.
    #
    # If there's an exception while performing 1. or 2., asserts that
    # both operations raised exceptions, that both exceptions have the
    # same type, and returns `None` for both results.
    def _unaryop(self, op):
        type_rexc = None
        type_rvexc = None

        # try with value object
        try:
            r = op(self._def)
        except Exception as e:
            type_rexc = type(e)

        # try with raw value
        try:
            rv = op(self._def_value)
        except Exception as e:
            type_rvexc = type(e)

        if type_rexc is not None or type_rvexc is not None:
            # at least one of the operations raised an exception: in
            # this case both operations should have raised the same
            # type of exception (division by zero, bit shift with a
            # floating point number operand, etc.)
            self.assertIs(type_rexc, type_rvexc)
            return None, None

        return r, rv

    # Tests that the unary operation `op` gives results with the same
    # type for both `self._def` and `self._def_value`.
    def _test_unaryop_type(self, op):
        r, rv = self._unaryop(op)

        if r is None:
            return

        self.assertIsInstance(r, type(rv))

    # Tests that the unary operation `op` gives results with the same
    # value for both `self._def` and `self._def_value`. This uses the
    # __eq__() operator of `self._def`.
    def _test_unaryop_value(self, op):
        r, rv = self._unaryop(op)

        if r is None:
            return

        self.assertEqual(r, rv)

    # Tests that the unary operation `op`, when applied to `self._def`,
    # does not change its underlying BT object address.
    def _test_unaryop_addr_same(self, op):
        addr_before = self._def.addr
        self._unaryop(op)
        self.assertEqual(self._def.addr, addr_before)

    # Tests that the unary operation `op`, when applied to `self._def`,
    # does not change its value.
    def _test_unaryop_value_same(self, op):
        value_before = self._def.__class__(self._def)
        self._unaryop(op)
        self.assertEqual(self._def, value_before)

    # Tests that the binary operation `op` gives results with the same
    # type for both `self._def` and `self._def_value`.
    def _test_binop_type(self, op, rhs):
        r, rv = self._binop(op, rhs)

        if r is None:
            return

        if op in _COMP_BINOPS:
            # __eq__() and __ne__() always return a 'bool' object
            self.assertIsInstance(r, bool)
        else:
            self.assertIsInstance(r, type(rv))

    # Tests that the binary operation `op` gives results with the same
    # value for both `self._def` and `self._def_value`. This uses the
    # __eq__() operator of `self._def`.
    def _test_binop_value(self, op, rhs):
        r, rv = self._binop(op, rhs)

        if r is None:
            return

        self.assertEqual(r, rv)

    # Tests that the binary operation `op`, when applied to `self._def`,
    # does not change its underlying BT object address.
    def _test_binop_lhs_addr_same(self, op, rhs):
        addr_before = self._def.addr
        r, rv = self._binop(op, rhs)
        self.assertEqual(self._def.addr, addr_before)

    # Tests that the binary operation `op`, when applied to `self._def`,
    # does not change its value.
    def _test_binop_lhs_value_same(self, op, rhs):
        value_before = self._def.__class__(self._def)
        r, rv = self._binop(op, rhs)
        self.assertEqual(self._def, value_before)

    # The methods below which take the `test_cb` and `op` parameters
    # are meant to be used with one of the _test_binop_*() functions
    # above as `test_cb` and a binary operator function as `op`.
    #
    # For example:
    #
    #     self._test_binop_rhs_pos_int(self._test_binop_value,
    #                                  operator.add)
    #
    # This tests that a numeric value object added to a positive integer
    # raw value gives a result with the expected value.
    #
    # `vint` and `vfloat` mean a signed integer value object and a real
    # value object.

    def _test_binop_unknown(self, op):
        if op is operator.eq:
            self.assertIs(op(self._def, object()), False)
        elif op is operator.ne:
            self.assertIs(op(self._def, object()), True)
        else:
            with self.assertRaises(TypeError):
                op(self._def, object())

    def _test_binop_none(self, op):
        if op is operator.eq:
            self.assertIs(op(self._def, None), False)
        elif op is operator.ne:
            self.assertIs(op(self._def, None), True)
        else:
            with self.assertRaises(TypeError):
                op(self._def, None)

    def _test_binop_rhs_false(self, test_cb, op):
        test_cb(op, False)

    def _test_binop_rhs_true(self, test_cb, op):
        test_cb(op, True)

    def _test_binop_rhs_pos_int(self, test_cb, op):
        test_cb(op, 2)

    def _test_binop_rhs_neg_int(self, test_cb, op):
        test_cb(op, -23)

    def _test_binop_rhs_zero_int(self, test_cb, op):
        test_cb(op, 0)

    def _test_binop_rhs_pos_vint(self, test_cb, op):
        test_cb(op, bt2.create_value(2))

    def _test_binop_rhs_neg_vint(self, test_cb, op):
        test_cb(op, bt2.create_value(-23))

    def _test_binop_rhs_zero_vint(self, test_cb, op):
        test_cb(op, bt2.create_value(0))

    def _test_binop_rhs_pos_float(self, test_cb, op):
        test_cb(op, 2.2)

    def _test_binop_rhs_neg_float(self, test_cb, op):
        test_cb(op, -23.4)

    def _test_binop_rhs_zero_float(self, test_cb, op):
        test_cb(op, 0.0)

    def _test_binop_rhs_complex(self, test_cb, op):
        test_cb(op, -23 + 19j)

    def _test_binop_rhs_zero_complex(self, test_cb, op):
        test_cb(op, 0j)

    def _test_binop_rhs_pos_vfloat(self, test_cb, op):
        test_cb(op, bt2.create_value(2.2))

    def _test_binop_rhs_neg_vfloat(self, test_cb, op):
        test_cb(op, bt2.create_value(-23.4))

    def _test_binop_rhs_zero_vfloat(self, test_cb, op):
        test_cb(op, bt2.create_value(0.0))

    def _test_binop_type_false(self, op):
        self._test_binop_rhs_false(self._test_binop_type, op)

    def _test_binop_type_true(self, op):
        self._test_binop_rhs_true(self._test_binop_type, op)

    def _test_binop_type_pos_int(self, op):
        self._test_binop_rhs_pos_int(self._test_binop_type, op)

    def _test_binop_type_neg_int(self, op):
        self._test_binop_rhs_neg_int(self._test_binop_type, op)

    def _test_binop_type_zero_int(self, op):
        self._test_binop_rhs_zero_int(self._test_binop_type, op)

    def _test_binop_type_pos_vint(self, op):
        self._test_binop_rhs_pos_vint(self._test_binop_type, op)

    def _test_binop_type_neg_vint(self, op):
        self._test_binop_rhs_neg_vint(self._test_binop_type, op)

    def _test_binop_type_zero_vint(self, op):
        self._test_binop_rhs_zero_vint(self._test_binop_type, op)

    def _test_binop_type_pos_float(self, op):
        self._test_binop_rhs_pos_float(self._test_binop_type, op)

    def _test_binop_type_neg_float(self, op):
        self._test_binop_rhs_neg_float(self._test_binop_type, op)

    def _test_binop_type_zero_float(self, op):
        self._test_binop_rhs_zero_float(self._test_binop_type, op)

    def _test_binop_type_pos_vfloat(self, op):
        self._test_binop_rhs_pos_vfloat(self._test_binop_type, op)

    def _test_binop_type_neg_vfloat(self, op):
        self._test_binop_rhs_neg_vfloat(self._test_binop_type, op)

    def _test_binop_type_zero_vfloat(self, op):
        self._test_binop_rhs_zero_vfloat(self._test_binop_type, op)

    def _test_binop_type_complex(self, op):
        self._test_binop_rhs_complex(self._test_binop_type, op)

    def _test_binop_type_zero_complex(self, op):
        self._test_binop_rhs_zero_complex(self._test_binop_type, op)

    def _test_binop_value_false(self, op):
        self._test_binop_rhs_false(self._test_binop_value, op)

    def _test_binop_value_true(self, op):
        self._test_binop_rhs_true(self._test_binop_value, op)

    def _test_binop_value_pos_int(self, op):
        self._test_binop_rhs_pos_int(self._test_binop_value, op)

    def _test_binop_value_neg_int(self, op):
        self._test_binop_rhs_neg_int(self._test_binop_value, op)

    def _test_binop_value_zero_int(self, op):
        self._test_binop_rhs_zero_int(self._test_binop_value, op)

    def _test_binop_value_pos_vint(self, op):
        self._test_binop_rhs_pos_vint(self._test_binop_value, op)

    def _test_binop_value_neg_vint(self, op):
        self._test_binop_rhs_neg_vint(self._test_binop_value, op)

    def _test_binop_value_zero_vint(self, op):
        self._test_binop_rhs_zero_vint(self._test_binop_value, op)

    def _test_binop_value_pos_float(self, op):
        self._test_binop_rhs_pos_float(self._test_binop_value, op)

    def _test_binop_value_neg_float(self, op):
        self._test_binop_rhs_neg_float(self._test_binop_value, op)

    def _test_binop_value_zero_float(self, op):
        self._test_binop_rhs_zero_float(self._test_binop_value, op)

    def _test_binop_value_pos_vfloat(self, op):
        self._test_binop_rhs_pos_vfloat(self._test_binop_value, op)

    def _test_binop_value_neg_vfloat(self, op):
        self._test_binop_rhs_neg_vfloat(self._test_binop_value, op)

    def _test_binop_value_zero_vfloat(self, op):
        self._test_binop_rhs_zero_vfloat(self._test_binop_value, op)

    def _test_binop_value_complex(self, op):
        self._test_binop_rhs_complex(self._test_binop_value, op)

    def _test_binop_value_zero_complex(self, op):
        self._test_binop_rhs_zero_complex(self._test_binop_value, op)

    def _test_binop_lhs_addr_same_false(self, op):
        self._test_binop_rhs_false(self._test_binop_lhs_addr_same, op)

    def _test_binop_lhs_addr_same_true(self, op):
        self._test_binop_rhs_true(self._test_binop_lhs_addr_same, op)

    def _test_binop_lhs_addr_same_pos_int(self, op):
        self._test_binop_rhs_pos_int(self._test_binop_lhs_addr_same, op)

    def _test_binop_lhs_addr_same_neg_int(self, op):
        self._test_binop_rhs_neg_int(self._test_binop_lhs_addr_same, op)

    def _test_binop_lhs_addr_same_zero_int(self, op):
        self._test_binop_rhs_zero_int(self._test_binop_lhs_addr_same, op)

    def _test_binop_lhs_addr_same_pos_vint(self, op):
        self._test_binop_rhs_pos_vint(self._test_binop_lhs_addr_same, op)

    def _test_binop_lhs_addr_same_neg_vint(self, op):
        self._test_binop_rhs_neg_vint(self._test_binop_lhs_addr_same, op)

    def _test_binop_lhs_addr_same_zero_vint(self, op):
        self._test_binop_rhs_zero_vint(self._test_binop_lhs_addr_same, op)

    def _test_binop_lhs_addr_same_pos_float(self, op):
        self._test_binop_rhs_pos_float(self._test_binop_lhs_addr_same, op)

    def _test_binop_lhs_addr_same_neg_float(self, op):
        self._test_binop_rhs_neg_float(self._test_binop_lhs_addr_same, op)

    def _test_binop_lhs_addr_same_zero_float(self, op):
        self._test_binop_rhs_zero_float(self._test_binop_lhs_addr_same, op)

    def _test_binop_lhs_addr_same_pos_vfloat(self, op):
        self._test_binop_rhs_pos_vfloat(self._test_binop_lhs_addr_same, op)

    def _test_binop_lhs_addr_same_neg_vfloat(self, op):
        self._test_binop_rhs_neg_vfloat(self._test_binop_lhs_addr_same, op)

    def _test_binop_lhs_addr_same_zero_vfloat(self, op):
        self._test_binop_rhs_zero_vfloat(self._test_binop_lhs_addr_same, op)

    def _test_binop_lhs_addr_same_complex(self, op):
        self._test_binop_rhs_complex(self._test_binop_lhs_addr_same, op)

    def _test_binop_lhs_addr_same_zero_complex(self, op):
        self._test_binop_rhs_zero_complex(self._test_binop_lhs_addr_same, op)

    def _test_binop_lhs_value_same_false(self, op):
        self._test_binop_rhs_false(self._test_binop_lhs_value_same, op)

    def _test_binop_lhs_value_same_true(self, op):
        self._test_binop_rhs_true(self._test_binop_lhs_value_same, op)

    def _test_binop_lhs_value_same_pos_int(self, op):
        self._test_binop_rhs_pos_int(self._test_binop_lhs_value_same, op)

    def _test_binop_lhs_value_same_neg_int(self, op):
        self._test_binop_rhs_neg_int(self._test_binop_lhs_value_same, op)

    def _test_binop_lhs_value_same_zero_int(self, op):
        self._test_binop_rhs_zero_int(self._test_binop_lhs_value_same, op)

    def _test_binop_lhs_value_same_pos_vint(self, op):
        self._test_binop_rhs_pos_vint(self._test_binop_lhs_value_same, op)

    def _test_binop_lhs_value_same_neg_vint(self, op):
        self._test_binop_rhs_neg_vint(self._test_binop_lhs_value_same, op)

    def _test_binop_lhs_value_same_zero_vint(self, op):
        self._test_binop_rhs_zero_vint(self._test_binop_lhs_value_same, op)

    def _test_binop_lhs_value_same_pos_float(self, op):
        self._test_binop_rhs_pos_float(self._test_binop_lhs_value_same, op)

    def _test_binop_lhs_value_same_neg_float(self, op):
        self._test_binop_rhs_neg_float(self._test_binop_lhs_value_same, op)

    def _test_binop_lhs_value_same_zero_float(self, op):
        self._test_binop_rhs_zero_float(self._test_binop_lhs_value_same, op)

    def _test_binop_lhs_value_same_pos_vfloat(self, op):
        self._test_binop_rhs_pos_vfloat(self._test_binop_lhs_value_same, op)

    def _test_binop_lhs_value_same_neg_vfloat(self, op):
        self._test_binop_rhs_neg_vfloat(self._test_binop_lhs_value_same, op)

    def _test_binop_lhs_value_same_zero_vfloat(self, op):
        self._test_binop_rhs_zero_vfloat(self._test_binop_lhs_value_same, op)

    def _test_binop_lhs_value_same_complex(self, op):
        self._test_binop_rhs_complex(self._test_binop_lhs_value_same, op)

    def _test_binop_lhs_value_same_zero_complex(self, op):
        self._test_binop_rhs_zero_complex(self._test_binop_lhs_value_same, op)

    def test_bool_op(self):
        self.assertEqual(bool(self._def), bool(self._def_value))

    def test_int_op(self):
        self.assertEqual(int(self._def), int(self._def_value))

    def test_float_op(self):
        self.assertEqual(float(self._def), float(self._def_value))

    def test_complex_op(self):
        self.assertEqual(complex(self._def), complex(self._def_value))

    def test_str_op(self):
        self.assertEqual(str(self._def), str(self._def_value))

    def test_eq_none(self):
        # Disable the "comparison to None" warning, as this is precisely what
        # we want to test here.
        self.assertFalse(self._def == None)  # noqa: E711

    def test_ne_none(self):
        # Disable the "comparison to None" warning, as this is precisely what
        # we want to test here.
        self.assertTrue(self._def != None)  # noqa: E711


# This is a list of binary operators used for
# _inject_numeric_testing_methods().
#
# Each entry is a pair of binary operator name (used as part of the
# created testing method's name) and operator function.
_BINOPS = (
    ('lt', operator.lt),
    ('le', operator.le),
    ('eq', operator.eq),
    ('ne', operator.ne),
    ('ge', operator.ge),
    ('gt', operator.gt),
    ('add', operator.add),
    ('radd', lambda a, b: operator.add(b, a)),
    ('and', operator.and_),
    ('rand', lambda a, b: operator.and_(b, a)),
    ('floordiv', operator.floordiv),
    ('rfloordiv', lambda a, b: operator.floordiv(b, a)),
    ('lshift', operator.lshift),
    ('rlshift', lambda a, b: operator.lshift(b, a)),
    ('mod', operator.mod),
    ('rmod', lambda a, b: operator.mod(b, a)),
    ('mul', operator.mul),
    ('rmul', lambda a, b: operator.mul(b, a)),
    ('or', operator.or_),
    ('ror', lambda a, b: operator.or_(b, a)),
    ('pow', operator.pow),
    ('rpow', lambda a, b: operator.pow(b, a)),
    ('rshift', operator.rshift),
    ('rrshift', lambda a, b: operator.rshift(b, a)),
    ('sub', operator.sub),
    ('rsub', lambda a, b: operator.sub(b, a)),
    ('truediv', operator.truediv),
    ('rtruediv', lambda a, b: operator.truediv(b, a)),
    ('xor', operator.xor),
    ('rxor', lambda a, b: operator.xor(b, a)),
)


# This is a list of unary operators used for
# _inject_numeric_testing_methods().
#
# Each entry is a pair of unary operator name (used as part of the
# created testing method's name) and operator function.
_UNARYOPS = (
    ('neg', operator.neg),
    ('pos', operator.pos),
    ('abs', operator.abs),
    ('invert', operator.invert),
    ('round', round),
    ('round_0', partial(round, ndigits=0)),
    ('round_1', partial(round, ndigits=1)),
    ('round_2', partial(round, ndigits=2)),
    ('round_3', partial(round, ndigits=3)),
    ('ceil', math.ceil),
    ('floor', math.floor),
    ('trunc', math.trunc),
)


# This function injects a bunch of testing methods to a numeric
# value test case.
#
# It is meant to be used like this:
#
#     _inject_numeric_testing_methods(MyNumericValueTestCase)
#
# This function injects:
#
# * One testing method for each _TestNumericValue._test_binop_*()
#   method, for each binary operator in the _BINOPS tuple.
#
# * One testing method for each _TestNumericValue._test_unaryop*()
#   method, for each unary operator in the _UNARYOPS tuple.
def _inject_numeric_testing_methods(cls):
    def test_binop_name(suffix):
        return 'test_binop_{}_{}'.format(name, suffix)

    def test_unaryop_name(suffix):
        return 'test_unaryop_{}_{}'.format(name, suffix)

    # inject testing methods for each binary operation
    for name, binop in _BINOPS:
        setattr(
            cls,
            test_binop_name('unknown'),
            partialmethod(_TestNumericValue._test_binop_unknown, op=binop),
        )
        setattr(
            cls,
            test_binop_name('none'),
            partialmethod(_TestNumericValue._test_binop_none, op=binop),
        )
        setattr(
            cls,
            test_binop_name('type_true'),
            partialmethod(_TestNumericValue._test_binop_type_true, op=binop),
        )
        setattr(
            cls,
            test_binop_name('type_pos_int'),
            partialmethod(_TestNumericValue._test_binop_type_pos_int, op=binop),
        )
        setattr(
            cls,
            test_binop_name('type_pos_vint'),
            partialmethod(_TestNumericValue._test_binop_type_pos_vint, op=binop),
        )
        setattr(
            cls,
            test_binop_name('value_true'),
            partialmethod(_TestNumericValue._test_binop_value_true, op=binop),
        )
        setattr(
            cls,
            test_binop_name('value_pos_int'),
            partialmethod(_TestNumericValue._test_binop_value_pos_int, op=binop),
        )
        setattr(
            cls,
            test_binop_name('value_pos_vint'),
            partialmethod(_TestNumericValue._test_binop_value_pos_vint, op=binop),
        )
        setattr(
            cls,
            test_binop_name('lhs_addr_same_true'),
            partialmethod(_TestNumericValue._test_binop_lhs_addr_same_true, op=binop),
        )
        setattr(
            cls,
            test_binop_name('lhs_addr_same_pos_int'),
            partialmethod(
                _TestNumericValue._test_binop_lhs_addr_same_pos_int, op=binop
            ),
        )
        setattr(
            cls,
            test_binop_name('lhs_addr_same_pos_vint'),
            partialmethod(
                _TestNumericValue._test_binop_lhs_addr_same_pos_vint, op=binop
            ),
        )
        setattr(
            cls,
            test_binop_name('lhs_value_same_true'),
            partialmethod(_TestNumericValue._test_binop_lhs_value_same_true, op=binop),
        )
        setattr(
            cls,
            test_binop_name('lhs_value_same_pos_int'),
            partialmethod(
                _TestNumericValue._test_binop_lhs_value_same_pos_int, op=binop
            ),
        )
        setattr(
            cls,
            test_binop_name('lhs_value_same_pos_vint'),
            partialmethod(
                _TestNumericValue._test_binop_lhs_value_same_pos_vint, op=binop
            ),
        )
        setattr(
            cls,
            test_binop_name('type_neg_int'),
            partialmethod(_TestNumericValue._test_binop_type_neg_int, op=binop),
        )
        setattr(
            cls,
            test_binop_name('type_neg_vint'),
            partialmethod(_TestNumericValue._test_binop_type_neg_vint, op=binop),
        )
        setattr(
            cls,
            test_binop_name('value_neg_int'),
            partialmethod(_TestNumericValue._test_binop_value_neg_int, op=binop),
        )
        setattr(
            cls,
            test_binop_name('value_neg_vint'),
            partialmethod(_TestNumericValue._test_binop_value_neg_vint, op=binop),
        )
        setattr(
            cls,
            test_binop_name('lhs_addr_same_neg_int'),
            partialmethod(
                _TestNumericValue._test_binop_lhs_addr_same_neg_int, op=binop
            ),
        )
        setattr(
            cls,
            test_binop_name('lhs_addr_same_neg_vint'),
            partialmethod(
                _TestNumericValue._test_binop_lhs_addr_same_neg_vint, op=binop
            ),
        )
        setattr(
            cls,
            test_binop_name('lhs_value_same_neg_int'),
            partialmethod(
                _TestNumericValue._test_binop_lhs_value_same_neg_int, op=binop
            ),
        )
        setattr(
            cls,
            test_binop_name('lhs_value_same_neg_vint'),
            partialmethod(
                _TestNumericValue._test_binop_lhs_value_same_neg_vint, op=binop
            ),
        )
        setattr(
            cls,
            test_binop_name('type_false'),
            partialmethod(_TestNumericValue._test_binop_type_false, op=binop),
        )
        setattr(
            cls,
            test_binop_name('type_zero_int'),
            partialmethod(_TestNumericValue._test_binop_type_zero_int, op=binop),
        )
        setattr(
            cls,
            test_binop_name('type_zero_vint'),
            partialmethod(_TestNumericValue._test_binop_type_zero_vint, op=binop),
        )
        setattr(
            cls,
            test_binop_name('value_false'),
            partialmethod(_TestNumericValue._test_binop_value_false, op=binop),
        )
        setattr(
            cls,
            test_binop_name('value_zero_int'),
            partialmethod(_TestNumericValue._test_binop_value_zero_int, op=binop),
        )
        setattr(
            cls,
            test_binop_name('value_zero_vint'),
            partialmethod(_TestNumericValue._test_binop_value_zero_vint, op=binop),
        )
        setattr(
            cls,
            test_binop_name('lhs_addr_same_false'),
            partialmethod(_TestNumericValue._test_binop_lhs_addr_same_false, op=binop),
        )
        setattr(
            cls,
            test_binop_name('lhs_addr_same_zero_int'),
            partialmethod(
                _TestNumericValue._test_binop_lhs_addr_same_zero_int, op=binop
            ),
        )
        setattr(
            cls,
            test_binop_name('lhs_addr_same_zero_vint'),
            partialmethod(
                _TestNumericValue._test_binop_lhs_addr_same_zero_vint, op=binop
            ),
        )
        setattr(
            cls,
            test_binop_name('lhs_value_same_false'),
            partialmethod(_TestNumericValue._test_binop_lhs_value_same_false, op=binop),
        )
        setattr(
            cls,
            test_binop_name('lhs_value_same_zero_int'),
            partialmethod(
                _TestNumericValue._test_binop_lhs_value_same_zero_int, op=binop
            ),
        )
        setattr(
            cls,
            test_binop_name('lhs_value_same_zero_vint'),
            partialmethod(
                _TestNumericValue._test_binop_lhs_value_same_zero_vint, op=binop
            ),
        )
        setattr(
            cls,
            test_binop_name('type_neg_float'),
            partialmethod(_TestNumericValue._test_binop_type_neg_float, op=binop),
        )
        setattr(
            cls,
            test_binop_name('type_neg_vfloat'),
            partialmethod(_TestNumericValue._test_binop_type_neg_vfloat, op=binop),
        )
        setattr(
            cls,
            test_binop_name('value_neg_float'),
            partialmethod(_TestNumericValue._test_binop_value_neg_float, op=binop),
        )
        setattr(
            cls,
            test_binop_name('value_neg_vfloat'),
            partialmethod(_TestNumericValue._test_binop_value_neg_vfloat, op=binop),
        )
        setattr(
            cls,
            test_binop_name('lhs_addr_same_neg_float'),
            partialmethod(
                _TestNumericValue._test_binop_lhs_addr_same_neg_float, op=binop
            ),
        )
        setattr(
            cls,
            test_binop_name('lhs_addr_same_neg_vfloat'),
            partialmethod(
                _TestNumericValue._test_binop_lhs_addr_same_neg_vfloat, op=binop
            ),
        )
        setattr(
            cls,
            test_binop_name('lhs_value_same_neg_float'),
            partialmethod(
                _TestNumericValue._test_binop_lhs_value_same_neg_float, op=binop
            ),
        )
        setattr(
            cls,
            test_binop_name('lhs_value_same_neg_vfloat'),
            partialmethod(
                _TestNumericValue._test_binop_lhs_value_same_neg_vfloat, op=binop
            ),
        )
        setattr(
            cls,
            test_binop_name('type_pos_float'),
            partialmethod(_TestNumericValue._test_binop_type_pos_float, op=binop),
        )
        setattr(
            cls,
            test_binop_name('type_pos_vfloat'),
            partialmethod(_TestNumericValue._test_binop_type_pos_vfloat, op=binop),
        )
        setattr(
            cls,
            test_binop_name('value_pos_float'),
            partialmethod(_TestNumericValue._test_binop_value_pos_float, op=binop),
        )
        setattr(
            cls,
            test_binop_name('value_pos_vfloat'),
            partialmethod(_TestNumericValue._test_binop_value_pos_vfloat, op=binop),
        )
        setattr(
            cls,
            test_binop_name('lhs_addr_same_pos_float'),
            partialmethod(
                _TestNumericValue._test_binop_lhs_addr_same_pos_float, op=binop
            ),
        )
        setattr(
            cls,
            test_binop_name('lhs_addr_same_pos_vfloat'),
            partialmethod(
                _TestNumericValue._test_binop_lhs_addr_same_pos_vfloat, op=binop
            ),
        )
        setattr(
            cls,
            test_binop_name('lhs_value_same_pos_float'),
            partialmethod(
                _TestNumericValue._test_binop_lhs_value_same_pos_float, op=binop
            ),
        )
        setattr(
            cls,
            test_binop_name('lhs_value_same_pos_vfloat'),
            partialmethod(
                _TestNumericValue._test_binop_lhs_value_same_pos_vfloat, op=binop
            ),
        )
        setattr(
            cls,
            test_binop_name('type_zero_float'),
            partialmethod(_TestNumericValue._test_binop_type_zero_float, op=binop),
        )
        setattr(
            cls,
            test_binop_name('type_zero_vfloat'),
            partialmethod(_TestNumericValue._test_binop_type_zero_vfloat, op=binop),
        )
        setattr(
            cls,
            test_binop_name('value_zero_float'),
            partialmethod(_TestNumericValue._test_binop_value_zero_float, op=binop),
        )
        setattr(
            cls,
            test_binop_name('value_zero_vfloat'),
            partialmethod(_TestNumericValue._test_binop_value_zero_vfloat, op=binop),
        )
        setattr(
            cls,
            test_binop_name('lhs_addr_same_zero_float'),
            partialmethod(
                _TestNumericValue._test_binop_lhs_addr_same_zero_float, op=binop
            ),
        )
        setattr(
            cls,
            test_binop_name('lhs_addr_same_zero_vfloat'),
            partialmethod(
                _TestNumericValue._test_binop_lhs_addr_same_zero_vfloat, op=binop
            ),
        )
        setattr(
            cls,
            test_binop_name('lhs_value_same_zero_float'),
            partialmethod(
                _TestNumericValue._test_binop_lhs_value_same_zero_float, op=binop
            ),
        )
        setattr(
            cls,
            test_binop_name('lhs_value_same_zero_vfloat'),
            partialmethod(
                _TestNumericValue._test_binop_lhs_value_same_zero_vfloat, op=binop
            ),
        )
        setattr(
            cls,
            test_binop_name('type_complex'),
            partialmethod(_TestNumericValue._test_binop_type_complex, op=binop),
        )
        setattr(
            cls,
            test_binop_name('type_zero_complex'),
            partialmethod(_TestNumericValue._test_binop_type_zero_complex, op=binop),
        )
        setattr(
            cls,
            test_binop_name('value_complex'),
            partialmethod(_TestNumericValue._test_binop_value_complex, op=binop),
        )
        setattr(
            cls,
            test_binop_name('value_zero_complex'),
            partialmethod(_TestNumericValue._test_binop_value_zero_complex, op=binop),
        )
        setattr(
            cls,
            test_binop_name('lhs_addr_same_complex'),
            partialmethod(
                _TestNumericValue._test_binop_lhs_addr_same_complex, op=binop
            ),
        )
        setattr(
            cls,
            test_binop_name('lhs_addr_same_zero_complex'),
            partialmethod(
                _TestNumericValue._test_binop_lhs_addr_same_zero_complex, op=binop
            ),
        )
        setattr(
            cls,
            test_binop_name('lhs_value_same_complex'),
            partialmethod(
                _TestNumericValue._test_binop_lhs_value_same_complex, op=binop
            ),
        )
        setattr(
            cls,
            test_binop_name('lhs_value_same_zero_complex'),
            partialmethod(
                _TestNumericValue._test_binop_lhs_value_same_zero_complex, op=binop
            ),
        )

    # inject testing methods for each unary operation
    for name, unaryop in _UNARYOPS:
        setattr(
            cls,
            test_unaryop_name('type'),
            partialmethod(_TestNumericValue._test_unaryop_type, op=unaryop),
        )
        setattr(
            cls,
            test_unaryop_name('value'),
            partialmethod(_TestNumericValue._test_unaryop_value, op=unaryop),
        )
        setattr(
            cls,
            test_unaryop_name('addr_same'),
            partialmethod(_TestNumericValue._test_unaryop_addr_same, op=unaryop),
        )
        setattr(
            cls,
            test_unaryop_name('value_same'),
            partialmethod(_TestNumericValue._test_unaryop_value_same, op=unaryop),
        )


class CreateValueFuncTestCase(unittest.TestCase):
    def test_create_none(self):
        v = bt2.create_value(None)
        self.assertIsNone(v)

    def test_create_bool_false(self):
        v = bt2.create_value(False)
        self.assertIsInstance(v, bt2.BoolValue)
        self.assertFalse(v)

    def test_create_bool_true(self):
        v = bt2.create_value(True)
        self.assertIsInstance(v, bt2.BoolValue)
        self.assertTrue(v)

    def test_create_int_pos(self):
        raw = 23
        v = bt2.create_value(raw)
        self.assertIsInstance(v, bt2.SignedIntegerValue)
        self.assertEqual(v, raw)

    def test_create_int_neg(self):
        raw = -23
        v = bt2.create_value(raw)
        self.assertIsInstance(v, bt2.SignedIntegerValue)
        self.assertEqual(v, raw)

    def test_create_float_pos(self):
        raw = 17.5
        v = bt2.create_value(raw)
        self.assertIsInstance(v, bt2.RealValue)
        self.assertEqual(v, raw)

    def test_create_float_neg(self):
        raw = -17.5
        v = bt2.create_value(raw)
        self.assertIsInstance(v, bt2.RealValue)
        self.assertEqual(v, raw)

    def test_create_string(self):
        raw = 'salut'
        v = bt2.create_value(raw)
        self.assertIsInstance(v, bt2.StringValue)
        self.assertEqual(v, raw)

    def test_create_string_empty(self):
        raw = ''
        v = bt2.create_value(raw)
        self.assertIsInstance(v, bt2.StringValue)
        self.assertEqual(v, raw)

    def test_create_array_from_list(self):
        raw = [1, 2, 3]
        v = bt2.create_value(raw)
        self.assertIsInstance(v, bt2.ArrayValue)
        self.assertEqual(v, raw)

    def test_create_array_from_tuple(self):
        raw = 4, 5, 6
        v = bt2.create_value(raw)
        self.assertIsInstance(v, bt2.ArrayValue)
        self.assertEqual(v, raw)

    def test_create_array_from_empty_list(self):
        raw = []
        v = bt2.create_value(raw)
        self.assertIsInstance(v, bt2.ArrayValue)
        self.assertEqual(v, raw)

    def test_create_array_from_empty_tuple(self):
        raw = ()
        v = bt2.create_value(raw)
        self.assertIsInstance(v, bt2.ArrayValue)
        self.assertEqual(v, raw)

    def test_create_map(self):
        raw = {'salut': 23}
        v = bt2.create_value(raw)
        self.assertIsInstance(v, bt2.MapValue)
        self.assertEqual(v, raw)

    def test_create_map_empty(self):
        raw = {}
        v = bt2.create_value(raw)
        self.assertIsInstance(v, bt2.MapValue)
        self.assertEqual(v, raw)

    def test_create_vfalse(self):
        v = bt2.create_value(bt2.create_value(False))
        self.assertIsInstance(v, bt2.BoolValue)
        self.assertFalse(v)

    def test_create_invalid(self):
        class A:
            pass

        a = A()

        with self.assertRaisesRegex(
            TypeError, "cannot create value object from 'A' object"
        ):
            bt2.create_value(a)


def _create_const_value(value):
    class MySink(bt2._UserSinkComponent):
        def _user_consume(self):
            pass

        @classmethod
        def _user_query(cls, priv_query_exec, obj, params, method_obj):
            nonlocal value
            return {'my_value': value}

    res = bt2.QueryExecutor(MySink, 'obj', None).query()
    return res['my_value']


class BoolValueTestCase(_TestNumericValue, unittest.TestCase):
    def setUp(self):
        self._f = bt2.BoolValue(False)
        self._t = bt2.BoolValue(True)
        self._def = self._f
        self._def_value = False
        self._def_new_value = True

    def tearDown(self):
        del self._f
        del self._t
        del self._def

    def _assert_expecting_bool(self):
        return self.assertRaisesRegex(TypeError, r"expecting a 'bool' object")

    def test_create_default(self):
        b = bt2.BoolValue()
        self.assertFalse(b)

    def test_create_false(self):
        self.assertFalse(self._f)

    def test_create_true(self):
        self.assertTrue(self._t)

    def test_create_from_vfalse(self):
        b = bt2.BoolValue(self._f)
        self.assertFalse(b)

    def test_create_from_vtrue(self):
        b = bt2.BoolValue(self._t)
        self.assertTrue(b)

    def test_create_from_int_non_zero(self):
        with self.assertRaises(TypeError):
            bt2.BoolValue(23)

    def test_create_from_int_zero(self):
        with self.assertRaises(TypeError):
            bt2.BoolValue(0)

    def test_assign_true(self):
        b = bt2.BoolValue()
        b.value = True
        self.assertTrue(b)

    def test_assign_false(self):
        b = bt2.BoolValue()
        b.value = False
        self.assertFalse(b)

    def test_assign_vtrue(self):
        b = bt2.BoolValue()
        b.value = self._t
        self.assertTrue(b)

    def test_assign_vfalse(self):
        b = bt2.BoolValue()
        b.value = False
        self.assertFalse(b)

    def test_assign_int(self):
        with self.assertRaises(TypeError):
            b = bt2.BoolValue()
            b.value = 23

    def test_bool_op(self):
        self.assertEqual(bool(self._def), bool(self._def_value))

    def test_str_op(self):
        self.assertEqual(str(self._def), str(self._def_value))

    def test_eq_none(self):
        # Disable the "comparison to None" warning, as this is precisely what
        # we want to test here.
        self.assertFalse(self._def == None)  # noqa: E711

    def test_ne_none(self):
        # Disable the "comparison to None" warning, as this is precisely what
        # we want to test here.
        self.assertTrue(self._def != None)  # noqa: E711

    def test_vfalse_eq_false(self):
        self.assertEqual(self._f, False)

    def test_vfalse_ne_true(self):
        self.assertNotEqual(self._f, True)

    def test_vtrue_eq_true(self):
        self.assertEqual(self._t, True)

    def test_vtrue_ne_false(self):
        self.assertNotEqual(self._t, False)


_inject_numeric_testing_methods(BoolValueTestCase)


class _TestIntegerValue(_TestNumericValue):
    def setUp(self):
        self._pv = 23
        self._ip = self._CLS(self._pv)
        self._def = self._ip
        self._def_value = self._pv
        self._def_new_value = 101

    def tearDown(self):
        del self._ip
        del self._def
        del self._def_value

    def _assert_expecting_int(self):
        return self.assertRaisesRegex(TypeError, r'expecting an integral number object')

    def _assert_expecting_int64(self):
        return self.assertRaisesRegex(
            ValueError, r"expecting a signed 64-bit integral value"
        )

    def _assert_expecting_uint64(self):
        return self.assertRaisesRegex(
            ValueError, r"expecting an unsigned 64-bit integral value"
        )

    def test_create_default(self):
        i = self._CLS()
        self.assertEqual(i, 0)

    def test_create_pos(self):
        self.assertEqual(self._ip, self._pv)

    def test_create_neg(self):
        self.assertEqual(self._in, self._nv)

    def test_create_from_vint(self):
        i = self._CLS(self._ip)
        self.assertEqual(i, self._pv)

    def test_create_from_false(self):
        i = self._CLS(False)
        self.assertFalse(i)

    def test_create_from_true(self):
        i = self._CLS(True)
        self.assertTrue(i)

    def test_create_from_unknown(self):
        class A:
            pass

        with self._assert_expecting_int():
            self._CLS(A())

    def test_create_from_varray(self):
        with self._assert_expecting_int():
            self._CLS(bt2.ArrayValue())

    def test_assign_true(self):
        raw = True
        self._def.value = raw
        self.assertEqual(self._def, raw)

    def test_assign_false(self):
        raw = False
        self._def.value = raw
        self.assertEqual(self._def, raw)

    def test_assign_pos_int(self):
        raw = 477
        self._def.value = raw
        self.assertEqual(self._def, raw)

    def test_assign_vint(self):
        raw = 999
        self._def.value = bt2.create_value(raw)
        self.assertEqual(self._def, raw)


class SignedIntegerValueTestCase(_TestIntegerValue, unittest.TestCase):
    _CLS = bt2.SignedIntegerValue

    def setUp(self):
        super().setUp()
        self._nv = -52
        self._in = self._CLS(self._nv)
        self._def_new_value = -101

    def tearDown(self):
        super().tearDown()
        del self._in

    def test_create_neg(self):
        self.assertEqual(self._in, self._nv)

    def test_create_pos_too_big(self):
        with self._assert_expecting_int64():
            self._CLS(2 ** 63)

    def test_create_neg_too_big(self):
        with self._assert_expecting_int64():
            self._CLS(-(2 ** 63) - 1)

    def test_assign_neg_int(self):
        raw = -13
        self._def.value = raw
        self.assertEqual(self._def, raw)

    def test_compare_big_int(self):
        # Larger than the IEEE 754 double-precision exact representation of
        # integers.
        raw = (2 ** 53) + 1
        v = bt2.create_value(raw)
        self.assertEqual(v, raw)


_inject_numeric_testing_methods(SignedIntegerValueTestCase)


class UnsignedIntegerValueTestCase(_TestIntegerValue, unittest.TestCase):
    _CLS = bt2.UnsignedIntegerValue

    def test_create_pos_too_big(self):
        with self._assert_expecting_uint64():
            self._CLS(2 ** 64)

    def test_create_neg(self):
        with self._assert_expecting_uint64():
            self._CLS(-1)


_inject_numeric_testing_methods(UnsignedIntegerValueTestCase)


class RealValueTestCase(_TestNumericValue, unittest.TestCase):
    def setUp(self):
        self._pv = 23.4
        self._nv = -52.7
        self._fp = bt2.RealValue(self._pv)
        self._fn = bt2.RealValue(self._nv)
        self._def = self._fp
        self._def_value = self._pv
        self._def_new_value = -101.88

    def tearDown(self):
        del self._fp
        del self._fn
        del self._def
        del self._def_value

    def _assert_expecting_float(self):
        return self.assertRaisesRegex(TypeError, r"expecting a real number object")

    def _test_invalid_op(self, cb):
        with self.assertRaises(TypeError):
            cb()

    def test_create_default(self):
        f = bt2.RealValue()
        self.assertEqual(f, 0.0)

    def test_create_pos(self):
        self.assertEqual(self._fp, self._pv)

    def test_create_neg(self):
        self.assertEqual(self._fn, self._nv)

    def test_create_from_false(self):
        f = bt2.RealValue(False)
        self.assertFalse(f)

    def test_create_from_true(self):
        f = bt2.RealValue(True)
        self.assertTrue(f)

    def test_create_from_int(self):
        raw = 17
        f = bt2.RealValue(raw)
        self.assertEqual(f, float(raw))

    def test_create_from_vint(self):
        raw = 17
        f = bt2.RealValue(bt2.create_value(raw))
        self.assertEqual(f, float(raw))

    def test_create_from_vfloat(self):
        raw = 17.17
        f = bt2.RealValue(bt2.create_value(raw))
        self.assertEqual(f, raw)

    def test_create_from_unknown(self):
        class A:
            pass

        with self._assert_expecting_float():
            bt2.RealValue(A())

    def test_create_from_varray(self):
        with self._assert_expecting_float():
            bt2.RealValue(bt2.ArrayValue())

    def test_assign_true(self):
        self._def.value = True
        self.assertTrue(self._def)

    def test_assign_false(self):
        self._def.value = False
        self.assertFalse(self._def)

    def test_assign_pos_int(self):
        raw = 477
        self._def.value = raw
        self.assertEqual(self._def, float(raw))

    def test_assign_neg_int(self):
        raw = -13
        self._def.value = raw
        self.assertEqual(self._def, float(raw))

    def test_assign_vint(self):
        raw = 999
        self._def.value = bt2.create_value(raw)
        self.assertEqual(self._def, float(raw))

    def test_assign_float(self):
        raw = -19.23
        self._def.value = raw
        self.assertEqual(self._def, raw)

    def test_assign_vfloat(self):
        raw = 101.32
        self._def.value = bt2.create_value(raw)
        self.assertEqual(self._def, raw)

    def test_invalid_lshift(self):
        self._test_invalid_op(lambda: self._def << 23)

    def test_invalid_rshift(self):
        self._test_invalid_op(lambda: self._def >> 23)

    def test_invalid_and(self):
        self._test_invalid_op(lambda: self._def & 23)

    def test_invalid_or(self):
        self._test_invalid_op(lambda: self._def | 23)

    def test_invalid_xor(self):
        self._test_invalid_op(lambda: self._def ^ 23)

    def test_invalid_invert(self):
        self._test_invalid_op(lambda: ~self._def)


_inject_numeric_testing_methods(RealValueTestCase)


class StringValueTestCase(_TestCopySimple, unittest.TestCase):
    def setUp(self):
        self._def_value = 'Hello, World!'
        self._def = bt2.StringValue(self._def_value)
        self._def_const = _create_const_value(self._def_value)
        self._def_new_value = 'Yes!'

    def tearDown(self):
        del self._def

    def _assert_expecting_str(self):
        return self.assertRaises(TypeError)

    def test_create_default(self):
        s = bt2.StringValue()
        self.assertEqual(s, '')

    def test_create_from_str(self):
        raw = 'liberté'
        s = bt2.StringValue(raw)
        self.assertEqual(s, raw)

    def test_create_from_vstr(self):
        raw = 'liberté'
        s = bt2.StringValue(bt2.create_value(raw))
        self.assertEqual(s, raw)

    def test_create_from_unknown(self):
        class A:
            pass

        with self._assert_expecting_str():
            bt2.StringValue(A())

    def test_create_from_varray(self):
        with self._assert_expecting_str():
            bt2.StringValue(bt2.ArrayValue())

    def test_assign_int(self):
        with self._assert_expecting_str():
            self._def.value = 283

    def test_assign_str(self):
        raw = 'zorg'
        self._def = raw
        self.assertEqual(self._def, raw)

    def test_assign_vstr(self):
        raw = 'zorg'
        self._def = bt2.create_value(raw)
        self.assertEqual(self._def, raw)

    def test_eq(self):
        self.assertEqual(self._def, self._def_value)

    def test_const_eq(self):
        self.assertEqual(self._def_const, self._def_value)

    def test_eq_raw(self):
        self.assertNotEqual(self._def, 23)

    def test_lt_vstring(self):
        s1 = bt2.StringValue('allo')
        s2 = bt2.StringValue('bateau')
        self.assertLess(s1, s2)

    def test_lt_string(self):
        s1 = bt2.StringValue('allo')
        self.assertLess(s1, 'bateau')

    def test_le_vstring(self):
        s1 = bt2.StringValue('allo')
        s2 = bt2.StringValue('bateau')
        self.assertLessEqual(s1, s2)

    def test_le_string(self):
        s1 = bt2.StringValue('allo')
        self.assertLessEqual(s1, 'bateau')

    def test_gt_vstring(self):
        s1 = bt2.StringValue('allo')
        s2 = bt2.StringValue('bateau')
        self.assertGreater(s2, s1)

    def test_gt_string(self):
        s1 = bt2.StringValue('allo')
        self.assertGreater('bateau', s1)

    def test_ge_vstring(self):
        s1 = bt2.StringValue('allo')
        s2 = bt2.StringValue('bateau')
        self.assertGreaterEqual(s2, s1)

    def test_ge_string(self):
        s1 = bt2.StringValue('allo')
        self.assertGreaterEqual('bateau', s1)

    def test_in_string(self):
        s1 = bt2.StringValue('beau grand bateau')
        self.assertIn('bateau', s1)

    def test_in_vstring(self):
        s1 = bt2.StringValue('beau grand bateau')
        s2 = bt2.StringValue('bateau')
        self.assertIn(s2, s1)

    def test_bool_op(self):
        self.assertEqual(bool(self._def), bool(self._def_value))

    def test_str_op(self):
        self.assertEqual(str(self._def), str(self._def_value))

    def test_len(self):
        self.assertEqual(len(self._def), len(self._def_value))

    def test_getitem(self):
        self.assertEqual(self._def[5], self._def_value[5])

    def test_const_getitem(self):
        self.assertEqual(self._def_const[5], self._def_value[5])

    def test_iadd_str(self):
        to_append = 'meow meow meow'
        self._def += to_append
        self._def_value += to_append
        self.assertEqual(self._def, self._def_value)

    def test_const_iadd_str(self):
        to_append = 'meow meow meow'
        with self.assertRaises(TypeError):
            self._def_const += to_append

        self.assertEqual(self._def_const, self._def_value)

    def test_append_vstr(self):
        to_append = 'meow meow meow'
        self._def += bt2.create_value(to_append)
        self._def_value += to_append
        self.assertEqual(self._def, self._def_value)


class ArrayValueTestCase(_TestCopySimple, unittest.TestCase):
    def setUp(self):
        self._def_value = [None, False, True, -23, 0, 42, -42.4, 23.17, 'yes']
        self._def = bt2.ArrayValue(copy.deepcopy(self._def_value))
        self._def_const = _create_const_value(copy.deepcopy(self._def_value))

    def tearDown(self):
        del self._def

    def _modify_def(self):
        self._def[2] = 'xyz'

    def _assert_type_error(self):
        return self.assertRaises(TypeError)

    def test_create_default(self):
        a = bt2.ArrayValue()
        self.assertEqual(len(a), 0)

    def test_create_from_array(self):
        self.assertEqual(self._def, self._def_value)

    def test_create_from_tuple(self):
        t = 1, 2, False, None
        a = bt2.ArrayValue(t)
        self.assertEqual(a, t)

    def test_create_from_varray(self):
        va = bt2.ArrayValue(copy.deepcopy(self._def_value))
        a = bt2.ArrayValue(va)
        self.assertEqual(va, a)

    def test_create_from_unknown(self):
        class A:
            pass

        with self._assert_type_error():
            bt2.ArrayValue(A())

    def test_bool_op_true(self):
        self.assertTrue(bool(self._def))

    def test_bool_op_false(self):
        self.assertFalse(bool(bt2.ArrayValue()))

    def test_len(self):
        self.assertEqual(len(self._def), len(self._def_value))

    def test_eq_int(self):
        self.assertNotEqual(self._def, 23)

    def test_const_eq(self):
        a1 = _create_const_value([1, 2, 3])
        a2 = [1, 2, 3]
        self.assertEqual(a1, a2)

    def test_eq_diff_len(self):
        a1 = bt2.create_value([1, 2, 3])
        a2 = bt2.create_value([1, 2])
        self.assertIs(type(a1), bt2.ArrayValue)
        self.assertIs(type(a2), bt2.ArrayValue)
        self.assertNotEqual(a1, a2)

    def test_eq_diff_content_same_len(self):
        a1 = bt2.create_value([1, 2, 3])
        a2 = bt2.create_value([4, 5, 6])
        self.assertNotEqual(a1, a2)

    def test_eq_same_content_same_len(self):
        raw = (3, True, [1, 2.5, None, {'a': 17.6, 'b': None}])
        a1 = bt2.ArrayValue(raw)
        a2 = bt2.ArrayValue(copy.deepcopy(raw))
        self.assertEqual(a1, a2)

    def test_eq_non_sequence_iterable(self):
        dct = collections.OrderedDict([(1, 2), (3, 4), (5, 6)])
        a = bt2.ArrayValue((1, 3, 5))
        self.assertEqual(a, list(dct.keys()))
        self.assertNotEqual(a, dct)

    def test_setitem_int(self):
        raw = 19
        self._def[2] = raw
        self.assertEqual(self._def[2], raw)

    def test_setitem_vint(self):
        raw = 19
        self._def[2] = bt2.create_value(raw)
        self.assertEqual(self._def[2], raw)

    def test_setitem_none(self):
        self._def[2] = None
        self.assertIsNone(self._def[2])

    def test_setitem_index_wrong_type(self):
        with self._assert_type_error():
            self._def['yes'] = 23

    def test_setitem_index_neg(self):
        with self.assertRaises(IndexError):
            self._def[-2] = 23

    def test_setitem_index_out_of_range(self):
        with self.assertRaises(IndexError):
            self._def[len(self._def)] = 23

    def test_const_setitem(self):
        with self.assertRaises(TypeError):
            self._def_const[2] = 19

    def test_append_none(self):
        self._def.append(None)
        self.assertIsNone(self._def[len(self._def) - 1])

    def test_append_int(self):
        raw = 145
        self._def.append(raw)
        self.assertEqual(self._def[len(self._def) - 1], raw)

    def test_const_append(self):
        with self.assertRaises(AttributeError):
            self._def_const.append(12194)

    def test_append_vint(self):
        raw = 145
        self._def.append(bt2.create_value(raw))
        self.assertEqual(self._def[len(self._def) - 1], raw)

    def test_append_unknown(self):
        class A:
            pass

        with self._assert_type_error():
            self._def.append(A())

    def test_iadd(self):
        raw = 4, 5, True
        self._def += raw
        self.assertEqual(self._def[len(self._def) - 3], raw[0])
        self.assertEqual(self._def[len(self._def) - 2], raw[1])
        self.assertEqual(self._def[len(self._def) - 1], raw[2])

    def test_const_iadd(self):
        with self.assertRaises(TypeError):
            self._def_const += 12194

    def test_iadd_unknown(self):
        class A:
            pass

        with self._assert_type_error():
            self._def += A()

    def test_iadd_list_unknown(self):
        class A:
            pass

        with self._assert_type_error():
            self._def += [A()]

    def test_iter(self):
        for velem, elem in zip(self._def, self._def_value):
            self.assertEqual(velem, elem)

    def test_const_iter(self):
        for velem, elem in zip(self._def_const, self._def_value):
            self.assertEqual(velem, elem)

    def test_const_get_item(self):
        item1 = self._def_const[0]
        item2 = self._def_const[2]
        item3 = self._def_const[5]
        item4 = self._def_const[7]
        item5 = self._def_const[8]

        self.assertEqual(item1, None)

        self.assertIs(type(item2), bt2._BoolValueConst)
        self.assertEqual(item2, True)

        self.assertIs(type(item3), bt2._SignedIntegerValueConst)
        self.assertEqual(item3, 42)

        self.assertIs(type(item4), bt2._RealValueConst)
        self.assertEqual(item4, 23.17)

        self.assertIs(type(item5), bt2._StringValueConst)
        self.assertEqual(item5, 'yes')


class MapValueTestCase(_TestCopySimple, unittest.TestCase):
    def setUp(self):
        self._def_value = {
            'none': None,
            'false': False,
            'true': True,
            'neg-int': -23,
            'zero': 0,
            'pos-int': 42,
            'neg-float': -42.4,
            'pos-float': 23.17,
            'str': 'yes',
        }
        self._def = bt2.MapValue(copy.deepcopy(self._def_value))
        self._def_const = _create_const_value(self._def_value)

    def tearDown(self):
        del self._def

    def _modify_def(self):
        self._def['zero'] = 1

    def test_create_default(self):
        m = bt2.MapValue()
        self.assertEqual(len(m), 0)

    def test_create_from_dict(self):
        self.assertEqual(self._def, self._def_value)

    def test_create_from_vmap(self):
        vm = bt2.MapValue(copy.deepcopy(self._def_value))
        m = bt2.MapValue(vm)
        self.assertEqual(vm, m)

    def test_create_from_unknown(self):
        class A:
            pass

        with self.assertRaises(AttributeError):
            bt2.MapValue(A())

    def test_bool_op_true(self):
        self.assertTrue(bool(self._def))

    def test_bool_op_false(self):
        self.assertFalse(bool(bt2.MapValue()))

    def test_len(self):
        self.assertEqual(len(self._def), len(self._def_value))

    def test_const_eq(self):
        a1 = _create_const_value({'a': 1, 'b': 2, 'c': 3})
        a2 = {'a': 1, 'b': 2, 'c': 3}
        self.assertEqual(a1, a2)

    def test_eq_int(self):
        self.assertNotEqual(self._def, 23)

    def test_eq_diff_len(self):
        a1 = bt2.create_value({'a': 1, 'b': 2, 'c': 3})
        a2 = bt2.create_value({'a': 1, 'b': 2})
        self.assertNotEqual(a1, a2)

    def test_const_eq_diff_len(self):
        a1 = _create_const_value({'a': 1, 'b': 2, 'c': 3})
        a2 = _create_const_value({'a': 1, 'b': 2})
        self.assertNotEqual(a1, a2)

    def test_eq_diff_content_same_len(self):
        a1 = bt2.create_value({'a': 1, 'b': 2, 'c': 3})
        a2 = bt2.create_value({'a': 4, 'b': 2, 'c': 3})
        self.assertNotEqual(a1, a2)

    def test_const_eq_diff_content_same_len(self):
        a1 = _create_const_value({'a': 1, 'b': 2, 'c': 3})
        a2 = _create_const_value({'a': 4, 'b': 2, 'c': 3})
        self.assertNotEqual(a1, a2)

    def test_eq_same_content_diff_keys(self):
        a1 = bt2.create_value({'a': 1, 'b': 2, 'c': 3})
        a2 = bt2.create_value({'a': 1, 'k': 2, 'c': 3})
        self.assertNotEqual(a1, a2)

    def test_const_eq_same_content_diff_keys(self):
        a1 = _create_const_value({'a': 1, 'b': 2, 'c': 3})
        a2 = _create_const_value({'a': 1, 'k': 2, 'c': 3})
        self.assertNotEqual(a1, a2)

    def test_eq_same_content_same_len(self):
        raw = {'3': 3, 'True': True, 'array': [1, 2.5, None, {'a': 17.6, 'b': None}]}
        a1 = bt2.MapValue(raw)
        a2 = bt2.MapValue(copy.deepcopy(raw))
        self.assertEqual(a1, a2)
        self.assertEqual(a1, raw)

    def test_const_eq_same_content_same_len(self):
        raw = {'3': 3, 'True': True, 'array': [1, 2.5, None, {'a': 17.6, 'b': None}]}
        a1 = _create_const_value(raw)
        a2 = _create_const_value(copy.deepcopy(raw))
        self.assertEqual(a1, a2)
        self.assertEqual(a1, raw)

    def test_setitem_int(self):
        raw = 19
        self._def['pos-int'] = raw
        self.assertEqual(self._def['pos-int'], raw)

    def test_const_setitem_int(self):
        with self.assertRaises(TypeError):
            self._def_const['pos-int'] = 19

    def test_setitem_vint(self):
        raw = 19
        self._def['pos-int'] = bt2.create_value(raw)
        self.assertEqual(self._def['pos-int'], raw)

    def test_setitem_none(self):
        self._def['none'] = None
        self.assertIsNone(self._def['none'])

    def test_setitem_new_int(self):
        old_len = len(self._def)
        self._def['new-int'] = 23
        self.assertEqual(self._def['new-int'], 23)
        self.assertEqual(len(self._def), old_len + 1)

    def test_setitem_index_wrong_type(self):
        with self.assertRaises(TypeError):
            self._def[18] = 23

    def test_iter(self):
        for vkey, vval in self._def.items():
            val = self._def_value[vkey]
            self.assertEqual(vval, val)

    def test_const_iter(self):
        for vkey, vval in self._def_const.items():
            val = self._def_value[vkey]
            self.assertEqual(vval, val)

    def test_get_item(self):
        i = self._def['pos-float']
        self.assertIs(type(i), bt2.RealValue)
        self.assertEqual(i, 23.17)

    def test_const_get_item(self):
        item1 = self._def_const['none']
        item2 = self._def_const['true']
        item3 = self._def_const['pos-int']
        item4 = self._def_const['pos-float']
        item5 = self._def_const['str']

        self.assertEqual(item1, None)

        self.assertIs(type(item2), bt2._BoolValueConst)
        self.assertEqual(item2, True)

        self.assertIs(type(item3), bt2._SignedIntegerValueConst)
        self.assertEqual(item3, 42)

        self.assertIs(type(item4), bt2._RealValueConst)
        self.assertEqual(item4, 23.17)

        self.assertIs(type(item5), bt2._StringValueConst)
        self.assertEqual(item5, 'yes')

    def test_getitem_wrong_key(self):
        with self.assertRaises(KeyError):
            self._def['kilojoule']


if __name__ == '__main__':
    unittest.main()
