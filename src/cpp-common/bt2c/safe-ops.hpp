/*
 * Copyright (c) 2022 Philippe Proulx <pproulx@efficios.com>
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef BABELTRACE_CPP_COMMON_BT2C_SAFE_OPS_HPP
#define BABELTRACE_CPP_COMMON_BT2C_SAFE_OPS_HPP

#include <limits>
#include <type_traits>

#include "common/assert.h"

namespace bt2c {

template <typename T>
constexpr bool safeToMul(const T a, const T b)
{
    static_assert(std::is_unsigned<T>::value, "`T` is an unsigned type.");

    return a == 0 || b == 0 || a < std::numeric_limits<T>::max() / b;
}

template <typename T>
T safeMul(const T a, const T b) noexcept
{
    static_assert(std::is_unsigned<T>::value, "`T` is an unsigned type.");

    BT_ASSERT_DBG(safeToMul(a, b));
    return a * b;
}

template <typename T>
constexpr bool safeToAdd(const T a, const T b)
{
    static_assert(std::is_unsigned<T>::value, "`T` is an unsigned type.");

    return a <= std::numeric_limits<T>::max() - b;
}

template <typename T>
T safeAdd(const T a, const T b) noexcept
{
    static_assert(std::is_unsigned<T>::value, "`T` is an unsigned type.");

    BT_ASSERT_DBG(safeToAdd(a, b));
    return a + b;
}

template <typename T>
constexpr bool safeToSub(const T a, const T b)
{
    static_assert(std::is_unsigned<T>::value, "`T` is an unsigned type.");

    return a >= b;
}

template <typename T>
T safeSub(const T a, const T b) noexcept
{
    static_assert(std::is_unsigned<T>::value, "`T` is an unsigned type.");

    BT_ASSERT_DBG(safeToSub(a, b));
    return a - b;
}

} /* namespace bt2c */

#endif /* BABELTRACE_CPP_COMMON_BT2C_SAFE_OPS_HPP */
