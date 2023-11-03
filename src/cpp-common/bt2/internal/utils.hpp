/*
 * Copyright 2019-2020 (c) Philippe Proulx <pproulx@efficios.com>
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef BABELTRACE_CPP_COMMON_BT2_INTERNAL_UTILS_HPP
#define BABELTRACE_CPP_COMMON_BT2_INTERNAL_UTILS_HPP

#include <type_traits>

#include <babeltrace2/babeltrace.h>

#include "../exc.hpp"

namespace bt2 {

template <typename>
class CommonMapValue;

template <typename>
class CommonFieldClass;

template <typename>
class CommonPacket;

template <typename>
class CommonStream;

namespace internal {

template <typename LibObjPtrT>
void validateCreatedObjPtr(const LibObjPtrT libOjbPtr)
{
    if (!libOjbPtr) {
        throw MemoryError {};
    }
}

template <typename LibObjT, typename DepObjT, typename ConstDepObjT>
using DepType =
    typename std::conditional<std::is_const<LibObjT>::value, ConstDepObjT, DepObjT>::type;

template <typename LibObjT>
using DepUserAttrs = DepType<LibObjT, CommonMapValue<bt_value>, CommonMapValue<const bt_value>>;

template <typename LibObjT>
using DepFc =
    DepType<LibObjT, CommonFieldClass<bt_field_class>, CommonFieldClass<const bt_field_class>>;

template <typename LibObjT>
using DepPacket = DepType<LibObjT, CommonPacket<bt_packet>, CommonPacket<const bt_packet>>;

template <typename LibObjT>
using DepStream = DepType<LibObjT, CommonStream<bt_stream>, CommonStream<const bt_stream>>;

template <typename ObjT>
struct TypeDescr;

} /* namespace internal */
} /* namespace bt2 */

#endif /* BABELTRACE_CPP_COMMON_BT2_INTERNAL_UTILS_HPP */
