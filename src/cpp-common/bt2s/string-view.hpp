/*
 * Copyright (c) 2023 Philippe Proulx <pproulx@efficios.com>
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef BABELTRACE_CPP_COMMON_BT2S_STRING_VIEW_HPP
#define BABELTRACE_CPP_COMMON_BT2S_STRING_VIEW_HPP

#include "cpp-common/vendor/string_view-standalone/string_view.hpp"

namespace bt2s {

template <typename CharT, typename TraitsT = std::char_traits<CharT>>
using basic_string_view = bpstd::basic_string_view<CharT, TraitsT>;

using string_view = bpstd::string_view;
using wstring_view = bpstd::wstring_view;
using u16string_view = bpstd::u16string_view;
using u32string_view = bpstd::u32string_view;

} /* namespace bt2s */

#endif /* BABELTRACE_CPP_COMMON_BT2S_STRING_VIEW_HPP */
