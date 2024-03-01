/*
 * Copyright (c) 2023 Philippe Proulx <pproulx@efficios.com>
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef BABELTRACE_CPP_COMMON_BT2C_C_STRING_VIEW_HPP
#define BABELTRACE_CPP_COMMON_BT2C_C_STRING_VIEW_HPP

#include <cstddef>
#include <cstring>
#include <string>

#include "common/assert.h"
#include "cpp-common/bt2s/string-view.hpp"
#include "cpp-common/vendor/fmt/format.h" /* IWYU pragma: keep */

#include "type-traits.hpp"

namespace bt2c {

/*
 * A view on a constant null-terminated C string.
 *
 * Similar to `bt2s::string_view`, but len() and end() compute the
 * length on demand.
 */
class CStringView final
{
public:
    /*
     * Builds an empty view (data() returns `nullptr`).
     *
     * Intentionally not explicit.
     */
    constexpr CStringView() noexcept = default;

    /*
     * Builds a view of the C string `str` (may be `nullptr`).
     *
     * Intentionally not explicit.
     */
    constexpr CStringView(const char * const str) noexcept : _mStr {str}
    {
    }

    /*
     * Builds a view of the string `str`.
     *
     * Intentionally not explicit.
     */
    CStringView(const std::string& str) noexcept : _mStr {str.c_str()}
    {
    }

    /*
     * Makes this view view the C string `str` (may be `nullptr`).
     *
     * Intentionally not explicit.
     */
    CStringView& operator=(const char * const str) noexcept
    {
        _mStr = str;
        return *this;
    }

    /*
     * Viewed null-terminated C string (may be `nullptr`).
     */
    const char *data() const noexcept
    {
        return _mStr;
    }

    /*
     * Alias of data().
     */
    operator const char *() const noexcept
    {
        return this->data();
    }

    /*
     * Alias of data().
     */
    const char *operator*() const noexcept
    {
        return this->data();
    }

    /*
     * Alias of data().
     *
     * data() must not return `nullptr`.
     */
    const char *begin() const noexcept
    {
        BT_ASSERT_DBG(_mStr);
        return this->data();
    }

    /*
     * Pointer to the null character of the viewed C string.
     *
     * data() must not return `nullptr`.
     */
    const char *end() const noexcept
    {
        BT_ASSERT_DBG(_mStr);
        return _mStr + this->len();
    }

    /*
     * Length of the viewed C string, excluding the null character.
     *
     * data() must not return `nullptr`.
     */
    std::size_t len() const noexcept
    {
        BT_ASSERT_DBG(_mStr);
        return std::strlen(_mStr);
    }

    /*
     * Returns an `std::string` instance containing a copy of the viewed
     * C string.
     *
     * data() must not return `nullptr`.
     */
    std::string str() const
    {
        BT_ASSERT_DBG(_mStr);
        return std::string {_mStr};
    }

    /*
     * Alias of str().
     */
    operator std::string() const
    {
        return this->str();
    }

    /*
     * Returns a `bt2s::string_view` instance to view the contents,
     * excluding the null character, of the viewed C string.
     */
    bt2s::string_view strView() const noexcept
    {
        if (_mStr) {
            return bt2s::string_view {this->begin(), this->len()};
        } else {
            return {};
        }
    }

    /*
     * Alias of strView().
     */
    operator bt2s::string_view() const noexcept
    {
        return this->strView();
    }

    /*
     * Returns the character at index `i`.
     *
     * `i` must be less than what len() returns.
     *
     * data() must not return `nullptr`.
     */
    char operator[](const std::size_t i) const noexcept
    {
        BT_ASSERT_DBG(_mStr);
        BT_ASSERT_DBG(i < this->len());
        return _mStr[i];
    }

private:
    const char *_mStr = nullptr;
};

inline const char *format_as(const CStringView& str)
{
    return str ? *str : "(null)";
}

namespace internal {

template <typename StrT>
const char *asConstCharPtr(StrT&& val) noexcept
{
    return val.data();
}

inline const char *asConstCharPtr(const char * const val) noexcept
{
    return val;
}

template <typename StrT>
using ComparableWithCStringView =
    IsOneOf<typename std::decay<StrT>::type, CStringView, std::string, const char *>;

} /* namespace internal */

/*
 * Returns true if `lhs` is equal to `rhs`.
 *
 * `LhsT` and `RhsT` may be any of:
 *
 * • `const char *`
 * • `std::string`
 * • `CStringView`
 *
 * Both `lhs` and `rhs` must not have an underlying `nullptr` raw data.
 */
template <
    typename LhsT, typename RhsT,
    typename = typename std::enable_if<internal::ComparableWithCStringView<LhsT>::value>::type,
    typename = typename std::enable_if<internal::ComparableWithCStringView<RhsT>::value>::type>
bool operator==(LhsT&& lhs, RhsT&& rhs) noexcept
{
    const auto rawLhs = internal::asConstCharPtr(lhs);
    const auto rawRhs = internal::asConstCharPtr(rhs);

    BT_ASSERT_DBG(rawLhs);
    BT_ASSERT_DBG(rawRhs);
    return std::strcmp(rawLhs, rawRhs) == 0;
}

/*
 * Returns true if `lhs` is not equal to `rhs`.
 *
 * `LhsT` and `RhsT` may be any of:
 *
 * • `const char *`
 * • `std::string`
 * • `CStringView`
 *
 * Both `lhs` and `rhs` must not have an underlying `nullptr` raw data.
 */
template <
    typename LhsT, typename RhsT,
    typename = typename std::enable_if<internal::ComparableWithCStringView<LhsT>::value>::type,
    typename = typename std::enable_if<internal::ComparableWithCStringView<RhsT>::value>::type>
bool operator!=(LhsT&& lhs, RhsT&& rhs) noexcept
{
    return !(std::forward<LhsT>(lhs) == std::forward<RhsT>(rhs));
}

} /* namespace bt2c */

#endif /* BABELTRACE_CPP_COMMON_BT2C_C_STRING_VIEW_HPP */
