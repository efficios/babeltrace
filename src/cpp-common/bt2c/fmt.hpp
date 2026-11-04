/*
 * Copyright (c) 2024 EfficiOS, inc.
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef BABELTRACE_CPP_COMMON_BT2C_FMT_HPP
#define BABELTRACE_CPP_COMMON_BT2C_FMT_HPP

/*!
@file

@brief
    Custom {fmt} formatting.

@ingroup common-cpp-bt2c

@code{.cpp}
#include "cpp-common/bt2c/fmt.hpp"
@endcode

When you include this file, you get custom {fmt} formatting of:

- Any wise_enum enumerator within the \c bt2c or \c bt2 namespace.
- A bt2c::UuidView instance.
- A bt2c::Date, bt2c::TimeOfDay, or bt2c::DateTime instance, using the
  ISO&nbsp;8601 extended format (for example, <code>2026-10-23</code>,
  <code>14:10:33</code>, and <code>2026-10-23T14:10:33</code>).

  The year of a date has at least four digits. A year outside
  [0,&nbsp;9999] also has an explicit sign (for example,
  <code>-0044-03-15</code> and <code>+10000-01-01</code>).

- A bt2c::SafeInt instance, like its raw value (format specifications
  apply, for example <code>{:\#x}</code>).
*/

#include <fmt/format.h> /* IWYU pragma: keep */

#include "cpp-common/bt2/value.hpp"
#include "cpp-common/vendor/wise-enum/wise_enum.h"

#include "date-time.hpp"
#include "safe-int.hpp"
#include "uuid.hpp"

namespace internal {

template <typename T>
using EnableIfIsWiseEnum = std::enable_if_t<wise_enum::is_wise_enum_v<T>, wise_enum::string_type>;

} /* namespace internal */

namespace bt2 {

template <typename T>
::internal::EnableIfIsWiseEnum<T> format_as(const T val) noexcept
{
    return wise_enum::to_string<T>(val);
}

inline std::string format_as(const bt2::ConstValue val) noexcept
{
    switch (val.type()) {
    case ValueType::Null:
        return "null";

    case ValueType::Bool:
        return val.asBool().value() ? "true" : "false";

    case ValueType::UnsignedInteger:
        return fmt::format("{}u", val.asUnsignedInteger().value());

    case ValueType::SignedInteger:
        return fmt::format("{}", val.asSignedInteger().value());

    case ValueType::Real:
        return fmt::format("{}", val.asReal().value());

    case ValueType::String:
        return fmt::format("\"{}\"", val.asString().value());

    case ValueType::Array:
    {
        std::string ret {'['};
        const char *maybeComma = "";

        for (const auto elem : val.asArray()) {
            ret += fmt::format("{}{}", maybeComma, elem);
            maybeComma = ", ";
        }

        ret += ']';
        return ret;
    }

    case ValueType::Map:
    {
        std::string ret {'{'};
        const char *maybeComma = "";

        val.asMap().forEach([&](const auto k, const auto v) {
            ret += fmt::format("{}{}: {}", maybeComma, k, v);
            maybeComma = ", ";
        });

        ret += '}';
        return ret;
    }
    }

    bt_common_abort();
}

} /* namespace bt2 */

namespace bt2c {

template <typename T>
::internal::EnableIfIsWiseEnum<T> format_as(const T val) noexcept
{
    return wise_enum::to_string<T>(val);
}

inline std::string format_as(const UuidView uuid)
{
    return uuid.str();
}

inline std::string format_as(const Date& date)
{
    if (date.year() >= 0 && date.year() <= 9999) {
        return fmt::format("{:04}-{:02}-{:02}", date.year(), date.month(), date.day());
    }

    /* ISO 8601 expanded representation */
    return fmt::format("{:+05}-{:02}-{:02}", date.year(), date.month(), date.day());
}

inline std::string format_as(const TimeOfDay& timeOfDay)
{
    return fmt::format("{:02}:{:02}:{:02}", timeOfDay.hour(), timeOfDay.minute(),
                       timeOfDay.second());
}

inline std::string format_as(const DateTime& dateTime)
{
    return fmt::format("{}T{}", dateTime.date(), dateTime.timeOfDay());
}

template <typename ValT, SafeIntPolicy PolicyV>
constexpr ValT format_as(const SafeInt<ValT, PolicyV> safeVal) noexcept
{
    return safeVal.val();
}

} /* namespace bt2c */

#endif /* BABELTRACE_CPP_COMMON_BT2C_FMT_HPP */
