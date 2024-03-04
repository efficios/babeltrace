/*
 * Copyright (c) 2023 Philippe Proulx <pproulx@efficios.com>
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef BABELTRACE_CPP_COMMON_BT2S_STRING_VIEW_HPP
#define BABELTRACE_CPP_COMMON_BT2S_STRING_VIEW_HPP

#include "cpp-common/vendor/string-view-lite/string_view.hpp"

namespace bt2s {

using nonstd::basic_string_view;
using nonstd::string_view;
using nonstd::wstring_view;
using nonstd::u16string_view;
using nonstd::u32string_view;

} /* namespace bt2s */

#endif /* BABELTRACE_CPP_COMMON_BT2S_STRING_VIEW_HPP */
