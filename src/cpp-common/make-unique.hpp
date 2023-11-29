/*
 * Copyright (c) 2022 Philippe Proulx <pproulx@efficios.com>
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef BABELTRACE_CPP_COMMON_MAKE_UNIQUE_HPP
#define BABELTRACE_CPP_COMMON_MAKE_UNIQUE_HPP

#include <memory>
#include <type_traits>
#include <utility>

namespace bt2_common {

/*
 * Our implementation of std::make_unique<>() for C++11.
 */
template <typename T, typename... ArgTs>
std::unique_ptr<T> makeUnique(ArgTs&&...args)
{
    static_assert(!std::is_array<T>::value, "`T` is not an array (unsupported).");

    return std::unique_ptr<T>(new T {std::forward<ArgTs>(args)...});
}

} /* namespace bt2_common */

#endif /* BABELTRACE_CPP_COMMON_MAKE_UNIQUE_HPP */
