/*
 * Copyright (c) 2020-2022 Philippe Proulx <pproulx@efficios.com>
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef BABELTRACE_CPP_COMMON_BT2_EXC_HPP
#define BABELTRACE_CPP_COMMON_BT2_EXC_HPP

#include "cpp-common/bt2c/exc.hpp"

namespace bt2 {

using Error = bt2c::Error;
using OverflowError = bt2c::OverflowError;
using MemoryError = bt2c::MemoryError;
using TryAgain = bt2c::TryAgain;

/*
 * Unknown query object.
 */
class UnknownObject : public std::exception
{
public:
    explicit UnknownObject() noexcept : std::exception {}
    {
    }
};

} /* namespace bt2 */

#endif /* BABELTRACE_CPP_COMMON_BT2_EXC_HPP */
