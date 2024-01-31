/*
 * Copyright (c) 2023 Philippe Proulx <pproulx@efficios.com>
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef BABELTRACE_CPP_COMMON_BT2S_STRING_VIEW_HPP
#define BABELTRACE_CPP_COMMON_BT2S_STRING_VIEW_HPP

#include "cpp-common/vendor/string_view-standalone/string_view.hpp"

namespace bt2s {

using bpstd::basic_string_view;
using bpstd::string_view;
using bpstd::wstring_view;
using bpstd::u16string_view;
using bpstd::u32string_view;

} /* namespace bt2s */

#endif /* BABELTRACE_CPP_COMMON_BT2S_STRING_VIEW_HPP */
