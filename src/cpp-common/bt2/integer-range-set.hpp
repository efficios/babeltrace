/*
 * Copyright (c) 2020 Philippe Proulx <pproulx@efficios.com>
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef BABELTRACE_CPP_COMMON_BT2_INTEGER_RANGE_SET_HPP
#define BABELTRACE_CPP_COMMON_BT2_INTEGER_RANGE_SET_HPP

#include <cstdint>
#include <type_traits>
#include <babeltrace2/babeltrace.h>

#include "internal/borrowed-obj.hpp"
#include "internal/utils.hpp"
#include "integer-range.hpp"
#include "lib-error.hpp"

namespace bt2 {

namespace internal {

template <typename LibObjT>
struct IntegerRangeSetRefFuncs;

template <>
struct IntegerRangeSetRefFuncs<const bt_integer_range_set_unsigned> final
{
    static void get(const bt_integer_range_set_unsigned * const libObjPtr)
    {
        bt_integer_range_set_unsigned_get_ref(libObjPtr);
    }

    static void put(const bt_integer_range_set_unsigned * const libObjPtr)
    {
        bt_integer_range_set_unsigned_get_ref(libObjPtr);
    }
};

template <>
struct IntegerRangeSetRefFuncs<const bt_integer_range_set_signed> final
{
    static void get(const bt_integer_range_set_signed * const libObjPtr)
    {
        bt_integer_range_set_signed_get_ref(libObjPtr);
    }

    static void put(const bt_integer_range_set_signed * const libObjPtr)
    {
        bt_integer_range_set_signed_get_ref(libObjPtr);
    }
};

template <typename LibObjT>
struct CommonIntegerRangeSetSpec;

/* Functions specific to unsigned integer range sets */
template <>
struct CommonIntegerRangeSetSpec<const bt_integer_range_set_unsigned> final
{
    static std::uint64_t size(const bt_integer_range_set_unsigned * const libRangePtr) noexcept
    {
        return bt_integer_range_set_get_range_count(
            bt_integer_range_set_unsigned_as_range_set_const(libRangePtr));
    }

    static const bt_integer_range_unsigned *
    rangeByIndex(const bt_integer_range_set_unsigned * const libRangePtr,
                 const std::uint64_t index) noexcept
    {
        return bt_integer_range_set_unsigned_borrow_range_by_index_const(libRangePtr, index);
    }

    static bool isEqual(const bt_integer_range_set_unsigned * const libRangePtrA,
                        const bt_integer_range_set_unsigned * const libRangePtrB) noexcept
    {
        return static_cast<bool>(
            bt_integer_range_set_unsigned_is_equal(libRangePtrA, libRangePtrB));
    }

    static bt_integer_range_set_add_range_status
    addRange(bt_integer_range_set_unsigned * const libRangePtr, const std::uint64_t lower,
             const std::uint64_t upper) noexcept
    {
        return bt_integer_range_set_unsigned_add_range(libRangePtr, lower, upper);
    }

    static bt_integer_range_set_unsigned *create() noexcept
    {
        return bt_integer_range_set_unsigned_create();
    }
};

/* Functions specific to signed integer range sets */
template <>
struct CommonIntegerRangeSetSpec<const bt_integer_range_set_signed> final
{
    static std::uint64_t size(const bt_integer_range_set_signed * const libRangePtr) noexcept
    {
        return bt_integer_range_set_get_range_count(
            bt_integer_range_set_signed_as_range_set_const(libRangePtr));
    }

    static const bt_integer_range_signed *
    rangeByIndex(const bt_integer_range_set_signed * const libRangePtr,
                 const std::uint64_t index) noexcept
    {
        return bt_integer_range_set_signed_borrow_range_by_index_const(libRangePtr, index);
    }

    static bool isEqual(const bt_integer_range_set_signed * const libRangePtrA,
                        const bt_integer_range_set_signed * const libRangePtrB) noexcept
    {
        return static_cast<bool>(bt_integer_range_set_signed_is_equal(libRangePtrA, libRangePtrB));
    }

    static bt_integer_range_set_add_range_status
    addRange(bt_integer_range_set_signed * const libRangePtr, const std::int64_t lower,
             const std::int64_t upper) noexcept
    {
        return bt_integer_range_set_signed_add_range(libRangePtr, lower, upper);
    }

    static bt_integer_range_set_signed *create() noexcept
    {
        return bt_integer_range_set_signed_create();
    }
};

} /* namespace internal */

template <typename LibObjT>
class ConstVariantWithIntegerSelectorFieldClassOption;

template <typename LibObjT, typename RangeSetT>
class CommonVariantWithIntegerSelectorFieldClass;

template <typename LibObjT>
class CommonTraceClass;

template <typename LibObjT>
class CommonIntegerRangeSet final : public internal::BorrowedObj<LibObjT>
{
    /* Allow operator==() to call `other.libObjPtr()` */
    friend class CommonIntegerRangeSet<bt_integer_range_set_unsigned>;
    friend class CommonIntegerRangeSet<const bt_integer_range_set_unsigned>;
    friend class CommonIntegerRangeSet<bt_integer_range_set_signed>;
    friend class CommonIntegerRangeSet<const bt_integer_range_set_signed>;

    /* Allow appendOption() to call `ranges.libObjPtr()` */
    friend class CommonVariantWithIntegerSelectorFieldClass<
        bt_field_class,
        ConstVariantWithIntegerSelectorFieldClassOption<
            const bt_field_class_variant_with_selector_field_integer_unsigned_option>>;

    friend class CommonVariantWithIntegerSelectorFieldClass<
        bt_field_class,
        ConstVariantWithIntegerSelectorFieldClassOption<
            const bt_field_class_variant_with_selector_field_integer_signed_option>>;

    /* Allow create*FieldClass() to call `ranges.libObjPtr()` */
    friend class CommonTraceClass<bt_trace_class>;

private:
    using typename internal::BorrowedObj<LibObjT>::_ThisBorrowedObj;
    using typename internal::BorrowedObj<LibObjT>::_LibObjPtr;
    using _ConstLibObjT = typename std::add_const<LibObjT>::type;
    using _RefFuncs = internal::IntegerRangeSetRefFuncs<_ConstLibObjT>;
    using _Spec = internal::CommonIntegerRangeSetSpec<_ConstLibObjT>;
    using _ThisCommonIntegerRangeSet = CommonIntegerRangeSet<LibObjT>;

public:
    using Shared = internal::SharedObj<_ThisCommonIntegerRangeSet, LibObjT, _RefFuncs>;

    using Range = typename std::conditional<
        std::is_same<_ConstLibObjT, const bt_integer_range_set_unsigned>::value,
        ConstUnsignedIntegerRange, ConstSignedIntegerRange>::type;

    using Value = typename Range::Value;

    explicit CommonIntegerRangeSet(const _LibObjPtr libObjPtr) noexcept :
        _ThisBorrowedObj {libObjPtr}
    {
    }

    static Shared create()
    {
        const auto libObjPtr = _Spec::create();

        internal::validateCreatedObjPtr(libObjPtr);
        return Shared {_ThisCommonIntegerRangeSet {libObjPtr}};
    }

    template <typename OtherLibObjT>
    CommonIntegerRangeSet(const CommonIntegerRangeSet<OtherLibObjT>& rangeSet) noexcept :
        _ThisBorrowedObj {rangeSet}
    {
    }

    template <typename OtherLibObjT>
    _ThisCommonIntegerRangeSet&
    operator=(const CommonIntegerRangeSet<OtherLibObjT>& rangeSet) noexcept
    {
        _ThisBorrowedObj::operator=(rangeSet);
        return *this;
    }

    template <typename OtherLibObjT>
    bool operator==(const CommonIntegerRangeSet<OtherLibObjT>& other) const noexcept
    {
        return _Spec::isEqual(this->libObjPtr(), other.libObjPtr());
    }

    template <typename OtherLibObjT>
    bool operator!=(const CommonIntegerRangeSet<OtherLibObjT>& other) const noexcept
    {
        return !(*this == other);
    }

    void addRange(const Value lower, const Value upper)
    {
        static_assert(!std::is_const<LibObjT>::value, "`LibObjT` must NOT be `const`.");

        const auto status = _Spec::addRange(this->libObjPtr(), lower, upper);

        if (status == BT_INTEGER_RANGE_SET_ADD_RANGE_STATUS_MEMORY_ERROR) {
            throw LibMemoryError {};
        }
    }

    std::uint64_t size() const noexcept
    {
        return _Spec::size(this->libObjPtr());
    }

    Range operator[](const std::uint64_t index) const noexcept
    {
        return Range {_Spec::rangeByIndex(this->libObjPtr(), index)};
    }

    Shared shared() const noexcept
    {
        return Shared {*this};
    }
};

using UnsignedIntegerRangeSet = CommonIntegerRangeSet<bt_integer_range_set_unsigned>;
using ConstUnsignedIntegerRangeSet = CommonIntegerRangeSet<const bt_integer_range_set_unsigned>;
using SignedIntegerRangeSet = CommonIntegerRangeSet<bt_integer_range_set_signed>;
using ConstSignedIntegerRangeSet = CommonIntegerRangeSet<const bt_integer_range_set_signed>;

} /* namespace bt2 */

#endif /* BABELTRACE_CPP_COMMON_BT2_INTEGER_RANGE_SET_HPP */
