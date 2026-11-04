/*
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Copyright (C) 2026 EfficiOS, Inc.
 */

#include <cstdint>
#include <limits>
#include <type_traits>
#include <utility>

#include <fmt/format.h>

#include "cpp-common/bt2c/fmt.hpp" /* IWYU pragma: keep */
#include "cpp-common/bt2c/safe-int.hpp"

#include "catch2/catch_test_macros.hpp"
#include "catch2/matchers/catch_matchers.hpp"

namespace {

constexpr auto u64Max = std::numeric_limits<std::uint64_t>::max();
constexpr auto i64Min = std::numeric_limits<std::int64_t>::min();
constexpr auto i64Max = std::numeric_limits<std::int64_t>::max();

/*
 * Two distinct types having the same signedness and width: `long` and
 * `long long` with LP64, or `int` and `long` with ILP32 and LLP64.
 */
using SameWidthA = std::conditional_t<sizeof(long) == sizeof(long long), long, int>;
using SameWidthB = std::conditional_t<sizeof(long) == sizeof(long long), long long, long>;

static_assert(!std::is_same_v<SameWidthA, SameWidthB>);
static_assert(sizeof(SameWidthA) == sizeof(SameWidthB));

} /* namespace */

TEST_CASE("`bt2c::SafeInt`: default value is zero")
{
    CHECK(bt2c::SafeInt<int> {}.val() == 0);
}

TEST_CASE("`bt2c::SafeInt`: implicitly constructible from a type having the same signedness and "
          "width")
{
    STATIC_REQUIRE(std::is_convertible_v<std::uint32_t, bt2c::SafeInt<std::uint32_t>>);
    STATIC_REQUIRE(std::is_convertible_v<std::int64_t, bt2c::SafeInt<std::int64_t>>);
    STATIC_REQUIRE(std::is_convertible_v<SameWidthB, bt2c::SafeInt<SameWidthA>>);
}

TEST_CASE("`bt2c::SafeInt`: only explicitly constructible from another type of which any value "
          "fits")
{
    STATIC_REQUIRE_FALSE(std::is_convertible_v<std::int16_t, bt2c::SafeInt<std::int32_t>>);
    STATIC_REQUIRE_FALSE(std::is_convertible_v<std::uint32_t, bt2c::SafeInt<std::int64_t>>);
    STATIC_REQUIRE(std::is_constructible_v<bt2c::SafeInt<std::int32_t>, std::int16_t>);
    STATIC_REQUIRE(noexcept(bt2c::SafeInt<std::int32_t> {std::int16_t {}}));
    CHECK(bt2c::SafeInt<std::int32_t> {std::int16_t {-12}}.val() == -12);
}

TEST_CASE("`bt2c::SafeInt`: only explicitly constructible from a type of which some value "
          "doesn't fit")
{
    STATIC_REQUIRE_FALSE(std::is_convertible_v<std::int32_t, bt2c::SafeInt<std::uint64_t>>);
    STATIC_REQUIRE_FALSE(std::is_convertible_v<std::uint64_t, bt2c::SafeInt<std::int64_t>>);
    STATIC_REQUIRE(std::is_constructible_v<bt2c::SafeInt<std::int64_t>, std::uint64_t>);
}

TEST_CASE("`bt2c::SafeInt`: explicit construction keeps a value which fits")
{
    CHECK(bt2c::SafeInt<std::int64_t> {std::uint64_t {12}}.val() == 12);
}

TEST_CASE("`bt2c::SafeInt`: explicit construction throws when the value doesn't fit")
{
    CHECK_THROWS_WITH(bt2c::SafeInt<std::int64_t> {u64Max},
                      "`18446744073709551615` doesn't fit a 64-bit signed integer");
    CHECK_THROWS_WITH(bt2c::SafeInt<std::uint32_t> {-1},
                      "`-1` doesn't fit a 32-bit unsigned integer");
}

TEST_CASE("`bt2c::SafeInt`: implicitly constructible from a `bt2c::SafeInt` having the same "
          "signedness and width")
{
    STATIC_REQUIRE(std::is_convertible_v<bt2c::SafeInt<SameWidthB>, bt2c::SafeInt<SameWidthA>>);
}

TEST_CASE("`bt2c::SafeInt`: only explicitly constructible from a narrower `bt2c::SafeInt`")
{
    STATIC_REQUIRE_FALSE(
        std::is_convertible_v<bt2c::SafeInt<std::uint8_t>, bt2c::SafeInt<std::int16_t>>);
    STATIC_REQUIRE(
        std::is_constructible_v<bt2c::SafeInt<std::int16_t>, bt2c::SafeInt<std::uint8_t>>);
    CHECK(bt2c::SafeInt<std::int16_t> {bt2c::SafeInt<std::uint8_t> {255U}}.val() == 255);
}

TEST_CASE("`bt2c::SafeInt`: not constructible from a wider `bt2c::SafeInt`")
{
    STATIC_REQUIRE_FALSE(
        std::is_constructible_v<bt2c::SafeInt<std::uint8_t>, bt2c::SafeInt<std::int32_t>>);
}

TEST_CASE("`bt2c::SafeInt`: not convertible to its raw type")
{
    STATIC_REQUIRE_FALSE(std::is_convertible_v<bt2c::SafeInt<std::uint64_t>, std::uint64_t>);
}

TEST_CASE("bt2c::SafeInt::as() returns a `bt2c::SafeInt` having the same policy")
{
    STATIC_REQUIRE(std::is_same_v<decltype(bt2c::SafeInt<std::uint64_t> {}.as<std::int8_t>()),
                                  bt2c::SafeInt<std::int8_t>>);
    STATIC_REQUIRE(std::is_same_v<decltype(bt2c::SafeIntDbg<std::uint64_t> {}.as<std::int8_t>()),
                                  bt2c::SafeIntDbg<std::int8_t>>);
}

TEST_CASE("bt2c::SafeInt::as() returns a value which fits")
{
    CHECK(bt2c::SafeInt<std::uint64_t> {42U}.as<std::int8_t>() == 42);
    CHECK(bt2c::SafeInt<std::int8_t> {-42}.as<std::int64_t>() == -42);
}

TEST_CASE("bt2c::SafeInt::as() throws when the value doesn't fit")
{
    CHECK_THROWS_WITH(bt2c::SafeInt<std::uint64_t> {u64Max}.as<std::int64_t>(),
                      "`18446744073709551615` doesn't fit a 64-bit signed integer");
    CHECK_THROWS_WITH(bt2c::SafeInt<std::int32_t> {-1}.as<std::uint64_t>(),
                      "`-1` doesn't fit a 64-bit unsigned integer");
}

TEST_CASE("`bt2c::SafeInt`: fixed-width aliases")
{
    STATIC_REQUIRE(std::is_same_v<bt2c::SafeU8, bt2c::SafeInt<std::uint8_t>>);
    STATIC_REQUIRE(std::is_same_v<bt2c::SafeU16, bt2c::SafeInt<std::uint16_t>>);
    STATIC_REQUIRE(std::is_same_v<bt2c::SafeU32, bt2c::SafeInt<std::uint32_t>>);
    STATIC_REQUIRE(std::is_same_v<bt2c::SafeU64, bt2c::SafeInt<std::uint64_t>>);
    STATIC_REQUIRE(std::is_same_v<bt2c::SafeS8, bt2c::SafeInt<std::int8_t>>);
    STATIC_REQUIRE(std::is_same_v<bt2c::SafeS16, bt2c::SafeInt<std::int16_t>>);
    STATIC_REQUIRE(std::is_same_v<bt2c::SafeS32, bt2c::SafeInt<std::int32_t>>);
    STATIC_REQUIRE(std::is_same_v<bt2c::SafeS64, bt2c::SafeInt<std::int64_t>>);
    STATIC_REQUIRE(std::is_same_v<bt2c::SafeU8Dbg, bt2c::SafeIntDbg<std::uint8_t>>);
    STATIC_REQUIRE(std::is_same_v<bt2c::SafeU16Dbg, bt2c::SafeIntDbg<std::uint16_t>>);
    STATIC_REQUIRE(std::is_same_v<bt2c::SafeU32Dbg, bt2c::SafeIntDbg<std::uint32_t>>);
    STATIC_REQUIRE(std::is_same_v<bt2c::SafeU64Dbg, bt2c::SafeIntDbg<std::uint64_t>>);
    STATIC_REQUIRE(std::is_same_v<bt2c::SafeS8Dbg, bt2c::SafeIntDbg<std::int8_t>>);
    STATIC_REQUIRE(std::is_same_v<bt2c::SafeS16Dbg, bt2c::SafeIntDbg<std::int16_t>>);
    STATIC_REQUIRE(std::is_same_v<bt2c::SafeS32Dbg, bt2c::SafeIntDbg<std::int32_t>>);
    STATIC_REQUIRE(std::is_same_v<bt2c::SafeS64Dbg, bt2c::SafeIntDbg<std::int64_t>>);
}

TEST_CASE("bt2c::SafeInt::asU8() and the rest return a `bt2c::SafeInt` having the same policy")
{
    constexpr bt2c::SafeS32 val {};
    constexpr bt2c::SafeS32Dbg valDbg {};

    STATIC_REQUIRE(std::is_same_v<decltype(val.asU8()), bt2c::SafeU8>);
    STATIC_REQUIRE(std::is_same_v<decltype(val.asU16()), bt2c::SafeU16>);
    STATIC_REQUIRE(std::is_same_v<decltype(val.asU32()), bt2c::SafeU32>);
    STATIC_REQUIRE(std::is_same_v<decltype(val.asU64()), bt2c::SafeU64>);
    STATIC_REQUIRE(std::is_same_v<decltype(val.asS8()), bt2c::SafeS8>);
    STATIC_REQUIRE(std::is_same_v<decltype(val.asS16()), bt2c::SafeS16>);
    STATIC_REQUIRE(std::is_same_v<decltype(val.asS32()), bt2c::SafeS32>);
    STATIC_REQUIRE(std::is_same_v<decltype(val.asS64()), bt2c::SafeS64>);
    STATIC_REQUIRE(std::is_same_v<decltype(valDbg.asU8()), bt2c::SafeU8Dbg>);
    STATIC_REQUIRE(std::is_same_v<decltype(valDbg.asS64()), bt2c::SafeS64Dbg>);
    STATIC_REQUIRE_FALSE(noexcept(val.asU8()));
    STATIC_REQUIRE(noexcept(valDbg.asU8()));
}

TEST_CASE("bt2c::SafeInt::asU*()/bt2c::SafeInt::asS*() returns a value which fits")
{
    STATIC_REQUIRE(bt2c::SafeS64 {200}.asU8() == 200);
    STATIC_REQUIRE(bt2c::SafeS64 {-300}.asS16() == -300);
    CHECK(bt2c::SafeU64 {42U}.asS8() == 42);
    CHECK(bt2c::SafeS8 {-42}.asS64() == -42);
    CHECK(bt2c::SafeU8 {255U}.asU16().asU32().asU64() == 255);
    CHECK(bt2c::SafeS32 {-1}.asS16().asS8() == -1);
}

TEST_CASE("bt2c::SafeInt::asU*()/bt2c::SafeInt::asS*() throws when the value doesn't fit")
{
    CHECK_THROWS_WITH(bt2c::SafeS32 {256}.asU8(), "`256` doesn't fit an 8-bit unsigned integer");
    CHECK_THROWS_WITH(bt2c::SafeS32 {-1}.asU16(), "`-1` doesn't fit a 16-bit unsigned integer");
    CHECK_THROWS_WITH(bt2c::SafeS64 {-1}.asU32(), "`-1` doesn't fit a 32-bit unsigned integer");
    CHECK_THROWS_WITH(bt2c::SafeS8 {-1}.asU64(), "`-1` doesn't fit a 64-bit unsigned integer");
    CHECK_THROWS_WITH(bt2c::SafeU16 {128U}.asS8(), "`128` doesn't fit an 8-bit signed integer");
    CHECK_THROWS_WITH(bt2c::SafeU32 {32768U}.asS16(),
                      "`32768` doesn't fit a 16-bit signed integer");
    CHECK_THROWS_WITH(bt2c::SafeS64 {i64Min}.asS32(),
                      "`-9223372036854775808` doesn't fit a 32-bit signed integer");
    CHECK_THROWS_WITH(bt2c::SafeU64 {u64Max}.asS64(),
                      "`18446744073709551615` doesn't fit a 64-bit signed integer");
}

TEST_CASE("`bt2c::SafeInt`: result type is the type of the `bt2c::SafeInt` operand")
{
    STATIC_REQUIRE(std::is_same_v<decltype(bt2c::SafeInt<std::uint8_t> {} + std::uint8_t {}),
                                  bt2c::SafeInt<std::uint8_t>>);
    STATIC_REQUIRE(std::is_same_v<decltype(std::int32_t {} / bt2c::SafeInt<std::int32_t> {1}),
                                  bt2c::SafeInt<std::int32_t>>);
    STATIC_REQUIRE(
        std::is_same_v<decltype(bt2c::SafeInt<std::int16_t> {} * bt2c::SafeInt<std::int16_t> {}),
                       bt2c::SafeInt<std::int16_t>>);
}

TEST_CASE("`bt2c::SafeInt`: result type is the type of the left operand with two `bt2c::SafeInt` "
          "operands of distinct types having the same signedness and width")
{
    STATIC_REQUIRE(
        std::is_same_v<decltype(bt2c::SafeInt<SameWidthA> {} + bt2c::SafeInt<SameWidthB> {}),
                       bt2c::SafeInt<SameWidthA>>);
    STATIC_REQUIRE(
        std::is_same_v<decltype(bt2c::SafeInt<SameWidthB> {} + bt2c::SafeInt<SameWidthA> {}),
                       bt2c::SafeInt<SameWidthB>>);
}

TEST_CASE("`bt2c::SafeInt`: result has the policy of the operands")
{
    STATIC_REQUIRE(std::is_same_v<decltype(bt2c::SafeIntDbg<std::uint64_t> {} + std::uint64_t {}),
                                  bt2c::SafeIntDbg<std::uint64_t>>);
}

/*
 * Whether or not `std::declval<LT>() + std::declval<RT>()` is a
 * valid expression.
 */
template <typename LT, typename RT, typename = void>
constexpr bool canAdd = false;

template <typename LT, typename RT>
constexpr bool canAdd<LT, RT, std::void_t<decltype(std::declval<LT>() + std::declval<RT>())>> =
    true;

TEST_CASE("`bt2c::SafeInt`: accepts an integer or a `bt2c::SafeInt` having the same policy as "
          "the other operand")
{
    STATIC_REQUIRE(canAdd<bt2c::SafeInt<int>, int>);
    STATIC_REQUIRE(canAdd<int, bt2c::SafeInt<int>>);
    STATIC_REQUIRE(canAdd<bt2c::SafeInt<int>, bt2c::SafeInt<int>>);
}

TEST_CASE("`bt2c::SafeInt`: rejects a `bool`, a character, a floating point number, or a "
          "`bt2c::SafeInt` having another policy as the other operand")
{
    STATIC_REQUIRE_FALSE(canAdd<bt2c::SafeInt<int>, bool>);
    STATIC_REQUIRE_FALSE(canAdd<bt2c::SafeInt<int>, char>);
    STATIC_REQUIRE_FALSE(canAdd<bt2c::SafeInt<int>, wchar_t>);
    STATIC_REQUIRE_FALSE(canAdd<bt2c::SafeInt<int>, char16_t>);
    STATIC_REQUIRE_FALSE(canAdd<bt2c::SafeInt<int>, char32_t>);
    STATIC_REQUIRE_FALSE(canAdd<bt2c::SafeInt<int>, double>);
    STATIC_REQUIRE_FALSE(canAdd<bt2c::SafeInt<int>, bt2c::SafeIntDbg<int>>);
}

TEST_CASE("`bt2c::SafeInt`: operator+() returns the exact sum")
{
    CHECK((bt2c::SafeInt<std::uint64_t> {u64Max - 1} + std::uint64_t {1}).val() == u64Max);
    CHECK(+(bt2c::SafeInt<std::int8_t> {-100} + std::int8_t {-28}).val() == -128);
}

TEST_CASE("`bt2c::SafeInt`: operator+() with an operand of another type converted with as()")
{
    const bt2c::SafeInt<std::uint32_t> other {5U};

    CHECK((bt2c::SafeInt<std::int64_t> {-3} + other.as<std::int64_t>()).val() == 2);
}

TEST_CASE("`bt2c::SafeInt`: operator+() throws when the sum doesn't fit")
{
    CHECK_THROWS_WITH(bt2c::SafeInt<std::uint64_t> {u64Max} + std::uint64_t {1},
                      "`18446744073709551615 + 1` doesn't fit a 64-bit unsigned integer");
    CHECK_THROWS_WITH(bt2c::SafeInt<std::int64_t> {i64Max} + std::int64_t {1},
                      "`9223372036854775807 + 1` doesn't fit a 64-bit signed integer");
    CHECK_THROWS_WITH(bt2c::SafeInt<std::uint8_t> {200U} + std::uint8_t {100},
                      "`200 + 100` doesn't fit an 8-bit unsigned integer");
}

TEST_CASE("`bt2c::SafeInt`: operator-() returns the exact difference")
{
    CHECK((bt2c::SafeInt<std::uint32_t> {5U} - 3U).val() == 2);
    CHECK((bt2c::SafeInt<std::int32_t> {3} - std::int32_t {5}).val() == -2);
}

TEST_CASE("`bt2c::SafeInt`: operator-() with a plain operand of another type converted with the "
          "explicit constructor")
{
    const std::uint64_t other = 3;

    CHECK((bt2c::SafeInt<std::int64_t> {-2} - bt2c::SafeInt<std::int64_t> {other}).val() == -5);
}

TEST_CASE("`bt2c::SafeInt`: operator-() throws when the difference doesn't fit")
{
    CHECK_THROWS_WITH(bt2c::SafeInt<std::uint32_t> {3U} - 5U,
                      "`3 - 5` doesn't fit a 32-bit unsigned integer");
    CHECK_THROWS_WITH(bt2c::SafeInt<std::int64_t> {i64Min} - std::int64_t {1},
                      "`-9223372036854775808 - 1` doesn't fit a 64-bit signed integer");
}

TEST_CASE("`bt2c::SafeInt`: operator*() returns the exact product")
{
    CHECK((bt2c::SafeInt<std::uint64_t> {1'000'000'000U} * std::uint64_t {18}).val() ==
          18'000'000'000ULL);
    CHECK((bt2c::SafeInt<std::int64_t> {-4} * std::int64_t {3}).val() == -12);
}

TEST_CASE("`bt2c::SafeInt`: operator*() with a plain operand of another type converted with "
          "brace initialization")
{
    const std::uint32_t other = 3;

    CHECK((std::int64_t {other} * bt2c::SafeInt<std::int64_t> {-4}).val() == -12);
}

TEST_CASE("`bt2c::SafeInt`: operator*() throws when the product doesn't fit")
{
    CHECK_THROWS_WITH(bt2c::SafeInt<std::uint64_t> {u64Max / 2 + 1} * std::uint64_t {2},
                      "`9223372036854775808 * 2` doesn't fit a 64-bit unsigned integer");
    CHECK_THROWS_WITH(bt2c::SafeInt<std::int64_t> {i64Min} * std::int64_t {-1},
                      "`-9223372036854775808 * -1` doesn't fit a 64-bit signed integer");
}

TEST_CASE("`bt2c::SafeInt`: operator/() returns the quotient truncated towards zero")
{
    CHECK((bt2c::SafeInt<std::uint64_t> {17U} / std::uint64_t {5}).val() == 3);
    CHECK((bt2c::SafeInt<std::int64_t> {-17} / std::int64_t {5}).val() == -3);
    CHECK(+(bt2c::SafeInt<std::uint8_t> {200U} / std::uint8_t {3}).val() == 66);
}

TEST_CASE("`bt2c::SafeInt`: operator/() with an operand of another type converted with as()")
{
    const bt2c::SafeInt<std::uint32_t> other {5U};

    CHECK((bt2c::SafeInt<std::int64_t> {-17} / other.as<std::int64_t>()).val() == -3);
}

TEST_CASE("`bt2c::SafeInt`: operator/() throws on division by zero")
{
    CHECK_THROWS_WITH(bt2c::SafeInt<std::uint64_t> {17U} / std::uint64_t {0},
                      "Division by zero: `17 / 0`");
}

TEST_CASE("`bt2c::SafeInt`: operator/() throws when the quotient doesn't fit")
{
    CHECK_THROWS_WITH(bt2c::SafeInt<std::int64_t> {i64Min} / std::int64_t {-1},
                      "`-9223372036854775808 / -1` doesn't fit a 64-bit signed integer");
}

TEST_CASE("`bt2c::SafeInt`: operator<<() returns the shifted value")
{
    CHECK((bt2c::SafeInt<std::uint64_t> {1U} << 63).val() == 1ULL << 63);
    CHECK(+(bt2c::SafeInt<std::uint8_t> {0x0fU} << 4U).val() == 0xf0);
    CHECK((bt2c::SafeInt<std::int32_t> {-1} << 31).val() ==
          std::numeric_limits<std::int32_t>::min());
}

TEST_CASE("`bt2c::SafeInt`: operator<<() accepts a `bt2c::SafeInt` shift count")
{
    CHECK((bt2c::SafeInt<std::int32_t> {3} << bt2c::SafeInt<std::uint8_t> {4U}).val() == 48);
}

TEST_CASE("`bt2c::SafeInt`: operator<<() throws when the result doesn't fit")
{
    CHECK_THROWS_WITH(bt2c::SafeInt<std::uint8_t> {0x10U} << 4,
                      "`16 << 4` doesn't fit an 8-bit unsigned integer");
    CHECK_THROWS_WITH(bt2c::SafeInt<std::int32_t> {1} << 31,
                      "`1 << 31` doesn't fit a 32-bit signed integer");
    CHECK_THROWS_WITH(bt2c::SafeInt<std::int32_t> {-2} << 31,
                      "`-2 << 31` doesn't fit a 32-bit signed integer");
}

TEST_CASE("`bt2c::SafeInt`: operator<<() throws on an invalid shift count")
{
    CHECK_THROWS_WITH(bt2c::SafeInt<std::uint64_t> {0U} << 64,
                      "Invalid shift count for a 64-bit unsigned integer: `0 << 64`");
    CHECK_THROWS_WITH(bt2c::SafeInt<std::uint64_t> {8U} << -1,
                      "Invalid shift count for a 64-bit unsigned integer: `8 << -1`");
}

TEST_CASE("`bt2c::SafeInt::operator+=()` adds to the value")
{
    bt2c::SafeInt<std::int64_t> val {-5};

    val += std::int64_t {3};
    CHECK(val.val() == -2);
}

TEST_CASE("`bt2c::SafeInt::operator-=()` subtracts from the value")
{
    bt2c::SafeInt<std::int64_t> val {-4};

    val -= std::int64_t {1};
    CHECK(val.val() == -5);
}

TEST_CASE("`bt2c::SafeInt::operator*=()` multiplies the value")
{
    bt2c::SafeInt<std::int64_t> val {-2};

    val *= bt2c::SafeInt<std::int64_t> {4};
    CHECK(val.val() == -8);
}

TEST_CASE("`bt2c::SafeInt::operator/=()` divides the value")
{
    bt2c::SafeInt<std::int64_t> val {-8};

    val /= std::int64_t {2};
    CHECK(val.val() == -4);
}

TEST_CASE("`bt2c::SafeInt`: compound assignment throws when the result doesn't fit")
{
    bt2c::SafeInt<std::uint32_t> uVal {5U};

    CHECK_THROWS_WITH(uVal += std::numeric_limits<std::uint32_t>::max(),
                      "`5 + 4294967295` doesn't fit a 32-bit unsigned integer");

    bt2c::SafeInt<std::int32_t> iVal {std::numeric_limits<std::int32_t>::min()};

    CHECK_THROWS_WITH(iVal /= std::int32_t {-1},
                      "`-2147483648 / -1` doesn't fit a 32-bit signed integer");
}

TEST_CASE("`bt2c::SafeInt`: failed compound assignment keeps the value")
{
    bt2c::SafeInt<std::int64_t> val {-5};

    CHECK_THROWS_WITH(val *= i64Max,
                      "`-5 * 9223372036854775807` doesn't fit a 64-bit signed integer");
    CHECK(val.val() == -5);
    CHECK_THROWS_WITH(val /= std::int64_t {0}, "Division by zero: `-5 / 0`");
    CHECK(val.val() == -5);
}

TEST_CASE("`bt2c::SafeInt`: operator==() is mathematically correct")
{
    CHECK(bt2c::SafeInt<std::uint8_t> {3U} == bt2c::SafeInt<std::int64_t> {3});
    CHECK_FALSE(bt2c::SafeInt<int> {-1} == u64Max);
}

TEST_CASE("`bt2c::SafeInt`: operator!=() is mathematically correct")
{
    CHECK(bt2c::SafeInt<int> {-1} != u64Max);
}

TEST_CASE("`bt2c::SafeInt`: operator<() is mathematically correct")
{
    CHECK(bt2c::SafeInt<int> {-1} < 1U);
    CHECK(-1 < bt2c::SafeInt<unsigned int> {0U});
}

TEST_CASE("`bt2c::SafeInt`: operator>() is mathematically correct")
{
    CHECK(bt2c::SafeInt<std::uint64_t> {u64Max} > std::int64_t {-1});
}

TEST_CASE("`bt2c::SafeInt`: operator<=() is mathematically correct")
{
    CHECK(bt2c::SafeInt<std::int8_t> {-3} <= -3);
}

TEST_CASE("`bt2c::SafeInt`: operator>=() is mathematically correct")
{
    CHECK(bt2c::SafeInt<std::int8_t> {-3} >= -3);
}

TEST_CASE("`bt2c::SafeInt`: operations are usable in constant expressions")
{
    STATIC_REQUIRE(
        (bt2c::SafeInt<std::uint32_t> {3U} * std::uint32_t {4} + std::uint32_t {1}).val() == 13);
    STATIC_REQUIRE((bt2c::SafeInt<std::int32_t> {-17} / std::int32_t {5}).val() == -3);
    STATIC_REQUIRE(bt2c::SafeInt<std::uint8_t> {200U}.as<std::int64_t>() == 200);
    STATIC_REQUIRE((bt2c::SafeInt<std::uint32_t> {1U} << 31).val() == 0x80000000);
    STATIC_REQUIRE(bt2c::SafeInt<int> {-1} < 1U);
}

TEST_CASE("`bt2c::SafeInt`: {fmt} formats a `bt2c::SafeInt` like its raw value")
{
    CHECK(fmt::format("{}", bt2c::SafeInt<std::int64_t> {-42}) == "-42");
    CHECK(fmt::format("{:#x}", bt2c::SafeInt<std::uint32_t> {255U}) == "0xff");
    CHECK(fmt::format("{}", bt2c::SafeInt<std::uint8_t> {200U}) == "200");
    CHECK(fmt::format("{:>4}", bt2c::SafeIntDbg<std::int8_t> {-5}) == "  -5");
}

TEST_CASE("`bt2c::SafeIntDbg`: operations are `noexcept`")
{
    STATIC_REQUIRE(noexcept(bt2c::SafeIntDbg<std::uint64_t> {} + std::uint64_t {1}));
    STATIC_REQUIRE_FALSE(noexcept(bt2c::SafeInt<std::uint64_t> {} + std::uint64_t {1}));
}

TEST_CASE("`bt2c::SafeIntDbg`: operations without failure")
{
    bt2c::SafeIntDbg<std::uint64_t> val {10U};

    val *= std::uint64_t {8};
    val += std::uint64_t {2};
    val -= std::uint64_t {1};
    val /= std::uint64_t {3};
    CHECK(val.val() == 27);
    CHECK((val << 2).val() == 108);
    CHECK((bt2c::SafeIntDbg<std::int64_t> {-6} / std::int64_t {4}).val() == -1);
    CHECK(bt2c::SafeIntDbg<std::int64_t> {-6}.as<std::int8_t>() == -6);
}

TEST_CASE("`bt2c::SafeInt`: converts seconds to signed nanoseconds")
{
    const std::uint64_t sec = 1'700'000'000;
    const std::uint32_t fracNs = 123;

    CHECK((bt2c::SafeInt {sec} * std::uint64_t {1'000'000'000} + std::uint64_t {fracNs}).asS64() ==
          1'700'000'000'000'000'123LL);
}

TEST_CASE("`bt2c::SafeInt`: conversion of seconds to signed nanoseconds throws on overflow")
{
    CHECK_THROWS_AS(
        (bt2c::SafeInt {u64Max / 10} * std::uint64_t {1'000'000'000} + std::uint64_t {123}).asS64(),
        bt2c::OverflowError);
}
