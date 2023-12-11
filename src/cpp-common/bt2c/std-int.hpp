/*
 * Copyright (c) 2022 Philippe Proulx <pproulx@efficios.com>
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef BABELTRACE_CPP_COMMON_BT2C_STD_INT_HPP
#define BABELTRACE_CPP_COMMON_BT2C_STD_INT_HPP

#include <cstdint>

namespace bt2c {
namespace internal {

template <std::size_t LenBitsV, bool IsSignedV>
struct StdIntTBase;

template <>
struct StdIntTBase<8, true>
{
    using Type = std::int8_t;
};

template <>
struct StdIntTBase<8, false>
{
    using Type = std::uint8_t;
};

template <>
struct StdIntTBase<16, true>
{
    using Type = std::int16_t;
};

template <>
struct StdIntTBase<16, false>
{
    using Type = std::uint16_t;
};

template <>
struct StdIntTBase<32, true>
{
    using Type = std::int32_t;
};

template <>
struct StdIntTBase<32, false>
{
    using Type = std::uint32_t;
};

template <>
struct StdIntTBase<64, true>
{
    using Type = std::int64_t;
};

template <>
struct StdIntTBase<64, false>
{
    using Type = std::uint64_t;
};

} /* namespace internal */

/*
 * Standard fixed-length integer type `Type` of length `LenBitsV` bits
 * and signedness `IsSignedV`.
 *
 * `LenBitsV` must be one of 8, 16, 32, or 64.
 *
 * For example, `StdIntT<32, true>` is `std::int32_t`.
 */
template <std::size_t LenBitsV, bool IsSignedV>
using StdIntT = typename internal::StdIntTBase<LenBitsV, IsSignedV>::Type;

} /* namespace bt2c */

#endif /* BABELTRACE_CPP_COMMON_BT2C_STD_INT_HPP */
