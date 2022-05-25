/*
 * Copyright (c) 2022 Philippe Proulx <pproulx@efficios.com>
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef BABELTRACE_CPP_COMMON_READ_FIXED_LEN_INT_HPP
#define BABELTRACE_CPP_COMMON_READ_FIXED_LEN_INT_HPP

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <type_traits>

#include "cpp-common/endian.hpp"

namespace bt2_common {

/*
 * Reads a fixed-length integer of unknown byte order into a value of integral
 * type `IntT` from the buffer `buf` and returns it.
 */
template <typename IntT>
IntT readFixedLenInt(const std::uint8_t * const buf)
{
    static_assert(std::is_integral<IntT>::value, "`IntT` is an integral type.");

    IntT val;

    std::memcpy(&val, buf, sizeof(val));
    return val;
}

/*
 * Reads a fixed-length little-endian integer into a value of integral
 * type `IntT` from the buffer `buf` and returns it.
 */
template <typename IntT>
IntT readFixedLenIntLe(const std::uint8_t * const buf)
{
    return bt2_common::littleEndianToNative(readFixedLenInt<IntT>(buf));
}

/*
 * Reads a fixed-length big-endian integer into a value of integral
 * type `IntT` from the buffer `buf` and returns it.
 */
template <typename IntT>
IntT readFixedLenIntBe(const std::uint8_t * const buf)
{
    return bt2_common::bigEndianToNative(readFixedLenInt<IntT>(buf));
}

} /* namespace bt2_common */

#endif /* BABELTRACE_CPP_COMMON_READ_FIXED_LEN_INT_HPP */
