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
import unittest
import math
import copy
import itertools
import bt2
from utils import get_default_trace_class


_COMP_BINOPS = (
    operator.eq,
    operator.ne,
)


# Create and return a stream with the field classes part of its stream packet
# context.
#
# The stream is part of a dummy trace created from trace class `tc`.

def _create_stream(tc, ctx_field_classes):
    packet_context_fc = tc.create_structure_field_class()
    for name, fc in ctx_field_classes:
        packet_context_fc.append_member(name, fc)

    trace = tc()
    stream_class = tc.create_stream_class(packet_context_field_class=packet_context_fc)

    stream = trace.create_stream(stream_class)
    return stream


# Create a field of the given field class.
#
# The field is part of a dummy stream, itself part of a dummy trace created
# from trace class `tc`.

def _create_field(tc, field_class):
    field_name = 'field'
    stream = _create_stream(tc, [(field_name, field_class)])
    packet = stream.create_packet()
    return packet.context_field[field_name]


# Create a field of type string.
#
# The field is part of a dummy stream, itself part of a dummy trace created
# from trace class `tc`.  It is made out of a dummy string field class.

def _create_string_field(tc):
    field_name = 'string_field'
    stream = _create_stream(tc, [(field_name, tc.create_string_field_class())])
    packet = stream.create_packet()
    return packet.context_field[field_name]


# Create a field of type static array of ints.
#
# The field is part of a dummy stream, itself part of a dummy trace created
# from trace class `tc`.  It is made out of a dummy static array field class,
# with a dummy integer field class as element class.

def _create_int_array_field(tc, length):
    elem_fc = tc.create_signed_integer_field_class(32)
    fc = tc.create_static_array_field_class(elem_fc, length)
    field_name = 'int_array'
    stream = _create_stream(tc, [(field_name, fc)])
    packet = stream.create_packet()
    return packet.context_field[field_name]


# Create a field of type dynamic array of ints.
#
# The field is part of a dummy stream, itself part of a dummy trace created
# from trace class `tc`.  It is made out of a dummy static array field class,
# with a dummy integer field class as element and length classes.

def _create_dynamic_array(tc):
    elem_fc = tc.create_signed_integer_field_class(32)
    len_fc = tc.create_signed_integer_field_class(32)
    fc = tc.create_dynamic_array_field_class(elem_fc)
    field_name = 'int_dyn_array'
    stream = _create_stream(tc, [('thelength', len_fc), (field_name, fc)])
    packet = stream.create_packet()
    packet.context_field[field_name].length = 3
    return packet.context_field[field_name]


# Create a field of type array of (empty) structures.
#
# The field is part of a dummy stream, itself part of a dummy trace created
# from trace class `tc`.  It is made out of a dummy static array field class,
# with a dummy struct field class as element class.

def _create_struct_array_field(tc, length):
    elem_fc = tc.create_structure_field_class()
    fc = tc.create_static_array_field_class(elem_fc, length)
    field_name = 'struct_array'
    stream = _create_stream(tc, [(field_name, fc)])
    packet = stream.create_packet()
    return packet.context_field[field_name]


class _TestNumericField:
    def _binop(self, op, rhs):
        rexc = None
        rvexc = None
        comp_value = rhs

        try:
            r = op(self._def, rhs)
        except Exception as e:
            rexc = e

        try:
            rv = op(self._def_value, comp_value)
        except Exception as e:
            rvexc = e

        if rexc is not None or rvexc is not None:
            # at least one of the operations raised an exception: in
            # this case both operations should have raised the same
            # type of exception (division by zero, bit shift with a
            # floating point number operand, etc.)
            self.assertIs(type(rexc), type(rvexc))
            return None, None

        return r, rv

    def _unaryop(self, op):
        rexc = None
        rvexc = None

        try:
            r = op(self._def)
        except Exception as e:
            rexc = e

        try:
            rv = op(self._def_value)
        except Exception as e:
            rvexc = e

        if rexc is not None or rvexc is not None:
            # at least one of the operations raised an exception: in
            # this case both operations should have raised the same
            # type of exception (division by zero, bit shift with a
            # floating point number operand, etc.)
            self.assertIs(type(rexc), type(rvexc))
            return None, None

        return r, rv

    def _test_unaryop_type(self, op):
        r, rv = self._unaryop(op)

        if r is None:
            return

        self.assertIsInstance(r, type(rv))

    def _test_unaryop_value(self, op):
        r, rv = self._unaryop(op)

        if r is None:
            return

        self.assertEqual(r, rv)

    def _test_unaryop_addr_same(self, op):
        addr_before = self._def.addr
        self._unaryop(op)
        self.assertEqual(self._def.addr, addr_before)

    def _test_unaryop_value_same(self, op):
        value_before = copy.copy(self._def_value)
        self._unaryop(op)
        self.assertEqual(self._def, value_before)

    def _test_binop_type(self, op, rhs):
        r, rv = self._binop(op, rhs)

        if r is None:
            return

        if op in _COMP_BINOPS:
            # __eq__() and __ne__() always return a 'bool' object
            self.assertIsInstance(r, bool)
        else:
            self.assertIsInstance(r, type(rv))

    def _test_binop_value(self, op, rhs):
        r, rv = self._binop(op, rhs)

        if r is None:
            return

        self.assertEqual(r, rv)

    def _test_binop_lhs_addr_same(self, op, rhs):
        addr_before = self._def.addr
        r, rv = self._binop(op, rhs)
        self.assertEqual(self._def.addr, addr_before)

    @unittest.skip('copy is not implemented')
    def _test_binop_lhs_value_same(self, op, rhs):
        value_before = copy.copy(self._def)
        r, rv = self._binop(op, rhs)
        self.assertEqual(self._def, value_before)

    def _test_binop_invalid_unknown(self, op):
        if op in _COMP_BINOPS:
            self.skipTest('not testing')

        class A:
            pass

        with self.assertRaises(TypeError):
            op(self._def, A())

    def _test_binop_invalid_none(self, op):
        if op in _COMP_BINOPS:
            self.skipTest('not testing')

        with self.assertRaises(TypeError):
            op(self._def, None)

    def _test_ibinop_value(self, op, rhs):
        r, rv = self._binop(op, rhs)

        if r is None:
            return

        # The inplace operators are special for field objects because
        # they do not return a new, immutable object like it's the case
        # for Python numbers. In Python, `a += 2`, where `a` is a number
        # object, assigns a new number object reference to `a`, dropping
        # the old reference. Since BT's field objects are mutable, we
        # modify their internal value with the inplace operators. This
        # means however that we can lose data in the process, for
        # example:
        #
        #     int_value_obj += 3.3
        #
        # Here, if `int_value_obj` is a Python `int` with the value 2,
        # it would be a `float` object after this, holding the value
        # 5.3. In our case, if `int_value_obj` is an integer field
        # object, 3.3 is converted to an `int` object (3) and added to
        # the current value of `int_value_obj`, so after this the value
        # of the object is 5. This does not compare to 5.3, which is
        # why we also use the `int()` type here.
        if isinstance(self._def, bt2.field._IntegerField):
            rv = int(rv)

        self.assertEqual(r, rv)

    def _test_ibinop_type(self, op, rhs):
        r, rv = self._binop(op, rhs)

        if r is None:
            return

        self.assertIs(r, self._def)

    def _test_ibinop_invalid_unknown(self, op):
        class A:
            pass

        with self.assertRaises(TypeError):
            op(self._def, A())

    def _test_ibinop_invalid_none(self, op):
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

    def _test_ibinop_type_false(self, op):
        self._test_binop_rhs_false(self._test_ibinop_type, op)

    def _test_ibinop_type_true(self, op):
        self._test_binop_rhs_true(self._test_ibinop_type, op)

    def _test_ibinop_type_pos_int(self, op):
        self._test_binop_rhs_pos_int(self._test_ibinop_type, op)

    def _test_ibinop_type_neg_int(self, op):
        self._test_binop_rhs_neg_int(self._test_ibinop_type, op)

    def _test_ibinop_type_zero_int(self, op):
        self._test_binop_rhs_zero_int(self._test_ibinop_type, op)

    def _test_ibinop_type_pos_vint(self, op):
        self._test_binop_rhs_pos_vint(self._test_ibinop_type, op)

    def _test_ibinop_type_neg_vint(self, op):
        self._test_binop_rhs_neg_vint(self._test_ibinop_type, op)

    def _test_ibinop_type_zero_vint(self, op):
        self._test_binop_rhs_zero_vint(self._test_ibinop_type, op)

    def _test_ibinop_type_pos_float(self, op):
        self._test_binop_rhs_pos_float(self._test_ibinop_type, op)

    def _test_ibinop_type_neg_float(self, op):
        self._test_binop_rhs_neg_float(self._test_ibinop_type, op)

    def _test_ibinop_type_zero_float(self, op):
        self._test_binop_rhs_zero_float(self._test_ibinop_type, op)

    def _test_ibinop_type_pos_vfloat(self, op):
        self._test_binop_rhs_pos_vfloat(self._test_ibinop_type, op)

    def _test_ibinop_type_neg_vfloat(self, op):
        self._test_binop_rhs_neg_vfloat(self._test_ibinop_type, op)

    def _test_ibinop_type_zero_vfloat(self, op):
        self._test_binop_rhs_zero_vfloat(self._test_ibinop_type, op)

    def _test_ibinop_value_false(self, op):
        self._test_binop_rhs_false(self._test_ibinop_value, op)

    def _test_ibinop_value_true(self, op):
        self._test_binop_rhs_true(self._test_ibinop_value, op)

    def _test_ibinop_value_pos_int(self, op):
        self._test_binop_rhs_pos_int(self._test_ibinop_value, op)

    def _test_ibinop_value_neg_int(self, op):
        self._test_binop_rhs_neg_int(self._test_ibinop_value, op)

    def _test_ibinop_value_zero_int(self, op):
        self._test_binop_rhs_zero_int(self._test_ibinop_value, op)

    def _test_ibinop_value_pos_vint(self, op):
        self._test_binop_rhs_pos_vint(self._test_ibinop_value, op)

    def _test_ibinop_value_neg_vint(self, op):
        self._test_binop_rhs_neg_vint(self._test_ibinop_value, op)

    def _test_ibinop_value_zero_vint(self, op):
        self._test_binop_rhs_zero_vint(self._test_ibinop_value, op)

    def _test_ibinop_value_pos_float(self, op):
        self._test_binop_rhs_pos_float(self._test_ibinop_value, op)

    def _test_ibinop_value_neg_float(self, op):
        self._test_binop_rhs_neg_float(self._test_ibinop_value, op)

    def _test_ibinop_value_zero_float(self, op):
        self._test_binop_rhs_zero_float(self._test_ibinop_value, op)

    def _test_ibinop_value_pos_vfloat(self, op):
        self._test_binop_rhs_pos_vfloat(self._test_ibinop_value, op)

    def _test_ibinop_value_neg_vfloat(self, op):
        self._test_binop_rhs_neg_vfloat(self._test_ibinop_value, op)

    def _test_ibinop_value_zero_vfloat(self, op):
        self._test_binop_rhs_zero_vfloat(self._test_ibinop_value, op)

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
        # Ignore this lint error:
        #   E711 comparison to None should be 'if cond is None:'
        # since this is what we want to test (even though not good practice).
        self.assertFalse(self._def == None) # noqa: E711

    def test_ne_none(self):
        # Ignore this lint error:
        #   E711 comparison to None should be 'if cond is not None:'
        # since this is what we want to test (even though not good practice).
        self.assertTrue(self._def != None) # noqa: E711


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


_IBINOPS = (
    ('iadd', operator.iadd),
    ('iand', operator.iand),
    ('ifloordiv', operator.ifloordiv),
    ('ilshift', operator.ilshift),
    ('imod', operator.imod),
    ('imul', operator.imul),
    ('ior', operator.ior),
    ('ipow', operator.ipow),
    ('irshift', operator.irshift),
    ('isub', operator.isub),
    ('itruediv', operator.itruediv),
    ('ixor', operator.ixor),
)


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


def _inject_numeric_testing_methods(cls):
    def test_binop_name(suffix):
        return 'test_binop_{}_{}'.format(name, suffix)

    def test_ibinop_name(suffix):
        return 'test_ibinop_{}_{}'.format(name, suffix)

    def test_unaryop_name(suffix):
        return 'test_unaryop_{}_{}'.format(name, suffix)

    # inject testing methods for each binary operation
    for name, binop in _BINOPS:
        setattr(cls, test_binop_name('invalid_unknown'), partialmethod(_TestNumericField._test_binop_invalid_unknown, op=binop))
        setattr(cls, test_binop_name('invalid_none'), partialmethod(_TestNumericField._test_binop_invalid_none, op=binop))
        setattr(cls, test_binop_name('type_true'), partialmethod(_TestNumericField._test_binop_type_true, op=binop))
        setattr(cls, test_binop_name('type_pos_int'), partialmethod(_TestNumericField._test_binop_type_pos_int, op=binop))
        setattr(cls, test_binop_name('type_pos_vint'), partialmethod(_TestNumericField._test_binop_type_pos_vint, op=binop))
        setattr(cls, test_binop_name('value_true'), partialmethod(_TestNumericField._test_binop_value_true, op=binop))
        setattr(cls, test_binop_name('value_pos_int'), partialmethod(_TestNumericField._test_binop_value_pos_int, op=binop))
        setattr(cls, test_binop_name('value_pos_vint'), partialmethod(_TestNumericField._test_binop_value_pos_vint, op=binop))
        setattr(cls, test_binop_name('lhs_addr_same_true'), partialmethod(_TestNumericField._test_binop_lhs_addr_same_true, op=binop))
        setattr(cls, test_binop_name('lhs_addr_same_pos_int'), partialmethod(_TestNumericField._test_binop_lhs_addr_same_pos_int, op=binop))
        setattr(cls, test_binop_name('lhs_addr_same_pos_vint'), partialmethod(_TestNumericField._test_binop_lhs_addr_same_pos_vint, op=binop))
        setattr(cls, test_binop_name('lhs_value_same_true'), partialmethod(_TestNumericField._test_binop_lhs_value_same_true, op=binop))
        setattr(cls, test_binop_name('lhs_value_same_pos_int'), partialmethod(_TestNumericField._test_binop_lhs_value_same_pos_int, op=binop))
        setattr(cls, test_binop_name('lhs_value_same_pos_vint'), partialmethod(_TestNumericField._test_binop_lhs_value_same_pos_vint, op=binop))
        setattr(cls, test_binop_name('type_neg_int'), partialmethod(_TestNumericField._test_binop_type_neg_int, op=binop))
        setattr(cls, test_binop_name('type_neg_vint'), partialmethod(_TestNumericField._test_binop_type_neg_vint, op=binop))
        setattr(cls, test_binop_name('value_neg_int'), partialmethod(_TestNumericField._test_binop_value_neg_int, op=binop))
        setattr(cls, test_binop_name('value_neg_vint'), partialmethod(_TestNumericField._test_binop_value_neg_vint, op=binop))
        setattr(cls, test_binop_name('lhs_addr_same_neg_int'), partialmethod(_TestNumericField._test_binop_lhs_addr_same_neg_int, op=binop))
        setattr(cls, test_binop_name('lhs_addr_same_neg_vint'), partialmethod(_TestNumericField._test_binop_lhs_addr_same_neg_vint, op=binop))
        setattr(cls, test_binop_name('lhs_value_same_neg_int'), partialmethod(_TestNumericField._test_binop_lhs_value_same_neg_int, op=binop))
        setattr(cls, test_binop_name('lhs_value_same_neg_vint'), partialmethod(_TestNumericField._test_binop_lhs_value_same_neg_vint, op=binop))
        setattr(cls, test_binop_name('type_false'), partialmethod(_TestNumericField._test_binop_type_false, op=binop))
        setattr(cls, test_binop_name('type_zero_int'), partialmethod(_TestNumericField._test_binop_type_zero_int, op=binop))
        setattr(cls, test_binop_name('type_zero_vint'), partialmethod(_TestNumericField._test_binop_type_zero_vint, op=binop))
        setattr(cls, test_binop_name('value_false'), partialmethod(_TestNumericField._test_binop_value_false, op=binop))
        setattr(cls, test_binop_name('value_zero_int'), partialmethod(_TestNumericField._test_binop_value_zero_int, op=binop))
        setattr(cls, test_binop_name('value_zero_vint'), partialmethod(_TestNumericField._test_binop_value_zero_vint, op=binop))
        setattr(cls, test_binop_name('lhs_addr_same_false'), partialmethod(_TestNumericField._test_binop_lhs_addr_same_false, op=binop))
        setattr(cls, test_binop_name('lhs_addr_same_zero_int'), partialmethod(_TestNumericField._test_binop_lhs_addr_same_zero_int, op=binop))
        setattr(cls, test_binop_name('lhs_addr_same_zero_vint'), partialmethod(_TestNumericField._test_binop_lhs_addr_same_zero_vint, op=binop))
        setattr(cls, test_binop_name('lhs_value_same_false'), partialmethod(_TestNumericField._test_binop_lhs_value_same_false, op=binop))
        setattr(cls, test_binop_name('lhs_value_same_zero_int'), partialmethod(_TestNumericField._test_binop_lhs_value_same_zero_int, op=binop))
        setattr(cls, test_binop_name('lhs_value_same_zero_vint'), partialmethod(_TestNumericField._test_binop_lhs_value_same_zero_vint, op=binop))
        setattr(cls, test_binop_name('type_pos_float'), partialmethod(_TestNumericField._test_binop_type_pos_float, op=binop))
        setattr(cls, test_binop_name('type_neg_float'), partialmethod(_TestNumericField._test_binop_type_neg_float, op=binop))
        setattr(cls, test_binop_name('type_pos_vfloat'), partialmethod(_TestNumericField._test_binop_type_pos_vfloat, op=binop))
        setattr(cls, test_binop_name('type_neg_vfloat'), partialmethod(_TestNumericField._test_binop_type_neg_vfloat, op=binop))
        setattr(cls, test_binop_name('value_pos_float'), partialmethod(_TestNumericField._test_binop_value_pos_float, op=binop))
        setattr(cls, test_binop_name('value_neg_float'), partialmethod(_TestNumericField._test_binop_value_neg_float, op=binop))
        setattr(cls, test_binop_name('value_pos_vfloat'), partialmethod(_TestNumericField._test_binop_value_pos_vfloat, op=binop))
        setattr(cls, test_binop_name('value_neg_vfloat'), partialmethod(_TestNumericField._test_binop_value_neg_vfloat, op=binop))
        setattr(cls, test_binop_name('lhs_addr_same_pos_float'), partialmethod(_TestNumericField._test_binop_lhs_addr_same_pos_float, op=binop))
        setattr(cls, test_binop_name('lhs_addr_same_neg_float'), partialmethod(_TestNumericField._test_binop_lhs_addr_same_neg_float, op=binop))
        setattr(cls, test_binop_name('lhs_addr_same_pos_vfloat'), partialmethod(_TestNumericField._test_binop_lhs_addr_same_pos_vfloat, op=binop))
        setattr(cls, test_binop_name('lhs_addr_same_neg_vfloat'), partialmethod(_TestNumericField._test_binop_lhs_addr_same_neg_vfloat, op=binop))
        setattr(cls, test_binop_name('lhs_value_same_pos_float'), partialmethod(_TestNumericField._test_binop_lhs_value_same_pos_float, op=binop))
        setattr(cls, test_binop_name('lhs_value_same_neg_float'), partialmethod(_TestNumericField._test_binop_lhs_value_same_neg_float, op=binop))
        setattr(cls, test_binop_name('lhs_value_same_pos_vfloat'), partialmethod(_TestNumericField._test_binop_lhs_value_same_pos_vfloat, op=binop))
        setattr(cls, test_binop_name('lhs_value_same_neg_vfloat'), partialmethod(_TestNumericField._test_binop_lhs_value_same_neg_vfloat, op=binop))
        setattr(cls, test_binop_name('type_zero_float'), partialmethod(_TestNumericField._test_binop_type_zero_float, op=binop))
        setattr(cls, test_binop_name('type_zero_vfloat'), partialmethod(_TestNumericField._test_binop_type_zero_vfloat, op=binop))
        setattr(cls, test_binop_name('value_zero_float'), partialmethod(_TestNumericField._test_binop_value_zero_float, op=binop))
        setattr(cls, test_binop_name('value_zero_vfloat'), partialmethod(_TestNumericField._test_binop_value_zero_vfloat, op=binop))
        setattr(cls, test_binop_name('lhs_addr_same_zero_float'), partialmethod(_TestNumericField._test_binop_lhs_addr_same_zero_float, op=binop))
        setattr(cls, test_binop_name('lhs_addr_same_zero_vfloat'), partialmethod(_TestNumericField._test_binop_lhs_addr_same_zero_vfloat, op=binop))
        setattr(cls, test_binop_name('lhs_value_same_zero_float'), partialmethod(_TestNumericField._test_binop_lhs_value_same_zero_float, op=binop))
        setattr(cls, test_binop_name('lhs_value_same_zero_vfloat'), partialmethod(_TestNumericField._test_binop_lhs_value_same_zero_vfloat, op=binop))

    # inject testing methods for each unary operation
    for name, unaryop in _UNARYOPS:
        setattr(cls, test_unaryop_name('type'), partialmethod(_TestNumericField._test_unaryop_type, op=unaryop))
        setattr(cls, test_unaryop_name('value'), partialmethod(_TestNumericField._test_unaryop_value, op=unaryop))
        setattr(cls, test_unaryop_name('addr_same'), partialmethod(_TestNumericField._test_unaryop_addr_same, op=unaryop))
        setattr(cls, test_unaryop_name('value_same'), partialmethod(_TestNumericField._test_unaryop_value_same, op=unaryop))

    # inject testing methods for each inplace binary operation
    for name, ibinop in _IBINOPS:
        setattr(cls, test_ibinop_name('invalid_unknown'), partialmethod(_TestNumericField._test_ibinop_invalid_unknown, op=ibinop))
        setattr(cls, test_ibinop_name('invalid_none'), partialmethod(_TestNumericField._test_ibinop_invalid_none, op=ibinop))
        setattr(cls, test_ibinop_name('type_true'), partialmethod(_TestNumericField._test_ibinop_type_true, op=ibinop))
        setattr(cls, test_ibinop_name('value_true'), partialmethod(_TestNumericField._test_ibinop_value_true, op=ibinop))
        setattr(cls, test_ibinop_name('type_pos_int'), partialmethod(_TestNumericField._test_ibinop_type_pos_int, op=ibinop))
        setattr(cls, test_ibinop_name('type_pos_vint'), partialmethod(_TestNumericField._test_ibinop_type_pos_vint, op=ibinop))
        setattr(cls, test_ibinop_name('value_pos_int'), partialmethod(_TestNumericField._test_ibinop_value_pos_int, op=ibinop))
        setattr(cls, test_ibinop_name('value_pos_vint'), partialmethod(_TestNumericField._test_ibinop_value_pos_vint, op=ibinop))
        setattr(cls, test_ibinop_name('type_neg_int'), partialmethod(_TestNumericField._test_ibinop_type_neg_int, op=ibinop))
        setattr(cls, test_ibinop_name('type_neg_vint'), partialmethod(_TestNumericField._test_ibinop_type_neg_vint, op=ibinop))
        setattr(cls, test_ibinop_name('value_neg_int'), partialmethod(_TestNumericField._test_ibinop_value_neg_int, op=ibinop))
        setattr(cls, test_ibinop_name('value_neg_vint'), partialmethod(_TestNumericField._test_ibinop_value_neg_vint, op=ibinop))
        setattr(cls, test_ibinop_name('type_false'), partialmethod(_TestNumericField._test_ibinop_type_false, op=ibinop))
        setattr(cls, test_ibinop_name('value_false'), partialmethod(_TestNumericField._test_ibinop_value_false, op=ibinop))
        setattr(cls, test_ibinop_name('type_zero_int'), partialmethod(_TestNumericField._test_ibinop_type_zero_int, op=ibinop))
        setattr(cls, test_ibinop_name('type_zero_vint'), partialmethod(_TestNumericField._test_ibinop_type_zero_vint, op=ibinop))
        setattr(cls, test_ibinop_name('value_zero_int'), partialmethod(_TestNumericField._test_ibinop_value_zero_int, op=ibinop))
        setattr(cls, test_ibinop_name('value_zero_vint'), partialmethod(_TestNumericField._test_ibinop_value_zero_vint, op=ibinop))
        setattr(cls, test_ibinop_name('type_pos_float'), partialmethod(_TestNumericField._test_ibinop_type_pos_float, op=ibinop))
        setattr(cls, test_ibinop_name('type_neg_float'), partialmethod(_TestNumericField._test_ibinop_type_neg_float, op=ibinop))
        setattr(cls, test_ibinop_name('type_pos_vfloat'), partialmethod(_TestNumericField._test_ibinop_type_pos_vfloat, op=ibinop))
        setattr(cls, test_ibinop_name('type_neg_vfloat'), partialmethod(_TestNumericField._test_ibinop_type_neg_vfloat, op=ibinop))
        setattr(cls, test_ibinop_name('value_pos_float'), partialmethod(_TestNumericField._test_ibinop_value_pos_float, op=ibinop))
        setattr(cls, test_ibinop_name('value_neg_float'), partialmethod(_TestNumericField._test_ibinop_value_neg_float, op=ibinop))
        setattr(cls, test_ibinop_name('value_pos_vfloat'), partialmethod(_TestNumericField._test_ibinop_value_pos_vfloat, op=ibinop))
        setattr(cls, test_ibinop_name('value_neg_vfloat'), partialmethod(_TestNumericField._test_ibinop_value_neg_vfloat, op=ibinop))
        setattr(cls, test_ibinop_name('type_zero_float'), partialmethod(_TestNumericField._test_ibinop_type_zero_float, op=ibinop))
        setattr(cls, test_ibinop_name('type_zero_vfloat'), partialmethod(_TestNumericField._test_ibinop_type_zero_vfloat, op=ibinop))
        setattr(cls, test_ibinop_name('value_zero_float'), partialmethod(_TestNumericField._test_ibinop_value_zero_float, op=ibinop))
        setattr(cls, test_ibinop_name('value_zero_vfloat'), partialmethod(_TestNumericField._test_ibinop_value_zero_vfloat, op=ibinop))


class _TestIntegerFieldCommon(_TestNumericField):
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

    def test_assign_neg_int(self):
        raw = -13
        self._def.value = raw
        self.assertEqual(self._def, raw)

    def test_assign_int_field(self):
        raw = 999
        field = _create_field(self._tc, self._create_fc(self._tc))
        field.value = raw
        self._def.value = field
        self.assertEqual(self._def, raw)

    def test_assign_float(self):
        raw = 123.456
        self._def.value = raw
        self.assertEqual(self._def, int(raw))

    def test_assign_invalid_type(self):
        with self.assertRaises(TypeError):
            self._def.value = 'yes'

    def test_assign_uint(self):
        uint_fc = self._tc.create_unsigned_integer_field_class(32)
        field = _create_field(self._tc, uint_fc)
        raw = 1777
        field.value = 1777
        self.assertEqual(field, raw)

    def test_assign_big_uint(self):
        uint_fc = self._tc.create_unsigned_integer_field_class(64)
        field = _create_field(self._tc, uint_fc)
        # Larger than the IEEE 754 double-precision exact representation of
        # integers.
        raw = (2**53) + 1
        field.value = (2**53) + 1
        self.assertEqual(field, raw)

    def test_assign_uint_invalid_neg(self):
        uint_fc = self._tc.create_unsigned_integer_field_class(32)
        field = _create_field(self._tc, uint_fc)

        with self.assertRaises(ValueError):
            field.value = -23

    def test_str_op(self):
        self.assertEqual(str(self._def), str(self._def_value))


_inject_numeric_testing_methods(_TestIntegerFieldCommon)


class SignedIntegerFieldTestCase(_TestIntegerFieldCommon, unittest.TestCase):
    def _create_fc(self, tc):
        return tc.create_signed_integer_field_class(25)

    def setUp(self):
        self._tc = get_default_trace_class()
        self._field = _create_field(self._tc, self._create_fc(self._tc))
        self._field.value = 17
        self._def = _create_field(self._tc, self._create_fc(self._tc))
        self._def.value = 17
        self._def_value = 17
        self._def_new_value = -101


class SignedEnumerationFieldTestCase(_TestIntegerFieldCommon, unittest.TestCase):
    def _create_fc(self, tc):
        fc = tc.create_signed_enumeration_field_class(32)
        fc.map_range('something', 17)
        fc.map_range('speaker', 12, 16)
        fc.map_range('can', 18, 2540)
        fc.map_range('whole range', -(2 ** 31), (2 ** 31) - 1)
        fc.map_range('zip', -45, 1001)
        return fc

    def setUp(self):
        self._tc = get_default_trace_class()
        self._field = _create_field(self._tc, self._create_fc(self._tc))
        self._def = _create_field(self._tc, self._create_fc(self._tc))
        self._def.value = 17
        self._def_value = 17
        self._def_new_value = -101

    def test_str_op(self):
        expected_string_found = False
        s = str(self._def)

        # Establish all permutations of the three expected matches since
        # the order in which mappings are enumerated is not explicitly part of
        # the API.
        for p in itertools.permutations(['whole range', 'something',
                                         'zip']):
            candidate = '{} ({})'.format(self._def_value, ', '.join(p))
            if candidate == s:
                expected_string_found = True
                break

        self.assertTrue(expected_string_found)

    def test_labels(self):
        self._field.value = 17
        labels = sorted(self._field.labels)
        self.assertEqual(labels, ['something', 'whole range', 'zip'])


class RealFieldTestCase(_TestNumericField, unittest.TestCase):
    def _create_fc(self, tc):
        return tc.create_real_field_class()

    def setUp(self):
        self._tc = get_default_trace_class()
        self._field = _create_field(self._tc, self._create_fc(self._tc))
        self._def = _create_field(self._tc, self._create_fc(self._tc))
        self._def.value = 52.7
        self._def_value = 52.7
        self._def_new_value = -17.164857

    def _test_invalid_op(self, cb):
        with self.assertRaises(TypeError):
            cb()

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

    def test_assign_int_field(self):
        int_fc = self._tc.create_signed_integer_field_class(32)
        int_field = _create_field(self._tc, int_fc)
        raw = 999
        int_field.value = raw
        self._def.value = int_field
        self.assertEqual(self._def, float(raw))

    def test_assign_float(self):
        raw = -19.23
        self._def.value = raw
        self.assertEqual(self._def, raw)

    def test_assign_float_field(self):
        field = _create_field(self._tc, self._create_fc(self._tc))
        raw = 101.32
        field.value = raw
        self._def.value = field
        self.assertEqual(self._def, raw)

    def test_assign_invalid_type(self):
        with self.assertRaises(TypeError):
            self._def.value = 'yes'

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

    def test_str_op(self):
        self.assertEqual(str(self._def), str(self._def_value))


_inject_numeric_testing_methods(RealFieldTestCase)


class StringFieldTestCase(unittest.TestCase):
    def setUp(self):
        self._tc = get_default_trace_class()
        self._def_value = 'Hello, World!'
        self._def = _create_string_field(self._tc)
        self._def.value = self._def_value
        self._def_new_value = 'Yes!'

    def test_assign_int(self):
        with self.assertRaises(TypeError):
            self._def.value = 283

    def test_assign_string_field(self):
        field = _create_string_field(self._tc)
        raw = 'zorg'
        field.value = raw
        self.assertEqual(field, raw)

    def test_eq(self):
        self.assertEqual(self._def, self._def_value)

    def test_not_eq(self):
        self.assertNotEqual(self._def, 23)

    def test_lt_vstring(self):
        s1 = _create_string_field(self._tc)
        s1.value = 'allo'
        s2 = _create_string_field(self._tc)
        s2.value = 'bateau'
        self.assertLess(s1, s2)

    def test_lt_string(self):
        s1 = _create_string_field(self._tc)
        s1.value = 'allo'
        self.assertLess(s1, 'bateau')

    def test_le_vstring(self):
        s1 = _create_string_field(self._tc)
        s1.value = 'allo'
        s2 = _create_string_field(self._tc)
        s2.value = 'bateau'
        self.assertLessEqual(s1, s2)

    def test_le_string(self):
        s1 = _create_string_field(self._tc)
        s1.value = 'allo'
        self.assertLessEqual(s1, 'bateau')

    def test_gt_vstring(self):
        s1 = _create_string_field(self._tc)
        s1.value = 'allo'
        s2 = _create_string_field(self._tc)
        s2.value = 'bateau'
        self.assertGreater(s2, s1)

    def test_gt_string(self):
        s1 = _create_string_field(self._tc)
        s1.value = 'allo'
        self.assertGreater('bateau', s1)

    def test_ge_vstring(self):
        s1 = _create_string_field(self._tc)
        s1.value = 'allo'
        s2 = _create_string_field(self._tc)
        s2.value = 'bateau'
        self.assertGreaterEqual(s2, s1)

    def test_ge_string(self):
        s1 = _create_string_field(self._tc)
        s1.value = 'allo'
        self.assertGreaterEqual('bateau', s1)

    def test_bool_op(self):
        self.assertEqual(bool(self._def), bool(self._def_value))

    def test_str_op(self):
        self.assertEqual(str(self._def), str(self._def_value))

    def test_len(self):
        self.assertEqual(len(self._def), len(self._def_value))

    def test_getitem(self):
        self.assertEqual(self._def[5], self._def_value[5])

    def test_append_str(self):
        to_append = 'meow meow meow'
        self._def += to_append
        self._def_value += to_append
        self.assertEqual(self._def, self._def_value)

    def test_append_string_field(self):
        field = _create_string_field(self._tc)
        to_append = 'meow meow meow'
        field.value = to_append
        self._def += field
        self._def_value += to_append
        self.assertEqual(self._def, self._def_value)


class _TestArrayFieldCommon:
    def _modify_def(self):
        self._def[2] = 23

    def test_bool_op_true(self):
        self.assertTrue(self._def)

    def test_len(self):
        self.assertEqual(len(self._def), 3)

    def test_length(self):
        self.assertEqual(self._def.length, 3)

    def test_getitem(self):
        field = self._def[1]
        self.assertIs(type(field), bt2.field._SignedIntegerField)
        self.assertEqual(field, 1847)

    def test_eq(self):
        field = _create_int_array_field(self._tc, 3)
        field[0] = 45
        field[1] = 1847
        field[2] = 1948754
        self.assertEqual(self._def, field)

    def test_eq_invalid_type(self):
        self.assertNotEqual(self._def, 23)

    def test_eq_diff_len(self):
        field = _create_int_array_field(self._tc, 2)
        field[0] = 45
        field[1] = 1847
        self.assertNotEqual(self._def, field)

    def test_eq_diff_content_same_len(self):
        field = _create_int_array_field(self._tc, 3)
        field[0] = 45
        field[1] = 1846
        field[2] = 1948754
        self.assertNotEqual(self._def, field)

    def test_setitem(self):
        self._def[2] = 24
        self.assertEqual(self._def[2], 24)

    def test_setitem_int_field(self):
        int_fc = self._tc.create_signed_integer_field_class(32)
        int_field = _create_field(self._tc, int_fc)
        int_field.value = 19487
        self._def[1] = int_field
        self.assertEqual(self._def[1], 19487)

    def test_setitem_non_basic_field(self):
        array_field = _create_struct_array_field(self._tc, 2)
        with self.assertRaises(TypeError):
            array_field[1] = 23

    def test_setitem_none(self):
        with self.assertRaises(TypeError):
            self._def[1] = None

    def test_setitem_index_wrong_type(self):
        with self.assertRaises(TypeError):
            self._def['yes'] = 23

    def test_setitem_index_neg(self):
        with self.assertRaises(IndexError):
            self._def[-2] = 23

    def test_setitem_index_out_of_range(self):
        with self.assertRaises(IndexError):
            self._def[len(self._def)] = 134679

    def test_iter(self):
        for field, value in zip(self._def, (45, 1847, 1948754)):
            self.assertEqual(field, value)

    def test_value_int_field(self):
        values = [45646, 145, 12145]
        self._def.value = values
        self.assertEqual(values, self._def)

    def test_value_check_sequence(self):
        values = 42
        with self.assertRaises(TypeError):
            self._def.value = values

    def test_value_wrong_type_in_sequence(self):
        values = [32, 'hello', 11]
        with self.assertRaises(TypeError):
            self._def.value = values

    def test_value_complex_type(self):
        struct_fc = self._tc.create_structure_field_class()
        int_fc = self._tc.create_signed_integer_field_class(32)
        another_int_fc = self._tc.create_signed_integer_field_class(32)
        str_fc = self._tc.create_string_field_class()
        struct_fc.append_member(field_class=int_fc, name='an_int')
        struct_fc.append_member(field_class=str_fc, name='a_string')
        struct_fc.append_member(field_class=another_int_fc, name='another_int')
        array_fc = self._tc.create_static_array_field_class(struct_fc, 3)
        stream = _create_stream(self._tc, [('array_field', array_fc)])
        values = [
            {
                'an_int': 42,
                'a_string': 'hello',
                'another_int': 66
            },
            {
                'an_int': 1,
                'a_string': 'goodbye',
                'another_int': 488
            },
            {
                'an_int': 156,
                'a_string': 'or not',
                'another_int': 4648
            },
        ]

        array = stream.create_packet().context_field['array_field']
        array.value = values
        self.assertEqual(values, array)
        values[0]['an_int'] = 'a string'
        with self.assertRaises(TypeError):
            array.value = values

    def test_str_op(self):
        s = str(self._def)
        expected_string = '[{}]'.format(', '.join(
            [repr(v) for v in self._def_value]))
        self.assertEqual(expected_string, s)


class StaticArrayFieldTestCase(_TestArrayFieldCommon, unittest.TestCase):
    def setUp(self):
        self._tc = get_default_trace_class()
        self._def = _create_int_array_field(self._tc, 3)
        self._def[0] = 45
        self._def[1] = 1847
        self._def[2] = 1948754
        self._def_value = [45, 1847, 1948754]

    def test_value_wrong_len(self):
        values = [45, 1847]
        with self.assertRaises(ValueError):
            self._def.value = values


class DynamicArrayFieldTestCase(_TestArrayFieldCommon, unittest.TestCase):
    def setUp(self):
        self._tc = get_default_trace_class()
        self._def = _create_dynamic_array(self._tc)
        self._def[0] = 45
        self._def[1] = 1847
        self._def[2] = 1948754
        self._def_value = [45, 1847, 1948754]

    def test_value_resize(self):
        new_values = [1, 2, 3, 4]
        self._def.value = new_values
        self.assertCountEqual(self._def, new_values)

    def test_set_length(self):
        self._def.length = 4
        self._def[3] = 0
        self.assertEqual(len(self._def), 4)

    def test_set_invalid_length(self):
        with self.assertRaises(TypeError):
            self._def.length = 'cheval'


class StructureFieldTestCase(unittest.TestCase):
    def _create_fc(self, tc):
        fc = tc.create_structure_field_class()
        fc.append_member('A', self._fc0_fn())
        fc.append_member('B', self._fc1_fn())
        fc.append_member('C', self._fc2_fn())
        fc.append_member('D', self._fc3_fn())
        fc.append_member('E', self._fc4_fn())
        fc5 = self._fc5_fn()
        fc5.append_member('F_1', self._fc5_inner_fn())
        fc.append_member('F', fc5)
        return fc

    def setUp(self):
        self._tc = get_default_trace_class()
        self._fc0_fn = self._tc.create_signed_integer_field_class
        self._fc1_fn = self._tc.create_string_field_class
        self._fc2_fn = self._tc.create_real_field_class
        self._fc3_fn = self._tc.create_signed_integer_field_class
        self._fc4_fn = self._tc.create_structure_field_class
        self._fc5_fn = self._tc.create_structure_field_class
        self._fc5_inner_fn = self._tc.create_signed_integer_field_class

        self._fc = self._create_fc(self._tc)
        self._def = _create_field(self._tc, self._fc)
        self._def['A'] = -1872
        self._def['B'] = 'salut'
        self._def['C'] = 17.5
        self._def['D'] = 16497
        self._def['E'] = {}
        self._def['F'] = {'F_1': 52}
        self._def_value = {
            'A': -1872,
            'B': 'salut',
            'C': 17.5,
            'D': 16497,
            'E': {},
            'F': {'F_1': 52}
        }

    def _modify_def(self):
        self._def['B'] = 'hola'

    def test_bool_op_true(self):
        self.assertTrue(self._def)

    def test_bool_op_false(self):
        field = self._def['E']
        self.assertFalse(field)

    def test_len(self):
        self.assertEqual(len(self._def), len(self._def_value))

    def test_getitem(self):
        field = self._def['A']
        self.assertIs(type(field), bt2.field._SignedIntegerField)
        self.assertEqual(field, -1872)

    def test_member_at_index_out_of_bounds_after(self):
        with self.assertRaises(IndexError):
            self._def.member_at_index(len(self._def_value))

    def test_eq(self):
        field = _create_field(self._tc, self._create_fc(self._tc, ))
        field['A'] = -1872
        field['B'] = 'salut'
        field['C'] = 17.5
        field['D'] = 16497
        field['E'] = {}
        field['F'] = {'F_1': 52}
        self.assertEqual(self._def, field)

    def test_eq_invalid_type(self):
        self.assertNotEqual(self._def, 23)

    def test_eq_diff_len(self):
        fc = self._tc.create_structure_field_class()
        fc.append_member('A', self._fc0_fn())
        fc.append_member('B', self._fc1_fn())
        fc.append_member('C', self._fc2_fn())

        field = _create_field(self._tc, fc)
        field['A'] = -1872
        field['B'] = 'salut'
        field['C'] = 17.5
        self.assertNotEqual(self._def, field)

    def test_eq_diff_keys(self):
        fc = self._tc.create_structure_field_class()
        fc.append_member('U', self._fc0_fn())
        fc.append_member('V', self._fc1_fn())
        fc.append_member('W', self._fc2_fn())
        fc.append_member('X', self._fc3_fn())
        fc.append_member('Y', self._fc4_fn())
        fc.append_member('Z', self._fc5_fn())
        field = _create_field(self._tc, fc)
        field['U'] = -1871
        field['V'] = "gerry"
        field['W'] = 18.19
        field['X'] = 16497
        field['Y'] = {}
        field['Z'] = {}
        self.assertNotEqual(self._def, field)

    def test_eq_diff_content_same_len(self):
        field = _create_field(self._tc, self._create_fc(self._tc))
        field['A'] = -1872
        field['B'] = 'salut'
        field['C'] = 17.4
        field['D'] = 16497
        field['E'] = {}
        field['F'] = {'F_1': 0}
        self.assertNotEqual(self._def, field)

    def test_eq_same_content_diff_keys(self):
        fc = self._tc.create_structure_field_class()
        fc.append_member('A', self._fc0_fn())
        fc.append_member('B', self._fc1_fn())
        fc.append_member('E', self._fc2_fn())
        fc.append_member('D', self._fc3_fn())
        fc.append_member('C', self._fc4_fn())
        fc.append_member('F', self._fc5_fn())
        field = _create_field(self._tc, fc)
        field['A'] = -1872
        field['B'] = 'salut'
        field['E'] = 17.5
        field['D'] = 16497
        field['C'] = {}
        field['F'] = {}
        self.assertNotEqual(self._def, field)

    def test_setitem(self):
        self._def['C'] = -18.47
        self.assertEqual(self._def['C'], -18.47)

    def test_setitem_int_field(self):
        int_fc = self._tc.create_signed_integer_field_class(32)
        int_field = _create_field(self._tc, int_fc)
        int_field.value = 19487
        self._def['D'] = int_field
        self.assertEqual(self._def['D'], 19487)

    def test_setitem_non_basic_field(self):
        elem_fc = self._tc.create_structure_field_class()
        struct_fc = self._tc.create_structure_field_class()
        struct_fc.append_member('A', elem_fc)
        struct_field = _create_field(self._tc, struct_fc)

        # Will fail on access to .items() of the value
        with self.assertRaises(AttributeError):
            struct_field['A'] = 23

    def test_setitem_none(self):
        with self.assertRaises(TypeError):
            self._def['C'] = None

    def test_setitem_key_wrong_type(self):
        with self.assertRaises(TypeError):
            self._def[3] = 23

    def test_setitem_wrong_key(self):
        with self.assertRaises(KeyError):
            self._def['hi'] = 134679

    def test_member_at_index(self):
        self.assertEqual(self._def.member_at_index(1), 'salut')

    def test_iter(self):
        orig_values = {
            'A': -1872,
            'B': 'salut',
            'C': 17.5,
            'D': 16497,
            'E': {},
            'F': {'F_1': 52}
        }

        for vkey, vval in self._def.items():
            val = orig_values[vkey]
            self.assertEqual(vval, val)

    def test_value(self):
        orig_values = {
            'A': -1872,
            'B': 'salut',
            'C': 17.5,
            'D': 16497,
            'E': {},
            'F': {'F_1': 52}
        }
        self.assertEqual(self._def, orig_values)

    def test_set_value(self):
        int_fc = self._tc.create_signed_integer_field_class(32)
        another_int_fc = self._tc.create_signed_integer_field_class(32)
        str_fc = self._tc.create_string_field_class()
        struct_fc = self._tc.create_structure_field_class()
        struct_fc.append_member(field_class=int_fc, name='an_int')
        struct_fc.append_member(field_class=str_fc, name='a_string')
        struct_fc.append_member(field_class=another_int_fc, name='another_int')
        values = {
            'an_int': 42,
            'a_string': 'hello',
            'another_int': 66
        }

        struct = _create_field(self._tc, struct_fc)
        struct.value = values
        self.assertEqual(values, struct)

        bad_type_values = copy.deepcopy(values)
        bad_type_values['an_int'] = 'a string'
        with self.assertRaises(TypeError):
            struct.value = bad_type_values

        unknown_key_values = copy.deepcopy(values)
        unknown_key_values['unknown_key'] = 16546
        with self.assertRaises(KeyError):
            struct.value = unknown_key_values

    def test_str_op(self):
        expected_string_found = False
        s = str(self._def)
        # Establish all permutations of the three expected matches since
        # the order in which mappings are enumerated is not explicitly part of
        # the API.
        for p in itertools.permutations([(k, v) for k, v in self._def.items()]):
            items = ['{}: {}'.format(repr(k), repr(v)) for k, v in p]
            candidate = '{{{}}}'.format(', '.join(items))
            if candidate == s:
                expected_string_found = True
                break

        self.assertTrue(expected_string_found)


class VariantFieldTestCase(unittest.TestCase):
    def _create_fc(self, tc):
        selector_fc = tc.create_signed_enumeration_field_class(field_value_range=32)
        selector_fc.map_range('corner', 23)
        selector_fc.map_range('zoom', 17, 20)
        selector_fc.map_range('mellotron', 1001)
        selector_fc.map_range('giorgio', 2000, 3000)

        ft0 = tc.create_signed_integer_field_class(32)
        ft1 = tc.create_string_field_class()
        ft2 = tc.create_real_field_class()
        ft3 = tc.create_signed_integer_field_class(17)

        fc = tc.create_variant_field_class()
        fc.append_option('corner', ft0)
        fc.append_option('zoom', ft1)
        fc.append_option('mellotron', ft2)
        fc.append_option('giorgio', ft3)
        fc.selector_field_class = selector_fc

        top_fc = tc.create_structure_field_class()
        top_fc.append_member('selector_field', selector_fc)
        top_fc.append_member('variant_field', fc)
        return top_fc

    def setUp(self):
        self._tc = get_default_trace_class()
        fld = _create_field(self._tc, self._create_fc(self._tc))
        self._def = fld['variant_field']

    def test_bool_op(self):
        self._def.selected_option_index = 2
        self._def.value = -17.34
        with self.assertRaises(NotImplementedError):
            bool(self._def)

    def test_selected_option_index(self):
        self._def.selected_option_index = 2
        self.assertEqual(self._def.selected_option_index, 2)

    def test_selected_option(self):
        self._def.selected_option_index = 2
        self._def.value = -17.34
        self.assertEqual(self._def.selected_option, -17.34)

        self._def.selected_option_index = 3
        self._def.value = 1921
        self.assertEqual(self._def.selected_option, 1921)

    def test_eq(self):
        field = _create_field(self._tc, self._create_fc(self._tc))
        field = field['variant_field']
        field.selected_option_index = 0
        field.value = 1774
        self._def.selected_option_index = 0
        self._def.value = 1774
        self.assertEqual(self._def, field)

    def test_eq_invalid_type(self):
        self._def.selected_option_index = 1
        self._def.value = 'gerry'
        self.assertNotEqual(self._def, 23)

    def test_str_op_int(self):
        field = _create_field(self._tc, self._create_fc(self._tc))
        field = field['variant_field']
        field.selected_option_index = 0
        field.value = 1774
        other_field = _create_field(self._tc, self._create_fc(self._tc))
        other_field = other_field['variant_field']
        other_field.selected_option_index = 0
        other_field.value = 1774
        self.assertEqual(str(field), str(other_field))

    def test_str_op_str(self):
        field = _create_field(self._tc, self._create_fc(self._tc))
        field = field['variant_field']
        field.selected_option_index = 1
        field.value = 'un beau grand bateau'
        other_field = _create_field(self._tc, self._create_fc(self._tc))
        other_field = other_field['variant_field']
        other_field.selected_option_index = 1
        other_field.value = 'un beau grand bateau'
        self.assertEqual(str(field), str(other_field))

    def test_str_op_float(self):
        field = _create_field(self._tc, self._create_fc(self._tc))
        field = field['variant_field']
        field.selected_option_index = 2
        field.value = 14.4245
        other_field = _create_field(self._tc, self._create_fc(self._tc))
        other_field = other_field['variant_field']
        other_field.selected_option_index = 2
        other_field.value = 14.4245
        self.assertEqual(str(field), str(other_field))
