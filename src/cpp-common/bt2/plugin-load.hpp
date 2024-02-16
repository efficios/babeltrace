/*
 * Copyright (c) 2024 EfficiOS, Inc.
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef BABELTRACE_CPP_COMMON_BT2_PLUGIN_LOAD_HPP
#define BABELTRACE_CPP_COMMON_BT2_PLUGIN_LOAD_HPP

#include <babeltrace2/babeltrace.h>

#include "common/common.h"
#include "cpp-common/bt2c/c-string-view.hpp"

#include "exc.hpp"
#include "plugin-set.hpp"
#include "plugin.hpp"

namespace bt2 {

inline ConstPlugin::Shared
findPlugin(const bt2c::CStringView name, const bool findInStdEnvVar = true,
           const bool findInUserDir = true, const bool findInSysDir = true,
           const bool findInStatic = true, const bool failOnLoadError = false)
{
    const bt_plugin *plugin;
    const auto status = bt_plugin_find(name, findInStdEnvVar, findInUserDir, findInSysDir,
                                       findInStatic, failOnLoadError, &plugin);

    if (status == BT_PLUGIN_FIND_STATUS_MEMORY_ERROR) {
        throw MemoryError {};
    } else if (status == BT_PLUGIN_FIND_STATUS_ERROR) {
        throw Error {};
    } else if (status == BT_PLUGIN_FIND_STATUS_NOT_FOUND) {
        return ConstPlugin::Shared {};
    }

    return ConstPlugin::Shared::createWithoutRef(plugin);
}

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

#endif /* BABELTRACE_CPP_COMMON_BT2_PLUGIN_LOAD_HPP */
