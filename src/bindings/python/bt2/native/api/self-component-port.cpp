/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2026 EfficiOS Inc.
 */

#include <Python.h>

#include <babeltrace2/babeltrace.h>

#include "../api.hpp"
#include "../runtime/py-module-builder.hpp"
#include "../runtime/utils.hpp"

namespace bt2py {
namespace {

/*
 * Returns the user data of `selfCompPort` (new reference).
 *
 * The user data of a port which the Python layer adds is always a
 * Python object (possibly `None`) which the port owns.
 */
PyObjUP getSelfCompPortData(const bt_self_component_port * const selfCompPort) noexcept
{
    return pyObjFromUserData(bt_self_component_port_get_data(selfCompPort));
}

} /* namespace */

void registerSelfComponentPortApi(PyModuleBuilder& mb)
{
    mb.addLibFunc<bt_self_component_port_as_port>();
    mb.addFunc<getSelfCompPortData>("self_component_port_get_data");
    mb.addLibFunc<bt_self_component_port_input_as_self_component_port>();
    mb.addLibFunc<bt_self_component_port_output_as_self_component_port>();
}

} /* namespace bt2py */
