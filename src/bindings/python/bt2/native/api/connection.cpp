/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2026 EfficiOS Inc.
 */

#include <babeltrace2/babeltrace.h>

#include "../api.hpp"
#include "../runtime/py-module-builder.hpp"

namespace bt2py {

void registerConnectionApi(PyModuleBuilder& mb)
{
    mb.addLibFunc<bt_connection_get_ref>();
    mb.addLibFunc<bt_connection_put_ref>();
    mb.addLibFunc<bt_connection_borrow_downstream_port_const>();
    mb.addLibFunc<bt_connection_borrow_upstream_port_const>();
}

} /* namespace bt2py */
