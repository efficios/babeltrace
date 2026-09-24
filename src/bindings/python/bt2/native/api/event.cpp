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

void registerEventApi(PyModuleBuilder& mb)
{
    mb.addLibFunc<bt_event_borrow_class>();
    mb.addLibFunc<bt_event_borrow_class_const>();
    mb.addLibFunc<bt_event_borrow_stream>();
    mb.addLibFunc<bt_event_borrow_stream_const>();
    mb.addLibFunc<bt_event_borrow_packet>();
    mb.addLibFunc<bt_event_borrow_packet_const>();

    mb.addLibFunc<bt_event_borrow_payload_field>();
    mb.addLibFunc<bt_event_borrow_payload_field_const>();
    mb.addLibFunc<bt_event_borrow_specific_context_field>();
    mb.addLibFunc<bt_event_borrow_specific_context_field_const>();
    mb.addLibFunc<bt_event_borrow_common_context_field>();
    mb.addLibFunc<bt_event_borrow_common_context_field_const>();
}

} /* namespace bt2py */
