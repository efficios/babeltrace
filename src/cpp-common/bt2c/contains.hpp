/*
 * Copyright (c) 2024 Philippe Proulx <pproulx@efficios.com>
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef BABELTRACE_CPP_COMMON_BT2C_CONTAINS_HPP
#define BABELTRACE_CPP_COMMON_BT2C_CONTAINS_HPP

namespace bt2c {

/*
 * Returns whether or not the STL container `container` contains the
 * value `val`.
 */
template <typename T, typename V>
bool contains(const T& container, const V& val) noexcept
{
    return container.find(val) != container.end();
}

} /* namespace bt2c */

#endif /* BABELTRACE_CPP_COMMON_BT2C_CONTAINS_HPP */
