/*
 * Copyright (c) 2023 Philippe Proulx <pproulx@efficios.com>
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef BABELTRACE_CPP_COMMON_BT2C_TYPE_TRAITS_HPP
#define BABELTRACE_CPP_COMMON_BT2C_TYPE_TRAITS_HPP

#include <type_traits>

namespace bt2c {

/*
 * Provides the member constant `value` equal to:
 *
 * `T` is in the list of types `Ts`:
 *     `true`
 *
 * Otherwise:
 *     `false`
 */
template <typename T, typename... Ts>
struct IsOneOf : std::false_type
{
};

template <typename T, typename... Ts>
struct IsOneOf<T, T, Ts...> : std::true_type
{
};

template <typename T, typename U, typename... Ts>
struct IsOneOf<T, U, Ts...> : IsOneOf<T, Ts...>
{
};

} /* namespace bt2c */

#endif /* BABELTRACE_CPP_COMMON_BT2C_TYPE_TRAITS_HPP */
