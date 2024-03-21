/*
 * Copyright (c) 2024 EfficiOS Inc.
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef BABELTRACE_CPP_COMMON_BT2C_SPAN_HPP
#define BABELTRACE_CPP_COMMON_BT2C_SPAN_HPP

#include "cpp-common/bt2s/span.hpp"

namespace bt2c {

template <class T>
inline constexpr bt2s::span<T> makeSpan(T * const ptr, const size_t count) noexcept
{
    return nonstd::make_span(ptr, count);
}

} /* namespace bt2c */

#endif /* BABELTRACE_CPP_COMMON_BT2C_SPAN_HPP */
