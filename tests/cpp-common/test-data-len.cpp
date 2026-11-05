/*
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Copyright (C) 2026 EfficiOS, Inc.
 */

#include <type_traits>

#include "cpp-common/bt2c/data-len.hpp"
#include "cpp-common/bt2c/exc.hpp"

#include "catch2/catch_test_macros.hpp"
#include "catch2/matchers/catch_matchers.hpp"

using namespace bt2c::literals::datalen;

TEST_CASE("bt2c::DataLen::fromBits() sets the expected number of bits")
{
    CHECK(bt2c::DataLen::fromBits(10).bits() == 10);
}

TEST_CASE("bt2c::DataLen::fromBytes() sets the expected number of bits")
{
    CHECK(bt2c::DataLen::fromBytes(4).bits() == 32);
}

TEST_CASE("bt2c::DataLen::fromBytes() throws when the number of bits doesn't fit")
{
    CHECK_THROWS_WITH(bt2c::DataLen::fromBytes(1ULL << 61),
                      "`2305843009213693952 * 8` doesn't fit a 64-bit unsigned integer");
}

TEST_CASE("bt2c::DataLen::fromBytesDbg() sets the expected number of bits")
{
    CHECK(bt2c::DataLen::fromBytesDbg(4).bits() == 32);
}

TEST_CASE("bt2c::DataLen operations which may overflow are only `noexcept` with the `Dbg` suffix")
{
    constexpr auto len = bt2c::DataLen::fromBits(8);

    STATIC_REQUIRE_FALSE(noexcept(bt2c::DataLen::fromBytes(1)));
    STATIC_REQUIRE_FALSE(noexcept(len + len));
    STATIC_REQUIRE_FALSE(noexcept(len - len));
    STATIC_REQUIRE(noexcept(bt2c::DataLen::fromBytesDbg(1)));
    STATIC_REQUIRE(noexcept(len.addDbg(len)));
    STATIC_REQUIRE(noexcept(len.subDbg(len)));
    STATIC_REQUIRE(noexcept(bt2c::DataLen::fromBits(1)));
    STATIC_REQUIRE(noexcept(len < len));
}

TEST_CASE("bt2c::DataLen::operator+=() throws when the sum doesn't fit")
{
    auto len = bt2c::DataLen::fromBits(~0ULL);

    CHECK_THROWS_WITH(len += 1_bits,
                      "`18446744073709551615 + 1` doesn't fit a 64-bit unsigned integer");
    CHECK(len.bits() == ~0ULL);
}

TEST_CASE("bt2c::DataLen::operator-=() throws when the difference is negative")
{
    auto len = bt2c::DataLen::fromBits(8);

    CHECK_THROWS_WITH(len -= 9_bits, "`8 - 9` doesn't fit a 64-bit unsigned integer");
    CHECK(len.bits() == 8);
}

TEST_CASE("operator+() and operator-() with `bt2c::DataLen` throw on overflow")
{
    CHECK_THROWS_AS(bt2c::DataLen::fromBits(1ULL << 63) + bt2c::DataLen::fromBits(1ULL << 63),
                    bt2c::OverflowError);
    CHECK_THROWS_AS(1_bits - 2_bits, bt2c::OverflowError);
}

TEST_CASE("bt2c::DataLen::addDbg() returns the sum")
{
    const auto len = 8_bits;

    CHECK(len.addDbg(4_bits) == 12_bits);
    CHECK(len == 8_bits);
}

TEST_CASE("bt2c::DataLen::subDbg() returns the difference")
{
    const auto len = 8_bits;

    CHECK(len.subDbg(3_bits) == 5_bits);
    CHECK(len == 8_bits);
}

TEST_CASE("bt2c::DataLen is usable in constant expressions")
{
    STATIC_REQUIRE((2_KiBytes + 8_bits - 2_bits).bits() == 2 * 1024 * 8 + 6);
    STATIC_REQUIRE((bt2c::DataLen::fromBytes(3) + bt2c::DataLen::fromBytes(3)).bits() == 48);
    STATIC_REQUIRE(bt2c::DataLen::fromBytesDbg(3).addDbg(5_bits).subDbg(1_bits).bits() == 28);
}

TEST_CASE("bt2c::DataLen::bytes() floors the number of bytes")
{
    CHECK(bt2c::DataLen::fromBits(10).bytes() == 1);
    CHECK(bt2c::DataLen::fromBits(16).bytes() == 2);
    CHECK(bt2c::DataLen::fromBits(23).bytes() == 2);
}

TEST_CASE("bt2c::DataLen::hasExtraBits() with extra bits")
{
    CHECK(bt2c::DataLen::fromBits(10).hasExtraBits());
}

TEST_CASE("bt2c::DataLen::hasExtraBits() without extra bits")
{
    CHECK_FALSE(bt2c::DataLen::fromBits(16).hasExtraBits());
    CHECK_FALSE(bt2c::DataLen::fromBits(0).hasExtraBits());
}

TEST_CASE("bt2c::DataLen::extraBitCount() returns the expected value")
{
    CHECK(bt2c::DataLen::fromBits(10).extraBitCount() == 2);
    CHECK(bt2c::DataLen::fromBits(16).extraBitCount() == 0);
    CHECK(bt2c::DataLen::fromBits(23).extraBitCount() == 7);
}

TEST_CASE("bt2c::DataLen::isPowOfTwo() with a power of two")
{
    CHECK(bt2c::DataLen::fromBits(1).isPowOfTwo());
    CHECK(bt2c::DataLen::fromBits(8).isPowOfTwo());
    CHECK(bt2c::DataLen::fromBits(1024).isPowOfTwo());
}

TEST_CASE("bt2c::DataLen::isPowOfTwo() with a value which isn't a power of two")
{
    CHECK_FALSE(bt2c::DataLen::fromBits(0).isPowOfTwo());
    CHECK_FALSE(bt2c::DataLen::fromBits(9).isPowOfTwo());
    CHECK_FALSE(bt2c::DataLen::fromBits(1023).isPowOfTwo());
}

TEST_CASE("Dereference operator of `bt2c::DataLen` is an alias of bits()")
{
    const auto len = bt2c::DataLen::fromBits(42);

    CHECK(*len == len.bits());
}

TEST_CASE("bt2c::DataLen::operator==() works as expected")
{
    CHECK(bt2c::DataLen::fromBits(8) == bt2c::DataLen::fromBits(8));
    CHECK_FALSE(bt2c::DataLen::fromBits(8) == bt2c::DataLen::fromBits(9));
}

TEST_CASE("bt2c::DataLen::operator!=() works as expected")
{
    CHECK(bt2c::DataLen::fromBits(8) != bt2c::DataLen::fromBits(9));
    CHECK_FALSE(bt2c::DataLen::fromBits(8) != bt2c::DataLen::fromBits(8));
}

TEST_CASE("bt2c::DataLen::operator<() works as expected")
{
    CHECK(bt2c::DataLen::fromBits(8) < bt2c::DataLen::fromBits(9));
    CHECK_FALSE(bt2c::DataLen::fromBits(9) < bt2c::DataLen::fromBits(8));
    CHECK_FALSE(bt2c::DataLen::fromBits(8) < bt2c::DataLen::fromBits(8));
}

TEST_CASE("bt2c::DataLen::operator<=() works as expected")
{
    CHECK(bt2c::DataLen::fromBits(8) <= bt2c::DataLen::fromBits(9));
    CHECK(bt2c::DataLen::fromBits(8) <= bt2c::DataLen::fromBits(8));
    CHECK_FALSE(bt2c::DataLen::fromBits(9) <= bt2c::DataLen::fromBits(8));
}

TEST_CASE("bt2c::DataLen::operator>() works as expected")
{
    CHECK(bt2c::DataLen::fromBits(9) > bt2c::DataLen::fromBits(8));
    CHECK_FALSE(bt2c::DataLen::fromBits(8) > bt2c::DataLen::fromBits(9));
    CHECK_FALSE(bt2c::DataLen::fromBits(8) > bt2c::DataLen::fromBits(8));
}

TEST_CASE("bt2c::DataLen::operator>=() works as expected")
{
    CHECK(bt2c::DataLen::fromBits(9) >= bt2c::DataLen::fromBits(8));
    CHECK(bt2c::DataLen::fromBits(8) >= bt2c::DataLen::fromBits(8));
    CHECK_FALSE(bt2c::DataLen::fromBits(8) >= bt2c::DataLen::fromBits(9));
}

TEST_CASE("bt2c::DataLen::operator+=() adds bits")
{
    auto len = bt2c::DataLen::fromBits(8);

    len += bt2c::DataLen::fromBits(4);
    CHECK(len == bt2c::DataLen::fromBits(12));
}

TEST_CASE("bt2c::DataLen::operator-=() subtracts bits")
{
    auto len = bt2c::DataLen::fromBits(8);

    len -= bt2c::DataLen::fromBits(4);
    CHECK(len == bt2c::DataLen::fromBits(4));
}

TEST_CASE("operator+() returns the sum of two `bt2c::DataLen` instances")
{
    CHECK(bt2c::DataLen::fromBits(8) + bt2c::DataLen::fromBits(4) == bt2c::DataLen::fromBits(12));
}

TEST_CASE("operator-() returns the difference of two `bt2c::DataLen` instances")
{
    CHECK(bt2c::DataLen::fromBits(8) - bt2c::DataLen::fromBits(4) == bt2c::DataLen::fromBits(4));
}

TEST_CASE("`_bits` user-defined literal creates the expected `bt2c::DataLen`")
{
    CHECK(10_bits == bt2c::DataLen::fromBits(10));
}

TEST_CASE("`_KiBits` user-defined literal creates the expected `bt2c::DataLen`")
{
    CHECK(2_KiBits == bt2c::DataLen::fromBits(2 * 1024ULL));
}

TEST_CASE("`_MiBits` user-defined literal creates the expected `bt2c::DataLen`")
{
    CHECK(2_MiBits == bt2c::DataLen::fromBits(2 * 1024ULL * 1024));
}

TEST_CASE("`_GiBits` user-defined literal creates the expected `bt2c::DataLen`")
{
    CHECK(2_GiBits == bt2c::DataLen::fromBits(2 * 1024ULL * 1024 * 1024));
}

TEST_CASE("`_bytes` user-defined literal creates the expected `bt2c::DataLen`")
{
    CHECK(10_bytes == bt2c::DataLen::fromBytes(10));
}

TEST_CASE("`_KiBytes` user-defined literal creates the expected `bt2c::DataLen`")
{
    CHECK(2_KiBytes == bt2c::DataLen::fromBytes(2 * 1024ULL));
}

TEST_CASE("`_MiBytes` user-defined literal creates the expected `bt2c::DataLen`")
{
    CHECK(2_MiBytes == bt2c::DataLen::fromBytes(2 * 1024ULL * 1024));
}

TEST_CASE("`_GiBytes` user-defined literal creates the expected `bt2c::DataLen`")
{
    CHECK(2_GiBytes == bt2c::DataLen::fromBytes(2 * 1024ULL * 1024 * 1024));
}
