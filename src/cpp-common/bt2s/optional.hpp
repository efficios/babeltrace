/*
 * Copyright (c) 2023 Philippe Proulx <pproulx@efficios.com>
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef BABELTRACE_CPP_COMMON_BT2S_OPTIONAL_HPP
#define BABELTRACE_CPP_COMMON_BT2S_OPTIONAL_HPP

#include "cpp-common/vendor/optional-lite/optional.hpp"

namespace bt2s {

template <typename T>
using optional = nonstd::optional<T>;

using nullopt_t = nonstd::nullopt_t;
using bad_optional_access = nonstd::bad_optional_access;
constexpr nullopt_t nullopt {nonstd::nullopt};

template <typename T>
constexpr optional<typename std::decay<T>::type> make_optional(T&& value)
{
    return nonstd::make_optional(std::forward<T>(value));
}

template <typename T, typename... ArgTs>
constexpr optional<T> make_optional(ArgTs&&...args)
{
    return nonstd::make_optional<T>(std::forward<ArgTs>(args)...);
}

using in_place_t = nonstd::in_place_t;

template <typename T>
inline in_place_t in_place()
{
    return nonstd::in_place<T>();
}

template <std::size_t K>
inline in_place_t in_place()
{
    return nonstd::in_place<K>();
}

template <typename T>
inline in_place_t in_place_type()
{
    return nonstd::in_place_type<T>();
}

template <std::size_t K>
inline in_place_t in_place_index()
{
    return nonstd::in_place_index<K>();
}

} /* namespace bt2s */

#endif /* BABELTRACE_CPP_COMMON_BT2S_OPTIONAL_HPP */
