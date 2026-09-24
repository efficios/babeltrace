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

void registerInterrupterApi(PyModuleBuilder& mb)
{
    mb.addLibFunc<bt_interrupter_create>();
    mb.addLibFunc<bt_interrupter_get_ref>();
    mb.addLibFunc<bt_interrupter_put_ref>();
    mb.addLibFunc<bt_interrupter_set>();
    mb.addLibFunc<bt_interrupter_reset>();
    mb.addLibBoolRetFunc<bt_interrupter_is_set>();
}

} /* namespace bt2py */
