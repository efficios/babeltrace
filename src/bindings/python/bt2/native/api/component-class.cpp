/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2026 EfficiOS Inc.
 */

#include <Python.h>

#include <babeltrace2/babeltrace.h>

#include "cpp-common/bt2/component-class.hpp"

#include "../api.hpp"
#include "../runtime/py-module-builder.hpp"

namespace bt2py {

void registerComponentClassApi(PyModuleBuilder& mb)
{
    mb.addWiseEnumConstants<bt2::ComponentClassType>("COMPONENT_CLASS_TYPE_");

    mb.addLibFunc<bt_component_class_get_ref>();
    mb.addLibFunc<bt_component_class_put_ref>();
    mb.addLibFunc<bt_component_class_get_name>();
    mb.addLibFunc<bt_component_class_get_description>();
    mb.addLibFunc<bt_component_class_get_help>();

    mb.addLibFunc<bt_component_class_source_get_ref>();
    mb.addLibFunc<bt_component_class_source_put_ref>();
    mb.addLibFunc<bt_component_class_source_as_component_class>();

    mb.addLibFunc<bt_component_class_filter_get_ref>();
    mb.addLibFunc<bt_component_class_filter_put_ref>();
    mb.addLibFunc<bt_component_class_filter_as_component_class>();

    mb.addLibFunc<bt_component_class_sink_get_ref>();
    mb.addLibFunc<bt_component_class_sink_put_ref>();
    mb.addLibFunc<bt_component_class_sink_as_component_class>();
}

} /* namespace bt2py */
