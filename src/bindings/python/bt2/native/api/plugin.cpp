/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2026 EfficiOS Inc.
 */

#include <Python.h>

#include <optional>
#include <tuple>

#include <babeltrace2/babeltrace.h>

#include "cpp-common/bt2/plugin-load.hpp"
#include "cpp-common/bt2/plugin-provider-set.hpp"
#include "cpp-common/bt2/plugin-provider.hpp"
#include "cpp-common/bt2/plugin.hpp"
#include "cpp-common/bt2c/c-string-view.hpp"

#include "../api.hpp"
#include "../runtime/py-module-builder.hpp"

namespace bt2py {
namespace {

/*
 * Returns the version of `obj` (plugin or plugin provider) as a tuple
 * (major, minor, patch, and extra information), if available.
 */
template <typename ObjT>
std::optional<std::tuple<unsigned int, unsigned int, unsigned int, bt2c::CStringView>>
getVersion(const ObjT obj) noexcept
{
    if (const auto version = obj.version()) {
        return std::make_tuple(version->major(), version->minor(), version->patch(),
                               version->extra());
    }

    return std::nullopt;
}

} /* namespace */

void registerPluginApi(PyModuleBuilder& mb)
{
    /* Plugin provider */
    mb.addFunc<bt2::pluginProviderSet>("plugin_provider_set_borrow");
    mb.addLibFunc<bt_plugin_provider_set_get_plugin_provider_count>();
    mb.addLibFunc<bt_plugin_provider_set_borrow_plugin_provider_by_index>();

    mb.addLibFunc<bt_plugin_provider_get_name>();
    mb.addLibFunc<bt_plugin_provider_get_description>();
    mb.addLibFunc<bt_plugin_provider_get_author>();
    mb.addLibFunc<bt_plugin_provider_get_license>();
    mb.addLibFunc<bt_plugin_provider_get_path>();
    mb.addFunc<getVersion<bt2::ConstPluginProvider>>("plugin_provider_get_version");

    /* Plugin */
    mb.addLibFunc<bt_plugin_create>();
    mb.addLibFunc<bt_plugin_get_ref>();
    mb.addLibFunc<bt_plugin_put_ref>();

    mb.addLibFunc<bt_plugin_get_name>();
    mb.addLibFunc<bt_plugin_set_description>();
    mb.addLibFunc<bt_plugin_get_description>();
    mb.addLibFunc<bt_plugin_set_author>();
    mb.addLibFunc<bt_plugin_get_author>();
    mb.addLibFunc<bt_plugin_set_license>();
    mb.addLibFunc<bt_plugin_get_license>();
    mb.addLibFunc<bt_plugin_set_path>();
    mb.addLibFunc<bt_plugin_get_path>();
    mb.addLibFunc<bt_plugin_set_version>();
    mb.addFunc<getVersion<bt2::ConstPlugin>>("plugin_get_version");
    mb.addLibFunc<bt_plugin_borrow_provider>();

    mb.addLibFunc<bt_plugin_add_component_class>();
    mb.addLibFunc<bt_plugin_get_source_component_class_count>();
    mb.addLibFunc<bt_plugin_get_filter_component_class_count>();
    mb.addLibFunc<bt_plugin_get_sink_component_class_count>();
    mb.addLibFunc<bt_plugin_borrow_source_component_class_by_index_const>();
    mb.addLibFunc<bt_plugin_borrow_filter_component_class_by_index_const>();
    mb.addLibFunc<bt_plugin_borrow_sink_component_class_by_index_const>();
    mb.addLibFunc<bt_plugin_borrow_source_component_class_by_name_const>();
    mb.addLibFunc<bt_plugin_borrow_filter_component_class_by_name_const>();
    mb.addLibFunc<bt_plugin_borrow_sink_component_class_by_name_const>();

    /* Plugin set */
    mb.addLibFunc<bt_plugin_set_get_ref>();
    mb.addLibFunc<bt_plugin_set_put_ref>();
    mb.addLibFunc<bt_plugin_set_get_plugin_count>();
    mb.addLibFunc<bt_plugin_set_borrow_plugin_by_index_const>();

    /* Plugin loading */
    mb.addFunc<bt2::findPlugin>("plugin_find");
    mb.addFunc<bt2::findAllPlugins>("plugin_find_all");
    mb.addFunc<bt2::findAllPluginsFromFile>("plugin_find_all_from_file");
    mb.addFunc<bt2::findAllPluginsFromDir>("plugin_find_all_from_dir");
}

} /* namespace bt2py */
