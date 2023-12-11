/*
 * Copyright (c) 2022 Philippe Proulx <pproulx@efficios.com>
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef BABELTRACE_CPP_COMMON_BT2C_ALIGN_HPP
#define BABELTRACE_CPP_COMMON_BT2C_ALIGN_HPP

#include <type_traits>

#include "common/align.h"

namespace bt2c {

template <typename ValT, typename AlignT>
ValT align(const ValT val, const AlignT align) noexcept
{
    static_assert(std::is_unsigned<ValT>::value, "`ValT` is unsigned.");
    return BT_ALIGN(val, static_cast<ValT>(align));
}

} /* namespace bt2c */

#endif /* BABELTRACE_CPP_COMMON_BT2C_ALIGN_HPP */
