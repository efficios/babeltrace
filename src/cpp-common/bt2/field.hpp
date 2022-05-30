/*
 * Copyright (c) 2020 Philippe Proulx <pproulx@efficios.com>
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef BABELTRACE_CPP_COMMON_BT2_FIELD_HPP
#define BABELTRACE_CPP_COMMON_BT2_FIELD_HPP

#include <type_traits>
#include <cstdint>
#include <babeltrace2/babeltrace.h>

#include "common/assert.h"
#include "internal/borrowed-obj.hpp"
#include "internal/utils.hpp"
#include "cpp-common/optional.hpp"
#include "cpp-common/string_view.hpp"
#include "field-class.hpp"

namespace bt2 {

template <typename LibObjT>
class CommonBoolField;

template <typename LibObjT>
class CommonBitArrayField;

template <typename LibObjT>
class CommonUnsignedIntegerField;

template <typename LibObjT>
class CommonSignedIntegerField;

template <typename LibObjT>
class CommonUnsignedEnumerationField;

template <typename LibObjT>
class CommonSignedEnumerationField;

template <typename LibObjT>
class CommonSinglePrecisionRealField;

template <typename LibObjT>
class CommonDoublePrecisionRealField;

template <typename LibObjT>
class CommonStringField;

template <typename LibObjT>
class CommonStructureField;

template <typename LibObjT>
class CommonArrayField;

template <typename LibObjT>
class CommonDynamicArrayField;

template <typename LibObjT>
class CommonOptionField;

template <typename LibObjT>
class CommonVariantField;

namespace internal {

template <typename LibObjT>
struct CommonFieldSpec;

/* Functions specific to mutable fields */
template <>
struct CommonFieldSpec<bt_field> final
{
    static bt_field_class *cls(bt_field * const libObjPtr) noexcept
    {
        return bt_field_borrow_class(libObjPtr);
    }
};

/* Functions specific to constant fields */
template <>
struct CommonFieldSpec<const bt_field> final
{
    static const bt_field_class *cls(const bt_field * const libObjPtr) noexcept
    {
        return bt_field_borrow_class_const(libObjPtr);
    }
};

} /* namespace internal */

template <typename LibObjT>
class CommonField : public internal::BorrowedObj<LibObjT>
{
private:
    using typename internal::BorrowedObj<LibObjT>::_ThisBorrowedObj;

protected:
    using typename internal::BorrowedObj<LibObjT>::_LibObjPtr;
    using _ThisCommonField = CommonField<LibObjT>;

public:
    using Class =
        typename std::conditional<std::is_const<LibObjT>::value, ConstFieldClass, FieldClass>::type;

    explicit CommonField(const _LibObjPtr libObjPtr) noexcept : _ThisBorrowedObj {libObjPtr}
    {
    }

    template <typename OtherLibObjT>
    CommonField(const CommonField<OtherLibObjT> val) noexcept : _ThisBorrowedObj {val}
    {
    }

    template <typename OtherLibObjT>
    _ThisCommonField& operator=(const CommonField<OtherLibObjT> val) noexcept
    {
        _ThisBorrowedObj::operator=(val);
        return *this;
    }

    FieldClassType classType() const noexcept
    {
        return static_cast<FieldClassType>(bt_field_get_class_type(this->libObjPtr()));
    }

    ConstFieldClass cls() const noexcept
    {
        return ConstFieldClass {internal::CommonFieldSpec<const bt_field>::cls(this->libObjPtr())};
    }

    Class cls() noexcept
    {
        return Class {internal::CommonFieldSpec<LibObjT>::cls(this->libObjPtr())};
    }

    bool isBool() const noexcept
    {
        return this->cls().isBool();
    }

    bool isBitArray() const noexcept
    {
        return this->cls().isBitArray();
    }

    bool isUnsignedInteger() const noexcept
    {
        return this->cls().isUnsignedInteger();
    }

    bool isSignedInteger() const noexcept
    {
        return this->cls().isSignedInteger();
    }

    bool isUnsignedEnumeration() const noexcept
    {
        return this->cls().isUnsignedEnumeration();
    }

    bool isSignedEnumeration() const noexcept
    {
        return this->cls().isSignedEnumeration();
    }

    bool isSinglePrecisionReal() const noexcept
    {
        return this->cls().isSinglePrecisionReal();
    }

    bool isDoublePrecisionReal() const noexcept
    {
        return this->cls().isDoublePrecisionReal();
    }

    bool isString() const noexcept
    {
        return this->cls().isString();
    }

    bool isStructure() const noexcept
    {
        return this->cls().isStructure();
    }

    bool isArray() const noexcept
    {
        return this->cls().isArray();
    }

    bool isDynamicArray() const noexcept
    {
        return this->cls().isDynamicArray();
    }

    bool isOption() const noexcept
    {
        return this->cls().isOption();
    }

    bool isVariant() const noexcept
    {
        return this->cls().isVariant();
    }

    template <typename FieldT>
    FieldT as() const noexcept
    {
        return FieldT {this->libObjPtr()};
    }

    CommonBoolField<LibObjT> asBool() const noexcept;
    CommonBitArrayField<LibObjT> asBitArray() const noexcept;
    CommonUnsignedIntegerField<LibObjT> asUnsignedInteger() const noexcept;
    CommonSignedIntegerField<LibObjT> asSignedInteger() const noexcept;
    CommonUnsignedEnumerationField<LibObjT> asUnsignedEnumeration() const noexcept;
    CommonSignedEnumerationField<LibObjT> asSignedEnumeration() const noexcept;
    CommonSinglePrecisionRealField<LibObjT> asSinglePrecisionReal() const noexcept;
    CommonDoublePrecisionRealField<LibObjT> asDoublePrecisionReal() const noexcept;
    CommonStringField<LibObjT> asString() const noexcept;
    CommonStructureField<LibObjT> asStructure() const noexcept;
    CommonArrayField<LibObjT> asArray() const noexcept;
    CommonDynamicArrayField<LibObjT> asDynamicArray() const noexcept;
    CommonOptionField<LibObjT> asOption() const noexcept;
    CommonVariantField<LibObjT> asVariant() const noexcept;
};

using Field = CommonField<bt_field>;
using ConstField = CommonField<const bt_field>;

namespace internal {

struct FieldTypeDescr
{
    using Const = ConstField;
    using NonConst = Field;
};

template <>
struct TypeDescr<Field> : public FieldTypeDescr
{
};

template <>
struct TypeDescr<ConstField> : public FieldTypeDescr
{
};

} /* namespace internal */

template <typename LibObjT>
class CommonBoolField final : public CommonField<LibObjT>
{
private:
    using typename CommonField<LibObjT>::_LibObjPtr;
    using typename CommonField<LibObjT>::_ThisCommonField;

public:
    using Value = bool;

    explicit CommonBoolField(const _LibObjPtr libObjPtr) noexcept : _ThisCommonField {libObjPtr}
    {
        BT_ASSERT_DBG(this->isBool());
    }

    template <typename OtherLibObjT>
    CommonBoolField(const CommonBoolField<OtherLibObjT> val) noexcept : _ThisCommonField {val}
    {
    }

    template <typename OtherLibObjT>
    CommonBoolField<LibObjT>& operator=(const CommonBoolField<OtherLibObjT> val) noexcept
    {
        _ThisCommonField::operator=(val);
        return *this;
    }

    CommonBoolField<LibObjT>& operator=(const Value val) noexcept
    {
        static_assert(!std::is_const<LibObjT>::value, "`LibObjT` must NOT be `const`.");

        bt_field_bool_set_value(this->libObjPtr(), static_cast<bt_bool>(val));
        return *this;
    }

    Value value() const noexcept
    {
        return static_cast<Value>(bt_field_bool_get_value(this->libObjPtr()));
    }

    operator Value() const noexcept
    {
        return this->value();
    }
};

using BoolField = CommonBoolField<bt_field>;
using ConstBoolField = CommonBoolField<const bt_field>;

namespace internal {

struct BoolFieldTypeDescr
{
    using Const = ConstBoolField;
    using NonConst = BoolField;
};

template <>
struct TypeDescr<BoolField> : public BoolFieldTypeDescr
{
};

template <>
struct TypeDescr<ConstBoolField> : public BoolFieldTypeDescr
{
};

} /* namespace internal */

template <typename LibObjT>
class CommonBitArrayField final : public CommonField<LibObjT>
{
private:
    using typename CommonField<LibObjT>::_LibObjPtr;
    using typename CommonField<LibObjT>::_ThisCommonField;

public:
    using Class = typename std::conditional<std::is_const<LibObjT>::value, ConstBitArrayFieldClass,
                                            BitArrayFieldClass>::type;

    explicit CommonBitArrayField(const _LibObjPtr libObjPtr) noexcept : _ThisCommonField {libObjPtr}
    {
        BT_ASSERT_DBG(this->isBitArray());
    }

    template <typename OtherLibObjT>
    CommonBitArrayField(const CommonBitArrayField<OtherLibObjT> val) noexcept :
        _ThisCommonField {val}
    {
    }

    template <typename OtherLibObjT>
    CommonBitArrayField<LibObjT>& operator=(const CommonBitArrayField<OtherLibObjT> val) noexcept
    {
        _ThisCommonField::operator=(val);
        return *this;
    }

    ConstBitArrayFieldClass cls() const noexcept
    {
        return ConstBitArrayFieldClass {
            internal::CommonFieldSpec<const bt_field>::cls(this->libObjPtr())};
    }

    Class cls() noexcept
    {
        return Class {internal::CommonFieldSpec<LibObjT>::cls(this->libObjPtr())};
    }

    CommonBitArrayField<LibObjT>& operator=(const std::uint64_t bits) noexcept
    {
        static_assert(!std::is_const<LibObjT>::value, "`LibObjT` must NOT be `const`.");

        bt_field_bit_array_set_value_as_integer(this->libObjPtr(), bits);
        return *this;
    }

    std::uint64_t valueAsInteger() const noexcept
    {
        return bt_field_bit_array_get_value_as_integer(this->libObjPtr());
    }

    bool bitValue(const std::uint64_t index) const noexcept
    {
        BT_ASSERT_DBG(index < this->cls().length());
        return static_cast<bool>(this->valueAsInteger() & (1ULL << index));
    }
};

using BitArrayField = CommonBitArrayField<bt_field>;
using ConstBitArrayField = CommonBitArrayField<const bt_field>;

namespace internal {

struct BitArrayFieldTypeDescr
{
    using Const = ConstBitArrayField;
    using NonConst = BitArrayField;
};

template <>
struct TypeDescr<BitArrayField> : public BitArrayFieldTypeDescr
{
};

template <>
struct TypeDescr<ConstBitArrayField> : public BitArrayFieldTypeDescr
{
};

} /* namespace internal */

template <typename LibObjT>
class CommonUnsignedIntegerField final : public CommonField<LibObjT>
{
private:
    using typename CommonField<LibObjT>::_ThisCommonField;

protected:
    using typename CommonField<LibObjT>::_LibObjPtr;
    using _ThisCommonUnsignedIntegerField = CommonUnsignedIntegerField<LibObjT>;

public:
    using Value = std::uint64_t;

    using Class = typename std::conditional<std::is_const<LibObjT>::value, ConstIntegerFieldClass,
                                            IntegerFieldClass>::type;

    explicit CommonUnsignedIntegerField(const _LibObjPtr libObjPtr) noexcept :
        _ThisCommonField {libObjPtr}
    {
        BT_ASSERT_DBG(this->isUnsignedInteger());
    }

    template <typename OtherLibObjT>
    CommonUnsignedIntegerField(const CommonUnsignedIntegerField<OtherLibObjT> val) noexcept :
        _ThisCommonField {val}
    {
    }

    template <typename OtherLibObjT>
    _ThisCommonUnsignedIntegerField&
    operator=(const CommonUnsignedIntegerField<OtherLibObjT> val) noexcept
    {
        _ThisCommonField::operator=(val);
        return *this;
    }

    ConstIntegerFieldClass cls() const noexcept
    {
        return ConstIntegerFieldClass {
            internal::CommonFieldSpec<const bt_field>::cls(this->libObjPtr())};
    }

    Class cls() noexcept
    {
        return Class {internal::CommonFieldSpec<LibObjT>::cls(this->libObjPtr())};
    }

    CommonUnsignedIntegerField<LibObjT>& operator=(const Value val) noexcept
    {
        static_assert(!std::is_const<LibObjT>::value, "`LibObjT` must NOT be `const`.");

        bt_field_integer_unsigned_set_value(this->libObjPtr(), val);
        return *this;
    }

    Value value() const noexcept
    {
        return bt_field_integer_unsigned_get_value(this->libObjPtr());
    }

    operator Value() const noexcept
    {
        return this->value();
    }
};

using UnsignedIntegerField = CommonUnsignedIntegerField<bt_field>;
using ConstUnsignedIntegerField = CommonUnsignedIntegerField<const bt_field>;

namespace internal {

struct UnsignedIntegerFieldTypeDescr
{
    using Const = ConstUnsignedIntegerField;
    using NonConst = UnsignedIntegerField;
};

template <>
struct TypeDescr<UnsignedIntegerField> : public UnsignedIntegerFieldTypeDescr
{
};

template <>
struct TypeDescr<ConstUnsignedIntegerField> : public UnsignedIntegerFieldTypeDescr
{
};

} /* namespace internal */

template <typename LibObjT>
class CommonSignedIntegerField final : public CommonField<LibObjT>
{
private:
    using typename CommonField<LibObjT>::_ThisCommonField;

protected:
    using typename CommonField<LibObjT>::_LibObjPtr;
    using _ThisCommonSignedIntegerField = CommonSignedIntegerField<LibObjT>;

public:
    using Value = std::uint64_t;

    using Class = typename std::conditional<std::is_const<LibObjT>::value, ConstIntegerFieldClass,
                                            IntegerFieldClass>::type;

    explicit CommonSignedIntegerField(const _LibObjPtr libObjPtr) noexcept :
        _ThisCommonField {libObjPtr}
    {
        BT_ASSERT_DBG(this->isSignedInteger());
    }

    template <typename OtherLibObjT>
    CommonSignedIntegerField(const CommonSignedIntegerField<OtherLibObjT> val) noexcept :
        _ThisCommonField {val}
    {
    }

    template <typename OtherLibObjT>
    _ThisCommonSignedIntegerField&
    operator=(const CommonSignedIntegerField<OtherLibObjT> val) noexcept
    {
        _ThisCommonField::operator=(val);
        return *this;
    }

    ConstIntegerFieldClass cls() const noexcept
    {
        return ConstIntegerFieldClass {
            internal::CommonFieldSpec<const bt_field>::cls(this->libObjPtr())};
    }

    Class cls() noexcept
    {
        return Class {internal::CommonFieldSpec<LibObjT>::cls(this->libObjPtr())};
    }

    CommonSignedIntegerField<LibObjT>& operator=(const Value val) noexcept
    {
        static_assert(!std::is_const<LibObjT>::value, "`LibObjT` must NOT be `const`.");

        bt_field_integer_signed_set_value(this->libObjPtr(), val);
        return *this;
    }

    Value value() const noexcept
    {
        return bt_field_integer_signed_get_value(this->libObjPtr());
    }

    operator Value() const noexcept
    {
        return this->value();
    }
};

using SignedIntegerField = CommonSignedIntegerField<bt_field>;
using ConstSignedIntegerField = CommonSignedIntegerField<const bt_field>;

namespace internal {

struct SignedIntegerFieldTypeDescr
{
    using Const = ConstSignedIntegerField;
    using NonConst = SignedIntegerField;
};

template <>
struct TypeDescr<SignedIntegerField> : public SignedIntegerFieldTypeDescr
{
};

template <>
struct TypeDescr<ConstSignedIntegerField> : public SignedIntegerFieldTypeDescr
{
};

} /* namespace internal */

class EnumerationFieldClassMappingLabels
{
public:
    explicit EnumerationFieldClassMappingLabels(
        const bt_field_class_enumeration_mapping_label_array labels, const std::uint64_t size) :
        _mLabels {labels},
        _mSize {size}
    {
    }

    EnumerationFieldClassMappingLabels(const EnumerationFieldClassMappingLabels&) noexcept =
        default;

    EnumerationFieldClassMappingLabels&
    operator=(const EnumerationFieldClassMappingLabels&) noexcept = default;

    std::uint64_t size() const noexcept
    {
        return _mSize;
    }

    bpstd::string_view operator[](const std::uint64_t index) const noexcept
    {
        return _mLabels[index];
    }

private:
    bt_field_class_enumeration_mapping_label_array _mLabels;
    std::uint64_t _mSize;
};

template <typename LibObjT>
class CommonUnsignedEnumerationField final : public CommonUnsignedIntegerField<LibObjT>
{
private:
    using typename CommonUnsignedIntegerField<LibObjT>::_ThisCommonUnsignedIntegerField;
    using typename CommonField<LibObjT>::_LibObjPtr;

public:
    using Class =
        typename std::conditional<std::is_const<LibObjT>::value, ConstUnsignedEnumerationFieldClass,
                                  UnsignedEnumerationFieldClass>::type;

    explicit CommonUnsignedEnumerationField(const _LibObjPtr libObjPtr) noexcept :
        _ThisCommonUnsignedIntegerField {libObjPtr}
    {
        BT_ASSERT_DBG(this->isUnsignedEnumeration());
    }

    template <typename OtherLibObjT>
    CommonUnsignedEnumerationField(const CommonUnsignedEnumerationField<OtherLibObjT> val) noexcept
        :
        _ThisCommonUnsignedIntegerField {val}
    {
    }

    template <typename OtherLibObjT>
    CommonUnsignedEnumerationField<LibObjT>&
    operator=(const CommonUnsignedEnumerationField<OtherLibObjT> val) noexcept
    {
        _ThisCommonUnsignedIntegerField::operator=(val);
        return *this;
    }

    ConstUnsignedEnumerationFieldClass cls() const noexcept
    {
        return ConstUnsignedEnumerationFieldClass {
            internal::CommonFieldSpec<const bt_field>::cls(this->libObjPtr())};
    }

    Class cls() noexcept
    {
        return Class {internal::CommonFieldSpec<LibObjT>::cls(this->libObjPtr())};
    }

    EnumerationFieldClassMappingLabels labels() const
    {
        bt_field_class_enumeration_mapping_label_array labelArray;
        std::uint64_t count;
        const auto status = bt_field_enumeration_unsigned_get_mapping_labels(this->libObjPtr(),
                                                                             &labelArray, &count);

        if (status == BT_FIELD_ENUMERATION_GET_MAPPING_LABELS_STATUS_MEMORY_ERROR) {
            throw MemoryError {};
        }

        return EnumerationFieldClassMappingLabels {labelArray, count};
    }
};

using UnsignedEnumerationField = CommonUnsignedEnumerationField<bt_field>;
using ConstUnsignedEnumerationField = CommonUnsignedEnumerationField<const bt_field>;

namespace internal {

struct UnsignedEnumerationFieldTypeDescr
{
    using Const = ConstUnsignedEnumerationField;
    using NonConst = UnsignedEnumerationField;
};

template <>
struct TypeDescr<UnsignedEnumerationField> : public UnsignedEnumerationFieldTypeDescr
{
};

template <>
struct TypeDescr<ConstUnsignedEnumerationField> : public UnsignedEnumerationFieldTypeDescr
{
};

} /* namespace internal */

template <typename LibObjT>
class CommonSignedEnumerationField final : public CommonSignedIntegerField<LibObjT>
{
private:
    using typename CommonSignedIntegerField<LibObjT>::_ThisCommonSignedIntegerField;
    using typename CommonField<LibObjT>::_LibObjPtr;

public:
    using Class =
        typename std::conditional<std::is_const<LibObjT>::value, ConstSignedEnumerationFieldClass,
                                  SignedEnumerationFieldClass>::type;

    explicit CommonSignedEnumerationField(const _LibObjPtr libObjPtr) noexcept :
        _ThisCommonSignedIntegerField {libObjPtr}
    {
        BT_ASSERT_DBG(this->isSignedEnumeration());
    }

    template <typename OtherLibObjT>
    CommonSignedEnumerationField(const CommonSignedEnumerationField<OtherLibObjT> val) noexcept :
        _ThisCommonSignedIntegerField {val}
    {
    }

    template <typename OtherLibObjT>
    CommonSignedEnumerationField<LibObjT>&
    operator=(const CommonSignedEnumerationField<OtherLibObjT> val) noexcept
    {
        _ThisCommonSignedIntegerField::operator=(val);
        return *this;
    }

    ConstSignedEnumerationFieldClass cls() const noexcept
    {
        return ConstSignedEnumerationFieldClass {
            internal::CommonFieldSpec<const bt_field>::cls(this->libObjPtr())};
    }

    Class cls() noexcept
    {
        return Class {internal::CommonFieldSpec<LibObjT>::cls(this->libObjPtr())};
    }

    EnumerationFieldClassMappingLabels labels() const
    {
        bt_field_class_enumeration_mapping_label_array labelArray;
        std::uint64_t count;
        const auto status =
            bt_field_enumeration_signed_get_mapping_labels(this->libObjPtr(), &labelArray, &count);

        if (status == BT_FIELD_ENUMERATION_GET_MAPPING_LABELS_STATUS_MEMORY_ERROR) {
            throw MemoryError {};
        }

        return EnumerationFieldClassMappingLabels {labelArray, count};
    }
};

using SignedEnumerationField = CommonSignedEnumerationField<bt_field>;
using ConstSignedEnumerationField = CommonSignedEnumerationField<const bt_field>;

namespace internal {

struct SignedEnumerationFieldTypeDescr
{
    using Const = ConstSignedEnumerationField;
    using NonConst = SignedEnumerationField;
};

template <>
struct TypeDescr<SignedEnumerationField> : public SignedEnumerationFieldTypeDescr
{
};

template <>
struct TypeDescr<ConstSignedEnumerationField> : public SignedEnumerationFieldTypeDescr
{
};

} /* namespace internal */

template <typename LibObjT>
class CommonSinglePrecisionRealField final : public CommonField<LibObjT>
{
private:
    using typename CommonField<LibObjT>::_LibObjPtr;
    using typename CommonField<LibObjT>::_ThisCommonField;

public:
    using Value = float;

    explicit CommonSinglePrecisionRealField(const _LibObjPtr libObjPtr) noexcept :
        _ThisCommonField {libObjPtr}
    {
        BT_ASSERT_DBG(this->isSinglePrecisionReal());
    }

    template <typename OtherLibObjT>
    CommonSinglePrecisionRealField(const CommonSinglePrecisionRealField<OtherLibObjT> val) noexcept
        :
        _ThisCommonField {val}
    {
    }

    template <typename OtherLibObjT>
    CommonSinglePrecisionRealField<LibObjT>&
    operator=(const CommonSinglePrecisionRealField<OtherLibObjT> val) noexcept
    {
        _ThisCommonField::operator=(val);
        return *this;
    }

    CommonSinglePrecisionRealField<LibObjT>& operator=(const Value val) noexcept
    {
        static_assert(!std::is_const<LibObjT>::value, "`LibObjT` must NOT be `const`.");

        bt_field_real_single_precision_set_value(this->libObjPtr(), val);
        return *this;
    }

    Value value() const noexcept
    {
        return bt_field_real_single_precision_get_value(this->libObjPtr());
    }

    operator Value() const noexcept
    {
        return this->value();
    }
};

using SinglePrecisionRealField = CommonSinglePrecisionRealField<bt_field>;
using ConstSinglePrecisionRealField = CommonSinglePrecisionRealField<const bt_field>;

namespace internal {

struct SinglePrecisionRealFieldTypeDescr
{
    using Const = ConstSinglePrecisionRealField;
    using NonConst = SinglePrecisionRealField;
};

template <>
struct TypeDescr<SinglePrecisionRealField> : public SinglePrecisionRealFieldTypeDescr
{
};

template <>
struct TypeDescr<ConstSinglePrecisionRealField> : public SinglePrecisionRealFieldTypeDescr
{
};

} /* namespace internal */

template <typename LibObjT>
class CommonDoublePrecisionRealField final : public CommonField<LibObjT>
{
private:
    using typename CommonField<LibObjT>::_LibObjPtr;
    using typename CommonField<LibObjT>::_ThisCommonField;

public:
    using Value = double;

    explicit CommonDoublePrecisionRealField(const _LibObjPtr libObjPtr) noexcept :
        _ThisCommonField {libObjPtr}
    {
        BT_ASSERT_DBG(this->isDoublePrecisionReal());
    }

    template <typename OtherLibObjT>
    CommonDoublePrecisionRealField(const CommonDoublePrecisionRealField<OtherLibObjT> val) noexcept
        :
        _ThisCommonField {val}
    {
    }

    template <typename OtherLibObjT>
    CommonDoublePrecisionRealField<LibObjT>&
    operator=(const CommonDoublePrecisionRealField<OtherLibObjT> val) noexcept
    {
        _ThisCommonField::operator=(val);
        return *this;
    }

    CommonDoublePrecisionRealField<LibObjT>& operator=(const Value val) noexcept
    {
        static_assert(!std::is_const<LibObjT>::value, "`LibObjT` must NOT be `const`.");

        bt_field_real_single_precision_set_value(this->libObjPtr(), val);
        return *this;
    }

    Value value() const noexcept
    {
        return bt_field_real_single_precision_get_value(this->libObjPtr());
    }

    operator Value() const noexcept
    {
        return this->value();
    }
};

using DoublePrecisionRealField = CommonDoublePrecisionRealField<bt_field>;
using ConstDoublePrecisionRealField = CommonDoublePrecisionRealField<const bt_field>;

namespace internal {

struct DoublePrecisionRealFieldTypeDescr
{
    using Const = ConstDoublePrecisionRealField;
    using NonConst = DoublePrecisionRealField;
};

template <>
struct TypeDescr<DoublePrecisionRealField> : public DoublePrecisionRealFieldTypeDescr
{
};

template <>
struct TypeDescr<ConstDoublePrecisionRealField> : public DoublePrecisionRealFieldTypeDescr
{
};

} /* namespace internal */

template <typename LibObjT>
class CommonStringField final : public CommonField<LibObjT>
{
private:
    using typename CommonField<LibObjT>::_LibObjPtr;
    using typename CommonField<LibObjT>::_ThisCommonField;

public:
    explicit CommonStringField(const _LibObjPtr libObjPtr) noexcept : _ThisCommonField {libObjPtr}
    {
        BT_ASSERT_DBG(this->isString());
    }

    template <typename OtherLibObjT>
    CommonStringField(const CommonStringField<OtherLibObjT> val) noexcept : _ThisCommonField {val}
    {
    }

    template <typename OtherLibObjT>
    CommonStringField<LibObjT>& operator=(const CommonStringField<OtherLibObjT> val) noexcept
    {
        _ThisCommonField::operator=(val);
        return *this;
    }

    CommonStringField<LibObjT>& operator=(const char * const val) noexcept
    {
        static_assert(!std::is_const<LibObjT>::value, "`LibObjT` must NOT be `const`.");

        const auto status = bt_field_string_set_value(this->libObjPtr(), val);

        if (status == BT_FIELD_STRING_SET_VALUE_STATUS_MEMORY_ERROR) {
            throw MemoryError {};
        }

        return *this;
    }

    CommonStringField<LibObjT>& operator=(const std::string& val) noexcept
    {
        return *this = val.data();
    }

    void clear() noexcept
    {
        static_assert(!std::is_const<LibObjT>::value, "`LibObjT` must NOT be `const`.");

        bt_field_string_clear(this->libObjPtr());
    }

    bpstd::string_view value() const noexcept
    {
        return bt_field_string_get_value(this->libObjPtr());
    }
};

using StringField = CommonStringField<bt_field>;
using ConstStringField = CommonStringField<const bt_field>;

namespace internal {

struct StringFieldTypeDescr
{
    using Const = ConstStringField;
    using NonConst = StringField;
};

template <>
struct TypeDescr<StringField> : public StringFieldTypeDescr
{
};

template <>
struct TypeDescr<ConstStringField> : public StringFieldTypeDescr
{
};

template <typename LibObjT>
struct CommonStructureFieldSpec;

/* Functions specific to mutable structure fields */
template <>
struct CommonStructureFieldSpec<bt_field> final
{
    static bt_field *memberFieldByIndex(bt_field * const libObjPtr,
                                        const std::uint64_t index) noexcept
    {
        return bt_field_structure_borrow_member_field_by_index(libObjPtr, index);
    }

    static bt_field *memberFieldByName(bt_field * const libObjPtr, const char * const name) noexcept
    {
        return bt_field_structure_borrow_member_field_by_name(libObjPtr, name);
    }
};

/* Functions specific to constant structure fields */
template <>
struct CommonStructureFieldSpec<const bt_field> final
{
    static const bt_field *memberFieldByIndex(const bt_field * const libObjPtr,
                                              const std::uint64_t index) noexcept
    {
        return bt_field_structure_borrow_member_field_by_index_const(libObjPtr, index);
    }

    static const bt_field *memberFieldByName(const bt_field * const libObjPtr,
                                             const char * const name) noexcept
    {
        return bt_field_structure_borrow_member_field_by_name_const(libObjPtr, name);
    }
};

} /* namespace internal */

template <typename LibObjT>
class CommonStructureField final : public CommonField<LibObjT>
{
private:
    using typename CommonField<LibObjT>::_LibObjPtr;
    using typename CommonField<LibObjT>::_ThisCommonField;
    using _Spec = internal::CommonStructureFieldSpec<LibObjT>;

public:
    using Class = typename std::conditional<std::is_const<LibObjT>::value, ConstStructureFieldClass,
                                            StructureFieldClass>::type;

    explicit CommonStructureField(const _LibObjPtr libObjPtr) noexcept :
        _ThisCommonField {libObjPtr}
    {
        BT_ASSERT_DBG(this->isStructure());
    }

    template <typename OtherLibObjT>
    CommonStructureField(const CommonStructureField<OtherLibObjT> val) noexcept :
        _ThisCommonField {val}
    {
    }

    template <typename OtherLibObjT>
    CommonStructureField<LibObjT>& operator=(const CommonStructureField<OtherLibObjT> val) noexcept
    {
        _ThisCommonField::operator=(val);
        return *this;
    }

    ConstStructureFieldClass cls() const noexcept
    {
        return ConstStructureFieldClass {
            internal::CommonFieldSpec<const bt_field>::cls(this->libObjPtr())};
    }

    Class cls() noexcept
    {
        return Class {internal::CommonFieldSpec<LibObjT>::cls(this->libObjPtr())};
    }

    std::uint64_t size() const noexcept
    {
        return this->cls().size();
    }

    ConstField operator[](const std::uint64_t index) const noexcept
    {
        return ConstField {internal::CommonStructureFieldSpec<const bt_field>::memberFieldByIndex(
            this->libObjPtr(), index)};
    }

    CommonField<LibObjT> operator[](const std::uint64_t index) noexcept
    {
        return CommonField<LibObjT> {_Spec::memberFieldByIndex(this->libObjPtr(), index)};
    }

    nonstd::optional<ConstField> operator[](const char * const name) const noexcept
    {
        const auto libObjPtr =
            internal::CommonStructureFieldSpec<const bt_field>::memberFieldByName(this->libObjPtr(),
                                                                                  name);

        if (libObjPtr) {
            return ConstField {libObjPtr};
        }

        return nonstd::nullopt;
    }

    nonstd::optional<ConstField> operator[](const std::string& name) const noexcept
    {
        return (*this)[name.data()];
    }

    nonstd::optional<CommonField<LibObjT>> operator[](const char * const name) noexcept
    {
        const auto libObjPtr = _Spec::memberFieldByName(this->libObjPtr(), name);

        if (libObjPtr) {
            return CommonField<LibObjT> {libObjPtr};
        }

        return nonstd::nullopt;
    }

    nonstd::optional<CommonField<LibObjT>> operator[](const std::string& name) noexcept
    {
        return (*this)[name.data()];
    }
};

using StructureField = CommonStructureField<bt_field>;
using ConstStructureField = CommonStructureField<const bt_field>;

namespace internal {

struct StructureFieldTypeDescr
{
    using Const = ConstStructureField;
    using NonConst = StructureField;
};

template <>
struct TypeDescr<StructureField> : public StructureFieldTypeDescr
{
};

template <>
struct TypeDescr<ConstStructureField> : public StructureFieldTypeDescr
{
};

template <typename LibObjT>
struct CommonArrayFieldSpec;

/* Functions specific to mutable array fields */
template <>
struct CommonArrayFieldSpec<bt_field> final
{
    static bt_field *elementFieldByIndex(bt_field * const libObjPtr,
                                         const std::uint64_t index) noexcept
    {
        return bt_field_array_borrow_element_field_by_index(libObjPtr, index);
    }
};

/* Functions specific to constant array fields */
template <>
struct CommonArrayFieldSpec<const bt_field> final
{
    static const bt_field *elementFieldByIndex(const bt_field * const libObjPtr,
                                               const std::uint64_t index) noexcept
    {
        return bt_field_array_borrow_element_field_by_index_const(libObjPtr, index);
    }
};

} /* namespace internal */

template <typename LibObjT>
class CommonArrayField : public CommonField<LibObjT>
{
private:
    using typename CommonField<LibObjT>::_ThisCommonField;
    using _Spec = internal::CommonArrayFieldSpec<LibObjT>;

protected:
    using typename CommonField<LibObjT>::_LibObjPtr;
    using _ThisCommonArrayField = CommonArrayField<LibObjT>;

public:
    using Class = typename std::conditional<std::is_const<LibObjT>::value, ConstArrayFieldClass,
                                            ArrayFieldClass>::type;

    explicit CommonArrayField(const _LibObjPtr libObjPtr) noexcept : _ThisCommonField {libObjPtr}
    {
        BT_ASSERT_DBG(this->isArray());
    }

    template <typename OtherLibObjT>
    CommonArrayField(const CommonArrayField<OtherLibObjT> val) noexcept : _ThisCommonField {val}
    {
    }

    template <typename OtherLibObjT>
    _ThisCommonArrayField& operator=(const CommonArrayField<OtherLibObjT> val) noexcept
    {
        _ThisCommonField::operator=(val);
        return *this;
    }

    ConstArrayFieldClass cls() const noexcept
    {
        return ConstArrayFieldClass {
            internal::CommonFieldSpec<const bt_field>::cls(this->libObjPtr())};
    }

    Class cls() noexcept
    {
        return Class {internal::CommonFieldSpec<LibObjT>::cls(this->libObjPtr())};
    }

    std::uint64_t length() const noexcept
    {
        return bt_field_array_get_length(this->libObjPtr());
    }

    ConstField operator[](const std::uint64_t index) const noexcept
    {
        return ConstField {internal::CommonArrayFieldSpec<const bt_field>::elementFieldByIndex(
            this->libObjPtr(), index)};
    }

    CommonField<LibObjT> operator[](const std::uint64_t index) noexcept
    {
        return CommonField<LibObjT> {_Spec::elementFieldByIndex(this->libObjPtr(), index)};
    }
};

using ArrayField = CommonArrayField<bt_field>;
using ConstArrayField = CommonArrayField<const bt_field>;

namespace internal {

struct ArrayFieldTypeDescr
{
    using Const = ConstArrayField;
    using NonConst = ArrayField;
};

template <>
struct TypeDescr<ArrayField> : public ArrayFieldTypeDescr
{
};

template <>
struct TypeDescr<ConstArrayField> : public ArrayFieldTypeDescr
{
};

} /* namespace internal */

template <typename LibObjT>
class CommonDynamicArrayField : public CommonArrayField<LibObjT>
{
private:
    using typename CommonField<LibObjT>::_LibObjPtr;
    using typename CommonArrayField<LibObjT>::_ThisCommonArrayField;

public:
    explicit CommonDynamicArrayField(const _LibObjPtr libObjPtr) noexcept :
        _ThisCommonArrayField {libObjPtr}
    {
        BT_ASSERT_DBG(this->isDynamicArray());
    }

    template <typename OtherLibObjT>
    CommonDynamicArrayField(const CommonDynamicArrayField<OtherLibObjT> val) noexcept :
        _ThisCommonArrayField {val}
    {
    }

    template <typename OtherLibObjT>
    CommonDynamicArrayField<LibObjT>&
    operator=(const CommonDynamicArrayField<OtherLibObjT> val) noexcept
    {
        _ThisCommonArrayField::operator=(val);
        return *this;
    }

    std::uint64_t length() const noexcept
    {
        return _ThisCommonArrayField::length();
    }

    void length(const std::uint64_t length)
    {
        static_assert(!std::is_const<LibObjT>::value, "`LibObjT` must NOT be `const`.");

        const auto status = bt_field_array_dynamic_set_length(this->libObjPtr(), length);

        if (status == BT_FIELD_DYNAMIC_ARRAY_SET_LENGTH_STATUS_MEMORY_ERROR) {
            throw MemoryError {};
        }
    }
};

using DynamicArrayField = CommonDynamicArrayField<bt_field>;
using ConstDynamicArrayField = CommonDynamicArrayField<const bt_field>;

namespace internal {

struct DynamicArrayFieldTypeDescr
{
    using Const = ConstDynamicArrayField;
    using NonConst = DynamicArrayField;
};

template <>
struct TypeDescr<DynamicArrayField> : public DynamicArrayFieldTypeDescr
{
};

template <>
struct TypeDescr<ConstDynamicArrayField> : public DynamicArrayFieldTypeDescr
{
};

template <typename LibObjT>
struct CommonOptionFieldSpec;

/* Functions specific to mutable option fields */
template <>
struct CommonOptionFieldSpec<bt_field> final
{
    static bt_field *field(bt_field * const libObjPtr) noexcept
    {
        return bt_field_option_borrow_field(libObjPtr);
    }
};

/* Functions specific to constant option fields */
template <>
struct CommonOptionFieldSpec<const bt_field> final
{
    static const bt_field *field(const bt_field * const libObjPtr) noexcept
    {
        return bt_field_option_borrow_field_const(libObjPtr);
    }
};

} /* namespace internal */

template <typename LibObjT>
class CommonOptionField : public CommonField<LibObjT>
{
private:
    using typename CommonField<LibObjT>::_LibObjPtr;
    using typename CommonField<LibObjT>::_ThisCommonField;
    using _Spec = internal::CommonOptionFieldSpec<LibObjT>;

public:
    using Class = typename std::conditional<std::is_const<LibObjT>::value, ConstOptionFieldClass,
                                            OptionFieldClass>::type;

    explicit CommonOptionField(const _LibObjPtr libObjPtr) noexcept : _ThisCommonField {libObjPtr}
    {
        BT_ASSERT_DBG(this->isOption());
    }

    template <typename OtherLibObjT>
    CommonOptionField(const CommonOptionField<OtherLibObjT> val) noexcept : _ThisCommonField {val}
    {
    }

    template <typename OtherLibObjT>
    CommonOptionField<LibObjT>& operator=(const CommonOptionField<OtherLibObjT> val) noexcept
    {
        _ThisCommonField::operator=(val);
        return *this;
    }

    ConstOptionFieldClass cls() const noexcept
    {
        return ConstOptionFieldClass {
            internal::CommonFieldSpec<const bt_field>::cls(this->libObjPtr())};
    }

    Class cls() noexcept
    {
        return Class {internal::CommonFieldSpec<LibObjT>::cls(this->libObjPtr())};
    }

    void hasField(const bool hasField) noexcept
    {
        static_assert(!std::is_const<LibObjT>::value, "`LibObjT` must NOT be `const`.");

        bt_field_option_set_has_field(this->libObjPtr(), static_cast<bt_bool>(hasField));
    }

    bool hasField() const noexcept
    {
        return this->field();
    }

    nonstd::optional<ConstField> field() const noexcept
    {
        const auto libObjPtr =
            internal::CommonOptionFieldSpec<const bt_field>::field(this->libObjPtr());

        if (libObjPtr) {
            return ConstField {libObjPtr};
        }

        return nonstd::nullopt;
    }

    nonstd::optional<CommonField<LibObjT>> field() noexcept
    {
        const auto libObjPtr = _Spec::field(this->libObjPtr());

        if (libObjPtr) {
            return CommonField<LibObjT> {libObjPtr};
        }

        return nonstd::nullopt;
    }
};

using OptionField = CommonOptionField<bt_field>;
using ConstOptionField = CommonOptionField<const bt_field>;

namespace internal {

struct OptionFieldTypeDescr
{
    using Const = ConstOptionField;
    using NonConst = OptionField;
};

template <>
struct TypeDescr<OptionField> : public OptionFieldTypeDescr
{
};

template <>
struct TypeDescr<ConstOptionField> : public OptionFieldTypeDescr
{
};

template <typename LibObjT>
struct CommonVariantFieldSpec;

/* Functions specific to mutable variant fields */
template <>
struct CommonVariantFieldSpec<bt_field> final
{
    static bt_field *selectedOptionField(bt_field * const libObjPtr) noexcept
    {
        return bt_field_variant_borrow_selected_option_field(libObjPtr);
    }
};

/* Functions specific to constant variant fields */
template <>
struct CommonVariantFieldSpec<const bt_field> final
{
    static const bt_field *selectedOptionField(const bt_field * const libObjPtr) noexcept
    {
        return bt_field_variant_borrow_selected_option_field_const(libObjPtr);
    }
};

} /* namespace internal */

template <typename LibObjT>
class CommonVariantField : public CommonField<LibObjT>
{
private:
    using typename CommonField<LibObjT>::_LibObjPtr;
    using typename CommonField<LibObjT>::_ThisCommonField;
    using _Spec = internal::CommonVariantFieldSpec<LibObjT>;

public:
    using Class = typename std::conditional<std::is_const<LibObjT>::value, ConstVariantFieldClass,
                                            VariantFieldClass>::type;

    explicit CommonVariantField(const _LibObjPtr libObjPtr) noexcept : _ThisCommonField {libObjPtr}
    {
        BT_ASSERT_DBG(this->isVariant());
    }

    template <typename OtherLibObjT>
    CommonVariantField(const CommonVariantField<OtherLibObjT> val) noexcept : _ThisCommonField {val}
    {
    }

    template <typename OtherLibObjT>
    CommonVariantField<LibObjT>& operator=(const CommonVariantField<OtherLibObjT> val) noexcept
    {
        _ThisCommonField::operator=(val);
        return *this;
    }

    ConstVariantFieldClass cls() const noexcept
    {
        return ConstVariantFieldClass {
            internal::CommonFieldSpec<const bt_field>::cls(this->libObjPtr())};
    }

    Class cls() noexcept
    {
        return Class {internal::CommonFieldSpec<LibObjT>::cls(this->libObjPtr())};
    }

    void selectOption(const std::uint64_t index) noexcept
    {
        static_assert(!std::is_const<LibObjT>::value, "`LibObjT` must NOT be `const`.");

        static_cast<void>(bt_field_variant_select_option_by_index(this->libObjPtr(), index));
    }

    ConstField selectedOptionField() const noexcept
    {
        return ConstField {internal::CommonVariantFieldSpec<const bt_field>::selectedOptionField(
            this->libObjPtr())};
    }

    CommonField<LibObjT> selectedOptionField() noexcept
    {
        return CommonField<LibObjT> {_Spec::selectedOptionField(this->libObjPtr())};
    }

    std::uint64_t selectedOptionIndex() const noexcept
    {
        return bt_field_variant_get_selected_option_index(this->libObjPtr());
    }
};

using VariantField = CommonVariantField<bt_field>;
using ConstVariantField = CommonVariantField<const bt_field>;

namespace internal {

struct VariantFieldTypeDescr
{
    using Const = ConstVariantField;
    using NonConst = VariantField;
};

template <>
struct TypeDescr<VariantField> : public VariantFieldTypeDescr
{
};

template <>
struct TypeDescr<ConstVariantField> : public VariantFieldTypeDescr
{
};

} /* namespace internal */

template <typename LibObjT>
CommonBoolField<LibObjT> CommonField<LibObjT>::asBool() const noexcept
{
    BT_ASSERT_DBG(this->isBool());
    return CommonBoolField<LibObjT> {this->libObjPtr()};
}

template <typename LibObjT>
CommonBitArrayField<LibObjT> CommonField<LibObjT>::asBitArray() const noexcept
{
    BT_ASSERT_DBG(this->isBitArray());
    return CommonBitArrayField<LibObjT> {this->libObjPtr()};
}

template <typename LibObjT>
CommonUnsignedIntegerField<LibObjT> CommonField<LibObjT>::asUnsignedInteger() const noexcept
{
    BT_ASSERT_DBG(this->isUnsignedInteger());
    return CommonUnsignedIntegerField<LibObjT> {this->libObjPtr()};
}

template <typename LibObjT>
CommonSignedIntegerField<LibObjT> CommonField<LibObjT>::asSignedInteger() const noexcept
{
    BT_ASSERT_DBG(this->isSignedInteger());
    return CommonSignedIntegerField<LibObjT> {this->libObjPtr()};
}

template <typename LibObjT>
CommonUnsignedEnumerationField<LibObjT> CommonField<LibObjT>::asUnsignedEnumeration() const noexcept
{
    BT_ASSERT_DBG(this->isUnsignedEnumeration());
    return CommonUnsignedEnumerationField<LibObjT> {this->libObjPtr()};
}

template <typename LibObjT>
CommonSignedEnumerationField<LibObjT> CommonField<LibObjT>::asSignedEnumeration() const noexcept
{
    BT_ASSERT_DBG(this->isSignedEnumeration());
    return CommonSignedEnumerationField<LibObjT> {this->libObjPtr()};
}

template <typename LibObjT>
CommonSinglePrecisionRealField<LibObjT> CommonField<LibObjT>::asSinglePrecisionReal() const noexcept
{
    BT_ASSERT_DBG(this->isSinglePrecisionReal());
    return CommonSinglePrecisionRealField<LibObjT> {this->libObjPtr()};
}

template <typename LibObjT>
CommonDoublePrecisionRealField<LibObjT> CommonField<LibObjT>::asDoublePrecisionReal() const noexcept
{
    BT_ASSERT_DBG(this->isDoublePrecisionReal());
    return CommonDoublePrecisionRealField<LibObjT> {this->libObjPtr()};
}

template <typename LibObjT>
CommonStringField<LibObjT> CommonField<LibObjT>::asString() const noexcept
{
    BT_ASSERT_DBG(this->isString());
    return CommonStringField<LibObjT> {this->libObjPtr()};
}

template <typename LibObjT>
CommonStructureField<LibObjT> CommonField<LibObjT>::asStructure() const noexcept
{
    BT_ASSERT_DBG(this->isStructure());
    return CommonStructureField<LibObjT> {this->libObjPtr()};
}

template <typename LibObjT>
CommonArrayField<LibObjT> CommonField<LibObjT>::asArray() const noexcept
{
    BT_ASSERT_DBG(this->isArray());
    return CommonArrayField<LibObjT> {this->libObjPtr()};
}

template <typename LibObjT>
CommonDynamicArrayField<LibObjT> CommonField<LibObjT>::asDynamicArray() const noexcept
{
    BT_ASSERT_DBG(this->isDynamicArray());
    return CommonDynamicArrayField<LibObjT> {this->libObjPtr()};
}

template <typename LibObjT>
CommonOptionField<LibObjT> CommonField<LibObjT>::asOption() const noexcept
{
    BT_ASSERT_DBG(this->isOption());
    return CommonOptionField<LibObjT> {this->libObjPtr()};
}

template <typename LibObjT>
CommonVariantField<LibObjT> CommonField<LibObjT>::asVariant() const noexcept
{
    BT_ASSERT_DBG(this->isVariant());
    return CommonVariantField<LibObjT> {this->libObjPtr()};
}

} /* namespace bt2 */

#endif /* BABELTRACE_CPP_COMMON_BT2_FIELD_HPP */
