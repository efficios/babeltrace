/*
 * Copyright (c) 2023 Philippe Proulx <pproulx@efficios.com>
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef BABELTRACE_CPP_COMMON_BT2C_CALL_HPP
#define BABELTRACE_CPP_COMMON_BT2C_CALL_HPP

#include <functional>
#include <utility>

namespace bt2c {

/*
 * Partial implementation of INVOKE.
 *
 * As found in
 * <https://www.open-std.org/jtc1/sc22/wg21/docs/papers/2017/p0312r1.html>.
 */
template <typename FuncT, typename... ArgTs>
auto call(FuncT func, ArgTs&&...args) -> decltype(std::ref(func)(std::forward<ArgTs>(args)...))
{
    return std::ref(func)(std::forward<ArgTs>(args)...);
}

} /* namespace bt2c */

#endif /* BABELTRACE_CPP_COMMON_BT2C_CALL_HPP */
