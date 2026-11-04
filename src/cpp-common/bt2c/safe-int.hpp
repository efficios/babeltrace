/*
 * Copyright (c) 2026 Philippe Proulx <pproulx@efficios.com>
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef BABELTRACE_CPP_COMMON_BT2C_SAFE_INT_HPP
#define BABELTRACE_CPP_COMMON_BT2C_SAFE_INT_HPP

/*!
@file

@brief
    Checked integer wrapper.

@ingroup common-cpp-bt2c

@code{.cpp}
#include "cpp-common/bt2c/safe-int.hpp"
@endcode

bt2c::SafeInt wraps a value of some integer type \bt_p{ValT} (not
\c bool or a character type). Its arithmetic operators compute the
mathematically exact result and fail if this result doesn't fit
\bt_p{ValT}, for example:

@code{.cpp}
bt2c::SafeInt<std::uint32_t> {3} - 5U;                         // Fails
bt2c::SafeInt<std::uint64_t> {UINT64_MAX} * std::uint64_t {2}; // Fails
bt2c::SafeInt<std::int64_t> {-3} + std::int64_t {5};           // 2
@endcode

How an operation fails depends on the policy (second template
parameter; see bt2c::SafeIntPolicy):

<dl>
  <dt>bt2c::SafeIntPolicy::Throw (default)
  <dd>
    Throws bt2c::OverflowError, its reason (bt2c::OverflowError::what())
    describing the failed operation, for example:

    @verbatim
    `18446744073709551615 * 2` doesn't fit a 64-bit unsigned integer
    @endverbatim

    Use this in code which handles untrusted data.

  <dt>bt2c::SafeIntPolicy::AssertDbg (alias: bt2c::SafeIntDbg)
  <dd>
    Asserts that the operation doesn't fail, like BT_ASSERT_DBG() does,
    showing the same reason.

    Unless you configured the project with
    <code>BABELTRACE_DEBUG_MODE=1</code>, the operations don't check
    anything: use this in fast or frequent code paths where a failure is
    a programming error.
</dl>

<h2>Operands and result type</h2>

The operands of an arithmetic operator (<code>+</code>, <code>-</code>,
<code>*</code>, <code>/</code>, and their compound assignment versions)
are two bt2c::SafeInt instances, or one bt2c::SafeInt instance and a
plain integer. Both bt2c::SafeInt operands must have the same policy.

Both operand types must also have the same signedness and width: the
result type is the type of the (left) bt2c::SafeInt operand, without any
promotion to \c int (for example,
<code>std::uint8_t</code>&nbsp;+&nbsp;<code>std::uint8_t</code> gives
<code>std::uint8_t</code>).

To mix operand types, convert one operand first:

- A plain integer: with brace initialization when any value of its
  type fits (the compiler diagnoses a narrowing conversion: an error
  with Clang, a warning with GCC), or else with the explicit
  bt2c::SafeInt constructor, which checks the value.

- A bt2c::SafeInt: with bt2c::SafeInt::as() or one of its aliases
  (bt2c::SafeInt::asU8(), bt2c::SafeInt::asS32(), and the rest), which
  checks the value.

For example, with \c u32, \c u64, and \c safeU32 having the types
<code>std::uint32_t</code>, <code>std::uint64_t</code>, and
bt2c::SafeU32:

@code{.cpp}
bt2c::SafeU64 {x} - std::uint64_t {1};   // Okay
bt2c::SafeU64 {x} - 1U;                  // Doesn't build
bt2c::SafeS64 {x} + u32;                 // Doesn't build
bt2c::SafeS64 {x} + std::int64_t {u32};  // Okay
bt2c::SafeS64 {x} + std::int64_t {u64};  // Narrowing
bt2c::SafeS64 {x} + bt2c::SafeS64 {u64}; // Okay (checks `u64`)
bt2c::SafeS64 {x} + safeU32.asS64();     // Okay
@endcode

A compound assignment operator follows the same rules.

Those rules don't apply to:

- The comparison operators: they work with any operand types and
  are mathematically correct (<code>bt2c::SafeInt<int> {-1} < 1U</code>
  is true).

- The left shift operator (<code>&lt;&lt;</code>): the result type is
  the left operand type, the shift count may be of any integer type, and
  the shift count must be less than the width of the left operand type.

<h2>Conversions</h2>

A bt2c::SafeInt is implicitly constructible from an integer or from
another bt2c::SafeInt only when their types have the same signedness
and width (same rule as the arithmetic operators).

Otherwise, the constructor is explicit, and checks the value when some
value of the source type doesn't fit \bt_p{ValT}:

@code{.cpp}
bt2c::SafeInt<std::int32_t> a = std::int32_t {x}; // Okay
bt2c::SafeInt<std::int32_t> b = std::int16_t {y}; // Doesn't build
bt2c::SafeInt<std::int32_t> c {std::int16_t {y}}; // Okay
bt2c::SafeInt<std::int32_t> d {std::int64_t {z}}; // Okay, but checks `z`
@endcode

Get the raw value with bt2c::SafeInt::val() or convert it to a
bt2c::SafeInt of another type, checking the value, with
bt2c::SafeInt::as() or one of its aliases (bt2c::SafeInt::asU8(),
bt2c::SafeInt::asS32(), and the rest).

<h2>Fixed-width aliases</h2>

bt2c::SafeU8, bt2c::SafeU16, bt2c::SafeU32, bt2c::SafeU64,
bt2c::SafeS8, bt2c::SafeS16, bt2c::SafeS32, and bt2c::SafeS64 are
aliases of bt2c::SafeInt with the corresponding fixed-width integer
types, while bt2c::SafeU8Dbg, bt2c::SafeS32Dbg, and the rest are the
bt2c::SafeIntDbg equivalents:

@code{.cpp}
bt2c::SafeS32 {35}; // Same as `bt2c::SafeInt<std::int32_t> {35}`
@endcode

Include \ref fmt.hpp to format a bt2c::SafeInt with {fmt}.
*/

#include <cstdint>
#include <limits>
#include <string>
#include <type_traits>

#include <fmt/format.h>

#include "common/assert.h"
#include "cpp-common/bt2c/exc.hpp"

namespace bt2c {

/*!
@brief
    What a bt2c::SafeInt operation does when it fails.
*/
enum class SafeIntPolicy
{
    /*! Throw bt2c::OverflowError. */
    Throw,

    /*! Assert (only in debug mode, like BT_ASSERT_DBG()). */
    AssertDbg,
};

template <typename ValT, SafeIntPolicy PolicyV = SafeIntPolicy::Throw>
class SafeInt;

/*!
@brief
    bt2c::SafeInt which asserts in debug mode instead of throwing.
*/
template <typename ValT>
using SafeIntDbg = SafeInt<ValT, SafeIntPolicy::AssertDbg>;

namespace internal {

/*
 * Whether or not `T` is a valid raw value type (integer, not `bool` or
 * a character type).
 */
template <typename T>
constexpr auto isLegalSafeIntValType =
    std::is_integral_v<T> && !std::is_same_v<T, bool> && !std::is_same_v<T, char> &&
    !std::is_same_v<T, wchar_t> && !std::is_same_v<T, char16_t> && !std::is_same_v<T, char32_t>;

/*
 * Whether or not `T` is a `SafeInt` type (any raw value type
 * and policy).
 */
template <typename T>
constexpr auto isSafeInt = false;

template <typename ValT, SafeIntPolicy PolicyV>
constexpr auto isSafeInt<SafeInt<ValT, PolicyV>> = true;

/*
 * Returns the plain integer `val` itself.
 */
template <typename ValT>
constexpr ValT rawVal(const ValT val) noexcept
{
    return val;
}

/*
 * Returns the raw value of `safeVal`.
 */
template <typename ValT, SafeIntPolicy PolicyV>
constexpr ValT rawVal(const SafeInt<ValT, PolicyV> safeVal) noexcept
{
    return safeVal.val();
}

/*
 * Result type of an arithmetic operator having the valid operand types
 * `LT` and `RT`: the `SafeInt` one (`LT` if both are).
 */
template <typename LT, typename RT>
using BinOpRes = std::conditional_t<isSafeInt<LT>, LT, RT>;

/*
 * Like C++20's std::cmp_equal().
 */
template <typename ValAT, typename ValBT>
constexpr bool cmpEqual(const ValAT a, const ValBT b) noexcept
{
    if constexpr (std::is_signed_v<ValAT> == std::is_signed_v<ValBT>) {
        return a == b;
    } else if constexpr (std::is_signed_v<ValAT>) {
        return a >= 0 && static_cast<std::make_unsigned_t<ValAT>>(a) == b;
    } else {
        return b >= 0 && a == static_cast<std::make_unsigned_t<ValBT>>(b);
    }
}

/*
 * Like C++20's std::cmp_less().
 */
template <typename ValAT, typename ValBT>
constexpr bool cmpLess(const ValAT a, const ValBT b) noexcept
{
    if constexpr (std::is_signed_v<ValAT> == std::is_signed_v<ValBT>) {
        return a < b;
    } else if constexpr (std::is_signed_v<ValAT>) {
        return a < 0 || static_cast<std::make_unsigned_t<ValAT>>(a) < b;
    } else {
        return b >= 0 && a < static_cast<std::make_unsigned_t<ValBT>>(b);
    }
}

/*
 * Like C++20's std::in_range().
 */
template <typename DstValT, typename SrcValT>
constexpr bool inRange(const SrcValT val) noexcept
{
    return !cmpLess(val, std::numeric_limits<DstValT>::min()) &&
           !cmpLess(std::numeric_limits<DstValT>::max(), val);
}

/*
 * Implementation of `isLosslessConv` below.
 *
 * Overload resolution may evaluate `isLosslessConv` with non-integer
 * types, for which `std::numeric_limits` might not compile: unlike
 * `&&`, `if constexpr` prevents this instantiation.
 */
template <typename SrcValT, typename DstValT>
constexpr bool isLosslessConvFunc() noexcept
{
    if constexpr (isLegalSafeIntValType<SrcValT> && isLegalSafeIntValType<DstValT>) {
        return inRange<DstValT>(std::numeric_limits<SrcValT>::min()) &&
               inRange<DstValT>(std::numeric_limits<SrcValT>::max());
    } else {
        return false;
    }
}

/*
 * Whether or not `SrcValT` and `DstValT` are valid integer types and
 * any value of `SrcValT` fits `DstValT`.
 */
template <typename SrcValT, typename DstValT>
constexpr auto isLosslessConv = isLosslessConvFunc<SrcValT, DstValT>();

/*
 * Description of the integer type `ValT` for a failure reason, like "a
 * 64-bit unsigned integer".
 */
template <typename ValT>
std::string typeDescr()
{
    return fmt::format("{} {}-bit {} integer", sizeof(ValT) == 1 ? "an" : "a", sizeof(ValT) * 8,
                       std::is_signed_v<ValT> ? "signed" : "unsigned");
}

/*
 * Whether or not the operations of a `SafeInt` having the policy
 * `PolicyV` are `noexcept` (only `SafeIntPolicy::Throw` throws).
 */
template <SafeIntPolicy PolicyV>
constexpr auto isNoExcept = PolicyV == SafeIntPolicy::AssertDbg;

/*
 * Fails, according to `PolicyV`, if `ok` is false.
 *
 * `makeReason()` returns the reason of the failure; this function only
 * calls it on failure.
 */
template <SafeIntPolicy PolicyV, typename MakeReasonFuncT>
constexpr void check([[maybe_unused]] const bool ok,
                     [[maybe_unused]] MakeReasonFuncT&& makeReason) noexcept(isNoExcept<PolicyV>)
{
    if constexpr (PolicyV == SafeIntPolicy::Throw) {
        if (!ok) {
            throw OverflowError {makeReason()};
        }
    } else {
#ifdef BT_DEBUG_MODE
        if (!ok) {
            bt_common_assert_failed(__FILE__, __LINE__, __func__, makeReason().c_str());
        }
#endif
    }
}

/*
 * Whether or not the integer types `ValAT` and `ValBT` may be the
 * operand types of an arithmetic operator, that is, whether or not they
 * have the same signedness and width.
 *
 * This isn't `std::is_same_v` so that, for example, `long` and
 * `long long` are compatible on a platform where they have the same
 * width: whether `std::int64_t` is one or the other varies.
 */
template <typename ValAT, typename ValBT>
constexpr auto areCompatValTypes =
    std::is_signed_v<ValAT> == std::is_signed_v<ValBT> && sizeof(ValAT) == sizeof(ValBT);

/*
 * Statically asserts that `ValAT` and `ValBT` are compatible operand
 * types (see `areCompatValTypes`).
 */
template <typename ValAT, typename ValBT>
constexpr void staticAssertCompatValTypes() noexcept
{
    static_assert(areCompatValTypes<ValAT, ValBT>,
                  "Operands must have the same signedness and width: convert one with as().");
}

/*
 * Whether or not a `SafeInt` of `DstValT` is implicitly constructible
 * from `SrcValT`, that is, whether or not `SrcValT` is a valid raw
 * value type compatible with `DstValT` (see `areCompatValTypes`).
 */
template <typename SrcValT, typename DstValT>
constexpr bool isImplicitConv =
    isLegalSafeIntValType<SrcValT> && areCompatValTypes<SrcValT, DstValT>;

/*
 * Returns `val` as `DstValT`, failing according to `PolicyV` if `val`
 * doesn't fit `DstValT`.
 */
template <typename DstValT, SafeIntPolicy PolicyV, typename SrcValT>
constexpr DstValT conv(const SrcValT val) noexcept(isNoExcept<PolicyV>)
{
    check<PolicyV>(inRange<DstValT>(val), [&] {
        return fmt::format("`{}` doesn't fit {}", val, typeDescr<DstValT>());
    });

    return static_cast<DstValT>(val);
}

/*
 * Returns `a + b` as `ValAT`, failing according to `PolicyV` if the
 * exact result doesn't fit `ValAT`.
 *
 * `ValAT` and `ValBT` must be compatible
 * (see staticAssertCompatValTypes()).
 */
template <SafeIntPolicy PolicyV, typename ValAT, typename ValBT>
constexpr ValAT add(const ValAT a, const ValBT b) noexcept(isNoExcept<PolicyV>)
{
    staticAssertCompatValTypes<ValAT, ValBT>();

    ValAT res {};
    const bool overflows = __builtin_add_overflow(a, b, &res);

    check<PolicyV>(!overflows, [&] {
        return fmt::format("`{} + {}` doesn't fit {}", a, b, typeDescr<ValAT>());
    });

    return res;
}

/*
 * Like add(), but for `a - b`.
 */
template <SafeIntPolicy PolicyV, typename ValAT, typename ValBT>
constexpr ValAT sub(const ValAT a, const ValBT b) noexcept(isNoExcept<PolicyV>)
{
    staticAssertCompatValTypes<ValAT, ValBT>();

    ValAT res {};
    const bool overflows = __builtin_sub_overflow(a, b, &res);

    check<PolicyV>(!overflows, [&] {
        return fmt::format("`{} - {}` doesn't fit {}", a, b, typeDescr<ValAT>());
    });

    return res;
}

/*
 * Like add(), but for `a * b`.
 */
template <SafeIntPolicy PolicyV, typename ValAT, typename ValBT>
constexpr ValAT mul(const ValAT a, const ValBT b) noexcept(isNoExcept<PolicyV>)
{
    staticAssertCompatValTypes<ValAT, ValBT>();

    ValAT res {};
    const bool overflows = __builtin_mul_overflow(a, b, &res);

    check<PolicyV>(!overflows, [&] {
        return fmt::format("`{} * {}` doesn't fit {}", a, b, typeDescr<ValAT>());
    });

    return res;
}

/*
 * Like add(), but for `a / b` (truncated towards zero), also failing if
 * `b` is zero.
 */
template <SafeIntPolicy PolicyV, typename ValAT, typename ValBT>
constexpr ValAT div(const ValAT a, const ValBT b) noexcept(isNoExcept<PolicyV>)
{
    staticAssertCompatValTypes<ValAT, ValBT>();

    check<PolicyV>(b != 0, [&] {
        return fmt::format("Division by zero: `{} / 0`", a);
    });

    if constexpr (std::is_signed_v<ValAT>) {
        /* `-min` doesn't fit `ValAT` */
        check<PolicyV>(!(a == std::numeric_limits<ValAT>::min() && b == -1), [&] {
            return fmt::format("`{} / {}` doesn't fit {}", a, b, typeDescr<ValAT>());
        });
    }

    return static_cast<ValAT>(a / static_cast<ValAT>(b));
}

/*
 * Returns `val` shifted left by `count` bits, failing according to
 * `PolicyV` if `count` is negative, if `count` isn't less than the
 * width of `ValT`, or if the exact result doesn't fit `ValT`.
 */
template <typename ValT, SafeIntPolicy PolicyV, typename CountT>
constexpr ValT shiftLeft(const ValT val, const CountT count) noexcept(isNoExcept<PolicyV>)
{
    check<PolicyV>(!cmpLess(count, 0) &&
                       cmpLess(count, std::numeric_limits<std::make_unsigned_t<ValT>>::digits),
                   [&] {
                       return fmt::format("Invalid shift count for {}: `{} << {}`",
                                          typeDescr<ValT>(), val, count);
                   });

    const auto intCount = static_cast<int>(count);
    auto fits = val <= (std::numeric_limits<ValT>::max() >> intCount);

    if constexpr (std::is_signed_v<ValT>) {
        if (val < 0) {
            fits = val >= (std::numeric_limits<ValT>::min() >> intCount);
        }
    }

    check<PolicyV>(fits, [&] {
        return fmt::format("`{} << {}` doesn't fit {}", val, count, typeDescr<ValT>());
    });

    return static_cast<ValT>(static_cast<std::make_unsigned_t<ValT>>(val) << intCount);
}

/*
 * Whether or not `LT` and `RT` make a valid pair of operands: at least
 * one of them is a `SafeInt`, and the other one is either a `SafeInt`
 * with the same policy or an integer.
 *
 * The `SafeInt` operators below (arithmetic, comparison, left shift,
 * and compound assignment) use this as their `std::enable_if` condition
 * so that they only participate in overload resolution when a `SafeInt`
 * is involved, not for any other pair of types.
 */
template <typename LT, typename RT>
constexpr bool isOperandPair() noexcept
{
    if constexpr (isSafeInt<LT> && isSafeInt<RT>) {
        return LT::policy == RT::policy;
    } else if constexpr (isSafeInt<LT>) {
        return isLegalSafeIntValType<RT>;
    } else if constexpr (isSafeInt<RT>) {
        return isLegalSafeIntValType<LT>;
    } else {
        return false;
    }
}

} /* namespace internal */

/*!
@brief
    Checked integer of type \bt_p{ValT} having the
    policy \bt_p{PolicyV}.

See the file documentation.

A SafeInt instance only contains a \bt_p{ValT} value: pass and return
by copy.
*/
template <typename ValT, SafeIntPolicy PolicyV>
class SafeInt final
{
    static_assert(internal::isLegalSafeIntValType<ValT>,
                  "`ValT` is an integer type (not `bool` or a character type).");

private:
    static constexpr bool _isNoExcept = internal::isNoExcept<PolicyV>;

    template <typename UT>
    static constexpr bool _isOtherOperand = internal::isOperandPair<SafeInt, UT>();

public:
    /*! @brief Raw value type. */
    using Val = ValT;

    /*! @brief Policy. */
    static constexpr SafeIntPolicy policy = PolicyV;

    /*!
    @brief
        Builds a safe integer having the value zero.
    */
    constexpr SafeInt() noexcept = default;

    /*!
    @brief
        Builds a safe integer having the value \bt_p{val}.

    Only participates in overload resolution when \bt_p{SrcValT} and
    \bt_p{ValT} have the same signedness and width.

    @param[in] val
        Value of this safe integer.
    */
    template <typename SrcValT, std::enable_if_t<internal::isImplicitConv<SrcValT, ValT>, int> = 0>
    constexpr SafeInt(const SrcValT val) noexcept
        : _mVal {static_cast<ValT>(val)}
    {
    }

    /*!
    @brief
        Builds a safe integer having the value \bt_p{val}.

    Only participates in overload resolution when any value of
    \bt_p{SrcValT} fits \bt_p{ValT}, but \bt_p{SrcValT} and
    \bt_p{ValT} don't have the same signedness and width.

    @param[in] val
        Value of this safe integer.
    */
    template <typename SrcValT, std::enable_if_t<internal::isLosslessConv<SrcValT, ValT> &&
                                                     !internal::isImplicitConv<SrcValT, ValT>,
                                                 int> = 0>
    constexpr explicit SafeInt(const SrcValT val) noexcept
        : _mVal {static_cast<ValT>(val)}
    {
    }

    /*!
    @brief
        Builds a safe integer having the value \bt_p{val}, failing if
        \bt_p{val} doesn't fit \bt_p{ValT}.

    Only participates in overload resolution when some value of
    \bt_p{SrcValT} doesn't fit \bt_p{ValT}.

    Fails according to \bt_p{PolicyV} (see SafeIntPolicy).

    @param[in] val
        Value of this safe integer.
    */
    template <typename SrcValT, std::enable_if_t<internal::isLegalSafeIntValType<SrcValT> &&
                                                     !internal::isLosslessConv<SrcValT, ValT>,
                                                 int> = 0>
    constexpr explicit SafeInt(const SrcValT val) noexcept(_isNoExcept)
        : _mVal {internal::conv<ValT, PolicyV>(val)}
    {
    }

    /*!
    @brief
        Builds a safe integer having the value of \bt_p{safeVal}.

    Only participates in overload resolution when \bt_p{SrcValT} and
    \bt_p{ValT} have the same signedness and width.

    @param[in] safeVal
        Safe integer of which to copy the value.
    */
    template <typename SrcValT, std::enable_if_t<internal::isImplicitConv<SrcValT, ValT>, int> = 0>
    constexpr SafeInt(const SafeInt<SrcValT, PolicyV> safeVal) noexcept
        : _mVal {static_cast<ValT>(safeVal.val())}
    {
    }

    /*!
    @brief
        Builds a safe integer having the value of \bt_p{safeVal}.

    Only participates in overload resolution when any value of
    \bt_p{SrcValT} fits \bt_p{ValT}, but \bt_p{SrcValT} and
    \bt_p{ValT} don't have the same signedness and width.

    Use as() to convert a safe integer of which some value doesn't
    fit \bt_p{ValT}.

    @param[in] safeVal
        Safe integer of which to copy the value.
    */
    template <typename SrcValT, std::enable_if_t<internal::isLosslessConv<SrcValT, ValT> &&
                                                     !internal::isImplicitConv<SrcValT, ValT>,
                                                 int> = 0>
    constexpr explicit SafeInt(const SafeInt<SrcValT, PolicyV> safeVal) noexcept
        : _mVal {static_cast<ValT>(safeVal.val())}
    {
    }

    /*!
    @brief
        Raw value.

    @returns
        Raw value.
    */
    constexpr ValT val() const noexcept
    {
        return _mVal;
    }

    /*!
    @brief
        This safe integer as a safe integer of type \bt_p{UT} having
        the policy \bt_p{PolicyV}.

    Fails, according to \bt_p{PolicyV} (see SafeIntPolicy), if the
    raw value doesn't fit \bt_p{UT}.

    @returns
        This safe integer as a safe integer of type \bt_p{UT}.
    */
    template <typename UT>
    constexpr SafeInt<UT, PolicyV> as() const noexcept(_isNoExcept)
    {
        static_assert(internal::isLegalSafeIntValType<UT>,
                      "`UT` is an integer type (not `bool` or a character type).");
        return SafeInt<UT, PolicyV> {internal::conv<UT, PolicyV>(_mVal)};
    }

    /*!
    @brief
        Alias of as() with <code>std::uint8_t</code>.

    @returns
        See as().
    */
    constexpr SafeInt<std::uint8_t, PolicyV> asU8() const noexcept(_isNoExcept)
    {
        return this->as<std::uint8_t>();
    }

    /*!
    @brief
        Alias of as() with <code>std::uint16_t</code>.

    @returns
        See as().
    */
    constexpr SafeInt<std::uint16_t, PolicyV> asU16() const noexcept(_isNoExcept)
    {
        return this->as<std::uint16_t>();
    }

    /*!
    @brief
        Alias of as() with <code>std::uint32_t</code>.

    @returns
        See as().
    */
    constexpr SafeInt<std::uint32_t, PolicyV> asU32() const noexcept(_isNoExcept)
    {
        return this->as<std::uint32_t>();
    }

    /*!
    @brief
        Alias of as() with <code>std::uint64_t</code>.

    @returns
        See as().
    */
    constexpr SafeInt<std::uint64_t, PolicyV> asU64() const noexcept(_isNoExcept)
    {
        return this->as<std::uint64_t>();
    }

    /*!
    @brief
        Alias of as() with <code>std::int8_t</code>.

    @returns
        See as().
    */
    constexpr SafeInt<std::int8_t, PolicyV> asS8() const noexcept(_isNoExcept)
    {
        return this->as<std::int8_t>();
    }

    /*!
    @brief
        Alias of as() with <code>std::int16_t</code>.

    @returns
        See as().
    */
    constexpr SafeInt<std::int16_t, PolicyV> asS16() const noexcept(_isNoExcept)
    {
        return this->as<std::int16_t>();
    }

    /*!
    @brief
        Alias of as() with <code>std::int32_t</code>.

    @returns
        See as().
    */
    constexpr SafeInt<std::int32_t, PolicyV> asS32() const noexcept(_isNoExcept)
    {
        return this->as<std::int32_t>();
    }

    /*!
    @brief
        Alias of as() with <code>std::int64_t</code>.

    @returns
        See as().
    */
    constexpr SafeInt<std::int64_t, PolicyV> asS64() const noexcept(_isNoExcept)
    {
        return this->as<std::int64_t>();
    }

    /*!
    @brief
        Adds \bt_p{other} to this value.

    \bt_p{other} is either an integer or a SafeInt having the
    policy \bt_p{PolicyV}: see the file documentation for the valid
    operand types.

    Fails, according to \bt_p{PolicyV} (see SafeIntPolicy), if the
    exact result doesn't fit \bt_p{ValT}. On failure with the
    SafeIntPolicy::Throw policy, this value doesn't change.

    @param[in] other
        Value to add.

    @returns
        This safe integer.
    */
    template <typename UT, std::enable_if_t<_isOtherOperand<UT>, int> = 0>
    constexpr SafeInt& operator+=(const UT other) noexcept(_isNoExcept)
    {
        _mVal = internal::add<PolicyV>(_mVal, internal::rawVal(other));
        return *this;
    }

    /*!
    @brief
        Subtracts \bt_p{other} from this value.

    \bt_p{other} is either an integer or a SafeInt having the
    policy \bt_p{PolicyV}: see the file documentation for the valid
    operand types.

    Fails, according to \bt_p{PolicyV} (see SafeIntPolicy), if the
    exact result doesn't fit \bt_p{ValT}. On failure with the
    SafeIntPolicy::Throw policy, this value doesn't change.

    @param[in] other
        Value to subtract.

    @returns
        This safe integer.
    */
    template <typename UT, std::enable_if_t<_isOtherOperand<UT>, int> = 0>
    constexpr SafeInt& operator-=(const UT other) noexcept(_isNoExcept)
    {
        _mVal = internal::sub<PolicyV>(_mVal, internal::rawVal(other));
        return *this;
    }

    /*!
    @brief
        Multiplies this value by \bt_p{other}.

    \bt_p{other} is either an integer or a SafeInt having the
    policy \bt_p{PolicyV}: see the file documentation for the valid
    operand types.

    Fails, according to \bt_p{PolicyV} (see SafeIntPolicy), if the
    exact result doesn't fit \bt_p{ValT}. On failure with the
    SafeIntPolicy::Throw policy, this value doesn't change.

    @param[in] other
        Value by which to multiply.

    @returns
        This safe integer.
    */
    template <typename UT, std::enable_if_t<_isOtherOperand<UT>, int> = 0>
    constexpr SafeInt& operator*=(const UT other) noexcept(_isNoExcept)
    {
        _mVal = internal::mul<PolicyV>(_mVal, internal::rawVal(other));
        return *this;
    }

    /*!
    @brief
        Divides this value by \bt_p{other}, truncating towards zero.

    \bt_p{other} is either an integer or a SafeInt having the
    policy \bt_p{PolicyV}: see the file documentation for the valid
    operand types.

    Fails, according to \bt_p{PolicyV} (see SafeIntPolicy), if
    \bt_p{other} is zero or if the exact result doesn't fit
    \bt_p{ValT}. On failure with the SafeIntPolicy::Throw policy,
    this value doesn't change.

    @param[in] other
        Divisor.

    @returns
        This safe integer.
    */
    template <typename UT, std::enable_if_t<_isOtherOperand<UT>, int> = 0>
    constexpr SafeInt& operator/=(const UT other) noexcept(_isNoExcept)
    {
        _mVal = internal::div<PolicyV>(_mVal, internal::rawVal(other));
        return *this;
    }

private:
    ValT _mVal = 0;
};

template <typename ValT>
SafeInt(ValT) -> SafeInt<ValT>;

/*!
@brief
    bt2c::SafeInt of <code>std::uint8_t</code>.
*/
using SafeU8 = SafeInt<std::uint8_t>;

/*!
@brief
    bt2c::SafeInt of <code>std::uint16_t</code>.
*/
using SafeU16 = SafeInt<std::uint16_t>;

/*!
@brief
    bt2c::SafeInt of <code>std::uint32_t</code>.
*/
using SafeU32 = SafeInt<std::uint32_t>;

/*!
@brief
    bt2c::SafeInt of <code>std::uint64_t</code>.
*/
using SafeU64 = SafeInt<std::uint64_t>;

/*!
@brief
    bt2c::SafeInt of <code>std::int8_t</code>.
*/
using SafeS8 = SafeInt<std::int8_t>;

/*!
@brief
    bt2c::SafeInt of <code>std::int16_t</code>.
*/
using SafeS16 = SafeInt<std::int16_t>;

/*!
@brief
    bt2c::SafeInt of <code>std::int32_t</code>.
*/
using SafeS32 = SafeInt<std::int32_t>;

/*!
@brief
    bt2c::SafeInt of <code>std::int64_t</code>.
*/
using SafeS64 = SafeInt<std::int64_t>;

/*!
@brief
    bt2c::SafeIntDbg of <code>std::uint8_t</code>.
*/
using SafeU8Dbg = SafeIntDbg<std::uint8_t>;

/*!
@brief
    bt2c::SafeIntDbg of <code>std::uint16_t</code>.
*/
using SafeU16Dbg = SafeIntDbg<std::uint16_t>;

/*!
@brief
    bt2c::SafeIntDbg of <code>std::uint32_t</code>.
*/
using SafeU32Dbg = SafeIntDbg<std::uint32_t>;

/*!
@brief
    bt2c::SafeIntDbg of <code>std::uint64_t</code>.
*/
using SafeU64Dbg = SafeIntDbg<std::uint64_t>;

/*!
@brief
    bt2c::SafeIntDbg of <code>std::int8_t</code>.
*/
using SafeS8Dbg = SafeIntDbg<std::int8_t>;

/*!
@brief
    bt2c::SafeIntDbg of <code>std::int16_t</code>.
*/
using SafeS16Dbg = SafeIntDbg<std::int16_t>;

/*!
@brief
    bt2c::SafeIntDbg of <code>std::int32_t</code>.
*/
using SafeS32Dbg = SafeIntDbg<std::int32_t>;

/*!
@brief
    bt2c::SafeIntDbg of <code>std::int64_t</code>.
*/
using SafeS64Dbg = SafeIntDbg<std::int64_t>;

/*!
@brief
    Returns \bt_p{l}&nbsp;+&nbsp;\bt_p{r}.

At least one of \bt_p{l} and \bt_p{r} is a SafeInt: see the file
documentation for the valid operand types and the result type.

Fails, according to the policy (see SafeIntPolicy), if the exact
result doesn't fit the result type.

@param[in] l
    First term.
@param[in] r
    Second term.

@returns
    \bt_p{l}&nbsp;+&nbsp;\bt_p{r}.
*/
template <typename LT, typename RT, std::enable_if_t<internal::isOperandPair<LT, RT>(), int> = 0>
constexpr internal::BinOpRes<LT, RT>
operator+(const LT l, const RT r) noexcept(internal::isNoExcept<internal::BinOpRes<LT, RT>::policy>)
{
    using Res = internal::BinOpRes<LT, RT>;

    return Res {internal::add<Res::policy>(internal::rawVal(l), internal::rawVal(r))};
}

/*!
@brief
    Returns \bt_p{l}&nbsp;−&nbsp;\bt_p{r}.

At least one of \bt_p{l} and \bt_p{r} is a SafeInt: see the file
documentation for the valid operand types and the result type.

Fails, according to the policy (see SafeIntPolicy), if the exact
result doesn't fit the result type.

@param[in] l
    Minuend.
@param[in] r
    Subtrahend.

@returns
    \bt_p{l}&nbsp;−&nbsp;\bt_p{r}.
*/
template <typename LT, typename RT, std::enable_if_t<internal::isOperandPair<LT, RT>(), int> = 0>
constexpr internal::BinOpRes<LT, RT>
operator-(const LT l, const RT r) noexcept(internal::isNoExcept<internal::BinOpRes<LT, RT>::policy>)
{
    using Res = internal::BinOpRes<LT, RT>;

    return Res {internal::sub<Res::policy>(internal::rawVal(l), internal::rawVal(r))};
}

/*!
@brief
    Returns \bt_p{l}&nbsp;×&nbsp;\bt_p{r}.

At least one of \bt_p{l} and \bt_p{r} is a SafeInt: see the file
documentation for the valid operand types and the result type.

Fails, according to the policy (see SafeIntPolicy), if the exact
result doesn't fit the result type.

@param[in] l
    First factor.
@param[in] r
    Second factor.

@returns
    \bt_p{l}&nbsp;×&nbsp;\bt_p{r}.
*/
template <typename LT, typename RT, std::enable_if_t<internal::isOperandPair<LT, RT>(), int> = 0>
constexpr internal::BinOpRes<LT, RT>
operator*(const LT l, const RT r) noexcept(internal::isNoExcept<internal::BinOpRes<LT, RT>::policy>)
{
    using Res = internal::BinOpRes<LT, RT>;

    return Res {internal::mul<Res::policy>(internal::rawVal(l), internal::rawVal(r))};
}

/*!
@brief
    Returns \bt_p{l}&nbsp;÷&nbsp;\bt_p{r}, truncated towards zero.

At least one of \bt_p{l} and \bt_p{r} is a SafeInt: see the file
documentation for the valid operand types and the result type.

Fails, according to the policy (see SafeIntPolicy), if \bt_p{r}
is zero or if the exact result doesn't fit the result type.

@param[in] l
    Dividend.
@param[in] r
    Divisor.

@returns
    \bt_p{l}&nbsp;÷&nbsp;\bt_p{r}, truncated towards zero.
*/
template <typename LT, typename RT, std::enable_if_t<internal::isOperandPair<LT, RT>(), int> = 0>
constexpr internal::BinOpRes<LT, RT>
operator/(const LT l, const RT r) noexcept(internal::isNoExcept<internal::BinOpRes<LT, RT>::policy>)
{
    using Res = internal::BinOpRes<LT, RT>;

    return Res {internal::div<Res::policy>(internal::rawVal(l), internal::rawVal(r))};
}

/*!
@brief
    Returns \bt_p{safeVal} shifted left by \bt_p{count} bits.

\bt_p{count} is either an integer of any type or a SafeInt having
the policy \bt_p{PolicyV}.

Fails, according to \bt_p{PolicyV} (see SafeIntPolicy), if
\bt_p{count} is negative, if \bt_p{count} isn't less than the width of
\bt_p{ValT}, or if the exact result doesn't fit \bt_p{ValT}.

@param[in] safeVal
    Safe integer to shift.
@param[in] count
    Number of bits by which to shift \bt_p{safeVal}.

@returns
    \bt_p{safeVal}&nbsp;×&nbsp;2<sup>\bt_p{count}</sup>, as a
    SafeInt of type \bt_p{ValT}.
*/
template <typename ValT, SafeIntPolicy PolicyV, typename CountT,
          std::enable_if_t<internal::isOperandPair<SafeInt<ValT, PolicyV>, CountT>(), int> = 0>
constexpr SafeInt<ValT, PolicyV>
operator<<(const SafeInt<ValT, PolicyV> safeVal,
           const CountT count) noexcept(internal::isNoExcept<PolicyV>)
{
    return SafeInt<ValT, PolicyV> {
        internal::shiftLeft<ValT, PolicyV>(safeVal.val(), internal::rawVal(count))};
}

/*!
@brief
    Returns whether or not \bt_p{l} is equal to \bt_p{r}.

At least one of \bt_p{l} and \bt_p{r} is a SafeInt; the other one
may be of any integer type.

The comparison is mathematically correct, whatever the signedness of
the operand types: <code>SafeInt {-1} == UINT_MAX</code>
is false.

@param[in] l
    Left operand.
@param[in] r
    Right operand.

@returns
    \c true if \bt_p{l} is equal to \bt_p{r}.
*/
template <typename LT, typename RT, std::enable_if_t<internal::isOperandPair<LT, RT>(), int> = 0>
constexpr bool operator==(const LT l, const RT r) noexcept
{
    return internal::cmpEqual(internal::rawVal(l), internal::rawVal(r));
}

/*!
@brief
    Returns whether or not \bt_p{l} isn't equal to \bt_p{r}.

At least one of \bt_p{l} and \bt_p{r} is a SafeInt; the other one
may be of any integer type.

The comparison is mathematically correct, whatever the signedness of
the operand types: <code>SafeInt {-1} != UINT_MAX</code> is
true.

@param[in] l
    Left operand.
@param[in] r
    Right operand.

@returns
    \c true if \bt_p{l} isn't equal to \bt_p{r}.
*/
template <typename LT, typename RT, std::enable_if_t<internal::isOperandPair<LT, RT>(), int> = 0>
constexpr bool operator!=(const LT l, const RT r) noexcept
{
    return !(l == r);
}

/*!
@brief
    Returns whether or not \bt_p{l} is less than \bt_p{r}.

At least one of \bt_p{l} and \bt_p{r} is a SafeInt; the other one
may be of any integer type.

The comparison is mathematically correct, whatever the signedness of
the operand types: <code>SafeInt {-1} < 1U</code> is true.

@param[in] l
    Left operand.
@param[in] r
    Right operand.

@returns
    \c true if \bt_p{l} is less than \bt_p{r}.
*/
template <typename LT, typename RT, std::enable_if_t<internal::isOperandPair<LT, RT>(), int> = 0>
constexpr bool operator<(const LT l, const RT r) noexcept
{
    return internal::cmpLess(internal::rawVal(l), internal::rawVal(r));
}

/*!
@brief
    Returns whether or not \bt_p{l} is greater than \bt_p{r}.

At least one of \bt_p{l} and \bt_p{r} is a SafeInt; the other one
may be of any integer type.

The comparison is mathematically correct, whatever the signedness of
the operand types: <code>1U > SafeInt {-1}</code> is true.

@param[in] l
    Left operand.
@param[in] r
    Right operand.

@returns
    \c true if \bt_p{l} is greater than \bt_p{r}.
*/
template <typename LT, typename RT, std::enable_if_t<internal::isOperandPair<LT, RT>(), int> = 0>
constexpr bool operator>(const LT l, const RT r) noexcept
{
    return r < l;
}

/*!
@brief
    Returns whether or not \bt_p{l} is less than or equal
    to \bt_p{r}.

At least one of \bt_p{l} and \bt_p{r} is a SafeInt; the other one
may be of any integer type.

The comparison is mathematically correct, whatever the signedness of
the operand types: <code>SafeInt<int> {-1} <= 0U</code> is true.

@param[in] l
    Left operand.
@param[in] r
    Right operand.

@returns
    \c true if \bt_p{l} is less than or equal to \bt_p{r}.
*/
template <typename LT, typename RT, std::enable_if_t<internal::isOperandPair<LT, RT>(), int> = 0>
constexpr bool operator<=(const LT l, const RT r) noexcept
{
    return !(r < l);
}

/*!
@brief
    Returns whether or not \bt_p{l} is greater than or equal
    to \bt_p{r}.

At least one of \bt_p{l} and \bt_p{r} is a SafeInt; the other one
may be of any integer type.

The comparison is mathematically correct, whatever the signedness of
the operand types: <code>0U >= SafeInt<int> {-1}</code> is true.

@param[in] l
    Left operand.
@param[in] r
    Right operand.

@returns
    \c true if \bt_p{l} is greater than or equal to \bt_p{r}.
*/
template <typename LT, typename RT, std::enable_if_t<internal::isOperandPair<LT, RT>(), int> = 0>
constexpr bool operator>=(const LT l, const RT r) noexcept
{
    return !(l < r);
}

} /* namespace bt2c */

#endif /* BABELTRACE_CPP_COMMON_BT2C_SAFE_INT_HPP */
