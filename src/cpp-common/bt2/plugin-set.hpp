/*
 * Copyright (c) 2024 EfficiOS, Inc.
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef BABELTRACE_CPP_COMMON_BT2_PLUGIN_SET_HPP
#define BABELTRACE_CPP_COMMON_BT2_PLUGIN_SET_HPP

#include <cstdint>

#include <babeltrace2/babeltrace.h>

#include "borrowed-object.hpp"
#include "shared-object.hpp"

namespace bt2 {
namespace internal {

struct PluginSetRefFuncs
{
    static void get(const bt_plugin_set * const libObjPtr) noexcept
    {
        bt_plugin_set_get_ref(libObjPtr);
    }

    static void put(const bt_plugin_set * const libObjPtr) noexcept
    {
        bt_plugin_set_put_ref(libObjPtr);
    }
};

} /* namespace internal */

class ConstPluginSet final : public BorrowedObject<const bt_plugin_set>
{
public:
    using Shared = SharedObject<ConstPluginSet, const bt_plugin_set, internal::PluginSetRefFuncs>;

    explicit ConstPluginSet(const bt_plugin_set * const plugin_set) :
        _ThisBorrowedObject {plugin_set}
    {
    }

    std::uint64_t length() const noexcept
    {
        return bt_plugin_set_get_plugin_count(this->libObjPtr());
    }
};

} /* namespace bt2 */

#endif /* BABELTRACE_CPP_COMMON_BT2_PLUGIN_SET_HPP */
