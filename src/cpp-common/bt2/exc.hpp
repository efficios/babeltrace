/*
 * Copyright (c) 2020-2022 Philippe Proulx <pproulx@efficios.com>
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef BABELTRACE_CPP_COMMON_BT2_EXC_HPP
#define BABELTRACE_CPP_COMMON_BT2_EXC_HPP

#include "cpp-common/exc.hpp"

namespace bt2 {

using Error = bt2_common::Error;
using OverflowError = bt2_common::OverflowError;
using MemoryError = bt2_common::MemoryError;
using TryAgain = bt2_common::TryAgain;

} /* namespace bt2 */

#endif /* BABELTRACE_CPP_COMMON_BT2_EXC_HPP */
