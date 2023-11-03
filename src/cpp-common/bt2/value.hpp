/*
 * Copyright (c) 2020 Philippe Proulx <pproulx@efficios.com>
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef BABELTRACE_CPP_COMMON_BT2_VALUE_HPP
#define BABELTRACE_CPP_COMMON_BT2_VALUE_HPP

#include <cstdint>
#include <functional>
#include <type_traits>

#include <babeltrace2/babeltrace.h>

#include "common/assert.h"
#include "common/common.h"
#include "cpp-common/optional.hpp"
#include "cpp-common/string_view.hpp"

#include "borrowed-object.hpp"
#include "common-iterator.hpp"
#include "exc.hpp"
#include "internal/utils.hpp"
#include "shared-object.hpp"

namespace bt2 {
namespace internal {

struct ValueRefFuncs final
{
    static void get(const bt_value * const libObjPtr) noexcept
    {
        bt_value_get_ref(libObjPtr);
    }

    static void put(const bt_value * const libObjPtr) noexcept
    {
        bt_value_put_ref(libObjPtr);
    }
};

} /* namespace internal */

template <typename ObjT, typename LibObjT>
using SharedValue = SharedObject<ObjT, LibObjT, internal::ValueRefFuncs>;

template <typename LibObjT>
class CommonNullValue;

template <typename LibObjT>
class CommonBoolValue;

template <typename LibObjT>
class CommonUnsignedIntegerValue;

template <typename LibObjT>
class CommonSignedIntegerValue;

template <typename LibObjT>
class CommonRealValue;

template <typename LibObjT>
class CommonStringValue;

template <typename LibObjT>
class CommonArrayValue;

template <typename LibObjT>
class CommonMapValue;

enum class ValueType
{
    NUL = BT_VALUE_TYPE_NULL,
    BOOL = BT_VALUE_TYPE_BOOL,
    UNSIGNED_INTEGER = BT_VALUE_TYPE_UNSIGNED_INTEGER,
    SIGNED_INTEGER = BT_VALUE_TYPE_SIGNED_INTEGER,
    REAL = BT_VALUE_TYPE_REAL,
    STRING = BT_VALUE_TYPE_STRING,
    ARRAY = BT_VALUE_TYPE_ARRAY,
    MAP = BT_VALUE_TYPE_MAP,
};

template <typename LibObjT>
class CommonValue : public BorrowedObject<LibObjT>
{
private:
    using typename BorrowedObject<LibObjT>::_ThisBorrowedObject;

protected:
    using typename BorrowedObject<LibObjT>::_LibObjPtr;
    using _ThisCommonValue = CommonValue<LibObjT>;

public:
    using Shared = SharedValue<CommonValue<LibObjT>, LibObjT>;

    explicit CommonValue(const _LibObjPtr libObjPtr) noexcept : _ThisBorrowedObject {libObjPtr}
    {
    }

    template <typename OtherLibObjT>
    CommonValue(const CommonValue<OtherLibObjT> val) noexcept : _ThisBorrowedObject {val}
    {
    }

    template <typename OtherLibObjT>
    _ThisCommonValue& operator=(const CommonValue<OtherLibObjT> val) noexcept
    {
        _ThisBorrowedObject::operator=(val);
        return *this;
    }

    CommonValue<const bt_value> asConst() const noexcept
    {
        return CommonValue<const bt_value> {*this};
    }

    ValueType type() const noexcept
    {
        return static_cast<ValueType>(bt_value_get_type(this->libObjPtr()));
    }

    bool isNull() const noexcept
    {
        return this->_libTypeIs(BT_VALUE_TYPE_NULL);
    }

    bool isBool() const noexcept
    {
        return this->_libTypeIs(BT_VALUE_TYPE_BOOL);
    }

    bool isInteger() const noexcept
    {
        return this->_libTypeIs(BT_VALUE_TYPE_INTEGER);
    }

    bool isUnsignedInteger() const noexcept
    {
        return this->_libTypeIs(BT_VALUE_TYPE_UNSIGNED_INTEGER);
    }

    bool isSignedInteger() const noexcept
    {
        return this->_libTypeIs(BT_VALUE_TYPE_SIGNED_INTEGER);
    }

    bool isReal() const noexcept
    {
        return this->_libTypeIs(BT_VALUE_TYPE_REAL);
    }

    bool isString() const noexcept
    {
        return this->_libTypeIs(BT_VALUE_TYPE_STRING);
    }

    bool isArray() const noexcept
    {
        return this->_libTypeIs(BT_VALUE_TYPE_ARRAY);
    }

    bool isMap() const noexcept
    {
        return this->_libTypeIs(BT_VALUE_TYPE_MAP);
    }

    template <typename OtherLibObjT>
    bool operator==(const CommonValue<OtherLibObjT> other) const noexcept
    {
        return static_cast<bool>(bt_value_is_equal(this->libObjPtr(), other.libObjPtr()));
    }

    template <typename OtherLibObjT>
    bool operator!=(const CommonValue<OtherLibObjT> other) const noexcept
    {
        return !(*this == other);
    }

    Shared shared() const noexcept
    {
        return Shared::createWithRef(*this);
    }

    template <typename ValueT>
    ValueT as() const noexcept
    {
        return ValueT {this->libObjPtr()};
    }

    CommonNullValue<LibObjT> asNull() const noexcept;
    CommonBoolValue<LibObjT> asBool() const noexcept;
    CommonSignedIntegerValue<LibObjT> asSignedInteger() const noexcept;
    CommonUnsignedIntegerValue<LibObjT> asUnsignedInteger() const noexcept;
    CommonRealValue<LibObjT> asReal() const noexcept;
    CommonStringValue<LibObjT> asString() const noexcept;
    CommonArrayValue<LibObjT> asArray() const noexcept;
    CommonMapValue<LibObjT> asMap() const noexcept;

protected:
    bool _libTypeIs(const bt_value_type type) const noexcept
    {
        return bt_value_type_is(bt_value_get_type(this->libObjPtr()), type);
    }
};

using Value = CommonValue<bt_value>;
using ConstValue = CommonValue<const bt_value>;

namespace internal {

struct ValueTypeDescr
{
    using Const = ConstValue;
    using NonConst = Value;
};

template <>
struct TypeDescr<Value> : public ValueTypeDescr
{
};

template <>
struct TypeDescr<ConstValue> : public ValueTypeDescr
{
};

} /* namespace internal */

template <typename LibObjT>
class CommonNullValue final : public CommonValue<LibObjT>
{
private:
    using typename CommonValue<LibObjT>::_ThisCommonValue;

public:
    using Shared = SharedValue<CommonNullValue<LibObjT>, LibObjT>;

    CommonNullValue() noexcept : _ThisCommonValue {bt_value_null}
    {
    }

    template <typename OtherLibObjT>
    CommonNullValue(const CommonNullValue<OtherLibObjT> val) noexcept : _ThisCommonValue {val}
    {
    }

    template <typename OtherLibObjT>
    CommonNullValue<LibObjT>& operator=(const CommonNullValue<OtherLibObjT> val) noexcept
    {
        _ThisCommonValue::operator=(val);
        return *this;
    }

    CommonNullValue<const bt_value> asConst() const noexcept
    {
        return CommonNullValue<const bt_value> {*this};
    }

    Shared shared() const noexcept
    {
        return Shared::createWithRef(*this);
    }
};

using NullValue = CommonNullValue<bt_value>;
using ConstNullValue = CommonNullValue<const bt_value>;

namespace internal {

struct NullValueTypeDescr
{
    using Const = ConstNullValue;
    using NonConst = NullValue;
};

template <>
struct TypeDescr<NullValue> : public NullValueTypeDescr
{
};

template <>
struct TypeDescr<ConstNullValue> : public NullValueTypeDescr
{
};

} /* namespace internal */

template <typename LibObjT>
class CommonBoolValue final : public CommonValue<LibObjT>
{
private:
    using typename CommonValue<LibObjT>::_LibObjPtr;
    using typename CommonValue<LibObjT>::_ThisCommonValue;

public:
    using Shared = SharedValue<CommonBoolValue<LibObjT>, LibObjT>;
    using Value = bool;

    explicit CommonBoolValue(const _LibObjPtr libObjPtr) noexcept : _ThisCommonValue {libObjPtr}
    {
        BT_ASSERT_DBG(this->isBool());
    }

    template <typename OtherLibObjT>
    CommonBoolValue(const CommonBoolValue<OtherLibObjT> val) noexcept : _ThisCommonValue {val}
    {
    }

    static Shared create(const Value rawVal = false)
    {
        const auto libObjPtr = bt_value_bool_create_init(static_cast<bt_bool>(rawVal));

        internal::validateCreatedObjPtr(libObjPtr);
        return CommonBoolValue::Shared::createWithoutRef(libObjPtr);
    }

    template <typename OtherLibObjT>
    CommonBoolValue<LibObjT>& operator=(const CommonBoolValue<OtherLibObjT> val) noexcept
    {
        _ThisCommonValue::operator=(val);
        return *this;
    }

    CommonBoolValue<const bt_value> asConst() const noexcept
    {
        return CommonBoolValue<const bt_value> {*this};
    }

    CommonBoolValue<LibObjT> operator=(const Value rawVal) const noexcept
    {
        static_assert(!std::is_const<LibObjT>::value, "`LibObjT` must NOT be `const`.");

        bt_value_bool_set(this->libObjPtr(), static_cast<bt_bool>(rawVal));
        return *this;
    }

    Value value() const noexcept
    {
        return static_cast<Value>(bt_value_bool_get(this->libObjPtr()));
    }

    operator Value() const noexcept
    {
        return this->value();
    }

    Shared shared() const noexcept
    {
        return Shared::createWithRef(*this);
    }
};

using BoolValue = CommonBoolValue<bt_value>;
using ConstBoolValue = CommonBoolValue<const bt_value>;

namespace internal {

struct BoolValueTypeDescr
{
    using Const = ConstBoolValue;
    using NonConst = BoolValue;
};

template <>
struct TypeDescr<BoolValue> : public BoolValueTypeDescr
{
};

template <>
struct TypeDescr<ConstBoolValue> : public BoolValueTypeDescr
{
};

} /* namespace internal */

template <typename LibObjT>
class CommonUnsignedIntegerValue final : public CommonValue<LibObjT>
{
private:
    using typename CommonValue<LibObjT>::_LibObjPtr;
    using typename CommonValue<LibObjT>::_ThisCommonValue;

public:
    using Shared = SharedValue<CommonUnsignedIntegerValue<LibObjT>, LibObjT>;
    using Value = std::uint64_t;

    explicit CommonUnsignedIntegerValue(const _LibObjPtr libObjPtr) noexcept :
        _ThisCommonValue {libObjPtr}
    {
        BT_ASSERT_DBG(this->isUnsignedInteger());
    }

    static Shared create(const Value rawVal = 0)
    {
        const auto libObjPtr = bt_value_integer_unsigned_create_init(rawVal);

        internal::validateCreatedObjPtr(libObjPtr);
        return CommonUnsignedIntegerValue::Shared::createWithoutRef(libObjPtr);
    }

    template <typename OtherLibObjT>
    CommonUnsignedIntegerValue(const CommonUnsignedIntegerValue<OtherLibObjT> val) noexcept :
        _ThisCommonValue {val}
    {
    }

    template <typename OtherLibObjT>
    CommonUnsignedIntegerValue<LibObjT>&
    operator=(const CommonUnsignedIntegerValue<OtherLibObjT> val) noexcept
    {
        static_assert(!std::is_const<LibObjT>::value, "`LibObjT` must NOT be `const`.");

        _ThisCommonValue::operator=(val);
        return *this;
    }

    CommonUnsignedIntegerValue<const bt_value> asConst() const noexcept
    {
        return CommonUnsignedIntegerValue<const bt_value> {*this};
    }

    CommonUnsignedIntegerValue<LibObjT> operator=(const Value rawVal) const noexcept
    {
        bt_value_integer_unsigned_set(this->libObjPtr(), rawVal);
        return *this;
    }

    Value value() const noexcept
    {
        return bt_value_integer_unsigned_get(this->libObjPtr());
    }

    operator Value() const noexcept
    {
        return this->value();
    }

    Shared shared() const noexcept
    {
        return Shared::createWithRef(*this);
    }
};

using UnsignedIntegerValue = CommonUnsignedIntegerValue<bt_value>;
using ConstUnsignedIntegerValue = CommonUnsignedIntegerValue<const bt_value>;

namespace internal {

struct UnsignedIntegerValueTypeDescr
{
    using Const = ConstUnsignedIntegerValue;
    using NonConst = UnsignedIntegerValue;
};

template <>
struct TypeDescr<UnsignedIntegerValue> : public UnsignedIntegerValueTypeDescr
{
};

template <>
struct TypeDescr<ConstUnsignedIntegerValue> : public UnsignedIntegerValueTypeDescr
{
};

} /* namespace internal */

template <typename LibObjT>
class CommonSignedIntegerValue final : public CommonValue<LibObjT>
{
private:
    using typename CommonValue<LibObjT>::_LibObjPtr;
    using typename CommonValue<LibObjT>::_ThisCommonValue;

public:
    using Shared = SharedValue<CommonSignedIntegerValue<LibObjT>, LibObjT>;
    using Value = std::int64_t;

    explicit CommonSignedIntegerValue(const _LibObjPtr libObjPtr) noexcept :
        _ThisCommonValue {libObjPtr}
    {
        BT_ASSERT_DBG(this->isSignedInteger());
    }

    static Shared create(const Value rawVal = 0)
    {
        const auto libObjPtr = bt_value_integer_signed_create_init(rawVal);

        internal::validateCreatedObjPtr(libObjPtr);
        return CommonSignedIntegerValue::Shared::createWithoutRef(libObjPtr);
    }

    template <typename OtherLibObjT>
    CommonSignedIntegerValue(const CommonSignedIntegerValue<OtherLibObjT> val) noexcept :
        _ThisCommonValue {val}
    {
    }

    template <typename OtherLibObjT>
    CommonSignedIntegerValue<LibObjT>
    operator=(const CommonSignedIntegerValue<OtherLibObjT> val) noexcept
    {
        _ThisCommonValue::operator=(val);
        return *this;
    }

    CommonSignedIntegerValue<const bt_value> asConst() const noexcept
    {
        return CommonSignedIntegerValue<const bt_value> {*this};
    }

    CommonSignedIntegerValue<LibObjT> operator=(const Value rawVal) const noexcept
    {
        static_assert(!std::is_const<LibObjT>::value, "`LibObjT` must NOT be `const`.");

        bt_value_integer_signed_set(this->libObjPtr(), rawVal);
        return *this;
    }

    Value value() const noexcept
    {
        return bt_value_integer_signed_get(this->libObjPtr());
    }

    operator Value() const noexcept
    {
        return this->value();
    }

    Shared shared() const noexcept
    {
        return Shared::createWithRef(*this);
    }
};

using SignedIntegerValue = CommonSignedIntegerValue<bt_value>;
using ConstSignedIntegerValue = CommonSignedIntegerValue<const bt_value>;

namespace internal {

struct SignedIntegerValueTypeDescr
{
    using Const = ConstSignedIntegerValue;
    using NonConst = SignedIntegerValue;
};

template <>
struct TypeDescr<SignedIntegerValue> : public SignedIntegerValueTypeDescr
{
};

template <>
struct TypeDescr<ConstSignedIntegerValue> : public SignedIntegerValueTypeDescr
{
};

} /* namespace internal */

template <typename LibObjT>
class CommonRealValue final : public CommonValue<LibObjT>
{
private:
    using typename CommonValue<LibObjT>::_LibObjPtr;
    using typename CommonValue<LibObjT>::_ThisCommonValue;

public:
    using Shared = SharedValue<CommonRealValue<LibObjT>, LibObjT>;
    using Value = double;

    explicit CommonRealValue(const _LibObjPtr libObjPtr) noexcept : _ThisCommonValue {libObjPtr}
    {
        BT_ASSERT_DBG(this->isReal());
    }

    static Shared create(const Value rawVal = 0)
    {
        const auto libObjPtr = bt_value_real_create_init(rawVal);

        internal::validateCreatedObjPtr(libObjPtr);
        return CommonRealValue::Shared::createWithoutRef(libObjPtr);
    }

    template <typename OtherLibObjT>
    CommonRealValue(const CommonRealValue<OtherLibObjT> val) noexcept : _ThisCommonValue {val}
    {
    }

    template <typename OtherLibObjT>
    CommonRealValue<LibObjT>& operator=(const CommonRealValue<OtherLibObjT> val) noexcept
    {
        _ThisCommonValue::operator=(val);
        return *this;
    }

    CommonRealValue<const bt_value> asConst() const noexcept
    {
        return CommonRealValue<const bt_value> {*this};
    }

    CommonRealValue<LibObjT> operator=(const Value rawVal) const noexcept
    {
        static_assert(!std::is_const<LibObjT>::value, "`LibObjT` must NOT be `const`.");

        bt_value_real_set(this->libObjPtr(), rawVal);
        return *this;
    }

    Value value() const noexcept
    {
        return bt_value_real_get(this->libObjPtr());
    }

    operator Value() const noexcept
    {
        return this->value();
    }

    Shared shared() const noexcept
    {
        return Shared::createWithRef(*this);
    }
};

using RealValue = CommonRealValue<bt_value>;
using ConstRealValue = CommonRealValue<const bt_value>;

namespace internal {

struct RealValueTypeDescr
{
    using Const = ConstRealValue;
    using NonConst = RealValue;
};

template <>
struct TypeDescr<RealValue> : public RealValueTypeDescr
{
};

template <>
struct TypeDescr<ConstRealValue> : public RealValueTypeDescr
{
};

} /* namespace internal */

template <typename LibObjT>
class CommonStringValue final : public CommonValue<LibObjT>
{
private:
    using typename CommonValue<LibObjT>::_LibObjPtr;
    using typename CommonValue<LibObjT>::_ThisCommonValue;

public:
    using Shared = SharedValue<CommonStringValue<LibObjT>, LibObjT>;

    explicit CommonStringValue(const _LibObjPtr libObjPtr) noexcept : _ThisCommonValue {libObjPtr}
    {
        BT_ASSERT_DBG(this->isString());
    }

    static Shared create(const char * const rawVal = "")
    {
        const auto libObjPtr = bt_value_string_create_init(rawVal);

        internal::validateCreatedObjPtr(libObjPtr);
        return CommonStringValue::Shared::createWithoutRef(libObjPtr);
    }

    static Shared create(const std::string& rawVal)
    {
        return CommonStringValue::create(rawVal.data());
    }

    template <typename OtherLibObjT>
    CommonStringValue(const CommonStringValue<OtherLibObjT> val) noexcept : _ThisCommonValue {val}
    {
    }

    template <typename OtherLibObjT>
    CommonStringValue<LibObjT>& operator=(const CommonStringValue<OtherLibObjT> val) noexcept
    {
        _ThisCommonValue::operator=(val);
        return *this;
    }

    CommonStringValue<const bt_value> asConst() const noexcept
    {
        return CommonStringValue<const bt_value> {*this};
    }

    CommonStringValue<LibObjT> operator=(const char * const rawVal) const
    {
        static_assert(!std::is_const<LibObjT>::value, "`LibObjT` must NOT be `const`.");

        const auto status = bt_value_string_set(this->libObjPtr(), rawVal);

        if (status == BT_VALUE_STRING_SET_STATUS_MEMORY_ERROR) {
            throw MemoryError {};
        }

        return *this;
    }

    CommonStringValue<LibObjT> operator=(const std::string& rawVal) const noexcept
    {
        return *this = rawVal.data();
    }

    bpstd::string_view value() const noexcept
    {
        return bt_value_string_get(this->libObjPtr());
    }

    Shared shared() const noexcept
    {
        return Shared::createWithRef(*this);
    }
};

using StringValue = CommonStringValue<bt_value>;
using ConstStringValue = CommonStringValue<const bt_value>;

namespace internal {

struct StringValueTypeDescr
{
    using Const = ConstStringValue;
    using NonConst = StringValue;
};

template <>
struct TypeDescr<StringValue> : public StringValueTypeDescr
{
};

template <>
struct TypeDescr<ConstStringValue> : public StringValueTypeDescr
{
};

template <typename LibObjT>
struct CommonArrayValueSpec;

/* Functions specific to mutable array values */
template <>
struct CommonArrayValueSpec<bt_value> final
{
    static bt_value *elementByIndex(bt_value * const libValPtr, const std::uint64_t index) noexcept
    {
        return bt_value_array_borrow_element_by_index(libValPtr, index);
    }
};

/* Functions specific to constant array values */
template <>
struct CommonArrayValueSpec<const bt_value> final
{
    static const bt_value *elementByIndex(const bt_value * const libValPtr,
                                          const std::uint64_t index) noexcept
    {
        return bt_value_array_borrow_element_by_index_const(libValPtr, index);
    }
};

} /* namespace internal */

template <typename LibObjT>
class CommonArrayValue final : public CommonValue<LibObjT>
{
private:
    using typename CommonValue<LibObjT>::_LibObjPtr;
    using typename CommonValue<LibObjT>::_ThisCommonValue;

public:
    using Shared = SharedValue<CommonArrayValue<LibObjT>, LibObjT>;
    using Iterator = CommonIterator<CommonArrayValue<LibObjT>, CommonValue<LibObjT>>;

    explicit CommonArrayValue(const _LibObjPtr libObjPtr) noexcept : _ThisCommonValue {libObjPtr}
    {
        BT_ASSERT_DBG(this->isArray());
    }

    static Shared create()
    {
        const auto libObjPtr = bt_value_array_create();

        internal::validateCreatedObjPtr(libObjPtr);
        return CommonArrayValue::Shared::createWithoutRef(libObjPtr);
    }

    template <typename OtherLibObjT>
    CommonArrayValue(const CommonArrayValue<OtherLibObjT> val) noexcept : _ThisCommonValue {val}
    {
    }

    template <typename OtherLibObjT>
    CommonArrayValue<LibObjT>& operator=(const CommonArrayValue<OtherLibObjT> val) noexcept
    {
        _ThisCommonValue::operator=(val);
        return *this;
    }

    CommonArrayValue<const bt_value> asConst() const noexcept
    {
        return CommonArrayValue<const bt_value> {*this};
    }

    std::uint64_t length() const noexcept
    {
        return bt_value_array_get_length(this->libObjPtr());
    }

    Iterator begin() const noexcept
    {
        return Iterator {*this, 0};
    }

    Iterator end() const noexcept
    {
        return Iterator {*this, this->length()};
    }

    bool isEmpty() const noexcept
    {
        return this->length() == 0;
    }

    CommonValue<LibObjT> operator[](const std::uint64_t index) const noexcept
    {
        return CommonValue<LibObjT> {
            internal::CommonArrayValueSpec<LibObjT>::elementByIndex(this->libObjPtr(), index)};
    }

    void append(const Value val) const
    {
        static_assert(!std::is_const<LibObjT>::value, "`LibObjT` must NOT be `const`.");

        const auto status = bt_value_array_append_element(this->libObjPtr(), val.libObjPtr());

        this->_handleAppendLibStatus(status);
    }

    void append(const bool rawVal) const
    {
        static_assert(!std::is_const<LibObjT>::value, "`LibObjT` must NOT be `const`.");

        const auto status =
            bt_value_array_append_bool_element(this->libObjPtr(), static_cast<bt_bool>(rawVal));

        this->_handleAppendLibStatus(status);
    }

    void append(const std::uint64_t rawVal) const
    {
        static_assert(!std::is_const<LibObjT>::value, "`LibObjT` must NOT be `const`.");

        const auto status =
            bt_value_array_append_unsigned_integer_element(this->libObjPtr(), rawVal);

        this->_handleAppendLibStatus(status);
    }

    void append(const std::int64_t rawVal) const
    {
        static_assert(!std::is_const<LibObjT>::value, "`LibObjT` must NOT be `const`.");

        const auto status = bt_value_array_append_signed_integer_element(this->libObjPtr(), rawVal);

        this->_handleAppendLibStatus(status);
    }

    void append(const double rawVal) const
    {
        static_assert(!std::is_const<LibObjT>::value, "`LibObjT` must NOT be `const`.");

        const auto status = bt_value_array_append_real_element(this->libObjPtr(), rawVal);

        this->_handleAppendLibStatus(status);
    }

    void append(const char * const rawVal) const
    {
        static_assert(!std::is_const<LibObjT>::value, "`LibObjT` must NOT be `const`.");

        const auto status = bt_value_array_append_string_element(this->libObjPtr(), rawVal);

        this->_handleAppendLibStatus(status);
    }

    void append(const std::string& rawVal) const
    {
        this->append(rawVal.data());
    }

    CommonArrayValue<bt_value> appendEmptyArray() const;
    CommonMapValue<bt_value> appendEmptyMap() const;

    void operator+=(const Value val) const
    {
        this->append(val);
    }

    void operator+=(const bool rawVal) const
    {
        this->append(rawVal);
    }

    void operator+=(const std::uint64_t rawVal) const
    {
        this->append(rawVal);
    }

    void operator+=(const std::int64_t rawVal) const
    {
        this->append(rawVal);
    }

    void operator+=(const double rawVal) const
    {
        this->append(rawVal);
    }

    void operator+=(const char * const rawVal) const
    {
        this->append(rawVal);
    }

    void operator+=(const std::string& rawVal) const
    {
        this->append(rawVal);
    }

    Shared shared() const noexcept
    {
        return Shared::createWithRef(*this);
    }

private:
    void _handleAppendLibStatus(const bt_value_array_append_element_status status) const
    {
        if (status == BT_VALUE_ARRAY_APPEND_ELEMENT_STATUS_MEMORY_ERROR) {
            throw MemoryError {};
        }
    }
};

using ArrayValue = CommonArrayValue<bt_value>;
using ConstArrayValue = CommonArrayValue<const bt_value>;

namespace internal {

struct ArrayValueTypeDescr
{
    using Const = ConstArrayValue;
    using NonConst = ArrayValue;
};

template <>
struct TypeDescr<ArrayValue> : public ArrayValueTypeDescr
{
};

template <>
struct TypeDescr<ConstArrayValue> : public ArrayValueTypeDescr
{
};

/*
 * Type of a user function passed to `CommonMapValue<ObjT>::forEach()`.
 *
 * First argument is the entry's key, second is its value.
 */
template <typename ObjT>
using CommonMapValueForEachUserFunc = std::function<void(const bpstd::string_view&, ObjT)>;

/*
 * Template of a function to be passed to bt_value_map_foreach_entry()
 * for bt_value_map_foreach_entry_const() which calls a user function.
 *
 * `userData` is casted to a `const` pointer to
 * `CommonMapValueForEachUserFunc<ObjT>` (the user function to call).
 *
 * This function catches any exception which the user function throws
 * and returns the `ErrorStatus` value. If there's no execption, this
 * function returns the `OkStatus` value.
 */
template <typename ObjT, typename LibObjT, typename LibStatusT, int OkStatus, int ErrorStatus>
LibStatusT mapValueForEachLibFunc(const char * const key, LibObjT * const libObjPtr,
                                  void * const userData)
{
    const auto& userFunc = *reinterpret_cast<const CommonMapValueForEachUserFunc<ObjT> *>(userData);

    try {
        userFunc(key, ObjT {libObjPtr});
    } catch (...) {
        return static_cast<LibStatusT>(ErrorStatus);
    }

    return static_cast<LibStatusT>(OkStatus);
}

template <typename LibObjT>
struct CommonMapValueSpec;

/* Functions specific to mutable map values */
template <>
struct CommonMapValueSpec<bt_value> final
{
    static bt_value *entryByKey(bt_value * const libValPtr, const char * const key) noexcept
    {
        return bt_value_map_borrow_entry_value(libValPtr, key);
    }

    static void forEach(bt_value * const libValPtr,
                        const CommonMapValueForEachUserFunc<Value>& func)
    {
        const auto status = bt_value_map_foreach_entry(
            libValPtr,
            mapValueForEachLibFunc<Value, bt_value, bt_value_map_foreach_entry_func_status,
                                   BT_VALUE_MAP_FOREACH_ENTRY_FUNC_STATUS_OK,
                                   BT_VALUE_MAP_FOREACH_ENTRY_FUNC_STATUS_ERROR>,
            const_cast<void *>(reinterpret_cast<const void *>(&func)));

        switch (status) {
        case BT_VALUE_MAP_FOREACH_ENTRY_STATUS_OK:
            return;
        case BT_VALUE_MAP_FOREACH_ENTRY_STATUS_USER_ERROR:
        case BT_VALUE_MAP_FOREACH_ENTRY_STATUS_ERROR:
            throw Error {};
        default:
            bt_common_abort();
        }
    }
};

/* Functions specific to constant map values */
template <>
struct CommonMapValueSpec<const bt_value> final
{
    static const bt_value *entryByKey(const bt_value * const libValPtr,
                                      const char * const key) noexcept
    {
        return bt_value_map_borrow_entry_value_const(libValPtr, key);
    }

    static void forEach(const bt_value * const libValPtr,
                        const CommonMapValueForEachUserFunc<ConstValue>& func)
    {
        const auto status = bt_value_map_foreach_entry_const(
            libValPtr,
            mapValueForEachLibFunc<ConstValue, const bt_value,
                                   bt_value_map_foreach_entry_const_func_status,
                                   BT_VALUE_MAP_FOREACH_ENTRY_CONST_FUNC_STATUS_OK,
                                   BT_VALUE_MAP_FOREACH_ENTRY_CONST_FUNC_STATUS_ERROR>,
            const_cast<void *>(reinterpret_cast<const void *>(&func)));

        switch (status) {
        case BT_VALUE_MAP_FOREACH_ENTRY_CONST_STATUS_OK:
            return;
        case BT_VALUE_MAP_FOREACH_ENTRY_CONST_STATUS_USER_ERROR:
        case BT_VALUE_MAP_FOREACH_ENTRY_CONST_STATUS_ERROR:
            throw Error {};
        default:
            bt_common_abort();
        }
    }
};

} /* namespace internal */

template <typename LibObjT>
class CommonMapValue final : public CommonValue<LibObjT>
{
private:
    using typename CommonValue<LibObjT>::_LibObjPtr;
    using typename CommonValue<LibObjT>::_ThisCommonValue;

public:
    using Shared = SharedValue<CommonMapValue<LibObjT>, LibObjT>;

    explicit CommonMapValue(const _LibObjPtr libObjPtr) noexcept : _ThisCommonValue {libObjPtr}
    {
        BT_ASSERT_DBG(this->isMap());
    }

    static Shared create()
    {
        const auto libObjPtr = bt_value_map_create();

        internal::validateCreatedObjPtr(libObjPtr);
        return CommonMapValue::Shared::createWithoutRef(libObjPtr);
    }

    template <typename OtherLibObjT>
    CommonMapValue(const CommonMapValue<OtherLibObjT> val) noexcept : _ThisCommonValue {val}
    {
    }

    template <typename OtherLibObjT>
    CommonMapValue<LibObjT>& operator=(const CommonMapValue<OtherLibObjT> val) noexcept
    {
        _ThisCommonValue::operator=(val);
        return *this;
    }

    CommonMapValue<const bt_value> asConst() const noexcept
    {
        return CommonMapValue<const bt_value> {*this};
    }

    std::uint64_t length() const noexcept
    {
        return bt_value_map_get_size(this->libObjPtr());
    }

    bool isEmpty() const noexcept
    {
        return this->length() == 0;
    }

    nonstd::optional<CommonValue<LibObjT>> operator[](const char * const key) const noexcept
    {
        const auto libObjPtr =
            internal::CommonMapValueSpec<LibObjT>::entryByKey(this->libObjPtr(), key);

        if (!libObjPtr) {
            return nonstd::nullopt;
        }

        return CommonValue<LibObjT> {libObjPtr};
    }

    nonstd::optional<CommonValue<LibObjT>> operator[](const std::string& key) const noexcept
    {
        return (*this)[key.data()];
    }

    bool hasEntry(const char * const key) const noexcept
    {
        return static_cast<bool>(bt_value_map_has_entry(this->libObjPtr(), key));
    }

    bool hasEntry(const std::string& key) const noexcept
    {
        return this->hasEntry(key.data());
    }

    void insert(const char * const key, const Value val) const
    {
        static_assert(!std::is_const<LibObjT>::value, "`LibObjT` must NOT be `const`.");

        const auto status = bt_value_map_insert_entry(this->libObjPtr(), key, val.libObjPtr());

        this->_handleInsertLibStatus(status);
    }

    void insert(const std::string& key, const Value val) const
    {
        this->insert(key.data(), val);
    }

    void insert(const char * const key, const bool rawVal) const
    {
        static_assert(!std::is_const<LibObjT>::value, "`LibObjT` must NOT be `const`.");

        const auto status =
            bt_value_map_insert_bool_entry(this->libObjPtr(), key, static_cast<bt_bool>(rawVal));

        this->_handleInsertLibStatus(status);
    }

    void insert(const std::string& key, const bool rawVal) const
    {
        this->insert(key.data(), rawVal);
    }

    void insert(const char * const key, const std::uint64_t rawVal) const
    {
        static_assert(!std::is_const<LibObjT>::value, "`LibObjT` must NOT be `const`.");

        const auto status =
            bt_value_map_insert_unsigned_integer_entry(this->libObjPtr(), key, rawVal);

        this->_handleInsertLibStatus(status);
    }

    void insert(const std::string& key, const std::uint64_t rawVal) const
    {
        this->insert(key.data(), rawVal);
    }

    void insert(const char * const key, const std::int64_t rawVal) const
    {
        static_assert(!std::is_const<LibObjT>::value, "`LibObjT` must NOT be `const`.");

        const auto status =
            bt_value_map_insert_signed_integer_entry(this->libObjPtr(), key, rawVal);

        this->_handleInsertLibStatus(status);
    }

    void insert(const std::string& key, const std::int64_t rawVal) const
    {
        this->insert(key.data(), rawVal);
    }

    void insert(const char * const key, const double rawVal) const
    {
        static_assert(!std::is_const<LibObjT>::value, "`LibObjT` must NOT be `const`.");

        const auto status = bt_value_map_insert_real_entry(this->libObjPtr(), key, rawVal);

        this->_handleInsertLibStatus(status);
    }

    void insert(const std::string& key, const double rawVal) const
    {
        this->insert(key.data(), rawVal);
    }

    void insert(const char * const key, const char * const rawVal) const
    {
        static_assert(!std::is_const<LibObjT>::value, "`LibObjT` must NOT be `const`.");

        const auto status = bt_value_map_insert_string_entry(this->libObjPtr(), key, rawVal);

        this->_handleInsertLibStatus(status);
    }

    void insert(const char * const key, const std::string& rawVal) const
    {
        this->insert(key, rawVal.data());
    }

    void insert(const std::string& key, const char * const rawVal) const
    {
        this->insert(key.data(), rawVal);
    }

    void insert(const std::string& key, const std::string& rawVal) const
    {
        this->insert(key.data(), rawVal.data());
    }

    CommonArrayValue<bt_value> insertEmptyArray(const char *key) const;
    CommonArrayValue<bt_value> insertEmptyArray(const std::string& key) const;
    CommonMapValue<bt_value> insertEmptyMap(const char *key) const;
    CommonMapValue<bt_value> insertEmptyMap(const std::string& key) const;

    void forEach(const internal::CommonMapValueForEachUserFunc<CommonValue<LibObjT>>& func) const
    {
        internal::CommonMapValueSpec<LibObjT>::forEach(this->libObjPtr(), func);
    }

    Shared shared() const noexcept
    {
        return Shared::createWithRef(*this);
    }

private:
    void _handleInsertLibStatus(const bt_value_map_insert_entry_status status) const
    {
        if (status == BT_VALUE_MAP_INSERT_ENTRY_STATUS_MEMORY_ERROR) {
            throw MemoryError {};
        }
    }
};

using MapValue = CommonMapValue<bt_value>;
using ConstMapValue = CommonMapValue<const bt_value>;

namespace internal {

struct MapValueTypeDescr
{
    using Const = ConstMapValue;
    using NonConst = MapValue;
};

template <>
struct TypeDescr<MapValue> : public MapValueTypeDescr
{
};

template <>
struct TypeDescr<ConstMapValue> : public MapValueTypeDescr
{
};

} /* namespace internal */

template <typename LibObjT>
CommonNullValue<LibObjT> CommonValue<LibObjT>::asNull() const noexcept
{
    BT_ASSERT_DBG(this->isNull());
    return CommonNullValue<LibObjT> {this->libObjPtr()};
}

template <typename LibObjT>
CommonBoolValue<LibObjT> CommonValue<LibObjT>::asBool() const noexcept
{
    BT_ASSERT_DBG(this->isBool());
    return CommonBoolValue<LibObjT> {this->libObjPtr()};
}

template <typename LibObjT>
CommonSignedIntegerValue<LibObjT> CommonValue<LibObjT>::asSignedInteger() const noexcept
{
    BT_ASSERT_DBG(this->isSignedInteger());
    return CommonSignedIntegerValue<LibObjT> {this->libObjPtr()};
}

template <typename LibObjT>
CommonUnsignedIntegerValue<LibObjT> CommonValue<LibObjT>::asUnsignedInteger() const noexcept
{
    BT_ASSERT_DBG(this->isUnsignedInteger());
    return CommonUnsignedIntegerValue<LibObjT> {this->libObjPtr()};
}

template <typename LibObjT>
CommonRealValue<LibObjT> CommonValue<LibObjT>::asReal() const noexcept
{
    BT_ASSERT_DBG(this->isReal());
    return CommonRealValue<LibObjT> {this->libObjPtr()};
}

template <typename LibObjT>
CommonStringValue<LibObjT> CommonValue<LibObjT>::asString() const noexcept
{
    BT_ASSERT_DBG(this->isString());
    return CommonStringValue<LibObjT> {this->libObjPtr()};
}

template <typename LibObjT>
CommonArrayValue<LibObjT> CommonValue<LibObjT>::asArray() const noexcept
{
    BT_ASSERT_DBG(this->isArray());
    return CommonArrayValue<LibObjT> {this->libObjPtr()};
}

template <typename LibObjT>
CommonMapValue<LibObjT> CommonValue<LibObjT>::asMap() const noexcept
{
    BT_ASSERT_DBG(this->isMap());
    return CommonMapValue<LibObjT> {this->libObjPtr()};
}

template <typename LibObjT>
ArrayValue CommonArrayValue<LibObjT>::appendEmptyArray() const
{
    static_assert(!std::is_const<LibObjT>::value, "`LibObjT` must NOT be `const`.");

    bt_value *libElemPtr;
    const auto status = bt_value_array_append_empty_array_element(this->libObjPtr(), &libElemPtr);

    this->_handleAppendLibStatus(status);
    return ArrayValue {libElemPtr};
}

template <typename LibObjT>
MapValue CommonArrayValue<LibObjT>::appendEmptyMap() const
{
    static_assert(!std::is_const<LibObjT>::value, "`LibObjT` must NOT be `const`.");

    bt_value *libElemPtr;
    const auto status = bt_value_array_append_empty_map_element(this->libObjPtr(), &libElemPtr);

    this->_handleAppendLibStatus(status);
    return MapValue {libElemPtr};
}

template <typename LibObjT>
ArrayValue CommonMapValue<LibObjT>::insertEmptyArray(const char * const key) const
{
    static_assert(!std::is_const<LibObjT>::value, "`LibObjT` must NOT be `const`.");

    bt_value *libEntryPtr;
    const auto status = bt_value_map_insert_empty_array_entry(this->libObjPtr(), key, &libEntryPtr);

    this->_handleInsertLibStatus(status);
    return ArrayValue {libEntryPtr};
}

template <typename LibObjT>
ArrayValue CommonMapValue<LibObjT>::insertEmptyArray(const std::string& key) const
{
    return this->insertEmptyArray(key.data());
}

template <typename LibObjT>
MapValue CommonMapValue<LibObjT>::insertEmptyMap(const char * const key) const
{
    static_assert(!std::is_const<LibObjT>::value, "`LibObjT` must NOT be `const`.");

    bt_value *libEntryPtr;
    const auto status = bt_value_map_insert_empty_map_entry(this->libObjPtr(), key, &libEntryPtr);

    this->_handleInsertLibStatus(status);
    return MapValue {libEntryPtr};
}

template <typename LibObjT>
MapValue CommonMapValue<LibObjT>::insertEmptyMap(const std::string& key) const
{
    return this->insertEmptyMap(key.data());
}

inline BoolValue::Shared createValue(const bool rawVal)
{
    return BoolValue::create(rawVal);
}

inline UnsignedIntegerValue::Shared createValue(const std::uint64_t rawVal)
{
    return UnsignedIntegerValue::create(rawVal);
}

inline SignedIntegerValue::Shared createValue(const std::int64_t rawVal)
{
    return SignedIntegerValue::create(rawVal);
}

inline RealValue::Shared createValue(const double rawVal)
{
    return RealValue::create(rawVal);
}

inline StringValue::Shared createValue(const char * const rawVal)
{
    return StringValue::create(rawVal);
}

inline StringValue::Shared createValue(const std::string& rawVal)
{
    return StringValue::create(rawVal);
}

} /* namespace bt2 */

#endif /* BABELTRACE_CPP_COMMON_BT2_VALUE_HPP */
