/*
 * Copyright 2019-2020 (c) Philippe Proulx <pproulx@efficios.com>
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef BABELTRACE_CPP_COMMON_BT2_INTERNAL_UTILS_HPP
#define BABELTRACE_CPP_COMMON_BT2_INTERNAL_UTILS_HPP

#include <type_traits>

#include "../lib-error.hpp"

namespace bt2 {
namespace internal {

template <typename LibObjPtrT>
void validateCreatedObjPtr(const LibObjPtrT libOjbPtr)
{
    if (!libOjbPtr) {
        throw LibMemoryError {};
    }
}

} // namespace internal
} // namespace bt2

#endif // BABELTRACE_CPP_COMMON_BT2_INTERNAL_UTILS_HPP
