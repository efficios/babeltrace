/*
 * Copyright (c) 2020 Philippe Proulx <pproulx@efficios.com>
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef BABELTRACE_CPP_COMMON_BT2_FIELD_HPP
#define BABELTRACE_CPP_COMMON_BT2_FIELD_HPP

#include <cstdint>
#include <type_traits>

#include <babeltrace2/babeltrace.h>

#include "common/assert.h"
#include "cpp-common/bt2s/optional.hpp"

#include "borrowed-object.hpp"
#include "field-class.hpp"
#include "internal/utils.hpp"
#include "raw-value-proxy.hpp"

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
class CommonField : public BorrowedObject<LibObjT>
{
private:
    using typename BorrowedObject<LibObjT>::_ThisBorrowedObject;

protected:
    using _ThisCommonField = CommonField<LibObjT>;

public:
    using typename BorrowedObject<LibObjT>::LibObjPtr;
    using Class = internal::DepFc<LibObjT>;

    explicit CommonField(const LibObjPtr libObjPtr) noexcept : _ThisBorrowedObject {libObjPtr}
    {
    }

    template <typename OtherLibObjT>
    CommonField(const CommonField<OtherLibObjT> val) noexcept : _ThisBorrowedObject {val}
    {
    }

    template <typename OtherLibObjT>
    _ThisCommonField operator=(const CommonField<OtherLibObjT> val) noexcept
    {
        _ThisBorrowedObject::operator=(val);
        return *this;
    }

    CommonField<const bt_field> asConst() const noexcept
    {
        return CommonField<const bt_field> {*this};
    }

    FieldClassType classType() const noexcept
    {
        return static_cast<FieldClassType>(bt_field_get_class_type(this->libObjPtr()));
    }

    Class cls() const noexcept
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
    using typename CommonField<LibObjT>::_ThisCommonField;

public:
    using typename CommonField<LibObjT>::LibObjPtr;
    using Value = bool;

    explicit CommonBoolField(const LibObjPtr libObjPtr) noexcept : _ThisCommonField {libObjPtr}
    {
        BT_ASSERT_DBG(this->isBool());
    }

    template <typename OtherLibObjT>
    CommonBoolField(const CommonBoolField<OtherLibObjT> val) noexcept : _ThisCommonField {val}
    {
    }

    template <typename OtherLibObjT>
    CommonBoolField<LibObjT> operator=(const CommonBoolField<OtherLibObjT> val) noexcept
    {
        _ThisCommonField::operator=(val);
        return *this;
    }

    CommonBoolField<const bt_field> asConst() const noexcept
    {
        return CommonBoolField<const bt_field> {*this};
    }

    RawValueProxy<CommonBoolField> operator*() const noexcept
    {
        return RawValueProxy<CommonBoolField> {*this};
    }

    void value(const Value val) const noexcept
    {
        static_assert(!std::is_const<LibObjT>::value, "Not available with `bt2::ConstBoolField`.");

        bt_field_bool_set_value(this->libObjPtr(), static_cast<bt_bool>(val));
    }

    Value value() const noexcept
    {
        return static_cast<Value>(bt_field_bool_get_value(this->libObjPtr()));
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
    using typename CommonField<LibObjT>::_ThisCommonField;

public:
    using typename CommonField<LibObjT>::LibObjPtr;
    using Class = internal::DepType<LibObjT, BitArrayFieldClass, ConstBitArrayFieldClass>;

    explicit CommonBitArrayField(const LibObjPtr libObjPtr) noexcept : _ThisCommonField {libObjPtr}
    {
        BT_ASSERT_DBG(this->isBitArray());
    }

    template <typename OtherLibObjT>
    CommonBitArrayField(const CommonBitArrayField<OtherLibObjT> val) noexcept :
        _ThisCommonField {val}
    {
    }

    template <typename OtherLibObjT>
    CommonBitArrayField<LibObjT> operator=(const CommonBitArrayField<OtherLibObjT> val) noexcept
    {
        _ThisCommonField::operator=(val);
        return *this;
    }

    CommonBitArrayField<const bt_field> asConst() const noexcept
    {
        return CommonBitArrayField<const bt_field> {*this};
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

    void valueAsInteger(const std::uint64_t bits) const noexcept
    {
        static_assert(!std::is_const<LibObjT>::value,
                      "Not available with `bt2::ConstBitArrayField`.");

        bt_field_bit_array_set_value_as_integer(this->libObjPtr(), bits);
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
class CommonUnsignedIntegerField : public CommonField<LibObjT>
{
private:
    using typename CommonField<LibObjT>::_ThisCommonField;

protected:
    using _ThisCommonUnsignedIntegerField = CommonUnsignedIntegerField<LibObjT>;

public:
    using typename CommonField<LibObjT>::LibObjPtr;
    using Value = std::uint64_t;
    using Class = internal::DepType<LibObjT, IntegerFieldClass, ConstIntegerFieldClass>;

    explicit CommonUnsignedIntegerField(const LibObjPtr libObjPtr) noexcept :
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
    _ThisCommonUnsignedIntegerField
    operator=(const CommonUnsignedIntegerField<OtherLibObjT> val) noexcept
    {
        _ThisCommonField::operator=(val);
        return *this;
    }

    CommonUnsignedIntegerField<const bt_field> asConst() const noexcept
    {
        return CommonUnsignedIntegerField<const bt_field> {*this};
    }

    Class cls() const noexcept
    {
        return Class {internal::CommonFieldSpec<LibObjT>::cls(this->libObjPtr())};
    }

    RawValueProxy<CommonUnsignedIntegerField> operator*() const noexcept
    {
        return RawValueProxy<CommonUnsignedIntegerField> {*this};
    }

    void value(const Value val) const noexcept
    {
        static_assert(!std::is_const<LibObjT>::value,
                      "Not available with `bt2::ConstUnsignedIntegerField`.");

        bt_field_integer_unsigned_set_value(this->libObjPtr(), val);
    }

    Value value() const noexcept
    {
        return bt_field_integer_unsigned_get_value(this->libObjPtr());
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
class CommonSignedIntegerField : public CommonField<LibObjT>
{
private:
    using typename CommonField<LibObjT>::_ThisCommonField;

protected:
    using _ThisCommonSignedIntegerField = CommonSignedIntegerField<LibObjT>;

public:
    using typename CommonField<LibObjT>::LibObjPtr;
    using Value = std::int64_t;
    using Class = internal::DepType<LibObjT, IntegerFieldClass, ConstIntegerFieldClass>;

    explicit CommonSignedIntegerField(const LibObjPtr libObjPtr) noexcept :
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
    _ThisCommonSignedIntegerField
    operator=(const CommonSignedIntegerField<OtherLibObjT> val) noexcept
    {
        _ThisCommonField::operator=(val);
        return *this;
    }

    CommonSignedIntegerField<const bt_field> asConst() const noexcept
    {
        return CommonSignedIntegerField<const bt_field> {*this};
    }

    Class cls() const noexcept
    {
        return Class {internal::CommonFieldSpec<LibObjT>::cls(this->libObjPtr())};
    }

    RawValueProxy<CommonSignedIntegerField> operator*() const noexcept
    {
        return RawValueProxy<CommonSignedIntegerField> {*this};
    }

    void value(const Value val) const noexcept
    {
        static_assert(!std::is_const<LibObjT>::value,
                      "Not available with `bt2::ConstSignedIntegerField`.");

        bt_field_integer_signed_set_value(this->libObjPtr(), val);
    }

    Value value() const noexcept
    {
        return bt_field_integer_signed_get_value(this->libObjPtr());
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
        _mLen {size}
    {
    }

    std::uint64_t length() const noexcept
    {
        return _mLen;
    }

    const char *operator[](const std::uint64_t index) const noexcept
    {
        return _mLabels[index];
    }

private:
    bt_field_class_enumeration_mapping_label_array _mLabels;
    std::uint64_t _mLen;
};

template <typename LibObjT>
class CommonUnsignedEnumerationField final : public CommonUnsignedIntegerField<LibObjT>
{
private:
    using typename CommonUnsignedIntegerField<LibObjT>::_ThisCommonUnsignedIntegerField;

public:
    using typename CommonField<LibObjT>::LibObjPtr;

    using Class = internal::DepType<LibObjT, UnsignedEnumerationFieldClass,
                                    ConstUnsignedEnumerationFieldClass>;

    explicit CommonUnsignedEnumerationField(const LibObjPtr libObjPtr) noexcept :
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
    CommonUnsignedEnumerationField<LibObjT>
    operator=(const CommonUnsignedEnumerationField<OtherLibObjT> val) noexcept
    {
        _ThisCommonUnsignedIntegerField::operator=(val);
        return *this;
    }

    CommonUnsignedEnumerationField<const bt_field> asConst() const noexcept
    {
        return CommonUnsignedEnumerationField<const bt_field> {*this};
    }

    Class cls() const noexcept
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

public:
    using typename CommonField<LibObjT>::LibObjPtr;

    using Class =
        internal::DepType<LibObjT, SignedEnumerationFieldClass, ConstSignedEnumerationFieldClass>;

    explicit CommonSignedEnumerationField(const LibObjPtr libObjPtr) noexcept :
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
    CommonSignedEnumerationField<LibObjT>
    operator=(const CommonSignedEnumerationField<OtherLibObjT> val) noexcept
    {
        _ThisCommonSignedIntegerField::operator=(val);
        return *this;
    }

    CommonSignedEnumerationField<const bt_field> asConst() const noexcept
    {
        return CommonSignedEnumerationField<const bt_field> {*this};
    }

    Class cls() const noexcept
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
    using typename CommonField<LibObjT>::_ThisCommonField;

public:
    using typename CommonField<LibObjT>::LibObjPtr;
    using Value = float;

    explicit CommonSinglePrecisionRealField(const LibObjPtr libObjPtr) noexcept :
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
    CommonSinglePrecisionRealField<LibObjT>
    operator=(const CommonSinglePrecisionRealField<OtherLibObjT> val) noexcept
    {
        _ThisCommonField::operator=(val);
        return *this;
    }

    CommonSinglePrecisionRealField<const bt_field> asConst() const noexcept
    {
        return CommonSinglePrecisionRealField<const bt_field> {*this};
    }

    RawValueProxy<CommonSinglePrecisionRealField> operator*() const noexcept
    {
        return RawValueProxy<CommonSinglePrecisionRealField> {*this};
    }

    void value(const Value val) const noexcept
    {
        static_assert(!std::is_const<LibObjT>::value,
                      "Not available with `bt2::ConstSinglePrecisionRealField`.");

        bt_field_real_single_precision_set_value(this->libObjPtr(), val);
    }

    Value value() const noexcept
    {
        return bt_field_real_single_precision_get_value(this->libObjPtr());
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
    using typename CommonField<LibObjT>::_ThisCommonField;

public:
    using typename CommonField<LibObjT>::LibObjPtr;
    using Value = double;

    explicit CommonDoublePrecisionRealField(const LibObjPtr libObjPtr) noexcept :
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
    CommonDoublePrecisionRealField<LibObjT>
    operator=(const CommonDoublePrecisionRealField<OtherLibObjT> val) noexcept
    {
        _ThisCommonField::operator=(val);
        return *this;
    }

    CommonDoublePrecisionRealField<const bt_field> asConst() const noexcept
    {
        return CommonDoublePrecisionRealField<const bt_field> {*this};
    }

    RawValueProxy<CommonDoublePrecisionRealField> operator*() const noexcept
    {
        return RawValueProxy<CommonDoublePrecisionRealField> {*this};
    }

    void value(const Value val) const noexcept
    {
        static_assert(!std::is_const<LibObjT>::value,
                      "Not available with `bt2::ConstDoublePrecisionRealField`.");

        bt_field_real_double_precision_set_value(this->libObjPtr(), val);
    }

    Value value() const noexcept
    {
        return bt_field_real_double_precision_get_value(this->libObjPtr());
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
    using typename CommonField<LibObjT>::_ThisCommonField;

public:
    using typename CommonField<LibObjT>::LibObjPtr;
    using Value = const char *;

    explicit CommonStringField(const LibObjPtr libObjPtr) noexcept : _ThisCommonField {libObjPtr}
    {
        BT_ASSERT_DBG(this->isString());
    }

    template <typename OtherLibObjT>
    CommonStringField(const CommonStringField<OtherLibObjT> val) noexcept : _ThisCommonField {val}
    {
    }

    template <typename OtherLibObjT>
    CommonStringField<LibObjT> operator=(const CommonStringField<OtherLibObjT> val) noexcept
    {
        _ThisCommonField::operator=(val);
        return *this;
    }

    CommonStringField<const bt_field> asConst() const noexcept
    {
        return CommonStringField<const bt_field> {*this};
    }

    RawStringValueProxy<CommonStringField> operator*() const noexcept
    {
        return RawStringValueProxy<CommonStringField> {*this};
    }

    void value(const char * const val) const
    {
        static_assert(!std::is_const<LibObjT>::value,
                      "Not available with `bt2::ConstStringField`.");

        const auto status = bt_field_string_set_value(this->libObjPtr(), val);

        if (status == BT_FIELD_STRING_SET_VALUE_STATUS_MEMORY_ERROR) {
            throw MemoryError {};
        }
    }

    void value(const std::string& val) const
    {
        this->value(val.data());
    }

    void append(const char * const begin, const std::uint64_t len) const
    {
        static_assert(!std::is_const<LibObjT>::value,
                      "Not available with `bt2::ConstStringField`.");

        const auto status = bt_field_string_append_with_length(this->libObjPtr(), begin, len);

        if (status == BT_FIELD_STRING_APPEND_STATUS_MEMORY_ERROR) {
            throw MemoryError {};
        }
    }

    void append(const std::string& val) const
    {
        this->append(val.data(), val.size());
    }

    void clear() const noexcept
    {
        static_assert(!std::is_const<LibObjT>::value,
                      "Not available with `bt2::ConstStringField`.");

        bt_field_string_clear(this->libObjPtr());
    }

    const char *value() const noexcept
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
    using typename CommonField<LibObjT>::_ThisCommonField;
    using _Spec = internal::CommonStructureFieldSpec<LibObjT>;

public:
    using typename CommonField<LibObjT>::LibObjPtr;
    using Class = internal::DepType<LibObjT, StructureFieldClass, ConstStructureFieldClass>;

    explicit CommonStructureField(const LibObjPtr libObjPtr) noexcept : _ThisCommonField {libObjPtr}
    {
        BT_ASSERT_DBG(this->isStructure());
    }

    template <typename OtherLibObjT>
    CommonStructureField(const CommonStructureField<OtherLibObjT> val) noexcept :
        _ThisCommonField {val}
    {
    }

    template <typename OtherLibObjT>
    CommonStructureField<LibObjT> operator=(const CommonStructureField<OtherLibObjT> val) noexcept
    {
        _ThisCommonField::operator=(val);
        return *this;
    }

    CommonStructureField<const bt_field> asConst() const noexcept
    {
        return CommonStructureField<const bt_field> {*this};
    }

    Class cls() const noexcept
    {
        return Class {internal::CommonFieldSpec<LibObjT>::cls(this->libObjPtr())};
    }

    std::uint64_t length() const noexcept
    {
        return this->cls().length();
    }

    CommonField<LibObjT> operator[](const std::uint64_t index) const noexcept
    {
        return CommonField<LibObjT> {_Spec::memberFieldByIndex(this->libObjPtr(), index)};
    }

    bt2s::optional<CommonField<LibObjT>> operator[](const char * const name) const noexcept
    {
        const auto libObjPtr = _Spec::memberFieldByName(this->libObjPtr(), name);

        if (libObjPtr) {
            return CommonField<LibObjT> {libObjPtr};
        }

        return bt2s::nullopt;
    }

    bt2s::optional<CommonField<LibObjT>> operator[](const std::string& name) const noexcept
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
    using _ThisCommonArrayField = CommonArrayField<LibObjT>;

public:
    using typename CommonField<LibObjT>::LibObjPtr;
    using Class = internal::DepType<LibObjT, ArrayFieldClass, ConstArrayFieldClass>;

    explicit CommonArrayField(const LibObjPtr libObjPtr) noexcept : _ThisCommonField {libObjPtr}
    {
        BT_ASSERT_DBG(this->isArray());
    }

    template <typename OtherLibObjT>
    CommonArrayField(const CommonArrayField<OtherLibObjT> val) noexcept : _ThisCommonField {val}
    {
    }

    template <typename OtherLibObjT>
    _ThisCommonArrayField operator=(const CommonArrayField<OtherLibObjT> val) noexcept
    {
        _ThisCommonField::operator=(val);
        return *this;
    }

    CommonArrayField<const bt_field> asConst() const noexcept
    {
        return CommonArrayField<const bt_field> {*this};
    }

    Class cls() const noexcept
    {
        return Class {internal::CommonFieldSpec<LibObjT>::cls(this->libObjPtr())};
    }

    std::uint64_t length() const noexcept
    {
        return bt_field_array_get_length(this->libObjPtr());
    }

    CommonField<LibObjT> operator[](const std::uint64_t index) const noexcept
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
    using typename CommonArrayField<LibObjT>::_ThisCommonArrayField;

public:
    using typename CommonField<LibObjT>::LibObjPtr;

    explicit CommonDynamicArrayField(const LibObjPtr libObjPtr) noexcept :
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
    CommonDynamicArrayField<LibObjT>
    operator=(const CommonDynamicArrayField<OtherLibObjT> val) noexcept
    {
        _ThisCommonArrayField::operator=(val);
        return *this;
    }

    CommonDynamicArrayField<const bt_field> asConst() const noexcept
    {
        return CommonDynamicArrayField<const bt_field> {*this};
    }

    std::uint64_t length() const noexcept
    {
        return _ThisCommonArrayField::length();
    }

    void length(const std::uint64_t length) const
    {
        static_assert(!std::is_const<LibObjT>::value,
                      "Not available with `bt2::ConstDynamicArrayField`.");

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
    using typename CommonField<LibObjT>::_ThisCommonField;
    using _Spec = internal::CommonOptionFieldSpec<LibObjT>;

public:
    using typename CommonField<LibObjT>::LibObjPtr;
    using Class = internal::DepType<LibObjT, OptionFieldClass, ConstOptionFieldClass>;

    explicit CommonOptionField(const LibObjPtr libObjPtr) noexcept : _ThisCommonField {libObjPtr}
    {
        BT_ASSERT_DBG(this->isOption());
    }

    template <typename OtherLibObjT>
    CommonOptionField(const CommonOptionField<OtherLibObjT> val) noexcept : _ThisCommonField {val}
    {
    }

    template <typename OtherLibObjT>
    CommonOptionField<LibObjT> operator=(const CommonOptionField<OtherLibObjT> val) noexcept
    {
        _ThisCommonField::operator=(val);
        return *this;
    }

    CommonOptionField<const bt_field> asConst() const noexcept
    {
        return CommonOptionField<const bt_field> {*this};
    }

    Class cls() const noexcept
    {
        return Class {internal::CommonFieldSpec<LibObjT>::cls(this->libObjPtr())};
    }

    void hasField(const bool hasField) const noexcept
    {
        static_assert(!std::is_const<LibObjT>::value,
                      "Not available with `bt2::ConstOptionField`.");

        bt_field_option_set_has_field(this->libObjPtr(), static_cast<bt_bool>(hasField));
    }

    bool hasField() const noexcept
    {
        return this->field().has_value();
    }

    bt2s::optional<CommonField<LibObjT>> field() const noexcept
    {
        const auto libObjPtr = _Spec::field(this->libObjPtr());

        if (libObjPtr) {
            return CommonField<LibObjT> {libObjPtr};
        }

        return bt2s::nullopt;
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
    using typename CommonField<LibObjT>::_ThisCommonField;
    using _Spec = internal::CommonVariantFieldSpec<LibObjT>;

public:
    using typename CommonField<LibObjT>::LibObjPtr;
    using Class = internal::DepType<LibObjT, VariantFieldClass, ConstVariantFieldClass>;

    explicit CommonVariantField(const LibObjPtr libObjPtr) noexcept : _ThisCommonField {libObjPtr}
    {
        BT_ASSERT_DBG(this->isVariant());
    }

    template <typename OtherLibObjT>
    CommonVariantField(const CommonVariantField<OtherLibObjT> val) noexcept : _ThisCommonField {val}
    {
    }

    template <typename OtherLibObjT>
    CommonVariantField<LibObjT> operator=(const CommonVariantField<OtherLibObjT> val) noexcept
    {
        _ThisCommonField::operator=(val);
        return *this;
    }

    CommonVariantField<const bt_field> asConst() const noexcept
    {
        return CommonVariantField<const bt_field> {*this};
    }

    Class cls() const noexcept
    {
        return Class {internal::CommonFieldSpec<LibObjT>::cls(this->libObjPtr())};
    }

    void selectOption(const std::uint64_t index) const noexcept
    {
        static_assert(!std::is_const<LibObjT>::value,
                      "Not available with `bt2::ConstVariantField`.");

        static_cast<void>(bt_field_variant_select_option_by_index(this->libObjPtr(), index));
    }

    CommonField<LibObjT> selectedOptionField() const noexcept
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
