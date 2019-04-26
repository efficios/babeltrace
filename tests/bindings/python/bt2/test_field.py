from functools import partial, partialmethod
import operator
import unittest
import numbers
import math
import copy
import itertools
import bt2


class _TestCopySimple:
    def test_copy(self):
        cpy = copy.copy(self._def)
        self.assertIsNot(cpy, self._def)
        self.assertNotEqual(cpy.addr, self._def.addr)
        self.assertEqual(cpy, self._def)

    def test_deepcopy(self):
        cpy = copy.deepcopy(self._def)
        self.assertIsNot(cpy, self._def)
        self.assertNotEqual(cpy.addr, self._def.addr)
        self.assertEqual(cpy, self._def)


_COMP_BINOPS = (
    operator.eq,
    operator.ne,
)


class _TestNumericField(_TestCopySimple):
    def _binop(self, op, rhs):
        rexc = None
        rvexc = None
        comp_value = rhs

        if isinstance(rhs, (bt2.field._IntegerField, bt2.field._FloatingPointNumberField)):
            comp_value = copy.copy(rhs)

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
        self.assertFalse(self._def == None)

    def test_ne_none(self):
        self.assertTrue(self._def != None)

    def test_is_set(self):
        raw = self._def_value
        field = self._fc()
        self.assertFalse(field.is_set)
        field.value = raw
        self.assertTrue(field.is_set)

    def test_reset(self):
        raw = self._def_value
        field = self._fc()
        field.value = raw
        self.assertTrue(field.is_set)
        field.reset()
        self.assertFalse(field.is_set)
        other = self._fc()
        self.assertEqual(other, field)


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
        field = self._fc()
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
        fc = bt2.IntegerFieldClass(size=32, is_signed=False)
        field = fc()
        raw = 1777
        field.value = 1777
        self.assertEqual(field, raw)

    def test_assign_uint_invalid_neg(self):
        fc = bt2.IntegerFieldClass(size=32, is_signed=False)
        field = fc()

        with self.assertRaises(ValueError):
            field.value = -23

    def test_str_op(self):
        self.assertEqual(str(self._def), str(self._def_value))

    def test_str_op_unset(self):
        self.assertEqual(str(self._fc()), 'Unset')


_inject_numeric_testing_methods(_TestIntegerFieldCommon)


@unittest.skip("this is broken")
class IntegerFieldTestCase(_TestIntegerFieldCommon, unittest.TestCase):
    def setUp(self):
        self._fc = bt2.IntegerFieldClass(25, is_signed=True)
        self._field = self._fc()
        self._def = self._fc()
        self._def.value = 17
        self._def_value = 17
        self._def_new_value = -101

    def tearDown(self):
        del self._fc
        del self._field
        del self._def


@unittest.skip("this is broken")
class EnumerationFieldTestCase(_TestIntegerFieldCommon, unittest.TestCase):
    def setUp(self):
        self._fc = bt2.EnumerationFieldClass(size=32, is_signed=True)
        self._fc.add_mapping('whole range', -(2 ** 31), (2 ** 31) - 1)
        self._fc.add_mapping('something', 17)
        self._fc.add_mapping('speaker', 12, 16)
        self._fc.add_mapping('can', 18, 2540)
        self._fc.add_mapping('zip', -45, 1001)
        self._def = self._fc()
        self._def.value = 17
        self._def_value = 17
        self._def_new_value = -101

    def tearDown(self):
        del self._fc
        del self._def

    def test_mappings(self):
        mappings = (
            ('whole range', -(2 ** 31), (2 ** 31) - 1),
            ('something', 17, 17),
            ('zip', -45, 1001),
        )

        total = 0
        index_set = set()

        for fm in self._def.mappings:
            total += 1
            for index, mapping in enumerate(mappings):
                if fm.name == mapping[0] and fm.lower == mapping[1] and fm.upper == mapping[2]:
                    index_set.add(index)

        self.assertEqual(total, 3)
        self.assertTrue(0 in index_set and 1 in index_set and 2 in index_set)

    def test_str_op(self):
        expected_string_found = False
        s = str(self._def)

        # Establish all permutations of the three expected matches since
        # the order in which mappings are enumerated is not explicitly part of
        # the API.
        for p in itertools.permutations(["'whole range'", "'something'",
                                         "'zip'"]):
            candidate = '{} ({})'.format(self._def_value, ', '.join(p))
            if candidate == s:
                expected_string_found = True
                break

        self.assertTrue(expected_string_found)

    def test_str_op_unset(self):
        self.assertEqual(str(self._fc()), 'Unset')


@unittest.skip("this is broken")
class FloatingPointNumberFieldTestCase(_TestNumericField, unittest.TestCase):
    def setUp(self):
        self._fc = bt2.FloatingPointNumberFieldClass()
        self._field = self._fc()
        self._def = self._fc()
        self._def.value = 52.7
        self._def_value = 52.7
        self._def_new_value = -17.164857

    def tearDown(self):
        del self._fc
        del self._field
        del self._def

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
        fc = bt2.IntegerFieldClass(32)
        field = fc()
        raw = 999
        field.value = raw
        self._def.value = field
        self.assertEqual(self._def, float(raw))

    def test_assign_float(self):
        raw = -19.23
        self._def.value = raw
        self.assertEqual(self._def, raw)

    def test_assign_float_field(self):
        fc = bt2.FloatingPointNumberFieldClass(32)
        field = fc()
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

    def test_str_op_unset(self):
        self.assertEqual(str(self._fc()), 'Unset')

_inject_numeric_testing_methods(FloatingPointNumberFieldTestCase)


@unittest.skip("this is broken")
class StringFieldTestCase(_TestCopySimple, unittest.TestCase):
    def setUp(self):
        self._fc = bt2.StringFieldClass()
        self._def_value = 'Hello, World!'
        self._def = self._fc()
        self._def.value = self._def_value
        self._def_new_value = 'Yes!'

    def tearDown(self):
        del self._fc
        del self._def

    def test_assign_int(self):
        with self.assertRaises(TypeError):
            self._def.value = 283

    def test_assign_string_field(self):
        fc = bt2.StringFieldClass()
        field = fc()
        raw = 'zorg'
        field.value = raw
        self.assertEqual(field, raw)

    def test_eq(self):
        self.assertEqual(self._def, self._def_value)

    def test_eq(self):
        self.assertNotEqual(self._def, 23)

    def test_lt_vstring(self):
        s1 = self._fc()
        s1.value = 'allo'
        s2 = self._fc()
        s2.value = 'bateau'
        self.assertLess(s1, s2)

    def test_lt_string(self):
        s1 = self._fc()
        s1.value = 'allo'
        self.assertLess(s1, 'bateau')

    def test_le_vstring(self):
        s1 = self._fc()
        s1.value = 'allo'
        s2 = self._fc()
        s2.value = 'bateau'
        self.assertLessEqual(s1, s2)

    def test_le_string(self):
        s1 = self._fc()
        s1.value = 'allo'
        self.assertLessEqual(s1, 'bateau')

    def test_gt_vstring(self):
        s1 = self._fc()
        s1.value = 'allo'
        s2 = self._fc()
        s2.value = 'bateau'
        self.assertGreater(s2, s1)

    def test_gt_string(self):
        s1 = self._fc()
        s1.value = 'allo'
        self.assertGreater('bateau', s1)

    def test_ge_vstring(self):
        s1 = self._fc()
        s1.value = 'allo'
        s2 = self._fc()
        s2.value = 'bateau'
        self.assertGreaterEqual(s2, s1)

    def test_ge_string(self):
        s1 = self._fc()
        s1.value = 'allo'
        self.assertGreaterEqual('bateau', s1)

    def test_bool_op(self):
        self.assertEqual(bool(self._def), bool(self._def_value))

    def test_str_op(self):
        self.assertEqual(str(self._def), str(self._def_value))

    def test_str_op_unset(self):
        self.assertEqual(str(self._fc()), 'Unset')

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
        fc = bt2.StringFieldClass()
        field = fc()
        to_append = 'meow meow meow'
        field.value = to_append
        self._def += field
        self._def_value += to_append
        self.assertEqual(self._def, self._def_value)

    def test_is_set(self):
        raw = self._def_value
        field = self._fc()
        self.assertFalse(field.is_set)
        field.value = raw
        self.assertTrue(field.is_set)

    def test_reset(self):
        raw = self._def_value
        field = self._fc()
        field.value = raw
        self.assertTrue(field.is_set)
        field.reset()
        self.assertFalse(field.is_set)
        other = self._fc()
        self.assertEqual(other, field)


class _TestArraySequenceFieldCommon(_TestCopySimple):
    def _modify_def(self):
        self._def[2] = 23

    def test_bool_op_true(self):
        self.assertTrue(self._def)

    def test_len(self):
        self.assertEqual(len(self._def), 3)

    def test_getitem(self):
        field = self._def[1]
        self.assertIs(type(field), bt2.field._IntegerField)
        self.assertEqual(field, 1847)

    def test_eq(self):
        fc = bt2.ArrayFieldClass(self._elem_fc, 3)
        field = fc()
        field[0] = 45
        field[1] = 1847
        field[2] = 1948754
        self.assertEqual(self._def, field)

    def test_eq_invalid_type(self):
        self.assertNotEqual(self._def, 23)

    def test_eq_diff_len(self):
        fc = bt2.ArrayFieldClass(self._elem_fc, 2)
        field = fc()
        field[0] = 45
        field[1] = 1847
        self.assertNotEqual(self._def, field)

    def test_eq_diff_content_same_len(self):
        fc = bt2.ArrayFieldClass(self._elem_fc, 3)
        field = fc()
        field[0] = 45
        field[1] = 1846
        field[2] = 1948754
        self.assertNotEqual(self._def, field)

    def test_setitem(self):
        self._def[2] = 24
        self.assertEqual(self._def[2], 24)

    def test_setitem_int_field(self):
        int_field = self._elem_fc()
        int_field.value = 19487
        self._def[1] = int_field
        self.assertEqual(self._def[1], 19487)

    def test_setitem_non_basic_field(self):
        elem_fc = bt2.StructureFieldClass()
        array_fc = bt2.ArrayFieldClass(elem_fc, 3)
        elem_field = elem_fc()
        array_field = array_fc()

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

    def test_value_unset(self):
        values = [45646, None, 12145]
        self._def.value = values
        self.assertFalse(self._def[1].is_set)

    def test_value_rollback(self):
        values = [45, 1847, 1948754]
        # value is out of range, should not affect those we set previously
        with self.assertRaises(bt2.Error):
            self._def[2].value = 2**60
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
        struct_fc = bt2.StructureFieldClass()
        int_fc = bt2.IntegerFieldClass(32)
        str_fc = bt2.StringFieldClass()
        struct_fc.append_field(field_class=int_fc, name='an_int')
        struct_fc.append_field(field_class=str_fc, name='a_string')
        struct_fc.append_field(field_class=int_fc, name='another_int')
        array_fc = bt2.ArrayFieldClass(struct_fc, 3)
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

        array = array_fc()
        array.value = values
        self.assertEqual(values, array)
        values[0]['an_int'] = 'a string'
        with self.assertRaises(TypeError):
            array.value = values

    def test_is_set(self):
        raw = self._def_value
        field = self._fc()
        self.assertFalse(field.is_set)
        field.value = raw
        self.assertTrue(field.is_set)

    def test_reset(self):
        raw = self._def_value
        field = self._fc()
        field.value = raw
        self.assertTrue(field.is_set)
        field.reset()
        self.assertFalse(field.is_set)
        other = self._fc()
        self.assertEqual(other, field)

    def test_str_op(self):
        s = str(self._def)
        expected_string = '[{}]'.format(', '.join(
            [repr(v) for v in self._def_value]))
        self.assertEqual(expected_string, s)

    def test_str_op_unset(self):
        self.assertEqual(str(self._fc()), 'Unset')


@unittest.skip("this is broken")
class ArrayFieldTestCase(_TestArraySequenceFieldCommon, unittest.TestCase):
    def setUp(self):
        self._elem_fc = bt2.IntegerFieldClass(32)
        self._fc = bt2.ArrayFieldClass(self._elem_fc, 3)
        self._def = self._fc()
        self._def[0] = 45
        self._def[1] = 1847
        self._def[2] = 1948754
        self._def_value = [45, 1847, 1948754]

    def tearDown(self):
        del self._elem_fc
        del self._fc
        del self._def

    def test_value_wrong_len(self):
        values = [45, 1847]
        with self.assertRaises(ValueError):
            self._def.value = values


@unittest.skip("this is broken")
class SequenceFieldTestCase(_TestArraySequenceFieldCommon, unittest.TestCase):
    def setUp(self):
        self._elem_fc = bt2.IntegerFieldClass(32)
        self._fc = bt2.SequenceFieldClass(self._elem_fc, 'the.length')
        self._def = self._fc()
        self._length_field = self._elem_fc(3)
        self._def.length_field = self._length_field
        self._def[0] = 45
        self._def[1] = 1847
        self._def[2] = 1948754
        self._def_value = [45, 1847, 1948754]

    def tearDown(self):
        del self._elem_fc
        del self._fc
        del self._def
        del self._length_field

    def test_value_resize(self):
        new_values = [1, 2, 3, 4]
        self._def.value = new_values
        self.assertCountEqual(self._def, new_values)

    def test_value_resize_rollback(self):
        with self.assertRaises(TypeError):
            self._def.value = [1, 2, 3, 'unexpected string']
        self.assertEqual(self._def, self._def_value)

        self._def.reset()
        with self.assertRaises(TypeError):
            self._def.value = [1, 2, 3, 'unexpected string']
        self.assertFalse(self._def.is_set)


@unittest.skip("this is broken")
class StructureFieldTestCase(_TestCopySimple, unittest.TestCase):
    def setUp(self):
        self._fc0 = bt2.IntegerFieldClass(32, is_signed=True)
        self._fc1 = bt2.StringFieldClass()
        self._fc2 = bt2.FloatingPointNumberFieldClass()
        self._fc3 = bt2.IntegerFieldClass(17)
        self._fc = bt2.StructureFieldClass()
        self._fc.append_field('A', self._fc0)
        self._fc.append_field('B', self._fc1)
        self._fc.append_field('C', self._fc2)
        self._fc.append_field('D', self._fc3)
        self._def = self._fc()
        self._def['A'] = -1872
        self._def['B'] = 'salut'
        self._def['C'] = 17.5
        self._def['D'] = 16497
        self._def_value = {
            'A': -1872,
            'B': 'salut',
            'C': 17.5,
            'D': 16497
        }

    def tearDown(self):
        del self._fc0
        del self._fc1
        del self._fc2
        del self._fc3
        del self._fc
        del self._def

    def _modify_def(self):
        self._def['B'] = 'hola'

    def test_bool_op_true(self):
        self.assertTrue(self._def)

    def test_bool_op_false(self):
        fc = bt2.StructureFieldClass()
        field = fc()
        self.assertFalse(field)

    def test_len(self):
        self.assertEqual(len(self._def), 4)

    def test_getitem(self):
        field = self._def['A']
        self.assertIs(type(field), bt2.field._IntegerField)
        self.assertEqual(field, -1872)

    def test_at_index_out_of_bounds_after(self):
        with self.assertRaises(IndexError):
            self._def.at_index(len(self._fc))

    def test_eq(self):
        fc = bt2.StructureFieldClass()
        fc.append_field('A', self._fc0)
        fc.append_field('B', self._fc1)
        fc.append_field('C', self._fc2)
        fc.append_field('D', self._fc3)
        field = fc()
        field['A'] = -1872
        field['B'] = 'salut'
        field['C'] = 17.5
        field['D'] = 16497
        self.assertEqual(self._def, field)

    def test_eq_invalid_type(self):
        self.assertNotEqual(self._def, 23)

    def test_eq_diff_len(self):
        fc = bt2.StructureFieldClass()
        fc.append_field('A', self._fc0)
        fc.append_field('B', self._fc1)
        fc.append_field('C', self._fc2)
        field = fc()
        field['A'] = -1872
        field['B'] = 'salut'
        field['C'] = 17.5
        self.assertNotEqual(self._def, field)

    def test_eq_diff_content_same_len(self):
        fc = bt2.StructureFieldClass()
        fc.append_field('A', self._fc0)
        fc.append_field('B', self._fc1)
        fc.append_field('C', self._fc2)
        fc.append_field('D', self._fc3)
        field = fc()
        field['A'] = -1872
        field['B'] = 'salut'
        field['C'] = 17.4
        field['D'] = 16497
        self.assertNotEqual(self._def, field)

    def test_eq_same_content_diff_keys(self):
        fc = bt2.StructureFieldClass()
        fc.append_field('A', self._fc0)
        fc.append_field('B', self._fc1)
        fc.append_field('E', self._fc2)
        fc.append_field('D', self._fc3)
        field = fc()
        field['A'] = -1872
        field['B'] = 'salut'
        field['E'] = 17.4
        field['D'] = 16497
        self.assertNotEqual(self._def, field)

    def test_setitem(self):
        self._def['C'] = -18.47
        self.assertEqual(self._def['C'], -18.47)

    def test_setitem_int_field(self):
        int_fc = bt2.IntegerFieldClass(16)
        int_field = int_fc()
        int_field.value = 19487
        self._def['D'] = int_field
        self.assertEqual(self._def['D'], 19487)

    def test_setitem_non_basic_field(self):
        elem_fc = bt2.StructureFieldClass()
        elem_field = elem_fc()
        struct_fc = bt2.StructureFieldClass()
        struct_fc.append_field('A', elem_fc)
        struct_field = struct_fc()

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

    def test_at_index(self):
        self.assertEqual(self._def.at_index(1), 'salut')

    def test_iter(self):
        orig_values = {
            'A': -1872,
            'B': 'salut',
            'C': 17.5,
            'D': 16497,
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
        }
        self.assertEqual(self._def, orig_values)

    def test_set_value(self):
        int_fc = bt2.IntegerFieldClass(32)
        str_fc = bt2.StringFieldClass()
        struct_fc = bt2.StructureFieldClass()
        struct_fc.append_field(field_class=int_fc, name='an_int')
        struct_fc.append_field(field_class=str_fc, name='a_string')
        struct_fc.append_field(field_class=int_fc, name='another_int')
        values = {
            'an_int': 42,
            'a_string': 'hello',
            'another_int': 66
        }

        struct = struct_fc()
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

    def test_value_rollback(self):
        int_fc = bt2.IntegerFieldClass(32)
        str_fc = bt2.StringFieldClass()
        struct_fc = bt2.StructureFieldClass()
        struct_fc.append_field(field_class=int_fc, name='an_int')
        struct_fc.append_field(field_class=str_fc, name='a_string')
        struct_fc.append_field(field_class=int_fc, name='another_int')
        values = {
            'an_int': 42,
            'a_string': 'hello',
            'another_int': 66
        }

    def test_is_set(self):
        values = {
            'an_int': 42,
            'a_string': 'hello',
            'another_int': 66
        }

        int_fc = bt2.IntegerFieldClass(32)
        str_fc = bt2.StringFieldClass()
        struct_fc = bt2.StructureFieldClass()
        struct_fc.append_field(field_class=int_fc, name='an_int')
        struct_fc.append_field(field_class=str_fc, name='a_string')
        struct_fc.append_field(field_class=int_fc, name='another_int')

        struct = struct_fc()
        self.assertFalse(struct.is_set)
        struct.value = values
        self.assertTrue(struct.is_set)

        struct = struct_fc()
        struct['an_int'].value = 42
        self.assertFalse(struct.is_set)

    def test_reset(self):
        values = {
            'an_int': 42,
            'a_string': 'hello',
            'another_int': 66
        }

        int_fc = bt2.IntegerFieldClass(32)
        str_fc = bt2.StringFieldClass()
        struct_fc = bt2.StructureFieldClass()
        struct_fc.append_field(field_class=int_fc, name='an_int')
        struct_fc.append_field(field_class=str_fc, name='a_string')
        struct_fc.append_field(field_class=int_fc, name='another_int')

        struct = struct_fc()
        struct.value = values
        self.assertTrue(struct.is_set)
        struct.reset()
        self.assertEqual(struct_fc(), struct)

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

    def test_str_op_unset(self):
        self.assertEqual(str(self._fc()), 'Unset')


@unittest.skip("this is broken")
class VariantFieldTestCase(_TestCopySimple, unittest.TestCase):
    def setUp(self):
        self._tag_fc = bt2.EnumerationFieldClass(size=32)
        self._tag_fc.add_mapping('corner', 23)
        self._tag_fc.add_mapping('zoom', 17, 20)
        self._tag_fc.add_mapping('mellotron', 1001)
        self._tag_fc.add_mapping('giorgio', 2000, 3000)
        self._fc0 = bt2.IntegerFieldClass(32, is_signed=True)
        self._fc1 = bt2.StringFieldClass()
        self._fc2 = bt2.FloatingPointNumberFieldClass()
        self._fc3 = bt2.IntegerFieldClass(17)
        self._fc = bt2.VariantFieldClass('salut', self._tag_fc)
        self._fc.append_field('corner', self._fc0)
        self._fc.append_field('zoom', self._fc1)
        self._fc.append_field('mellotron', self._fc2)
        self._fc.append_field('giorgio', self._fc3)
        self._def = self._fc()

    def tearDown(self):
        del self._tag_fc
        del self._fc0
        del self._fc1
        del self._fc2
        del self._fc3
        del self._fc
        del self._def

    def test_bool_op_true(self):
        tag_field = self._tag_fc(1001)
        self._def.field(tag_field).value = -17.34
        self.assertTrue(self._def)

    def test_bool_op_false(self):
        self.assertFalse(self._def)

    def test_tag_field_none(self):
        self.assertIsNone(self._def.tag_field)

    def test_tag_field(self):
        tag_field = self._tag_fc(2800)
        self._def.field(tag_field).value = 1847
        self.assertEqual(self._def.tag_field, tag_field)
        self.assertEqual(self._def.tag_field.addr, tag_field.addr)

    def test_selected_field_none(self):
        self.assertIsNone(self._def.selected_field)

    def test_selected_field(self):
        var_field1 = self._fc()
        tag_field1 = self._tag_fc(1001)
        var_field1.field(tag_field1).value = -17.34
        self.assertEqual(var_field1.field(), -17.34)
        self.assertEqual(var_field1.selected_field, -17.34)
        var_field2 = self._fc()
        tag_field2 = self._tag_fc(2500)
        var_field2.field(tag_field2).value = 1921
        self.assertEqual(var_field2.field(), 1921)
        self.assertEqual(var_field2.selected_field, 1921)

    def test_eq(self):
        tag_fc = bt2.EnumerationFieldClass(size=32)
        tag_fc.add_mapping('corner', 23)
        tag_fc.add_mapping('zoom', 17, 20)
        tag_fc.add_mapping('mellotron', 1001)
        tag_fc.add_mapping('giorgio', 2000, 3000)
        fc0 = bt2.IntegerFieldClass(32, is_signed=True)
        fc1 = bt2.StringFieldClass()
        fc2 = bt2.FloatingPointNumberFieldClass()
        fc3 = bt2.IntegerFieldClass(17)
        fc = bt2.VariantFieldClass('salut', tag_fc)
        fc.append_field('corner', fc0)
        fc.append_field('zoom', fc1)
        fc.append_field('mellotron', fc2)
        fc.append_field('giorgio', fc3)
        field = fc()
        field_tag = tag_fc(23)
        def_tag = self._tag_fc(23)
        field.field(field_tag).value = 1774
        self._def.field(def_tag).value = 1774
        self.assertEqual(self._def, field)

    def test_eq_invalid_type(self):
        self.assertNotEqual(self._def, 23)

    def test_is_set(self):
        self.assertFalse(self._def.is_set)
        tag_field = self._tag_fc(2800)
        self._def.field(tag_field).value = 684
        self.assertTrue(self._def.is_set)

    def test_reset(self):
        tag_field = self._tag_fc(2800)
        self._def.field(tag_field).value = 684
        self._def.reset()
        self.assertFalse(self._def.is_set)
        self.assertIsNone(self._def.selected_field)
        self.assertIsNone(self._def.tag_field)

    def test_str_op_int(self):
        v = self._fc()
        v.field(self._tag_fc(23)).value = 42
        f = self._fc0(42)
        self.assertEqual(str(f), str(v))

    def test_str_op_str(self):
        v = self._fc()
        v.field(self._tag_fc(18)).value = 'some test string'
        f = self._fc1('some test string')
        self.assertEqual(str(f), str(v))

    def test_str_op_flt(self):
        v = self._fc()
        v.field(self._tag_fc(1001)).value = 14.4245
        f = self._fc2(14.4245)
        self.assertEqual(str(f), str(v))

    def test_str_op_unset(self):
        self.assertEqual(str(self._fc()), 'Unset')
