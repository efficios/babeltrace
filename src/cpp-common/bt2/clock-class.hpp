/*
 * Copyright (c) 2020 Philippe Proulx <pproulx@efficios.com>
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef BABELTRACE_CPP_COMMON_BT2_CLOCK_CLASS_HPP
#define BABELTRACE_CPP_COMMON_BT2_CLOCK_CLASS_HPP

#include <cstdint>
#include <string>
#include <type_traits>

#include <babeltrace2/babeltrace.h>

#include "cpp-common/optional.hpp"
#include "cpp-common/string_view.hpp"
#include "cpp-common/uuid-view.hpp"

#include "borrowed-object.hpp"
#include "exc.hpp"
#include "internal/utils.hpp"
#include "shared-object.hpp"
#include "value.hpp"

namespace bt2 {
namespace internal {

struct ClockClassRefFuncs final
{
    static void get(const bt_clock_class * const libObjPtr) noexcept
    {
        bt_clock_class_get_ref(libObjPtr);
    }

    static void put(const bt_clock_class * const libObjPtr) noexcept
    {
        bt_clock_class_put_ref(libObjPtr);
    }
};

template <typename LibObjT>
struct CommonClockClassSpec;

/* Functions specific to mutable clock classes */
template <>
struct CommonClockClassSpec<bt_clock_class> final
{
    static bt_value *userAttributes(bt_clock_class * const libObjPtr) noexcept
    {
        return bt_clock_class_borrow_user_attributes(libObjPtr);
    }
};

/* Functions specific to constant clock classes */
template <>
struct CommonClockClassSpec<const bt_clock_class> final
{
    static const bt_value *userAttributes(const bt_clock_class * const libObjPtr) noexcept
    {
        return bt_clock_class_borrow_user_attributes_const(libObjPtr);
    }
};

} /* namespace internal */

class ClockClassOffset final
{
public:
    explicit ClockClassOffset(const std::int64_t seconds, const std::uint64_t cycles) :
        _mSeconds {seconds}, _mCycles {cycles}
    {
    }

    ClockClassOffset(const ClockClassOffset&) noexcept = default;
    ClockClassOffset& operator=(const ClockClassOffset&) noexcept = default;

    std::int64_t seconds() const noexcept
    {
        return _mSeconds;
    }

    std::uint64_t cycles() const noexcept
    {
        return _mCycles;
    }

private:
    std::int64_t _mSeconds;
    std::uint64_t _mCycles;
};

template <typename LibObjT>
class CommonClockClass final : public BorrowedObject<LibObjT>
{
private:
    using typename BorrowedObject<LibObjT>::_ThisBorrowedObject;
    using typename BorrowedObject<LibObjT>::_LibObjPtr;
    using _ThisCommonClockClass = CommonClockClass<LibObjT>;

public:
    using Shared = SharedObject<_ThisCommonClockClass, LibObjT, internal::ClockClassRefFuncs>;

    using UserAttributes =
        typename std::conditional<std::is_const<LibObjT>::value, ConstMapValue, MapValue>::type;

    explicit CommonClockClass(const _LibObjPtr libObjPtr) noexcept : _ThisBorrowedObject {libObjPtr}
    {
    }

    template <typename OtherLibObjT>
    CommonClockClass(const CommonClockClass<OtherLibObjT> clkClass) noexcept :
        _ThisBorrowedObject {clkClass}
    {
    }

    template <typename OtherLibObjT>
    _ThisCommonClockClass& operator=(const CommonClockClass<OtherLibObjT> clkClass) noexcept
    {
        _ThisBorrowedObject::operator=(clkClass);
        return *this;
    }

    void frequency(const std::uint64_t frequency) const noexcept
    {
        static_assert(!std::is_const<LibObjT>::value, "`LibObjT` must NOT be `const`.");

        bt_clock_class_set_frequency(this->libObjPtr(), frequency);
    }

    std::uint64_t frequency() const noexcept
    {
        return bt_clock_class_get_frequency(this->libObjPtr());
    }

    void offset(const ClockClassOffset& offset) const noexcept
    {
        static_assert(!std::is_const<LibObjT>::value, "`LibObjT` must NOT be `const`.");

        bt_clock_class_set_offset(this->libObjPtr(), offset.seconds(), offset.cycles());
    }

    ClockClassOffset offset() const noexcept
    {
        std::int64_t seconds;
        std::uint64_t cycles;

        bt_clock_class_get_offset(this->libObjPtr(), &seconds, &cycles);
        return ClockClassOffset {seconds, cycles};
    }

    void precision(const std::uint64_t precision) const noexcept
    {
        static_assert(!std::is_const<LibObjT>::value, "`LibObjT` must NOT be `const`.");

        bt_clock_class_set_precision(this->libObjPtr(), precision);
    }

    std::uint64_t precision() const noexcept
    {
        return bt_clock_class_get_precision(this->libObjPtr());
    }

    void originIsUnixEpoch(const bool originIsUnixEpoch) const noexcept
    {
        static_assert(!std::is_const<LibObjT>::value, "`LibObjT` must NOT be `const`.");

        bt_clock_class_set_origin_is_unix_epoch(this->libObjPtr(),
                                                static_cast<bt_bool>(originIsUnixEpoch));
    }

    bool originIsUnixEpoch() const noexcept
    {
        return static_cast<bool>(bt_clock_class_origin_is_unix_epoch(this->libObjPtr()));
    }

    void name(const char * const name) const
    {
        static_assert(!std::is_const<LibObjT>::value, "`LibObjT` must NOT be `const`.");

        const auto status = bt_clock_class_set_name(this->libObjPtr(), name);

        if (status == BT_CLOCK_CLASS_SET_NAME_STATUS_MEMORY_ERROR) {
            throw MemoryError {};
        }
    }

    void name(const std::string& name) const
    {
        this->name(name.data());
    }

    nonstd::optional<bpstd::string_view> name() const noexcept
    {
        const auto name = bt_clock_class_get_name(this->libObjPtr());

        if (name) {
            return name;
        }

        return nonstd::nullopt;
    }

    void description(const char * const description) const
    {
        static_assert(!std::is_const<LibObjT>::value, "`LibObjT` must NOT be `const`.");

        const auto status = bt_clock_class_set_description(this->libObjPtr(), description);

        if (status == BT_CLOCK_CLASS_SET_DESCRIPTION_STATUS_MEMORY_ERROR) {
            throw MemoryError {};
        }
    }

    void description(const std::string& description) const
    {
        this->description(description.data());
    }

    nonstd::optional<bpstd::string_view> description() const noexcept
    {
        const auto description = bt_clock_class_get_description(this->libObjPtr());

        if (description) {
            return description;
        }

        return nonstd::nullopt;
    }

    void uuid(const std::uint8_t * const uuid) const noexcept
    {
        bt_clock_class_set_uuid(this->libObjPtr(), uuid);
    }

    nonstd::optional<bt2_common::UuidView> uuid() const noexcept
    {
        const auto uuid = bt_clock_class_get_uuid(this->libObjPtr());

        if (uuid) {
            return bt2_common::UuidView {uuid};
        }

        return nonstd::nullopt;
    }

    template <typename LibValT>
    void userAttributes(const CommonMapValue<LibValT> userAttrs) const
    {
        static_assert(!std::is_const<LibObjT>::value, "`LibObjT` must NOT be `const`.");

        bt_clock_class_set_user_attributes(this->libObjPtr(), userAttrs.libObjPtr());
    }

    UserAttributes userAttributes() const noexcept
    {
        return UserAttributes {
            internal::CommonClockClassSpec<LibObjT>::userAttributes(this->libObjPtr())};
    }

    std::int64_t cyclesToNsFromOrigin(const std::uint64_t value) const
    {
        std::int64_t nsFromOrigin;
        const auto status =
            bt_clock_class_cycles_to_ns_from_origin(this->libObjPtr(), value, &nsFromOrigin);

        if (status == BT_CLOCK_CLASS_CYCLES_TO_NS_FROM_ORIGIN_STATUS_OVERFLOW_ERROR) {
            throw OverflowError {};
        }

        return nsFromOrigin;
    }

    Shared shared() const noexcept
    {
        return Shared::createWithRef(*this);
    }
};

using ClockClass = CommonClockClass<bt_clock_class>;
using ConstClockClass = CommonClockClass<const bt_clock_class>;

namespace internal {

struct ClockClassTypeDescr
{
    using Const = ConstClockClass;
    using NonConst = ClockClass;
};

template <>
struct TypeDescr<ClockClass> : public ClockClassTypeDescr
{
};

template <>
struct TypeDescr<ConstClockClass> : public ClockClassTypeDescr
{
};

} /* namespace internal */
} /* namespace bt2 */

#endif /* BABELTRACE_CPP_COMMON_BT2_CLOCK_CLASS_HPP */
