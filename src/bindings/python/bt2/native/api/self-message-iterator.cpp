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

void registerSelfMessageIteratorApi(PyModuleBuilder& mb)
{
    /*
     * `self_message_iterator_get_component_data` is part of
     * `api/component-class-dev.cpp`: it knows the actual component
     * user data type.
     */
    mb.addLibFunc<bt_self_message_iterator_borrow_port>();
    mb.addLibBoolRetFunc<bt_self_message_iterator_is_interrupted>();

    mb.addLibFunc<bt_self_message_iterator_configuration_set_can_seek_forward>();
}

} /* namespace bt2py */
