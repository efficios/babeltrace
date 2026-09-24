/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2026 EfficiOS Inc.
 */

#include <Python.h>

#include <babeltrace2/babeltrace.h>

#include "cpp-common/bt2/self-component-port.hpp"
#include "cpp-common/bt2c/c-string-view.hpp"

#include "../api.hpp"
#include "../runtime/py-module-builder.hpp"

namespace bt2py {
namespace {

bt2::SelfComponentOutputPort selfCompSourceAddOutputPort(const bt2::SelfSourceComponent selfComp,
                                                         const bt2c::CStringView name,
                                                         PyObject * const pyUserData)
{
    const auto port = selfComp.addOutputPort(name, *pyUserData);

    Py_INCREF(pyUserData);
    return port;
}

bt2::SelfComponentInputPort selfCompFilterAddInputPort(const bt2::SelfFilterComponent selfComp,
                                                       const bt2c::CStringView name,
                                                       PyObject * const pyUserData)
{
    const auto port = selfComp.addInputPort(name, *pyUserData);

    Py_INCREF(pyUserData);
    return port;
}

bt2::SelfComponentOutputPort selfCompFilterAddOutputPort(const bt2::SelfFilterComponent selfComp,
                                                         const bt2c::CStringView name,
                                                         PyObject * const pyUserData)
{
    const auto port = selfComp.addOutputPort(name, *pyUserData);

    Py_INCREF(pyUserData);
    return port;
}

bt2::SelfComponentInputPort selfCompSinkAddInputPort(const bt2::SelfSinkComponent selfComp,
                                                     const bt2c::CStringView name,
                                                     PyObject * const pyUserData)
{
    const auto port = selfComp.addInputPort(name, *pyUserData);

    Py_INCREF(pyUserData);
    return port;
}

} /* namespace */

void registerSelfComponentApi(PyModuleBuilder& mb)
{
    mb.addLibFunc<bt_self_component_get_graph_mip_version>();

    mb.addLibFunc<bt_self_component_source_as_self_component>();
    mb.addLibFunc<bt_self_component_source_as_component_source>();
    mb.addLibFunc<bt_self_component_source_borrow_output_port_by_index>();
    mb.addLibFunc<bt_self_component_source_borrow_output_port_by_name>();
    mb.addFunc<selfCompSourceAddOutputPort>("self_component_source_add_output_port");

    mb.addLibFunc<bt_self_component_filter_as_self_component>();
    mb.addLibFunc<bt_self_component_filter_as_component_filter>();
    mb.addLibFunc<bt_self_component_filter_borrow_input_port_by_index>();
    mb.addLibFunc<bt_self_component_filter_borrow_input_port_by_name>();
    mb.addLibFunc<bt_self_component_filter_borrow_output_port_by_index>();
    mb.addLibFunc<bt_self_component_filter_borrow_output_port_by_name>();
    mb.addFunc<selfCompFilterAddInputPort>("self_component_filter_add_input_port");
    mb.addFunc<selfCompFilterAddOutputPort>("self_component_filter_add_output_port");

    mb.addLibFunc<bt_self_component_sink_as_self_component>();
    mb.addLibFunc<bt_self_component_sink_as_component_sink>();
    mb.addLibFunc<bt_self_component_sink_borrow_input_port_by_index>();
    mb.addLibFunc<bt_self_component_sink_borrow_input_port_by_name>();
    mb.addFunc<selfCompSinkAddInputPort>("self_component_sink_add_input_port");
    mb.addLibBoolRetFunc<bt_self_component_sink_is_interrupted>();
}

} /* namespace bt2py */
