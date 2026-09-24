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

void registerStreamApi(PyModuleBuilder& mb)
{
    mb.addLibFunc<bt_stream_create>();
    mb.addLibFunc<bt_stream_create_with_id>();
    mb.addLibFunc<bt_stream_get_ref>();
    mb.addLibFunc<bt_stream_put_ref>();
    mb.addLibFunc<bt_stream_borrow_class>();
    mb.addLibFunc<bt_stream_borrow_class_const>();
    mb.addLibFunc<bt_stream_borrow_trace>();
    mb.addLibFunc<bt_stream_borrow_trace_const>();
    mb.addLibFunc<bt_stream_get_id>();

    mb.addLibFunc<bt_stream_set_name>();
    mb.addLibFunc<bt_stream_get_name>();

    mb.addLibFunc<bt_stream_set_user_attributes>();
    mb.addLibFunc<bt_stream_borrow_user_attributes>();
    mb.addLibFunc<bt_stream_borrow_user_attributes_const>();
}

} /* namespace bt2py */
