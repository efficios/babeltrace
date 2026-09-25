# SPDX-License-Identifier: GPL-2.0-only
# Copyright (C) 2019-2026 EfficiOS Inc.

import functools

import bt2
import utils
import pytest


# Subclass must define:
#
# create_fc_func() fixture:
#     Returns a callable that accepts `*args` and `**kwargs` and returns
#     a fresh, valid, mutable field class.
#
#     The callable should capture any required fixtures (like `def_tc`)
#     in its closure.
#
# const_fc() fixture:
#     Returns a const field class for testing const-specific behavior.
#
#     Use utils.create_const_fc().
#
# Provides:
#
# • fc() fixture (calls your create_fc_func() fixture).
# • User attribute tests.
class _TestBase:
    @pytest.fixture
    def fc(self, create_fc_func):
        return create_fc_func()

    def test_create_def_not_none(self, fc):
        assert fc is not None

    def test_create_def_zero_user_attrs(self, fc):
        assert len(fc.user_attributes) == 0

    def test_create_user_attrs(self, create_fc_func):
        fc = create_fc_func(user_attributes={"salut": 23})
        assert fc.user_attributes == {"salut": 23}
        assert type(fc.user_attributes) is bt2.MapValue

    def test_const_user_attrs(self, const_fc):
        assert type(const_fc.user_attributes) is bt2._MapValueConst

    def test_create_invalid_user_attrs(self, create_fc_func):
        with pytest.raises(TypeError):
            create_fc_func(user_attributes=object())

    def test_create_invalid_user_attrs_value_type(self, create_fc_func):
        with pytest.raises(TypeError):
            create_fc_func(user_attributes=23)


class TestBool(_TestBase):
    @pytest.fixture
    def create_fc_func(self, def_tc):
        return def_tc.create_bool_field_class

    @pytest.fixture
    def const_fc(self, def_tc, fc):
        def value_setter(field):
            field.value = False

        return utils.create_const_fc(def_tc, fc, value_setter)


class TestBitArray(_TestBase):
    @pytest.fixture
    def create_fc_func(self, def_tc):
        return functools.partial(def_tc.create_bit_array_field_class, 17)

    @pytest.fixture
    def const_fc(self, def_tc, fc):
        def value_setter(field):
            field.value = []

        return utils.create_const_fc(def_tc, fc, value_setter)

    def test_create_len_out_of_range(self, def_tc):
        with pytest.raises(ValueError):
            def_tc.create_bit_array_field_class(65)

    def test_create_len_zero(self, def_tc):
        with pytest.raises(ValueError):
            def_tc.create_bit_array_field_class(0)

    def test_create_len_invalid_type(self, def_tc):
        with pytest.raises(TypeError):
            def_tc.create_bit_array_field_class("lel")

    def test_len_prop(self, fc):
        assert fc.length == 17


# Subclass must satisfy the requirements of `_TestBase`.
class _TestInt(_TestBase):
    def test_create_def(self, fc):
        assert fc.field_value_range == 64
        assert fc.preferred_display_base == bt2.IntegerDisplayBase.DECIMAL

    def test_create_range(self, create_fc_func):
        assert create_fc_func(field_value_range=35).field_value_range == 35

    def test_create_range_positional(self, create_fc_func):
        assert create_fc_func(36).field_value_range == 36

    def test_create_invalid_range_type(self, create_fc_func):
        with pytest.raises(TypeError):
            create_fc_func("yes")

    def test_create_invalid_range_type_kwarg(self, create_fc_func):
        with pytest.raises(TypeError):
            create_fc_func(field_value_range="yes")

    def test_create_invalid_range_negative(self, create_fc_func):
        with pytest.raises(ValueError):
            create_fc_func(field_value_range=-2)

    def test_create_invalid_range_zero(self, create_fc_func):
        with pytest.raises(ValueError):
            create_fc_func(field_value_range=0)

    def test_create_base(self, create_fc_func):
        fc = create_fc_func(preferred_display_base=bt2.IntegerDisplayBase.HEXADECIMAL)
        assert fc.preferred_display_base == bt2.IntegerDisplayBase.HEXADECIMAL

    def test_create_base_int(self, create_fc_func):
        fc = create_fc_func(preferred_display_base=16)
        assert fc.preferred_display_base == bt2.IntegerDisplayBase.HEXADECIMAL

    def test_create_invalid_base_type(self, create_fc_func):
        with pytest.raises(TypeError):
            create_fc_func(preferred_display_base="yes")

    def test_create_invalid_base_int_value(self, create_fc_func):
        with pytest.raises(ValueError, match="444 is not a valid IntegerDisplayBase"):
            create_fc_func(preferred_display_base=444)

    def test_create_full(self, create_fc_func):
        fc = create_fc_func(24, preferred_display_base=bt2.IntegerDisplayBase.OCTAL)
        assert fc.field_value_range == 24
        assert fc.preferred_display_base == bt2.IntegerDisplayBase.OCTAL


class TestSInt(_TestInt):
    @pytest.fixture
    def create_fc_func(self, def_tc):
        return def_tc.create_signed_integer_field_class

    @pytest.fixture
    def const_fc(self, def_tc, fc):
        def value_setter(field):
            field.value = -18

        return utils.create_const_fc(def_tc, fc, value_setter)


class TestUInt(_TestInt):
    @pytest.fixture
    def create_fc_func(self, def_tc):
        return def_tc.create_unsigned_integer_field_class

    @pytest.fixture
    def const_fc(self, def_tc, fc):
        def value_setter(field):
            field.value = 18

        return utils.create_const_fc(def_tc, fc, value_setter)


# Subclass must satisfy the requirements of `_TestInt` and define:
#
# mapping_type() fixture:
#     Returns the mapping type for test_getitem() assertion.
#
# range_set_const_type() fixture:
#     Returns the range set const type for test_getitem() assertion.
#
# find_by_value() fixture:
#     Returns the value to use in test_find_by_value().
#
# ranges_1() fixture:
#     Returns the first range set.
#
# ranges_2() fixture:
#     Returns the second range set.
#
# ranges_3() fixture:
#     Returns the third range set.
#
# invalid_ranges() fixture:
#     Returns an invalid range set (wrong signedness).
class _TestEnum(_TestInt):
    def test_create_from_invalid_type(self, create_fc_func):
        with pytest.raises(TypeError):
            create_fc_func("coucou")

    def test_add_mapping_simple(self, fc, ranges_1):
        fc.add_mapping("hello", ranges_1)
        mapping = fc["hello"]
        assert mapping.label == "hello"
        assert mapping.ranges == ranges_1

    def test_const_add_mapping(self, const_fc, ranges_1):
        with pytest.raises(AttributeError):
            const_fc.add_mapping("hello", ranges_1)

    def test_add_mapping_simple_kwargs(self, fc, ranges_1):
        fc.add_mapping(label="hello", ranges=ranges_1)
        mapping = fc["hello"]
        assert mapping.label == "hello"
        assert mapping.ranges == ranges_1

    def test_add_mapping_invalid_name(self, fc, ranges_1):
        with pytest.raises(TypeError):
            fc.add_mapping(17, ranges_1)

    def test_add_mapping_invalid_range(self, fc):
        with pytest.raises(TypeError):
            fc.add_mapping("allo", "meow")

    def test_add_mapping_dup_label(self, fc, ranges_1, ranges_2):
        with pytest.raises(ValueError):
            fc.add_mapping("a", ranges_1)
            fc.add_mapping("a", ranges_2)

    def test_add_mapping_invalid_ranges_signedness(self, fc, invalid_ranges):
        with pytest.raises(TypeError):
            fc.add_mapping("allo", invalid_ranges)

    def test_iadd(self, fc, ranges_1, ranges_2, ranges_3):
        fc.add_mapping("c", ranges_1)
        fc += [("d", ranges_2), ("e", ranges_3)]
        assert len(fc) == 3
        assert fc["c"].label == "c"
        assert fc["c"].ranges == ranges_1
        assert fc["d"].label == "d"
        assert fc["d"].ranges == ranges_2
        assert fc["e"].label == "e"
        assert fc["e"].ranges == ranges_3

    def test_const_iadd(self, const_fc, ranges_2, ranges_3):
        with pytest.raises(TypeError):
            const_fc += [("d", ranges_2), ("e", ranges_3)]

    def test_bool_op(self, fc, ranges_1):
        assert not fc
        fc.add_mapping("a", ranges_1)
        assert fc

    def test_len(self, fc, ranges_1, ranges_2, ranges_3):
        fc.add_mapping("a", ranges_1)
        fc.add_mapping("b", ranges_2)
        fc.add_mapping("c", ranges_3)
        assert len(fc) == 3

    def test_getitem(
        self, fc, ranges_1, ranges_2, ranges_3, mapping_type, range_set_const_type
    ):
        fc.add_mapping("a", ranges_1)
        fc.add_mapping("b", ranges_2)
        fc.add_mapping("c", ranges_3)
        mapping = fc["a"]
        assert mapping.label == "a"
        assert mapping.ranges == ranges_1
        assert type(mapping) is mapping_type
        assert type(mapping.ranges) is range_set_const_type

    def test_getitem_nonexistent(self, fc):
        with pytest.raises(KeyError):
            fc["doesnotexist"]

    def test_iter(self, fc, ranges_1, ranges_2, ranges_3):
        fc.add_mapping("a", ranges_1)
        fc.add_mapping("b", ranges_2)
        fc.add_mapping("c", ranges_3)
        assert sorted(fc) == ["a", "b", "c"]

    def test_find_by_value(self, fc, ranges_1, ranges_2, ranges_3, find_by_value):
        fc.add_mapping("a", ranges_1)
        fc.add_mapping("b", ranges_2)
        fc.add_mapping("c", ranges_3)
        mappings = fc.mappings_for_value(find_by_value)
        assert {mapping.label for mapping in mappings} == {"a", "c"}

    def test_find_by_value_none_found(self, fc, ranges_1):
        fc.add_mapping("a", ranges_1)
        mappings = fc.mappings_for_value(999999)
        assert mappings == []


class TestUnsignedEnum(_TestEnum):
    @pytest.fixture(scope="class")
    def mapping_type(self):
        return bt2._UnsignedEnumerationFieldClassMappingConst

    @pytest.fixture(scope="class")
    def range_set_const_type(self):
        return bt2._UnsignedIntegerRangeSetConst

    @pytest.fixture(scope="class")
    def find_by_value(self):
        return 20

    @pytest.fixture
    def create_fc_func(self, def_tc):
        return def_tc.create_unsigned_enumeration_field_class

    @pytest.fixture
    def const_fc(self, def_tc, fc):
        def value_setter(field):
            field.value = 0

        return utils.create_const_fc(def_tc, fc, value_setter)

    @pytest.fixture
    def ranges_1(self):
        return bt2.UnsignedIntegerRangeSet([(1, 4), (18, 47)])

    @pytest.fixture
    def ranges_2(self):
        return bt2.UnsignedIntegerRangeSet([(5, 5)])

    @pytest.fixture
    def ranges_3(self):
        return bt2.UnsignedIntegerRangeSet([(8, 22), (48, 99)])

    @pytest.fixture
    def invalid_ranges(self):
        return bt2.SignedIntegerRangeSet([(-8, -5), (48, 1928)])


class TestSignedEnum(_TestEnum):
    @pytest.fixture(scope="class")
    def mapping_type(self):
        return bt2._SignedEnumerationFieldClassMappingConst

    @pytest.fixture(scope="class")
    def range_set_const_type(self):
        return bt2._SignedIntegerRangeSetConst

    @pytest.fixture(scope="class")
    def find_by_value(self):
        return -7

    @pytest.fixture
    def create_fc_func(self, def_tc):
        return def_tc.create_signed_enumeration_field_class

    @pytest.fixture
    def const_fc(self, def_tc, fc):
        def value_setter(field):
            field.value = 0

        return utils.create_const_fc(def_tc, fc, value_setter)

    @pytest.fixture
    def ranges_1(self):
        return bt2.SignedIntegerRangeSet([(-10, -4), (18, 47)])

    @pytest.fixture
    def ranges_2(self):
        return bt2.SignedIntegerRangeSet([(-3, -3)])

    @pytest.fixture
    def ranges_3(self):
        return bt2.SignedIntegerRangeSet([(-100, -1), (8, 16), (48, 99)])

    @pytest.fixture
    def invalid_ranges(self):
        return bt2.UnsignedIntegerRangeSet([(8, 16), (48, 99)])


class TestSingleReal(_TestBase):
    @pytest.fixture
    def create_fc_func(self, def_tc):
        return def_tc.create_single_precision_real_field_class

    @pytest.fixture
    def const_fc(self, def_tc, fc):
        def value_setter(field):
            field.value = -18.0

        return utils.create_const_fc(def_tc, fc, value_setter)


class TestDoubleReal(_TestBase):
    @pytest.fixture
    def create_fc_func(self, def_tc):
        return def_tc.create_double_precision_real_field_class

    @pytest.fixture
    def const_fc(self, def_tc, fc):
        def value_setter(field):
            field.value = -18.0

        return utils.create_const_fc(def_tc, fc, value_setter)


class TestStr(_TestBase):
    @pytest.fixture
    def create_fc_func(self, def_tc):
        return def_tc.create_string_field_class

    @pytest.fixture
    def const_fc(self, def_tc, fc):
        def value_setter(field):
            field.value = "chaine"

        return utils.create_const_fc(def_tc, fc, value_setter)


class TestStruct(_TestBase):
    @pytest.fixture
    def create_fc_func(self, def_tc):
        return def_tc.create_structure_field_class

    @pytest.fixture
    def const_fc(self, def_tc, fc):
        def value_setter(field):
            field.value = {}

        return utils.create_const_fc(def_tc, fc, value_setter)

    @pytest.fixture
    def int_fc(self, def_tc):
        return def_tc.create_signed_integer_field_class(32)

    def test_append_member(self, fc, int_fc):
        fc.append_member("int32", int_fc)
        assert fc["int32"].field_class.addr == int_fc.addr

    def test_append_member_kwargs(self, fc, int_fc):
        fc.append_member(name="int32", field_class=int_fc)
        assert fc["int32"].field_class.addr == int_fc.addr

    def test_append_member_invalid_name(self, def_tc, fc):
        sub_fc = def_tc.create_string_field_class()
        with pytest.raises(TypeError):
            fc.append_member(23, sub_fc)

    def test_append_member_invalid_fc(self, fc):
        with pytest.raises(TypeError):
            fc.append_member("yes", object())

    def test_append_member_dup_name(self, def_tc, fc):
        sub_fc_1 = def_tc.create_string_field_class()
        sub_fc_2 = def_tc.create_string_field_class()

        with pytest.raises(ValueError):
            fc.append_member("yes", sub_fc_1)
            fc.append_member("yes", sub_fc_2)

    def test_attr_fc(self, fc, int_fc):
        fc.append_member("int32", int_fc)
        assert type(fc["int32"].field_class) is bt2._SignedIntegerFieldClass

    def test_const_attr_fc(self, def_tc, fc, int_fc):
        def value_setter(field):
            field.value = {}

        fc.append_member("int32", int_fc)
        const_fc = utils.create_const_fc(def_tc, fc, value_setter)

        assert type(const_fc["int32"].field_class) is bt2._SignedIntegerFieldClassConst

    def test_iadd(self, def_tc, fc):
        a_fc = def_tc.create_single_precision_real_field_class()
        b_fc = def_tc.create_signed_integer_field_class(17)
        fc.append_member("a_float", a_fc)
        fc.append_member("b_int", b_fc)
        c_fc = def_tc.create_string_field_class()
        d_fc = def_tc.create_signed_enumeration_field_class(field_value_range=32)
        e_fc = def_tc.create_structure_field_class()
        fc += [
            ("c_string", c_fc),
            ("d_enum", d_fc),
            ("e_struct", e_fc),
        ]
        assert fc["a_float"].field_class.addr == a_fc.addr
        assert fc["a_float"].name == "a_float"
        assert fc["b_int"].field_class.addr == b_fc.addr
        assert fc["b_int"].name == "b_int"
        assert fc["c_string"].field_class.addr == c_fc.addr
        assert fc["c_string"].name == "c_string"
        assert fc["d_enum"].field_class.addr == d_fc.addr
        assert fc["d_enum"].name == "d_enum"
        assert fc["e_struct"].field_class.addr == e_fc.addr
        assert fc["e_struct"].name == "e_struct"

    def test_const_iadd(self, def_tc, const_fc):
        a_fc = def_tc.create_single_precision_real_field_class()

        with pytest.raises(TypeError):
            const_fc += a_fc

    def test_bool_op(self, def_tc, fc):
        assert not fc
        fc.append_member("a", def_tc.create_string_field_class())
        assert fc

    def test_len(self, def_tc, fc):
        fc.append_member("a", def_tc.create_string_field_class())
        fc.append_member("b", def_tc.create_string_field_class())
        fc.append_member("c", def_tc.create_string_field_class())
        assert len(fc) == 3

    def test_getitem(self, def_tc, fc):
        b_fc = def_tc.create_string_field_class()
        fc.append_member("a", def_tc.create_signed_integer_field_class(32))
        fc.append_member("b", b_fc)
        fc.append_member("c", def_tc.create_single_precision_real_field_class())
        assert fc["b"].field_class.addr == b_fc.addr
        assert fc["b"].name == "b"

    def test_getitem_invalid_key_type(self, fc):
        with pytest.raises(TypeError):
            fc[0]

    def test_getitem_invalid_key(self, fc):
        with pytest.raises(KeyError):
            fc["no way"]

    def test_contains(self, def_tc, fc):
        assert "a" not in fc
        fc.append_member("a", def_tc.create_string_field_class())
        assert "a" in fc

    def test_iter(self, def_tc, fc):
        members = (
            ("a", def_tc.create_signed_integer_field_class(32)),
            ("b", def_tc.create_string_field_class()),
            ("c", def_tc.create_single_precision_real_field_class()),
        )

        for name, member_fc in members:
            fc.append_member(name, member_fc)

        for (name, member), test_member in zip(fc.items(), members):
            assert member.name == test_member[0]
            assert name == member.name
            assert member.field_class.addr == test_member[1].addr
            assert len(member.user_attributes) == 0

    def test_at_index(self, def_tc, fc):
        a_fc = def_tc.create_signed_integer_field_class(32)
        fc.append_member("c", def_tc.create_single_precision_real_field_class())
        fc.append_member("a", a_fc)
        fc.append_member("b", def_tc.create_string_field_class())
        member = fc.member_at_index(1)
        assert member.field_class.addr == a_fc.addr
        assert member.name == "a"

    def test_at_index_invalid(self, def_tc, fc):
        fc.append_member("c", def_tc.create_signed_integer_field_class(32))

        with pytest.raises(TypeError):
            fc.member_at_index("yes")

    def test_at_index_out_of_bounds(self, def_tc, fc):
        fc.append_member("c", def_tc.create_signed_integer_field_class(32))

        with pytest.raises(IndexError):
            fc.member_at_index(len(fc))

    def test_member_user_attrs(self, def_tc, fc):
        fc.append_member(
            "c",
            def_tc.create_string_field_class(),
            user_attributes={"salut": 23},
        )

        assert fc["c"].user_attributes == {"salut": 23}
        assert type(fc.user_attributes) is bt2.MapValue
        assert type(fc["c"].user_attributes) is bt2.MapValue

    def test_invalid_member_user_attrs(self, def_tc, fc):
        with pytest.raises(TypeError):
            fc.append_member(
                "c",
                def_tc.create_string_field_class(),
                user_attributes=object(),
            )

    def test_invalid_member_user_attrs_value_type(self, def_tc, fc):
        with pytest.raises(TypeError):
            fc.append_member(
                "c",
                def_tc.create_string_field_class(),
                user_attributes=23,
            )

    def test_const_member_fc(self, def_tc):
        def real_value_setter(field):
            field.value = {"real": 0}

        const_fc = utils.create_const_fc(
            def_tc,
            def_tc.create_structure_field_class(
                members=(("real", def_tc.create_single_precision_real_field_class()),)
            ),
            real_value_setter,
        )

        assert (
            type(const_fc["real"].field_class)
            is bt2._SinglePrecisionRealFieldClassConst
        )

    def test_member_fc(self, def_tc):
        fc = def_tc.create_structure_field_class(
            members=(("real", def_tc.create_single_precision_real_field_class()),)
        )

        assert type(fc["real"].field_class) is bt2._SinglePrecisionRealFieldClass


class TestStaticArray(_TestBase):
    @pytest.fixture
    def create_fc_func(self, def_tc):
        def func(*args, **kwargs):
            elem_fc = def_tc.create_signed_integer_field_class(23)
            return def_tc.create_static_array_field_class(elem_fc, 45, *args, **kwargs)

        return func

    @pytest.fixture
    def const_fc(self, def_tc, fc):
        def value_setter(field):
            field.value = [9] * 45

        return utils.create_const_fc(def_tc, fc, value_setter)

    @pytest.fixture
    def elem_fc(self, def_tc):
        return def_tc.create_signed_integer_field_class(23)

    def test_create_def(self, def_tc, elem_fc):
        fc = def_tc.create_static_array_field_class(elem_fc, 45)
        assert fc.element_field_class.addr == elem_fc.addr
        assert fc.length == 45
        assert len(fc.user_attributes) == 0

    def test_create_invalid_elem_fc(self, def_tc):
        with pytest.raises(TypeError):
            def_tc.create_static_array_field_class(object(), 45)

    def test_create_invalid_len(self, def_tc):
        with pytest.raises(ValueError):
            def_tc.create_static_array_field_class(
                def_tc.create_string_field_class(), -17
            )

    def test_create_invalid_len_type(self, def_tc):
        with pytest.raises(TypeError):
            def_tc.create_static_array_field_class(
                def_tc.create_string_field_class(), "the length"
            )

    def test_attr_elem_fc(self, fc):
        assert type(fc.element_field_class) is bt2._SignedIntegerFieldClass

    def test_const_attr_elem_fc(self, const_fc):
        assert type(const_fc.element_field_class) is bt2._SignedIntegerFieldClassConst


# Subclass must satisfy the requirements of `_TestBase`.
class _TestDynArray(_TestBase):
    @pytest.fixture
    def elem_fc(self, def_tc):
        return def_tc.create_signed_integer_field_class(23)

    def test_attr_elem_fc(self, fc):
        assert type(fc.element_field_class) is bt2._SignedIntegerFieldClass

    def test_const_attr_elem_fc(self, const_fc):
        assert type(const_fc.element_field_class) is bt2._SignedIntegerFieldClassConst


class TestDynArray(_TestDynArray):
    @pytest.fixture
    def create_fc_func(self, def_tc):
        def func(*args, **kwargs):
            return def_tc.create_dynamic_array_field_class(
                def_tc.create_signed_integer_field_class(23), *args, **kwargs
            )

        return func

    @pytest.fixture
    def const_fc(self, def_tc, fc):
        def value_setter(field):
            field.value = []

        return utils.create_const_fc(def_tc, fc, value_setter)

    def test_create_def(self, def_tc, elem_fc):
        fc = def_tc.create_dynamic_array_field_class(elem_fc)
        assert fc.element_field_class.addr == elem_fc.addr
        assert len(fc.user_attributes) == 0

    def test_create_invalid_fc(self, def_tc):
        with pytest.raises(TypeError):
            def_tc.create_dynamic_array_field_class(object())


class TestDynArrayWithLenField(_TestDynArray):
    @pytest.fixture
    def create_fc_func(self, def_tc):
        def func(*args, **kwargs):
            return def_tc.create_dynamic_array_field_class(
                def_tc.create_signed_integer_field_class(23),
                def_tc.create_unsigned_integer_field_class(12),
                *args,
                **kwargs
            )

        return func

    @pytest.fixture
    def const_fc(self, def_tc):
        len_fc = def_tc.create_unsigned_integer_field_class(12)

        def value_setter(field):
            field["len"] = 0
            field["dyn_array"] = []

        return utils.create_const_fc(
            def_tc,
            def_tc.create_structure_field_class(
                members=(
                    ("len", len_fc),
                    (
                        "dyn_array",
                        def_tc.create_dynamic_array_field_class(
                            def_tc.create_signed_integer_field_class(23), len_fc
                        ),
                    ),
                )
            ),
            value_setter,
        )["dyn_array"].field_class

    @pytest.fixture
    def len_fc(self, def_tc):
        return def_tc.create_unsigned_integer_field_class(12)

    def test_create_def(self, def_tc, elem_fc, len_fc):
        fc = def_tc.create_dynamic_array_field_class(elem_fc, len_fc)
        assert fc.element_field_class.addr == elem_fc.addr
        assert fc.length_field_path is None
        assert len(fc.user_attributes) == 0

    @pytest.fixture
    def fc_for_field_path_test(self, def_tc, elem_fc, len_fc):
        fc = def_tc.create_dynamic_array_field_class(elem_fc, len_fc)

        def_tc.create_stream_class(
            packet_context_field_class=def_tc.create_structure_field_class(
                members=(
                    ("foo", def_tc.create_single_precision_real_field_class()),
                    (
                        "inner_struct",
                        def_tc.create_static_array_field_class(
                            def_tc.create_structure_field_class(
                                members=(
                                    ("bar", def_tc.create_string_field_class()),
                                    ("baz", def_tc.create_string_field_class()),
                                    ("len", len_fc),
                                    ("dyn_array", fc),
                                )
                            ),
                            2,
                        ),
                    ),
                )
            ),
            supports_packets=True,
        )

        return fc

    def test_field_path_len(self, fc_for_field_path_test):
        assert len(fc_for_field_path_test.length_field_path) == 3

    def test_field_path_iter(self, fc_for_field_path_test):
        path_items = list(fc_for_field_path_test.length_field_path)
        assert len(path_items) == 3
        assert isinstance(path_items[0], bt2._IndexFieldPathItem)
        assert path_items[0].index == 1
        assert isinstance(path_items[1], bt2._CurrentArrayElementFieldPathItem)
        assert isinstance(path_items[2], bt2._IndexFieldPathItem)
        assert path_items[2].index == 2

    def test_field_path_root_scope(self, fc_for_field_path_test):
        assert (
            fc_for_field_path_test.length_field_path.root_scope
            == bt2.FieldPathScope.PACKET_CONTEXT
        )

    def test_create_invalid_len_type(self, def_tc):
        with pytest.raises(TypeError):
            def_tc.create_dynamic_array_field_class(
                def_tc.create_string_field_class(), 17
            )


# Subclass must satisfy the requirements of `_TestBase`.
#
# Provides:
#
# • `content_fc()` fixture (signed integer field class with
#   23-bit range).
#
# • `test_attr_fc()` and `test_const_attr_fc()` tests.
class _TestOpt(_TestBase):
    @pytest.fixture
    def content_fc(self, def_tc):
        return def_tc.create_signed_integer_field_class(23)

    def test_attr_fc(self, fc):
        assert type(fc.field_class) is bt2._SignedIntegerFieldClass

    def test_const_attr_fc(self, const_fc):
        assert type(const_fc.field_class) is bt2._SignedIntegerFieldClassConst


# Subclass must satisfy the requirements of `_TestOpt` and define:
#
# sel_fc() fixture:
#     Returns the selector field class.
#
# Provides:
#
# • const_fc() fixture.
class _TestOptWithSel(_TestOpt):
    @pytest.fixture
    def const_fc(self, def_tc, fc, sel_fc):
        def value_setter(field):
            field["opt"].has_field = True
            field["opt"].value = 12

        return utils.create_const_fc(
            def_tc,
            def_tc.create_structure_field_class(
                members=(("selecteux", sel_fc), ("opt", fc))
            ),
            value_setter,
        )["opt"].field_class


@pytest.mark.filterwarnings(
    r"ignore:.*create_option_without_selector_field_class.*:DeprecationWarning"
)
class TestOptWithoutSel(_TestOpt):
    @pytest.fixture
    def create_fc_func(self, def_tc, content_fc):
        return functools.partial(
            def_tc.create_option_without_selector_field_class, content_fc
        )

    @pytest.fixture
    def const_fc(self, def_tc, fc):
        def value_setter(field):
            field.has_field = True
            field.value = 12

        return utils.create_const_fc(def_tc, fc, value_setter)

    def test_create_def(self, fc, content_fc):
        assert fc.field_class.addr == content_fc.addr
        assert len(fc.user_attributes) == 0

    def test_create_invalid_fc(self, def_tc):
        with pytest.raises(TypeError):
            def_tc.create_option_without_selector_field_class(object())


@pytest.mark.filterwarnings(
    r"ignore:.*create_option_with_bool_selector_field_class.*:DeprecationWarning",
    r"ignore:.*create_option_without_selector_field_class.*:DeprecationWarning",
)
class TestOptWithBoolSel(_TestOptWithSel):
    @pytest.fixture
    def sel_fc(self, def_tc):
        return def_tc.create_bool_field_class()

    @pytest.fixture
    def create_fc_func(self, def_tc, content_fc, sel_fc):
        return functools.partial(
            def_tc.create_option_with_bool_selector_field_class, content_fc, sel_fc
        )

    def test_create_def(self, fc, content_fc):
        assert fc.field_class.addr == content_fc.addr
        assert not fc.selector_is_reversed
        assert len(fc.user_attributes) == 0

    def test_create_selector_is_reversed_wrong_type(self, def_tc, content_fc, sel_fc):
        with pytest.raises(TypeError):
            def_tc.create_option_with_bool_selector_field_class(
                content_fc, sel_fc, selector_is_reversed=23
            )

    def test_create_invalid_selector_type(self, def_tc, content_fc):
        with pytest.raises(TypeError):
            def_tc.create_option_with_bool_selector_field_class(content_fc, 17)

    def test_attr_selector_is_reversed(self, def_tc, content_fc, sel_fc):
        fc = def_tc.create_option_with_bool_selector_field_class(
            content_fc, sel_fc, selector_is_reversed=True
        )

        assert fc.selector_is_reversed

    def test_const_attr_selector_is_reversed(self, def_tc, content_fc, sel_fc):
        def value_setter(field):
            field["opt"].has_field = False

        const_fc = utils.create_const_fc(
            def_tc,
            def_tc.create_structure_field_class(
                members=(
                    ("selecteux", sel_fc),
                    (
                        "opt",
                        def_tc.create_option_with_bool_selector_field_class(
                            content_fc, sel_fc, selector_is_reversed=True
                        ),
                    ),
                )
            ),
            value_setter,
        )["opt"].field_class

        assert const_fc.selector_is_reversed

    @pytest.fixture
    def fc_for_field_path_test(self, def_tc, content_fc, sel_fc):
        fc = def_tc.create_option_with_bool_selector_field_class(content_fc, sel_fc)

        def_tc.create_stream_class(
            packet_context_field_class=def_tc.create_structure_field_class(
                members=(
                    ("foo", def_tc.create_single_precision_real_field_class()),
                    (
                        "inner_opt",
                        def_tc.create_option_without_selector_field_class(
                            def_tc.create_structure_field_class(
                                members=(
                                    ("bar", def_tc.create_string_field_class()),
                                    ("baz", def_tc.create_string_field_class()),
                                    ("tag", sel_fc),
                                    ("opt", fc),
                                )
                            )
                        ),
                    ),
                )
            ),
            supports_packets=True,
        )

        return fc

    def test_field_path_len(self, fc_for_field_path_test):
        assert len(fc_for_field_path_test.selector_field_path) == 3

    def test_field_path_iter(self, fc_for_field_path_test):
        path_items = list(fc_for_field_path_test.selector_field_path)
        assert len(path_items) == 3
        assert isinstance(path_items[0], bt2._IndexFieldPathItem)
        assert path_items[0].index == 1
        assert isinstance(path_items[1], bt2._CurrentOptionContentFieldPathItem)
        assert isinstance(path_items[2], bt2._IndexFieldPathItem)
        assert path_items[2].index == 2

    def test_field_path_root_scope(self, fc_for_field_path_test):
        assert (
            fc_for_field_path_test.selector_field_path.root_scope
            == bt2.FieldPathScope.PACKET_CONTEXT
        )


# Subclass must satisfy the requirements of `_TestOptWithSel`
# and define:
#
# ranges() fixture:
#     Returns the ranges for the option field class.
#
# Provides:
#
# • create_fc_func() fixture.
# • Common tests.
@pytest.mark.filterwarnings(
    r"ignore:.*create_option_with_integer_selector_field_class.*:DeprecationWarning"
)
class _TestOptWithIntSel(_TestOptWithSel):
    @pytest.fixture
    def create_fc_func(self, def_tc, content_fc, sel_fc, ranges):
        return functools.partial(
            def_tc.create_option_with_integer_selector_field_class,
            content_fc,
            sel_fc,
            ranges,
        )

    def test_create_def(self, fc, content_fc, ranges):
        assert fc.field_class.addr == content_fc.addr
        assert fc.ranges == ranges
        assert len(fc.user_attributes) == 0

    def test_create_ranges_wrong_type(self, def_tc, content_fc, sel_fc):
        with pytest.raises(TypeError):
            def_tc.create_option_with_integer_selector_field_class(
                content_fc, sel_fc, 23
            )

    def test_create_invalid_sel_type(self, def_tc, content_fc, ranges):
        with pytest.raises(TypeError):
            def_tc.create_option_with_integer_selector_field_class(
                content_fc, 17, ranges
            )

    def test_attr_ranges(self, fc, ranges):
        assert fc.ranges == ranges

    def test_const_attr_ranges(self, const_fc, ranges):
        assert const_fc.ranges == ranges


class TestOptWithUIntSel(_TestOptWithIntSel):
    @pytest.fixture
    def sel_fc(self, def_tc):
        return def_tc.create_unsigned_integer_field_class()

    @pytest.fixture
    def ranges(self):
        return bt2.UnsignedIntegerRangeSet([(1, 3), (18, 44)])

    def test_create_ranges_empty(self, def_tc, content_fc, sel_fc):
        with pytest.raises(ValueError):
            def_tc.create_option_with_integer_selector_field_class(
                content_fc, sel_fc, bt2.UnsignedIntegerRangeSet()
            )


class TestOptWithSIntSel(_TestOptWithIntSel):
    @pytest.fixture
    def sel_fc(self, def_tc):
        return def_tc.create_signed_integer_field_class()

    @pytest.fixture
    def ranges(self):
        return bt2.SignedIntegerRangeSet([(-10, -4), (18, 44)])

    def test_create_ranges_empty(self, def_tc, content_fc, sel_fc):
        with pytest.raises(ValueError):
            def_tc.create_option_with_integer_selector_field_class(
                content_fc, sel_fc, bt2.SignedIntegerRangeSet()
            )


# Subclass must satisfy the requirements of `_TestBase`.
class _TestVar(_TestBase):
    def test_getitem_invalid_key_type(self, fc):
        with pytest.raises(TypeError):
            fc[0]

    def test_getitem_invalid_key(self, fc):
        with pytest.raises(KeyError):
            fc["no way"]

    def test_opt_with_name_invalid_type(self, fc):
        with pytest.raises(TypeError, match="'int' is not a 'str' object"):
            fc.option_with_name(23)


class TestVarWithoutSel(_TestVar):
    @pytest.fixture
    def create_fc_func(self, def_tc):
        return def_tc.create_variant_field_class

    @pytest.fixture
    def const_fc(self, def_tc, fc):
        def value_setter(field):
            field.selected_option_index = 0
            field.value = 12

        fc.append_option("int32", def_tc.create_signed_integer_field_class(32))
        return utils.create_const_fc(def_tc, fc, value_setter)

    def test_append_opt(self, def_tc, fc):
        int_fc = def_tc.create_signed_integer_field_class(32)
        fc.append_option("int32", int_fc)
        assert fc["int32"].field_class.addr == int_fc.addr

    def test_append_opt_kwargs(self, def_tc, fc):
        int_fc = def_tc.create_signed_integer_field_class(32)
        fc.append_option(name="int32", field_class=int_fc)
        assert fc["int32"].field_class.addr == int_fc.addr

    def test_append_opt_invalid_name(self, def_tc, fc):
        sub_fc = def_tc.create_string_field_class()
        with pytest.raises(TypeError):
            fc.append_option(23, sub_fc)

    def test_append_opt_invalid_fc(self, fc):
        with pytest.raises(TypeError):
            fc.append_option("yes", object())

    def test_append_opt_dup_name(self, def_tc, fc):
        sub_fc_1 = def_tc.create_string_field_class()
        sub_fc_2 = def_tc.create_string_field_class()
        with pytest.raises(ValueError):
            fc.append_option("yes", sub_fc_1)
            fc.append_option("yes", sub_fc_2)

    def test_iadd(self, def_tc, fc):
        a_fc = def_tc.create_single_precision_real_field_class()
        b_fc = def_tc.create_signed_integer_field_class(17)
        fc.append_option("a_float", a_fc)
        fc.append_option("b_int", b_fc)
        c_fc = def_tc.create_string_field_class()
        d_fc = def_tc.create_signed_enumeration_field_class(field_value_range=32)
        e_fc = def_tc.create_structure_field_class()
        fc += [
            ("c_string", c_fc),
            ("d_enum", d_fc),
            ("e_struct", e_fc),
        ]
        assert fc["a_float"].field_class.addr == a_fc.addr
        assert fc["a_float"].name == "a_float"
        assert fc["b_int"].field_class.addr == b_fc.addr
        assert fc["b_int"].name == "b_int"
        assert fc["c_string"].field_class.addr == c_fc.addr
        assert fc["c_string"].name == "c_string"
        assert fc["d_enum"].field_class.addr == d_fc.addr
        assert fc["d_enum"].name == "d_enum"
        assert fc["e_struct"].field_class.addr == e_fc.addr
        assert fc["e_struct"].name == "e_struct"

    def test_const_iadd(self, def_tc, const_fc):
        a_fc = def_tc.create_single_precision_real_field_class()

        with pytest.raises(TypeError):
            const_fc += a_fc

    def test_bool_op(self, def_tc, fc):
        assert not fc
        fc.append_option("a", def_tc.create_string_field_class())
        assert fc

    def test_len(self, def_tc, fc):
        fc.append_option("a", def_tc.create_string_field_class())
        fc.append_option("b", def_tc.create_string_field_class())
        fc.append_option("c", def_tc.create_string_field_class())
        assert len(fc) == 3

    def test_getitem(self, def_tc, fc):
        b_fc = def_tc.create_string_field_class()
        fc.append_option("a", def_tc.create_signed_integer_field_class(32))
        fc.append_option("b", b_fc)
        fc.append_option("c", def_tc.create_single_precision_real_field_class())
        assert fc["b"].field_class.addr == b_fc.addr
        assert fc["b"].name == "b"

    def test_contains(self, def_tc, fc):
        assert "a" not in fc
        fc.append_option("a", def_tc.create_string_field_class())
        assert "a" in fc

    def test_iter(self, def_tc, fc):
        opts = (
            ("a", def_tc.create_signed_integer_field_class(32)),
            ("b", def_tc.create_string_field_class()),
            ("c", def_tc.create_single_precision_real_field_class()),
        )

        for name, elem_fc in opts:
            fc.append_option(name, elem_fc)

        for (name, opt), test_opt in zip(fc.items(), opts):
            assert opt.name == test_opt[0]
            assert name == opt.name
            assert opt.field_class.addr == test_opt[1].addr
            assert len(opt.user_attributes) == 0

    def test_at_index(self, def_tc, fc):
        a_fc = def_tc.create_signed_integer_field_class(32)
        fc.append_option("c", def_tc.create_single_precision_real_field_class())
        fc.append_option("a", a_fc)
        fc.append_option("b", def_tc.create_string_field_class())
        opt = fc.option_at_index(1)
        assert opt.field_class.addr == a_fc.addr
        assert opt.name == "a"

    def test_at_index_invalid(self, def_tc, fc):
        fc.append_option("c", def_tc.create_signed_integer_field_class(32))

        with pytest.raises(TypeError):
            fc.option_at_index("yes")

    def test_at_index_out_of_bounds(self, def_tc, fc):
        fc.append_option("c", def_tc.create_signed_integer_field_class(32))

        with pytest.raises(IndexError):
            fc.option_at_index(len(fc))

    def test_opt_user_attrs(self, def_tc, fc):
        fc.append_option(
            "c",
            def_tc.create_string_field_class(),
            user_attributes={"salut": 23},
        )

        assert fc["c"].user_attributes == {"salut": 23}
        assert type(fc.user_attributes) is bt2.MapValue
        assert type(fc["c"].user_attributes) is bt2.MapValue

    def test_invalid_opt_user_attrs(self, def_tc, fc):
        with pytest.raises(TypeError):
            fc.append_option(
                "c",
                def_tc.create_string_field_class(),
                user_attributes=object(),
            )

    def test_invalid_opt_user_attrs_value_type(self, def_tc, fc):
        with pytest.raises(TypeError):
            fc.append_option(
                "c",
                def_tc.create_string_field_class(),
                user_attributes=23,
            )

    def test_opt_with_name(self, def_tc, fc):
        a_fc = def_tc.create_signed_integer_field_class(32)
        fc.append_option("c", def_tc.create_single_precision_real_field_class())
        fc.append_option("a", a_fc)
        fc.append_option("b", def_tc.create_string_field_class())
        assert fc.option_with_name("a").field_class.addr == a_fc.addr
        assert fc.option_with_name("a").name == "a"
        assert fc.option_with_name("bloup") is None


# Subclass must provide:
#
# sel_fc() fixture:
#     Returns the selector field class.
#
# ranges_1() fixture:
# ranges_2() fixture:
# ranges_3() fixture:
#     Return range sets for tests.
#
# invalid_ranges() fixture:
#     Returns an invalid range set (wrong signedness).
#
# Provides:
#
# • create_fc_func() and const_fc() fixtures.
#
# • Append/__iadd__() tests with ranges.
#
# • Selector field path tests.
class _TestVarWithIntSel(_TestVar):
    @pytest.fixture
    def create_fc_func(self, def_tc, sel_fc):
        return functools.partial(def_tc.create_variant_field_class, selector_fc=sel_fc)

    @pytest.fixture
    def const_fc(self, def_tc, fc, sel_fc, ranges_1):
        def value_setter(field):
            field["variant"].selected_option_index = 0
            field["variant"] = 12

        fc.append_option("a", def_tc.create_signed_integer_field_class(32), ranges_1)
        struct_fc = def_tc.create_structure_field_class(
            members=(("selecteux", sel_fc),)
        )
        struct_fc.append_member("variant", fc)
        return utils.create_const_fc(def_tc, struct_fc, value_setter)[
            "variant"
        ].field_class

    def test_append_opt(self, def_tc, fc, ranges_1):
        str_fc = def_tc.create_string_field_class()
        fc.append_option("str", str_fc, ranges_1)
        opt = fc["str"]
        assert opt.field_class.addr == str_fc.addr
        assert opt.name == "str"
        assert opt.ranges.addr == ranges_1.addr

    def test_const_append(self, def_tc, const_fc, ranges_1):
        str_fc = def_tc.create_string_field_class()

        with pytest.raises(AttributeError):
            const_fc.append_option("str", str_fc, ranges_1)

    def test_append_opt_kwargs(self, def_tc, fc, ranges_1):
        int_fc = def_tc.create_signed_integer_field_class(32)
        fc.append_option(name="int32", field_class=int_fc, ranges=ranges_1)
        opt = fc["int32"]
        assert opt.field_class.addr == int_fc.addr
        assert opt.name == "int32"
        assert opt.ranges.addr == ranges_1.addr

    def test_append_opt_invalid_ranges_signedness(self, def_tc, fc, invalid_ranges):
        sub_fc = def_tc.create_string_field_class()

        with pytest.raises(TypeError):
            fc.append_option("str", sub_fc, invalid_ranges)

    def test_iadd(self, def_tc, fc, ranges_1, ranges_2, ranges_3):
        a_fc = def_tc.create_single_precision_real_field_class()
        fc.append_option("a_float", a_fc, ranges_1)
        c_fc = def_tc.create_string_field_class()
        d_fc = def_tc.create_signed_enumeration_field_class(field_value_range=32)
        fc += [
            ("c_string", c_fc, ranges_2),
            ("d_enum", d_fc, ranges_3),
        ]
        assert fc["a_float"].field_class.addr == a_fc.addr
        assert fc["a_float"].name == "a_float"
        assert fc["a_float"].ranges == ranges_1
        assert fc["c_string"].field_class.addr == c_fc.addr
        assert fc["c_string"].name == "c_string"
        assert fc["c_string"].ranges == ranges_2
        assert fc["d_enum"].field_class.addr == d_fc.addr
        assert fc["d_enum"].name == "d_enum"
        assert fc["d_enum"].ranges == ranges_3

    @pytest.fixture
    def fc_for_field_path_test(self, def_tc, fc, sel_fc, ranges_1, ranges_2, ranges_3):
        fc.append_option(
            "a", def_tc.create_single_precision_real_field_class(), ranges_1
        )
        fc.append_option("b", def_tc.create_signed_integer_field_class(21), ranges_2)
        fc.append_option("c", def_tc.create_unsigned_integer_field_class(34), ranges_3)

        def_tc.create_stream_class(
            supports_packets=True,
            packet_context_field_class=def_tc.create_structure_field_class(
                members=(
                    ("foo", def_tc.create_single_precision_real_field_class()),
                    (
                        "inner_struct",
                        def_tc.create_static_array_field_class(
                            def_tc.create_structure_field_class(
                                members=(
                                    ("selector", sel_fc),
                                    ("bar", def_tc.create_string_field_class()),
                                    ("baz", def_tc.create_string_field_class()),
                                    ("variant", fc),
                                )
                            ),
                            2,
                        ),
                    ),
                )
            ),
        )

        return fc

    def test_sel_field_path_len(self, fc_for_field_path_test):
        assert len(fc_for_field_path_test.selector_field_path) == 3

    def test_sel_field_path_iter(self, fc_for_field_path_test):
        path_items = list(fc_for_field_path_test.selector_field_path)
        assert len(path_items) == 3
        assert isinstance(path_items[0], bt2._IndexFieldPathItem)
        assert path_items[0].index == 1
        assert isinstance(path_items[1], bt2._CurrentArrayElementFieldPathItem)
        assert isinstance(path_items[2], bt2._IndexFieldPathItem)
        assert path_items[2].index == 0

    def test_sel_field_path_root_scope(self, fc_for_field_path_test):
        assert (
            fc_for_field_path_test.selector_field_path.root_scope
            == bt2.FieldPathScope.PACKET_CONTEXT
        )

    def test_append_opt_invalid_name(self, def_tc, fc, ranges_1):
        sub_fc = def_tc.create_string_field_class()
        with pytest.raises(TypeError):
            fc.append_option(23, sub_fc, ranges_1)

    def test_append_opt_invalid_fc(self, fc, ranges_1):
        with pytest.raises(TypeError):
            fc.append_option("yes", object(), ranges_1)

    def test_append_opt_invalid_ranges(self, def_tc, fc):
        sub_fc = def_tc.create_string_field_class()
        with pytest.raises(TypeError):
            fc.append_option("yes", sub_fc, "lel")

    def test_append_opt_dup_name(self, def_tc, fc, ranges_1, ranges_2):
        sub_fc_1 = def_tc.create_string_field_class()
        sub_fc_2 = def_tc.create_string_field_class()

        with pytest.raises(ValueError):
            fc.append_option("yes", sub_fc_1, ranges_1)
            fc.append_option("yes", sub_fc_2, ranges_2)

    def test_opt_user_attrs(self, def_tc, fc, ranges_1):
        fc.append_option(
            "c",
            def_tc.create_string_field_class(),
            ranges_1,
            user_attributes={"salut": 23},
        )

        assert fc["c"].user_attributes == {"salut": 23}
        assert type(fc.user_attributes) is bt2.MapValue

    def test_const_opt_user_attrs(self, const_fc):
        assert type(const_fc.user_attributes) is bt2._MapValueConst

    def test_invalid_opt_user_attrs(self, def_tc, fc, ranges_1):
        with pytest.raises(TypeError):
            fc.append_option(
                "c",
                def_tc.create_string_field_class(),
                ranges_1,
                user_attributes=object(),
            )

    def test_invalid_opt_user_attrs_value_type(self, def_tc, fc, ranges_1):
        with pytest.raises(TypeError):
            fc.append_option(
                "c",
                def_tc.create_string_field_class(),
                ranges_1,
                user_attributes=23,
            )

    def test_const_iadd(self, def_tc, const_fc, ranges_1):
        a_fc = def_tc.create_single_precision_real_field_class()

        with pytest.raises(TypeError):
            const_fc += [("a_float", a_fc, ranges_1)]

    def test_bool_op(self, def_tc, fc, ranges_1):
        assert not fc
        fc.append_option("a", def_tc.create_string_field_class(), ranges_1)
        assert fc

    def test_len(self, def_tc, fc, ranges_1, ranges_2, ranges_3):
        fc.append_option("a", def_tc.create_string_field_class(), ranges_1)
        fc.append_option("b", def_tc.create_string_field_class(), ranges_2)
        fc.append_option("c", def_tc.create_string_field_class(), ranges_3)
        assert len(fc) == 3

    def test_getitem(self, def_tc, fc, ranges_1, ranges_2, ranges_3):
        a_fc = def_tc.create_signed_integer_field_class(32)
        b_fc = def_tc.create_string_field_class()
        c_fc = def_tc.create_single_precision_real_field_class()
        fc.append_option("a", a_fc, ranges_1)
        fc.append_option("b", b_fc, ranges_2)
        fc.append_option("c", c_fc, ranges_3)
        assert fc["b"].field_class.addr == b_fc.addr
        assert fc["b"].name == "b"
        assert fc["b"].ranges.addr == ranges_2.addr

    def test_opt_fc(self, def_tc, fc, ranges_1):
        a_fc = def_tc.create_signed_integer_field_class(32)
        fc.append_option("a", a_fc, ranges_1)
        assert type(fc["a"].field_class) is bt2._SignedIntegerFieldClass

    def test_opt_const_fc(self, const_fc):
        assert type(const_fc["a"].field_class) is bt2._SignedIntegerFieldClassConst

    def test_contains(self, def_tc, fc, ranges_1):
        assert "a" not in fc
        fc.append_option("a", def_tc.create_string_field_class(), ranges_1)
        assert "a" in fc

    def test_iter(self, def_tc, fc, ranges_1, ranges_2, ranges_3):
        a_fc = def_tc.create_signed_integer_field_class(32)
        b_fc = def_tc.create_string_field_class()
        c_fc = def_tc.create_single_precision_real_field_class()
        opts = (
            ("a", a_fc, ranges_1),
            ("b", b_fc, ranges_2),
            ("c", c_fc, ranges_3),
        )

        for opt in opts:
            fc.append_option(*opt)

        for (name, opt), test_opt in zip(fc.items(), opts):
            assert opt.name == test_opt[0]
            assert name == opt.name
            assert opt.field_class.addr == test_opt[1].addr
            assert opt.ranges.addr == test_opt[2].addr

    def test_at_index(self, def_tc, fc, ranges_1, ranges_2, ranges_3):
        a_fc = def_tc.create_signed_integer_field_class(32)
        b_fc = def_tc.create_string_field_class()
        c_fc = def_tc.create_single_precision_real_field_class()
        fc.append_option("c", c_fc, ranges_1)
        fc.append_option("a", a_fc, ranges_2)
        fc.append_option("b", b_fc, ranges_3)
        assert fc.option_at_index(1).field_class.addr == a_fc.addr
        assert fc.option_at_index(1).name == "a"
        assert fc.option_at_index(1).ranges.addr == ranges_2.addr

    def test_at_index_invalid(self, def_tc, fc, ranges_3):
        fc.append_option("c", def_tc.create_signed_integer_field_class(32), ranges_3)

        with pytest.raises(TypeError):
            fc.option_at_index("yes")

    def test_at_index_out_of_bounds(self, def_tc, fc, ranges_3):
        fc.append_option("c", def_tc.create_signed_integer_field_class(32), ranges_3)

        with pytest.raises(IndexError):
            fc.option_at_index(len(fc))

    def test_opt_with_name(self, def_tc, fc, ranges_1, ranges_2, ranges_3):
        a_fc = def_tc.create_signed_integer_field_class(32)
        fc.append_option(
            "c", def_tc.create_single_precision_real_field_class(), ranges_1
        )
        fc.append_option("a", a_fc, ranges_2)
        fc.append_option("b", def_tc.create_string_field_class(), ranges_3)
        assert fc.option_with_name("a").field_class.addr == a_fc.addr
        assert fc.option_with_name("a").name == "a"
        assert fc.option_with_name("a").ranges.addr == ranges_2.addr
        assert fc.option_with_name("bloup") is None


class TestVarWithUIntSel(_TestVarWithIntSel):
    @pytest.fixture
    def sel_fc(self, def_tc):
        return def_tc.create_unsigned_integer_field_class()

    @pytest.fixture
    def ranges_1(self):
        return bt2.UnsignedIntegerRangeSet([(1, 4), (18, 47)])

    @pytest.fixture
    def ranges_2(self):
        return bt2.UnsignedIntegerRangeSet([(5, 5)])

    @pytest.fixture
    def ranges_3(self):
        return bt2.UnsignedIntegerRangeSet([(8, 16), (48, 99)])

    @pytest.fixture
    def invalid_ranges(self):
        return bt2.SignedIntegerRangeSet([(-8, 16), (48, 99)])


class TestVarWithSIntSel(_TestVarWithIntSel):
    @pytest.fixture
    def sel_fc(self, def_tc):
        return def_tc.create_signed_integer_field_class()

    @pytest.fixture
    def ranges_1(self):
        return bt2.SignedIntegerRangeSet([(-10, -4), (18, 47)])

    @pytest.fixture
    def ranges_2(self):
        return bt2.SignedIntegerRangeSet([(-3, -3)])

    @pytest.fixture
    def ranges_3(self):
        return bt2.SignedIntegerRangeSet([(8, 16), (48, 99)])

    @pytest.fixture
    def invalid_ranges(self):
        return bt2.UnsignedIntegerRangeSet([(8, 16), (48, 99)])
