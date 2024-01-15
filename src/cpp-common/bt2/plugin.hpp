/*
 * Copyright (c) 2024 EfficiOS, Inc.
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef BABELTRACE_CPP_COMMON_BT2_PLUGIN_HPP
#define BABELTRACE_CPP_COMMON_BT2_PLUGIN_HPP

#include <babeltrace2/babeltrace.h>

#include "common/common.h"
#include "cpp-common/bt2/borrowed-object.hpp"
#include "cpp-common/bt2/exc.hpp"
#include "cpp-common/bt2/shared-object.hpp"
#include "cpp-common/bt2c/c-string-view.hpp"

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

inline ConstPluginSet::Shared findAllPluginsFromDir(const bt2c::CStringView path,
                                                    const bool recurse, const bool failOnLoadError)
{
    const bt_plugin_set *pluginSet;

    switch (bt_plugin_find_all_from_dir(path, recurse, failOnLoadError, &pluginSet)) {
    case BT_PLUGIN_FIND_ALL_FROM_DIR_STATUS_OK:
        return ConstPluginSet::Shared::createWithoutRef(pluginSet);
    case BT_PLUGIN_FIND_ALL_FROM_DIR_STATUS_NOT_FOUND:
        return ConstPluginSet::Shared {};
    case BT_PLUGIN_FIND_ALL_FROM_DIR_STATUS_MEMORY_ERROR:
        throw MemoryError {};
    case BT_PLUGIN_FIND_ALL_FROM_DIR_STATUS_ERROR:
        throw Error {};
    }

    bt_common_abort();
}

} /* namespace bt2 */

#endif /* BABELTRACE_CPP_COMMON_BT2_PLUGIN_HPP */
