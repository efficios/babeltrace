/*
 * Copyright (c) 2023 Philippe Proulx <pproulx@efficios.com>
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef BABELTRACE_CPP_COMMON_BT2S_OPTIONAL_HPP
#define BABELTRACE_CPP_COMMON_BT2S_OPTIONAL_HPP

#include "cpp-common/vendor/optional-lite/optional.hpp"

namespace bt2s {

using nonstd::optional;
using nonstd::nullopt_t;
using nonstd::bad_optional_access;
using nonstd::nullopt;
using nonstd::make_optional;
using nonstd::in_place_t;
using nonstd::in_place;
using nonstd::in_place_type;
using nonstd::in_place_index;

} /* namespace bt2s */

#endif /* BABELTRACE_CPP_COMMON_BT2S_OPTIONAL_HPP */
