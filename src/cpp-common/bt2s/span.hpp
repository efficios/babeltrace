/*
 * Copyright (c) 2024 Philippe Proulx <pproulx@efficios.com>
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef BABELTRACE_CPP_COMMON_BT2S_SPAN_HPP
#define BABELTRACE_CPP_COMMON_BT2S_SPAN_HPP

#define span_FEATURE_MAKE_SPAN 1

#ifdef BT_DEBUG_MODE
#    define span_CONFIG_CONTRACT_LEVEL_ON 1
#else
#    define span_CONFIG_CONTRACT_LEVEL_OFF 1
#endif

#include "cpp-common/vendor/span-lite/span.hpp" /* IWYU pragma: export */

namespace bt2s {

using nonstd::dynamic_extent;
using nonstd::span;

} /* namespace bt2s */

#endif /* BABELTRACE_CPP_COMMON_BT2S_SPAN_HPP */
