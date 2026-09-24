/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2026 EfficiOS Inc.
 */

#include <Python.h>

#include <babeltrace2/babeltrace.h>

#include "../api.hpp"
#include "../runtime/py-module-builder.hpp"

namespace bt2py {

void registerPortApi(PyModuleBuilder& mb)
{
    mb.addConstant("PORT_TYPE_INPUT", BT_PORT_TYPE_INPUT);
    mb.addConstant("PORT_TYPE_OUTPUT", BT_PORT_TYPE_OUTPUT);

    mb.addLibFunc<bt_port_get_ref>();
    mb.addLibFunc<bt_port_put_ref>();
    mb.addLibFunc<bt_port_get_name>();
    mb.addLibFunc<bt_port_borrow_connection_const>();
    mb.addLibFunc<bt_port_input_as_port_const>();
    mb.addLibFunc<bt_port_output_as_port_const>();
}

} /* namespace bt2py */
